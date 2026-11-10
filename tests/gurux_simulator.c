#include "ami_cosem_model.h"
#include "ami_dlms_apdu.h"
#include "ami_dlms_association.h"
#include "ami_dlms_hdlc.h"
#include "ami_dlms_block_transfer.h"
#include "ami_disconnect_control.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_VALUE INVALID_SOCKET
static int socket_startup(void) { WSADATA data; return WSAStartup(MAKEWORD(2, 2), &data); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#define SOCKET_ERROR_VALUE (-1)
static int socket_startup(void) { return 0; }
#endif

#define SERVER_PORT 4059

static AmiBlockTransferContext s_sim_bt;
static AmiDisconnectControlContext s_sim_dc;

static int sim_breaker_actuator(void *user, AmiBreakerCommand cmd)
{
    (void)user;
    if (cmd == AMI_BREAKER_CMD_OPEN) {
        fprintf(stdout, ">>> [HARDWARE BREAKER] Pulse RESET Coil (100ms) -> Breaker CONTACTS OPENED <<<\n");
    } else {
        fprintf(stdout, ">>> [HARDWARE BREAKER] Pulse SET Coil (100ms) -> Breaker CONTACTS CLOSED <<<\n");
    }
    fflush(stdout);
    return 0;
}

static int send_all(socket_t client, const uint8_t *data, size_t length)
{
    size_t sent = 0u;
    while (sent < length) {
#ifdef _WIN32
        int n = send(client, (const char *)data + sent, (int)(length - sent), 0);
#else
        ssize_t n = send(client, data + sent, length - sent, 0);
#endif
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

static void make_development_snapshot(AmiCosemRegisterSnapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->phase_voltage_millivolts[0] = 230000u;
    snapshot->phase_voltage_millivolts[1] = 231000u;
    snapshot->phase_voltage_millivolts[2] = 229000u;
    snapshot->phase_current_milliamps[0] = 10500u;
    snapshot->phase_current_milliamps[1] = 10300u;
    snapshot->phase_current_milliamps[2] = 9800u;
    snapshot->active_import_wh = 1234567u;
    snapshot->active_export_wh = 0u;

    /* Multi-Tariff (WBP & LWBP) */
    snapshot->active_import_tariff_wh[0] = 500000u;  /* T1 LWBP 1 */
    snapshot->active_import_tariff_wh[1] = 600000u;  /* T2 WBP */
    snapshot->active_import_tariff_wh[2] = 134567u;  /* T3 LWBP 2 */
    snapshot->active_import_tariff_wh[3] = 0u;       /* T4 */

    /* Power Quality */
    snapshot->frequency_mhz = 50010u;                /* 50.01 Hz */
    snapshot->power_factor[0] = 985;                 /* PF Total = 0.985 */
    snapshot->power_factor[1] = 982;
    snapshot->power_factor[2] = 987;
    snapshot->power_factor[3] = 986;
    snapshot->thd_voltage_percent_x10[0] = 15u;      /* 1.5% */
    snapshot->thd_voltage_percent_x10[1] = 14u;
    snapshot->thd_voltage_percent_x10[2] = 16u;
    snapshot->thd_voltage_percent_x10[3] = 15u;
    snapshot->thd_current_percent_x10[0] = 22u;      /* 2.2% */
    snapshot->thd_current_percent_x10[1] = 21u;
    snapshot->thd_current_percent_x10[2] = 24u;
    snapshot->thd_current_percent_x10[3] = 22u;

    /* Demand Limiter Class 71 & Disconnect Control Class 70 */
    snapshot->limiter_threshold_active_watts = 13200u;
    snapshot->limiter_threshold_normal_watts = 13000u;
    snapshot->limiter_min_over_threshold_duration_sec = 10u;
    snapshot->limiter_min_under_threshold_duration_sec = 30u;
    snapshot->disconnect_output_state = 1u;
    snapshot->disconnect_control_state = 1u;
    snapshot->disconnect_control_mode = 2u;
    strcpy(snapshot->meter_id, "PLN2026102801");
    strcpy(snapshot->meter_type, "3P-POSTPAID-SPLN");

    /* Sprint 1 host profile includes the COSEM Clock object, so make its
     * attribute 2 available to Gurux instead of advertising unavailable data. */
    snapshot->clock_date_time.year[0] = 0x07u;
    snapshot->clock_date_time.year[1] = 0xEAu; /* 2026 */
    snapshot->clock_date_time.month = 11u;     /* November */
    snapshot->clock_date_time.day = 6u;        /* 6th */
    snapshot->clock_date_time.day_of_week = 5u;
    snapshot->clock_date_time.hour = 10u;
    snapshot->clock_date_time.minute = 10u;
    snapshot->clock_date_time.second = 0u;
    snapshot->clock_date_time.deviation[0] = 0x80u; /* unspecified */
    snapshot->clock_date_time.deviation[1] = 0x00u;

    /* Reactive energy & instantaneous power (dibutuhkan oleh 1.0.3.8.0, 1.0.4.8.0,
     * 1.0.3.8.x dan 1.0.3.7.0 agar Gurux tidak menerima "temporary-failure"). */
    snapshot->reactive_import_varh = 345678u;
    snapshot->reactive_export_varh = 1234u;
    snapshot->reactive_import_tariff_varh[0] = 120000u;
    snapshot->reactive_import_tariff_varh[1] = 150000u;
    snapshot->reactive_import_tariff_varh[2] = 75678u;
    snapshot->reactive_import_tariff_varh[3] = 0u;
    snapshot->active_power_watts = 7150u;
    snapshot->reactive_power_vars = 1240u;

    /* Security Setup (0.0.43.0.0.255) attribute 4/5: system titles */
    static const uint8_t client_title[8] = {'G', 'R', 'X', 0x00u, 0x00u, 0x00u, 0x00u, 0x01u};
    static const uint8_t server_title[8] = {'P', 'L', 'N', 0x26u, 0x10u, 0x28u, 0x01u, 0x01u};
    memcpy(snapshot->client_system_title, client_title, sizeof(client_title));
    memcpy(snapshot->server_system_title, server_title, sizeof(server_title));

    /* Load Profile 15-min (1.0.99.1.0.255): satu entri contoh */
    snapshot->load_profile_count = 1u;
    snapshot->load_profile[0].timestamp = snapshot->clock_date_time;
    snapshot->load_profile[0].active_import_wh = snapshot->active_import_wh;
    snapshot->load_profile[0].active_export_wh = snapshot->active_export_wh;
    snapshot->load_profile[0].reactive_import_varh = snapshot->reactive_import_varh;
    snapshot->load_profile[0].reactive_export_varh = snapshot->reactive_export_varh;
    for (size_t i = 0u; i < 3u; ++i) {
        snapshot->load_profile[0].phase_voltage_mv[i] = snapshot->phase_voltage_millivolts[i];
        snapshot->load_profile[0].phase_current_ma[i] = snapshot->phase_current_milliamps[i];
    }
    snapshot->load_profile[0].active_power_watts = snapshot->active_power_watts;
    snapshot->load_profile[0].reactive_power_vars = snapshot->reactive_power_vars;
    snapshot->load_profile[0].power_factor = snapshot->power_factor[0];
    snapshot->load_profile[0].frequency_mhz = snapshot->frequency_mhz;

    /* Extended SPLN registers (per-phase power/energy, apparent, quadrant, neutral).
     * Register ini dibaca dari measurement_samples; tanpa sampel, model mengembalikan
     * AMI_COSEM_VALUE_UNAVAILABLE -> Gurux: "Device reports a temporary failure". */
    static const struct { uint8_t c, d; uint64_t value; } ext_samples[] = {
        /* Neutral current / voltage (scaler -3) */
        {91u, 7u, 300u}, {92u, 7u, 500u},
        /* Total export / apparent power */
        {2u, 7u, 0u}, {4u, 7u, 0u}, {9u, 7u, 7260u}, {10u, 7u, 0u},
        /* Per-phase active power import / export (W) */
        {21u, 7u, 2380u}, {41u, 7u, 2340u}, {61u, 7u, 2430u},
        {22u, 7u, 0u}, {42u, 7u, 0u}, {62u, 7u, 0u},
        /* Per-phase reactive power import / export (var) */
        {23u, 7u, 410u}, {43u, 7u, 415u}, {63u, 7u, 415u},
        {24u, 7u, 0u}, {44u, 7u, 0u}, {64u, 7u, 0u},
        /* Per-phase apparent power import / export (VA) */
        {29u, 7u, 2415u}, {49u, 7u, 2380u}, {69u, 7u, 2245u},
        {30u, 7u, 0u}, {50u, 7u, 0u}, {70u, 7u, 0u},
        /* Per-phase active energy import / export (Wh) */
        {21u, 8u, 411522u}, {41u, 8u, 411523u}, {61u, 8u, 411522u},
        {22u, 8u, 0u}, {42u, 8u, 0u}, {62u, 8u, 0u},
        /* Per-phase reactive energy import / export (varh) */
        {23u, 8u, 115226u}, {43u, 8u, 115226u}, {63u, 8u, 115226u},
        {24u, 8u, 411u}, {44u, 8u, 411u}, {64u, 8u, 412u},
        /* Per-phase apparent energy import / export (VAh) */
        {29u, 8u, 425000u}, {49u, 8u, 423000u}, {69u, 8u, 427000u},
        {30u, 8u, 0u}, {50u, 8u, 0u}, {70u, 8u, 0u},
        /* Per-phase reactive energy per quadrant Q1..Q4 (varh) */
        {25u, 8u, 100000u}, {45u, 8u, 100000u}, {65u, 8u, 100000u},
        {26u, 8u, 15226u}, {66u, 8u, 15226u},
        {27u, 8u, 300u}, {47u, 8u, 300u}, {67u, 8u, 300u},
        {28u, 8u, 111u}, {48u, 8u, 111u}, {68u, 8u, 112u},
        /* Total reactive energy per quadrant Q1..Q4 (varh) */
        {5u, 8u, 300000u}, {6u, 8u, 45678u}, {7u, 8u, 900u}, {8u, 8u, 334u},
    };
    snapshot->measurement_sample_count = 0u;
    for (size_t i = 0u; i < sizeof(ext_samples) / sizeof(ext_samples[0]) &&
                        snapshot->measurement_sample_count < AMI_COSEM_REGISTER_SAMPLE_MAX; ++i) {
        AmiCosemRegisterSample *s = &snapshot->measurement_samples[snapshot->measurement_sample_count++];
        s->logical_name[0] = 1u;
        s->logical_name[1] = 0u;
        s->logical_name[2] = ext_samples[i].c;
        s->logical_name[3] = ext_samples[i].d;
        s->logical_name[4] = 0u;
        s->logical_name[5] = 255u;
        s->raw_value = ext_samples[i].value;
        s->signed_value = (int32_t)ext_samples[i].value;
        s->valid = 1u;
    }

    snapshot->valid_mask = AMI_COSEM_VALID_REACTIVE_IMPORT |
                           AMI_COSEM_VALID_REACTIVE_EXPORT |
                           AMI_COSEM_VALID_SECURITY_CLIENT_TITLE |
                           AMI_COSEM_VALID_LOAD_PROFILE |
                           AMI_COSEM_VALID_VOLTAGE_A |
                           AMI_COSEM_VALID_VOLTAGE_B |
                           AMI_COSEM_VALID_VOLTAGE_C |
                           AMI_COSEM_VALID_CURRENT_A |
                           AMI_COSEM_VALID_CURRENT_B |
                           AMI_COSEM_VALID_CURRENT_C |
                           AMI_COSEM_VALID_ACTIVE_IMPORT |
                           AMI_COSEM_VALID_ACTIVE_EXPORT |
                           AMI_COSEM_VALID_CLOCK |
                           AMI_COSEM_VALID_EVENT_LOG |
                           AMI_COSEM_VALID_BILLING_PROFILE |
                           AMI_COSEM_VALID_TARIFF_REGISTERS |
                           AMI_COSEM_VALID_POWER_QUALITY |
                           AMI_COSEM_VALID_LIMITER |
                           AMI_COSEM_VALID_DISCONNECT_CTRL |
                           AMI_COSEM_VALID_DEVICE_INFO;
    snapshot->event_log_count = 1u;
    snapshot->event_log[0].timestamp = snapshot->clock_date_time;
    snapshot->event_log[0].event_code = 1u;

    snapshot->billing_history_count = 1u;
    snapshot->billing_history[0].billing_timestamp = snapshot->clock_date_time;
    snapshot->billing_history[0].active_import_wh = 1234567u;
    snapshot->billing_history[0].active_export_wh = 0u;
}

static int process_i_frame(HdlcContext *hdlc, const HdlcFrame *request,
                           AmiDlmsAssociation *association,
                           AmiCosemRegisterSnapshot *snapshot,
                           uint8_t *wire_response, size_t wire_capacity,
                           size_t *wire_length)
{
    uint8_t apdu_response[AMI_BT_MAX_CACHE_BUFFER_SIZE];
    size_t apdu_length = 0u;
    AmiDlmsAssociationStatus association_status;
    AmiDlmsApduStatus apdu_status;

    *wire_length = 0u;
    /* ami_dlms_hdlc_parse_byte removes the LLC header after frame validation. */
    const uint8_t *apdu = request->payload;
    const size_t apdu_length_in = request->payload_length;
    if (apdu_length_in == 0u) return 1;

    if (apdu[0] == 0x60u) {
        ami_dlms_association_reset(association);
        ami_bt_reset(&s_sim_bt);
        association_status = ami_dlms_process_aarq(association, apdu, apdu_length_in,
            apdu_response, sizeof(apdu_response), &apdu_length);
        fprintf(stdout, "[APDU AARE] status=%d response=%u bytes",
                (int)association_status, (unsigned)apdu_length);
        for (size_t i = 0u; i < apdu_length && i < 48u; ++i)
            fprintf(stdout, " %02X", apdu_response[i]);
        fputc('\n', stdout);
        fflush(stdout);
        if (association_status != AMI_DLMS_ASSOC_OK) {
            fprintf(stderr, "AARQ rejected (status=%d)\n", (int)association_status);
            if (apdu_length == 0u) return 1;
        }
    } else if (apdu[0] == 0x62u) {
        /* ACSE RLRQ (Release Request) -> Reply with RLRE (Release Response) */
        ami_dlms_association_reset(association);
        ami_bt_reset(&s_sim_bt);
        apdu_response[0] = 0x63u; /* RLRE tag */
        apdu_response[1] = 0x03u; /* length */
        apdu_response[2] = 0x80u; /* reason tag */
        apdu_response[3] = 0x01u;
        apdu_response[4] = 0x00u; /* normal */
        apdu_length = 5u;
        fprintf(stdout, "[APDU RLRE] Association released cleanly.\n");
        fflush(stdout);
    } else if (association->state != AMI_DLMS_ASSOC_ASSOCIATED) {
        /* When unassociated:
         * Per DLMS CTT T_APPL_IDLE_N1, meter must NOT answer for Get request.
         * For Short Name Read (0x05) when checking disconnect (APPL_REL_P1), return ExceptionResponse.
         */
        if (apdu[0] == 0x05u) {
            apdu_response[0] = 0xD8u; /* ExceptionResponse */
            apdu_response[1] = 0x00u; /* no state error */
            apdu_response[2] = 0x02u; /* service not supported */
            apdu_length = 3u;
            fprintf(stdout, "[APDU EXCEPTION] Unassociated Read SN rejected\n");
            fflush(stdout);
        } else {
            fprintf(stdout, "[APDU IDLE] Silently dropping unassociated request 0x%02X per T_APPL_IDLE_N1\n", apdu[0]);
            fflush(stdout);
            return 1;
        }
    } else if (apdu[0] == 0xC0u) {
        if (apdu_length_in >= 2u && apdu[1] == 0x02u) {
            /* GET-Request-For-Next-Datablock */
            uint8_t req_invoke_id = 0u;
            uint32_t req_block_num = 0u;
            if (ami_bt_parse_get_next_block_request(apdu, apdu_length_in, &req_invoke_id, &req_block_num)) {
                int is_last = 0;
                /* If client acknowledges the block it already received (e.g. block 1 in Gurux DLMS Director),
                 * advance to next block. If client already requested the target block (block 2), use it. */
                uint32_t target_block = req_block_num;
                if (target_block == s_sim_bt.current_block_number) {
                    target_block = s_sim_bt.current_block_number + 1u;
                }
                AmiBtStatus bt_st = ami_bt_build_get_response_block(
                    &s_sim_bt, target_block, apdu_response, sizeof(apdu_response), &apdu_length, &is_last);
                if (bt_st != AMI_BT_OK) {
                    apdu_length = ami_bt_build_error_response_block(
                        req_invoke_id, target_block, 2u, apdu_response, sizeof(apdu_response));
                }
                fprintf(stdout, "[APDU GET-DATABLOCK] req=%u target=%u last=%d length=%u bytes\n",
                        (unsigned)req_block_num, (unsigned)target_block, is_last, (unsigned)apdu_length);
                fflush(stdout);
            }
        } else {
            apdu_status = ami_dlms_process_get_request(association, snapshot, apdu,
                apdu_length_in, apdu_response, sizeof(apdu_response), &apdu_length);

            /* Safety fallback: if processing returned length 0, never send empty APDU */
            if (apdu_length == 0u) {
                apdu_response[0] = 0xC4u; /* GetResponse */
                apdu_response[1] = 0x01u; /* GetResponseNormal */
                apdu_response[2] = apdu_length_in > 2u ? apdu[2] : 0u; /* invoke_id */
                apdu_response[3] = 0x01u; /* data-access-result choice: error */
                apdu_response[4] = 0x04u; /* object-undefined */
                apdu_length = 5u;
                fprintf(stdout, "[APDU GET] Safety fallback applied: object-undefined (5 bytes)\n");
            }

            /* If response payload exceeds HDLC frame limit (480 bytes), use block transfer */
            if (apdu_status == AMI_DLMS_APDU_OK && apdu_length > 480u &&
                apdu_response[0] == 0xC4u && apdu_response[1] == 0x01u && apdu_response[3] == 0x00u) {
                uint8_t invoke_id = apdu_response[2];
                AmiBtStatus bt_st = ami_bt_start_get_transfer(
                    &s_sim_bt, invoke_id, &apdu_response[4], apdu_length - 4u);
                if (bt_st == AMI_BT_OK) {
                    int is_last = 0;
                    (void)ami_bt_build_get_response_block(
                        &s_sim_bt, 1u, apdu_response, sizeof(apdu_response), &apdu_length, &is_last);
                    fprintf(stdout, "[APDU GET-WITH-DATABLOCK START] block=1 last=%d length=%u bytes\n",
                            is_last, (unsigned)apdu_length);
                    fflush(stdout);
                }
            } else {
                fprintf(stdout, "[APDU GET] status=%d response=%u bytes",
                        (int)apdu_status, (unsigned)apdu_length);
                for (size_t i = 0u; i < apdu_length && i < 16u; ++i)
                    fprintf(stdout, " %02X", apdu_response[i]);
                fputc('\n', stdout);
                fflush(stdout);
            }
        }
    } else if (apdu[0] == 0xC1u) {
        /* SetRequest -> SetResponseNormal with read-write-denied (3) */
        apdu_response[0] = 0xC5u; /* SetResponse */
        apdu_response[1] = 0x01u; /* SetResponseNormal */
        apdu_response[2] = apdu_length_in > 2u ? apdu[2] : 0u; /* invoke_id */
        apdu_response[3] = 0x03u; /* read-write-denied */
        apdu_length = 4u;
        fprintf(stdout, "[APDU SET] Denied (read-only meter)\n");
        fflush(stdout);
    } else if (apdu[0] == 0xC3u) {
        if (apdu_length_in >= 12u &&
            apdu[1] == 0x01u &&
            apdu[3] == 0x00u && apdu[4] == 0x46u &&
            apdu[5] == 0x00u && apdu[6] == 0x00u && apdu[7] == 0x60u &&
            apdu[8] == 0x03u && apdu[9] == 0x0Au && apdu[10] == 0xFFu) {
            uint8_t method_id = apdu[11];
            AmiDisconnectStatus dc_st = AMI_DISCONNECT_ERR_INVALID_PARAM;
            if (method_id == 1u) {
                dc_st = ami_disconnect_control_remote_disconnect(&s_sim_dc, 0);
                fprintf(stdout, "[APDU ACTION] remote_disconnect -> status=%d (Breaker State: %u)\n",
                        (int)dc_st, s_sim_dc.control_state);
            } else if (method_id == 2u) {
                dc_st = ami_disconnect_control_remote_reconnect(&s_sim_dc, 0);
                fprintf(stdout, "[APDU ACTION] remote_reconnect -> status=%d (Breaker State: %u)\n",
                        (int)dc_st, s_sim_dc.control_state);
            }
            if (dc_st == AMI_DISCONNECT_OK) {
                ami_disconnect_control_sync_snapshot(&s_sim_dc, snapshot);
                apdu_response[0] = 0xC7u;
                apdu_response[1] = 0x01u;
                apdu_response[2] = apdu[2];
                apdu_response[3] = 0x00u; /* success */
                apdu_response[4] = 0x00u; /* data-present = false */
                apdu_length = 5u;
            } else if (dc_st == AMI_DISCONNECT_ERR_NOT_PERMITTED) {
                apdu_response[0] = 0xC7u;
                apdu_response[1] = 0x01u;
                apdu_response[2] = apdu[2];
                apdu_response[3] = 0x01u; /* error */
                apdu_response[4] = 0x03u; /* read-write-denied */
                apdu_length = 5u;
            } else {
                apdu_response[0] = 0xC7u;
                apdu_response[1] = 0x01u;
                apdu_response[2] = apdu[2];
                apdu_response[3] = 0x01u;
                apdu_response[4] = 0x02u; /* temporary-failure */
                apdu_length = 5u;
            }
            fflush(stdout);
        } else {
            /* ActionRequest on unsupported method -> ActionResponseNormal with read-write-denied (3) */
            apdu_response[0] = 0xC7u; /* ActionResponse */
            apdu_response[1] = 0x01u; /* ActionResponseNormal */
            apdu_response[2] = apdu_length_in > 2u ? apdu[2] : 0u; /* invoke_id */
            apdu_response[3] = 0x01u; /* action-result choice: error */
            apdu_response[4] = 0x03u; /* read-write-denied */
            apdu_length = 5u;
            fprintf(stdout, "[APDU ACTION] Denied (read-only / unsupported method)\n");
            fflush(stdout);
        }
    } else {
        /* Short Name referencing (e.g. 0x05 ReadRequest) or unsupported service on LN meter */
        apdu_response[0] = 0xD8u; /* ExceptionResponse */
        apdu_response[1] = 0x00u; /* no state error */
        apdu_response[2] = 0x02u; /* service-error: service-not-supported */
        apdu_length = 3u;
        fprintf(stdout, "[APDU EXCEPTION] Service 0x%02X not supported (LN meter)\n", apdu[0]);
        fflush(stdout);
    }

    return ami_dlms_hdlc_build_frame(hdlc, request, apdu_response, apdu_length,
        wire_response, wire_capacity, wire_length) == AMI_HDLC_FRAME_READY;
}

static int serve_client(socket_t client)
{
    HdlcContext hdlc;
    HdlcFrame frame;
    AmiDlmsAssociation association = {0};
    AmiCosemRegisterSnapshot snapshot;
    uint8_t wire_response[AMI_HDLC_MAX_FRAME_BYTES];
    size_t response_length;

    ami_dlms_hdlc_init(&hdlc);
    ami_bt_init(&s_sim_bt, AMI_BT_DEFAULT_BLOCK_SIZE);
    ami_disconnect_control_init(&s_sim_dc, AMI_DISCONNECT_MODE_2, sim_breaker_actuator, NULL);
    make_development_snapshot(&snapshot);
    ami_disconnect_control_sync_snapshot(&s_sim_dc, &snapshot);
    fprintf(stdout, "Gurux client connected; development no-auth profile active.\n");
    fflush(stdout);

    unsigned byte_count = 0;
    for (;;) {
        uint8_t byte;
#ifdef _WIN32
        int received = recv(client, (char *)&byte, 1, 0);
#else
        ssize_t received = recv(client, &byte, 1u, 0);
#endif
        if (received == 0) return 1;
        if (received < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Client socket timeout reached.\n");
                return 0;
            }
#else
            if (WSAGetLastError() == WSAETIMEDOUT) {
                fprintf(stderr, "Client socket timeout reached.\n");
                return 0;
            }
#endif
            perror("recv");
            return 0;
        }

        if (byte_count++ < 16) {
            fprintf(stdout, "[RX byte #%u: 0x%02X ('%c')]\n",
                    byte_count, byte, (byte >= 32 && byte <= 126) ? (char)byte : '.');
            fflush(stdout);
        }

        /* Detect if an HTTP browser (Chrome/Edge) or non-DLMS client connects by mistake */
        if (byte_count == 4 && hdlc.frame_length == 0u) {
            /* If the stream started with "GET " or "POST" */
            if (byte == ' ' || byte == 'T') {
                fprintf(stderr, "\n[PERINGATAN] Client mengirim request HTTP (bukan DLMS HDLC). Browser Anda sedang membuka port ini!\n");
                const char *http_resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                                        "Ini adalah Smart Meter DLMS HDLC Simulator Port 4059.\r\n"
                                        "Silakan tutup tab browser ini dan sambungkan melalui software GXDLMSDirector.\r\n";
                (void)send(client, http_resp, (int)strlen(http_resp), 0);
                return 0;
            }
        }

        if (byte == AMI_HDLC_FLAG && hdlc.frame_length != 0u) {
            fprintf(stdout, "[HDLC raw frame: %u bytes]", (unsigned)hdlc.frame_length);
            for (size_t i = 0u; i < hdlc.frame_length; ++i)
                fprintf(stdout, " %02X", hdlc.frame_buffer[i]);
            fputc('\n', stdout);
            fflush(stdout);
        }
        AmiHdlcStatus status = ami_dlms_hdlc_parse_byte(&hdlc, byte, &frame);
        if (status == AMI_HDLC_INCOMPLETE) continue;
        if (status != AMI_HDLC_FRAME_READY) {
            fprintf(stderr, "[HDLC Frame Error: %d on byte 0x%02X]\n", (int)status, byte);
            fflush(stderr);
            continue;
        }

        fprintf(stdout, "[RX from Gurux] Type=%d (Ctrl=0x%02X, Dst=%u, Src=%u, Payload=%u bytes)\n",
                (int)frame.type, frame.control, frame.destination_sap, frame.source_sap, (unsigned)frame.payload_length);
        fflush(stdout);

        response_length = 0u;
        if (frame.type == AMI_HDLC_FRAME_SNRM || frame.type == AMI_HDLC_FRAME_DISC) {
            ami_dlms_association_reset(&association);
            ami_bt_reset(&s_sim_bt);
            status = ami_dlms_hdlc_build_frame(&hdlc, &frame, NULL, 0u,
                wire_response, sizeof(wire_response), &response_length);
            if (status == AMI_HDLC_FRAME_READY) {
                fprintf(stdout, "[TX to Gurux] UA Response (%u bytes)\n", (unsigned)response_length);
                fflush(stdout);
                if (!send_all(client, wire_response, response_length)) return 0;
            } else {
                fprintf(stderr, "[ERR] Failed to build UA response (status=%d)\n", (int)status);
            }
        } else if (frame.type == AMI_HDLC_FRAME_RR) {
            status = ami_dlms_hdlc_build_frame(&hdlc, &frame, NULL, 0u,
                wire_response, sizeof(wire_response), &response_length);
            if (status == AMI_HDLC_FRAME_READY) {
                fprintf(stdout, "[TX to Gurux] RR Response (%u bytes):", (unsigned)response_length);
                for (size_t i = 0u; i < response_length; ++i) fprintf(stdout, " %02X", wire_response[i]);
                fputc('\n', stdout);
                fflush(stdout);
                if (!send_all(client, wire_response, response_length)) return 0;
            } else {
                fprintf(stderr, "[ERR] Failed to build RR response (status=%d)\n", (int)status);
            }
        } else if (frame.type == AMI_HDLC_FRAME_I) {
            if (!process_i_frame(&hdlc, &frame, &association, &snapshot,
                    wire_response, sizeof(wire_response), &response_length)) {
                fprintf(stderr, "Could not build I-frame response.\n");
                continue;
            }
            if (response_length != 0u) {
                fprintf(stdout, "[TX to Gurux] I-Frame Response (%u bytes)\n", (unsigned)response_length);
                fflush(stdout);
                if (!send_all(client, wire_response, response_length)) return 0;
            }
        }
    }
}

int main(void)
{
    if (socket_startup() != 0) {
        fprintf(stderr, "Socket initialization failed.\n");
        return 1;
    }

    socket_t server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == SOCKET_ERROR_VALUE) {
        fprintf(stderr, "Could not create TCP socket.\n");
        return 1;
    }

    int reuse = 1;
    (void)setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(SERVER_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server, 8) != 0) {
        fprintf(stderr, "Could not bind/listen on 0.0.0.0:%d.\n", SERVER_PORT);
        CLOSE_SOCKET(server);
        return 1;
    }
    fprintf(stdout, "Listening for raw HDLC-over-TCP on 0.0.0.0:%d\n", SERVER_PORT);

    for (;;) {
        socket_t client = accept(server, NULL, NULL);
        if (client == SOCKET_ERROR_VALUE) {
            perror("accept");
            continue;
        }
        int nodelay = 1;
        (void)setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
        int sndbuf = 0;
        (void)setsockopt(client, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sizeof(sndbuf));

        /* Set 180-second timeout on receive so GXDLMSDirector user has ample time between operations */
#ifdef _WIN32
        DWORD timeout_ms = 180000;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = 180;
        tv.tv_usec = 0;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#endif

        (void)serve_client(client);
        CLOSE_SOCKET(client);
        fprintf(stdout, "Client disconnected.\n");
        fflush(stdout);
    }
}
