#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_security.h"

// --- Mock Backend Callbacks for testing the interface layer ---

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

// --- Test Scenarios: Interface & Boundary Sanitization ---

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

// --- Test Scenarios: NIST SP 800-38D Standard Known Answer Tests (KAT) ---

static void test_nist_sp800_38d_kat_vectors(void) {
    // -------------------------------------------------------------
    // NIST SP 800-38D Test Case 1 (Empty Plaintext, Empty AAD)
    // -------------------------------------------------------------
    {
        uint8_t k[16] = {0};
        uint8_t iv[12] = {0};
        uint8_t tag[16] = {0};

        assert(ami_sec_aes_128_gcm_encrypt(k, iv, 12, NULL, 0, NULL, 0, NULL, tag, 16) == AMI_SEC_OK);

        const uint8_t exp_tag[16] = {
            0x58u, 0xe2u, 0xfcu, 0xceu, 0xfau, 0x7eu, 0x30u, 0x61u,
            0x36u, 0x7fu, 0x1du, 0x57u, 0xa4u, 0xe7u, 0x45u, 0x5au
        };
        assert(memcmp(tag, exp_tag, 16) == 0);

        // Verify decryption/authentication of empty message
        assert(ami_sec_aes_128_gcm_decrypt(k, iv, 12, NULL, 0, NULL, 0, tag, 16, NULL) == AMI_SEC_OK);
    }

    // -------------------------------------------------------------
    // NIST SP 800-38D Test Case 2 (16-byte zero Plaintext, Empty AAD)
    // -------------------------------------------------------------
    {
        uint8_t k[16] = {0};
        uint8_t iv[12] = {0};
        uint8_t pt[16] = {0};
        uint8_t ct[16] = {0};
        uint8_t tag[16] = {0};

        assert(ami_sec_aes_128_gcm_encrypt(k, iv, 12, NULL, 0, pt, 16, ct, tag, 16) == AMI_SEC_OK);

        const uint8_t exp_ct[16] = {
            0x03u, 0x88u, 0xdau, 0xceu, 0x60u, 0xb6u, 0xa3u, 0x92u,
            0xf3u, 0x28u, 0xc2u, 0xb9u, 0x71u, 0xb2u, 0xfeu, 0x78u
        };
        const uint8_t exp_tag[16] = {
            0xabu, 0x6eu, 0x47u, 0xd4u, 0x2cu, 0xecu, 0x13u, 0xbdu,
            0xf5u, 0x3au, 0x67u, 0xb2u, 0x12u, 0x57u, 0xbdu, 0xdfu
        };
        assert(memcmp(ct, exp_ct, 16) == 0);
        assert(memcmp(tag, exp_tag, 16) == 0);

        // Decryption test
        uint8_t decrypted[16] = {0xFF};
        assert(ami_sec_aes_128_gcm_decrypt(k, iv, 12, NULL, 0, ct, 16, tag, 16, decrypted) == AMI_SEC_OK);
        assert(memcmp(decrypted, pt, 16) == 0);

        // Tamper test: corrupted tag must reject and wipe buffer
        uint8_t bad_tag[16];
        memcpy(bad_tag, tag, 16);
        bad_tag[15] ^= 0x01u;
        memset(decrypted, 0xAA, 16);
        assert(ami_sec_aes_128_gcm_decrypt(k, iv, 12, NULL, 0, ct, 16, bad_tag, 16, decrypted) == AMI_SEC_AUTHENTICATION_FAILURE);
        for (int i = 0; i < 16; ++i) assert(decrypted[i] == 0);
    }

    // -------------------------------------------------------------
    // NIST SP 800-38D Test Case 4 (60-byte Plaintext, 20-byte AAD)
    // -------------------------------------------------------------
    {
        const uint8_t k[16] = {
            0xfeu, 0xffu, 0xe9u, 0x92u, 0x86u, 0x65u, 0x73u, 0x1cu,
            0x6du, 0x6au, 0x8fu, 0x94u, 0x67u, 0x30u, 0x83u, 0x08u
        };
        const uint8_t iv[12] = {
            0xcau, 0xfeu, 0xbau, 0xbeu, 0xfau, 0xceu, 0xdbu, 0xadu,
            0xdeu, 0xcau, 0xf8u, 0x88u
        };
        const uint8_t pt[60] = {
            0xd9u, 0x31u, 0x32u, 0x25u, 0xf8u, 0x84u, 0x06u, 0xe5u,
            0xa5u, 0x59u, 0x09u, 0xc5u, 0xafu, 0xf5u, 0x26u, 0x9au,
            0x86u, 0xa7u, 0xa9u, 0x53u, 0x15u, 0x34u, 0xf7u, 0xdau,
            0x2eu, 0x4cu, 0x30u, 0x3du, 0x8au, 0x31u, 0x8au, 0x72u,
            0x1cu, 0x3cu, 0x0cu, 0x95u, 0x95u, 0x68u, 0x09u, 0x53u,
            0x2fu, 0xcfu, 0x0eu, 0x24u, 0x49u, 0xa6u, 0xb5u, 0x25u,
            0xb1u, 0x6au, 0xedu, 0xf5u, 0xaau, 0x0du, 0xe6u, 0x57u,
            0xbau, 0x63u, 0x7bu, 0x39u
        };
        const uint8_t aad[20] = {
            0xfeu, 0xedu, 0xfau, 0xceu, 0xdeu, 0xadu, 0xbeu, 0xefu,
            0xfeu, 0xedu, 0xfau, 0xceu, 0xdeu, 0xadu, 0xbeu, 0xefu,
            0xabu, 0xadu, 0xdau, 0xd2u
        };

        uint8_t ct[60] = {0};
        uint8_t tag[16] = {0};

        assert(ami_sec_aes_128_gcm_encrypt(k, iv, 12, aad, 20, pt, 60, ct, tag, 16) == AMI_SEC_OK);

        const uint8_t exp_ct[60] = {
            0x42u, 0x83u, 0x1eu, 0xc2u, 0x21u, 0x77u, 0x74u, 0x24u,
            0x4bu, 0x72u, 0x21u, 0xb7u, 0x84u, 0xd0u, 0xd4u, 0x9cu,
            0xe3u, 0xaau, 0x21u, 0x2fu, 0x2cu, 0x02u, 0xa4u, 0xe0u,
            0x35u, 0xc1u, 0x7eu, 0x23u, 0x29u, 0xacu, 0xa1u, 0x2eu,
            0x21u, 0xd5u, 0x14u, 0xb2u, 0x54u, 0x66u, 0x93u, 0x1cu,
            0x7du, 0x8fu, 0x6au, 0x5au, 0xacu, 0x84u, 0xaau, 0x05u,
            0x1bu, 0xa3u, 0x0bu, 0x39u, 0x6au, 0x0au, 0xacu, 0x97u,
            0x3du, 0x58u, 0xe0u, 0x91u
        };
        const uint8_t exp_tag[16] = {
            0x5bu, 0xc9u, 0x4fu, 0xbcu, 0x32u, 0x21u, 0xa5u, 0xdbu,
            0x94u, 0xfau, 0xe9u, 0x5au, 0xe7u, 0x12u, 0x1au, 0x47u
        };

        assert(memcmp(ct, exp_ct, 60) == 0);
        assert(memcmp(tag, exp_tag, 16) == 0);

        // Decrypt and verify
        uint8_t decrypted[60] = {0};
        assert(ami_sec_aes_128_gcm_decrypt(k, iv, 12, aad, 20, ct, 60, tag, 16, decrypted) == AMI_SEC_OK);
        assert(memcmp(decrypted, pt, 60) == 0);

        // Tamper test: AAD modification must cause authentication failure
        uint8_t bad_aad[20];
        memcpy(bad_aad, aad, 20);
        bad_aad[0] ^= 0x01u;
        assert(ami_sec_aes_128_gcm_decrypt(k, iv, 12, bad_aad, 20, ct, 60, tag, 16, decrypted) == AMI_SEC_AUTHENTICATION_FAILURE);
    }

    printf("  [PASS] test_nist_sp800_38d_kat_vectors\n");
}

