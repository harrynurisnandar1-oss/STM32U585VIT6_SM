#include "ami_dlms_apdu.h"

#include <limits.h>
#include <string.h>

#define GET_REQUEST_TAG 0xC0u
#define SET_REQUEST_TAG 0xC1u
#define GET_RESPONSE_TAG 0xC4u
#define SET_RESPONSE_TAG 0xC5u
#define GLO_GET_REQUEST_TAG 0xC8u
#define GLO_SET_REQUEST_TAG 0xC9u
#define GLO_GET_RESPONSE_TAG 0xCCu
#define GLO_SET_RESPONSE_TAG 0xCDu
#define GET_NORMAL_CHOICE 0x01u
#define SECURITY_HEADER_LENGTH 5u /* security-control + invocation-counter */
#define GLO_HEADER_LENGTH 6u /* tag + security header */

static int encode_data(const AmiCosemData *data, uint8_t *out,
                       size_t capacity, size_t *written)
{
    size_t offset = 0u;
    if (data == NULL || out == NULL || written == NULL) {
        return 0;
    }

    switch (data->type) {
    case AMI_COSEM_TYPE_ENCODED:
        if (data->length > capacity || data->length > sizeof(data->value.encoded)) {
            return 0;
        }
        memcpy(out, data->value.encoded, data->length);
        offset = data->length;
        break;
    case AMI_COSEM_TYPE_OCTET_STRING:
        if (data->length > UINT8_MAX || data->length > capacity ||
            capacity - data->length < 2u) {
            return 0;
        }
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_OCTET_STRING;
        out[offset++] = (uint8_t)data->length;
        if (data->length == AMI_COSEM_LOGICAL_NAME_LENGTH) {
            memcpy(out + offset, data->value.logical_name, data->length);
        } else if (data->length == sizeof(AmiCosemDateTime)) {
            memcpy(out + offset, &data->value.date_time, data->length);
        } else {
            return 0;
        }
        offset += data->length;
        break;
    case AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED:
        if (capacity < 5u) return 0;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_DOUBLE_LONG_UNSIGNED;
        out[offset++] = (uint8_t)(data->value.double_long_unsigned >> 24);
        out[offset++] = (uint8_t)(data->value.double_long_unsigned >> 16);
        out[offset++] = (uint8_t)(data->value.double_long_unsigned >> 8);
        out[offset++] = (uint8_t)data->value.double_long_unsigned;
        break;
    case AMI_COSEM_TYPE_LONG64_UNSIGNED:
        if (capacity < 9u) return 0;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_LONG64_UNSIGNED;
        for (int shift = 56; shift >= 0; shift -= 8) {
            out[offset++] = (uint8_t)(data->value.long64_unsigned >> shift);
        }
        break;
    case AMI_COSEM_TYPE_DOUBLE_LONG:
        if (capacity < 5u) return 0;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_DOUBLE_LONG;
        out[offset++] = (uint8_t)((uint32_t)data->value.double_long >> 24);
        out[offset++] = (uint8_t)((uint32_t)data->value.double_long >> 16);
        out[offset++] = (uint8_t)((uint32_t)data->value.double_long >> 8);
        out[offset++] = (uint8_t)data->value.double_long;
        break;
    case AMI_COSEM_TYPE_UNSIGNED:
        if (capacity < 2u) return 0;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_UNSIGNED;
        out[offset++] = data->value.unsigned_integer;
        break;
    case AMI_COSEM_TYPE_STRUCTURE:
        if (capacity < 6u) return 0;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_STRUCTURE;
        out[offset++] = 2u;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_INTEGER;
        out[offset++] = (uint8_t)data->value.scaler_unit.scaler;
        out[offset++] = (uint8_t)AMI_COSEM_TYPE_UNSIGNED;
        out[offset++] = data->value.scaler_unit.unit;
        break;
    default:
        return 0;
    }
    *written = offset;
    return 1;
}

static uint8_t access_error(AmiCosemStatus status)
{
    switch (status) {
    case AMI_COSEM_OBJECT_NOT_FOUND: return 4u; /* object-undefined */
    case AMI_COSEM_ATTRIBUTE_NOT_FOUND: return 3u; /* read-write-denied */
    case AMI_COSEM_VALUE_UNAVAILABLE: return 2u; /* temporary-failure */
    default: return 15u; /* other-reason */
    }
}

static AmiDlmsApduStatus build_error(uint8_t invoke_id, uint8_t error,
                                     uint8_t *response, size_t capacity,
                                     size_t *response_length)
{
    if (response == NULL || response_length == NULL || capacity < 5u) {
        return AMI_DLMS_APDU_BUFFER_TOO_SMALL;
    }
    response[0] = GET_RESPONSE_TAG;
    response[1] = GET_NORMAL_CHOICE;
    response[2] = invoke_id;
    response[3] = 1u; /* data-access-error alternative */
    response[4] = error;
    *response_length = 5u;
    return AMI_DLMS_APDU_ACCESS_ERROR;
}

