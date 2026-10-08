#ifndef AMI_DLMS_APDU_H
#define AMI_DLMS_APDU_H

#include "ami_cosem_model.h"
#include "ami_dlms_association.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AMI_DLMS_APDU_OK = 0,
    AMI_DLMS_APDU_NOT_ASSOCIATED,
    AMI_DLMS_APDU_MALFORMED,
    AMI_DLMS_APDU_ACCESS_ERROR,
    AMI_DLMS_APDU_BUFFER_TOO_SMALL,
    AMI_DLMS_APDU_INVALID_ARGUMENT
} AmiDlmsApduStatus;

/* Processes a single GET-Request-Normal (C0 01), no selective access. */
AmiDlmsApduStatus ami_dlms_process_get_request(
    const AmiDlmsAssociation *association,
    const AmiCosemRegisterSnapshot *snapshot,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
