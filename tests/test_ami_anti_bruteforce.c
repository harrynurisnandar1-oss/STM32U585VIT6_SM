/**
 * @file test_ami_anti_bruteforce.c
 * @brief Unit tests for Anti-Brute-Force Progressive Exponential Backoff & Lockout per SPLN D5.002:2024.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ami_anti_bruteforce.h"
#include "ami_dlms_port.h"
#include "ami_dlms_association.h"
#include "ami_security.h"

static void test_abf_default_initialization(void)
{
    AmiAntiBruteforceContext ctx;
    assert(ami_anti_bruteforce_init(&ctx, NULL) == 0);

    assert(ctx.config.initial_delay_sec == 5u);
    assert(ctx.config.backoff_multiplier == 2u);
    assert(ctx.config.max_failed_attempts == 5u);
    assert(ctx.config.max_penalty_delay_sec == 60u);
    assert(ctx.failed_attempts == 0u);
    assert(ctx.lockout_until_sec == 0u);
    assert(ctx.total_failures_logged == 0u);
    assert(ctx.is_locked_permanent == 0);

    assert(ami_anti_bruteforce_is_allowed(&ctx, 100u) == 1);
    assert(ami_anti_bruteforce_get_state(&ctx, 100u) == AMI_ABF_STATE_READY);
    assert(ami_anti_bruteforce_get_remaining_lockout(&ctx, 100u) == 0u);

    printf("test_abf_default_initialization: PASS\n");
}

static void test_abf_progressive_exponential_backoff(void)
{
    AmiAntiBruteforceContext ctx;
    assert(ami_anti_bruteforce_init(&ctx, NULL) == 0);

    uint32_t t = 1000u;

    /* --- Attempt 1 Failed --- */
    uint32_t delay1 = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(delay1 == 5u);
    assert(ctx.failed_attempts == 1u);
    assert(ctx.lockout_until_sec == t + 5u);
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 0);
    assert(ami_anti_bruteforce_is_allowed(&ctx, t + 4u) == 0);
    assert(ami_anti_bruteforce_get_remaining_lockout(&ctx, t + 2u) == 3u);
    assert(ami_anti_bruteforce_get_state(&ctx, t + 2u) == AMI_ABF_STATE_PENALTY_DELAY);

    /* Penalty expires at t + 5 */
    t += 5u;
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 1);
    assert(ami_anti_bruteforce_get_state(&ctx, t) == AMI_ABF_STATE_READY);

    /* --- Attempt 2 Failed (5s * 2^1 = 10s delay) --- */
    uint32_t delay2 = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(delay2 == 10u);
    assert(ctx.failed_attempts == 2u);
    assert(ctx.lockout_until_sec == t + 10u);
    assert(ami_anti_bruteforce_is_allowed(&ctx, t + 9u) == 0);

    /* Penalty expires at t + 10 */
    t += 10u;
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 1);

    /* --- Attempt 3 Failed (5s * 2^2 = 20s delay) --- */
    uint32_t delay3 = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(delay3 == 20u);
    assert(ctx.failed_attempts == 3u);
    assert(ctx.lockout_until_sec == t + 20u);

    t += 20u;
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 1);

    /* --- Attempt 4 Failed (5s * 2^3 = 40s delay) --- */
    uint32_t delay4 = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(delay4 == 40u);
    assert(ctx.failed_attempts == 4u);
    assert(ctx.lockout_until_sec == t + 40u);

    t += 40u;
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 1);

    /* --- Attempt 5 Failed -> Hard Permanent Lockout (SPLN D5.002:2024) --- */
    uint32_t delay5 = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(delay5 == 0xFFFFFFFFu);
    assert(ctx.failed_attempts == 5u);
    assert(ctx.is_locked_permanent == 1);
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 0);
    assert(ami_anti_bruteforce_get_state(&ctx, t) == AMI_ABF_STATE_LOCKED_OUT);

    /* Even after 100,000 seconds, meter remains strictly locked out */
    assert(ami_anti_bruteforce_is_allowed(&ctx, t + 100000u) == 0);
    assert(ami_anti_bruteforce_get_remaining_lockout(&ctx, t + 100000u) == 0xFFFFFFFFu);

    printf("test_abf_progressive_exponential_backoff: PASS\n");
}

