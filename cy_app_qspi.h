/***************************************************************************/
/*
 * \file cy_app_qspi.h
 * \version 1.0
 *
 * Provided API declarations for the QSPI implementation.
 *
 *******************************************************************************
 * \copyright
 * (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/***************************************************************************/
#ifndef _QSPI_H_
#define _QSPI_H_

#include "cy_pdl.h"
#include "cy_app_scb.h"
#include "cy_debug.h"
#include "cy_usb_app.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*****************************************************************************
*                          SMIF enums
******************************************************************************/

typedef enum cy_en_flash_index_t
{
  SPI_FLASH_0    = 0,
  SPI_FLASH_1    = 1,
  DUAL_SPI_FLASH = 2,
  NUM_SPI_FLASH,
}cy_en_flash_index_t;

typedef enum cy_en_flashMode_t
{
    SINGLE_MODE = 0,
    QUAD_MODE = 1, 
    OCTAL_MODE = 2,
    INVALID_MODE = 0xFF
}cy_en_flashMode_t;


/*****************************************************************************
*                          SMIF macros                                       *
******************************************************************************/

#define SMIF_HW SMIF0

#define SMIF_CLK_HSIOM (P6_0_SMIF_SPI_CLK)
#define SMIF_CLK_PORT (P6_0_PORT)
#define SMIF_CLK_PIN (P6_0_PIN)

#define SMIF_SELECT0_HSIOM (P6_1_SMIF_SPI_SELECT0)
#define SMIF_SELECT0_PORT (P6_1_PORT)
#define SMIF_SELECT0_PIN (P6_1_PIN)

#define SMIF_SELECT1_HSIOM (P6_2_SMIF_SPI_SELECT1)
#define SMIF_SELECT1_PORT (P6_2_PORT)
#define SMIF_SELECT1_PIN (P6_2_PIN)

#define SMIF_DATA0_HSIOM (P7_0_SMIF_SPI_DATA0)
#define SMIF_DATA0_PORT (P7_0_PORT)
#define SMIF_DATA0_PIN (P7_0_PIN)

#define SMIF_DATA1_HSIOM (P7_1_SMIF_SPI_DATA1)
#define SMIF_DATA1_PORT (P7_1_PORT)
#define SMIF_DATA1_PIN (P7_1_PIN)

#define SMIF_DATA2_HSIOM (P7_2_SMIF_SPI_DATA2)
#define SMIF_DATA2_PORT (P7_2_PORT)
#define SMIF_DATA2_PIN (P7_2_PIN)

#define SMIF_DATA3_HSIOM (P7_3_SMIF_SPI_DATA3)
#define SMIF_DATA3_PORT (P7_3_PORT)
#define SMIF_DATA3_PIN (P7_3_PIN)

#define SMIF_DATA4_HSIOM (P7_4_SMIF_SPI_DATA4)
#define SMIF_DATA4_PORT (P7_4_PORT)
#define SMIF_DATA4_PIN (P7_4_PIN)

#define SMIF_DATA5_HSIOM (P7_5_SMIF_SPI_DATA5)
#define SMIF_DATA5_PORT (P7_5_PORT)
#define SMIF_DATA5_PIN (P7_5_PIN)

#define SMIF_DATA6_HSIOM (P7_6_SMIF_SPI_DATA6)
#define SMIF_DATA6_PORT (P7_6_PORT)
#define SMIF_DATA6_PIN (P7_6_PIN)

#define SMIF_DATA7_HSIOM (P7_7_SMIF_SPI_DATA7)
#define SMIF_DATA7_PORT (P7_7_PORT)
#define SMIF_DATA7_PIN (P7_7_PIN)




#define CY_APP_QSPI_QREAD_CMD                   (0xEB)
#define CY_APP_QSPI_QREAD_MODE_CMD              (0x01)
#define CY_APP_QSPI_READ_ID_CMD                 (0x9F)

#define CY_APP_QSPI_NUM_ADDRESS_BYTES           (3)
#define CY_APP_QSPI_ID_SIZE                     (16)
#define CY_APP_QSPI_QREAD_MODE_WIDTH            (CY_SMIF_WIDTH_QUAD)
#define CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFS  (8)
#define CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFL  (4)
#define CY_APP_QSPI_DEVICE_ID_OFFSET            (5)


/* SPI Commands for Infineon SFS256 Flash */
#define CY_SPI_WRITE_ENABLE_CMD                 (0x06)
#define CY_SPI_READ_ID_CMD                      (0x9F)
#define CY_SPI_STATUS_READ_CMD                  (0x05)
#define CY_APP_SPI_READ_ANY_REG_CMD             (0x65)
#define CY_APP_SPI_WRITE_ANY_REGISTER_CMD       (0x71)
#define CY_APP_SPI_RESET_ENABLE_CMD             (0x66)
#define CY_APP_SPI_SW_RESET_CMD                 (0x99)
#define CY_SPI_QREAD_MODE_CMD                   (0x01)
#define CY_SPI_QPI_READ_CMD                     (0xEB)
#define CY_SPI_READ_CMD                         (0x03)

#define CY_QSPI_NUM_DUMMY_CYCLES                (0x08) /*SFS256 Flash*/

#define SPI_ADDRESS_BYTE_COUNT                  (3)
#define CY_FLASH_ID_LENGTH                      (0x08)
#define CY_SFS256_CR1V_REG                      (0x800002)
#define CY_SFS256_CR2V_REG                      (0x800003)
#define CY_SFS256_CR3V_REG                      (0x800004)
#define CY_SPI_WRITE_ENABLE_LATCH_MASK          (0x02)

extern cy_stc_smif_context_t qspiContext;



    /*****************************************************************************
     * Function Name:Cy_QSPI_ConfigureSMIFPins
     ******************************************************************************
     * Summary:
     * Function to Configure SMIF pins
     *
     * Parameters:
     *  \param pAppCtxt
     *   User Context
     *  \param init
     *   Initialize
     *
     * Return:
     *  Does not return.
     *****************************************************************************/
    void Cy_QSPI_ConfigureSMIFPins(bool init);


cy_en_smif_status_t Cy_SMIF_Start(cy_stc_usb_app_ctxt_t *pUsbApp);


/*****************************************************************************
* Function Name:Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray, 
                            uint8_t numAddressBytes)
******************************************************************************
* Summary:
* Function to change address to array
*
* Parameters:
*   \param value
*   Value to convert to array
*  \param byteArray
*   Pointer to array
*  \param numAddressBytes
*   Number of bytes in address
*
* Return:
*  returns data
*****************************************************************************/
void Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray, uint8_t numAddressBytes);

cy_en_smif_status_t Cy_SPI_FlashInit(cy_en_flash_index_t flashIndex, 
                                     cy_en_flashMode_t mode);


cy_en_smif_status_t Cy_QSPI_Read(uint32_t address, uint8_t *rxBuffer, uint32_t length, cy_en_flash_index_t flashIndex);


#if defined(__cplusplus)
}
#endif
#endif /*_QSPI_H_*/
