#include "ami_cosem_model.h"
#include "ami_dlms_apdu.h"
#include "ami_dlms_association.h"
#include "ami_dlms_cipher.h"
#include "ami_dlms_hdlc.h"
#include "ami_dlms_port.h"
#include "ami_dlms_block_transfer.h"
#include "ami_disconnect_control.h"
#include "ami_anti_bruteforce.h"
#include "ami_security.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

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

static AmiDisconnectControlContext s_sim_dc;
static AmiAntiBruteforceContext s_sim_abf;
static AmiDlmsSecuritySuite0Context s_sim_suite0;
static AmiDlmsCipherBackend s_sim_cipher_backend;
static AmiCosemRegisterSnapshot s_sim_snapshot;
static uint32_t s_sim_rng_state = 0x53504C4Eu;
static int s_sim_rx_ic_initialized = 0;

static const uint8_t s_gurux_default_guek[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static const uint8_t s_gurux_default_ak[16] = {
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
};
static const uint8_t s_gurux_default_kek[16] = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36
};
static const uint8_t s_gurux_server_sys_title[8] = {
    0x4D, 0x4D, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x01 /* MMM00001 */
};
static const uint8_t s_gurux_default_client_sys_title[8] = {
    0x47, 0x58, 0x44, 0x4C, 0x4D, 0x53, 0x30, 0x31
};

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

static int sim_random_cb(void *user, uint8_t *out, size_t len)
{
    (void)user;
    if (out == NULL || len == 0u) return 0;
    for (size_t i = 0u; i < len; ++i) {
        s_sim_rng_state = s_sim_rng_state * 1664525u + 1013904223u;
        out[i] = (uint8_t)(s_sim_rng_state >> 24);
    }
    return 1;
}

static int sim_hls_proof_cb(void *user, const uint8_t *oid, size_t oid_len,
                            const uint8_t *challenge, size_t challenge_len,
                            uint8_t *proof, size_t proof_cap, size_t *proof_len)
{
    (void)user; (void)oid; (void)oid_len;
    uint32_t tx_ic = ++s_sim_suite0.current_tx_counter;
    AmiSecStatus st = ami_sec_dlms_hls5_gmac_calculate(
        s_sim_suite0.authentication_key,
        s_sim_suite0.server_system_title,
        tx_ic,
        challenge, challenge_len,
        proof, proof_cap, proof_len);
    fprintf(stdout, "[HLS5 StoC] Generated server proof (%u bytes, IC=%u, status=%d)\n",
            (unsigned)(*proof_len), (unsigned)tx_ic, (int)st);
    fflush(stdout);
    return (st == AMI_SEC_OK) ? 1 : 0;
}

static int sim_hls_verify_cb(void *user, const uint8_t *oid, size_t oid_len,
                             const uint8_t *client_sys_title, size_t client_sys_title_len,
                             const uint8_t *challenge, size_t challenge_len,
                             const uint8_t *client_proof, size_t client_proof_len)
{
    (void)user; (void)oid; (void)oid_len;
    if (client_sys_title != NULL && client_sys_title_len == AMI_DLMS_SYSTEM_TITLE_LENGTH) {
        memcpy(s_sim_suite0.client_system_title, client_sys_title, AMI_DLMS_SYSTEM_TITLE_LENGTH);
    }
    AmiSecStatus st = ami_sec_dlms_hls5_gmac_verify(
        s_sim_suite0.authentication_key,
        s_sim_suite0.client_system_title,
        client_proof, client_proof_len,
        challenge, challenge_len);
    fprintf(stdout, "[HLS5 Verify CtoS] status=%d (0=OK)\n", (int)st);
    fflush(stdout);
    return (st == AMI_SEC_OK) ? 1 : 0;
}

