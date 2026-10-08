#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_security.h"

// --- Mock Backend Callbacks ---

static int mock_random_success(void *user, uint8_t *out, size_t out_len) {
    (void)user;
    for (size_t i = 0; i < out_len; ++i) {
        out[i] = (uint8_t)(i + 1);
    }
    return 1;
}

static int mock_random_failure(void *user, uint8_t *out, size_t out_len) {
    (void)user;
    (void)out;
    (void)out_len;
    return 0; // Failure
}

static AmiSecStatus mock_aead_encrypt_success(
    void *user, AmiAeadAlgorithm algorithm, AmiKeyHandle key,
    const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *ciphertext, uint8_t *tag, size_t tag_len)
{
    (void)user; (void)algorithm; (void)key; (void)nonce; (void)nonce_len; (void)aad; (void)aad_len;
    // Dummy XOR encrypt
    for (size_t i = 0; i < plaintext_len; ++i) {
        ciphertext[i] = plaintext[i] ^ 0xAA;
    }
    memset(tag, 0x55, tag_len);
    return AMI_SEC_OK;
}

static AmiSecStatus mock_aead_encrypt_fail(
    void *user, AmiAeadAlgorithm algorithm, AmiKeyHandle key,
    const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plaintext, size_t plaintext_len,
    uint8_t *ciphertext, uint8_t *tag, size_t tag_len)
{
    (void)user; (void)algorithm; (void)key; (void)nonce; (void)nonce_len; (void)aad; (void)aad_len;
    (void)plaintext; (void)plaintext_len; (void)ciphertext; (void)tag; (void)tag_len;
    return AMI_SEC_BACKEND_FAILURE;
}

static AmiSecStatus mock_aead_decrypt_auth_fail(
    void *user, AmiAeadAlgorithm algorithm, AmiKeyHandle key,
    const uint8_t *nonce, size_t nonce_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *tag, size_t tag_len,
    uint8_t *plaintext)
{
    (void)user; (void)algorithm; (void)key; (void)nonce; (void)nonce_len; (void)aad; (void)aad_len;
    (void)ciphertext; (void)ciphertext_len; (void)tag; (void)tag_len;
    // Dirty buffer before failure to verify zeroing
    memset(plaintext, 0xFF, ciphertext_len);
    return AMI_SEC_AUTHENTICATION_FAILURE;
}

// --- Test Scenarios ---

static void test_backend_validation(void) {
    AmiSecBackend backend = {0};
    assert(ami_sec_backend_validate(NULL) == AMI_SEC_INVALID_ARGUMENT);
    assert(ami_sec_backend_validate(&backend) == AMI_SEC_INVALID_ARGUMENT);

    backend.random = mock_random_success;
    assert(ami_sec_backend_validate(&backend) == AMI_SEC_INVALID_ARGUMENT);

    backend.aead_encrypt = mock_aead_encrypt_success;
    assert(ami_sec_backend_validate(&backend) == AMI_SEC_INVALID_ARGUMENT);

    backend.aead_decrypt = mock_aead_decrypt_auth_fail;
    assert(ami_sec_backend_validate(&backend) == AMI_SEC_OK);

    printf("  [PASS] test_backend_validation\n");
}

static void test_random_generation_and_clearing(void) {
    AmiSecBackend backend = {
        .random = mock_random_success,
        .aead_encrypt = mock_aead_encrypt_success,
        .aead_decrypt = mock_aead_decrypt_auth_fail
    };

    uint8_t buf[16] = {0};
    assert(ami_sec_random(&backend, buf, sizeof(buf)) == AMI_SEC_OK);
    assert(buf[0] == 1 && buf[1] == 2);

    // Fail random test -> verify zeroing
    backend.random = mock_random_failure;
    memset(buf, 0xEE, sizeof(buf));
    assert(ami_sec_random(&backend, buf, sizeof(buf)) == AMI_SEC_BACKEND_FAILURE);
    for (size_t i = 0; i < sizeof(buf); ++i) {
        assert(buf[i] == 0); // Must be wiped to 0
    }

    printf("  [PASS] test_random_generation_and_clearing\n");
}

