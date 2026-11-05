/**
 * @file test_ami_disconnect_control.c
 * @brief Unit tests for DLMS/COSEM Class 70 Disconnect Control (Breaker) State Machine.
 */

#include "ami_disconnect_control.h"
#include "ami_dlms_port.h"
#include "ami_dlms_apdu.h"
#include "ami_dlms_hdlc.h"
#include "ami_dlms_cipher.h"
#include "ami_security.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Mock Breaker Actuator */
typedef struct {
    int fail_next_command;
    int open_call_count;
    int close_call_count;
    AmiBreakerCommand last_command;
} MockBreakerContext;

static int mock_breaker_actuator(void *user, AmiBreakerCommand cmd)
{
    MockBreakerContext *mock = (MockBreakerContext *)user;
    if (mock == NULL) return -1;
    if (mock->fail_next_command) {
        mock->fail_next_command = 0;
        return -1;
    }
    mock->last_command = cmd;
    if (cmd == AMI_BREAKER_CMD_OPEN) {
        mock->open_call_count++;
    } else {
        mock->close_call_count++;
    }
    return 0;
}

static void test_dc_init_defaults(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext ctx;
    ami_disconnect_control_init(&ctx, AMI_DISCONNECT_MODE_2, mock_breaker_actuator, &mock);

    assert(ctx.output_state == 1u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_CONNECTED);
    assert(ctx.control_mode == AMI_DISCONNECT_MODE_2);
    assert(ctx.transition_count == 0u);
    assert(ctx.open_command_count == 0u);
    assert(ctx.close_command_count == 0u);

    AmiCosemRegisterSnapshot snap = {0};
    ami_disconnect_control_sync_snapshot(&ctx, &snap);
    assert(snap.disconnect_output_state == 1u);
    assert(snap.disconnect_control_state == 1u);
    assert(snap.disconnect_control_mode == 2u);
    assert((snap.valid_mask & AMI_COSEM_VALID_DISCONNECT_CTRL) != 0u);
}

static void test_dc_mode_2_remote_disconnect_reconnect(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext ctx;
    ami_disconnect_control_init(&ctx, AMI_DISCONNECT_MODE_2, mock_breaker_actuator, &mock);

    /* 1. Remote Disconnect */
    AmiDisconnectStatus st = ami_disconnect_control_remote_disconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 0u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_DISCONNECTED);
    assert(ctx.open_command_count == 1u);
    assert(mock.open_call_count == 1);
    assert(mock.last_command == AMI_BREAKER_CMD_OPEN);

    /* Idempotent disconnect call */
    st = ami_disconnect_control_remote_disconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 0u);
    assert(mock.open_call_count == 1); /* Not pulsed again */

    /* 2. Remote Reconnect (Direct in Mode 2) */
    st = ami_disconnect_control_remote_reconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 1u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_CONNECTED);
    assert(ctx.close_command_count == 1u);
    assert(mock.close_call_count == 1);
    assert(mock.last_command == AMI_BREAKER_CMD_CLOSE);

    /* Idempotent reconnect call */
    st = ami_disconnect_control_remote_reconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 1u);
    assert(mock.close_call_count == 1);
}

static void test_dc_mode_1_manual_reconnect(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext ctx;
    ami_disconnect_control_init(&ctx, AMI_DISCONNECT_MODE_1, mock_breaker_actuator, &mock);

    /* Disconnect */
    AmiDisconnectStatus st = ami_disconnect_control_remote_disconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 0u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_DISCONNECTED);

    /* Manual reconnect before remote enable must be rejected in Mode 1 */
    st = ami_disconnect_control_manual_reconnect(&ctx);
    assert(st == AMI_DISCONNECT_ERR_NOT_PERMITTED);
    assert(ctx.output_state == 0u);

    /* Remote reconnect in Mode 1 transitions to Ready_for_Reconnection (output remains 0) */
    st = ami_disconnect_control_remote_reconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 0u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_READY_FOR_RECONNECTION);
    assert(mock.close_call_count == 0); /* Breaker coil not energized yet */

    /* Now consumer presses manual button on meter -> transitions to CONNECTED */
    st = ami_disconnect_control_manual_reconnect(&ctx);
    assert(st == AMI_DISCONNECT_OK);
    assert(ctx.output_state == 1u);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_CONNECTED);
    assert(mock.close_call_count == 1);
}

static void test_dc_mode_3_remote_prohibition(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext ctx;
    ami_disconnect_control_init(&ctx, AMI_DISCONNECT_MODE_3, mock_breaker_actuator, &mock);

    /* Mode 3 prohibits remote disconnect */
    AmiDisconnectStatus st = ami_disconnect_control_remote_disconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_ERR_NOT_PERMITTED);
    assert(ctx.control_state == AMI_DISCONNECT_STATE_CONNECTED);

    /* Prohibits remote reconnect */
    st = ami_disconnect_control_remote_reconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_ERR_NOT_PERMITTED);
}

