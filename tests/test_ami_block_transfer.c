#include "ami_dlms_block_transfer.h"
#include "ami_dlms_port.h"
#include "ami_dlms_association.h"
#include "ami_dlms_hdlc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_bt_init_and_reset(void)
{
    AmiBlockTransferContext ctx;
    ami_bt_init(&ctx, 64u);
    assert(ctx.state == AMI_BT_STATE_IDLE);
    assert(ctx.block_size == 64u);
    assert(ctx.total_length == 0u);

    /* Test clamp minimum */
    ami_bt_init(&ctx, 10u);
    assert(ctx.block_size == AMI_BT_DEFAULT_BLOCK_SIZE);

    /* Test clamp maximum */
    ami_bt_init(&ctx, 1000u);
    assert(ctx.block_size == AMI_BT_MAX_BLOCK_SIZE);

    /* Reset test */
    ctx.state = AMI_BT_STATE_TRANSFERRING;
    ctx.total_length = 100u;
    ami_bt_reset(&ctx);
    assert(ctx.state == AMI_BT_STATE_IDLE);
    assert(ctx.total_length == 0u);
    printf("test_bt_init_and_reset passed\n");
}

static void test_bt_start_transfer_and_chunking(void)
{
    AmiBlockTransferContext ctx;
    ami_bt_init(&ctx, 128u);

    /* Generate 350 bytes of dummy test data */
    uint8_t dummy_data[350];
    for (size_t i = 0; i < sizeof(dummy_data); ++i) {
        dummy_data[i] = (uint8_t)(i & 0xFFu);
    }

    AmiBtStatus st = ami_bt_start_get_transfer(&ctx, 0xC1u, dummy_data, sizeof(dummy_data));
    assert(st == AMI_BT_OK);
    assert(ctx.state == AMI_BT_STATE_TRANSFERRING);
    assert(ctx.total_length == 350u);

    uint8_t assembled_data[512];
    size_t assembled_len = 0u;

    uint8_t resp[256];
    size_t resp_len = 0u;
    int is_last = 0;

    /* Block 1 (bytes 0..127) */
    st = ami_bt_build_get_response_block(&ctx, 1u, resp, sizeof(resp), &resp_len, &is_last);
    assert(st == AMI_BT_OK);
    assert(is_last == 0);
    assert(resp[0] == 0xC4u); /* Get-Response */
    assert(resp[1] == 0x02u); /* With-Datablock */
    assert(resp[2] == 0xC1u); /* invoke-id */
    assert(resp[3] == 0x00u); /* last-block = false */
    assert(resp[4] == 0x00u && resp[5] == 0x00u && resp[6] == 0x00u && resp[7] == 0x01u); /* block 1 */
    assert(resp[8] == 0x00u); /* raw-data choice */
    assert(resp[9] == 0x81u && resp[10] == 128u); /* A-XDR length: 128 bytes */
    assert(resp_len == 11u + 128u);
    memcpy(assembled_data + assembled_len, resp + 11u, 128u);
    assembled_len += 128u;

    /* Block 2 (bytes 128..255) */
    st = ami_bt_build_get_response_block(&ctx, 2u, resp, sizeof(resp), &resp_len, &is_last);
    assert(st == AMI_BT_OK);
    assert(is_last == 0);
    assert(resp[3] == 0x00u);
    assert(resp[7] == 0x02u); /* block 2 */
    assert(resp_len == 11u + 128u);
    memcpy(assembled_data + assembled_len, resp + 11u, 128u);
    assembled_len += 128u;

    /* Block 3 (bytes 256..349 = 94 bytes) */
    st = ami_bt_build_get_response_block(&ctx, 3u, resp, sizeof(resp), &resp_len, &is_last);
    assert(st == AMI_BT_OK);
    assert(is_last == 1);
    assert(resp[3] == 0x01u); /* last-block = true */
    assert(resp[7] == 0x03u); /* block 3 */
    assert(resp[9] == 94u);   /* A-XDR length < 128 */
    assert(resp_len == 10u + 94u);
    memcpy(assembled_data + assembled_len, resp + 10u, 94u);
    assembled_len += 94u;

    /* Verify assembled payload matches dummy_data exactly */
    assert(assembled_len == sizeof(dummy_data));
    assert(memcmp(assembled_data, dummy_data, sizeof(dummy_data)) == 0);
    assert(ctx.state == AMI_BT_STATE_COMPLETED);

    printf("test_bt_start_transfer_and_chunking passed\n");
}

