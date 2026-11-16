/**
 * @file    ami_boot_crypto.c
 * @brief   Self-contained Pure C11 FIPS 180-4 SHA-256 & FIPS 186-4 ECDSA P-256.
 *          Zero-heap, bounded stack, suitable for Cortex-M33 Secure Boot & Host.
 */
#include "ami_boot_crypto.h"
#include <string.h>

/* ========================================================================= */
/* FIPS 180-4 SHA-256                                                        */
/* ========================================================================= */

static inline uint32_t rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32U - n));
}

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static void sha256_transform(AmiSha256Context *ctx, const uint8_t block[64])
{
    uint32_t m[64];
    for (size_t i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)block[j] << 24) |
               ((uint32_t)block[j + 1] << 16) |
               ((uint32_t)block[j + 2] << 8) |
               ((uint32_t)block[j + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(m[i - 15], 7u) ^ rotr32(m[i - 15], 18u) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr32(m[i - 2], 17u) ^ rotr32(m[i - 2], 19u) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (size_t i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + m[i];
        uint32_t S0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void ami_sha256_init(AmiSha256Context *ctx)
{
    ctx->datalen = 0u;
    ctx->bitlen = 0u;
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
}

void ami_sha256_update(AmiSha256Context *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64u) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512u;
            ctx->datalen = 0u;
        }
    }
}

void ami_sha256_final(AmiSha256Context *ctx, uint8_t out_hash[32])
{
    size_t i = ctx->datalen;
    if (ctx->datalen < 56u) {
        ctx->data[i++] = 0x80u;
        while (i < 56u) {
            ctx->data[i++] = 0x00u;
        }
    } else {
        ctx->data[i++] = 0x80u;
        while (i < 64u) {
            ctx->data[i++] = 0x00u;
        }
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56u);
    }

    ctx->bitlen += (uint64_t)ctx->datalen * 8u;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (size_t w = 0; w < 8u; ++w) {
        out_hash[w * 4u + 0u] = (uint8_t)(ctx->state[w] >> 24);
        out_hash[w * 4u + 1u] = (uint8_t)(ctx->state[w] >> 16);
        out_hash[w * 4u + 2u] = (uint8_t)(ctx->state[w] >> 8);
        out_hash[w * 4u + 3u] = (uint8_t)(ctx->state[w]);
    }
}

void ami_sha256_compute(const uint8_t *data, size_t len, uint8_t out_hash[32])
{
    AmiSha256Context ctx;
    ami_sha256_init(&ctx);
    ami_sha256_update(&ctx, data, len);
    ami_sha256_final(&ctx, out_hash);
}

/* ========================================================================= */
/* 256-bit Unsigned & Modular Arithmetic for P-256 (secp256r1)               */
/* Limbs are 8 x uint32_t in little-endian order (limb[0] = LSB).            */
/* ========================================================================= */

typedef struct {
    uint32_t d[8];
} u256;

typedef struct {
    u256 x;
    u256 y;
    u256 z;
    int  infinity;
} ec_point_jac;

static const u256 P256_P = {{
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000001u, 0xFFFFFFFFu
}};

static const u256 P256_N = {{
    0xFC632551u, 0xF3B9CAC2u, 0xA7179E84u, 0xBCE6FAADu,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu
}};

static const u256 P256_B = {{
    0x27D2604Bu, 0x3BCE3C3Eu, 0xCC53B0F6u, 0x651D06B0u,
    0x769886BCu, 0xB3EBBD55u, 0xAA3A93E7u, 0x5AC635D8u
}};

static const u256 P256_GX = {{
    0xD898C296u, 0xF4A13945u, 0x2DEB33A0u, 0x77037D81u,
    0x63A440F2u, 0xF8BCE6E5u, 0xE12C4247u, 0x6B17D1F2u
}};

static const u256 P256_GY = {{
    0x37BF51F5u, 0xCBB64068u, 0x6B315ECEu, 0x2BCE3357u,
    0x7C0F9E16u, 0x8EE7EB4Au, 0xFE1A7F9Bu, 0x4FE342E2u
}};

