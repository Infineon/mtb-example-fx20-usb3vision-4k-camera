/***************************************************************************/
/*
 * \file cy_app_qspi.c
 * \version 1.0
 *
 *  Implements the OSPI data transfers part for FPGA configuration.
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
/*******************************************************************************/

#include "cy_app_qspi.h"
#include "cy_smif.h"

cy_stc_smif_context_t qspiContext;
cy_en_smif_txfr_width_t   glCommandWidth[NUM_SPI_FLASH]     = {CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_QUAD};
cy_en_smif_txfr_width_t   glReadWriteWidth[NUM_SPI_FLASH]   = {CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_OCTAL};
uint8_t glSlaveSelectIndex[NUM_SPI_FLASH] = {CY_SMIF_SLAVE_SELECT_0, CY_SMIF_SLAVE_SELECT_1, (CY_SMIF_SLAVE_SELECT_0 | CY_SMIF_SLAVE_SELECT_1)};

/* QSPI/ SMIF Config*/
static const cy_stc_smif_config_t qspiConfig =
    {
        .mode = (uint32_t)CY_SMIF_NORMAL,
        .deselectDelay = 7u,
        .rxClockSel = (uint32_t)CY_SMIF_SEL_INV_INTERNAL_CLK,
        .blockEvent = (uint32_t)CY_SMIF_BUS_ERROR,
};

/*******************************************************************************
 * Function name: Cy_SPI_FlashReset
 ****************************************************************************/
/**
 *
 * Description: Function to send reset command to selected flash
 *
 * \param flashIndex
 * SPI Flash Index
 *
 * \return
 * status
 *
 ********************************************************************************/
static cy_en_smif_status_t Cy_SPI_FlashReset(cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     CY_APP_SPI_RESET_ENABLE_CMD,
                                     CY_SMIF_WIDTH_QUAD,
                                     NULL,
                                     CY_SMIF_CMD_WITHOUT_PARAM,
                                     CY_SMIF_WIDTH_NA,
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     CY_APP_SPI_SW_RESET_CMD,
                                     CY_SMIF_WIDTH_QUAD,
                                     NULL,
                                     CY_SMIF_CMD_WITHOUT_PARAM,
                                     CY_SMIF_WIDTH_NA,
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    /*tRPH delay SFS256 Flash*/
    Cy_SysLib_DelayUs(50);

    return status;
}

/*******************************************************************************
 * Function name: Cy_SPI_AddressToArray
 ****************************************************************************/
/**
*
* Description: Function to convert 32-nit value to byte array
* \param value
* 32-bit value to be converted
*
* \param byteArray
* Pointer to byte array

* \param numAddressBytes
* Number of bytes
*
* \return
* status
*
********************************************************************************/
void Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray, uint8_t numAddressBytes)
{
    do
    {
        numAddressBytes--;
        byteArray[numAddressBytes] = (uint8_t)(value & 0x000000FF);
        value >>= 8; /* Shift to get the next byte */
    } while (numAddressBytes > 0U);
}

/*******************************************************************************
 * Function name: Cy_QSPI_WriteEnable
 ****************************************************************************/
/**
 *
 * Description: Function to Set write enable latch of the selected flash.
 *              This is needed before doing program and erase operations.
 *
 * \param flashIndex
 * SPI Flash Index
 *
 * \return
 * status
 *
 ********************************************************************************/
