#include "ami_dlms_association.h"

#include <assert.h>
#include <stdio.h>

static const uint8_t aarq_ln_no_auth[] = {
    0x60u, 0x0Bu, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u
};

static void test_valid_aarq_is_accepted(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_IDLE};
    uint8_t aare[32];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(aare_length == 25u && aare[0] == 0x61u && aare[1] == 23u);
    assert(aare[17] == 0x00u); /* associate-result: accepted */
}

static void test_malformed_and_authenticated_aarq_are_rejected(void)
{
    AmiDlmsAssociation association = {AMI_DLMS_ASSOC_IDLE};
    uint8_t aare[32];
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
    uint8_t aare[32];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_OK);
    assert(ami_dlms_process_aarq(&association, aarq_ln_no_auth,
             sizeof(aarq_ln_no_auth), aare, sizeof(aare), &aare_length) ==
           AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ERROR);
    ami_dlms_association_reset(&association);
    assert(association.state == AMI_DLMS_ASSOC_IDLE);
}

int main(void)
{
    test_valid_aarq_is_accepted();
    test_malformed_and_authenticated_aarq_are_rejected();
    test_state_transition_and_reset();
    puts("DLMS association tests passed.");
    return 0;
}
