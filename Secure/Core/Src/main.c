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
#define AMI_NS_FLASH_START           0x08040000U
#define AMI_NS_FLASH_END             0x081FFFFFU
#define AMI_NS_SRAM_START            0x20018000U
#define AMI_NS_SRAM_END              0x20040000U

typedef struct {
  uint32_t magic;           /* 0x504C4E21 'PLN!' */
  uint32_t version_epoch;   /* Monotonic counter for anti-rollback */
  uint32_t image_size;      /* NonSecure image size in bytes */
  uint32_t flags;           /* Security attributes */
  uint8_t  sha256_hash[32]; /* SHA-256 digest of NonSecure image */
} NonSecureImageManifestTypeDef;

static int SECURE_VerifyNonSecureImage(void)
{
  uint32_t msp_ns = *(uint32_t *)VTOR_TABLE_NS_START_ADDR;
  uint32_t reset_handler = *(uint32_t *)(VTOR_TABLE_NS_START_ADDR + 4U);

  /* 1. Verify NonSecure Vector Table boundary */
  if (VTOR_TABLE_NS_START_ADDR < AMI_NS_FLASH_START || VTOR_TABLE_NS_START_ADDR >= AMI_NS_FLASH_END) {
    return -1;
  }

  /* 2. Verify Initial MSP_NS lies within NonSecure RAM */
  if (msp_ns < AMI_NS_SRAM_START || msp_ns > AMI_NS_SRAM_END) {
    return -2;
  }

  /* 3. Verify ResetHandler points to executable NonSecure Flash with Thumb bit set */
  if ((reset_handler & 1U) == 0U || reset_handler < AMI_NS_FLASH_START || reset_handler >= AMI_NS_FLASH_END) {
    return -3;
  }

  /* 4. Manifest Integrity and Anti-Rollback Check */
  const NonSecureImageManifestTypeDef *manifest = (const NonSecureImageManifestTypeDef *)(VTOR_TABLE_NS_START_ADDR + 0x400U);
  if (manifest->magic == AMI_BOOT_MANIFEST_MAGIC) {
    if (manifest->image_size < 1024U || manifest->image_size > (AMI_NS_FLASH_END - AMI_NS_FLASH_START)) {
      return -4; /* Invalid image size declared in manifest */
    }

    /* Anti-rollback check: compare with Secure monotonic counter */
#if defined(TAMP)
    uint32_t installed_version = TAMP->BKP0R;
    if (manifest->version_epoch < installed_version) {
      return -5; /* Rollback detected: refusing to execute downgraded firmware */
    }
    if (manifest->version_epoch > installed_version) {
      TAMP->BKP0R = manifest->version_epoch;
    }
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

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
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