static int sim_decrypt_initiate_cb(void *user,
                                   const uint8_t *client_sys_title, size_t client_sys_title_len,
                                   uint8_t sc, uint32_t ic,
                                   const uint8_t *ct, size_t ct_len,
                                   const uint8_t *tag, size_t tag_len,
                                   uint8_t *pt, size_t pt_cap, size_t *pt_len)
{
    (void)user;
    if (pt_cap < ct_len || sc != AMI_DLMS_SECURITY_CONTROL_SUITE_0) return 0;
    if (client_sys_title != NULL && client_sys_title_len == AMI_DLMS_SYSTEM_TITLE_LENGTH) {
        memcpy(s_sim_suite0.client_system_title, client_sys_title, AMI_DLMS_SYSTEM_TITLE_LENGTH);
    }
    uint8_t nonce[AMI_GCM_NONCE_BYTES];
    memcpy(nonce, s_sim_suite0.client_system_title, AMI_DLMS_SYSTEM_TITLE_LENGTH);
    nonce[8]  = (uint8_t)(ic >> 24);
    nonce[9]  = (uint8_t)(ic >> 16);
    nonce[10] = (uint8_t)(ic >> 8);
    nonce[11] = (uint8_t)ic;

    uint8_t aad[1u + AMI_DLMS_KEY_LENGTH];
    aad[0] = sc;
    memcpy(&aad[1], s_sim_suite0.authentication_key, AMI_DLMS_KEY_LENGTH);

    AmiSecStatus st = ami_sec_aes_128_gcm_decrypt(
        s_sim_suite0.encryption_key, nonce, sizeof(nonce),
        aad, sizeof(aad), ct, ct_len, tag, tag_len, pt);
    if (st != AMI_SEC_OK) {
        fprintf(stderr, "[glo_initiateRequest] Decrypt FAILED (status=%d, IC=%u)\n", (int)st, (unsigned)ic);
        return 0;
    }
    if (ic == 0u) {
        s_sim_rx_ic_initialized = 1;
        s_sim_suite0.last_rx_counter = 0u;
    } else if (s_sim_cipher_backend.accept_rx_counter != NULL &&
        s_sim_cipher_backend.accept_rx_counter(&s_sim_suite0, ic) != 1) {
        fprintf(stderr, "[glo_initiateRequest] Replay counter rejected (IC=%u)\n", (unsigned)ic);
        return 0;
    } else {
        s_sim_rx_ic_initialized = 1;
    }
    *pt_len = ct_len;
    fprintf(stdout, "[glo_initiateRequest] Decrypted OK (IC=%u, %u bytes)\n", (unsigned)ic, (unsigned)ct_len);
    fflush(stdout);
    return 1;
}

static int sim_encrypt_initiate_cb(void *user,
                                   uint8_t sc, uint32_t *ic_out,
                                   const uint8_t *pt, size_t pt_len,
                                   uint8_t *ct, size_t ct_cap, size_t *ct_len,
                                   uint8_t *tag, size_t tag_len)
{
    (void)user;
    if (ct_cap < pt_len || ic_out == NULL) return 0;
    uint32_t tx_ic = ++s_sim_suite0.current_tx_counter;
    *ic_out = tx_ic;
    uint8_t nonce[AMI_GCM_NONCE_BYTES];
    memcpy(nonce, s_sim_suite0.server_system_title, AMI_DLMS_SYSTEM_TITLE_LENGTH);
    nonce[8]  = (uint8_t)(tx_ic >> 24);
    nonce[9]  = (uint8_t)(tx_ic >> 16);
    nonce[10] = (uint8_t)(tx_ic >> 8);
    nonce[11] = (uint8_t)tx_ic;

    uint8_t aad[1u + AMI_DLMS_KEY_LENGTH];
    aad[0] = sc;
    memcpy(&aad[1], s_sim_suite0.authentication_key, AMI_DLMS_KEY_LENGTH);

    AmiSecStatus st = ami_sec_aes_128_gcm_encrypt(
        s_sim_suite0.encryption_key, nonce, sizeof(nonce),
        aad, sizeof(aad), pt, pt_len, ct, tag, tag_len);
    if (st != AMI_SEC_OK) return 0;
    *ct_len = pt_len;
    return 1;
}

typedef struct {
    socket_t client;
} SimTransportCtx;