static cy_en_smif_status_t Cy_QSPI_WriteEnable(cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t statusVal = 0;

    if (flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n", __func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     CY_SPI_WRITE_ENABLE_CMD,
                                     glCommandWidth[flashIndex],
                                     NULL,
                                     CY_SMIF_CMD_WITHOUT_PARAM,
                                     CY_SMIF_WIDTH_NA,
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    /* Check if WRITE_ENABLE LATCH is set */
    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     CY_SPI_STATUS_READ_CMD,
                                     glCommandWidth[flashIndex],
                                     NULL,
                                     CY_SMIF_CMD_WITHOUT_PARAM,
                                     CY_SMIF_WIDTH_NA,
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_NOT_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW,
                                         &statusVal,
                                         1u,
                                         glReadWriteWidth[flashIndex],
                                         &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if (statusVal & CY_SPI_WRITE_ENABLE_LATCH_MASK)
    {
        status = CY_SMIF_SUCCESS;
    }
    else
    {
        status = CY_SMIF_BUSY;
        DBG_APP_ERR("Write Enable failed\r\n");
    }

    return status;
}

/*******************************************************************************
 * Function name: Cy_QSPI_ReadAnyRegister
 ****************************************************************************/
/**
 *
 * Description: Function to read selected flash's register
 *
 * \param flashIndex
 * SPI Flash Index
 *
 * \param address
 * register address
 *
 * \return
 * register value
 *
 ********************************************************************************/
static uint8_t Cy_QSPI_ReadAnyRegister(cy_en_flash_index_t flashIndex, uint32_t address)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t regValue = 0;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];
    if (flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n", __func__);
        return 0xFF;
    }

    Cy_SPI_AddressToArray(address, addrArray, SPI_ADDRESS_BYTE_COUNT);

    status = Cy_SMIF_TransmitCommand(SMIF0,
                                     CY_APP_SPI_READ_ANY_REG_CMD,
                                     glCommandWidth[flashIndex],
                                     addrArray,
                                     SPI_ADDRESS_BYTE_COUNT,
                                     glCommandWidth[flashIndex],
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_NOT_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

    Cy_SMIF_SendDummyCycles(SMIF_HW, CY_QSPI_NUM_DUMMY_CYCLES);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF0, &regValue, 1u, glReadWriteWidth[flashIndex], &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    APP_DBG_TRACE1("SLV%d:Read Register: 0x%x Read value = 0x%x\r\n", flashIndex, address, regValue);
    return regValue;
}

/*******************************************************************************
 * Function name: Cy_QSPI_WriteAnyRegister
 ****************************************************************************/
/**
 *
 * Description: Function to write to register address of the selected flash
 *
 * \param flashIndex
 * SPI Flash Index
 *
 * \param address
 * register address
 *
 * \param value
 * register value
 *
 * \return
 * status
 *
 ********************************************************************************/

static cy_en_smif_status_t Cy_QSPI_WriteAnyRegister(cy_en_flash_index_t flashIndex, uint32_t regAddress, uint8_t value)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];

    if (flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n", __func__);
        return CY_SMIF_BAD_PARAM;
    }

    Cy_SPI_AddressToArray(regAddress, addrArray, SPI_ADDRESS_BYTE_COUNT);
    APP_DBG_TRACE1("Write %d to Register: 0x%x\r\n", value, regAddress);

    status = Cy_QSPI_WriteEnable(flashIndex);
    Cy_SysLib_Delay(200);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

    if (CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_TransmitCommand(SMIF0,
                                         CY_APP_SPI_WRITE_ANY_REGISTER_CMD,
                                         glCommandWidth[flashIndex],
                                         addrArray,
                                         SPI_ADDRESS_BYTE_COUNT,
                                         glCommandWidth[flashIndex],
                                         (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                         CY_SMIF_TX_NOT_LAST_BYTE,
                                         &qspiContext);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

        status = Cy_SMIF_TransmitDataBlocking(SMIF0, &value, 1, glReadWriteWidth[flashIndex], &qspiContext);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    }
    return status;
}

/*******************************************************************************
 * Function name: Cy_SPI_ReadID
 ****************************************************************************/
/**
 *
 * Description: Function to read SPI Flash ID
 *
 * \param flashIndex
 * SPI Flash Index
 *
 ** \param rxBuffer
 * ID buffer pointer
 *
 * \return
 * void
 *
 ********************************************************************************/
