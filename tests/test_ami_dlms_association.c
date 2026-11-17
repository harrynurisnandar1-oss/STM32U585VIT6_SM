#include "ami_dlms_association.h"
#include "ami_security.h"

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
        0x5Fu, 0x1Fu, 0x04u, 0x00u, 0x00u, 0x00u, 0x14u,
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

static void test_real_hls5_gmac_association_handshake(void)
{
    AmiHls5GmacContext meter_sec_ctx = {
        .auth_key = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F},
        .system_title = {'P','L','N','M','E','T','E','R'},
        .invocation_counter = 100u
    };
    AmiDlmsAssociationSecurity security = {
        .random = mock_random,
        .hls_proof = ami_sec_hls5_gmac_proof_callback,
        .user = &meter_sec_ctx,
        .hls_mechanism_oid = hls_gmac_oid,
        .hls_mechanism_oid_length = sizeof(hls_gmac_oid)
    };

    AmiDlmsAssociation association = {0};
    uint8_t aare[96];
    size_t aare_length = 0u;

    // Pass 1 & 2: AARQ with HLS 5 mechanism
    assert(ami_dlms_process_aarq_with_security(&association, &security, aarq_hls,
        sizeof(aarq_hls), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_AUTH_PENDING);

    // Pass 3: Client calculates real GMAC proof of the server's challenge
    uint8_t client_proof[32] = {0};
    size_t client_proof_length = 0u;
    assert(ami_sec_dlms_hls5_gmac_calculate(
        meter_sec_ctx.auth_key, meter_sec_ctx.system_title, meter_sec_ctx.invocation_counter,
        association.server_challenge, association.server_challenge_length,
        client_proof, sizeof(client_proof), &client_proof_length) == AMI_SEC_OK);

    // Pass 4: Meter verifies client's GMAC proof and emits server's GMAC proof
    uint8_t server_proof[AMI_DLMS_AUTH_VALUE_MAX] = {0};
    size_t server_proof_length = 0u;
    assert(ami_dlms_process_hls_reply(&association, &security, client_proof,
        client_proof_length, server_proof, sizeof(server_proof),
        &server_proof_length) == AMI_DLMS_ASSOC_OK);
    assert(association.state == AMI_DLMS_ASSOC_ASSOCIATED);
    assert(server_proof_length == AMI_DLMS_HLS5_PROOF_LEN);

    // Client validates server's GMAC proof against its own challenge ("C1234567")
    const uint8_t client_orig_challenge[] = {'C', '1', '2', '3', '4', '5', '6', '7'};
    assert(ami_sec_dlms_hls5_gmac_verify(
        meter_sec_ctx.auth_key, meter_sec_ctx.system_title, server_proof,
        server_proof_length, client_orig_challenge, sizeof(client_orig_challenge)) == AMI_SEC_OK);

    // Negative check: Tampered client proof must be rejected
    ami_dlms_association_reset(&association);
    assert(ami_dlms_process_aarq_with_security(&association, &security, aarq_hls,
        sizeof(aarq_hls), aare, sizeof(aare), &aare_length) == AMI_DLMS_ASSOC_OK);
    client_proof[client_proof_length - 1u] ^= 0xFFu; // mutate tag
    assert(ami_dlms_process_hls_reply(&association, &security, client_proof,
        client_proof_length, server_proof, sizeof(server_proof),
        &server_proof_length) == AMI_DLMS_ASSOC_REJECTED);
    assert(association.state == AMI_DLMS_ASSOC_ERROR);
}

typedef struct {
    uint8_t guek[16];
    uint8_t ak[16];
    uint8_t server_title[8];
    uint8_t client_title[8];
    uint32_t tx_ic;
} CipherInitCtx;

static int test_decrypt_init(void *user, const uint8_t *cst, size_t cst_len,
                             uint8_t sc, uint32_t ic, const uint8_t *ct, size_t ct_len,
                             const uint8_t *tag, size_t tag_len,
                             uint8_t *pt, size_t pt_cap, size_t *pt_len)
{
    CipherInitCtx *c = (CipherInitCtx *)user;
    if (cst_len != 8u || memcmp(cst, c->client_title, 8u) != 0 || pt_cap < ct_len) return 0;
    uint8_t nonce[12];
    memcpy(nonce, cst, 8u);
    nonce[8] = (uint8_t)(ic >> 24); nonce[9] = (uint8_t)(ic >> 16);
    nonce[10] = (uint8_t)(ic >> 8); nonce[11] = (uint8_t)ic;
    uint8_t aad[17];
    aad[0] = sc; memcpy(aad + 1, c->ak, 16u);
    if (ami_sec_aes_128_gcm_decrypt(c->guek, nonce, 12u, aad, 17u,
            ct, ct_len, tag, tag_len, pt) != AMI_SEC_OK) return 0;
    *pt_len = ct_len;
    return 1;
}

static int test_encrypt_init(void *user, uint8_t sc, uint32_t *ic_out,
                             const uint8_t *pt, size_t pt_len,
                             uint8_t *ct, size_t ct_cap, size_t *ct_len,
                             uint8_t *tag, size_t tag_len)
{
    CipherInitCtx *c = (CipherInitCtx *)user;
    if (ct_cap < pt_len) return 0;
    uint32_t ic = ++c->tx_ic;
    *ic_out = ic;
    uint8_t nonce[12];
    memcpy(nonce, c->server_title, 8u);
    nonce[8] = (uint8_t)(ic >> 24); nonce[9] = (uint8_t)(ic >> 16);
    nonce[10] = (uint8_t)(ic >> 8); nonce[11] = (uint8_t)ic;
    uint8_t aad[17];
    aad[0] = sc; memcpy(aad + 1, c->ak, 16u);
    if (ami_sec_aes_128_gcm_encrypt(c->guek, nonce, 12u, aad, 17u,
            pt, pt_len, ct, tag, tag_len) != AMI_SEC_OK) return 0;
    *ct_len = pt_len;
    return 1;
}

