#include "ami_dlms_association.h"

#include <string.h>

#define AARQ_TAG 0x60u
#define AARE_TAG 0x61u
#define BER_OID_TAG 0x06u
#define BER_APP_CONTEXT_TAG 0xA1u
#define BER_CALLING_AUTH_VALUE_TAG 0xACu
#define BER_RESPONDING_AUTH_VALUE_TAG 0xAAu

static const uint8_t logical_name_context_oid[] =
    {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u};
static const uint8_t logical_name_ciphering_context_oid[] =
    {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x03u};
#if defined(STM32U585xx) || defined(USE_HAL_DRIVER)
static const uint8_t hls5_gmac_mechanism_oid[] =
    {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u};
#endif
#if !defined(STM32U585xx) && !defined(USE_HAL_DRIVER)
static const uint8_t lls_mechanism_oid[] =
    {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x01u};
#endif

typedef struct {
    const uint8_t *value;
    size_t length;
    size_t encoded_length;
} BerTlv;

typedef struct {
    int context_seen;
    int ciphering_requested;
    int acse_auth_seen;
    const uint8_t *mechanism_oid;
    size_t mechanism_oid_length;
    const uint8_t *auth_value;
    size_t auth_value_length;
    uint8_t client_system_title[AMI_DLMS_SYSTEM_TITLE_MAX];
    size_t client_system_title_length;
    uint16_t client_max_pdu_size;
    int qos_present;
    uint8_t proposed_qos;
    int dedicated_key_present;
    uint8_t dedicated_key[AMI_DLMS_DEDICATED_KEY_MAX];
    int conformance_present;
    uint8_t proposed_conformance[3];
    const uint8_t *user_info_pdu;
    size_t user_info_pdu_length;
    int glo_initiate_present;
} ParsedAarq;

static int read_ber_length(const uint8_t *bytes, size_t available,
                           size_t *length, size_t *length_octets)
{
    if (bytes == NULL || length == NULL || length_octets == NULL || available == 0u) return 0;
    uint8_t first = bytes[0];
    if ((first & 0x80u) == 0u) {
        *length = first;
        *length_octets = 1u;
        return 1;
    }
    size_t count = first & 0x7fu;
    if (count == 0u || count > sizeof(size_t) || count > available - 1u || bytes[1] == 0u) return 0;
    size_t value = 0u;
    for (size_t i = 0u; i < count; ++i) {
        if (value > (SIZE_MAX >> 8)) return 0;
        value = (value << 8) | bytes[1u + i];
    }
    if (value < 128u) return 0;
    *length = value;
    *length_octets = count + 1u;
    return 1;
}

static int read_tlv(const uint8_t *bytes, size_t available, BerTlv *tlv)
{
    size_t length, length_octets;
    if (bytes == NULL || tlv == NULL || available < 2u ||
        !read_ber_length(bytes + 1u, available - 1u, &length, &length_octets) ||
        length > available - 1u - length_octets) return 0;
    tlv->value = bytes + 1u + length_octets;
    tlv->length = length;
    tlv->encoded_length = 1u + length_octets + length;
    return 1;
}

static int parse_auth_value(const uint8_t *bytes, size_t length,
                            const uint8_t **value, size_t *value_length)
{
    BerTlv inner;
    if (!read_tlv(bytes, length, &inner) || inner.encoded_length != length ||
        bytes[0] != 0x80u || inner.length == 0u ||
        inner.length > AMI_DLMS_AUTH_VALUE_MAX) return 0;
    *value = inner.value;
    *value_length = inner.length;
    return 1;
}

static int parse_axdr_length(const uint8_t *bytes, size_t available,
                             size_t *length, size_t *length_octets)
{
    if (bytes == NULL || length == NULL || length_octets == NULL || available == 0u) return 0;
    uint8_t first = bytes[0];
    if ((first & 0x80u) == 0u) {
        *length = first;
        *length_octets = 1u;
        return 1;
    }
    size_t count = first & 0x7fu;
    if (count == 0u || count > 2u || count > available - 1u) return 0;
    size_t value = 0u;
    for (size_t i = 0u; i < count; ++i) {
        value = (value << 8) | bytes[1u + i];
    }
    *length = value;
    *length_octets = count + 1u;
    return 1;
}