static void test_standalone_gmac(void) {
    const uint8_t k[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    const uint8_t iv[12] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B
    };
    const uint8_t data[24] = "GMAC_TEST_DATA_AUTHENTIC";

    uint8_t tag1[16] = {0};
    uint8_t tag2[16] = {0};

    assert(ami_sec_aes_128_gmac(k, iv, 12, data, sizeof(data), tag1, 16) == AMI_SEC_OK);
    // GMAC must be deterministic
    assert(ami_sec_aes_128_gmac(k, iv, 12, data, sizeof(data), tag2, 16) == AMI_SEC_OK);
    assert(memcmp(tag1, tag2, 16) == 0);

    // Altering data must produce a different tag
    uint8_t tag_diff[16] = {0};
    const uint8_t data_altered[24] = "GMAC_TEST_DATA_MODIFIED!";
    assert(ami_sec_aes_128_gmac(k, iv, 12, data_altered, sizeof(data_altered), tag_diff, 16) == AMI_SEC_OK);
    assert(memcmp(tag1, tag_diff, 16) != 0);

    printf("  [PASS] test_standalone_gmac\n");
}

static void test_dlms_hls5_gmac_calculate_and_verify(void) {
    const uint8_t auth_key[16] = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
    };
    const uint8_t sys_title[8] = {'P', 'L', 'N', 0x00, 0x00, 0x00, 0x01, 0x02};
    const uint32_t ic = 10045u;
    const uint8_t server_challenge[16] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
    };

    uint8_t proof[32] = {0};
    size_t proof_len = 0;

    // 1. Calculate HLS 5 proof (21 bytes expected: SC || IC || Tag)
    AmiSecStatus status = ami_sec_dlms_hls5_gmac_calculate(
        auth_key, sys_title, ic, server_challenge, sizeof(server_challenge),
        proof, sizeof(proof), &proof_len);
    assert(status == AMI_SEC_OK);
    assert(proof_len == AMI_DLMS_HLS5_PROOF_LEN);
    assert(proof[0] == AMI_DLMS_HLS5_SC_AUTH_ONLY);
    assert(proof[1] == (uint8_t)(ic >> 24));
    assert(proof[2] == (uint8_t)(ic >> 16));
    assert(proof[3] == (uint8_t)(ic >> 8));
    assert(proof[4] == (uint8_t)(ic));

    // 2. Positive verification
    assert(ami_sec_dlms_hls5_gmac_verify(
        auth_key, sys_title, proof, proof_len, server_challenge, sizeof(server_challenge)) == AMI_SEC_OK);

    // 3. Negative verification: Tampered challenge
    uint8_t bad_chal[16];
    memcpy(bad_chal, server_challenge, 16);
    bad_chal[0] ^= 0x01u;
    assert(ami_sec_dlms_hls5_gmac_verify(
        auth_key, sys_title, proof, proof_len, bad_chal, sizeof(bad_chal)) == AMI_SEC_AUTHENTICATION_FAILURE);

    // 4. Negative verification: Tampered Invocation Counter (Replay / Desync)
    uint8_t bad_proof[AMI_DLMS_HLS5_PROOF_LEN];
    memcpy(bad_proof, proof, AMI_DLMS_HLS5_PROOF_LEN);
    bad_proof[4] ^= 0x01u; // mutate IC byte
    assert(ami_sec_dlms_hls5_gmac_verify(
        auth_key, sys_title, bad_proof, proof_len, server_challenge, sizeof(server_challenge)) == AMI_SEC_AUTHENTICATION_FAILURE);

    // 5. Negative verification: Incorrect Authentication Key
    uint8_t bad_key[16];
    memcpy(bad_key, auth_key, 16);
    bad_key[0] ^= 0xFFu;
    assert(ami_sec_dlms_hls5_gmac_verify(
        bad_key, sys_title, proof, proof_len, server_challenge, sizeof(server_challenge)) == AMI_SEC_AUTHENTICATION_FAILURE);

    // 6. Negative verification: Incorrect System Title
    uint8_t bad_title[8];
    memcpy(bad_title, sys_title, 8);
    bad_title[7] ^= 0x01u;
    assert(ami_sec_dlms_hls5_gmac_verify(
        auth_key, bad_title, proof, proof_len, server_challenge, sizeof(server_challenge)) == AMI_SEC_AUTHENTICATION_FAILURE);

    // 7. Negative verification: Invalid SC Byte
    bad_proof[0] = 0x00u; // Must be 0x10
    assert(ami_sec_dlms_hls5_gmac_verify(
        auth_key, sys_title, bad_proof, proof_len, server_challenge, sizeof(server_challenge)) == AMI_SEC_AUTHENTICATION_FAILURE);

    // 8. Raw 16-byte tag output format (when capacity is exactly 16)
    uint8_t raw_tag[16] = {0};
    size_t raw_tag_len = 0;
    assert(ami_sec_dlms_hls5_gmac_calculate(
        auth_key, sys_title, ic, server_challenge, sizeof(server_challenge),
        raw_tag, 16, &raw_tag_len) == AMI_SEC_OK);
    assert(raw_tag_len == 16);
    // Raw tag matches tag portion of 21-byte proof
    assert(memcmp(raw_tag, proof + 5, 16) == 0);

    printf("  [PASS] test_dlms_hls5_gmac_calculate_and_verify\n");
}