static void test_bt_retransmission_and_errors(void)
{
    AmiBlockTransferContext ctx;
    ami_bt_init(&ctx, 100u);

    uint8_t dummy_data[250];
    memset(dummy_data, 0xAA, sizeof(dummy_data));

    ami_bt_start_get_transfer(&ctx, 0x81u, dummy_data, sizeof(dummy_data));

    uint8_t resp1[256];
    size_t resp1_len = 0u;
    int is_last = 0;

    /* Request block 1 */
    assert(ami_bt_build_get_response_block(&ctx, 1u, resp1, sizeof(resp1), &resp1_len, &is_last) == AMI_BT_OK);
    assert(is_last == 0);

    /* Retransmit block 1 */
    uint8_t resp1_retry[256];
    size_t resp1_retry_len = 0u;
    assert(ami_bt_build_get_response_block(&ctx, 1u, resp1_retry, sizeof(resp1_retry), &resp1_retry_len, &is_last) == AMI_BT_OK);
    assert(resp1_len == resp1_retry_len);
    assert(memcmp(resp1, resp1_retry, resp1_len) == 0);

    /* Out of sequence request (block 4 directly) */
    uint8_t resp_err[256];
    size_t resp_err_len = 0u;
    assert(ami_bt_build_get_response_block(&ctx, 4u, resp_err, sizeof(resp_err), &resp_err_len, &is_last) == AMI_BT_BLOCK_OUT_OF_SEQUENCE);

    /* Block 0 is invalid */
    assert(ami_bt_build_get_response_block(&ctx, 0u, resp_err, sizeof(resp_err), &resp_err_len, &is_last) == AMI_BT_BLOCK_OUT_OF_SEQUENCE);

    /* Build error response */
    size_t err_len = ami_bt_build_error_response_block(0x81u, 4u, 2u, resp_err, sizeof(resp_err));
    assert(err_len == 10u);
    assert(resp_err[0] == 0xC4u && resp_err[1] == 0x02u && resp_err[2] == 0x81u);
    assert(resp_err[3] == 0x01u); /* last block */
    assert(resp_err[8] == 0x01u); /* data-access-result choice */
    assert(resp_err[9] == 0x02u); /* temporary-failure */

    printf("test_bt_retransmission_and_errors passed\n");
}

static void test_bt_parse_get_next_block_request(void)
{
    /* C0 02 C1 00 00 00 02 */
    const uint8_t req[] = { 0xC0u, 0x02u, 0xC1u, 0x00u, 0x00u, 0x00u, 0x05u };
    uint8_t invoke_id = 0u;
    uint32_t block_num = 0u;

    assert(ami_bt_parse_get_next_block_request(req, sizeof(req), &invoke_id, &block_num) == 1);
    assert(invoke_id == 0xC1u);
    assert(block_num == 5u);

    /* Malformed tag */
    const uint8_t bad_req[] = { 0xC0u, 0x01u, 0xC1u, 0x00u, 0x00u, 0x00u, 0x05u };
    assert(ami_bt_parse_get_next_block_request(bad_req, sizeof(bad_req), &invoke_id, &block_num) == 0);

    /* Too short */
    assert(ami_bt_parse_get_next_block_request(req, 6u, &invoke_id, &block_num) == 0);

    printf("test_bt_parse_get_next_block_request passed\n");
}

static uint8_t s_captured_tx[1024];
static size_t s_captured_len = 0u;

static int port_test_tx(const uint8_t *data, size_t length, void *user_data)
{
    (void)user_data;
    if (length <= sizeof(s_captured_tx)) {
        memcpy(s_captured_tx, data, length);
        s_captured_len = length;
        return 1;
    }
    return 0;
}

static int port_test_snapshot(AmiCosemRegisterSnapshot *snapshot, void *user_data)
{
    (void)user_data;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->phase_voltage_millivolts[0] = 230000u;
    snapshot->active_import_wh = 1234567u;
    snapshot->valid_mask = AMI_COSEM_VALID_VOLTAGE_A | AMI_COSEM_VALID_ACTIVE_IMPORT | AMI_COSEM_VALID_EVENT_LOG;
    snapshot->event_log_count = 5u;
    for (size_t i = 0; i < 5u; ++i) {
        snapshot->event_log[i].event_code = (uint16_t)(100u + i);
    }
    return 1;
}

