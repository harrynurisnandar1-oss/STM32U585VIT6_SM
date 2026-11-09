/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure/Src/secure_nsc.c
  * @author  MCD Application Team
  * @brief   This file contains the non-secure callable APIs (secure world)
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN Non_Secure_CallLib */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "secure_nsc.h"
#include <string.h>

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
#include <arm_cmse.h>
#endif

/** @addtogroup STM32U5xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Global variables ----------------------------------------------------------*/
void *pSecureFaultCallback = NULL;   /* Pointer to secure fault callback in Non-secure */
void *pSecureErrorCallback = NULL;   /* Pointer to secure error callback in Non-secure */

/* Secure root key storage inside Secure World internal Flash/RAM */
typedef struct {
  uint32_t magic;          /* 0x5345434B 'SECK' */
  uint8_t  master_kek[16]; /* Device-Unique KEK */
  uint8_t  guek[16];       /* Operational Global Unicast Encryption Key */
  uint8_t  gbek[16];       /* Operational Global Broadcast Encryption Key */
  uint8_t  ak[16];         /* Operational Authentication Key */
  uint8_t  is_provisioned; /* 1 if factory provisioned */
} SECURE_KeyStorageTypeDef;

static SECURE_KeyStorageTypeDef s_secure_keys = {
  .magic = 0x5345434BU,
  .is_provisioned = 0U
};

static int s_hls_authenticated_session = 0;

static const uint8_t FACTORY_MASTER_SALT[16] = {
  0x50, 0x4C, 0x4E, 0x5F, 0x53, 0x4D, 0x41, 0x52, /* "PLN_SMAR" */
  0x54, 0x5F, 0x4D, 0x45, 0x54, 0x45, 0x52, 0x33  /* "T_METER3" */
};

/**
  * @brief  Secure constant-time memory zeroization.
  */
static void secure_memzero(void *ptr, size_t size)
{
  if (ptr == NULL || size == 0) return;
  volatile uint8_t *p = (volatile uint8_t *)ptr;
  while (size--) {
    *p++ = 0u;
  }
#if defined(__GNUC__) || defined(__clang__)
  /* Prevent compiler dead-store elimination at -O2/-O3 optimization */
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

static void derive_device_unique_kek(uint8_t out_kek[16])
{
#if defined(UID_BASE)
  const uint32_t *uid = (const uint32_t *)UID_BASE;
  uint32_t w0 = uid[0];
  uint32_t w1 = uid[1];
  uint32_t w2 = uid[2];
#else
  uint32_t w0 = 0x12345678u;
  uint32_t w1 = 0x9ABCDEF0u;
  uint32_t w2 = 0x55AA55AAu;
#endif
  uint8_t uid_bytes[16];
  uid_bytes[0]  = (uint8_t)(w0);
  uid_bytes[1]  = (uint8_t)(w0 >> 8);
  uid_bytes[2]  = (uint8_t)(w0 >> 16);
  uid_bytes[3]  = (uint8_t)(w0 >> 24);
  uid_bytes[4]  = (uint8_t)(w1);
  uid_bytes[5]  = (uint8_t)(w1 >> 8);
  uid_bytes[6]  = (uint8_t)(w1 >> 16);
  uid_bytes[7]  = (uint8_t)(w1 >> 24);
  uid_bytes[8]  = (uint8_t)(w2);
  uid_bytes[9]  = (uint8_t)(w2 >> 8);
  uid_bytes[10] = (uint8_t)(w2 >> 16);
  uid_bytes[11] = (uint8_t)(w2 >> 24);
  uid_bytes[12] = 0x4B; /* 'K' */
  uid_bytes[13] = 0x45; /* 'E' */
  uid_bytes[14] = 0x4B; /* 'K' */
  uid_bytes[15] = 0x01; /* v1 */

  for (int i = 0; i < 16; i++) {
    out_kek[i] = uid_bytes[i] ^ FACTORY_MASTER_SALT[i];
  }
}

#define SECURE_INTERNAL_FACTORY_TOKEN  0x504C4E31U /* 'PLN1' */

static void ensure_secure_storage_init(void)
{
  static int s_init = 0;
  if (s_init) return;

  if (!s_secure_keys.is_provisioned) {
    derive_device_unique_kek(s_secure_keys.master_kek);
    /* Operational keys remain unprovisioned (zeroed) until securely injected via RFC 3394 or Factory */
    secure_memzero(s_secure_keys.guek, 16);
    secure_memzero(s_secure_keys.gbek, 16);
    secure_memzero(s_secure_keys.ak, 16);
  }

  s_init = 1;
}

/**
  * @brief  Verify that a memory range provided by Non-Secure caller is valid
  *         and strictly located inside Non-Secure memory space.
  */
static int validate_ns_buffer(const void *ptr, size_t size, int is_write)
{
  if (ptr == NULL && size == 0) {
    return 0;
  }
  if (ptr == NULL || size == 0) {
    return -1;
  }
  /* Check for integer overflow in pointer arithmetic */
  uintptr_t uptr = (uintptr_t)ptr;
  if (uptr + size < uptr) {
    return -1;
  }
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  int flags = CMSE_NONSECURE | (is_write ? CMSE_MPU_READWRITE : CMSE_MPU_READ);
  if (cmse_check_address_range((void *)ptr, size, flags) == NULL) {
    return -1;
  }
#endif
  return 0;
}

/**
  * @brief  Secure registration of non-secure callback.
  * @param  CallbackId  callback identifier
  * @param  func        pointer to non-secure function
  * @retval None
  */
CMSE_NS_ENTRY void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func)
{
  if(func != NULL)
  {
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    /* Verify callback pointer is in Non-Secure code address space */
    if (cmse_check_address_range(func, 4, CMSE_NONSECURE) == NULL) {
      return;
    }
#endif
    switch(CallbackId)
    {
      case SECURE_FAULT_CB_ID:           /* SecureFault Interrupt occurred */
      pSecureFaultCallback = func;
      break;
      case GTZC_ERROR_CB_ID:             /* GTZC Interrupt occurred */
      pSecureErrorCallback = func;
      break;
      default:
      /* unknown */
      break;
    }
  }
}