static size_t write_axdr_length(uint8_t *out, size_t length)
{
    if (length < 128u) {
        out[0] = (uint8_t)length;
        return 1u;
    }
    if (length <= 0xFFu) {
        out[0] = 0x81u;
        out[1] = (uint8_t)length;
        return 2u;
    }
    out[0] = 0x82u;
    out[1] = (uint8_t)(length >> 8);
    out[2] = (uint8_t)length;
    return 3u;
}

static int parse_initiate_request(const uint8_t *pdu, size_t pdu_len, ParsedAarq *parsed)
{
    if (pdu == NULL || parsed == NULL || pdu_len < 8u || pdu[0] != 0x01u) return 0;
    size_t ppos = 1u;
    if (ppos < pdu_len && pdu[ppos] == 0x00u) {
        ppos++; /* dedicated_key absent */
    } else if (ppos < pdu_len && pdu[ppos] == 0x01u) {
        ppos++;
        parsed->dedicated_key_present = 1;
        if (ppos < pdu_len) {
            size_t klen = pdu[ppos++];
            if (ppos + klen > pdu_len) return 0;
            if (klen == AMI_DLMS_DEDICATED_KEY_MAX) {
                memcpy(parsed->dedicated_key, pdu + ppos, klen);
            }
            ppos += klen;
        }
    }
    if (ppos < pdu_len && pdu[ppos] == 0x00u) ppos++; /* response_allowed absent */
    else if (ppos + 1u < pdu_len && pdu[ppos] == 0x01u) ppos += 2u; /* response_allowed present */

    if (ppos < pdu_len && pdu[ppos] == 0x00u) {
        ppos++; /* qos absent */
        parsed->qos_present = 0;
    } else if (ppos + 1u < pdu_len && pdu[ppos] == 0x01u) {
        parsed->qos_present = 1;
        parsed->proposed_qos = pdu[ppos + 1u];
        ppos += 2u;
    }
    if (ppos < pdu_len) ppos++; /* dlms_version */
    if (ppos + 2u < pdu_len && pdu[ppos] == 0x5Fu && pdu[ppos + 1u] == 0x1Fu) {
        ppos += 2u;
        if (ppos < pdu_len) {
            size_t clen = pdu[ppos++];
            if (clen >= 1u && ppos + clen <= pdu_len) {
                parsed->conformance_present = 1;
                parsed->proposed_conformance[0] = (clen > 1u) ? pdu[ppos + 1u] : 0u;
                parsed->proposed_conformance[1] = (clen > 2u) ? pdu[ppos + 2u] : 0u;
                parsed->proposed_conformance[2] = (clen > 3u) ? pdu[ppos + 3u] : 0u;
            }
            ppos += clen;
        }
    }
    if (ppos + 1u < pdu_len) {
        parsed->client_max_pdu_size = (uint16_t)(((uint16_t)pdu[ppos] << 8) | pdu[ppos + 1u]);
    }
    return 1;
}