static void test_bt_port_integration(void)
{
    AmiDlmsPortContext port;
    ami_dlms_port_init(&port, port_test_tx, port_test_snapshot, NULL);

    /* Configure block transfer: chunk size 64 bytes, threshold 60 bytes */
    ami_dlms_port_configure_block_transfer(&port, 64u, 60u);

    /* Establish HDLC + Association via SNRM + AARQ */
    uint8_t snrm_wire[32];
    size_t snrm_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, AMI_HDLC_CONTROL_SNRM, NULL, 0,
                                     snrm_wire, sizeof(snrm_wire), &snrm_len) == AMI_HDLC_FRAME_READY);
    s_captured_len = 0u;
    AmiDlmsPortStatus ps = ami_dlms_port_process_buffer(&port, snrm_wire, snrm_len);
    assert(ps == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);

    /* AARQ No-Auth with LLC header */
    static const uint8_t aarq_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0x60, 0x1D, 0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00,
        0x00, 0x00, 0x14, 0x01, 0xF4
    };
    uint8_t aarq_wire[64];
    size_t aarq_wire_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, aarq_apdu, sizeof(aarq_apdu),
                                     aarq_wire, sizeof(aarq_wire), &aarq_wire_len) == AMI_HDLC_FRAME_READY);

    s_captured_len = 0u;
    ps = ami_dlms_port_process_buffer(&port, aarq_wire, aarq_wire_len);
    assert(ps == AMI_DLMS_PORT_OK);
    assert(port.association.state == AMI_DLMS_ASSOC_ASSOCIATED);

    /* Read Association Object List (0.0.40.0.0.255 attr 2): response > 200 bytes, triggers block transfer */
    static const uint8_t get_obj_list_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0xC0, 0x01, 0xC1, 0x00, 0x0F, 0x00, 0x00, 0x28, 0x00, 0x00, 0xFF, 0x02, 0x00
    };
    uint8_t get_wire[64];
    size_t get_wire_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x32, get_obj_list_apdu, sizeof(get_obj_list_apdu),
                                     get_wire, sizeof(get_wire), &get_wire_len) == AMI_HDLC_FRAME_READY);

    s_captured_len = 0u;
    ps = ami_dlms_port_process_buffer(&port, get_wire, get_wire_len);
    assert(ps == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);

    /* Decode wire reply to inspect APDU */
    HdlcContext parser;
    ami_dlms_hdlc_init(&parser);
    HdlcFrame reply_frame;
    AmiHdlcStatus hdlc_st = AMI_HDLC_INCOMPLETE;
    for (size_t i = 0; i < s_captured_len; ++i) {
        hdlc_st = ami_dlms_hdlc_parse_byte(&parser, s_captured_tx[i], &reply_frame);
        if (hdlc_st == AMI_HDLC_FRAME_READY) break;
    }
    assert(hdlc_st == AMI_HDLC_FRAME_READY);

    /* The reply MUST be a GET-Response-With-Datablock (0xC4 0x02) */
    assert(reply_frame.payload_length > 10u);
    assert(reply_frame.payload[0] == 0xC4u);
    assert(reply_frame.payload[1] == 0x02u); /* With-Datablock */
    assert(reply_frame.payload[2] == 0xC1u); /* invoke-id */
    assert(reply_frame.payload[3] == 0x00u); /* last-block = false */
    assert(reply_frame.payload[7] == 0x01u); /* block-number = 1 */

    /* Now simulate Client requesting Block 2 via GET-Request-For-Next-Datablock */
    static const uint8_t next_block_apdu[] = {
        0xE6, 0xE6, 0x00, /* LLC */
        0xC0, 0x02, 0xC1, 0x00, 0x00, 0x00, 0x02
    };
    uint8_t next_wire[64];
    size_t next_wire_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x54, next_block_apdu, sizeof(next_block_apdu),
                                     next_wire, sizeof(next_wire), &next_wire_len) == AMI_HDLC_FRAME_READY);

    s_captured_len = 0u;
    ps = ami_dlms_port_process_buffer(&port, next_wire, next_wire_len);
    assert(ps == AMI_DLMS_PORT_OK);
    assert(s_captured_len > 0u);

    ami_dlms_hdlc_init(&parser);
    for (size_t i = 0; i < s_captured_len; ++i) {
        hdlc_st = ami_dlms_hdlc_parse_byte(&parser, s_captured_tx[i], &reply_frame);
        if (hdlc_st == AMI_HDLC_FRAME_READY) break;
    }
    assert(hdlc_st == AMI_HDLC_FRAME_READY);
    assert(reply_frame.payload[0] == 0xC4u);
    assert(reply_frame.payload[1] == 0x02u);
    assert(reply_frame.payload[7] == 0x02u); /* block-number = 2 */

    printf("test_bt_port_integration passed\n");
}

int main(void)
{
    test_bt_init_and_reset();
    test_bt_start_transfer_and_chunking();
    test_bt_retransmission_and_errors();
    test_bt_parse_get_next_block_request();
    test_bt_port_integration();
    printf("All Block Transfer tests passed successfully!\n");
    return 0;
}
