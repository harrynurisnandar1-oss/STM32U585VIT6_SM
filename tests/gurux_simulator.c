#include "ami_cosem_model.h"
#include "ami_dlms_apdu.h"
#include "ami_dlms_association.h"
#include "ami_dlms_hdlc.h"

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
    /* Sprint 1 host profile includes the COSEM Clock object, so make its
     * attribute 2 available to Gurux instead of advertising unavailable data. */
    snapshot->clock_date_time.year[0] = 0x07u;
    snapshot->clock_date_time.year[1] = 0xEAu; /* 2026 */
    snapshot->clock_date_time.month = 10u;
    snapshot->clock_date_time.day = 13u;
    snapshot->clock_date_time.day_of_week = 2u;
    snapshot->clock_date_time.hour = 12u;
    snapshot->clock_date_time.deviation[0] = 0x80u; /* unspecified */
    snapshot->clock_date_time.deviation[1] = 0x00u;
    snapshot->valid_mask = AMI_COSEM_VALID_VOLTAGE_A |
                           AMI_COSEM_VALID_VOLTAGE_B |
                           AMI_COSEM_VALID_VOLTAGE_C |
                           AMI_COSEM_VALID_CURRENT_A |
                           AMI_COSEM_VALID_CURRENT_B |
                           AMI_COSEM_VALID_CURRENT_C |
                           AMI_COSEM_VALID_ACTIVE_IMPORT |
                           AMI_COSEM_VALID_ACTIVE_EXPORT |
                           AMI_COSEM_VALID_CLOCK |
                           AMI_COSEM_VALID_EVENT_LOG |
                           AMI_COSEM_VALID_BILLING_PROFILE;
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
                           const AmiCosemRegisterSnapshot *snapshot,
                           uint8_t *wire_response, size_t wire_capacity,
                           size_t *wire_length)
{
    uint8_t apdu_response[AMI_HDLC_MAX_INFO_LENGTH];
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
        apdu_status = ami_dlms_process_get_request(association, snapshot, apdu,
            apdu_length_in, apdu_response, sizeof(apdu_response), &apdu_length);
        fprintf(stdout, "[APDU GET] status=%d response=%u bytes",
                (int)apdu_status, (unsigned)apdu_length);
        for (size_t i = 0u; i < apdu_length && i < 16u; ++i)
            fprintf(stdout, " %02X", apdu_response[i]);
        fputc('\n', stdout);
        fflush(stdout);
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
        /* ActionRequest -> ActionResponseNormal with read-write-denied (3) */
        apdu_response[0] = 0xC7u; /* ActionResponse */
        apdu_response[1] = 0x01u; /* ActionResponseNormal */
        apdu_response[2] = apdu_length_in > 2u ? apdu[2] : 0u; /* invoke_id */
        apdu_response[3] = 0x01u; /* action-result choice: error */
        apdu_response[4] = 0x03u; /* read-write-denied */
        apdu_length = 5u;
        fprintf(stdout, "[APDU ACTION] Denied (read-only meter)\n");
        fflush(stdout);
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
    make_development_snapshot(&snapshot);
    fprintf(stdout, "Gurux client connected; development no-auth profile active.\n");

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
#endif
            perror("recv");
            return 0;
        }

        static unsigned byte_count = 0;
        if (byte_count++ == 0) {
            fprintf(stdout, "[First byte received from Gurux: 0x%02X]\n", byte);
            fflush(stdout);
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
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(server, 1) != 0) {
        fprintf(stderr, "Could not bind/listen on 127.0.0.1:%d.\n", SERVER_PORT);
        CLOSE_SOCKET(server);
        return 1;
    }
    fprintf(stdout, "Listening for raw HDLC-over-TCP on 127.0.0.1:%d\n", SERVER_PORT);

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
        (void)serve_client(client);
        CLOSE_SOCKET(client);
        fprintf(stdout, "Client disconnected.\n");
    }
}
