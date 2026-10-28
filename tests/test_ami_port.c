#include "ami_dlms_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MOCK_TX_BUFFER_SIZE 2048

static uint8_t s_mock_tx_captured[MOCK_TX_BUFFER_SIZE];
static size_t s_mock_tx_len = 0;
static int s_mock_tx_call_count = 0;

static int mock_uart_tx(const uint8_t *data, size_t length, void *user_data)
{
    (void)user_data;
    if (length > sizeof(s_mock_tx_captured)) {
        return 0;
    }
    memcpy(s_mock_tx_captured, data, length);
    s_mock_tx_len = length;
    s_mock_tx_call_count++;
    return 1;
}

static int mock_snapshot_provider(AmiCosemRegisterSnapshot *snapshot, void *user_data)
{
    (void)user_data;
    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->valid_mask = AMI_COSEM_VALID_VOLTAGE_A | AMI_COSEM_VALID_CURRENT_A |
                           AMI_COSEM_VALID_ACTIVE_IMPORT | AMI_COSEM_VALID_CLOCK;
    snapshot->phase_voltage_millivolts[0] = 230000u;
    snapshot->phase_current_milliamps[0] = 10000u;
    snapshot->active_import_wh = 54321u;
    snapshot->clock_date_time.year[0] = 0x07u;
    snapshot->clock_date_time.year[1] = 0xEAu; /* 2026 */
    snapshot->clock_date_time.month = 10u;
    snapshot->clock_date_time.day = 28u;
    snapshot->clock_date_time.hour = 14u;
    return 1;
}

static void test_port_snrm_and_disc_lifecycle(void)
{
    AmiDlmsPortContext ctx;
    HdlcContext client_hdlc;
    HdlcFrame reply;
    uint8_t snrm_wire[32];
    size_t snrm_len = 0;
    uint8_t disc_wire[32];
    size_t disc_len = 0;

    assert(ami_dlms_hdlc_encode_frame(1, 16, AMI_HDLC_CONTROL_SNRM, NULL, 0, snrm_wire, sizeof(snrm_wire), &snrm_len) == AMI_HDLC_FRAME_READY);
    assert(ami_dlms_hdlc_encode_frame(1, 16, AMI_HDLC_CONTROL_DISC, NULL, 0, disc_wire, sizeof(disc_wire), &disc_len) == AMI_HDLC_FRAME_READY);

    ami_dlms_port_init(&ctx, mock_uart_tx, mock_snapshot_provider, NULL);
    ami_dlms_hdlc_init(&client_hdlc);

    /* 1. Feed SNRM */
    s_mock_tx_len = 0;
    s_mock_tx_call_count = 0;
    AmiDlmsPortStatus s = ami_dlms_port_process_buffer(&ctx, snrm_wire, snrm_len);
    assert(s == AMI_DLMS_PORT_OK);
    assert(s_mock_tx_call_count == 1);
    assert(s_mock_tx_len > 0);

    /* Verify response is valid UA */
    for (size_t i = 0; i < s_mock_tx_len; ++i) {
        if (ami_dlms_hdlc_parse_byte(&client_hdlc, s_mock_tx_captured[i], &reply) == AMI_HDLC_FRAME_READY) {
            break;
        }
    }
    assert(reply.type == AMI_HDLC_FRAME_UA);

    /* 2. Feed DISC */
    s_mock_tx_len = 0;
    s_mock_tx_call_count = 0;
    ami_dlms_hdlc_init(&client_hdlc);
    s = ami_dlms_port_process_buffer(&ctx, disc_wire, disc_len);
    assert(s == AMI_DLMS_PORT_OK);
    assert(s_mock_tx_call_count == 1);
    assert(s_mock_tx_len > 0);

    for (size_t i = 0; i < s_mock_tx_len; ++i) {
        if (ami_dlms_hdlc_parse_byte(&client_hdlc, s_mock_tx_captured[i], &reply) == AMI_HDLC_FRAME_READY) {
            break;
        }
    }
    assert(reply.type == AMI_HDLC_FRAME_UA);
}