static void test_abf_administrative_unlock(void)
{
    AmiAntiBruteforceContext ctx;
    assert(ami_anti_bruteforce_init(&ctx, NULL) == 0);

    /* Trigger 5 failures to reach hard lockout */
    for (int i = 0; i < 5; i++) {
        (void)ami_anti_bruteforce_record_failure(&ctx, 100u);
    }
    assert(ctx.is_locked_permanent == 1);
    assert(ami_anti_bruteforce_is_allowed(&ctx, 500u) == 0);

    /* Perform administrative unlock */
    assert(ami_anti_bruteforce_unlock(&ctx) == 0);
    assert(ctx.is_locked_permanent == 0);
    assert(ctx.failed_attempts == 0u);
    assert(ctx.lockout_until_sec == 0u);
    assert(ami_anti_bruteforce_is_allowed(&ctx, 500u) == 1);
    assert(ami_anti_bruteforce_get_state(&ctx, 500u) == AMI_ABF_STATE_READY);

    printf("test_abf_administrative_unlock: PASS\n");
}

static void test_abf_success_resets_backoff(void)
{
    AmiAntiBruteforceContext ctx;
    assert(ami_anti_bruteforce_init(&ctx, NULL) == 0);

    uint32_t t = 100u;
    /* 3 consecutive failures */
    (void)ami_anti_bruteforce_record_failure(&ctx, t);
    t += 5u;
    (void)ami_anti_bruteforce_record_failure(&ctx, t);
    t += 10u;
    (void)ami_anti_bruteforce_record_failure(&ctx, t);
    t += 20u;

    assert(ctx.failed_attempts == 3u);
    assert(ami_anti_bruteforce_is_allowed(&ctx, t) == 1);

    /* Successful authentication resets consecutive failure count */
    ami_anti_bruteforce_record_success(&ctx);
    assert(ctx.failed_attempts == 0u);
    assert(ctx.lockout_until_sec == 0u);

    /* Next failure starts fresh at initial delay (5 seconds, NOT 40 seconds) */
    uint32_t next_delay = ami_anti_bruteforce_record_failure(&ctx, t);
    assert(next_delay == 5u);
    assert(ctx.failed_attempts == 1u);

    printf("test_abf_success_resets_backoff: PASS\n");
}

/* Mock Tx callback to capture HDLC frames sent by port */
static uint8_t s_last_tx[512];
static size_t s_last_tx_len = 0;

static int mock_port_tx(const uint8_t *data, size_t length, void *user_data)
{
    (void)user_data;
    if (length <= sizeof(s_last_tx)) {
        memcpy(s_last_tx, data, length);
        s_last_tx_len = length;
        return 1;
    }
    return 0;
}

static void test_abf_dlms_port_aarq_lockout_rejection(void)
{
    AmiDlmsPortContext port;
    AmiAntiBruteforceContext abf;

    ami_dlms_port_init(&port, mock_port_tx, NULL, NULL);
    assert(ami_anti_bruteforce_init(&abf, NULL) == 0);
    ami_dlms_port_set_anti_bruteforce(&port, &abf);
    ami_dlms_port_set_time(&port, 100u);

    /* Trigger 5 failures into hard lockout */
    for (int i = 0; i < 5; i++) {
        (void)ami_anti_bruteforce_record_failure(&abf, 100u);
    }
    assert(ami_anti_bruteforce_is_allowed(&abf, 100u) == 0);

    /* Prepare a standard AARQ packet with LLC header (E6 E6 00) */
    static const uint8_t aarq_raw[] = {
        0xE6, 0xE6, 0x00,                               /* LLC */
        0x60, 0x1D,                                     /* AARQ */
        0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x1E, 0x5D, 0xFF, 0xFF
    };

    uint8_t wire_buf[128];
    size_t wire_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, aarq_raw, sizeof(aarq_raw),
                                      wire_buf, sizeof(wire_buf), &wire_len) == AMI_HDLC_FRAME_READY);

    s_last_tx_len = 0u;
    assert(ami_dlms_port_process_buffer(&port, wire_buf, wire_len) == AMI_DLMS_PORT_OK);

    /* Verify an AARE rejection response was transmitted */
    assert(s_last_tx_len > 0);

    /* Parse HDLC response frame */
    HdlcContext rx_hdlc;
    ami_dlms_hdlc_init(&rx_hdlc);
    HdlcFrame resp_frame;
    size_t parsed = 0;
    for (size_t i = 0; i < s_last_tx_len; i++) {
        if (ami_dlms_hdlc_parse_byte(&rx_hdlc, s_last_tx[i], &resp_frame) == AMI_HDLC_FRAME_READY) {
            parsed = 1;
            break;
        }
    }
    assert(parsed == 1);
    assert(resp_frame.payload_length >= 20);

    /* Verify AARE tag 0x61 and rejected-permanent result */
    assert(resp_frame.payload[0] == 0x61u); /* AARE */
    /* Check Association-Result tag 0xA2, len 3, integer tag 0x02, len 1, value 0x01 (rejected-permanent) */
    int found_rejected_result = 0;
    for (size_t i = 0; i < resp_frame.payload_length - 4; i++) {
        if (resp_frame.payload[i] == 0xA2u &&
            resp_frame.payload[i + 1] == 0x03u &&
            resp_frame.payload[i + 2] == 0x02u &&
            resp_frame.payload[i + 3] == 0x01u &&
            resp_frame.payload[i + 4] == 0x01u) {
            found_rejected_result = 1;
            break;
        }
    }
    assert(found_rejected_result == 1);

    printf("test_abf_dlms_port_aarq_lockout_rejection: PASS\n");
}