static AmiDlmsApduStatus process_get_request_plain(
    const AmiDlmsAssociation *association,
    const AmiCosemRegisterSnapshot *snapshot,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    int decrypted_hls_apdu)
{
    if (response_length != NULL) {
        *response_length = 0u;
    }
    if (association == NULL || snapshot == NULL || request == NULL ||
        response == NULL || response_length == NULL) {
        return AMI_DLMS_APDU_INVALID_ARGUMENT;
    }
    if (association->state != AMI_DLMS_ASSOC_ASSOCIATED) {
        return AMI_DLMS_APDU_NOT_ASSOCIATED;
    }
    if (association->mechanism == AMI_DLMS_AUTH_HLS && !decrypted_hls_apdu) {
        return AMI_DLMS_APDU_CIPHER_REQUIRED;
    }
    if (request_length < 3u || request[0] != GET_REQUEST_TAG ||
        request[1] != GET_NORMAL_CHOICE) {
        return build_error(request_length > 2u ? request[2] : 0u, 15u,
                           response, response_capacity, response_length);
    }
    if (request_length != 13u || request[12] != 0u) {
        return build_error(request[2], 15u, response,
                           response_capacity, response_length);
    }

    uint8_t logical_name[AMI_COSEM_LOGICAL_NAME_LENGTH];
    uint16_t class_id = (uint16_t)(((uint16_t)request[3] << 8) | request[4]);
    memcpy(logical_name, request + 5u, sizeof(logical_name));
    uint8_t attribute_id = request[11];
    AmiCosemData value;
    AmiCosemStatus read_status = ami_cosem_register_read(
        snapshot, class_id, logical_name, attribute_id, &value);
    if (read_status != AMI_COSEM_OK) {
        return build_error(request[2], access_error(read_status), response,
                           response_capacity, response_length);
    }

    if (response_capacity < 4u) {
        return AMI_DLMS_APDU_BUFFER_TOO_SMALL;
    }
    response[0] = GET_RESPONSE_TAG;
    response[1] = GET_NORMAL_CHOICE;
    response[2] = request[2];
    response[3] = 0u; /* data alternative */
    size_t encoded_length = 0u;
    if (!encode_data(&value, response + 4u, response_capacity - 4u,
                     &encoded_length)) {
        *response_length = 0u;
        return AMI_DLMS_APDU_BUFFER_TOO_SMALL;
    }
    *response_length = 4u + encoded_length;
    return AMI_DLMS_APDU_OK;
}