static int parse_aarq(const uint8_t *aarq, size_t length, ParsedAarq *parsed)
{
    BerTlv outer;
    if (aarq == NULL || parsed == NULL || length < 2u || aarq[0] != AARQ_TAG ||
        !read_tlv(aarq, length, &outer) || outer.encoded_length != length) return 0;
    memset(parsed, 0, sizeof(*parsed));
    int mechanism_seen = 0, auth_value_seen = 0;
    size_t offset = 0u;
    while (offset < outer.length) {
        BerTlv field;
        if (!read_tlv(outer.value + offset, outer.length - offset, &field)) return 0;
        const uint8_t tag = outer.value[offset];
        if (tag == BER_APP_CONTEXT_TAG) {
            BerTlv oid;
            if (parsed->context_seen || !read_tlv(field.value, field.length, &oid) ||
                oid.encoded_length != field.length || field.value[0] != BER_OID_TAG) return 0;
            if (oid.length == sizeof(logical_name_context_oid) &&
                memcmp(oid.value, logical_name_context_oid, oid.length) == 0) {
                parsed->ciphering_requested = 0;
            } else if (oid.length == sizeof(logical_name_ciphering_context_oid) &&
                       memcmp(oid.value, logical_name_ciphering_context_oid, oid.length) == 0) {
                parsed->ciphering_requested = 1;
            } else {
                return 0;
            }
            parsed->context_seen = 1;
        } else if (tag == 0xA6u) {
            /* calling-AP-title: OCTET STRING wrapping Client System Title (up to 8 bytes) */
            BerTlv ap_title;
            if (read_tlv(field.value, field.length, &ap_title) &&
                field.value[0] == 0x04u && ap_title.length > 0u &&
                ap_title.length <= AMI_DLMS_SYSTEM_TITLE_MAX) {
                memcpy(parsed->client_system_title, ap_title.value, ap_title.length);
                parsed->client_system_title_length = ap_title.length;
            }
        } else if (tag == 0x8Au) {
            if (parsed->acse_auth_seen || field.length != 2u ||
                field.value[0] != 0x07u || field.value[1] != 0x80u) return 0;
            parsed->acse_auth_seen = 1;
        } else if (tag == 0x8Bu) {
            if (mechanism_seen || field.length == 0u || field.length > AMI_DLMS_OID_MAX) return 0;
            mechanism_seen = 1;
            parsed->mechanism_oid = field.value;
            parsed->mechanism_oid_length = field.length;
        } else if (tag == BER_CALLING_AUTH_VALUE_TAG) {
            if (auth_value_seen || !parse_auth_value(field.value, field.length,
                    &parsed->auth_value, &parsed->auth_value_length)) return 0;
            auth_value_seen = 1;
        } else if (tag == 0xBEu) {
            BerTlv user_information;
            size_t available_ui = outer.length - offset > 2u ? outer.length - offset - 2u : field.length;
            const uint8_t *ui_bytes = outer.value + offset + 2u;
            if (available_ui < 2u || ui_bytes[0] != 0x04u ||
                !read_tlv(ui_bytes, available_ui, &user_information) ||
                user_information.length == 0u) return 0;
            field.encoded_length = 2u + user_information.encoded_length;
            const uint8_t *pdu = user_information.value;
            size_t pdu_len = user_information.length;
            parsed->user_info_pdu = pdu;
            parsed->user_info_pdu_length = pdu_len;
            if (pdu[0] == 0x21u) {
                parsed->glo_initiate_present = 1;
            } else if (pdu[0] == 0x01u) {
                (void)parse_initiate_request(pdu, pdu_len, parsed);
            }
        } else if (tag != 0x80u && tag != 0xA2u && tag != 0xA3u &&
                   tag != 0xA4u && tag != 0xA5u && tag != 0xBDu) {
            /* Unknown optional ACSE tags are tolerated in standard BER decoding */
        }
        offset += field.encoded_length;
    }
    const int is_none_mech = (mechanism_seen && parsed->mechanism_oid_length == 7u &&
        parsed->mechanism_oid[0] == 0x60u && parsed->mechanism_oid[1] == 0x85u &&
        parsed->mechanism_oid[2] == 0x74u && parsed->mechanism_oid[3] == 0x05u &&
        parsed->mechanism_oid[4] == 0x08u && parsed->mechanism_oid[5] == 0x02u &&
        parsed->mechanism_oid[6] == 0x00u);
    return parsed->context_seen &&
        (parsed->acse_auth_seen == mechanism_seen) &&
        (is_none_mech ? !auth_value_seen : (mechanism_seen == auth_value_seen));
}

