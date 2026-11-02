#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_security.h"
#include "ami_dlms_cipher.h"
#include "ami_dlms_port.h"

/* NIST SP 800-38F Appendix 4.1 Known Answer Test (KAT) */
static void test_rfc3394_kat(void)
{
    const uint8_t KEK[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    const uint8_t PLAIN_KEY[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };
    const uint8_t EXPECTED_CIPHERTEXT[24] = {
        0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47,
        0xAE, 0xF3, 0x4B, 0xD8, 0xFB, 0x5A, 0x7B, 0x82,
        0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5
    };

    uint8_t wrapped[32];
    size_t wrapped_len = 0;
    AmiSecStatus status = ami_sec_rfc3394_wrap(
        KEK, PLAIN_KEY, sizeof(PLAIN_KEY), wrapped, sizeof(wrapped), &wrapped_len);

    assert(status == AMI_SEC_OK);
    assert(wrapped_len == 24);
    assert(memcmp(wrapped, EXPECTED_CIPHERTEXT, 24) == 0);

    /* Test Unwrap back to plaintext */
    uint8_t unwrapped[32];
    size_t unwrapped_len = 0;
    status = ami_sec_rfc3394_unwrap(
        KEK, wrapped, wrapped_len, unwrapped, sizeof(unwrapped), &unwrapped_len);

    assert(status == AMI_SEC_OK);
    assert(unwrapped_len == 16);
    assert(memcmp(unwrapped, PLAIN_KEY, 16) == 0);

    printf("test_rfc3394_kat: PASS\n");
}

static void test_rfc3394_tamper_integrity(void)
{
    const uint8_t KEK[16] = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
    };
    const uint8_t PLAIN_KEY[16] = {
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01
    };

    uint8_t wrapped[24];
    size_t wrapped_len = 0;
    assert(ami_sec_rfc3394_wrap(KEK, PLAIN_KEY, 16, wrapped, sizeof(wrapped), &wrapped_len) == AMI_SEC_OK);

    /* Tamper every byte position in the wrapped ciphertext */
    for (size_t byte_pos = 0; byte_pos < 24; byte_pos++) {
        uint8_t corrupted[24];
        memcpy(corrupted, wrapped, 24);
        corrupted[byte_pos] ^= 0x01; /* Flip 1 bit */

        uint8_t recovered[16];
        size_t recovered_len = 0;
        AmiSecStatus status = ami_sec_rfc3394_unwrap(
            KEK, corrupted, sizeof(corrupted), recovered, sizeof(recovered), &recovered_len);

        /* RFC 3394 requires strict integrity authentication failure */
        assert(status == AMI_SEC_AUTHENTICATION_FAILURE);
    }

    printf("test_rfc3394_tamper_integrity: PASS\n");
}

static void test_suite0_atomic_key_transfer_success(void)
{
    const uint8_t INITIAL_GUEK[16] = {
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
    };
    const uint8_t INITIAL_AK[16] = {
        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22
    };
    const uint8_t MASTER_KEK[16] = {
        0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22,
        0x11, 0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA
    };
    const uint8_t CLIENT_TITLE[8] = {'P', 'L', 'N', 'C', 'L', 'I', '0', '1'};
    const uint8_t SERVER_TITLE[8] = {'P', 'L', 'N', 'M', 'T', 'R', '0', '1'};

    AmiDlmsSecuritySuite0Context cipher_ctx;
    assert(ami_dlms_cipher_suite0_init(&cipher_ctx, INITIAL_GUEK, INITIAL_AK,
                                       CLIENT_TITLE, SERVER_TITLE, 1u) == 0);
    assert(ami_dlms_cipher_suite0_set_kek(&cipher_ctx, MASTER_KEK) == 0);

    AmiDlmsCipherBackend backend;
    assert(ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend) == 0);

    /* Generate new keys wrapped using KEK */
    const uint8_t NEW_GUEK[16] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
    };
    const uint8_t NEW_AK[16] = {
        0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
        0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44
    };

    uint8_t wrapped_guek[24];
    size_t wrapped_guek_len = 0;
    assert(ami_dlms_cipher_suite0_wrap_key(&cipher_ctx, NEW_GUEK, 16,
                                           wrapped_guek, sizeof(wrapped_guek), &wrapped_guek_len) == 0);
    assert(wrapped_guek_len == 24);

    uint8_t wrapped_ak[24];
    size_t wrapped_ak_len = 0;
    assert(ami_dlms_cipher_suite0_wrap_key(&cipher_ctx, NEW_AK, 16,
                                           wrapped_ak, sizeof(wrapped_ak), &wrapped_ak_len) == 0);
    assert(wrapped_ak_len == 24);

    /* Prepare batch key transfer */
    AmiDlmsWrappedKey batch[2];
    batch[0].key_type = 0u; /* GUEK */
    batch[0].wrapped_key = wrapped_guek;
    batch[0].wrapped_key_length = wrapped_guek_len;

    batch[1].key_type = 2u; /* AK */
    batch[1].wrapped_key = wrapped_ak;
    batch[1].wrapped_key_length = wrapped_ak_len;

    /* Execute atomic key transfer */
    assert(backend.transfer_keys(backend.user, batch, 2) == AMI_SEC_OK);

    /* Verify context keys have been updated to new values */
    assert(memcmp(cipher_ctx.encryption_key, NEW_GUEK, 16) == 0);
    assert(memcmp(cipher_ctx.authentication_key, NEW_AK, 16) == 0);

    printf("test_suite0_atomic_key_transfer_success: PASS\n");
}

