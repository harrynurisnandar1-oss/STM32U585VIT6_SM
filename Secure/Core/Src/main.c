/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ami_boot_crypto.h"
#include "ami_boot_pubkey.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Non-secure Vector table to jump to (internal Flash Bank2 here)             */
/* Caution: address must correspond to non-secure internal Flash where is     */
/*          mapped in the non-secure vector table                             */
#define VTOR_TABLE_NS_START_ADDR  0x08100000UL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void NonSecure_Init(void);
static void MX_GPIO_Init(void);
static void MX_GTZC_S_Init(void);
static void MX_IWDG_Init(void);
static void SECURE_CheckAndApplyLitbangRdp(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* SAU/IDAU, FPU and interrupts secure/non-secure allocation setup done */
  /* in SystemInit() based on partition_stm32u585xx.h file's definitions. */

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* GTZC initialisation */
  MX_GTZC_S_Init();

  /* USER CODE BEGIN SysInit */
  MX_IWDG_Init();
  SECURE_CheckAndApplyLitbangRdp();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /*************** Setup and jump to non-secure *******************************/

  NonSecure_Init();

  /* Non-secure software does not return, this code is not executed */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

#define AMI_BOOT_MANIFEST_MAGIC      0x504C4E21U /* 'PLN!' */
#define AMI_BOOT_MANIFEST_OFFSET     0x400U
#define AMI_NS_FLASH_START           0x08100000U
#define AMI_NS_FLASH_END             0x081FFFFFU
#define AMI_NS_SRAM_START            0x20040000U
#define AMI_NS_SRAM_END              0x200C0000U

typedef struct {
  uint32_t magic;           /* 0x504C4E21 'PLN!' */
  uint32_t version_epoch;   /* Monotonic counter for anti-rollback */
  uint32_t image_size;      /* NonSecure image size in bytes */
  uint32_t flags;           /* Security attributes */
  uint8_t  sha256_hash[32]; /* SHA-256 digest of NonSecure image */
  uint8_t  ecdsa_sig[64];   /* ECDSA P-256 signature (r || s) */
} NonSecureImageManifestTypeDef;

static int SECURE_VerifyNonSecureImage(void)
{
  uint32_t msp_ns = *(uint32_t *)VTOR_TABLE_NS_START_ADDR;
  uint32_t reset_handler = *(uint32_t *)(VTOR_TABLE_NS_START_ADDR + 4U);

  /* 1. Verify NonSecure Vector Table boundary */
  if (VTOR_TABLE_NS_START_ADDR < AMI_NS_FLASH_START || VTOR_TABLE_NS_START_ADDR >= AMI_NS_FLASH_END) {
    return -1;
  }

  /* 2. Verify Initial MSP_NS lies within NonSecure RAM (0x20040000..0x200C0000) */
  if (msp_ns < AMI_NS_SRAM_START || msp_ns > AMI_NS_SRAM_END) {
    return -2;
  }

  /* 3. Verify ResetHandler points to executable NonSecure Flash with Thumb bit set */
  if ((reset_handler & 1U) == 0U || reset_handler < AMI_NS_FLASH_START || reset_handler >= AMI_NS_FLASH_END) {
    return -3;
  }

  /* 4. Manifest Cryptographic Integrity (SHA-256 + ECDSA P-256) & Anti-Rollback Check */
  const NonSecureImageManifestTypeDef *manifest =
      (const NonSecureImageManifestTypeDef *)(VTOR_TABLE_NS_START_ADDR + AMI_BOOT_MANIFEST_OFFSET);

  if (manifest->magic == AMI_BOOT_MANIFEST_MAGIC) {
    uint32_t min_size = AMI_BOOT_MANIFEST_OFFSET + (uint32_t)sizeof(NonSecureImageManifestTypeDef);
    if (manifest->image_size < min_size || manifest->image_size > (AMI_NS_FLASH_END - AMI_NS_FLASH_START)) {
      return -4; /* Invalid image size declared in manifest */
    }

    /* Compute canonical SHA-256 over [0..0x400) + [0x400+sizeof(manifest)..image_size) */
    AmiSha256Context sha;
    uint8_t computed_hash[32];
    const uint8_t *ns_base = (const uint8_t *)VTOR_TABLE_NS_START_ADDR;
    ami_sha256_init(&sha);
    ami_sha256_update(&sha, ns_base, AMI_BOOT_MANIFEST_OFFSET);
    ami_sha256_update(&sha, ns_base + min_size, manifest->image_size - min_size);
    ami_sha256_final(&sha, computed_hash);

    /* Constant-time digest comparison */
    uint8_t hash_diff = 0U;
    for (uint32_t i = 0U; i < 32U; ++i) {
      hash_diff |= (uint8_t)(computed_hash[i] ^ manifest->sha256_hash[i]);
    }
    if (hash_diff != 0U) {
      return -6; /* SHA-256 digest mismatch */
    }

    /* Verify ECDSA P-256 signature against embedded Litbang public key */
    if (ami_ecdsa_p256_verify(AMI_BOOT_ECDSA_PUBKEY_X, AMI_BOOT_ECDSA_PUBKEY_Y,
                              computed_hash, manifest->ecdsa_sig) != 0) {
      return -7; /* ECDSA P-256 signature verification failed */
    }

    /* Anti-rollback check: update monotonic epoch only after full signature verification */
#if defined(TAMP)
    uint32_t installed_version = TAMP->BKP0R;
    if (manifest->version_epoch < installed_version) {
      return -5; /* Rollback detected: refusing to execute downgraded firmware */
    }
    if (manifest->version_epoch > installed_version) {
      TAMP->BKP0R = manifest->version_epoch;
    }
#endif
  } else {
#if defined(AMI_STRICT_SECURE_BOOT) && (AMI_STRICT_SECURE_BOOT == 1)
    return -8; /* Unsigned NonSecure image rejected in strict Secure Boot mode */
#else
    /* Litbang Prototype ST-LINK Dev Fallback: record unsigned boot flag in TAMP->BKP2R */
#if defined(TAMP)
    TAMP->BKP2R = 0xDEADB001U;
#endif
#endif
  }

  return 0; /* Image verified */
}

/**
  * @brief  Non-secure call function
  *         This function is responsible for Non-secure initialization and switch
  *         to non-secure state
  * @retval None
  */
static void NonSecure_Init(void)
{
  funcptr_NS NonSecure_ResetHandler;

  /* Secure Boot integrity and anti-rollback verification prior to jump */
  if (SECURE_VerifyNonSecureImage() != 0) {
    /* Boot signature/integrity failure: halt system in Secure World, refuse to jump */
    Error_Handler();
  }

  SCB_NS->VTOR = VTOR_TABLE_NS_START_ADDR;

  /* Set non-secure main stack (MSP_NS) */
  __TZ_set_MSP_NS((*(uint32_t *)VTOR_TABLE_NS_START_ADDR));

  /* Get non-secure reset handler */
  NonSecure_ResetHandler = (funcptr_NS)(*((uint32_t *)((VTOR_TABLE_NS_START_ADDR) + 4U)));

  /* Start non-secure state software application */
  NonSecure_ResetHandler();
}

/**
  * @brief GTZC_S Initialization Function
  * @param None
  * @retval None
  */
static void MX_GTZC_S_Init(void)
{

  /* USER CODE BEGIN GTZC_S_Init 0 */

  /* USER CODE END GTZC_S_Init 0 */

  MPCBB_ConfigTypeDef MPCBB_Area_Desc = {0};

  /* USER CODE BEGIN GTZC_S_Init 1 */

  /* USER CODE END GTZC_S_Init 1 */
  /* Configure Secure hardware security & cryptographic peripherals */
  if (HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_SAES, GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK ||
      HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_RNG,  GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK ||
      HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_TAMP, GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK ||
      HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_RTC,  GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK ||
      HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_PKA,  GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK ||
      HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_HASH, GTZC_TZSC_PERIPH_SEC|GTZC_TZSC_PERIPH_NPRIV) != HAL_OK)
  {
    Error_Handler();
  }
  MPCBB_Area_Desc.SecureRWIllegalMode = GTZC_MPCBB_SRWILADIS_ENABLE;
  MPCBB_Area_Desc.InvertSecureState = GTZC_MPCBB_INVSECSTATE_NOT_INVERTED;
  for (uint32_t i = 0; i < 32; i++) {
    MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[i] = 0xFFFFFFFF;
    MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[i] = 0xFFFFFFFF;
  }
  MPCBB_Area_Desc.AttributeConfig.MPCBB_LockConfig_array[0] =   0x00000000;
  if (HAL_GTZC_MPCBB_ConfigMem(SRAM3_BASE, &MPCBB_Area_Desc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN GTZC_S_Init 2 */
  /* Lock MPCBB configuration to prevent NonSecure runtime tampering */
  if (HAL_GTZC_MPCBB_Lock(SRAM3_BASE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable TrustZone Interrupt Controller (TZIC) for illegal access detection */
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_SAES);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_RNG);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_TAMP);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_RTC);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_PKA);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_HASH);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_SRAM1);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_SRAM2);
  HAL_GTZC_TZIC_EnableIT(GTZC_PERIPH_MPCBB3_REG);

  /* Enable GTZC IRQ in NVIC with highest priority (0) */
  HAL_NVIC_SetPriority(GTZC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(GTZC_IRQn);

  /* Lock TZSC register configuration against runtime tampering */
  HAL_GTZC_TZSC_Lock(GTZC_TZSC1);
  /* USER CODE END GTZC_S_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Initialize Independent Watchdog (IWDG) with ~6 second timeout
  *         and freeze it when ST-LINK debugger halts the core.
  */
static void MX_IWDG_Init(void)
{
#if defined(DBGMCU) && defined(DBGMCU_APB1FZR1_DBG_IWDG_STOP)
  DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;
#endif
#if defined(IWDG)
  IWDG->KR  = 0x0000CCCCU; /* Enable IWDG */
  IWDG->KR  = 0x00005555U; /* Enable register access */
  IWDG->PR  = 0x04U;       /* Prescaler /64 -> 32kHz / 64 = 500 Hz */
  IWDG->RLR = 3000U;       /* 3000 / 500 Hz = 6.0 seconds */
  while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) { }
  IWDG->KR  = 0x0000AAAAU; /* Reload counter */
#endif
}

