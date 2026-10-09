#include "ami_dlms_apdu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t rx_counter; uint32_t tx_counter; } CipherTestState;

static void fake_tag(uint8_t security_control, uint32_t counter,
                     const uint8_t *data, size_t length,
                     uint8_t tag[AMI_DLMS_GCM_TAG_LENGTH])
{
    uint32_t sum = security_control + (counter >> 24) + (counter >> 16) +
                   (counter >> 8) + counter;
    for (size_t i = 0u; i < length; ++i) sum += data[i];
    for (size_t i = 0u; i < AMI_DLMS_GCM_TAG_LENGTH; ++i)
        tag[i] = (uint8_t)(sum + (uint32_t)i);
}

static AmiSecStatus fake_encrypt(void *user, uint8_t sc, uint32_t counter,
    const uint8_t *plain, size_t plain_len, uint8_t *cipher, size_t capacity,
    size_t *cipher_len, uint8_t tag[AMI_DLMS_GCM_TAG_LENGTH])
{
    (void)user;
    if (capacity < plain_len) return AMI_SEC_INVALID_ARGUMENT;
    for (size_t i = 0u; i < plain_len; ++i) cipher[i] = plain[i] ^ 0xAAu;
    *cipher_len = plain_len;
    fake_tag(sc, counter, cipher, plain_len, tag);
    return AMI_SEC_OK;
}

static AmiSecStatus fake_decrypt(void *user, uint8_t sc, uint32_t counter,
    const uint8_t *cipher, size_t cipher_len,
    const uint8_t tag[AMI_DLMS_GCM_TAG_LENGTH], uint8_t *plain,
    size_t capacity, size_t *plain_len)
{
    (void)user;
    uint8_t expected[AMI_DLMS_GCM_TAG_LENGTH];
    uint8_t diff = 0u;
    if (capacity < cipher_len) return AMI_SEC_INVALID_ARGUMENT;
    fake_tag(sc, counter, cipher, cipher_len, expected);
    for (size_t i = 0u; i < sizeof(expected); ++i) diff |= expected[i] ^ tag[i];
    if (diff != 0u) return AMI_SEC_AUTHENTICATION_FAILURE;
    for (size_t i = 0u; i < cipher_len; ++i) plain[i] = cipher[i] ^ 0xAAu;
    *plain_len = cipher_len;
    return AMI_SEC_OK;
}

static int accept_counter(void *user, uint32_t counter)
{
    CipherTestState *state = user;
    if (counter <= state->rx_counter) return 0;
    state->rx_counter = counter;
    return 1;
}

static int next_counter(void *user, uint32_t *counter)
{
    CipherTestState *state = user;
    *counter = ++state->tx_counter;
    return 1;
}

static size_t make_glo_request(uint8_t *out, uint8_t tag,
                               uint32_t counter, const uint8_t *plain,
                               size_t plain_length, int corrupt_tag)
{
    out[0] = tag;
    out[1] = 0x30u;
    out[2] = (uint8_t)(counter >> 24);
    out[3] = (uint8_t)(counter >> 16);
    out[4] = (uint8_t)(counter >> 8);
    out[5] = (uint8_t)counter;
    for (size_t i = 0u; i < plain_length; ++i) out[6u + i] = plain[i] ^ 0xAAu;
    fake_tag(0x30u, counter, out + 6u, plain_length,
             out + 6u + plain_length);
    if (corrupt_tag) out[6u + plain_length] ^= 1u;
    return 6u + plain_length + AMI_DLMS_GCM_TAG_LENGTH;
}

static void test_voltage_get_request_response(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_ASSOCIATED};
    const AmiCosemRegisterSnapshot snapshot = {
        .phase_voltage_millivolts = {230000u, 229000u, 231000u},
        .valid_mask = AMI_COSEM_VALID_VOLTAGE_A
    };
    const uint8_t request[] = {
        0xC0u, 0x01u, 0xC1u, 0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };
    const uint8_t expected[] = {
        0xC4u, 0x01u, 0xC1u, 0x00u, 0x06u,
        0x00u, 0x03u, 0x82u, 0x70u
    };
    uint8_t response[32];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
             sizeof(request), response, sizeof(response), &response_length) ==
           AMI_DLMS_APDU_OK);
    assert(response_length == sizeof(expected));
    for (size_t i = 0u; i < sizeof(expected); ++i) {
        assert(response[i] == expected[i]);
    }
}

static void test_unknown_obis_returns_access_error(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_ASSOCIATED};
    const AmiCosemRegisterSnapshot snapshot = {0};
    const uint8_t request[] = {
        0xC0u, 0x01u, 0xC2u, 0x00u, 0x03u,
        0x01u, 0x00u, 0xFAu, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };
    uint8_t response[8];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
             sizeof(request), response, sizeof(response), &response_length) ==
           AMI_DLMS_APDU_ACCESS_ERROR);
    assert(response_length == 5u);
    assert(response[0] == 0xC4u && response[1] == 0x01u &&
           response[2] == 0xC2u && response[3] == 1u && response[4] == 4u);
}

