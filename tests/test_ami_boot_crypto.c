/**
 * @file    test_ami_boot_crypto.c
 * @brief   Unit tests for FIPS 180-4 SHA-256, FIPS 186-4 ECDSA P-256,
 *          and NonSecure Secure Boot Manifest Verification (F2 / I6).
 */
#ifndef AMI_BOOT_CRYPTO_ENABLE_SIGN
#define AMI_BOOT_CRYPTO_ENABLE_SIGN
#endif
#include "ami_boot_crypto.h"
#include "ami_boot_pubkey.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t version_epoch;
    uint32_t image_size;
    uint32_t flags;
    uint8_t  sha256_hash[32];
    uint8_t  ecdsa_sig[64];
} TestNsManifest;

static const uint8_t TEST_LITBANG_PRIV[32] = {
    0x7A, 0x3F, 0x91, 0xC4, 0x28, 0xE5, 0x0B, 0x62,
    0xD4, 0x19, 0x8E, 0x57, 0xA3, 0x6C, 0x4F, 0x12,
    0x90, 0xB5, 0xE1, 0x3D, 0x48, 0x72, 0xC6, 0x09,
    0x5E, 0x8A, 0x14, 0xF3, 0x6B, 0x2D, 0x97, 0x41
};

static void test_sha256_fips180_4(void)
{
    /* Test 1: "abc" -> ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad */
    const uint8_t expected_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    uint8_t digest[32];
    ami_sha256_compute((const uint8_t *)"abc", 3u, digest);
    assert(memcmp(digest, expected_abc, 32u) == 0);

    /* Test 2: empty string -> e3b0c442 98fc1c14 9afbf4c8 996fb924 27ae41e4 649b934c a495991b 7852b855 */
    const uint8_t expected_empty[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    ami_sha256_compute((const uint8_t *)"", 0u, digest);
    assert(memcmp(digest, expected_empty, 32u) == 0);

    printf("  [PASS] test_sha256_fips180_4\n");
}

static void test_ecdsa_p256_sign_and_verify(void)
{
    uint8_t pub_x[32], pub_y[32];
    assert(ami_ecdsa_p256_pubkey_from_priv(TEST_LITBANG_PRIV, pub_x, pub_y) == 0);
    assert(memcmp(pub_x, AMI_BOOT_ECDSA_PUBKEY_X, 32u) == 0);
    assert(memcmp(pub_y, AMI_BOOT_ECDSA_PUBKEY_Y, 32u) == 0);

    uint8_t hash[32];
    ami_sha256_compute((const uint8_t *)"PLN_SMART_METER_NS_FW_V2.21", 26u, hash);

    uint8_t k_nonce[32];
    ami_sha256_compute((const uint8_t *)"DETERMINISTIC_NONCE_V1", 22u, k_nonce);

    uint8_t sig[64];
    assert(ami_ecdsa_p256_sign_deterministic(TEST_LITBANG_PRIV, hash, k_nonce, sig) == 0);

    /* Valid signature must pass */
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y, hash, sig) == 0);

    /* Negative: 1-bit flip in hash */
    uint8_t bad_hash[32];
    memcpy(bad_hash, hash, 32u);
    bad_hash[0] ^= 0x01u;
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y, bad_hash, sig) != 0);

    /* Negative: 1-bit flip in signature r and s */
    uint8_t bad_sig[64];
    memcpy(bad_sig, sig, 64u);
    bad_sig[15] ^= 0x80u;
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y, hash, bad_sig) != 0);

    memcpy(bad_sig, sig, 64u);
    bad_sig[48] ^= 0x01u;
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y, hash, bad_sig) != 0);

    /* Negative: r = 0 or s = 0 */
    memset(bad_sig, 0, 64u);
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y, hash, bad_sig) != 0);

    /* Negative: off-curve public key */
    uint8_t bad_pub_x[32];
    memcpy(bad_pub_x, AMI_BOOT_ECDSA_PUBKEY_X, 32u);
    bad_pub_x[31] ^= 0x01u;
    assert(ami_ecdsa_p256_verify(bad_pub_x, AMI_BOOT_ECDSA_PUBKEY_Y, hash, sig) != 0);

    printf("  [PASS] test_ecdsa_p256_sign_and_verify\n");
}

static void test_secure_boot_manifest_simulation(void)
{
    uint8_t image[2048];
    for (size_t i = 0; i < sizeof(image); ++i) {
        image[i] = (uint8_t)(i & 0xFFu);
    }

    TestNsManifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.magic = 0x504C4E21U;
    manifest.version_epoch = 2u;
    manifest.image_size = (uint32_t)sizeof(image);
    manifest.flags = 1u;

    size_t manifest_off = 0x400U;
    size_t tail_off = manifest_off + sizeof(TestNsManifest);

    AmiSha256Context sha;
    ami_sha256_init(&sha);
    ami_sha256_update(&sha, image, manifest_off);
    ami_sha256_update(&sha, image + tail_off, sizeof(image) - tail_off);
    ami_sha256_final(&sha, manifest.sha256_hash);

    uint8_t k_nonce[32];
    ami_sha256_compute((const uint8_t *)"MANIFEST_NONCE", 14u, k_nonce);
    assert(ami_ecdsa_p256_sign_deterministic(TEST_LITBANG_PRIV, manifest.sha256_hash,
                                             k_nonce, manifest.ecdsa_sig) == 0);
    memcpy(image + manifest_off, &manifest, sizeof(manifest));

    /* Verify simulated boot check */
    const TestNsManifest *m = (const TestNsManifest *)(image + manifest_off);
    uint8_t check_hash[32];
    ami_sha256_init(&sha);
    ami_sha256_update(&sha, image, manifest_off);
    ami_sha256_update(&sha, image + tail_off, m->image_size - tail_off);
    ami_sha256_final(&sha, check_hash);

    assert(memcmp(check_hash, m->sha256_hash, 32u) == 0);
    assert(ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y,
                                 check_hash, m->ecdsa_sig) == 0);

    /* Tamper 1 byte in NonSecure image body -> hash mismatch */
    image[tail_off + 10] ^= 0x01u;
    ami_sha256_init(&sha);
    ami_sha256_update(&sha, image, manifest_off);
    ami_sha256_update(&sha, image + tail_off, m->image_size - tail_off);
    ami_sha256_final(&sha, check_hash);
    assert(memcmp(check_hash, m->sha256_hash, 32u) != 0);

    printf("  [PASS] test_secure_boot_manifest_simulation\n");
}

int main(void)
{
    printf("Running Boot Crypto Tests (SHA-256 + ECDSA P-256)...\n");
    test_sha256_fips180_4();
    test_ecdsa_p256_sign_and_verify();
    test_secure_boot_manifest_simulation();
    printf("All Boot Crypto Tests PASSED!\n");
    return 0;
}