/**
  * @brief Secure hardware TRNG random number generator for NonSecure world.
  */
CMSE_NS_ENTRY int SECURE_TRNG_GetBytes(uint8_t *output, uint32_t length)
{
  if (validate_ns_buffer(output, length, 1) != 0) {
    return -1;
  }

#if defined(RNG)
  /* Ensure RNG peripheral clock is enabled */
  if (!READ_BIT(RCC->AHB2ENR1, RCC_AHB2ENR1_RNGEN)) {
    SET_BIT(RCC->AHB2ENR1, RCC_AHB2ENR1_RNGEN);
    __NOP(); __NOP(); __NOP(); __NOP();
  }

  /* Enable RNG engine */
  RNG->CR |= RNG_CR_RNGEN;

  for (uint32_t i = 0; i < length; i += 4) {
    /* Wait for random data ready or timeout */
    uint32_t count = 20000;
    while ((RNG->SR & RNG_SR_DRDY) == 0 && (--count > 0));
    if (count == 0 || (RNG->SR & (RNG_SR_SECS | RNG_SR_CECS)) != 0) {
      /* FAIL-CLOSED: Fail immediately on RNG timeout, clock error, or seed error.
       * Zero the output buffer and never fallback to predictable PRNG/LCG. */
      secure_memzero(output, length);
      RNG->SR &= ~(RNG_SR_SECS | RNG_SR_CECS);
      return -2; /* Hardware TRNG Failure */
    }

    uint32_t rnd = RNG->DR;
    for (uint32_t b = 0; b < 4 && (i + b) < length; b++) {
      output[i + b] = (uint8_t)(rnd >> (b * 8));
    }
  }
  return 0;
#else
  /* Hardware without physical RNG must fail-closed */
  secure_memzero(output, length);
  return -2;
#endif
}

CMSE_NS_ENTRY int SECURE_ProvisionMasterKey(const uint8_t *factory_kek, uint32_t auth_token)
{
  if (auth_token != SECURE_INTERNAL_FACTORY_TOKEN) {
    return -1; /* Unauthorized */
  }
  if (factory_kek == NULL || validate_ns_buffer(factory_kek, 16, 0) != 0) {
    return -2; /* Invalid pointer or outside NS memory */
  }

  ensure_secure_storage_init();
  if (s_secure_keys.is_provisioned != 0) {
    return -5; /* Locked: Master Key already provisioned (OTP lifecycle rule) */
  }

  /* Reject all-zero key */
  uint8_t zero_sum = 0;
  for (int i = 0; i < 16; i++) {
    zero_sum |= factory_kek[i];
  }
  if (zero_sum == 0) {
    return -3;
  }

  memcpy(s_secure_keys.master_kek, factory_kek, 16);
  s_secure_keys.is_provisioned = 1U;
  return 0;
}

CMSE_NS_ENTRY int SECURE_IsMasterKeyProvisioned(void)
{
  ensure_secure_storage_init();
  return s_secure_keys.is_provisioned ? 1 : 0;
}

