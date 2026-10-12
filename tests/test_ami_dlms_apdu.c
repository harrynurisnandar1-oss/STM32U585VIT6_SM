#include "ami_dlms_apdu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t rx_counter;
    uint32_t tx_counter;
    unsigned key_transfer_calls;
    size_t transferred_key_count;
} CipherTestState;

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

static AmiSecStatus accept_key_batch(void *user, const AmiDlmsWrappedKey *keys,
                                     size_t key_count)
{
    CipherTestState *state = user;
    if (keys == NULL || key_count == 0u || key_count > 2u) return AMI_SEC_INVALID_ARGUMENT;
    for (size_t i = 0u; i < key_count; ++i) {
        if ((keys[i].key_type != 0u && keys[i].key_type != 2u) ||
            keys[i].wrapped_key == NULL || keys[i].wrapped_key_length != 24u)
            return AMI_SEC_INVALID_ARGUMENT;
    }
    ++state->key_transfer_calls;
    state->transferred_key_count = key_count;
    return AMI_SEC_OK;
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

static void test_profile_entry_selective_access(void)
{
    AmiDlmsAssociation association = {.state = AMI_DLMS_ASSOC_ASSOCIATED};
    AmiCosemRegisterSnapshot snapshot = {0};
    snapshot.valid_mask = AMI_COSEM_VALID_EVENT_LOG;
    snapshot.event_log_count = 2u;
    snapshot.event_log[0].timestamp.year[0] = 0x07u;
    snapshot.event_log[0].timestamp.year[1] = 0xEAu;
    snapshot.event_log[0].timestamp.month = 10u;
    snapshot.event_log[0].timestamp.day = 12u;
    snapshot.event_log[0].timestamp.day_of_week = 0xFFu;
    snapshot.event_log[0].timestamp.deviation[0] = 0x80u;
    snapshot.event_log[0].timestamp.status = 0xFFu;
    snapshot.event_log[0].event_code = 1u;
    snapshot.event_log[1].timestamp.year[0] = 0x07u;
    snapshot.event_log[1].timestamp.year[1] = 0xEAu;
    snapshot.event_log[1].timestamp.month = 10u;
    snapshot.event_log[1].timestamp.day = 13u;
    snapshot.event_log[1].timestamp.day_of_week = 0xFFu;
    snapshot.event_log[1].timestamp.deviation[0] = 0x80u;
    snapshot.event_log[1].timestamp.status = 0xFFu;
    snapshot.event_log[1].event_code = 2u;
    const uint8_t request[] = {
        0xC0u, 0x01u, 0x55u, 0x00u, 0x07u, 0x00u, 0x00u, 0x63u, 0x62u, 0x00u, 0xFFu,
        0x02u, 0x01u, 0x02u, 0x02u, 0x04u,
        0x06u, 0x00u, 0x00u, 0x00u, 0x02u, /* from_entry = 2 */
        0x06u, 0x00u, 0x00u, 0x00u, 0x02u, /* to_entry = 2 */
        0x12u, 0x00u, 0x00u, 0x12u, 0x00u, 0x00u
    };
    uint8_t response[128];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
        sizeof(request), response, sizeof(response), &response_length) == AMI_DLMS_APDU_OK);
    assert(response_length > 6u && response[0] == 0xC4u && response[4] == 1u);
    assert(response[5] == 1u); /* exactly one row returned */
    assert(response[6] == AMI_COSEM_TYPE_STRUCTURE);
    assert(response[response_length - 2u] == AMI_COSEM_TYPE_UNSIGNED);
    assert(response[response_length - 1u] == 2u);
}

