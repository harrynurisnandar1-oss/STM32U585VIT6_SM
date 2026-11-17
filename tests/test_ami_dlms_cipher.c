#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_dlms_cipher.h"
#include "ami_dlms_port.h"

/* Wire buffer capture */
static uint8_t s_last_tx[512];
static size_t s_last_tx_len = 0;

static int mock_tx_func(const uint8_t *data, size_t length, void *user_data)
{
    (void)user_data;
    if (length <= sizeof(s_last_tx)) {
        memcpy(s_last_tx, data, length);
        s_last_tx_len = length;
        return 1;
    }
    return 0;
}

static const uint8_t TEST_GUEK[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const uint8_t TEST_AK[16] = {
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
};

static const uint8_t CLIENT_TITLE[8] = {'P', 'L', 'N', 'C', 'L', 'I', '0', '1'};
static const uint8_t SERVER_TITLE[8] = {'P', 'L', 'N', 'M', 'T', 'R', '0', '1'};

static void test_cipher_suite0_glo_get_roundtrip(void)
{
    AmiDlmsSecuritySuite0Context cipher_ctx;
    assert(ami_dlms_cipher_suite0_init(&cipher_ctx, TEST_GUEK, TEST_AK,
                                       CLIENT_TITLE, SERVER_TITLE, 100u) == 0);

    AmiDlmsCipherBackend backend;
    assert(ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend) == 0);

    /* Prepare Association in Associated HLS state */
    AmiDlmsAssociation assoc;
    memset(&assoc, 0, sizeof(assoc));
    assoc.state = AMI_DLMS_ASSOC_ASSOCIATED;
    assoc.mechanism = AMI_DLMS_AUTH_HLS;

    AmiCosemRegisterSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.phase_voltage_millivolts[0] = 230450u; /* 230.45 V */
    snapshot.valid_mask = AMI_COSEM_VALID_VOLTAGE_A;

    /* Plaintext GET request for Voltage Phase A: Class 3, 1.0.32.7.0.255, Attr 2 */
    const uint8_t plain_get[] = {
        0xC0u, 0x01u, 0x81u,
        0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };

    uint8_t glo_req[128];
    size_t glo_req_len = 0;
    assert(ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 1u,
                                                    plain_get, sizeof(plain_get),
                                                    glo_req, sizeof(glo_req),
                                                    &glo_req_len) == 0);

    assert(glo_req[0] == 0xC8u); /* GLO_GET_REQUEST_TAG */
    assert(glo_req[2] == 0x30u); /* SC */
    assert(glo_req[6] == 1u);    /* IC = 1 */

    uint8_t workspace[256];
    uint8_t glo_resp[256];
    size_t glo_resp_len = 0;

    AmiDlmsApduStatus st = ami_dlms_process_ciphered_request(
        &assoc, &snapshot, &backend,
        glo_req, glo_req_len,
        workspace, sizeof(workspace),
        glo_resp, sizeof(glo_resp),
        &glo_resp_len);

    assert(st == AMI_DLMS_APDU_OK);
    assert(glo_resp[0] == 0xCCu); /* GLO_GET_RESPONSE_TAG */
    assert(glo_resp[2] == 0x30u); /* SC */
    assert(glo_resp[6] == 101u);  /* Server TX IC incremented to 101 */

    /* Decrypt glo_resp to verify plaintext content */
    const size_t cipher_len = glo_resp_len - 7u - AMI_DLMS_GCM_TAG_LENGTH;
    uint8_t decrypted_resp[128];
    size_t dec_len = 0;

    /* Build server nonce */
    uint8_t srv_nonce[12];
    memcpy(srv_nonce, SERVER_TITLE, 8);
    srv_nonce[8]  = 0;
    srv_nonce[9]  = 0;
    srv_nonce[10] = 0;
    srv_nonce[11] = 101u;

    uint8_t srv_aad[17];
    srv_aad[0] = 0x30u;
    memcpy(&srv_aad[1], TEST_AK, 16);

    AmiSecStatus sec_st = ami_sec_aes_128_gcm_decrypt(
        TEST_GUEK, srv_nonce, sizeof(srv_nonce),
        srv_aad, sizeof(srv_aad),
        glo_resp + 7u, cipher_len,
        glo_resp + 7u + cipher_len, AMI_DLMS_GCM_TAG_LENGTH,
        decrypted_resp);

    assert(sec_st == AMI_SEC_OK);
    assert(decrypted_resp[0] == 0xC4u); /* GET-Response-Normal */
    assert(decrypted_resp[1] == 0x01u);
    assert(decrypted_resp[2] == 0x81u); /* invoke-id matches */
    assert(decrypted_resp[3] == 0x00u); /* result = data */

    printf("test_cipher_suite0_glo_get_roundtrip: PASS\n");
}

