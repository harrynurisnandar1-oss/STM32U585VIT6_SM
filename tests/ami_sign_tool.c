/**
 * @file    ami_sign_tool.c
 * @brief   Pure C11 Litbang Prototype ECDSA P-256 Keygen & NonSecure Image Signer.
 */
#ifndef AMI_BOOT_CRYPTO_ENABLE_SIGN
#define AMI_BOOT_CRYPTO_ENABLE_SIGN
#endif
#include "ami_boot_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Litbang PLN Prototype Signing Private Key (ECDSA P-256) */
static const uint8_t LITBANG_DEV_PRIVKEY[32] = {
    0x7A, 0x3F, 0x91, 0xC4, 0x28, 0xE5, 0x0B, 0x62,
    0xD4, 0x19, 0x8E, 0x57, 0xA3, 0x6C, 0x4F, 0x12,
    0x90, 0xB5, 0xE1, 0x3D, 0x48, 0x72, 0xC6, 0x09,
    0x5E, 0x8A, 0x14, 0xF3, 0x6B, 0x2D, 0x97, 0x41
};

#define AMI_BOOT_MANIFEST_MAGIC   0x504C4E21U /* 'PLN!' */
#define AMI_MANIFEST_OFFSET       0x400U

typedef struct {
    uint32_t magic;
    uint32_t version_epoch;
    uint32_t image_size;
    uint32_t flags;
    uint8_t  sha256_hash[32];
    uint8_t  ecdsa_sig[64];
} NonSecureImageManifestTypeDef;

static int generate_pubkey_header(const char *out_path)
{
    uint8_t pub_x[32], pub_y[32];
    if (ami_ecdsa_p256_pubkey_from_priv(LITBANG_DEV_PRIVKEY, pub_x, pub_y) != 0) {
        fprintf(stderr, "Error: failed to derive ECDSA P-256 public key\n");
        return 1;
    }

    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s for writing\n", out_path);
        return 1;
    }

    fprintf(fp, "/**\n");
    fprintf(fp, " * @file    ami_boot_pubkey.h\n");
    fprintf(fp, " * @brief   Auto-generated Litbang PLN Prototype ECDSA P-256 Public Key.\n");
    fprintf(fp, " */\n");
    fprintf(fp, "#ifndef AMI_BOOT_PUBKEY_H\n");
    fprintf(fp, "#define AMI_BOOT_PUBKEY_H\n\n");
    fprintf(fp, "#include <stdint.h>\n\n");

    fprintf(fp, "static const uint8_t AMI_BOOT_ECDSA_PUBKEY_X[32] = {\n");
    for (int i = 0; i < 32; ++i) {
        if (i % 8 == 0) fprintf(fp, "   ");
        fprintf(fp, " 0x%02XU%s", pub_x[i], (i == 31) ? "" : ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "};\n\n");

    fprintf(fp, "static const uint8_t AMI_BOOT_ECDSA_PUBKEY_Y[32] = {\n");
    for (int i = 0; i < 32; ++i) {
        if (i % 8 == 0) fprintf(fp, "   ");
        fprintf(fp, " 0x%02XU%s", pub_y[i], (i == 31) ? "" : ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "};\n\n");
    fprintf(fp, "#endif /* AMI_BOOT_PUBKEY_H */\n");
    fclose(fp);

    printf("Generated ECDSA P-256 public key header: %s\n", out_path);
    return 0;
}

static int sign_ns_binary(const char *bin_path, uint32_t version_epoch)
{
    FILE *fp = fopen(bin_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", bin_path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < (long)(AMI_MANIFEST_OFFSET + sizeof(NonSecureImageManifestTypeDef))) {
        fprintf(stderr, "Error: binary too small (%ld bytes)\n", fsize);
        fclose(fp);
        return 1;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)fsize);
    if (!buf) {
        fclose(fp);
        return 1;
    }
    if (fread(buf, 1, (size_t)fsize, fp) != (size_t)fsize) {
        free(buf);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    NonSecureImageManifestTypeDef manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.magic = AMI_BOOT_MANIFEST_MAGIC;
    manifest.version_epoch = version_epoch;
    manifest.image_size = (uint32_t)fsize;
    manifest.flags = 0x00000001U; /* ECDSA-P256 + SHA256 */

    /* Canonical image hash: hash [0..0x400) + [0x400+sizeof(manifest)..fsize) */
    AmiSha256Context sha;
    ami_sha256_init(&sha);
    ami_sha256_update(&sha, buf, AMI_MANIFEST_OFFSET);
    size_t tail_off = AMI_MANIFEST_OFFSET + sizeof(NonSecureImageManifestTypeDef);
    ami_sha256_update(&sha, buf + tail_off, (size_t)fsize - tail_off);
    ami_sha256_final(&sha, manifest.sha256_hash);

    /* Derive deterministic nonce k = SHA256(priv || hash) */
    uint8_t k_nonce[32];
    AmiSha256Context k_ctx;
    ami_sha256_init(&k_ctx);
    ami_sha256_update(&k_ctx, LITBANG_DEV_PRIVKEY, 32);
    ami_sha256_update(&k_ctx, manifest.sha256_hash, 32);
    ami_sha256_final(&k_ctx, k_nonce);

    if (ami_ecdsa_p256_sign_deterministic(LITBANG_DEV_PRIVKEY, manifest.sha256_hash,
                                          k_nonce, manifest.ecdsa_sig) != 0) {
        fprintf(stderr, "Error: ECDSA signing failed\n");
        free(buf);
        return 1;
    }

    memcpy(buf + AMI_MANIFEST_OFFSET, &manifest, sizeof(manifest));
    fp = fopen(bin_path, "wb");
    if (!fp) {
        free(buf);
        return 1;
    }
    fwrite(buf, 1, (size_t)fsize, fp);
    fclose(fp);
    free(buf);

    printf("Signed %s (size=%ld, epoch=%u)\n", bin_path, fsize, version_epoch);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--gen-header") == 0) {
        return generate_pubkey_header(argv[2]);
    }
    if (argc == 4 && strcmp(argv[1], "--sign") == 0) {
        uint32_t epoch = (uint32_t)strtoul(argv[3], NULL, 10);
        return sign_ns_binary(argv[2], epoch);
    }
    fprintf(stderr, "Usage:\n  %s --gen-header <out_pubkey.h>\n  %s --sign <ns_image.bin> <epoch>\n",
            argv[0], argv[0]);
    return 1;
}