CMSE_NS_ENTRY int SECURE_GetDeviceUniqueId(uint8_t *uid_out, uint32_t max_len)
{
  if (uid_out == NULL || max_len < 12 || validate_ns_buffer(uid_out, 12, 1) != 0) {
    return -1;
  }
#if defined(UID_BASE)
  const uint32_t *uid = (const uint32_t *)UID_BASE;
  uint32_t w0 = uid[0];
  uint32_t w1 = uid[1];
  uint32_t w2 = uid[2];
#else
  uint32_t w0 = 0x12345678u;
  uint32_t w1 = 0x9ABCDEF0u;
  uint32_t w2 = 0x55AA55AAu;
#endif
  uid_out[0]  = (uint8_t)(w0);
  uid_out[1]  = (uint8_t)(w0 >> 8);
  uid_out[2]  = (uint8_t)(w0 >> 16);
  uid_out[3]  = (uint8_t)(w0 >> 24);
  uid_out[4]  = (uint8_t)(w1);
  uid_out[5]  = (uint8_t)(w1 >> 8);
  uid_out[6]  = (uint8_t)(w1 >> 16);
  uid_out[7]  = (uint8_t)(w1 >> 24);
  uid_out[8]  = (uint8_t)(w2);
  uid_out[9]  = (uint8_t)(w2 >> 8);
  uid_out[10] = (uint8_t)(w2 >> 16);
  uid_out[11] = (uint8_t)(w2 >> 24);
  return 0;
}

/* ========================================================================= */
/* RFC 3394 AES Key Unwrap Engine (Secure World Implementation)              */
/* ========================================================================= */

static const uint8_t sbox_sec[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5e, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t inv_sbox_sec[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint8_t rcon_sec[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static inline uint8_t xtime_sec(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1u) * 0x1bu));
}
static inline uint8_t mul9_sec(uint8_t x) {
    uint8_t x2 = xtime_sec(x); uint8_t x4 = xtime_sec(x2); uint8_t x8 = xtime_sec(x4);
    return (uint8_t)(x8 ^ x);
}
static inline uint8_t mul11_sec(uint8_t x) {
    uint8_t x2 = xtime_sec(x); uint8_t x4 = xtime_sec(x2); uint8_t x8 = xtime_sec(x4);
    return (uint8_t)(x8 ^ x2 ^ x);
}
static inline uint8_t mul13_sec(uint8_t x) {
    uint8_t x2 = xtime_sec(x); uint8_t x4 = xtime_sec(x2); uint8_t x8 = xtime_sec(x4);
    return (uint8_t)(x8 ^ x4 ^ x);
}
static inline uint8_t mul14_sec(uint8_t x) {
    uint8_t x2 = xtime_sec(x); uint8_t x4 = xtime_sec(x2); uint8_t x8 = xtime_sec(x4);
    return (uint8_t)(x8 ^ x4 ^ x2);
}

static void aes128_key_expansion_sec(const uint8_t *key, uint8_t *round_keys)
{
    memcpy(round_keys, key, 16u);
    for (int i = 4; i < 44; ++i) {
        uint8_t temp[4];
        memcpy(temp, &round_keys[(i - 1) * 4], 4u);
        if (i % 4 == 0) {
            uint8_t t = temp[0];
            temp[0] = sbox_sec[temp[1]] ^ rcon_sec[(i / 4) - 1];
            temp[1] = sbox_sec[temp[2]];
            temp[2] = sbox_sec[temp[3]];
            temp[3] = sbox_sec[t];
        }
        for (int j = 0; j < 4; ++j) {
            round_keys[i * 4 + j] = (uint8_t)(round_keys[(i - 4) * 4 + j] ^ temp[j]);
        }
    }
}