static void test_dlms_hls5_gmac_callback_integration(void) {
    const uint8_t hls5_oid[] = {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u};
    const uint8_t wrong_oid[] = {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x01u}; // LLS OID

    AmiHls5GmacContext ctx = {
        .auth_key = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10},
        .system_title = {'M','E','T','E','R','0','0','1'},
        .invocation_counter = 500u
    };

    uint8_t chal[16] = "RANDOM_CHALLENGE";
    uint8_t proof[32] = {0};
    size_t proof_len = 0;

    // Must accept valid HLS 5 OID
    int ok = ami_sec_hls5_gmac_proof_callback(
        &ctx, hls5_oid, sizeof(hls5_oid), chal, 16, proof, sizeof(proof), &proof_len);
    assert(ok == 1);
    assert(proof_len == AMI_DLMS_HLS5_PROOF_LEN);

    // Must reject non-HLS5 mechanism OID
    ok = ami_sec_hls5_gmac_proof_callback(
        &ctx, wrong_oid, sizeof(wrong_oid), chal, 16, proof, sizeof(proof), &proof_len);
    assert(ok == 0);

    printf("  [PASS] test_dlms_hls5_gmac_callback_integration\n");
}

static void test_software_backend_adapter(void) {
    const uint8_t key[16] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };
    AmiSecBackend backend;
    assert(ami_sec_init_software_backend(&backend, key) == AMI_SEC_OK);
    assert(ami_sec_backend_validate(&backend) == AMI_SEC_OK);

    uint8_t nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t aad[8] = "SECURITY";
    uint8_t plaintext[16] = "HELLO_DLMS_COSEM";
    uint8_t ciphertext[16] = {0};
    uint8_t tag[16] = {0};
    uint8_t decrypted[16] = {0};

    // Encrypt via facade
    assert(ami_sec_aead_encrypt(
        &backend, AMI_AEAD_AES_128_GCM, 1, nonce, 12, aad, 8,
        plaintext, 16, ciphertext, tag, 16) == AMI_SEC_OK);

    // Decrypt via facade
    assert(ami_sec_aead_decrypt(
        &backend, AMI_AEAD_AES_128_GCM, 1, nonce, 12, aad, 8,
        ciphertext, 16, tag, 16, decrypted) == AMI_SEC_OK);
    assert(memcmp(plaintext, decrypted, 16) == 0);

    // Random generation test via software backend
    uint8_t rand_buf[8] = {0};
    assert(ami_sec_random(&backend, rand_buf, sizeof(rand_buf)) == AMI_SEC_OK);

    printf("  [PASS] test_software_backend_adapter\n");
}