/**
  * @brief  Opt-in RDP Level 1 lockdown for Litbang PLN physical readout testing.
  *         Disabled by default (RDP Level 0) for prototype debugging; never sets Level 2.
  */
static void SECURE_CheckAndApplyLitbangRdp(void)
{
#if defined(AMI_LITBANG_RDP_LEVEL1) && (AMI_LITBANG_RDP_LEVEL1 == 1)
  FLASH_OBProgramInitTypeDef ob_init;
  HAL_FLASHEx_OBGetConfig(&ob_init);
  if (ob_init.RDPLevel == OB_RDP_LEVEL_0) {
    if (HAL_FLASH_Unlock() == HAL_OK && HAL_FLASH_OB_Unlock() == HAL_OK) {
      ob_init.OptionType = OPTIONBYTE_RDP;
      ob_init.RDPLevel   = OB_RDP_LEVEL_1;
      if (HAL_FLASHEx_OBProgram(&ob_init) == HAL_OK) {
        HAL_FLASH_OB_Launch();
      }
      HAL_FLASH_OB_Lock();
      HAL_FLASH_Lock();
    }
  }
#endif
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
#if defined(TAMP)
  /* Record Secure fault marker in TAMP->BKP1R for post-reset diagnostics */
  TAMP->BKP1R = 0xFA017001U;
#endif
  NVIC_SystemReset();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
