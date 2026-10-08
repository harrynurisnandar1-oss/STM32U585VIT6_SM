#ifndef AMI_DLMS_ASSOCIATION_H
#define AMI_DLMS_ASSOCIATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AMI_DLMS_ASSOC_IDLE = 0,
    AMI_DLMS_ASSOC_ASSOCIATED,
    AMI_DLMS_ASSOC_ERROR
} AmiDlmsAssociationState;

typedef struct {
    AmiDlmsAssociationState state;
} AmiDlmsAssociation;

typedef enum {
    AMI_DLMS_ASSOC_OK = 0,
    AMI_DLMS_ASSOC_REJECTED,
    AMI_DLMS_ASSOC_INVALID_ARGUMENT,
    AMI_DLMS_ASSOC_BUFFER_TOO_SMALL
} AmiDlmsAssociationStatus;

/* Sprint 1 accepts logical-name referencing without authentication or ciphering. */
AmiDlmsAssociationStatus ami_dlms_process_aarq(
    AmiDlmsAssociation *association,
    const uint8_t *aarq,
    size_t aarq_length,
    uint8_t *aare,
    size_t aare_capacity,
    size_t *aare_length);

void ami_dlms_association_reset(AmiDlmsAssociation *association);

#ifdef __cplusplus
}
#endif

#endif