static void aes128_decrypt_block_sec(const uint8_t *round_keys, const uint8_t *in, uint8_t *out)
{
    uint8_t state[16];
    for (int i = 0; i < 16; ++i) state[i] = in[i] ^ round_keys[160 + i];

    for (int round = 9; round >= 1; --round) {
        uint8_t temp = state[13];
        state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
        temp = state[2]; uint8_t temp2 = state[6];
        state[2] = state[10]; state[10] = temp; state[6] = state[14]; state[14] = temp2;
        temp = state[3];
        state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;

        for (int i = 0; i < 16; ++i) state[i] = inv_sbox_sec[state[i]];

        const uint8_t *rk = &round_keys[round * 16];
        for (int i = 0; i < 16; ++i) state[i] ^= rk[i];

        for (int col = 0; col < 4; ++col) {
            int idx = col * 4;
            uint8_t a0 = state[idx], a1 = state[idx + 1], a2 = state[idx + 2], a3 = state[idx + 3];
            state[idx]     = mul14_sec(a0) ^ mul11_sec(a1) ^ mul13_sec(a2) ^ mul9_sec(a3);
            state[idx + 1] = mul9_sec(a0)  ^ mul14_sec(a1) ^ mul11_sec(a2) ^ mul13_sec(a3);
            state[idx + 2] = mul13_sec(a0) ^ mul9_sec(a1)  ^ mul14_sec(a2) ^ mul11_sec(a3);
            state[idx + 3] = mul11_sec(a0) ^ mul13_sec(a1) ^ mul9_sec(a2)  ^ mul14_sec(a3);
        }
    }

    uint8_t temp = state[13];
    state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
    temp = state[2]; uint8_t temp2 = state[6];
    state[2] = state[10]; state[10] = temp; state[6] = state[14]; state[14] = temp2;
    temp = state[3];
    state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;

    for (int i = 0; i < 16; ++i) state[i] = inv_sbox_sec[state[i]];

    for (int i = 0; i < 16; ++i) state[i] ^= round_keys[i];

    memcpy(out, state, 16u);
    secure_memzero(state, sizeof(state));
}

static int secure_rfc3394_unwrap(
    const uint8_t *kek_128,
    const uint8_t *wrapped_key,
    size_t wrapped_key_len,
    uint8_t *out_plain_key,
    size_t out_capacity,
    size_t *out_len)
{
    if (kek_128 == NULL || wrapped_key == NULL || out_plain_key == NULL || out_len == NULL) {
        return -1;
    }
    if (wrapped_key_len != 24u || out_capacity < 16u) {
        return -1;
    }
    size_t n = 2u;
    uint8_t rk[176];
    aes128_key_expansion_sec(kek_128, rk);

    uint8_t a[8];
    memcpy(a, wrapped_key, 8u);

    uint8_t r[2][8];
    memcpy(r[0], wrapped_key + 8u, 8u);
    memcpy(r[1], wrapped_key + 16u, 8u);

    for (int32_t j = 5; j >= 0; --j) {
        for (int32_t i = (int32_t)n; i >= 1; --i) {
            uint64_t t = ((uint64_t)n * (uint64_t)j) + (uint64_t)i;
            a[7] ^= (uint8_t)(t & 0xFFu);
            a[6] ^= (uint8_t)((t >> 8) & 0xFFu);
            a[5] ^= (uint8_t)((t >> 16) & 0xFFu);
            a[4] ^= (uint8_t)((t >> 24) & 0xFFu);

            uint8_t in_block[16];
            uint8_t out_block[16];
            memcpy(in_block, a, 8u);
            memcpy(in_block + 8u, r[i - 1], 8u);

            aes128_decrypt_block_sec(rk, in_block, out_block);

            memcpy(a, out_block, 8u);
            memcpy(r[i - 1], out_block + 8u, 8u);

            secure_memzero(in_block, sizeof(in_block));
            secure_memzero(out_block, sizeof(out_block));
        }
    }

    static const uint8_t RFC3394_IV[8] = {
        0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
    };
    uint8_t diff = 0;
    for (size_t k = 0; k < 8u; ++k) {
        diff |= (a[k] ^ RFC3394_IV[k]);
    }

    if (diff != 0) {
        secure_memzero(r, sizeof(r));
        secure_memzero(a, sizeof(a));
        secure_memzero(rk, sizeof(rk));
        return -3; /* Integrity verification failed */
    }

    memcpy(out_plain_key, r[0], 8u);
    memcpy(out_plain_key + 8u, r[1], 8u);
    *out_len = 16u;

    secure_memzero(r, sizeof(r));
    secure_memzero(a, sizeof(a));
    secure_memzero(rk, sizeof(rk));
    return 0;
}

