#include "ami_dlms_apdu.h"

#include <limits.h>
#include <string.h>

#define GET_REQUEST_TAG 0xC0u
#define GET_RESPONSE_TAG 0xC4u
#define GET_NORMAL_CHOICE 0x01u

static int encode_data(const AmiCosemData *data, uint8_t *out,
                       size_t capacity, size_t *written)
{
    size_t offset = 0u;
    if (data == NULL || out == NULL || written == NULL) {
        return 0;
    }

    switch (data->type) {
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

AmiDlmsApduStatus ami_dlms_process_get_request(
    const AmiDlmsAssociation *association,
    const AmiCosemRegisterSnapshot *snapshot,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
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