static void test_nonce_and_tag_bounds_enforcement(void) {
    AmiSecBackend backend = {
        .random = mock_random_success,
        .aead_encrypt = mock_aead_encrypt_success,
        .aead_decrypt = mock_aead_decrypt_auth_fail
    };

    uint8_t nonce[16] = {0};
    uint8_t tag[16] = {0};
    uint8_t plain[8] = "12345678";
    uint8_t cipher[8] = {0};

    // Nonce must be strictly 12 bytes
    assert(ami_sec_aead_encrypt(&backend, AMI_AEAD_AES_128_GCM, 1,
        nonce, 8, NULL, 0, plain, 8, cipher, tag, 16) == AMI_SEC_INVALID_ARGUMENT);
    assert(ami_sec_aead_encrypt(&backend, AMI_AEAD_AES_128_GCM, 1,
        nonce, 16, NULL, 0, plain, 8, cipher, tag, 16) == AMI_SEC_INVALID_ARGUMENT);

    // Tag must be strictly 16 bytes
    assert(ami_sec_aead_encrypt(&backend, AMI_AEAD_AES_128_GCM, 1,
        nonce, 12, NULL, 0, plain, 8, cipher, tag, 12) == AMI_SEC_INVALID_ARGUMENT);

    // Valid 12-byte nonce & 16-byte tag -> SUCCESS
    assert(ami_sec_aead_encrypt(&backend, AMI_AEAD_AES_128_GCM, 1,
        nonce, 12, NULL, 0, plain, 8, cipher, tag, 16) == AMI_SEC_OK);

    printf("  [PASS] test_nonce_and_tag_bounds_enforcement\n");
}

static void test_encryption_failure_buffer_wipe(void) {
    AmiSecBackend backend = {
        .random = mock_random_success,
        .aead_encrypt = mock_aead_encrypt_fail,
        .aead_decrypt = mock_aead_decrypt_auth_fail
    };

    uint8_t nonce[12] = {0};
    uint8_t tag[16] = {0xFF};
    uint8_t plain[4] = "TEST";
    uint8_t cipher[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    AmiSecStatus status = ami_sec_aead_encrypt(
        &backend, AMI_AEAD_AES_256_GCM, 1,
        nonce, 12, NULL, 0, plain, 4, cipher, tag, 16
    );

    assert(status == AMI_SEC_BACKEND_FAILURE);
    // Ciphertext and Tag must be completely zeroed upon failure
    for (int i = 0; i < 4; ++i) assert(cipher[i] == 0);
    for (int i = 0; i < 16; ++i) assert(tag[i] == 0);

    printf("  [PASS] test_encryption_failure_buffer_wipe\n");
}

static void test_decryption_auth_failure_wipe(void) {
    AmiSecBackend backend = {
        .random = mock_random_success,
        .aead_encrypt = mock_aead_encrypt_success,
        .aead_decrypt = mock_aead_decrypt_auth_fail
    };

    uint8_t nonce[12] = {0};
    uint8_t tag[16] = {0};
    uint8_t cipher[4] = {1, 2, 3, 4};
    uint8_t plain[4] = {0};

    AmiSecStatus status = ami_sec_aead_decrypt(
        &backend, AMI_AEAD_AES_128_GCM, 1,
        nonce, 12, NULL, 0, cipher, 4, tag, 16, plain
    );

    assert(status == AMI_SEC_AUTHENTICATION_FAILURE);
    // Plaintext buffer must be sanitized (zeroed) on auth failure
    for (int i = 0; i < 4; ++i) assert(plain[i] == 0);

    printf("  [PASS] test_decryption_auth_failure_wipe\n");
}

int main(void) {
    printf("========================================\n");
    printf("   RUNNING AMI SECURITY UNIT TESTS      \n");
    printf("========================================\n");

    test_backend_validation();
    test_random_generation_and_clearing();
    test_nonce_and_tag_bounds_enforcement();
    test_encryption_failure_buffer_wipe();
    test_decryption_auth_failure_wipe();

    printf("\n>>> ALL SECURITY TESTS PASSED SUCCESSFULLY! <<<\n\n");
    return 0;
}