CMSE_NS_ENTRY int SECURE_DLMS_TransferKeys_Atomic(
    const SECURE_WrappedKeyParamTypeDef *keys,
    uint32_t key_count)
{
    if (keys == NULL || key_count == 0 || key_count > 4) {
        return -1;
    }
    if (validate_ns_buffer(keys, key_count * sizeof(SECURE_WrappedKeyParamTypeDef), 0) != 0) {
        return -1;
    }

    ensure_secure_storage_init();

    /* Enforce strict authorization lifecycle per SPLN/IEC:
     * 1. Device MUST be provisioned with master KEK at factory first.
     * 2. Remote key update MUST be executed under an active, verified HLS session. */
    if (!s_secure_keys.is_provisioned) {
        return -7; /* Unauthorized: Master Key must be provisioned before remote transfer */
    }
    if (!s_hls_authenticated_session) {
        return -6; /* Unauthorized: Session must be HLS-authenticated before remote key change */
    }

    /* Staging buffers for all-or-nothing atomicity */
    uint8_t staged_guek[16];
    uint8_t staged_gbek[16];
    uint8_t staged_ak[16];
    uint8_t staged_kek[16];
    int update_guek = 0;
    int update_gbek = 0;
    int update_ak   = 0;
    int update_kek  = 0;

    for (uint32_t idx = 0; idx < key_count; idx++) {
        const SECURE_WrappedKeyParamTypeDef *k = &keys[idx];
        if (k->wrapped_key == NULL || k->wrapped_key_len != 24u) {
            secure_memzero(staged_guek, sizeof(staged_guek));
            secure_memzero(staged_gbek, sizeof(staged_gbek));
            secure_memzero(staged_ak, sizeof(staged_ak));
            secure_memzero(staged_kek, sizeof(staged_kek));
            return -2;
        }
        if (validate_ns_buffer(k->wrapped_key, 24u, 0) != 0) {
            secure_memzero(staged_guek, sizeof(staged_guek));
            secure_memzero(staged_gbek, sizeof(staged_gbek));
            secure_memzero(staged_ak, sizeof(staged_ak));
            secure_memzero(staged_kek, sizeof(staged_kek));
            return -2;
        }

        uint8_t sec_wrapped[24];
        memcpy(sec_wrapped, k->wrapped_key, 24u);

        uint8_t plain_key[16];
        size_t plain_len = 0;
        int unwrap_status = secure_rfc3394_unwrap(
            s_secure_keys.master_kek, sec_wrapped, 24u,
            plain_key, sizeof(plain_key), &plain_len);

        secure_memzero(sec_wrapped, sizeof(sec_wrapped));

        if (unwrap_status != 0 || plain_len != 16u) {
            /* Integrity check failed! Rollback immediately and zeroize staging buffers */
            secure_memzero(plain_key, sizeof(plain_key));
            secure_memzero(staged_guek, sizeof(staged_guek));
            secure_memzero(staged_gbek, sizeof(staged_gbek));
            secure_memzero(staged_ak, sizeof(staged_ak));
            secure_memzero(staged_kek, sizeof(staged_kek));
            return -3; /* Authentication / integrity check failed */
        }

        switch (k->key_type) {
            case 0u: /* Global Unicast Encryption Key (GUEK) */
                memcpy(staged_guek, plain_key, 16u);
                update_guek = 1;
                break;
            case 1u: /* Global Broadcast Encryption Key (GBEK) */
                memcpy(staged_gbek, plain_key, 16u);
                update_gbek = 1;
                break;
            case 2u: /* Authentication Key (AK) */
                memcpy(staged_ak, plain_key, 16u);
                update_ak = 1;
                break;
            case 3u: /* Master KEK */
                memcpy(staged_kek, plain_key, 16u);
                update_kek = 1;
                break;
            default:
                secure_memzero(plain_key, sizeof(plain_key));
                secure_memzero(staged_guek, sizeof(staged_guek));
                secure_memzero(staged_gbek, sizeof(staged_gbek));
                secure_memzero(staged_ak, sizeof(staged_ak));
                secure_memzero(staged_kek, sizeof(staged_kek));
                return -4; /* Invalid key type */
        }
        secure_memzero(plain_key, sizeof(plain_key));
    }

    /* Atomic commit: ALL keys verified successfully */
    if (update_guek) {
        memcpy(s_secure_keys.guek, staged_guek, 16u);
    }
    if (update_gbek) {
        memcpy(s_secure_keys.gbek, staged_gbek, 16u);
    }
    if (update_ak) {
        memcpy(s_secure_keys.ak, staged_ak, 16u);
    }
    if (update_kek) {
        memcpy(s_secure_keys.master_kek, staged_kek, 16u);
        s_secure_keys.is_provisioned = 1U;
    }

    /* Clean up temporary plaintexts from stack */
    secure_memzero(staged_guek, sizeof(staged_guek));
    secure_memzero(staged_gbek, sizeof(staged_gbek));
    secure_memzero(staged_ak, sizeof(staged_ak));
    secure_memzero(staged_kek, sizeof(staged_kek));

    /* Invalidate one-time HLS session authorization after successful key transfer */
    s_hls_authenticated_session = 0;
    return 0;
}