static void test_gmac_exhaustive_bit_flip_tampering(void) {
    const uint8_t key[16] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    const uint8_t iv[12] = {
        'P', 'L', 'N', '1', '2', '3', '4', '5', 0x00, 0x00, 0x00, 0x01
    };
    const uint8_t aad[33] = {
        0x10, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF
    };
    uint8_t original_tag[16] = {0};
    assert(ami_sec_aes_128_gmac(key, iv, 12, aad, sizeof(aad), original_tag, sizeof(original_tag)) == AMI_SEC_OK);

    /* Test 1: Flip each of the 16 tag bytes one by one */
    for (size_t byte_idx = 0; byte_idx < 16; ++byte_idx) {
        uint8_t tampered_tag[16];
        memcpy(tampered_tag, original_tag, 16);
        tampered_tag[byte_idx] ^= 0x01u; /* 1-bit flip */
        assert(memcmp(tampered_tag, original_tag, 16) != 0);
    }

    /* Test 2: Tamper with AAD bit-by-bit and verify GMAC tag mismatch */
    for (size_t aad_idx = 0; aad_idx < sizeof(aad); ++aad_idx) {
        uint8_t tampered_aad[sizeof(aad)];
        memcpy(tampered_aad, aad, sizeof(aad));
        tampered_aad[aad_idx] ^= 0x80u;
        uint8_t new_tag[16] = {0};
        assert(ami_sec_aes_128_gmac(key, iv, 12, tampered_aad, sizeof(aad), new_tag, sizeof(new_tag)) == AMI_SEC_OK);
        assert(memcmp(new_tag, original_tag, 16) != 0);
    }

    /* Test 3: Tamper with IV / Invocation Counter and verify GMAC tag mismatch */
    for (size_t iv_idx = 0; iv_idx < 12; ++iv_idx) {
        uint8_t tampered_iv[12];
        memcpy(tampered_iv, iv, 12);
        tampered_iv[iv_idx] ^= 0x55u;
        uint8_t new_tag[16] = {0};
        assert(ami_sec_aes_128_gmac(key, tampered_iv, 12, aad, sizeof(aad), new_tag, sizeof(new_tag)) == AMI_SEC_OK);
        assert(memcmp(new_tag, original_tag, 16) != 0);
    }

    printf("  [PASS] test_gmac_exhaustive_bit_flip_tampering\n");
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
    test_nist_sp800_38d_kat_vectors();
    test_standalone_gmac();
    test_dlms_hls5_gmac_calculate_and_verify();
    test_dlms_hls5_gmac_callback_integration();
    test_software_backend_adapter();
    test_gmac_exhaustive_bit_flip_tampering();

    printf("\n>>> ALL SECURITY & HLS 5 GMAC TESTS PASSED SUCCESSFULLY! <<<\n\n");
    return 0;
}