static void test_port_aarq_and_get_request(void)
{
    AmiDlmsPortContext ctx;
    HdlcContext client_hdlc;
    HdlcFrame reply;
    uint8_t wire_aarq[128];
    size_t wire_aarq_len = 0;
    uint8_t wire_get[128];
    size_t wire_get_len = 0;

    ami_dlms_port_init(&ctx, mock_uart_tx, mock_snapshot_provider, NULL);
    ami_dlms_hdlc_init(&client_hdlc);

    /* Raw AARQ payload with LLC (No authentication / Public Client) */
    const uint8_t aarq_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0x60, 0x1D,       /* AARQ length 29 */
        0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x00, 0x14, 0x01, 0xF4
    };

    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, aarq_apdu, sizeof(aarq_apdu),
                                     wire_aarq, sizeof(wire_aarq), &wire_aarq_len) == AMI_HDLC_FRAME_READY);

    s_mock_tx_len = 0;
    s_mock_tx_call_count = 0;
    AmiDlmsPortStatus s = ami_dlms_port_process_buffer(&ctx, wire_aarq, wire_aarq_len);
    assert(s == AMI_DLMS_PORT_OK);
    assert(s_mock_tx_call_count == 1);
    assert(ctx.association.state == AMI_DLMS_ASSOC_ASSOCIATED);

    /* Verify response has AARE */
    for (size_t i = 0; i < s_mock_tx_len; ++i) {
        if (ami_dlms_hdlc_parse_byte(&client_hdlc, s_mock_tx_captured[i], &reply) == AMI_HDLC_FRAME_READY) {
            break;
        }
    }
    assert(reply.type == AMI_HDLC_FRAME_I);
    assert(reply.payload_length > 0 && reply.payload[0] == 0x61u); /* AARE tag */

    /* Now send GET-Request-Normal for Voltage R (1.0.32.7.0.255, class 3, attr 2) */
    const uint8_t get_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0xC0, 0x01, 0xC1, /* GET-Request-Normal, invoke ID 0xC1 */
        0x00, 0x03,       /* Class 3 */
        0x01, 0x00, 0x20, 0x07, 0x00, 0xFF, /* 1.0.32.7.0.255 */
        0x02,             /* Attribute 2 (value) */
        0x00              /* No selective access */
    };

    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x12, get_apdu, sizeof(get_apdu),
                                     wire_get, sizeof(wire_get), &wire_get_len) == AMI_HDLC_FRAME_READY);

    s_mock_tx_len = 0;
    s_mock_tx_call_count = 0;
    ami_dlms_hdlc_init(&client_hdlc);
    s = ami_dlms_port_process_buffer(&ctx, wire_get, wire_get_len);
    assert(s == AMI_DLMS_PORT_OK);
    assert(s_mock_tx_call_count == 1);

    for (size_t i = 0; i < s_mock_tx_len; ++i) {
        if (ami_dlms_hdlc_parse_byte(&client_hdlc, s_mock_tx_captured[i], &reply) == AMI_HDLC_FRAME_READY) {
            break;
        }
    }
    assert(reply.type == AMI_HDLC_FRAME_I);
    /* 0xC4 0x01 0xC1 0x00 0x06 (double-long-unsigned) ... */
    assert(reply.payload_length >= 5);
    assert(reply.payload[0] == 0xC4u); /* GET-Response */
    assert(reply.payload[1] == 0x01u); /* GET-Response-Normal */
    assert(reply.payload[2] == 0xC1u); /* invoke-id */
    assert(reply.payload[3] == 0x00u); /* success */
}


static int mock_verify_lls(void *user, const uint8_t *password, size_t length)
{
    (void)user;
    static const uint8_t expected[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    return length == sizeof(expected) && memcmp(password, expected, length) == 0;
}

static void test_port_active_session_and_security(void)
{
    AmiDlmsPortContext ctx;
    HdlcContext client_hdlc;
    HdlcFrame reply;
    uint8_t wire[128];
    size_t wire_len = 0;

    ami_dlms_port_init(&ctx, mock_uart_tx, mock_snapshot_provider, NULL);
    ami_dlms_hdlc_init(&client_hdlc);

    /* 1. Establish valid no-auth association */
    const uint8_t aarq_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0x60, 0x1D,       /* AARQ length 29 */
        0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x00, 0x14, 0x01, 0xF4
    };
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, aarq_apdu, sizeof(aarq_apdu), wire, sizeof(wire), &wire_len) == AMI_HDLC_FRAME_READY);
    assert(ami_dlms_port_process_buffer(&ctx, wire, wire_len) == AMI_DLMS_PORT_OK);
    assert(ctx.association.state == AMI_DLMS_ASSOC_ASSOCIATED);

    /* 2. Unsolicited AARQ while already associated: must NOT reset/teardown active association */
    s_mock_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&ctx, wire, wire_len) == AMI_DLMS_PORT_OK);
    /* Existing association MUST still be ASSOCIATED */
    assert(ctx.association.state == AMI_DLMS_ASSOC_ASSOCIATED);

    /* 3. Port configured with LLS security requiring password */
    AmiDlmsAssociationSecurity sec = {
        .allow_no_auth = 0u,
        .verify_lls = mock_verify_lls
    };
    ami_dlms_port_init(&ctx, mock_uart_tx, mock_snapshot_provider, NULL);
    ami_dlms_port_set_security(&ctx, &sec);

    /* Sending no-auth AARQ should be rejected when security policy requires authentication */
    s_mock_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&ctx, wire, wire_len) == AMI_DLMS_PORT_OK);
    assert(ctx.association.state == AMI_DLMS_ASSOC_ERROR);
}

int main(void)
{
    test_port_snrm_and_disc_lifecycle();
    test_port_aarq_and_get_request();
    test_port_active_session_and_security();
    printf("DLMS Port tests passed.\n");
    return 0;
}

