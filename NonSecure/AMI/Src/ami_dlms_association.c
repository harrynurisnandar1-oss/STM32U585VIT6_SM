#include "ami_dlms_association.h"

#include <string.h>

#define AARQ_TAG 0x60u
#define AARE_TAG 0x61u
#define BER_OID_TAG 0x06u
#define BER_APP_CONTEXT_TAG 0xA1u

static const uint8_t logical_name_context_oid[] = {
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u
};

typedef struct {
    const uint8_t *value;
    size_t length;
    size_t encoded_length;
} BerTlv;

static int read_ber_length(const uint8_t *bytes, size_t available,
                           size_t *length, size_t *length_octets)
{
    if (bytes == NULL || length == NULL || length_octets == NULL || available == 0u) {
        return 0;
    }

    uint8_t first = bytes[0];
    if ((first & 0x80u) == 0u) {
        *length = first;
        *length_octets = 1u;
        return 1;
    }

    size_t count = first & 0x7fu;
    if (count == 0u || count > sizeof(size_t) || count > available - 1u ||
        bytes[1] == 0u) {
        return 0; /* Indefinite, oversized, or non-canonical BER length. */
    }

    size_t value = 0u;
    for (size_t i = 0u; i < count; ++i) {
        if (value > (SIZE_MAX >> 8)) {
            return 0;
        }
        value = (value << 8) | bytes[1u + i];
    }
    if (value < 128u) {
        return 0; /* Long form is only valid for values >= 128. */
    }

    *length = value;
    *length_octets = count + 1u;
    return 1;
}

static int read_ber_tlv(const uint8_t *bytes, size_t available, BerTlv *tlv)
{
    if (bytes == NULL || tlv == NULL || available < 2u) {
        return 0;
    }

    size_t value_length;
    size_t length_octets;
    if (!read_ber_length(bytes + 1u, available - 1u,
                         &value_length, &length_octets) ||
        value_length > available - 1u - length_octets) {
        return 0;
    }
    tlv->value = bytes + 1u + length_octets;
    tlv->length = value_length;
    tlv->encoded_length = 1u + length_octets + value_length;
    return 1;
}

static int aarq_is_supported_no_auth(const uint8_t *aarq, size_t length)
{
    BerTlv outer;
    if (aarq == NULL || length < 2u || aarq[0] != AARQ_TAG ||
        !read_ber_tlv(aarq, length, &outer) || outer.encoded_length != length) {
        return 0;
    }

    int context_seen = 0;
    size_t offset = 0u;
    while (offset < outer.length) {
        BerTlv field;
        if (!read_ber_tlv(outer.value + offset, outer.length - offset, &field)) {
            return 0;
        }
        uint8_t tag = outer.value[offset];
        if (tag == BER_APP_CONTEXT_TAG) {
            BerTlv oid;
            if (context_seen || !read_ber_tlv(field.value, field.length, &oid) ||
                oid.encoded_length != field.length || field.value[0] != BER_OID_TAG ||
                oid.length != sizeof(logical_name_context_oid) ||
                memcmp(oid.value, logical_name_context_oid, oid.length) != 0) {
                return 0;
            }
            context_seen = 1;
        } else if (tag == 0x8Bu || tag == 0xACu || tag == 0x8Au) {
            /* Authentication mechanism, credentials, or ACSE auth requirements. */
            return 0;
        } else if (tag == 0xBEu) {
            /* Sprint 1 permits only the A-XDR InitiateRequest carried as an
             * octet-aligned EXTERNAL; the initiate parameters are not negotiated yet. */
            BerTlv user_information;
            if (field.length < 2u || field.value[0] != 0x04u ||
                !read_ber_tlv(field.value, field.length, &user_information) ||
                user_information.encoded_length != field.length ||
                user_information.length == 0u) {
                return 0;
            }
        } else if (tag != 0x80u && tag != 0xA2u && tag != 0xA3u &&
                   tag != 0xA4u && tag != 0xA5u && tag != 0xBDu &&
                   tag != 0xBEu) {
            return 0;
        }
        offset += field.encoded_length;
    }
    return context_seen;
}

static int build_aare(uint8_t *out, size_t capacity, size_t *out_length,
                      uint8_t result, uint8_t diagnostic)
{
    /* AARE: application-context-name, associate-result, and source diagnostic. */
    static const uint8_t context_field[] = {
        BER_APP_CONTEXT_TAG, 0x09u, BER_OID_TAG, 0x07u,
        0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u
    };
    const uint8_t result_field[] = {0xA2u, 0x03u, 0x02u, 0x01u, result};
    const uint8_t diagnostic_field[] = {
        0xA3u, 0x05u, 0xA1u, 0x03u, 0x02u, 0x01u, diagnostic
    };
    const size_t body_length = sizeof(context_field) + sizeof(result_field) +
                               sizeof(diagnostic_field);
    if (out == NULL || out_length == NULL || capacity < body_length + 2u) {
        return 0;
    }
    out[0] = AARE_TAG;
    out[1] = (uint8_t)body_length;
    memcpy(out + 2u, context_field, sizeof(context_field));
    memcpy(out + 2u + sizeof(context_field), result_field, sizeof(result_field));
    memcpy(out + 2u + sizeof(context_field) + sizeof(result_field),
           diagnostic_field, sizeof(diagnostic_field));
    *out_length = body_length + 2u;
    return 1;
}

void ami_dlms_association_reset(AmiDlmsAssociation *association)
{
    if (association != NULL) {
        association->state = AMI_DLMS_ASSOC_IDLE;
    }
}

AmiDlmsAssociationStatus ami_dlms_process_aarq(
    AmiDlmsAssociation *association,
    const uint8_t *aarq,
    size_t aarq_length,
    uint8_t *aare,
    size_t aare_capacity,
    size_t *aare_length)
{
    if (association == NULL || aarq == NULL || aare == NULL || aare_length == NULL) {
        return AMI_DLMS_ASSOC_INVALID_ARGUMENT;
    }
    *aare_length = 0u;
    if (association->state != AMI_DLMS_ASSOC_IDLE) {
        association->state = AMI_DLMS_ASSOC_ERROR;
        return AMI_DLMS_ASSOC_REJECTED;
    }

    int accepted = aarq_is_supported_no_auth(aarq, aarq_length);
    uint8_t result = accepted ? 0u : 1u; /* accepted / rejected-permanent */
    uint8_t diagnostic = accepted ? 0u : 2u; /* no common application context */
    if (!build_aare(aare, aare_capacity, aare_length, result, diagnostic)) {
        association->state = AMI_DLMS_ASSOC_ERROR;
        return AMI_DLMS_ASSOC_BUFFER_TOO_SMALL;
    }

    association->state = accepted ? AMI_DLMS_ASSOC_ASSOCIATED : AMI_DLMS_ASSOC_ERROR;
    return accepted ? AMI_DLMS_ASSOC_OK : AMI_DLMS_ASSOC_REJECTED;
}