static int sim_port_tx(const uint8_t *data, size_t length, void *user_data)
{
    SimTransportCtx *t = (SimTransportCtx *)user_data;
    fprintf(stdout, "[TX to Gurux] %u bytes:", (unsigned)length);
    for (size_t i = 0u; i < length; ++i) {
        fprintf(stdout, " %02X", data[i]);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return send_all(t->client, data, length);
}

static int sim_port_snapshot(AmiCosemRegisterSnapshot *snapshot, void *user_data)
{
    (void)user_data;
    ami_disconnect_control_sync_snapshot(&s_sim_dc, &s_sim_snapshot);
    memcpy(s_sim_snapshot.client_system_title, s_sim_suite0.client_system_title,
           AMI_DLMS_SYSTEM_TITLE_LENGTH);
    memcpy(s_sim_snapshot.server_system_title, s_sim_suite0.server_system_title,
           AMI_DLMS_SYSTEM_TITLE_LENGTH);
    s_sim_snapshot.security_policy = 0x30u; /* Authenticated + Encrypted */
    s_sim_snapshot.security_suite = 0u;     /* Suite 0: AES-128-GCM */
    *snapshot = s_sim_snapshot;
    return 1;
}

static int serve_client(socket_t client)
{
    SimTransportCtx transport = { .client = client };
    AmiDlmsPortContext port_ctx;

    ami_disconnect_control_init(&s_sim_dc, AMI_DISCONNECT_MODE_2, sim_breaker_actuator, NULL);
    ami_anti_bruteforce_init(&s_sim_abf, NULL);
    make_development_snapshot(&s_sim_snapshot);
    ami_disconnect_control_sync_snapshot(&s_sim_dc, &s_sim_snapshot);

    ami_dlms_cipher_suite0_init(&s_sim_suite0,
                                s_gurux_default_guek,
                                s_gurux_default_ak,
                                s_gurux_default_client_sys_title,
                                s_gurux_server_sys_title,
                                0u);
    s_sim_rx_ic_initialized = 0;
    (void)ami_dlms_cipher_suite0_set_kek(&s_sim_suite0, s_gurux_default_kek);
    (void)ami_dlms_cipher_suite0_get_backend(&s_sim_suite0, &s_sim_cipher_backend);

    ami_dlms_port_init(&port_ctx, sim_port_tx, sim_port_snapshot, &transport);
    ami_dlms_port_configure_block_transfer(&port_ctx, AMI_BT_DEFAULT_BLOCK_SIZE, 480u);
    ami_dlms_port_set_anti_bruteforce(&port_ctx, &s_sim_abf);
    ami_dlms_port_set_disconnect_control(&port_ctx, &s_sim_dc);
    ami_dlms_port_set_cipher_backend(&port_ctx, &s_sim_cipher_backend);

    static const uint8_t hls5_oid[7] = {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u};
    AmiDlmsAssociationSecurity sec_policy;
    memset(&sec_policy, 0, sizeof(sec_policy));
    sec_policy.allow_no_auth = 1u;   /* Public Client (None) */
    sec_policy.allow_ciphering = 1u; /* Management Client (HighGMAC / Suite 0) */
    sec_policy.server_system_title = s_sim_suite0.server_system_title;
    sec_policy.server_system_title_length = AMI_DLMS_SYSTEM_TITLE_LENGTH;
    sec_policy.random = sim_random_cb;
    sec_policy.hls_proof = sim_hls_proof_cb;
    sec_policy.hls_verify = sim_hls_verify_cb;
    sec_policy.decrypt_initiate = sim_decrypt_initiate_cb;
    sec_policy.encrypt_initiate = sim_encrypt_initiate_cb;
    sec_policy.hls_mechanism_oid = hls5_oid;
    sec_policy.hls_mechanism_oid_length = sizeof(hls5_oid);
    port_ctx.security = sec_policy;
    port_ctx.use_custom_security = 1;

    fprintf(stdout, "Gurux client connected; SPLN D5.002:2024 auto-detect profile active (None + HighGMAC).\n");
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

        if (byte_count == 4 && port_ctx.hdlc.frame_length == 0u) {
            if (byte == ' ' || byte == 'T') {
                fprintf(stderr, "\n[PERINGATAN] Client mengirim request HTTP (bukan DLMS HDLC). Browser Anda sedang membuka port ini!\n");
                const char *http_resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                                        "Ini adalah Smart Meter DLMS HDLC Simulator Port 4059.\r\n"
                                        "Silakan tutup tab browser ini dan sambungkan melalui software GXDLMSDirector.\r\n";
                (void)send(client, http_resp, (int)strlen(http_resp), 0);
                return 0;
            }
        }

        ami_dlms_port_set_time(&port_ctx, (uint32_t)time(NULL));
        AmiDlmsPortStatus st = ami_dlms_port_process_byte(&port_ctx, byte);
        if (st == AMI_DLMS_PORT_TX_ERROR) return 0;
        if (st != AMI_DLMS_PORT_OK && st != AMI_DLMS_PORT_INCOMPLETE) {
            fprintf(stderr, "[DLMS Port Status=%d]\n", (int)st);
            fflush(stderr);
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