static void test_cipher_suite0_anti_replay_invocation_counter(void)
{
    AmiDlmsSecuritySuite0Context cipher_ctx;
    ami_dlms_cipher_suite0_init(&cipher_ctx, TEST_GUEK, TEST_AK,
                                CLIENT_TITLE, SERVER_TITLE, 1u);
    AmiDlmsCipherBackend backend;
    ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend);

    AmiDlmsAssociation assoc = {.state = AMI_DLMS_ASSOC_ASSOCIATED, .mechanism = AMI_DLMS_AUTH_HLS};
    AmiCosemRegisterSnapshot snapshot = {
        .phase_voltage_millivolts = {230000u, 230000u, 230000u},
        .valid_mask = AMI_COSEM_VALID_VOLTAGE_A
    };

    const uint8_t plain_get[] = {
        0xC0u, 0x01u, 0x81u,
        0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };

    uint8_t req_100[128];
    size_t req_100_len = 0;
    ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 100u,
                                             plain_get, sizeof(plain_get),
                                             req_100, sizeof(req_100), &req_100_len);

    uint8_t workspace[256];
    uint8_t resp[256];
    size_t resp_len = 0;

    /* First request with IC=100 -> Must Succeed */
    assert(ami_dlms_process_ciphered_request(&assoc, &snapshot, &backend,
                                            req_100, req_100_len,
                                            workspace, sizeof(workspace),
                                            resp, sizeof(resp), &resp_len) == AMI_DLMS_APDU_OK);

    /* Replay attack with same IC=100 -> Must be REJECTED! */
    assert(ami_dlms_process_ciphered_request(&assoc, &snapshot, &backend,
                                            req_100, req_100_len,
                                            workspace, sizeof(workspace),
                                            resp, sizeof(resp), &resp_len) == AMI_DLMS_APDU_COUNTER_REJECTED);

    /* Expired request with older IC=99 -> Must be REJECTED! */
    uint8_t req_99[128];
    size_t req_99_len = 0;
    ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 99u,
                                             plain_get, sizeof(plain_get),
                                             req_99, sizeof(req_99), &req_99_len);
    assert(ami_dlms_process_ciphered_request(&assoc, &snapshot, &backend,
                                            req_99, req_99_len,
                                            workspace, sizeof(workspace),
                                            resp, sizeof(resp), &resp_len) == AMI_DLMS_APDU_COUNTER_REJECTED);

    /* Fresh request with newer IC=101 -> Must Succeed */
    uint8_t req_101[128];
    size_t req_101_len = 0;
    ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 101u,
                                             plain_get, sizeof(plain_get),
                                             req_101, sizeof(req_101), &req_101_len);
    assert(ami_dlms_process_ciphered_request(&assoc, &snapshot, &backend,
                                            req_101, req_101_len,
                                            workspace, sizeof(workspace),
                                            resp, sizeof(resp), &resp_len) == AMI_DLMS_APDU_OK);

    printf("test_cipher_suite0_anti_replay_invocation_counter: PASS\n");
}