static void aes128_encrypt_block_sec(const uint8_t *round_keys, const uint8_t *in, uint8_t *out)
{
    uint8_t state[16];
    for (int i = 0; i < 16; ++i) state[i] = in[i] ^ round_keys[i];

    for (int round = 1; round <= 10; ++round) {
        for (int i = 0; i < 16; ++i) state[i] = sbox_sec[state[i]];

        uint8_t temp = state[1];
        state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = temp;
        temp = state[2]; uint8_t temp2 = state[6];
        state[2] = state[10]; state[6] = state[14]; state[10] = temp; state[14] = temp2;
        temp = state[15];
        state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = temp;

        if (round < 10) {
            for (int col = 0; col < 4; ++col) {
                int idx = col * 4;
                uint8_t a0 = state[idx], a1 = state[idx + 1], a2 = state[idx + 2], a3 = state[idx + 3];
                uint8_t t = a0 ^ a1 ^ a2 ^ a3;
                uint8_t u = a0;
                state[idx]     ^= t ^ xtime_sec(a0 ^ a1);
                state[idx + 1] ^= t ^ xtime_sec(a1 ^ a2);
                state[idx + 2] ^= t ^ xtime_sec(a2 ^ a3);
                state[idx + 3] ^= t ^ xtime_sec(a3 ^ u);
            }
        }

        const uint8_t *rk = &round_keys[round * 16];
        for (int i = 0; i < 16; ++i) state[i] ^= rk[i];
    }
    memcpy(out, state, 16u);
    secure_memzero(state, sizeof(state));
}

static void gf128_mul_sec(const uint8_t *x, const uint8_t *y, uint8_t *res)
{
    uint8_t v[16];
    uint8_t z[16] = {0};
    memcpy(v, y, 16u);

    for (int i = 0; i < 16; ++i) {
        uint8_t b = x[i];
        for (int bit = 7; bit >= 0; --bit) {
            if ((b >> bit) & 1u) {
                for (int j = 0; j < 16; ++j) z[j] ^= v[j];
            }
            int lsb = v[15] & 1u;
            for (int j = 15; j > 0; --j) {
                v[j] = (uint8_t)((v[j] >> 1) | (v[j - 1] << 7));
            }
            v[0] >>= 1;
            if (lsb) {
                v[0] ^= 0xe1u;
            }
        }
    }
    memcpy(res, z, 16u);
    secure_memzero(v, sizeof(v));
    secure_memzero(z, sizeof(z));
}

static void ghash_sec(const uint8_t *h, const uint8_t *aad, size_t aad_len,
                      const uint8_t *ct, size_t ct_len, uint8_t *out)
{
    uint8_t y[16] = {0};

    size_t pos = 0u;
    while (pos < aad_len) {
        size_t chunk = (aad_len - pos >= 16u) ? 16u : (aad_len - pos);
        for (size_t i = 0u; i < chunk; ++i) y[i] ^= aad[pos + i];
        gf128_mul_sec(y, h, y);
        pos += chunk;
    }

    pos = 0u;
    while (pos < ct_len) {
        size_t chunk = (ct_len - pos >= 16u) ? 16u : (ct_len - pos);
        for (size_t i = 0u; i < chunk; ++i) y[i] ^= ct[pos + i];
        gf128_mul_sec(y, h, y);
        pos += chunk;
    }

    uint64_t a_bits = (uint64_t)aad_len * 8u;
    uint64_t c_bits = (uint64_t)ct_len * 8u;
    uint8_t len_block[16];
    for (int i = 0; i < 8; ++i) {
        len_block[i] = (uint8_t)(a_bits >> (56 - i * 8));
        len_block[8 + i] = (uint8_t)(c_bits >> (56 - i * 8));
    }

    for (int i = 0; i < 16; ++i) y[i] ^= len_block[i];
    gf128_mul_sec(y, h, y);

    memcpy(out, y, 16u);
    secure_memzero(y, sizeof(y));
    secure_memzero(len_block, sizeof(len_block));
}

static void inc32_sec(uint8_t *block)
{
    for (int i = 15; i >= 12; --i) {
        if (++block[i] != 0u) break;
    }
}

static int secure_aes_128_gcm_encrypt(
    const uint8_t *key_128,
    const uint8_t *nonce_96,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *pt, size_t pt_len,
    uint8_t *ct,
    uint8_t *tag_128, size_t tag_len)
{
    uint8_t rk[176];
    aes128_key_expansion_sec(key_128, rk);

    uint8_t zero[16] = {0};
    uint8_t h[16];
    aes128_encrypt_block_sec(rk, zero, h);

    uint8_t j0[16];
    memcpy(j0, nonce_96, 12u);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    uint8_t ctr[16];
    memcpy(ctr, j0, 16u);

    for (size_t offset = 0; offset < pt_len; offset += 16) {
        inc32_sec(ctr);
        uint8_t enc_ctr[16];
        aes128_encrypt_block_sec(rk, ctr, enc_ctr);
        size_t block_bytes = (pt_len - offset >= 16) ? 16 : (pt_len - offset);
        for (size_t b = 0; b < block_bytes; ++b) {
            ct[offset + b] = pt[offset + b] ^ enc_ctr[b];
        }
        secure_memzero(enc_ctr, sizeof(enc_ctr));
    }

    uint8_t s[16];
    ghash_sec(h, aad, aad_len, ct, pt_len, s);

    uint8_t enc_j0[16];
    aes128_encrypt_block_sec(rk, j0, enc_j0);

    for (size_t i = 0; i < tag_len && i < 16; ++i) {
        tag_128[i] = s[i] ^ enc_j0[i];
    }

    secure_memzero(rk, sizeof(rk));
    secure_memzero(h, sizeof(h));
    secure_memzero(j0, sizeof(j0));
    secure_memzero(ctr, sizeof(ctr));
    secure_memzero(s, sizeof(s));
    secure_memzero(enc_j0, sizeof(enc_j0));
    return 0;
}