static void test_suite0_atomic_key_transfer_rollback_on_failure(void)
{
    const uint8_t INITIAL_GUEK[16] = {
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
    };
    const uint8_t INITIAL_AK[16] = {
        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22
    };
    const uint8_t MASTER_KEK[16] = {
        0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22,
        0x11, 0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA
    };
    const uint8_t CLIENT_TITLE[8] = {'P', 'L', 'N', 'C', 'L', 'I', '0', '1'};
    const uint8_t SERVER_TITLE[8] = {'P', 'L', 'N', 'M', 'T', 'R', '0', '1'};

    AmiDlmsSecuritySuite0Context cipher_ctx;
    assert(ami_dlms_cipher_suite0_init(&cipher_ctx, INITIAL_GUEK, INITIAL_AK,
                                       CLIENT_TITLE, SERVER_TITLE, 1u) == 0);
    assert(ami_dlms_cipher_suite0_set_kek(&cipher_ctx, MASTER_KEK) == 0);

    AmiDlmsCipherBackend backend;
    assert(ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend) == 0);

    const uint8_t NEW_GUEK[16] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
    };
    const uint8_t NEW_AK[16] = {
        0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
        0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44
    };

    uint8_t wrapped_guek[24];
    size_t wrapped_guek_len = 0;
    assert(ami_dlms_cipher_suite0_wrap_key(&cipher_ctx, NEW_GUEK, 16,
                                           wrapped_guek, sizeof(wrapped_guek), &wrapped_guek_len) == 0);

    uint8_t wrapped_ak[24];
    size_t wrapped_ak_len = 0;
    assert(ami_dlms_cipher_suite0_wrap_key(&cipher_ctx, NEW_AK, 16,
                                           wrapped_ak, sizeof(wrapped_ak), &wrapped_ak_len) == 0);

    /* Corrupt the second wrapped key (AK) to simulate tampering / bit error */
    wrapped_ak[10] ^= 0xFF;

    AmiDlmsWrappedKey batch[2];
    batch[0].key_type = 0u; /* Valid GUEK */
    batch[0].wrapped_key = wrapped_guek;
    batch[0].wrapped_key_length = wrapped_guek_len;

    batch[1].key_type = 2u; /* Tampered AK */
    batch[1].wrapped_key = wrapped_ak;
    batch[1].wrapped_key_length = wrapped_ak_len;

    /* Transfer MUST fail */
    assert(backend.transfer_keys(backend.user, batch, 2) == AMI_SEC_AUTHENTICATION_FAILURE);

    /* Crucial Atomic Guarantee:
     * Even though key 0 was valid, it MUST NOT be applied because sibling key 1 failed!
     * Both GUEK and AK must remain at their initial values!
     */
    assert(memcmp(cipher_ctx.encryption_key, INITIAL_GUEK, 16) == 0);
    assert(memcmp(cipher_ctx.authentication_key, INITIAL_AK, 16) == 0);

    printf("test_suite0_atomic_key_transfer_rollback_on_failure: PASS\n");
}

int main(void)
{
    printf("Starting Remote Key Transfer & RFC 3394 AES Key Wrap Tests...\n");
    test_rfc3394_kat();
    test_rfc3394_tamper_integrity();
    test_suite0_atomic_key_transfer_success();
    test_suite0_atomic_key_transfer_rollback_on_failure();
    printf("All Remote Key Transfer & RFC 3394 Tests PASSED!\n");
    return 0;
}

