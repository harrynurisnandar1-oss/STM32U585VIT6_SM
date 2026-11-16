/**
 * @file    ami_boot_crypto.h
 * @brief   Pure C11 FIPS 180-4 SHA-256 & FIPS 186-4 ECDSA P-256 (secp256r1)
 *          for Secure Boot verification (STM32U585VIT6 & Host Tools).
 */
#ifndef AMI_BOOT_CRYPTO_H
#define AMI_BOOT_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    size_t   datalen;
} AmiSha256Context;

void ami_sha256_init(AmiSha256Context *ctx);
void ami_sha256_update(AmiSha256Context *ctx, const uint8_t *data, size_t len);
void ami_sha256_final(AmiSha256Context *ctx, uint8_t out_hash[32]);
void ami_sha256_compute(const uint8_t *data, size_t len, uint8_t out_hash[32]);

/**
 * @brief Verify an ECDSA P-256 (secp256r1) signature over a 32-byte SHA-256 digest.
 * @param pub_x 32-byte big-endian public key X coordinate.
 * @param pub_y 32-byte big-endian public key Y coordinate.
 * @param hash  32-byte SHA-256 digest.
 * @param sig   64-byte raw signature (r || s, each 32-byte big-endian).
 * @return 0 if signature is valid, negative error code otherwise.
 */
int ami_ecdsa_p256_verify(const uint8_t pub_x[32],
                          const uint8_t pub_y[32],
                          const uint8_t hash[32],
                          const uint8_t sig[64]);

#ifdef AMI_BOOT_CRYPTO_ENABLE_SIGN
int ami_ecdsa_p256_pubkey_from_priv(const uint8_t priv[32],
                                    uint8_t pub_x[32],
                                    uint8_t pub_y[32]);

int ami_ecdsa_p256_sign_deterministic(const uint8_t priv[32],
                                      const uint8_t hash[32],
                                      const uint8_t k_nonce[32],
                                      uint8_t out_sig[64]);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AMI_BOOT_CRYPTO_H */
