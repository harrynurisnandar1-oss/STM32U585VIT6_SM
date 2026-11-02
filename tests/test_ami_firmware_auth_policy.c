#include "ami_dlms_association.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t aarq_no_auth[] = {
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

static const uint8_t hls5_oid[] = {
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u
};

static const uint8_t hls3_oid[] = {
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x03u
};

static const uint8_t hls4_oid[] = {
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x04u
};

static const uint8_t aarq_hls3[] = {
    0x60u, 0x24u, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
    0x8Au, 0x02u, 0x07u, 0x80u,
    0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x03u,
    0xACu, 0x0Au, 0x80u, 0x08u, 'C', '1', '2', '3', '4', '5', '6', '7'
};

static const uint8_t aarq_hls4[] = {
    0x60u, 0x24u, 0xA1u, 0x09u, 0x06u, 0x07u,
    0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
    0x8Au, 0x02u, 0x07u, 0x80u,
    0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x04u,
    0xACu, 0x0Au, 0x80u, 0x08u, 'C', '1', '2', '3', '4', '5', '6', '7'
};

static int accept_any_password(void *user, const uint8_t *password, size_t length)
{
    (void)user;
    (void)password;
    (void)length;
    return 1;
}

static int test_random(void *user, uint8_t *output, size_t length)
{
    (void)user;
    for (size_t i = 0u; i < length; ++i) output[i] = (uint8_t)(i + 1u);
    return 1;
}

static int test_hls_proof(void *user, const uint8_t *oid, size_t oid_length,
                          const uint8_t *challenge, size_t challenge_length,
                          uint8_t *proof, size_t capacity, size_t *proof_length)
{
    (void)user;
    if (oid == NULL || challenge == NULL || proof == NULL ||
        proof_length == NULL || capacity < challenge_length) return 0;
    memcpy(proof, challenge, challenge_length);
    *proof_length = challenge_length;
    return 1;
}

static void assert_rejected(const uint8_t *aarq, size_t aarq_length,
                            const AmiDlmsAssociationSecurity *security)
{
    AmiDlmsAssociation association = {0};
    uint8_t aare[128];
    size_t aare_length = 0u;

    assert(ami_dlms_process_aarq_with_security(&association, security,
        aarq, aarq_length, aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ERROR);
    assert(aare_length > 0u);
    assert(aare[0] == 0x61u);
    assert(aare[17] == 1u);
}

int main(void)
{
    const AmiDlmsAssociationSecurity no_auth_policy = {
        .allow_no_auth = 1u
    };
    const AmiDlmsAssociationSecurity lls_policy = {
        .verify_lls = accept_any_password
    };
    const AmiDlmsAssociationSecurity legacy_hls_policy = {
        .random = test_random,
        .hls_proof = test_hls_proof,
        .hls_mechanism_oid = hls3_oid,
        .hls_mechanism_oid_length = sizeof(hls3_oid)
    };
    const AmiDlmsAssociationSecurity legacy_sha1_policy = {
        .random = test_random,
        .hls_proof = test_hls_proof,
        .hls_mechanism_oid = hls4_oid,
        .hls_mechanism_oid_length = sizeof(hls4_oid)
    };
    const AmiDlmsAssociationSecurity hls5_policy = {
        .random = test_random,
        .hls_proof = test_hls_proof,
        .hls_mechanism_oid = hls5_oid,
        .hls_mechanism_oid_length = sizeof(hls5_oid)
    };

    assert_rejected(aarq_no_auth, sizeof(aarq_no_auth), &no_auth_policy);
    assert_rejected(aarq_lls, sizeof(aarq_lls), &lls_policy);
    assert_rejected(aarq_hls3, sizeof(aarq_hls3), &legacy_hls_policy);
    assert_rejected(aarq_hls4, sizeof(aarq_hls4), &legacy_sha1_policy);

    static const uint8_t aarq_hls5[] = {
        0x60u, 0x2Cu, 0xA1u, 0x09u, 0x06u, 0x07u,
        0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
        0x8Au, 0x02u, 0x07u, 0x80u,
        0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u,
        0xACu, 0x12u, 0x80u, 0x10u,
        'C', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    AmiDlmsAssociation association = {0};
    uint8_t aare[128];
    size_t aare_length = 0u;
    assert(ami_dlms_process_aarq_with_security(&association, &hls5_policy,
        aarq_hls5, sizeof(aarq_hls5), aare, sizeof(aare), &aare_length) ==
        AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_AUTH_PENDING);
    assert(association.mechanism == AMI_DLMS_AUTH_HLS);

    ami_dlms_association_reset(&association);
    static const uint8_t aarq_hls5_short_challenge[] = {
        0x60u, 0x24u, 0xA1u, 0x09u, 0x06u, 0x07u,
        0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x01u, 0x01u,
        0x8Au, 0x02u, 0x07u, 0x80u,
        0x8Bu, 0x07u, 0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u,
        0xACu, 0x0Au, 0x80u, 0x08u, 'C', '1', '2', '3', '4', '5', '6', '7'
    };
    assert_rejected(aarq_hls5_short_challenge,
                    sizeof(aarq_hls5_short_challenge), &hls5_policy);

    puts("Firmware authentication profile policy tests passed.");
    return 0;
}