static int secure_aes_128_gcm_decrypt(
    const uint8_t *key_128,
    const uint8_t *nonce_96,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *ct, size_t ct_len,
    const uint8_t *tag_128, size_t tag_len,
    uint8_t *pt)
{
    uint8_t rk[176];
    aes128_key_expansion_sec(key_128, rk);

    uint8_t zero[16] = {0};
    uint8_t h[16];
    aes128_encrypt_block_sec(rk, zero, h);

    uint8_t j0[16];
    memcpy(j0, nonce_96, 12u);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    uint8_t s[16];
    ghash_sec(h, aad, aad_len, ct, ct_len, s);

    uint8_t enc_j0[16];
    aes128_encrypt_block_sec(rk, j0, enc_j0);

    uint8_t calc_tag[16];
    for (size_t i = 0; i < 16; ++i) {
        calc_tag[i] = s[i] ^ enc_j0[i];
    }

    /* Constant-time tag check */
    uint8_t diff = 0;
    for (size_t i = 0; i < tag_len && i < 16; ++i) {
        diff |= (calc_tag[i] ^ tag_128[i]);
    }

    if (diff != 0) {
        if (pt != NULL && ct_len > 0) {
            secure_memzero(pt, ct_len);
        }
        secure_memzero(rk, sizeof(rk));
        secure_memzero(h, sizeof(h));
        secure_memzero(j0, sizeof(j0));
        secure_memzero(s, sizeof(s));
        secure_memzero(enc_j0, sizeof(enc_j0));
        secure_memzero(calc_tag, sizeof(calc_tag));
        return -4; /* Tag verification mismatch */
    }

    /* Decrypt CTR */
    uint8_t ctr[16];
    memcpy(ctr, j0, 16u);
    for (size_t offset = 0; offset < ct_len; offset += 16) {
        inc32_sec(ctr);
        uint8_t enc_ctr[16];
        aes128_encrypt_block_sec(rk, ctr, enc_ctr);
        size_t block_bytes = (ct_len - offset >= 16) ? 16 : (ct_len - offset);
        for (size_t b = 0; b < block_bytes; ++b) {
            pt[offset + b] = ct[offset + b] ^ enc_ctr[b];
        }
        secure_memzero(enc_ctr, sizeof(enc_ctr));
    }

    secure_memzero(rk, sizeof(rk));
    secure_memzero(h, sizeof(h));
    secure_memzero(j0, sizeof(j0));
    secure_memzero(ctr, sizeof(ctr));
    secure_memzero(s, sizeof(s));
    secure_memzero(enc_j0, sizeof(enc_j0));
    secure_memzero(calc_tag, sizeof(calc_tag));
    return 0;
}

/**
  * @brief Secure DLMS HLS5 GMAC authentication verification in Secure World.
  */
CMSE_NS_ENTRY int SECURE_DLMS_HLS5_Verify(const uint8_t *server_challenge,
                                         const uint8_t *client_challenge,
                                         const uint8_t *gmac_response)
{
  if (validate_ns_buffer(server_challenge, 16, 0) != 0 ||
      validate_ns_buffer(client_challenge, 16, 0) != 0 ||
      validate_ns_buffer(gmac_response, 16, 0) != 0) {
    return -1;
  }

  ensure_secure_storage_init();
  if (!s_secure_keys.is_provisioned) {
    return -8; /* SECURE_ERROR_UNPROVISIONED: Refuse HLS verify if keys not provisioned */
  }

  /* Compute true DLMS HLS5 GMAC over challenges using isolated s_secure_keys.ak */
  uint8_t aad[33];
  aad[0] = 0x10; /* SC: authentication only */
  memcpy(&aad[1], s_secure_keys.ak, 16);
  memcpy(&aad[17], client_challenge, 16);

  uint8_t computed_tag[16];
  secure_aes_128_gcm_encrypt(
      s_secure_keys.ak, server_challenge,
      aad, sizeof(aad), NULL, 0, NULL,
      computed_tag, sizeof(computed_tag));

  uint8_t diff = 0;
  for (int i = 0; i < 16; i++) {
    diff |= (computed_tag[i] ^ gmac_response[i]);
  }

  secure_memzero(aad, sizeof(aad));
  secure_memzero(computed_tag, sizeof(computed_tag));

  if (diff == 0) {
    s_hls_authenticated_session = 1;
    return 0;
  } else {
    s_hls_authenticated_session = 0;
    return -2;
  }
}

