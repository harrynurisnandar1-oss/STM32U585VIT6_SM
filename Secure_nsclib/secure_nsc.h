/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure_nsclib/secure_nsc.h
  * @author  MCD Application Team
  * @brief   Header for secure non-secure callable APIs list
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

/* USER CODE BEGIN Non_Secure_CallLib_h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SECURE_NSC_H
#define SECURE_NSC_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

/* Exported types ------------------------------------------------------------*/
/**
  * @brief  non-secure callback ID enumeration definition
  */
typedef enum
{
  SECURE_FAULT_CB_ID     = 0x00U, /*!< System secure fault callback ID */
  GTZC_ERROR_CB_ID       = 0x01U  /*!< GTZC secure error callback ID */
} SECURE_CallbackIDTypeDef;

typedef enum
{
  SECURE_KEY_MASTER = 0x00U,
  SECURE_KEY_GUEK   = 0x01U,
  SECURE_KEY_GBEK   = 0x02U
} SECURE_KeyIdTypeDef;

typedef struct
{
  SECURE_KeyIdTypeDef key_id;
  uint8_t is_encrypt;
  const uint8_t *iv;
  uint32_t iv_len;
  const uint8_t *aad;
  uint32_t aad_len;
  const uint8_t *input;
  uint32_t in_len;
  uint8_t *output;
  uint8_t *tag;
} SECURE_AesGcmParamsTypeDef;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func);

/**
 * @brief Secure hardware TRNG random number generator for NonSecure world.
 */
int SECURE_TRNG_GetBytes(uint8_t *output, uint32_t length);

/**
 * @brief Secure DLMS HLS5 GMAC authentication verification in Secure World.
 */
int SECURE_DLMS_HLS5_Verify(const uint8_t *server_challenge,
                           const uint8_t *client_challenge,
                           const uint8_t *gmac_response);

/**
 * @brief Secure AES-GCM encryption / decryption engine using Secure World hardware SAES.
 * Pointer parameter conforms to Cortex-M33 CMSE register passing (r0-r3).
 */
int SECURE_DLMS_AES_GCM_Process(const SECURE_AesGcmParamsTypeDef *params);

#endif /* SECURE_NSC_H */
/* USER CODE END Non_Secure_CallLib_h */