static void test_ln_with_ciphering_and_glo_initiate(void)
{
    static const uint8_t hls5_oid[] = {0x60u, 0x85u, 0x74u, 0x05u, 0x08u, 0x02u, 0x05u};
    CipherInitCtx cctx = {
        .guek = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        .ak = {0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF},
        .server_title = {0x4D,0x4D,0x4D,0,0,0,0,1},
        .client_title = {0x47,0x58,0x44,0x4C,0x4D,0x53,0x30,0x31},
        .tx_ic = 10u
    };
    const uint8_t plain_init_req[] = {
        0x01u, 0x00u, 0x00u, 0x00u, 0x06u, 0x5Fu, 0x1Fu, 0x04u, 0x00u,
        0x00u, 0x10u, 0x1Du, 0x01u, 0xF4u
    };
    uint8_t nonce[12] = {0x47,0x58,0x44,0x4C,0x4D,0x53,0x30,0x31, 0,0,0,1};
    uint8_t aad[17]; aad[0] = 0x30u; memcpy(aad + 1, cctx.ak, 16u);
    uint8_t ct[14], tag[12];
    assert(ami_sec_aes_128_gcm_encrypt(cctx.guek, nonce, 12u, aad, 17u,
        plain_init_req, sizeof(plain_init_req), ct, tag, sizeof(tag)) == AMI_SEC_OK);

    /* Build AARQ with 2.16.756.5.8.1.3, calling-AP-title (0xA6), HLS5, glo_initiateRequest (0x21) */
    uint8_t aarq[128];
    size_t n = 0u;
    aarq[n++] = 0x60u;
    size_t len_pos = n++;
    /* 0xA1 application context 2.16.756.5.8.1.3 */
    const uint8_t ctx_field[] = {0xA1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x03};
    memcpy(aarq + n, ctx_field, sizeof(ctx_field)); n += sizeof(ctx_field);
    /* 0xA6 calling-AP-title */
    aarq[n++] = 0xA6u; aarq[n++] = 10u; aarq[n++] = 0x04u; aarq[n++] = 8u;
    memcpy(aarq + n, cctx.client_title, 8u); n += 8u;
    /* 0x8A, 0x8B, 0xAC */
    const uint8_t acse_auth[] = {0x8A, 0x02, 0x07, 0x80, 0x8B, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x05};
    memcpy(aarq + n, acse_auth, sizeof(acse_auth)); n += sizeof(acse_auth);
    aarq[n++] = 0xACu; aarq[n++] = 18u; aarq[n++] = 0x80u; aarq[n++] = 16u;
    for (uint8_t i = 0u; i < 16u; ++i) aarq[n++] = (uint8_t)(0x10u + i);
    /* 0xBE user-information with 0x21 glo_initiateRequest */
    const size_t glo_len = 5u + sizeof(ct) + sizeof(tag);
    const size_t ui_len = 2u + glo_len;
    aarq[n++] = 0xBEu; aarq[n++] = (uint8_t)(2u + ui_len);
    aarq[n++] = 0x04u; aarq[n++] = (uint8_t)ui_len;
    aarq[n++] = 0x21u; aarq[n++] = (uint8_t)glo_len;
    aarq[n++] = 0x30u; aarq[n++] = 0u; aarq[n++] = 0u; aarq[n++] = 0u; aarq[n++] = 1u;
    memcpy(aarq + n, ct, sizeof(ct)); n += sizeof(ct);
    memcpy(aarq + n, tag, sizeof(tag)); n += sizeof(tag);
    aarq[len_pos] = (uint8_t)(n - 2u);

    AmiDlmsAssociation assoc = {0};
    const AmiDlmsAssociationSecurity sec = {
        .user = &cctx,
        .allow_ciphering = 1u,
        .server_system_title = cctx.server_title,
        .server_system_title_length = 8u,
        .random = mock_random,
        .hls_proof = mock_hls_proof,
        .decrypt_initiate = test_decrypt_init,
        .encrypt_initiate = test_encrypt_init,
        .hls_mechanism_oid = hls5_oid,
        .hls_mechanism_oid_length = sizeof(hls5_oid)
    };
    uint8_t aare[256];
    size_t aare_len = 0u;
    assert(ami_dlms_process_aarq_with_security(&assoc, &sec, aarq, n,
        aare, sizeof(aare), &aare_len) == AMI_DLMS_ASSOC_OK);
    assert(assoc.state == AMI_DLMS_ASSOC_AUTH_PENDING);
    assert(assoc.ciphering_enabled == 1u);
    assert(assoc.client_system_title_length == 8u);
    assert(memcmp(assoc.client_system_title, cctx.client_title, 8u) == 0);
    assert(aare[0] == 0x61u);
    assert(aare_len > 60u);
}

int main(void)
{
    test_valid_aarq_is_accepted();
    test_malformed_and_authenticated_aarq_are_rejected();
    test_state_transition_and_reset();
    test_lls_password_accepted_and_rejected();
    test_unsolicited_aarq_does_not_tear_down_session();
    test_hls_challenge_exchange();
    test_real_hls5_gmac_association_handshake();
    test_ln_with_ciphering_and_glo_initiate();
    puts("DLMS association tests passed.");
    return 0;
}