static void test_profile_clock_range_selective_access(void)
{
    AmiDlmsAssociation association = {.state = AMI_DLMS_ASSOC_ASSOCIATED};
    AmiCosemRegisterSnapshot snapshot = {0};
    snapshot.valid_mask = AMI_COSEM_VALID_EVENT_LOG;
    snapshot.event_log_count = 2u;
    snapshot.event_log[0].timestamp.year[0] = 0x07u;
    snapshot.event_log[0].timestamp.year[1] = 0xEAu;
    snapshot.event_log[0].timestamp.month = 10u;
    snapshot.event_log[0].timestamp.day = 12u;
    snapshot.event_log[0].timestamp.day_of_week = 0xFFu;
    snapshot.event_log[0].timestamp.deviation[0] = 0x80u;
    snapshot.event_log[0].timestamp.status = 0xFFu;
    snapshot.event_log[0].event_code = 4u;
    snapshot.event_log[1].timestamp.year[0] = 0x07u;
    snapshot.event_log[1].timestamp.year[1] = 0xEAu;
    snapshot.event_log[1].timestamp.month = 10u;
    snapshot.event_log[1].timestamp.day = 13u;
    snapshot.event_log[1].timestamp.day_of_week = 0xFFu;
    snapshot.event_log[1].timestamp.deviation[0] = 0x80u;
    snapshot.event_log[1].timestamp.status = 0xFFu;
    snapshot.event_log[1].event_code = 5u;
    const uint8_t request[] = {
        0xC0u, 0x01u, 0x56u, 0x00u, 0x07u, 0x00u, 0x00u, 0x63u, 0x62u, 0x00u, 0xFFu,
        0x02u, 0x01u, 0x01u, 0x02u, 0x04u,
        0x02u, 0x04u, 0x12u, 0x00u, 0x08u, 0x09u, 0x06u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0xFFu,
        0x0Fu, 0x02u, 0x12u, 0x00u, 0x00u,
        0x09u, 0x0Cu, 0x07u, 0xEAu, 0x0Au, 0x0Cu, 0xFFu, 0u, 0u, 0u, 0u, 0x80u, 0u, 0xFFu,
        0x09u, 0x0Cu, 0x07u, 0xEAu, 0x0Au, 0x0Cu, 0xFFu, 0x17u, 0x3Bu, 0x3Bu, 0u, 0x80u, 0u, 0xFFu,
        0x01u, 0x00u
    };
    uint8_t response[128];
    size_t response_length = 0u;
    assert(ami_dlms_process_get_request(&association, &snapshot, request,
        sizeof(request), response, sizeof(response), &response_length) == AMI_DLMS_APDU_OK);
    assert(response_length > 6u && response[0] == 0xC4u && response[4] == 1u);
    assert(response[5] == 1u); /* only the matching date is returned */
    assert(response[response_length - 1u] == 4u);
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

static void test_secure_setup_key_transfer_action(void)
{
    AmiDlmsAssociation association = {
        .state = AMI_DLMS_ASSOC_ASSOCIATED, .mechanism = AMI_DLMS_AUTH_HLS
    };
    const AmiCosemRegisterSnapshot snapshot = {0};
    CipherTestState state = {0};
    const AmiDlmsCipherBackend cipher = {
        .user = &state, .expected_security_control = 0x30u,
        .decrypt = fake_decrypt, .encrypt = fake_encrypt,
        .accept_rx_counter = accept_counter, .next_tx_counter = next_counter,
        .transfer_keys = accept_key_batch
    };
    const uint8_t plain_action[] = {
        0xC3u, 0x01u, 0x77u, 0x00u, 0x40u,
        0x00u, 0x00u, 0x2Bu, 0x00u, 0x00u, 0xFFu,
        0x02u, 0x01u, 0x01u, 0x01u,
        0x02u, 0x02u, 0x16u, 0x00u, 0x09u, 0x18u,
        1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u,
        13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u
    };
    uint8_t encrypted_request[96], encrypted_response[96], workspace[96];
    size_t response_length = 0u;
    const size_t request_length = make_glo_request(encrypted_request, 0xCBu,
        1u, plain_action, sizeof(plain_action), 0);
    AmiDlmsApduStatus action_status = ami_dlms_process_ciphered_request(&association, &snapshot, &cipher,
        encrypted_request, request_length, workspace, sizeof(workspace),
        encrypted_response, sizeof(encrypted_response), &response_length);
    assert(action_status == AMI_DLMS_APDU_OK);
    assert(state.key_transfer_calls == 1u && state.transferred_key_count == 1u);
    assert(encrypted_response[0] == 0xCEu && response_length == 6u + 5u + AMI_DLMS_GCM_TAG_LENGTH);
    assert((encrypted_response[6] ^ 0xAAu) == 0xC7u);
    assert((encrypted_response[9] ^ 0xAAu) == 0u); /* action-result success */

    /* A plain key-transfer Action APDU is never accepted after HLS. */
    assert(ami_dlms_process_get_request(&association, &snapshot, plain_action,
        sizeof(plain_action), workspace, sizeof(workspace), &response_length) ==
        AMI_DLMS_APDU_CIPHER_REQUIRED);
}

int main(void)
{
    test_voltage_get_request_response();
    test_unknown_obis_returns_access_error();
    test_profile_entry_selective_access();
    test_profile_clock_range_selective_access();
    test_hls_requires_ciphered_apdu_and_returns_ciphered_response();
    test_cipher_tamper_and_set_read_only_response();
    test_secure_setup_key_transfer_action();
    puts("DLMS APDU tests passed.");
    return 0;
}
