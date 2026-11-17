#ifndef AMI_DLMS_ASSOCIATION_H
#define AMI_DLMS_ASSOCIATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AMI_DLMS_AUTH_VALUE_MAX 64u
#define AMI_DLMS_OID_MAX 16u
#define AMI_DLMS_HLS_CHALLENGE_LENGTH 16u
#define AMI_DLMS_SYSTEM_TITLE_MAX 8u
#define AMI_DLMS_DEDICATED_KEY_MAX 16u

typedef enum {
    AMI_DLMS_ASSOC_IDLE = 0,
    AMI_DLMS_ASSOC_AUTH_PENDING,
    AMI_DLMS_ASSOC_ASSOCIATED,
    AMI_DLMS_ASSOC_ERROR
} AmiDlmsAssociationState;

typedef enum {
    AMI_DLMS_AUTH_NONE = 0,
    AMI_DLMS_AUTH_LLS,
    AMI_DLMS_AUTH_HLS
} AmiDlmsAuthMechanism;

typedef struct {
    AmiDlmsAssociationState state;
    AmiDlmsAuthMechanism mechanism;
    uint8_t ciphering_enabled;
    uint8_t dedicated_key_present;
    uint8_t dedicated_key[AMI_DLMS_DEDICATED_KEY_MAX];
    uint16_t client_sap;
    uint16_t server_sap;
    uint8_t client_system_title[AMI_DLMS_SYSTEM_TITLE_MAX];
    size_t client_system_title_length;
    uint8_t client_challenge[AMI_DLMS_AUTH_VALUE_MAX];
    size_t client_challenge_length;
    uint8_t server_challenge[AMI_DLMS_HLS_CHALLENGE_LENGTH];
    size_t server_challenge_length;
    uint8_t hls_mechanism_oid[AMI_DLMS_OID_MAX];
    size_t hls_mechanism_oid_length;
} AmiDlmsAssociation;

/* Callbacks must bridge to protected credential/key storage and a CSPRNG.
 * hls_proof calculates the profile-specific response for a challenge. */
typedef struct {
    void *user;
    uint8_t allow_no_auth;
    uint8_t allow_ciphering;
    const uint8_t *server_system_title;
    size_t server_system_title_length;
    int (*verify_lls)(void *user, const uint8_t *password, size_t password_length);
    int (*random)(void *user, uint8_t *output, size_t output_length);
    int (*hls_proof)(void *user, const uint8_t *mechanism_oid,
                     size_t mechanism_oid_length, const uint8_t *challenge,
                     size_t challenge_length, uint8_t *proof,
                     size_t proof_capacity, size_t *proof_length);
    /* Optional full HLS5 verifier that uses peer system title & embedded IC. */
    int (*hls_verify)(void *user, const uint8_t *mechanism_oid,
                      size_t mechanism_oid_length,
                      const uint8_t *client_system_title,
                      size_t client_system_title_length,
                      const uint8_t *challenge, size_t challenge_length,
                      const uint8_t *client_proof, size_t client_proof_length);
    /* Optional ciphering hooks for glo_initiateRequest (0x21) & glo_initiateResponse (0x28). */
    int (*decrypt_initiate)(void *user, const uint8_t *client_system_title,
                            size_t client_system_title_length,
                            uint8_t security_control, uint32_t invocation_counter,
                            const uint8_t *ciphertext, size_t ciphertext_length,
                            const uint8_t *tag, size_t tag_length,
                            uint8_t *plaintext, size_t plaintext_capacity,
                            size_t *plaintext_length);
    int (*encrypt_initiate)(void *user, uint8_t security_control,
                            uint32_t *invocation_counter,
                            const uint8_t *plaintext, size_t plaintext_length,
                            uint8_t *ciphertext, size_t ciphertext_capacity,
                            size_t *ciphertext_length,
                            uint8_t *tag, size_t tag_length);
    const uint8_t *hls_mechanism_oid;
    size_t hls_mechanism_oid_length;
} AmiDlmsAssociationSecurity;

typedef enum {
    AMI_DLMS_ASSOC_OK = 0,
    AMI_DLMS_ASSOC_REJECTED,
    AMI_DLMS_ASSOC_INVALID_ARGUMENT,
    AMI_DLMS_ASSOC_BUFFER_TOO_SMALL,
    AMI_DLMS_ASSOC_BACKEND_FAILURE
} AmiDlmsAssociationStatus;

/* Authenticates AARQ using no-auth, LLS, or the configured HLS profile. HLS
 * returns accepted AARE with a server challenge and enters AUTH_PENDING. */
AmiDlmsAssociationStatus ami_dlms_process_aarq_with_security(
    AmiDlmsAssociation *association,
    const AmiDlmsAssociationSecurity *security,
    const uint8_t *aarq, size_t aarq_length,
    uint8_t *aare, size_t aare_capacity, size_t *aare_length);

/* Completes HLS pass 3/4: verify the client's proof of the server challenge,
 * return the server proof of the client's challenge, then associate. */
AmiDlmsAssociationStatus ami_dlms_process_hls_reply(
    AmiDlmsAssociation *association,
    const AmiDlmsAssociationSecurity *security,
    const uint8_t *client_proof, size_t client_proof_length,
    uint8_t *server_proof, size_t server_proof_capacity,
    size_t *server_proof_length);

#if !defined(STM32U585xx) && !defined(USE_HAL_DRIVER)
/* Host-test compatibility entry point that permits no-auth association. */
AmiDlmsAssociationStatus ami_dlms_process_aarq(
    AmiDlmsAssociation *association, const uint8_t *aarq, size_t aarq_length,
    uint8_t *aare, size_t aare_capacity, size_t *aare_length);
#endif

void ami_dlms_association_reset(AmiDlmsAssociation *association);

#ifdef __cplusplus
}
#endif

#endif