static int build_aare(uint8_t *out, size_t capacity, size_t *out_length,
                      const AmiDlmsAssociationSecurity *security,
                      int ciphering_enabled,
                      uint8_t result, uint8_t diagnostic,
                      const uint8_t *mechanism_oid, size_t mechanism_oid_length,
                      const uint8_t *server_challenge, size_t challenge_length,
                      uint16_t client_max_pdu_size,
                      int qos_present, uint8_t proposed_qos,
                      int conformance_present, const uint8_t *proposed_conformance)
{
    const uint8_t context_field[] = {
        BER_APP_CONTEXT_TAG, 0x09u, BER_OID_TAG, 0x07u,
        0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u,
        ciphering_enabled ? 0x03u : 0x01u
    };
    const uint8_t result_field[] = {0xA2u, 0x03u, 0x02u, 0x01u, result};
    const uint8_t diagnostic_field[] = {0xA3u, 0x05u, 0xA1u, 0x03u, 0x02u, 0x01u, diagnostic};

    /* xDLMS InitiateResponse */
    uint16_t negotiated_pdu = (client_max_pdu_size > 0u && client_max_pdu_size < 500u)
        ? client_max_pdu_size : 500u;
    uint8_t conf0 = 0x00u;
    uint8_t conf1 = 0x00u;
    uint8_t conf2 = ciphering_enabled ? 0x1Du : 0x14u;
    if (conformance_present && proposed_conformance != NULL) {
        const uint8_t allowed_conf2 = ciphering_enabled ? 0x1Du : 0x14u;
        conf0 = (uint8_t)(proposed_conformance[0] & 0x00u);
        conf1 = (uint8_t)(proposed_conformance[1] & 0x50u);
        conf2 = (uint8_t)(proposed_conformance[2] & allowed_conf2);
    }

    uint8_t plain_init[16];
    size_t plain_init_len = 0u;
    plain_init[plain_init_len++] = 0x08u; /* InitiateResponse tag */
    if (qos_present) {
        plain_init[plain_init_len++] = 0x01u;
        plain_init[plain_init_len++] = proposed_qos;
    } else {
        plain_init[plain_init_len++] = 0x00u;
    }
    plain_init[plain_init_len++] = 0x06u; /* dlms version 6 */
    plain_init[plain_init_len++] = 0x5Fu;
    plain_init[plain_init_len++] = 0x1Fu;
    plain_init[plain_init_len++] = 0x04u;
    plain_init[plain_init_len++] = 0x00u;
    plain_init[plain_init_len++] = conf0;
    plain_init[plain_init_len++] = conf1;
    plain_init[plain_init_len++] = conf2;
    plain_init[plain_init_len++] = (uint8_t)(negotiated_pdu >> 8);
    plain_init[plain_init_len++] = (uint8_t)negotiated_pdu;
    plain_init[plain_init_len++] = 0x00u; /* vaa name */
    plain_init[plain_init_len++] = 0x07u;

    uint8_t initiate_response_field[64];
    size_t init_len = 0u;
    if (result == 0u) {
        uint8_t inner_ui[48];
        size_t inner_ui_len = 0u;
        if (ciphering_enabled && security != NULL && security->encrypt_initiate != NULL) {
            uint32_t tx_ic = 0u;
            uint8_t ct[24];
            size_t ct_len = 0u;
            uint8_t tag[12];
            if (security->encrypt_initiate(security->user, 0x30u, &tx_ic,
                    plain_init, plain_init_len, ct, sizeof(ct), &ct_len, tag, sizeof(tag)) != 1 ||
                ct_len != plain_init_len) {
                return 0;
            }
            const size_t glo_payload_len = 5u + ct_len + sizeof(tag);
            inner_ui[inner_ui_len++] = 0x28u; /* glo_initiateResponse */
            inner_ui_len += write_axdr_length(inner_ui + inner_ui_len, glo_payload_len);
            inner_ui[inner_ui_len++] = 0x30u; /* SC = Auth + Encrypt, Suite 0 */
            inner_ui[inner_ui_len++] = (uint8_t)(tx_ic >> 24);
            inner_ui[inner_ui_len++] = (uint8_t)(tx_ic >> 16);
            inner_ui[inner_ui_len++] = (uint8_t)(tx_ic >> 8);
            inner_ui[inner_ui_len++] = (uint8_t)tx_ic;
            memcpy(inner_ui + inner_ui_len, ct, ct_len); inner_ui_len += ct_len;
            memcpy(inner_ui + inner_ui_len, tag, sizeof(tag)); inner_ui_len += sizeof(tag);
        } else {
            memcpy(inner_ui, plain_init, plain_init_len);
            inner_ui_len = plain_init_len;
        }
        initiate_response_field[init_len++] = 0xBEu;
        initiate_response_field[init_len++] = (uint8_t)(2u + inner_ui_len);
        initiate_response_field[init_len++] = 0x04u;
        initiate_response_field[init_len++] = (uint8_t)inner_ui_len;
        memcpy(initiate_response_field + init_len, inner_ui, inner_ui_len);
        init_len += inner_ui_len;
    }

    const int include_ap_title = (ciphering_enabled && security != NULL &&
        security->server_system_title != NULL &&
        security->server_system_title_length > 0u &&
        security->server_system_title_length <= AMI_DLMS_SYSTEM_TITLE_MAX);
    const size_t ap_title_extra = include_ap_title
        ? (4u + security->server_system_title_length) : 0u;
    const size_t auth_extra = challenge_length == 0u ? 0u : 4u + challenge_length;
    const size_t mechanism_extra = challenge_length == 0u ? 0u : 2u + mechanism_oid_length;
    const size_t acse_extra = challenge_length == 0u ? 0u : 4u;
    const size_t user_info_extra = result == 0u ? init_len : 0u;
    const size_t body_length = sizeof(context_field) + sizeof(result_field) +
        sizeof(diagnostic_field) + ap_title_extra + auth_extra +
        mechanism_extra + acse_extra + user_info_extra;
    const size_t header_len = (body_length < 128u) ? 2u : 3u;
    if (out == NULL || out_length == NULL || body_length > 255u ||
        capacity < body_length + header_len ||
        (challenge_length != 0u && (server_challenge == NULL || mechanism_oid == NULL ||
          mechanism_oid_length == 0u || mechanism_oid_length > AMI_DLMS_OID_MAX))) return 0;
    size_t n = 0u;
    out[n++] = AARE_TAG;
    n += write_axdr_length(out + n, body_length);
    memcpy(out + n, context_field, sizeof(context_field)); n += sizeof(context_field);
    memcpy(out + n, result_field, sizeof(result_field)); n += sizeof(result_field);
    memcpy(out + n, diagnostic_field, sizeof(diagnostic_field)); n += sizeof(diagnostic_field);
    if (include_ap_title) {
        out[n++] = 0xA4u;
        out[n++] = (uint8_t)(2u + security->server_system_title_length);
        out[n++] = 0x04u;
        out[n++] = (uint8_t)security->server_system_title_length;
        memcpy(out + n, security->server_system_title, security->server_system_title_length);
        n += security->server_system_title_length;
    }
    if (challenge_length != 0u) {
        out[n++] = 0x88u; out[n++] = 0x02u; out[n++] = 0x07u; out[n++] = 0x80u;
        out[n++] = 0x89u; out[n++] = (uint8_t)mechanism_oid_length;
        memcpy(out + n, mechanism_oid, mechanism_oid_length); n += mechanism_oid_length;
        out[n++] = BER_RESPONDING_AUTH_VALUE_TAG;
        out[n++] = (uint8_t)(challenge_length + 2u);
        out[n++] = 0x80u; out[n++] = (uint8_t)challenge_length;
        memcpy(out + n, server_challenge, challenge_length); n += challenge_length;
    }
    if (result == 0u) {
        memcpy(out + n, initiate_response_field, init_len);
        n += init_len;
    }
    *out_length = n;
    return 1;
}