static void u256_from_be(u256 *out, const uint8_t in[32])
{
    for (int i = 0; i < 8; ++i) {
        int off = (7 - i) * 4;
        out->d[i] = ((uint32_t)in[off] << 24) |
                    ((uint32_t)in[off + 1] << 16) |
                    ((uint32_t)in[off + 2] << 8) |
                    ((uint32_t)in[off + 3]);
    }
}

#ifdef AMI_BOOT_CRYPTO_ENABLE_SIGN
static void u256_to_be(uint8_t out[32], const u256 *in)
{
    for (int i = 0; i < 8; ++i) {
        int off = (7 - i) * 4;
        out[off]     = (uint8_t)(in->d[i] >> 24);
        out[off + 1] = (uint8_t)(in->d[i] >> 16);
        out[off + 2] = (uint8_t)(in->d[i] >> 8);
        out[off + 3] = (uint8_t)(in->d[i]);
    }
}
#endif

static int u256_is_zero(const u256 *a)
{
    uint32_t acc = 0u;
    for (int i = 0; i < 8; ++i) acc |= a->d[i];
    return acc == 0u;
}

static int u256_cmp(const u256 *a, const u256 *b)
{
    for (int i = 7; i >= 0; --i) {
        if (a->d[i] > b->d[i]) return 1;
        if (a->d[i] < b->d[i]) return -1;
    }
    return 0;
}