static void test_dc_actuator_hardware_failure(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext ctx;
    ami_disconnect_control_init(&ctx, AMI_DISCONNECT_MODE_2, mock_breaker_actuator, &mock);

    /* Inject hardware fault */
    mock.fail_next_command = 1;
    AmiDisconnectStatus st = ami_disconnect_control_remote_disconnect(&ctx, 0);
    assert(st == AMI_DISCONNECT_ERR_ACTUATOR_FAILED);
    /* Fail-closed: state remains Connected if physical pulse failed */
    assert(ctx.control_state == AMI_DISCONNECT_STATE_CONNECTED);
    assert(ctx.output_state == 1u);
}

/* DLMS Port Mock Setup */
static uint8_t s_captured_frame[AMI_HDLC_MAX_FRAME_BYTES];
static size_t s_captured_len = 0u;

static int mock_port_tx(const uint8_t *data, size_t length, void *user)
{
    (void)user;
    if (length <= sizeof(s_captured_frame)) {
        memcpy(s_captured_frame, data, length);
        s_captured_len = length;
        return 1;
    }
    return 0;
}

static void test_dc_dlms_port_wire_action_request(void)
{
    MockBreakerContext mock = {0};
    AmiDisconnectControlContext dc;
    ami_disconnect_control_init(&dc, AMI_DISCONNECT_MODE_2, mock_breaker_actuator, &mock);

    AmiDlmsPortContext port;
    ami_dlms_port_init(&port, mock_port_tx, NULL, NULL);
    ami_dlms_port_set_disconnect_control(&port, &dc);

    /* 1. Send SNRM */
    uint8_t snrm_wire[32];
    size_t snrm_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1u, 16u, AMI_HDLC_CONTROL_SNRM, NULL, 0u,
                                      snrm_wire, sizeof(snrm_wire), &snrm_len) == AMI_HDLC_FRAME_READY);
    s_captured_len = 0u;
    assert(ami_dlms_port_process_buffer(&port, snrm_wire, snrm_len) == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);

    /* 2. Send AARQ (no-auth development) with LLC */
    const uint8_t aarq_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0x60, 0x1D, 0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00,
        0x00, 0x00, 0x14, 0x01, 0xF4
    };
    uint8_t aarq_frame[64];
    size_t aarq_frame_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1u, 16u, 0x10u, aarq_apdu, sizeof(aarq_apdu),
                                      aarq_frame, sizeof(aarq_frame), &aarq_frame_len) == AMI_HDLC_FRAME_READY);
    s_captured_len = 0u;
    assert(ami_dlms_port_process_buffer(&port, aarq_frame, aarq_frame_len) == AMI_DLMS_PORT_OK);
    assert(port.association.state == AMI_DLMS_ASSOC_ASSOCIATED);

    /* 3. Send Action-Request: Class 70 (0x0046), OBIS 0.0.96.3.10.255, Method 1 (remote_disconnect) */
    const uint8_t action_disconnect_apdu[] = {
        0xE6, 0xE6, 0x00,                   /* LLC */
        0xC3, 0x01, 0x81,                   /* Action-Request-Normal, invoke-id 0x81 */
        0x00, 0x46,                         /* Class 70 (Disconnect Control) */
        0x00, 0x00, 0x60, 0x03, 0x0A, 0xFF, /* OBIS 0.0.96.3.10.255 */
        0x01,                               /* Method 1: remote_disconnect */
        0x01, 0x0F, 0x00                    /* Data-present: Integer(0) */
    };
    uint8_t action_frame[64];
    size_t action_frame_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1u, 16u, 0x10u, action_disconnect_apdu,
                                      sizeof(action_disconnect_apdu),
                                      action_frame, sizeof(action_frame), &action_frame_len) == AMI_HDLC_FRAME_READY);
    s_captured_len = 0u;
    assert(ami_dlms_port_process_buffer(&port, action_frame, action_frame_len) == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);
    assert(dc.control_state == AMI_DISCONNECT_STATE_DISCONNECTED);
    assert(dc.output_state == 0u);
    assert(port.snapshot.disconnect_output_state == 0u);
    assert(mock.last_command == AMI_BREAKER_CMD_OPEN);

    /* Decode captured response frame to verify Action-Response success */
    HdlcFrame resp_frame;
    HdlcContext hdlc_parser;
    ami_dlms_hdlc_init(&hdlc_parser);
    int frame_ok = 0;
    for (size_t i = 0; i < s_captured_len; ++i) {
        if (ami_dlms_hdlc_parse_byte(&hdlc_parser, s_captured_frame[i], &resp_frame) == AMI_HDLC_FRAME_READY) {
            frame_ok = 1;
            break;
        }
    }
    assert(frame_ok);
    assert(resp_frame.payload_length >= 5u);
    assert(resp_frame.payload[0] == 0xC7u); /* Action-Response */
    assert(resp_frame.payload[1] == 0x01u); /* Action-Response-Normal */
    assert(resp_frame.payload[2] == 0x81u); /* invoke-id matching */
    assert(resp_frame.payload[3] == 0x00u); /* success (0) */

    /* 4. Send Action-Request: Method 2 (remote_reconnect) */
    const uint8_t action_reconnect_apdu[] = {
        0xE6, 0xE6, 0x00,                   /* LLC */
        0xC3, 0x01, 0x82,                   /* Action-Request-Normal, invoke-id 0x82 */
        0x00, 0x46,                         /* Class 70 (Disconnect Control) */
        0x00, 0x00, 0x60, 0x03, 0x0A, 0xFF, /* OBIS 0.0.96.3.10.255 */
        0x02,                               /* Method 2: remote_reconnect */
        0x01, 0x0F, 0x00                    /* Data-present: Integer(0) */
    };
    assert(ami_dlms_hdlc_encode_frame(1u, 16u, 0x10u, action_reconnect_apdu,
                                      sizeof(action_reconnect_apdu),
                                      action_frame, sizeof(action_frame), &action_frame_len) == AMI_HDLC_FRAME_READY);
    s_captured_len = 0u;
    assert(ami_dlms_port_process_buffer(&port, action_frame, action_frame_len) == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);
    assert(dc.control_state == AMI_DISCONNECT_STATE_CONNECTED);
    assert(dc.output_state == 1u);
    assert(port.snapshot.disconnect_output_state == 1u);
    assert(mock.last_command == AMI_BREAKER_CMD_CLOSE);
}