static int oid_equal(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
    return a != NULL && b != NULL && a_len == b_len && memcmp(a, b, a_len) == 0;
}

static int hls_profile_supported(const uint8_t *oid, size_t oid_length)
{
#if defined(STM32U585xx) || defined(USE_HAL_DRIVER)
    return oid_equal(oid, oid_length, hls5_gmac_mechanism_oid,
                     sizeof(hls5_gmac_mechanism_oid));
#else
    (void)oid;
    (void)oid_length;
    return 1;
#endif
}

static int constant_time_equal(const uint8_t *a, const uint8_t *b, size_t length)
{
    uint8_t diff = 0u;
    for (size_t i = 0u; i < length; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0u;
}

static void clear_challenges(AmiDlmsAssociation *association)
{
    memset(association->client_challenge, 0, sizeof(association->client_challenge));
    memset(association->server_challenge, 0, sizeof(association->server_challenge));
    association->client_challenge_length = 0u;
    association->server_challenge_length = 0u;
    memset(association->hls_mechanism_oid, 0, sizeof(association->hls_mechanism_oid));
    association->hls_mechanism_oid_length = 0u;
}

void ami_dlms_association_reset(AmiDlmsAssociation *association)
{
    if (association != NULL) {
        clear_challenges(association);
        association->mechanism = AMI_DLMS_AUTH_NONE;
        association->ciphering_enabled = 0u;
        association->dedicated_key_present = 0u;
        memset(association->dedicated_key, 0, sizeof(association->dedicated_key));
        association->client_sap = 0u;
        association->server_sap = 0u;
        memset(association->client_system_title, 0, sizeof(association->client_system_title));
        association->client_system_title_length = 0u;
        association->state = AMI_DLMS_ASSOC_IDLE;
    }
}

AmiDlmsAssociationStatus ami_dlms_process_aarq_with_security(
    AmiDlmsAssociation *association, const AmiDlmsAssociationSecurity *security,
    const uint8_t *aarq, size_t aarq_length, uint8_t *aare,
    size_t aare_capacity, size_t *aare_length)
{
    if (association == NULL || security == NULL || aarq == NULL || aare == NULL || aare_length == NULL)
        return AMI_DLMS_ASSOC_INVALID_ARGUMENT;
    *aare_length = 0u;
    if (association->state != AMI_DLMS_ASSOC_IDLE) {
        /* An unsolicited AARQ must not tear down an authenticated session. */
        return AMI_DLMS_ASSOC_REJECTED;
    }
    ParsedAarq parsed;
    int parsed_ok = parse_aarq(aarq, aarq_length, &parsed);
    if (parsed_ok && parsed.glo_initiate_present) {
        size_t glo_len = 0u, len_octets = 0u;
        if (!parsed.ciphering_requested || security->allow_ciphering == 0u ||
            security->decrypt_initiate == NULL ||
            !parse_axdr_length(parsed.user_info_pdu + 1u, parsed.user_info_pdu_length - 1u,
                               &glo_len, &len_octets) ||
            1u + len_octets + glo_len != parsed.user_info_pdu_length ||
            glo_len <= 17u) {
            parsed_ok = 0;
        } else {
            const uint8_t *glo_body = parsed.user_info_pdu + 1u + len_octets;
            const uint8_t sc = glo_body[0];
            const uint32_t ic = ((uint32_t)glo_body[1] << 24) | ((uint32_t)glo_body[2] << 16) |
                                ((uint32_t)glo_body[3] << 8) | (uint32_t)glo_body[4];
            const size_t ct_len = glo_len - 5u - 12u;
            const uint8_t *ct = glo_body + 5u;
            const uint8_t *tag = ct + ct_len;
            uint8_t plain_init[64];
            size_t plain_init_len = 0u;
            if (security->decrypt_initiate(security->user,
                    parsed.client_system_title, parsed.client_system_title_length,
                    sc, ic, ct, ct_len, tag, 12u,
                    plain_init, sizeof(plain_init), &plain_init_len) != 1 ||
                !parse_initiate_request(plain_init, plain_init_len, &parsed)) {
                parsed_ok = 0;
            }
            memset(plain_init, 0, sizeof(plain_init));
        }
    }

    AmiDlmsAuthMechanism mechanism = AMI_DLMS_AUTH_NONE;
    int accepted = 0;
    uint8_t server_challenge[AMI_DLMS_HLS_CHALLENGE_LENGTH] = {0};
    size_t server_challenge_length = 0u;
    const uint8_t allowed_conf2 = parsed.ciphering_requested ? 0x1Du : 0x14u;
    if (parsed_ok && parsed.ciphering_requested && security->allow_ciphering == 0u) {
        accepted = 0;
    } else if (parsed_ok && parsed.conformance_present &&
        (((parsed.proposed_conformance[0] & 0x00u) |
          (parsed.proposed_conformance[1] & 0x50u) |
          (parsed.proposed_conformance[2] & allowed_conf2)) == 0u)) {
        /* Empty/incompatible conformance: no common services, reject association */
        accepted = 0;
    } else if (parsed_ok && parsed.client_max_pdu_size != 0u && parsed.client_max_pdu_size < 12u) {
        /* T_APPL_OPEN_14: client max receive PDU size below the 12-byte minimum is rejected */
        accepted = 0;
    } else if (parsed_ok && !parsed.ciphering_requested && parsed.dedicated_key_present) {
        /* DLMS CTT T_APPL_OPEN_9: dedicated key when ciphering is not used must be rejected */
        accepted = 0;
    }
#if !defined(STM32U585xx) && !defined(USE_HAL_DRIVER)
    else if (parsed_ok &&
             (parsed.mechanism_oid == NULL ||
              (parsed.mechanism_oid_length == 7u &&
               parsed.mechanism_oid[0] == 0x60u && parsed.mechanism_oid[1] == 0x85u &&
               parsed.mechanism_oid[2] == 0x74u && parsed.mechanism_oid[3] == 0x05u &&
               parsed.mechanism_oid[4] == 0x08u && parsed.mechanism_oid[5] == 0x02u &&
               parsed.mechanism_oid[6] == 0x00u)) &&
             security->allow_no_auth != 0u) {
        accepted = 1;
    } else if (parsed_ok && !parsed.ciphering_requested &&
               oid_equal(parsed.mechanism_oid, parsed.mechanism_oid_length,
                         lls_mechanism_oid, sizeof(lls_mechanism_oid)) &&
               security->verify_lls != NULL) {
        mechanism = AMI_DLMS_AUTH_LLS;
        accepted = security->verify_lls(security->user, parsed.auth_value,
                                        parsed.auth_value_length) == 1;
    }
#endif
    else if (parsed_ok && security->hls_mechanism_oid != NULL &&
               security->hls_mechanism_oid_length > 0u &&
               security->hls_mechanism_oid_length <= AMI_DLMS_OID_MAX &&
               hls_profile_supported(security->hls_mechanism_oid,
                                     security->hls_mechanism_oid_length) &&
#if defined(STM32U585xx) || defined(USE_HAL_DRIVER)
               parsed.auth_value_length == AMI_DLMS_HLS_CHALLENGE_LENGTH &&
#endif
               security->random != NULL && security->hls_proof != NULL &&
               oid_equal(parsed.mechanism_oid, parsed.mechanism_oid_length,
                         security->hls_mechanism_oid, security->hls_mechanism_oid_length)) {
        mechanism = AMI_DLMS_AUTH_HLS;
        server_challenge_length = sizeof(server_challenge);
        accepted = security->random(security->user, server_challenge,
                                    server_challenge_length) == 1;
        if (accepted) {
            memcpy(association->client_challenge, parsed.auth_value, parsed.auth_value_length);
            association->client_challenge_length = parsed.auth_value_length;
            memcpy(association->server_challenge, server_challenge, server_challenge_length);
            association->server_challenge_length = server_challenge_length;
            memcpy(association->hls_mechanism_oid, security->hls_mechanism_oid,
                   security->hls_mechanism_oid_length);
            association->hls_mechanism_oid_length = security->hls_mechanism_oid_length;
        }
    }
    const int hls_started = accepted && mechanism == AMI_DLMS_AUTH_HLS;
    const uint8_t result = (uint8_t)(accepted ? 0u : 1u);
    const uint8_t diagnostic = (uint8_t)(accepted ? 0u : (parsed.context_seen ? 13u : 2u));
    if (!build_aare(aare, aare_capacity, aare_length, security,
                    parsed_ok ? parsed.ciphering_requested : 0,
                    result, diagnostic,
                    hls_started ? association->hls_mechanism_oid : NULL,
                    hls_started ? association->hls_mechanism_oid_length : 0u,
                    hls_started ? association->server_challenge : NULL,
                    hls_started ? association->server_challenge_length : 0u,
                    parsed.client_max_pdu_size,
                    parsed.qos_present, parsed.proposed_qos,
                    parsed.conformance_present, parsed.proposed_conformance)) {
        clear_challenges(association);
        association->state = AMI_DLMS_ASSOC_ERROR;
        return AMI_DLMS_ASSOC_BUFFER_TOO_SMALL;
    }
    if (accepted) {
        association->mechanism = mechanism;
        association->ciphering_enabled = (uint8_t)(parsed.ciphering_requested ? 1u : 0u);
        association->dedicated_key_present = (uint8_t)(parsed.dedicated_key_present ? 1u : 0u);
        if (parsed.dedicated_key_present) {
            memcpy(association->dedicated_key, parsed.dedicated_key, sizeof(association->dedicated_key));
        } else {
            memset(association->dedicated_key, 0, sizeof(association->dedicated_key));
        }
        association->client_system_title_length = parsed.client_system_title_length;
        if (parsed.client_system_title_length > 0u) {
            memcpy(association->client_system_title, parsed.client_system_title,
                   parsed.client_system_title_length);
        }
        association->state = hls_started ? AMI_DLMS_ASSOC_AUTH_PENDING : AMI_DLMS_ASSOC_ASSOCIATED;
        return AMI_DLMS_ASSOC_OK;
    }
    clear_challenges(association);
    association->state = AMI_DLMS_ASSOC_ERROR;
    return AMI_DLMS_ASSOC_REJECTED;
}

AmiDlmsAssociationStatus ami_dlms_process_hls_reply(
    AmiDlmsAssociation *association, const AmiDlmsAssociationSecurity *security,
    const uint8_t *client_proof, size_t client_proof_length,
    uint8_t *server_proof, size_t server_proof_capacity, size_t *server_proof_length)
{
    if (server_proof_length != NULL) *server_proof_length = 0u;
    if (association == NULL || security == NULL || client_proof == NULL ||
        server_proof == NULL || server_proof_length == NULL) return AMI_DLMS_ASSOC_INVALID_ARGUMENT;
    if (association->state != AMI_DLMS_ASSOC_AUTH_PENDING ||
        association->mechanism != AMI_DLMS_AUTH_HLS || security->hls_proof == NULL)
        return AMI_DLMS_ASSOC_REJECTED;
    int verified = 0;
    if (security->hls_verify != NULL) {
        verified = security->hls_verify(security->user,
            association->hls_mechanism_oid, association->hls_mechanism_oid_length,
            association->client_system_title, association->client_system_title_length,
            association->server_challenge, association->server_challenge_length,
            client_proof, client_proof_length) == 1;
    } else {
        uint8_t expected[AMI_DLMS_AUTH_VALUE_MAX] = {0};
        size_t expected_length = 0u;
        int ok = security->hls_proof(security->user, association->hls_mechanism_oid,
            association->hls_mechanism_oid_length, association->server_challenge,
            association->server_challenge_length, expected, sizeof(expected), &expected_length) == 1;
        verified = (ok && expected_length > 0u && expected_length <= sizeof(expected) &&
            expected_length == client_proof_length &&
            constant_time_equal(expected, client_proof, expected_length));
        memset(expected, 0, sizeof(expected));
    }
    if (!verified) {
        clear_challenges(association);
        association->state = AMI_DLMS_ASSOC_ERROR;
        return AMI_DLMS_ASSOC_REJECTED;
    }
    uint8_t response[AMI_DLMS_AUTH_VALUE_MAX] = {0};
    size_t response_length = 0u;
    int ok = security->hls_proof(security->user, association->hls_mechanism_oid,
        association->hls_mechanism_oid_length, association->client_challenge,
        association->client_challenge_length, response, sizeof(response),
        &response_length) == 1;
    if (!ok || response_length == 0u || response_length > sizeof(response)) {
        memset(response, 0, sizeof(response));
        clear_challenges(association);
        association->state = AMI_DLMS_ASSOC_ERROR;
        return AMI_DLMS_ASSOC_BACKEND_FAILURE;
    }
    if (response_length > server_proof_capacity) {
        memset(response, 0, sizeof(response));
        return AMI_DLMS_ASSOC_BUFFER_TOO_SMALL;
    }
    memcpy(server_proof, response, response_length);
    memset(response, 0, sizeof(response));
    *server_proof_length = response_length;
    clear_challenges(association);
    association->state = AMI_DLMS_ASSOC_ASSOCIATED;
    return AMI_DLMS_ASSOC_OK;
}

#if !defined(STM32U585xx) && !defined(USE_HAL_DRIVER)
AmiDlmsAssociationStatus ami_dlms_process_aarq(
    AmiDlmsAssociation *association, const uint8_t *aarq, size_t aarq_length,
    uint8_t *aare, size_t aare_capacity, size_t *aare_length)
{
    const AmiDlmsAssociationSecurity no_auth = {.allow_no_auth = 1u};
    return ami_dlms_process_aarq_with_security(association, &no_auth, aarq,
        aarq_length, aare, aare_capacity, aare_length);
}
#endif