static uint32_t u256_add_raw(u256 *r, const u256 *a, const u256 *b)
{
    uint64_t carry = 0u;
    for (int i = 0; i < 8; ++i) {
        uint64_t sum = (uint64_t)a->d[i] + b->d[i] + carry;
        r->d[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

static uint32_t u256_sub_raw(u256 *r, const u256 *a, const u256 *b)
{
    uint64_t borrow = 0u;
    for (int i = 0; i < 8; ++i) {
        uint64_t diff = (uint64_t)a->d[i] - b->d[i] - borrow;
        r->d[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1u;
    }
    return (uint32_t)borrow;
}

static void mod_add(u256 *r, const u256 *a, const u256 *b, const u256 *m)
{
    uint32_t c = u256_add_raw(r, a, b);
    if (c != 0u || u256_cmp(r, m) >= 0) {
        u256_sub_raw(r, r, m);
    }
}

static void mod_sub(u256 *r, const u256 *a, const u256 *b, const u256 *m)
{
    if (u256_sub_raw(r, a, b) != 0u) {
        u256_add_raw(r, r, m);
    }
}

/* Generic 256x256 -> 512 multiply + shift-subtract mod m (works for any odd/prime 256-bit m) */
static void mod_mul(u256 *r, const u256 *a, const u256 *b, const u256 *m)
{
    uint32_t prod[16];
    memset(prod, 0, sizeof(prod));
    for (int i = 0; i < 8; ++i) {
        uint64_t carry = 0u;
        for (int j = 0; j < 8; ++j) {
            uint64_t cur = (uint64_t)prod[i + j] + (uint64_t)a->d[i] * b->d[j] + carry;
            prod[i + j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        prod[i + 8] = (uint32_t)carry;
    }

    /* Bitwise shift-and-reduce from bit 511 down to 0 */
    u256 acc;
    memset(&acc, 0, sizeof(acc));
    for (int bit = 511; bit >= 0; --bit) {
        uint32_t top = (acc.d[7] >> 31) & 1u;
        for (int k = 7; k > 0; --k) {
            acc.d[k] = (acc.d[k] << 1) | (acc.d[k - 1] >> 31);
        }
        acc.d[0] = (acc.d[0] << 1) | ((prod[bit >> 5] >> (bit & 31)) & 1u);
        if (top != 0u || u256_cmp(&acc, m) >= 0) {
            u256_sub_raw(&acc, &acc, m);
        }
    }
    *r = acc;
}

static void mod_sqr(u256 *r, const u256 *a, const u256 *m)
{
    mod_mul(r, a, a, m);
}

/* Fermat's Little Theorem: a^(m-2) mod m (m is prime: P256_P or P256_N) */
static void mod_inv(u256 *r, const u256 *a, const u256 *m)
{
    u256 exp = *m;
    u256 two = {{2u, 0, 0, 0, 0, 0, 0, 0}};
    u256_sub_raw(&exp, &exp, &two);

    u256 res = {{1u, 0, 0, 0, 0, 0, 0, 0}};
    u256 base = *a;
    for (int bit = 0; bit < 256; ++bit) {
        if ((exp.d[bit >> 5] >> (bit & 31)) & 1u) {
            mod_mul(&res, &res, &base, m);
        }
        mod_sqr(&base, &base, m);
    }
    *r = res;
}

/* ========================================================================= */
/* Jacobian Point Operations on secp256r1 (a = -3 mod p)                     */
/* ========================================================================= */

static void ec_point_double(ec_point_jac *r, const ec_point_jac *p)
{
    if (p->infinity || u256_is_zero(&p->y)) {
        memset(r, 0, sizeof(*r));
        r->infinity = 1;
        return;
    }

    u256 s, m, t, y2, z2, x_new, y_new, z_new;
    /* z2 = Z^2 */
    mod_sqr(&z2, &p->z, &P256_P);
    /* m = 3*(X - Z^2)*(X + Z^2) */
    mod_sub(&t, &p->x, &z2, &P256_P);
    mod_add(&s, &p->x, &z2, &P256_P);
    mod_mul(&m, &t, &s, &P256_P);
    mod_add(&t, &m, &m, &P256_P);
    mod_add(&m, &t, &m, &P256_P);

    /* y2 = Y^2; s = 4*X*Y^2 */
    mod_sqr(&y2, &p->y, &P256_P);
    mod_mul(&s, &p->x, &y2, &P256_P);
    mod_add(&s, &s, &s, &P256_P);
    mod_add(&s, &s, &s, &P256_P);

    /* x_new = M^2 - 2*S */
    mod_sqr(&x_new, &m, &P256_P);
    mod_sub(&x_new, &x_new, &s, &P256_P);
    mod_sub(&x_new, &x_new, &s, &P256_P);

    /* y_new = M*(S - x_new) - 8*Y^4 */
    mod_sub(&t, &s, &x_new, &P256_P);
    mod_mul(&y_new, &m, &t, &P256_P);
    mod_sqr(&t, &y2, &P256_P);
    mod_add(&t, &t, &t, &P256_P);
    mod_add(&t, &t, &t, &P256_P);
    mod_add(&t, &t, &t, &P256_P);
    mod_sub(&y_new, &y_new, &t, &P256_P);

    /* z_new = 2*Y*Z */
    mod_mul(&z_new, &p->y, &p->z, &P256_P);
    mod_add(&z_new, &z_new, &z_new, &P256_P);

    r->x = x_new;
    r->y = y_new;
    r->z = z_new;
    r->infinity = 0;
}

static void ec_point_add(ec_point_jac *r, const ec_point_jac *p, const ec_point_jac *q)
{
    if (p->infinity) { *r = *q; return; }
    if (q->infinity) { *r = *p; return; }

    u256 z1z1, z2z2, u1, u2, s1, s2, h, i, j, r_val, v, t;
    mod_sqr(&z1z1, &p->z, &P256_P);
    mod_sqr(&z2z2, &q->z, &P256_P);

    mod_mul(&u1, &p->x, &z2z2, &P256_P);
    mod_mul(&u2, &q->x, &z1z1, &P256_P);

    mod_mul(&t, &q->z, &z2z2, &P256_P);
    mod_mul(&s1, &p->y, &t, &P256_P);

    mod_mul(&t, &p->z, &z1z1, &P256_P);
    mod_mul(&s2, &q->y, &t, &P256_P);

    if (u256_cmp(&u1, &u2) == 0) {
        if (u256_cmp(&s1, &s2) == 0) {
            ec_point_double(r, p);
        } else {
            memset(r, 0, sizeof(*r));
            r->infinity = 1;
        }
        return;
    }

    mod_sub(&h, &u2, &u1, &P256_P);
    mod_add(&i, &h, &h, &P256_P);
    mod_sqr(&i, &i, &P256_P);
    mod_mul(&j, &h, &i, &P256_P);

    mod_sub(&r_val, &s2, &s1, &P256_P);
    mod_add(&r_val, &r_val, &r_val, &P256_P);

    mod_mul(&v, &u1, &i, &P256_P);

    /* X3 = r^2 - J - 2*V */
    u256 x3, y3, z3;
    mod_sqr(&x3, &r_val, &P256_P);
    mod_sub(&x3, &x3, &j, &P256_P);
    mod_sub(&x3, &x3, &v, &P256_P);
    mod_sub(&x3, &x3, &v, &P256_P);

    /* Y3 = r*(V - X3) - 2*S1*J */
    mod_sub(&t, &v, &x3, &P256_P);
    mod_mul(&y3, &r_val, &t, &P256_P);
    mod_mul(&t, &s1, &j, &P256_P);
    mod_add(&t, &t, &t, &P256_P);
    mod_sub(&y3, &y3, &t, &P256_P);

    /* Z3 = ((Z1 + Z2)^2 - Z1Z1 - Z2Z2)*H */
    mod_add(&z3, &p->z, &q->z, &P256_P);
    mod_sqr(&z3, &z3, &P256_P);
    mod_sub(&z3, &z3, &z1z1, &P256_P);
    mod_sub(&z3, &z3, &z2z2, &P256_P);
    mod_mul(&z3, &z3, &h, &P256_P);

    r->x = x3;
    r->y = y3;
    r->z = z3;
    r->infinity = 0;
}

static void ec_scalar_mul(ec_point_jac *out, const u256 *k, const ec_point_jac *pt)
{
    ec_point_jac acc;
    memset(&acc, 0, sizeof(acc));
    acc.infinity = 1;

    for (int bit = 255; bit >= 0; --bit) {
        ec_point_double(&acc, &acc);
        if ((k->d[bit >> 5] >> (bit & 31)) & 1u) {
            ec_point_add(&acc, &acc, pt);
        }
    }
    *out = acc;
}

static int ec_is_on_curve(const u256 *x, const u256 *y)
{
    if (u256_cmp(x, &P256_P) >= 0 || u256_cmp(y, &P256_P) >= 0) {
        return 0;
    }
    u256 lhs, rhs, x3, three_x;
    mod_sqr(&lhs, y, &P256_P);

    mod_sqr(&x3, x, &P256_P);
    mod_mul(&x3, &x3, x, &P256_P);

    mod_add(&three_x, x, x, &P256_P);
    mod_add(&three_x, &three_x, x, &P256_P);

    mod_sub(&rhs, &x3, &three_x, &P256_P);
    mod_add(&rhs, &rhs, &P256_B, &P256_P);

    return u256_cmp(&lhs, &rhs) == 0;
}

int ami_ecdsa_p256_verify(const uint8_t pub_x[32],
                          const uint8_t pub_y[32],
                          const uint8_t hash[32],
                          const uint8_t sig[64])
{
    if (pub_x == NULL || pub_y == NULL || hash == NULL || sig == NULL) {
        return -1;
    }

    u256 qx, qy, r, s, z;
    u256_from_be(&qx, pub_x);
    u256_from_be(&qy, pub_y);
    u256_from_be(&r, sig);
    u256_from_be(&s, sig + 32);
    u256_from_be(&z, hash);

    /* Validate 1 <= r < n and 1 <= s < n */
    if (u256_is_zero(&r) || u256_cmp(&r, &P256_N) >= 0) return -2;
    if (u256_is_zero(&s) || u256_cmp(&s, &P256_N) >= 0) return -2;

    /* Validate public key Q lies on secp256r1 */
    if (!ec_is_on_curve(&qx, &qy)) return -3;

    /* Reduce z mod n if z >= n */
    if (u256_cmp(&z, &P256_N) >= 0) {
        u256_sub_raw(&z, &z, &P256_N);
    }

    u256 w, u1, u2;
    mod_inv(&w, &s, &P256_N);
    mod_mul(&u1, &z, &w, &P256_N);
    mod_mul(&u2, &r, &w, &P256_N);

    ec_point_jac G = { P256_GX, P256_GY, {{1u, 0, 0, 0, 0, 0, 0, 0}}, 0 };
    ec_point_jac Q = { qx, qy, {{1u, 0, 0, 0, 0, 0, 0, 0}}, 0 };
    ec_point_jac p1, p2, R;

    ec_scalar_mul(&p1, &u1, &G);
    ec_scalar_mul(&p2, &u2, &Q);
    ec_point_add(&R, &p1, &p2);

    if (R.infinity) {
        return -4;
    }

    /* Convert R.x from Jacobian (X / Z^2 mod p) */
    u256 z2, z2_inv, rx;
    mod_sqr(&z2, &R.z, &P256_P);
    mod_inv(&z2_inv, &z2, &P256_P);
    mod_mul(&rx, &R.x, &z2_inv, &P256_P);

    if (u256_cmp(&rx, &P256_N) >= 0) {
        u256_sub_raw(&rx, &rx, &P256_N);
    }

    return (u256_cmp(&rx, &r) == 0) ? 0 : -5;
}

#ifdef AMI_BOOT_CRYPTO_ENABLE_SIGN
int ami_ecdsa_p256_pubkey_from_priv(const uint8_t priv[32],
                                    uint8_t pub_x[32],
                                    uint8_t pub_y[32])
{
    if (priv == NULL || pub_x == NULL || pub_y == NULL) return -1;
    u256 d;
    u256_from_be(&d, priv);
    if (u256_is_zero(&d) || u256_cmp(&d, &P256_N) >= 0) return -2;

    ec_point_jac G = { P256_GX, P256_GY, {{1u, 0, 0, 0, 0, 0, 0, 0}}, 0 };
    ec_point_jac Q;
    ec_scalar_mul(&Q, &d, &G);
    if (Q.infinity) return -3;

    u256 z_inv, z2_inv, z3_inv, x, y;
    mod_inv(&z_inv, &Q.z, &P256_P);
    mod_sqr(&z2_inv, &z_inv, &P256_P);
    mod_mul(&z3_inv, &z2_inv, &z_inv, &P256_P);
    mod_mul(&x, &Q.x, &z2_inv, &P256_P);
    mod_mul(&y, &Q.y, &z3_inv, &P256_P);

    u256_to_be(pub_x, &x);
    u256_to_be(pub_y, &y);
    return 0;
}

int ami_ecdsa_p256_sign_deterministic(const uint8_t priv[32],
                                      const uint8_t hash[32],
                                      const uint8_t k_nonce[32],
                                      uint8_t out_sig[64])
{
    if (priv == NULL || hash == NULL || k_nonce == NULL || out_sig == NULL) return -1;
    u256 d, k, z;
    u256_from_be(&d, priv);
    u256_from_be(&k, k_nonce);
    u256_from_be(&z, hash);

    if (u256_is_zero(&d) || u256_cmp(&d, &P256_N) >= 0) return -2;
    if (u256_is_zero(&k) || u256_cmp(&k, &P256_N) >= 0) return -2;
    if (u256_cmp(&z, &P256_N) >= 0) u256_sub_raw(&z, &z, &P256_N);

    ec_point_jac G = { P256_GX, P256_GY, {{1u, 0, 0, 0, 0, 0, 0, 0}}, 0 };
    ec_point_jac R;
    ec_scalar_mul(&R, &k, &G);
    if (R.infinity) return -3;

    u256 z_inv, z2_inv, r;
    mod_inv(&z_inv, &R.z, &P256_P);
    mod_sqr(&z2_inv, &z_inv, &P256_P);
    mod_mul(&r, &R.x, &z2_inv, &P256_P);
    if (u256_cmp(&r, &P256_N) >= 0) u256_sub_raw(&r, &r, &P256_N);
    if (u256_is_zero(&r)) return -4;

    u256 k_inv, rd, s;
    mod_inv(&k_inv, &k, &P256_N);
    mod_mul(&rd, &r, &d, &P256_N);
    mod_add(&s, &z, &rd, &P256_N);
    mod_mul(&s, &k_inv, &s, &P256_N);
    if (u256_is_zero(&s)) return -4;

    u256_to_be(out_sig, &r);
    u256_to_be(out_sig + 32, &s);
    return 0;
}
#endif /* AMI_BOOT_CRYPTO_ENABLE_SIGN */