static int mock_test_hls_proof(void *user, const uint8_t *oid, size_t oid_length,
                                const uint8_t *challenge, size_t challenge_length,
                                uint8_t *proof, size_t proof_capacity, size_t *proof_length)
{
    (void)user;
    (void)oid;
    (void)oid_length;
    (void)challenge;
    (void)challenge_length;
    if (proof == NULL || proof_length == NULL || proof_capacity < 16) return 0;
    /* Deterministic mock proof: 0x55 x 16 */
    memset(proof, 0x55, 16);
    *proof_length = 16u;
    return 1;
}

static void test_abf_dlms_port_action_hls_reply(void)
{
    AmiDlmsPortContext port;
    AmiAntiBruteforceContext abf;

    ami_dlms_port_init(&port, mock_port_tx, NULL, NULL);
    assert(ami_anti_bruteforce_init(&abf, NULL) == 0);
    ami_dlms_port_set_anti_bruteforce(&port, &abf);
    ami_dlms_port_set_time(&port, 1000u);

    /* Configure HLS security policy on port */
    static const uint8_t hls5_oid[7] = {0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x05};
    AmiDlmsAssociationSecurity sec;
    memset(&sec, 0, sizeof(sec));
    sec.hls_proof = mock_test_hls_proof;
    sec.hls_mechanism_oid = hls5_oid;
    sec.hls_mechanism_oid_length = sizeof(hls5_oid);
    assert(ami_dlms_port_set_security(&port, &sec) == AMI_DLMS_PORT_OK);

    /* Simulate association state in AUTH_PENDING */
    port.association.state = AMI_DLMS_ASSOC_AUTH_PENDING;
    port.association.mechanism = AMI_DLMS_AUTH_HLS;
    memcpy(port.association.hls_mechanism_oid, hls5_oid, sizeof(hls5_oid));
    port.association.hls_mechanism_oid_length = sizeof(hls5_oid);

    /* 1. Send Action-Request with INVALID client proof (0xAA instead of expected 0x55) */
    uint8_t bad_action_apdu[64];
    bad_action_apdu[0] = 0xE6;
    bad_action_apdu[1] = 0xE6;
    bad_action_apdu[2] = 0x00;                               /* LLC */
    bad_action_apdu[3] = 0xC3;                               /* Action-Request */
    bad_action_apdu[4] = 0x01;                               /* Action-Request-Normal */
    bad_action_apdu[5] = 0xC1;                               /* Invoke ID 0xC1 */
    bad_action_apdu[6] = 0x00;
    bad_action_apdu[7] = 0x0F;                               /* Class 15: Association LN */
    bad_action_apdu[8] = 0x00;
    bad_action_apdu[9] = 0x00;
    bad_action_apdu[10] = 0x28;
    bad_action_apdu[11] = 0x00;
    bad_action_apdu[12] = 0x00;
    bad_action_apdu[13] = 0xFF;                              /* OBIS 0.0.40.0.0.255 */
    bad_action_apdu[14] = 0x01;                              /* Method 1: reply_to_HLS_authentication */
    bad_action_apdu[15] = 0x09;                              /* OctetString */
    bad_action_apdu[16] = 16u;                               /* Length 16 */
    memset(&bad_action_apdu[17], 0xAA, 16);                  /* Bad client proof */
    size_t bad_action_len = 17u + 16u;

    uint8_t wire_buf[256];
    size_t wire_len = 0u;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, bad_action_apdu, bad_action_len,
                                      wire_buf, sizeof(wire_buf), &wire_len) == AMI_HDLC_FRAME_READY);

    s_last_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&port, wire_buf, wire_len) == AMI_DLMS_PORT_OK);

    /* Parse response and verify ActionResponse with read-write-denied (3) */
    HdlcContext rx_hdlc;
    ami_dlms_hdlc_init(&rx_hdlc);
    HdlcFrame resp_frame;
    size_t parsed = 0;
    for (size_t i = 0; i < s_last_tx_len; i++) {
        if (ami_dlms_hdlc_parse_byte(&rx_hdlc, s_last_tx[i], &resp_frame) == AMI_HDLC_FRAME_READY) {
            parsed = 1;
            break;
        }
    }
    assert(parsed == 1);
    assert(resp_frame.payload_length >= 5);
    /* Action-Response tag 0xC7, result 0x03 */
    assert(resp_frame.payload[0] == 0xC7u);
    assert(resp_frame.payload[3] == 0x03u); /* read-write-denied / auth failure */

    /* Verify failure was recorded in Anti-Brute-Force context! */
    assert(abf.failed_attempts == 1u);
    assert(abf.lockout_until_sec == 1005u); /* 1000 + 5 */
    assert(ami_anti_bruteforce_is_allowed(&abf, 1000u) == 0);

    /* 2. Immediately send AARQ while locked out -> Must be rejected with AARE lockout rejection */
    static const uint8_t aarq_raw[] = {
        0xE6, 0xE6, 0x00,
        0x60, 0x1D,
        0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01,
        0xBE, 0x10, 0x04, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x1E, 0x5D, 0xFF, 0xFF
    };
    wire_len = 0;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, aarq_raw, sizeof(aarq_raw),
                                      wire_buf, sizeof(wire_buf), &wire_len) == AMI_HDLC_FRAME_READY);
    s_last_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&port, wire_buf, wire_len) == AMI_DLMS_PORT_OK);

    ami_dlms_hdlc_init(&rx_hdlc);
    parsed = 0;
    for (size_t i = 0; i < s_last_tx_len; i++) {
        if (ami_dlms_hdlc_parse_byte(&rx_hdlc, s_last_tx[i], &resp_frame) == AMI_HDLC_FRAME_READY) {
            parsed = 1;
            break;
        }
    }
    assert(parsed == 1);
    assert(resp_frame.payload[0] == 0x61u); /* AARE lockout rejection */

    /* 3. Advance virtual time past the 5-second penalty window (t = 1005) */
    ami_dlms_port_set_time(&port, 1005u);
    assert(ami_anti_bruteforce_is_allowed(&abf, 1005u) == 1);

    /* 4. Send Action-Request with VALID client proof (0x55) */
    port.association.state = AMI_DLMS_ASSOC_AUTH_PENDING;
    uint8_t good_action_apdu[64];
    memcpy(good_action_apdu, bad_action_apdu, sizeof(good_action_apdu));
    memset(&good_action_apdu[17], 0x55, 16); /* Good proof matching mock_test_hls_proof */

    wire_len = 0;
    assert(ami_dlms_hdlc_encode_frame(1, 16, 0x10, good_action_apdu, bad_action_len,
                                      wire_buf, sizeof(wire_buf), &wire_len) == AMI_HDLC_FRAME_READY);
    s_last_tx_len = 0;
    assert(ami_dlms_port_process_buffer(&port, wire_buf, wire_len) == AMI_DLMS_PORT_OK);

    ami_dlms_hdlc_init(&rx_hdlc);
    parsed = 0;
    for (size_t i = 0; i < s_last_tx_len; i++) {
        if (ami_dlms_hdlc_parse_byte(&rx_hdlc, s_last_tx[i], &resp_frame) == AMI_HDLC_FRAME_READY) {
            parsed = 1;
            break;
        }
    }
    assert(parsed == 1);
    assert(resp_frame.payload[0] == 0xC7u);
    assert(resp_frame.payload[3] == 0x00u); /* Action-Response result = success! */

    /* Success resets anti-bruteforce failure count! */
    assert(abf.failed_attempts == 0u);
    assert(abf.lockout_until_sec == 0u);

    printf("test_abf_dlms_port_action_hls_reply: PASS\n");
}

static void test_abf_zero_config_resilience(void)
{
    AmiAntiBruteforceContext ctx;
    AmiAntiBruteforceConfig bad_config;
    memset(&bad_config, 0, sizeof(bad_config)); /* All zeros */

    assert(ami_anti_bruteforce_init(&ctx, &bad_config) == 0);

    /* Record multiple failures with zero config to ensure no SIGFPE/arithmetic exception occurs */
    for (uint32_t t = 100u; t <= 500u; t += 100u) {
        uint32_t delay = ami_anti_bruteforce_record_failure(&ctx, t);
        assert(delay > 0u);
    }
    assert(ctx.is_locked_permanent == 1);
    printf("test_abf_zero_config_resilience: PASS\n");
}

int main(void)
{
    printf("=== Starting Anti-Brute-Force & Lockout Test Suite ===\n");
    test_abf_default_initialization();
    test_abf_progressive_exponential_backoff();
    test_abf_administrative_unlock();
    test_abf_success_resets_backoff();
    test_abf_dlms_port_aarq_lockout_rejection();
    test_abf_dlms_port_action_hls_reply();
    test_abf_zero_config_resilience();
    printf("=== All Anti-Brute-Force Tests PASSED (100%%) ===\n");
    return 0;
}