cy_en_smif_status_t Cy_SPI_ReadID(uint8_t *rxBuffer, cy_en_flash_index_t flashIndex)
{
    APP_DBG_TRACE1("Cy_SPI_ReadID Init \n\r");
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    if (flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n", __func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     CY_SPI_READ_ID_CMD,
                                     glCommandWidth[flashIndex],
                                     NULL,
                                     CY_SMIF_CMD_WITHOUT_PARAM,
                                     CY_SMIF_WIDTH_NA,
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_NOT_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW, rxBuffer, CY_FLASH_ID_LENGTH, glReadWriteWidth[flashIndex], &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_SPI_FlashInit
 *****************************************************************************
 * Summary:
 *  Initialize SPI flash device and configure operating mode
 *
 * Parameters:
 *  \param flashIndex
 *  SPI Flash Index
 *  \param mode
 *  Flash operating mode (QUAD_MODE, OCTAL_MODE)
 *
 * Return:
 *  cy_en_smif_status_t - SMIF operation status
 ****************************************************************************/
cy_en_smif_status_t Cy_SPI_FlashInit(cy_en_flash_index_t flashIndex, 
                                     cy_en_flashMode_t mode)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t regValue = 0;
    bool quadEnable = 0;
    bool qpiEnable = 0;


    if(mode == OCTAL_MODE)
    {
        quadEnable = true;
        qpiEnable = true;
    }
    else if (mode == QUAD_MODE)
    {
        quadEnable = true;
    }

    /*Get the flash memories to default state*/
    status = Cy_SPI_FlashReset(flashIndex);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if(quadEnable)
    {
        /* Enable QPI and QUAD Mode bits for SFS256 Flash */
        regValue = Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR1V_REG);
        status = Cy_QSPI_WriteAnyRegister(flashIndex, CY_SFS256_CR1V_REG, regValue | 0x02);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
        glReadWriteWidth[flashIndex] = CY_SMIF_WIDTH_QUAD;
        if(qpiEnable)
        {
            regValue = Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR2V_REG);
            status = Cy_QSPI_WriteAnyRegister(flashIndex, CY_SFS256_CR2V_REG, regValue | 0x40);
            ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
            glCommandWidth[flashIndex] = CY_SMIF_WIDTH_QUAD;
        }
    }
    return status;
}

/*******************************************************************************
 * Function name: Cy_QSPI_Read
 ****************************************************************************/
/**
 *
 * Description: Function to read the SPI Flash
 *
 * \param address
 * SPI Flash address
 *
 * \param rxBuffer
 * Data buffer pointer
 *
 * \param length
 * length of data received
 * \param flashIndex
 * SPI Flash index
 *
 * \return
 * status
 *
 ********************************************************************************/