static void test_dc_ciphered_action_request(void)
{
    const uint8_t GUEK[16] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    const uint8_t AK[16] = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    const uint8_t CLIENT_TITLE[8] = {'P', 'L', 'N', 'C', 'L', 'I', '0', '1'};
    const uint8_t SERVER_TITLE[8] = {'P', 'L', 'N', 'M', 'T', 'R', '0', '1'};

    AmiDlmsSecuritySuite0Context cipher_ctx;
    assert(ami_dlms_cipher_suite0_init(&cipher_ctx, GUEK, AK, CLIENT_TITLE, SERVER_TITLE, 1u) == 0);

    AmiDlmsCipherBackend backend;
    assert(ami_dlms_cipher_suite0_get_backend(&cipher_ctx, &backend) == 0);

    MockBreakerContext mock = {0};
    AmiDisconnectControlContext dc;
    ami_disconnect_control_init(&dc, AMI_DISCONNECT_MODE_2, mock_breaker_actuator, &mock);

    backend.disconnect_control = &dc;

    AmiDlmsAssociation assoc = {
        .state = AMI_DLMS_ASSOC_ASSOCIATED,
        .mechanism = AMI_DLMS_AUTH_HLS
    };
    AmiCosemRegisterSnapshot snap = {0};

    /* Build plaintext Action-Request: Class 70 Method 1 (remote_disconnect) */
    const uint8_t plain_action[] = {
        0xC3, 0x01, 0x90,                   /* Action-Request-Normal, invoke-id 0x90 */
        0x00, 0x46,                         /* Class 70 (Disconnect Control) */
        0x00, 0x00, 0x60, 0x03, 0x0A, 0xFF, /* OBIS 0.0.96.3.10.255 */
        0x01,                               /* Method 1: remote_disconnect */
        0x01, 0x0F, 0x00                    /* Data-present: Integer(0) */
    };

    /* Build glo-action-request */
    uint8_t glo_req[128];
    size_t glo_req_len = 0;
    assert(ami_dlms_cipher_suite0_build_glo_request(
        &cipher_ctx, 0xCBu, 100u, plain_action, sizeof(plain_action),
        glo_req, sizeof(glo_req), &glo_req_len) == 0);

    uint8_t workspace[256];
    uint8_t glo_resp[256];
    size_t glo_resp_len = 0;

    assert(ami_dlms_process_ciphered_request(
        &assoc, &snap, &backend,
        glo_req, glo_req_len,
        workspace, sizeof(workspace),
        glo_resp, sizeof(glo_resp), &glo_resp_len) == AMI_DLMS_APDU_OK);

    assert(glo_resp_len > 0);
    assert(glo_resp[0] == 0xCEu); /* GLO-ACTION-RESPONSE tag */
    assert(dc.control_state == AMI_DISCONNECT_STATE_DISCONNECTED);
    assert(dc.output_state == 0u);
    assert(mock.last_command == AMI_BREAKER_CMD_OPEN);
}

int main(void)
{
    test_dc_init_defaults();
    test_dc_mode_2_remote_disconnect_reconnect();
    test_dc_mode_1_manual_reconnect();
    test_dc_mode_3_remote_prohibition();
    test_dc_actuator_hardware_failure();
    test_dc_dlms_port_wire_action_request();
    test_dc_ciphered_action_request();

    printf("All Disconnect Control (Class 70) unit tests passed successfully.\n");
    return 0;
}
