#include "ami_dlms_association.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t aarq_ln_no_auth[] = {
    0x60u, 0x0Bu, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u
};

static const uint8_t aarq_lls[] = {
    0x60u, 0x24u, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
    0x8Au, 0x02u, 0x07u, 0x80u,
    0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x01u,
    0xACu, 0x0Au, 0x80u, 0x08u, '1', '2', '3', '4', '5', '6', '7', '8'
};

static const uint8_t hls_gmac_oid[] = {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u};
static const uint8_t aarq_hls[] = {
    0x60u, 0x24u, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
    0x8Au, 0x02u, 0x07u, 0x80u,
    0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u,
    0xACu, 0x0Au, 0x80u, 0x08u, 'C', '1', '2', '3', '4', '5', '6', '7'
};

static int mock_verify_lls(void *user, const uint8_t *password, size_t length)
{
    (void)user;
    static const uint8_t expected[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    return length == sizeof(expected) && memcmp(password, expected, length) == 0;
}

static int mock_random(void *user, uint8_t *output, size_t length)
{
    (void)user;
    for (size_t i = 0u; i < length; ++i) output[i] = (uint8_t)i;
    return 1;
}

static int mock_hls_proof(void *user, const uint8_t *oid, size_t oid_length,
                         const uint8_t *challenge, size_t challenge_length,
                         uint8_t *proof, size_t capacity, size_t *proof_length)
{
    (void)user;
    if (oid_length != sizeof(hls_gmac_oid) ||
        memcmp(oid, hls_gmac_oid, oid_length) != 0 || capacity < challenge_length) return 0;
    for (size_t i = 0u; i < challenge_length; ++i) proof[i] = challenge[i] ^ 0xA5u;
    *proof_length = challenge_length;
    return 1;
}

static void test_valid_aarq_is_accepted(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_IDLE};
    uint8_t aare[64];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(aare_length == 43u && aare[0] == 0x61u && aare[1] == 41u);
    assert(aare[17] == 0x00u); /* associate-result: accepted */
    const uint8_t initiate_response[] = {
        0xBEu, 0x10u, 0x04u, 0x0Eu, 0x08u, 0x00u, 0x06u,
        0x5Fu, 0x1Fu, 0x04u, 0x00u, 0x00u, 0x50u, 0x1Fu,
        0x01u, 0xF4u, 0x00u, 0x07u
    };
    assert(memcmp(aare + aare_length - sizeof(initiate_response),
                  initiate_response, sizeof(initiate_response)) == 0);
}

static void test_malformed_and_authenticated_aarq_are_rejected(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_IDLE};
    uint8_t aare[64];
    size_t aare_length = 0u;
    const uint8_t malformed[] = {0x60u, 0x82u, 0x01u};
    assert(ami_dlms_process_aarq(&association, malformed, sizeof(malformed),
             aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ERROR);
    assert(aare_length == 25u && aare[17] == 1u);

    ami_dlms_association_reset(&association);
    uint8_t aarq_with_mechanism[sizeof(aarq_ln_no_auth) + 3u];
    const uint8_t auth_mechanism[] = {0x8Bu, 0x01u, 0x00u};
    aarq_with_mechanism[0] = 0x60u;
    aarq_with_mechanism[1] = 0x0Eu;
    for (size_t i = 2u; i < sizeof(aarq_ln_no_auth); ++i) {
        aarq_with_mechanism[i] = aarq_ln_no_auth[i];
    }
    for (size_t i = 0u; i < sizeof(auth_mechanism); ++i) {
        aarq_with_mechanism[sizeof(aarq_ln_no_auth) + i] = auth_mechanism[i];
    }
    assert(ami_dlms_process_aarq(&association, aarq_with_mechanism,
             sizeof(aarq_with_mechanism), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_REJECTED);
}

static void test_state_transition_and_reset(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_IDLE};
    uint8_t aare[64];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_OK);
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    ami_dlms_association_reset(&association);
    assert(association.state == AMI_DLMS_ASSOC_IDLE);
}

static void test_lls_password_accepted_and_rejected(void)
{
    AmiDlmsAssociationSecurity security = {.verify_lls = mock_verify_lls};
    AmiDlmsAssociation association = {0};
    uint8_t aare[64];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq_with_security(&association, &security, aarq_lls,
        sizeof(aarq_lls), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(association.mechanism == AMI_DLMS_AUTH_LLS);
    assert(aare[0] == 0x61u && aare[17] == 0u);

    ami_dlms_association_reset(&association);
    uint8_t bad_password_aarq[sizeof(aarq_lls)];
    memcpy(bad_password_aarq, aarq_lls, sizeof(aarq_lls));
    bad_password_aarq[sizeof(bad_password_aarq) - 1u] ^= 1u;
    assert(ami_dlms_process_aarq_with_security(&association, &security, bad_password_aarq,
        sizeof(bad_password_aarq), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ERROR);
}

static void test_unsolicited_aarq_does_not_tear_down_session(void)
{
    AmiDlmsAssociationSecurity security = {.verify_lls = mock_verify_lls};
    AmiDlmsAssociation association = {0};
    uint8_t aare[64];
    size_t aare_length = 0u;
    const uint8_t stray[] = {0x60u};
    assert(ami_dlms_process_aarq_with_security(&association, &security, aarq_lls,
        sizeof(aarq_lls), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_OK);
    assert(ami_dlms_process_aarq_with_security(&association, &security, stray,
        sizeof(stray), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(association.mechanism == AMI_DLMS_AUTH_LLS);
}

static void test_hls_challenge_exchange(void)
{
    AmiDlmsAssociationSecurity security = {
        .random = mock_random,
        .hls_proof = mock_hls_proof,
        .hls_mechanism_oid = hls_gmac_oid,
        .hls_mechanism_oid_length = sizeof(hls_gmac_oid)
    };
    AmiDlmsAssociation association = {0};
    uint8_t aare[96];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq_with_security(&association, &security, aarq_hls,
        sizeof(aarq_hls), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_AUTH_PENDING);
    assert(aare[0] == 0x61u && aare[25] == 0x88u && aare[29] == 0x89u);
    assert(aare[38] == 0xAAu && aare[42] == 0u && aare[57] == 15u);

    uint8_t client_proof[AMI_DLMS_HLS_CHALLENGE_LENGTH];
    for (size_t i = 0u; i < sizeof(client_proof); ++i) client_proof[i] = (uint8_t)i ^ 0xA5u;
    uint8_t server_proof[AMI_DLMS_AUTH_VALUE_MAX];
    size_t server_proof_length = 0u;
    assert(ami_dlms_process_hls_reply(&association, &security, client_proof,
        sizeof(client_proof), server_proof, sizeof(server_proof),
        &server_proof_length) == AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(server_proof_length == 8u && server_proof[0] == ('C' ^ 0xA5u));
}

int main(void)
{
    test_valid_aarq_is_accepted();
    test_malformed_and_authenticated_aarq_are_rejected();
    test_state_transition_and_reset();
    test_lls_password_accepted_and_rejected();
    test_unsolicited_aarq_does_not_tear_down_session();
    test_hls_challenge_exchange();
    puts("DLMS association tests passed.");
    return 0;
}