AmiDlmsApduStatus ami_dlms_process_get_request(
    const AmiDlmsAssociation *association,
    const AmiCosemRegisterSnapshot *snapshot,
    const uint8_t *request, size_t request_length,
    uint8_t *response, size_t response_capacity, size_t *response_length)
{
    return process_get_request_plain(association, snapshot, request, request_length,
        response, response_capacity, response_length, 0);
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static int ranges_overlap(const void *left, size_t left_length,
                          const void *right, size_t right_length)
{
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    if (left_length > UINTPTR_MAX - left_start ||
        right_length > UINTPTR_MAX - right_start) return 1;
    uintptr_t left_end = left_start + left_length;
    uintptr_t right_end = right_start + right_length;
    return left_start < right_end && right_start < left_end;
}

AmiDlmsApduStatus ami_dlms_process_ciphered_request(
    const AmiDlmsAssociation *association,
    const AmiCosemRegisterSnapshot *snapshot,
    const AmiDlmsCipherBackend *cipher,
    const uint8_t *request, size_t request_length,
    uint8_t *workspace, size_t workspace_capacity,
    uint8_t *response, size_t response_capacity, size_t *response_length)
{
    if (response_length != NULL) *response_length = 0u;
    if (association == NULL || snapshot == NULL || cipher == NULL || request == NULL ||
        workspace == NULL || response == NULL || response_length == NULL) {
        return AMI_DLMS_APDU_INVALID_ARGUMENT;
    }
    if (ranges_overlap(request, request_length, workspace, workspace_capacity) ||
        ranges_overlap(response, response_capacity, workspace, workspace_capacity)) {
        return AMI_DLMS_APDU_INVALID_ARGUMENT;
    }
    if (association->state != AMI_DLMS_ASSOC_ASSOCIATED ||
        association->mechanism != AMI_DLMS_AUTH_HLS) {
        return AMI_DLMS_APDU_NOT_ASSOCIATED;
    }
    if (cipher->decrypt == NULL || cipher->encrypt == NULL ||
        cipher->accept_rx_counter == NULL || cipher->next_tx_counter == NULL) {
        return AMI_DLMS_APDU_INVALID_ARGUMENT;
    }
    if (request_length <= GLO_HEADER_LENGTH + AMI_DLMS_GCM_TAG_LENGTH ||
        (request[0] != GLO_GET_REQUEST_TAG && request[0] != GLO_SET_REQUEST_TAG)) {
        return AMI_DLMS_APDU_MALFORMED;
    }
    const uint8_t security_control = request[1];
    if (security_control != cipher->expected_security_control ||
        (security_control & 0x30u) != 0x30u) {
        return AMI_DLMS_APDU_AUTHENTICATION_FAILURE;
    }
    const uint32_t rx_counter = read_u32_be(request + 2u);
    const size_t ciphertext_length = request_length - GLO_HEADER_LENGTH - AMI_DLMS_GCM_TAG_LENGTH;
    const uint8_t *ciphertext = request + GLO_HEADER_LENGTH;
    const uint8_t *tag = ciphertext + ciphertext_length;
    if (ciphertext_length > workspace_capacity) return AMI_DLMS_APDU_BUFFER_TOO_SMALL;

    size_t plaintext_length = 0u;
    AmiSecStatus sec_status = cipher->decrypt(cipher->user, security_control, rx_counter,
        ciphertext, ciphertext_length, tag, workspace, workspace_capacity, &plaintext_length);
    if (sec_status != AMI_SEC_OK || plaintext_length == 0u ||
        plaintext_length > workspace_capacity) {
        memset(workspace, 0, workspace_capacity);
        return sec_status == AMI_SEC_AUTHENTICATION_FAILURE
            ? AMI_DLMS_APDU_AUTHENTICATION_FAILURE : AMI_DLMS_APDU_BACKEND_FAILURE;
    }
    /* Commit only after tag verification; callback must atomically reject replay. */
    if (cipher->accept_rx_counter(cipher->user, rx_counter) != 1) {
        memset(workspace, 0, plaintext_length);
        return AMI_DLMS_APDU_COUNTER_REJECTED;
    }

    const uint8_t expected_plain_tag = request[0] == GLO_GET_REQUEST_TAG
        ? GET_REQUEST_TAG : SET_REQUEST_TAG;
    AmiDlmsApduStatus service_status;
    size_t response_plain_length = 0u;
    if (workspace[0] != expected_plain_tag) {
        memset(workspace, 0, plaintext_length);
        return AMI_DLMS_APDU_MALFORMED;
    }
    if (expected_plain_tag == GET_REQUEST_TAG) {
        service_status = process_get_request_plain(association, snapshot, workspace,
            plaintext_length, workspace, workspace_capacity,
            &response_plain_length, 1);
    } else {
        /* Sprint 1 exposes read-only COSEM objects; SET is authenticated but denied. */
        if (plaintext_length < 14u || workspace[1] != 0x01u || workspace[12] != 0u) {
            memset(workspace, 0, plaintext_length);
            return AMI_DLMS_APDU_MALFORMED;
        }
        if (workspace_capacity < 4u) {
            memset(workspace, 0, plaintext_length);
            return AMI_DLMS_APDU_BUFFER_TOO_SMALL;
        }
        workspace[0] = SET_RESPONSE_TAG;
        workspace[1] = 0x01u;
        /* workspace[2] already holds the invoke-id. */
        workspace[3] = 3u; /* read-write-denied: meter is read-only */
        response_plain_length = 4u;
        service_status = AMI_DLMS_APDU_ACCESS_ERROR;
    }
    if (response_plain_length == 0u) {
        memset(workspace, 0, plaintext_length);
        return service_status;
    }

    if (response_capacity < GLO_HEADER_LENGTH + response_plain_length + AMI_DLMS_GCM_TAG_LENGTH) {
        memset(workspace, 0, response_plain_length);
        return AMI_DLMS_APDU_BUFFER_TOO_SMALL;
    }
    uint32_t tx_counter = 0u;
    if (cipher->next_tx_counter(cipher->user, &tx_counter) != 1) {
        memset(workspace, 0, response_plain_length);
        return AMI_DLMS_APDU_BACKEND_FAILURE;
    }
    const uint8_t response_tag = expected_plain_tag == GET_REQUEST_TAG
        ? GLO_GET_RESPONSE_TAG : GLO_SET_RESPONSE_TAG;
    response[0] = response_tag;
    response[1] = security_control;
    write_u32_be(response + 2u, tx_counter);
    size_t encrypted_length = 0u;
    sec_status = cipher->encrypt(cipher->user, security_control, tx_counter,
        workspace, response_plain_length, response + GLO_HEADER_LENGTH,
        response_capacity - GLO_HEADER_LENGTH - AMI_DLMS_GCM_TAG_LENGTH,
        &encrypted_length, response + GLO_HEADER_LENGTH + response_plain_length);
    memset(workspace, 0, response_plain_length);
    if (sec_status != AMI_SEC_OK || encrypted_length != response_plain_length) {
        memset(response, 0, response_capacity);
        return AMI_DLMS_APDU_BACKEND_FAILURE;
    }
    *response_length = GLO_HEADER_LENGTH + encrypted_length + AMI_DLMS_GCM_TAG_LENGTH;
    return service_status;
}