static void test_cipher_suite0_tag_tampering(void)
{
    AmiDlmsSecuritySuite0Context cipher_ctx;
    ami_dlms_cipher_suite0_init(&cipher_ctx, TEST_GUEK, TEST_AK,
                                CLIENT_TITLE, SERVER_TITLE, 1u);
    AmiDlmsCipherBackend backend;
    ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend);

    AmiDlmsAssociation assoc = {.state = AMI_DLMS_ASSOC_ASSOCIATED, .mechanism = AMI_DLMS_AUTH_HLS};
    AmiCosemRegisterSnapshot snapshot = {
        .phase_voltage_millivolts = {230000u, 230000u, 230000u},
        .valid_mask = AMI_COSEM_VALID_VOLTAGE_A
    };

    const uint8_t plain_get[] = {
        0xC0u, 0x01u, 0x81u,
        0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };

    uint8_t req[128];
    size_t req_len = 0;
    ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 50u,
                                             plain_get, sizeof(plain_get),
                                             req, sizeof(req), &req_len);

    /* Tamper with the last byte of the GCM tag */
    req[req_len - 1] ^= 0x55u;

    uint8_t workspace[256];
    uint8_t resp[256];
    size_t resp_len = 0;

    /* Authentication must fail! */
    AmiDlmsApduStatus st = ami_dlms_process_ciphered_request(&assoc, &snapshot, &backend,
                                                            req, req_len,
                                                            workspace, sizeof(workspace),
                                                            resp, sizeof(resp), &resp_len);
    assert(st == AMI_DLMS_APDU_AUTHENTICATION_FAILURE);

    /* Workspace must be safely zeroized */
    for (size_t i = 0; i < sizeof(workspace); i++) {
        assert(workspace[i] == 0u);
    }

    printf("test_cipher_suite0_tag_tampering: PASS\n");
}

static void test_cipher_suite0_port_integration(void)
{
    AmiDlmsPortContext port;
    ami_dlms_port_init(&port, mock_tx_func, NULL, NULL);

    AmiDlmsSecuritySuite0Context cipher_ctx;
    ami_dlms_cipher_suite0_init(&cipher_ctx, TEST_GUEK, TEST_AK,
                                CLIENT_TITLE, SERVER_TITLE, 200u);
    AmiDlmsCipherBackend backend;
    ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend);
    ami_dlms_port_set_cipher_backend(&port, &backend);

    /* Set association to HLS Associated */
    port.association.state = AMI_DLMS_ASSOC_ASSOCIATED;
    port.association.mechanism = AMI_DLMS_AUTH_HLS;
    port.snapshot.phase_voltage_millivolts[0] = 230000u;
    port.snapshot.valid_mask = AMI_COSEM_VALID_VOLTAGE_A;

    /* Build glo-get-request for Voltage Phase A */
    const uint8_t plain_get[] = {
        0xC0u, 0x01u, 0x81u,
        0x00u, 0x03u,
        0x01u, 0x00u, 0x20u, 0x07u, 0x00u, 0xFFu,
        0x02u, 0x00u
    };
    uint8_t glo_apdu[128];
    size_t glo_apdu_len = 0;
    ami_dlms_cipher_suite0_build_glo_request(&cipher_ctx, 0xC8u, 10u,
                                             plain_get, sizeof(plain_get),
                                             glo_apdu, sizeof(glo_apdu), &glo_apdu_len);

    /* Wrap inside HDLC I-frame (from client 16 to server 1, control=0x10) */
    uint8_t wire_frame[256];
    size_t wire_len = 0;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10,
                                      glo_apdu, glo_apdu_len,
                                      wire_frame, sizeof(wire_frame), &wire_len) == AMI_HDLC_FRAME_READY);

    /* Process wire buffer through port */
    s_last_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&port, wire_frame, wire_len) == AMI_DLMS_PORT_OK);

    /* Verify port transmitted an HDLC frame */
    assert(s_last_tx_len > 0);

    /* Parse response frame */
    HdlcContext parser;
    ami_dlms_hdlc_init(&parser);
    HdlcFrame resp_frame;
    AmiHdlcStatus hdlc_st = AMI_HDLC_INCOMPLETE;
    for (size_t i = 0; i < s_last_tx_len; i++) {
        hdlc_st = ami_dlms_hdlc_parse_byte(&parser, s_last_tx[i], &resp_frame);
    }
    assert(hdlc_st == AMI_HDLC_FRAME_READY);
    assert(resp_frame.type == AMI_HDLC_FRAME_I);

    /* Payload must be glo-get-response (0xCC) */
    assert(resp_frame.payload_length > 0);
    assert(resp_frame.payload[0] == 0xCCu);
    assert(resp_frame.payload[2] == 0x30u); /* SC */

    printf("test_cipher_suite0_port_integration: PASS\n");
}

int main(void)
{
    printf("Starting DLMS Security Suite 0 (Glo-APDU & Invocation Counter) Tests...\n");
    test_cipher_suite0_glo_get_roundtrip();
    test_cipher_suite0_anti_replay_invocation_counter();
    test_cipher_suite0_tag_tampering();
    test_cipher_suite0_port_integration();
    printf("All DLMS Security Suite 0 Tests PASSED!\n");
    return 0;
}