cy_en_smif_status_t Cy_QSPI_Read(uint32_t address, uint8_t *rxBuffer, uint32_t length, cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];
    uint8_t readCommand = CY_SPI_READ_CMD;

    if ((glCommandWidth[flashIndex] == CY_SMIF_WIDTH_QUAD) ||
        (glReadWriteWidth[flashIndex] == CY_SMIF_WIDTH_QUAD))
    {
        readCommand = CY_SPI_QPI_READ_CMD;
    }

    Cy_SPI_AddressToArray(address, addrArray, SPI_ADDRESS_BYTE_COUNT);
    status = Cy_SMIF_TransmitCommand(SMIF_HW,
                                     readCommand,
                                     glCommandWidth[flashIndex],
                                     addrArray,
                                     SPI_ADDRESS_BYTE_COUNT,
                                     glReadWriteWidth[flashIndex],
                                     (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                     CY_SMIF_TX_NOT_LAST_BYTE,
                                     &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if (glReadWriteWidth[flashIndex] != CY_SMIF_WIDTH_SINGLE)
    {
        Cy_SMIF_TransmitCommand(SMIF_HW,
                                CY_SPI_QREAD_MODE_CMD,
                                glReadWriteWidth[flashIndex],
                                NULL,
                                CY_SMIF_CMD_WITHOUT_PARAM,
                                CY_SMIF_WIDTH_NA,
                                (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                                CY_SMIF_TX_NOT_LAST_BYTE,
                                &qspiContext);

        Cy_SMIF_SendDummyCycles(SMIF_HW, CY_QSPI_NUM_DUMMY_CYCLES);
    }

    if (status == CY_SMIF_SUCCESS)
    {
        status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW, rxBuffer, length, glReadWriteWidth[flashIndex], &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    }
    return status;
}

/*****************************************************************************
 * Function Name: Cy_SMIF_Start
 *****************************************************************************
 * Summary:
 *  Initialize and start SMIF interface for flash operations
 *
 * Parameters:
 *  \param pAppCtxt
 *  Application context pointer
 *
 * Return:
 *  cy_en_smif_status_t - SMIF operation status
 ****************************************************************************/
cy_en_smif_status_t Cy_SMIF_Start(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    /* Change QSPI Clock to 150 MHz / <DIVIDER> value */
    Cy_SysClk_ClkHfDisable(1);
    Cy_SysClk_ClkHfSetSource(1, CY_SYSCLK_CLKHF_IN_CLKPATH2);
    Cy_SysClk_ClkHfSetDivider(1, CY_SYSCLK_CLKHF_NO_DIVIDE);
    Cy_SysClk_ClkHfEnable(1);

    /*Initialize SMIF Pins for QSPI*/
    Cy_QSPI_ConfigureSMIFPins(true);

    status = Cy_SMIF_Init(SMIF_HW, &qspiConfig, 10000u, &qspiContext);
    ASSERT(CY_SMIF_SUCCESS == status, status);

    Cy_SMIF_SetDataSelect(SMIF_HW, CY_SMIF_SLAVE_SELECT_0, CY_SMIF_DATA_SEL0);
    Cy_SMIF_SetDataSelect(SMIF_HW, CY_SMIF_SLAVE_SELECT_1, CY_SMIF_DATA_SEL2);

    Cy_SMIF_Enable(SMIF_HW, &qspiContext);
    APP_DBG_TRACE1("Cy_USB_QSPIEnabled \n\r:");
    return status;
}

/*******************************************************************************
 * Function name: Cy_QSPI_ConfigureSMIFPins
 ****************************************************************************/
/**
 *
 * Description: Function to configure SMIF pins
 *
 * \param init
 * initialize SMIF pins if true
 *
 *
 * \return
 * void
 *
 ********************************************************************************/
void Cy_QSPI_ConfigureSMIFPins(bool init)
{
    cy_en_gpio_status_t status = CY_GPIO_SUCCESS;
    cy_stc_gpio_pin_config_t pinCfg;

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    if (init)
    {
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        /* Configure P6.0 as QSPI Clock */
        pinCfg.hsiom = SMIF_CLK_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.0 as floating GPIO */
        pinCfg.hsiom = P6_0_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_CLK_PORT, SMIF_CLK_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        /* Configure P6.1 as QSPI Select 0 */
        pinCfg.outVal = 1;
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        pinCfg.hsiom = SMIF_SELECT0_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.1 as floating GPIO */
        pinCfg.hsiom = P6_1_GPIO;
    }

    status = Cy_GPIO_Pin_Init(SMIF_SELECT0_PORT, SMIF_SELECT0_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        /* Configure P6.2 as QSPI Select 1 */
        pinCfg.outVal = 1;
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        pinCfg.hsiom = SMIF_SELECT1_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.2 as floating GPIO */
        pinCfg.hsiom = P6_2_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_SELECT1_PORT, SMIF_SELECT1_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.0 as QSPI Data 0 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA0_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.0 as floating GPIO */
        pinCfg.hsiom = P7_0_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA0_PORT, SMIF_DATA0_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.1 as QSPI Data 1 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA1_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.1 as floating GPIO */
        pinCfg.hsiom = P7_1_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA1_PORT, SMIF_DATA1_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.2 as QSPI Data 2*/
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA2_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.2 as floating GPIO */
        pinCfg.hsiom = P7_2_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA2_PORT, SMIF_DATA2_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.3 as QSPI Data 3*/
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA3_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.3 as floating GPIO */
        pinCfg.hsiom = P7_3_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA3_PORT, SMIF_DATA3_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.4 as QSPI Data 4 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA4_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.4 as floating GPIO */
        pinCfg.hsiom = P7_4_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA4_PORT, SMIF_DATA4_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.5 as QSPI Data 5 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA5_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.5 as floating GPIO */
        pinCfg.hsiom = P7_5_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA5_PORT, SMIF_DATA5_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.6 as QSPI Data 6 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA6_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.6 as floating GPIO */
        pinCfg.hsiom = P7_6_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA6_PORT, SMIF_DATA6_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if (init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.7 as QSPI Data 7 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA7_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.7 as floating GPIO */
        pinCfg.hsiom = P7_7_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA7_PORT, SMIF_DATA7_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);
}