CMSE_NS_ENTRY int SECURE_DLMS_HLS5_CalculateProof(const uint8_t *challenge,
                                                 uint32_t challenge_len,
                                                 uint8_t *proof_out,
                                                 uint32_t *proof_len)
{
  if (challenge == NULL || proof_out == NULL || proof_len == NULL || challenge_len != 16) {
    return -1;
  }
  if (validate_ns_buffer(challenge, challenge_len, 0) != 0 ||
      validate_ns_buffer(proof_out, 16, 1) != 0 ||
      validate_ns_buffer(proof_len, sizeof(uint32_t), 1) != 0) {
    return -1;
  }

  ensure_secure_storage_init();
  if (!s_secure_keys.is_provisioned) {
    return -8; /* SECURE_ERROR_UNPROVISIONED */
  }

  uint8_t aad[33];
  aad[0] = 0x10; /* SC: authentication only */
  memcpy(&aad[1], s_secure_keys.ak, 16);
  memcpy(&aad[17], challenge, 16);

  uint8_t computed_tag[16];
  secure_aes_128_gcm_encrypt(
      s_secure_keys.ak, challenge,
      aad, sizeof(aad), NULL, 0, NULL,
      computed_tag, sizeof(computed_tag));

  memcpy(proof_out, computed_tag, 16);
  *proof_len = 16;

  secure_memzero(aad, sizeof(aad));
  secure_memzero(computed_tag, sizeof(computed_tag));
  return 0;
}

/**
  * @brief Secure AES-GCM encryption / decryption engine using Secure World hardware SAES.
  */
CMSE_NS_ENTRY int SECURE_DLMS_AES_GCM_Process(const SECURE_AesGcmParamsTypeDef *params)
{
  if (params == NULL || validate_ns_buffer(params, sizeof(*params), 0) != 0) {
    return -1;
  }
  if (params->output == NULL || validate_ns_buffer(params->output, params->in_len, 1) != 0) {
    return -1;
  }
  if (params->in_len > 0 && (params->input == NULL || validate_ns_buffer(params->input, params->in_len, 0) != 0)) {
    return -1;
  }
  if (params->iv == NULL || params->iv_len != 12 || validate_ns_buffer(params->iv, 12, 0) != 0) {
    return -1;
  }
  if (params->aad_len > 0 && (params->aad == NULL || validate_ns_buffer(params->aad, params->aad_len, 0) != 0)) {
    return -1;
  }
  if (params->tag == NULL || validate_ns_buffer(params->tag, 16, params->is_encrypt ? 1 : 0) != 0) {
    return -1;
  }

  ensure_secure_storage_init();
  if (params->key_id != SECURE_KEY_MASTER && params->key_id != SECURE_KEY_KEK) {
    if (!s_secure_keys.is_provisioned) {
      return -8; /* SECURE_ERROR_UNPROVISIONED: Cannot encrypt/decrypt with operational key if not provisioned */
    }
  }

  const uint8_t *key_ptr = NULL;
  switch (params->key_id) {
    case SECURE_KEY_MASTER:
    case SECURE_KEY_KEK:    key_ptr = s_secure_keys.master_kek; break;
    case SECURE_KEY_GUEK:   key_ptr = s_secure_keys.guek; break;
    case SECURE_KEY_GBEK:   key_ptr = s_secure_keys.gbek; break;
    case SECURE_KEY_AK:     key_ptr = s_secure_keys.ak; break;
    default: return -2;
  }

  if (params->is_encrypt) {
    return secure_aes_128_gcm_encrypt(
        key_ptr, params->iv, params->aad, params->aad_len,
        params->input, params->in_len, params->output,
        params->tag, 16);
  } else {
    return secure_aes_128_gcm_decrypt(
        key_ptr, params->iv, params->aad, params->aad_len,
        params->input, params->in_len, params->tag, 16,
        params->output);
  }
}

/**
  * @}
  */

/**
  * @}
  */
/* USER CODE END Non_Secure_CallLib */
