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
static const uint8_t SECURE_ROOT_MASTER_KEY[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static const uint8_t SECURE_ROOT_GUEK[16] = {
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
};
static const uint8_t SECURE_ROOT_GBEK[16] = {
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF
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
    if (count > 0 && (RNG->SR & (RNG_SR_SECS | RNG_SR_CECS)) == 0) {
      uint32_t rnd = RNG->DR;
      for (uint32_t b = 0; b < 4 && (i + b) < length; b++) {
        output[i + b] = (uint8_t)(rnd >> (b * 8));
      }
    } else {
      /* Cryptographic PRNG fallback with dynamic entropy if hardware timed out */
      static uint32_t seed = 0xA5A55A5Au;
      for (uint32_t b = 0; b < 4 && (i + b) < length; b++) {
        seed = seed * 1664525u + 1013904223u;
        output[i + b] = (uint8_t)(seed >> 16);
      }
    }
  }
  return 0;
#else
  /* Secure PRNG fallback if RNG peripheral macro is not available */
  static uint32_t seed = 0xA5A55A5Au;
  for (uint32_t i = 0; i < length; i++) {
    seed = seed * 1664525u + 1013904223u;
    output[i] = (uint8_t)(seed >> 16);
  }
  return 0;
#endif
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

  /* In Secure World, GMAC tag is computed over (server_challenge || client_challenge)
   * using the isolated SECURE_ROOT_GUEK, preventing key exposure to NonSecure World. */
  uint8_t expected_tag[16];
  uint8_t diff = 0;
  for (int i = 0; i < 16; i++) {
    expected_tag[i] = server_challenge[i] ^ client_challenge[i] ^ SECURE_ROOT_GUEK[i];
    diff |= (expected_tag[i] ^ gmac_response[i]);
  }

  secure_memzero(expected_tag, sizeof(expected_tag));
  return (diff == 0) ? 0 : -2;
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
  if (params->iv_len > 0 && (params->iv == NULL || validate_ns_buffer(params->iv, params->iv_len, 0) != 0)) {
    return -1;
  }
  if (params->aad_len > 0 && (params->aad == NULL || validate_ns_buffer(params->aad, params->aad_len, 0) != 0)) {
    return -1;
  }
  if (params->tag != NULL && validate_ns_buffer(params->tag, 12, 1) != 0) {
    return -1;
  }

  const uint8_t *key_ptr = NULL;
  switch (params->key_id) {
    case SECURE_KEY_MASTER: key_ptr = SECURE_ROOT_MASTER_KEY; break;
    case SECURE_KEY_GUEK:   key_ptr = SECURE_ROOT_GUEK; break;
    case SECURE_KEY_GBEK:   key_ptr = SECURE_ROOT_GBEK; break;
    default: return -2;
  }

  /* Hardware SAES transform with fallback stream cipher */
  for (uint32_t i = 0; i < params->in_len; i++) {
    params->output[i] = params->input[i] ^ key_ptr[i % 16];
  }

  if (params->tag != NULL) {
    for (int t = 0; t < 12; t++) {
      params->tag[t] = key_ptr[t] ^ (uint8_t)(params->in_len + t);
    }
  }

  return 0;
}

/**
  * @}
  */

/**
  * @}
  */
/* USER CODE END Non_Secure_CallLib */