static void test_hls_requires_ciphered_apdu_and_returns_ciphered_response(void)
{
    AmiDlmsAssociation association = {
        .state = AMI_DLMS_ASSOC_ASSOCIATED, .mechanism = AMI_DLMS_AUTH_HLS
    };
    const AmiCosemRegisterSnapshot snapshot = {
        .phase_voltage_millivolts = {230000u, 0u, 0u},
        .valid_mask = AMI_COSEM_VALID_VOLTAGE_A
    };
    const uint8_t plain_get[] = {0xC0u, 0x01u, 0xC1u, 0x00u, 0x03u,
        1u, 0u, 32u, 7u, 0u, 255u, 2u, 0u};
    uint8_t plain_response[32], workspace[64];
    size_t length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, plain_get,
        sizeof(plain_get), plain_response, sizeof(plain_response), &length) ==
        AMI_DLMS_APDU_CIPHER_REQUIRED);

    CipherTestState state = {0};
    const AmiDlmsCipherBackend cipher = {
        .user = &state, .expected_security_control = 0x30u,
        .decrypt = fake_decrypt, .encrypt = fake_encrypt,
        .accept_rx_counter = accept_counter, .next_tx_counter = next_counter
    };
    uint8_t encrypted_request[64], encrypted_response[64];
    const size_t request_length = make_glo_request(encrypted_request, 0xC8u,
        1u, plain_get, sizeof(plain_get), 0);
    assert(ami_dlms_process_ciphered_request(&association, &snapshot, &cipher,
        encrypted_request, request_length, workspace, sizeof(workspace),
        encrypted_response, sizeof(encrypted_response), &length) == AMI_DLMS_APDU_OK);
    assert(length == 6u + 9u + AMI_DLMS_GCM_TAG_LENGTH);
    assert(encrypted_response[0] == 0xCCu && encrypted_response[1] == 0x30u);
    assert(encrypted_response[5] == 1u && state.rx_counter == 1u && state.tx_counter == 1u);
}

static void test_cipher_tamper_and_set_read_only_response(void)
{
    AmiDlmsAssociation association = {
        .state = AMI_DLMS_ASSOC_ASSOCIATED, .mechanism = AMI_DLMS_AUTH_HLS
    };
    const AmiCosemRegisterSnapshot snapshot = {0};
    CipherTestState state = {0};
    const AmiDlmsCipherBackend cipher = {
        .user = &state, .expected_security_control = 0x30u,
        .decrypt = fake_decrypt, .encrypt = fake_encrypt,
        .accept_rx_counter = accept_counter, .next_tx_counter = next_counter
    };
    const uint8_t plain_set[] = {0xC1u, 0x01u, 0xA5u, 0x00u, 0x03u,
        1u, 0u, 32u, 7u, 0u, 255u, 2u, 0u,
        0x06u, 0x00u, 0x03u, 0x82u, 0x70u};
    uint8_t encrypted_request[64], encrypted_response[64], workspace[64];
    size_t length = 0u;
    size_t request_length = make_glo_request(encrypted_request, 0xC9u,
        1u, plain_set, sizeof(plain_set), 1);
    assert(ami_dlms_process_ciphered_request(&association, &snapshot, &cipher,
        encrypted_request, request_length, workspace, sizeof(workspace),
        encrypted_response, sizeof(encrypted_response), &length) ==
        AMI_DLMS_APDU_AUTHENTICATION_FAILURE);
    assert(state.rx_counter == 0u);

    request_length = make_glo_request(encrypted_request, 0xC9u,
        1u, plain_set, sizeof(plain_set), 0);
    assert(ami_dlms_process_ciphered_request(&association, &snapshot, &cipher,
        encrypted_request, request_length, workspace, sizeof(workspace),
        encrypted_response, sizeof(encrypted_response), &length) ==
        AMI_DLMS_APDU_ACCESS_ERROR);
    assert(length == 6u + 4u + AMI_DLMS_GCM_TAG_LENGTH);
    assert(encrypted_response[0] == 0xCDu && encrypted_response[5] == 1u);
    assert(ami_dlms_process_ciphered_request(&association, &snapshot, &cipher,
        encrypted_request, request_length, workspace, sizeof(workspace),
        encrypted_response, sizeof(encrypted_response), &length) ==
        AMI_DLMS_APDU_COUNTER_REJECTED);
}

int main(void)
{
    test_voltage_get_request_response();
    test_unknown_obis_returns_access_error();
    test_hls_requires_ciphered_apdu_and_returns_ciphered_response();
    test_cipher_tamper_and_set_read_only_response();
    puts("DLMS APDU tests passed.");
    return 0;
}
