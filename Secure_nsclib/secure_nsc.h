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
  SECURE_KEY_GBEK   = 0x02U,
  SECURE_KEY_AK     = 0x03U,
  SECURE_KEY_KEK    = 0x04U
} SECURE_KeyIdTypeDef;

#define SECURE_ERROR_GENERIC           -1
#define SECURE_ERROR_RNG_FAIL          -2
#define SECURE_ERROR_AUTH_FAIL         -3
#define SECURE_ERROR_INVALID_KEY       -4
#define SECURE_ERROR_LOCKED            -5
#define SECURE_ERROR_UNAUTHORIZED      -6
#define SECURE_ERROR_NO_KEK            -7
#define SECURE_ERROR_UNPROVISIONED     -8

typedef struct
{
  uint8_t key_type;            /*!< 0 = GUEK, 1 = GBEK, 2 = AK, 3 = Master KEK */
  const uint8_t *wrapped_key;  /*!< Pointer to 24-byte RFC 3394 wrapped key in NonSecure memory */
  uint32_t wrapped_key_len;    /*!< Length in bytes, must be 24 */
} SECURE_WrappedKeyParamTypeDef;

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
 * @brief Factory Provisioning of device-unique Master Key (KEK) into Secure Storage.
 *
 * Can only be executed with valid authorization token.
 *
 * @param factory_kek 16-byte Master Key buffer in NonSecure memory.
 * @param auth_token Factory authorization token (SECURE_FACTORY_PROVISION_TOKEN).
 * @return 0 on success, negative error code on invalid parameters or unauthorized call.
 */
int SECURE_ProvisionMasterKey(const uint8_t *factory_kek, uint32_t auth_token);

/**
 * @brief Check if Master Key has been provisioned.
 * @return 1 if provisioned, 0 if using hardware UID-derived fallback KEK.
 */
int SECURE_IsMasterKeyProvisioned(void);

/**
 * @brief Read STM32 96-bit Unique Device ID (UID) from Secure World.
 * @param uid_out Pointer to receive 12-byte UID.
 * @param max_len Size of buffer (minimum 12 bytes).
 * @return 0 on success, negative error code on failure.
 */
int SECURE_GetDeviceUniqueId(uint8_t *uid_out, uint32_t max_len);

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
 * @brief Calculate DLMS HLS5 GMAC server authentication proof using isolated Secure World AK.
 * @param challenge 16-byte challenge.
 * @param challenge_len Must be 16.
 * @param proof_out Output buffer for 16-byte GMAC proof.
 * @param proof_capacity Must be at least 16.
 * @param proof_len Pointer to receive length written (16).
 * @return 0 on success, negative error code on failure.
 */
int SECURE_DLMS_HLS5_CalculateProof(const uint8_t *challenge,
                                   uint32_t challenge_len,
                                   uint8_t *proof_out,
                                   uint32_t proof_capacity,
                                   uint32_t *proof_len);

/**
 * @brief Secure AES-GCM encryption / decryption engine using Secure World hardware SAES.
 * Pointer parameter conforms to Cortex-M33 CMSE register passing (r0-r3).
 */
int SECURE_DLMS_AES_GCM_Process(const SECURE_AesGcmParamsTypeDef *params);

/**
 * @brief Atomically unwrap and commit a batch of remote keys inside Secure World.
 *
 * Uses device-unique Master Key (KEK) held in Secure Storage to unwrap RFC 3394
 * 24-byte wrapped keys.
 * If ANY key in the batch fails RFC 3394 integrity check (IV mismatch), the entire
 * operation aborts immediately and NO operational keys are updated (atomic all-or-nothing).
 * Plaintext keys NEVER leak to NonSecure World.
 *
 * @param keys Array of wrapped key descriptors in NonSecure memory.
 * @param key_count Number of keys in batch (1 to 4).
 * @return 0 on success, negative error code on failure or integrity violation.
 */
int SECURE_DLMS_TransferKeys_Atomic(const SECURE_WrappedKeyParamTypeDef *keys,
                                    uint32_t key_count);

#endif /* SECURE_NSC_H */
/* USER CODE END Non_Secure_CallLib_h */
