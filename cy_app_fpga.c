/***************************************************************************/
/*
 * \file cy_app_fpga.c
 * \version 1.0
 *
 *  Implements the FPGA related APIs; I2C and QSPI interface.
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

#include "cy_scb_i2c.h"
#include "cy_usb_app.h"
#include "cy_lvds.h"
#if FPGA_ENABLE
#include "cy_app_fpga.h"
#include "cy_app_lvds.h"

bool glIsFPGAConfigured = false;
cy_en_fpgaConfigMode_t glFpgaConfigMode = FPGA_CONFIG_MODE;
char *glFpgaConfigModeString[FPGA_CFG_MODE_MAX] = {"FPGA_CFG_MODE_NONE", "ACTIVE_SERIAL_MODE", "PASSIVE_SERIAL_x4_MODE", "PASSIVE_SERIAL_x8_MODE", "FPGA_CFG_TRY_ALL_MODE"};
cy_stc_externalFlashMetadata_t glFpgaFileMetadata;

/*****************************************************************************
 * Function Name:Cy_FPGA_IsConfigurationCompleted
 ******************************************************************************
 * Summary:
 * Check for fpga configuration completion flag
 *
 * Parameters:
 *   \param void
 *   None
 *
 * Return: bool
 *  returns flag status of fpga configuration
 *****************************************************************************/
bool Cy_FPGA_IsConfigurationCompleted(void)
{
    return glIsFPGAConfigured;
}

/*****************************************************************************
 * Function Name:Cy_FPGA_InitConfigurationPins
 ******************************************************************************
 * Summary:
 * Init FX GPIOs connected to FPGA
 *
 * Parameters:
 *  \param void
 *  none
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_FPGA_InitConfigurationPins(void)
{
    cy_en_gpio_status_t status = CY_GPIO_SUCCESS;
    cy_stc_gpio_pin_config_t pinCfg;
    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure output GPIOs. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom = HSIOM_SEL_GPIO;

    status = Cy_GPIO_Pin_Init(FPGA_CRESET_PORT, FPGA_CRESET_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    status = Cy_GPIO_Pin_Init(FPGA_SSN_PORT, FPGA_SSN_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    status = Cy_GPIO_Pin_Init(DEMO_KIT_USER_LED_PORT, DEMO_KIT_USER_LED_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    status = Cy_GPIO_Pin_Init(FPGA_SOFTRESET_PORT, FPGA_SOFTRESET_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    status = Cy_GPIO_Pin_Init(FPGA_CSI_PORT, FPGA_CSI_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    /* Configure input GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    status = Cy_GPIO_Pin_Init(FPGA_CDONE_PORT, FPGA_CDONE_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
    
    APP_DBG_TRACE1("Init of FX Device GPIOs pins for FPGA Configuration completed\r\n");

    Cy_GPIO_Clr(FPGA_CRESET_PORT, FPGA_CRESET_PIN); /* On initialization, Hold FPGA at Reset state */
    Cy_GPIO_Set(FPGA_SOFTRESET_PORT, FPGA_SOFTRESET_PIN);
    Cy_GPIO_Set(FPGA_CSI_PORT, FPGA_CSI_PIN);
    /*FPGA's SSL_N should be HIGH for Active Serial*/
    Cy_GPIO_Set(FPGA_SSN_PORT, FPGA_SSN_PIN);
}

static bool Cy_FPGA_IsCDoneVal(bool value, uint32_t timeout_10Ms)
{
    bool cdoneVal = false;
    uint32_t maxWait = timeout_10Ms;
    bool ret = false;

    cdoneVal = Cy_GPIO_Read(FPGA_CDONE_PORT, FPGA_CDONE_PIN);

    while (cdoneVal != value)
    {
        /*Check if CDONE is HIGH or FPGA is configured */
        cdoneVal = Cy_GPIO_Read(FPGA_CDONE_PORT, FPGA_CDONE_PIN);
         vTaskDelay(pdMS_TO_TICKS(10));
        maxWait--;
        if (!maxWait)
        {
            break;
        }
    }
    if((maxWait == 0) && (cdoneVal != value))
    {
        ret = false;
    }
    else
    {
        ret = true;
    }
    return ret;
}


/*****************************************************************************
 * Function Name:Cy_FPGA_ConfigPassiveSerial
 ******************************************************************************
 * Summary:
 * Initiate FPGA configuration process
 *
 * Parameters:
 *  \param mode
 *  FPGA configuration mode (Passive Serial x4/x8)
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_FPGA_ConfigPassiveSerial(cy_en_fpgaConfigMode_t mode)
{
    cy_en_smif_slave_select_t configSlaveSel = CY_SMIF_SLAVE_SELECT_0;
    cy_en_flash_index_t flashMode = SPI_FLASH_0;
    uint32_t bitFileSize = EFINIX_MAX_CONFIG_FILE_SIZE;
    uint32_t bitFileStartAddress = 0;
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t addrArray[3];
    APP_DBG_TRACE("Trying FPGA Configuration in %s\r\n", mode == PASSIVE_SERIAL_x4_MODE ? "PASSIVE_SERIAL_x4_MODE"
                                        : "PASSIVE_SERIAL_x8_MODE");
    if(mode == PASSIVE_SERIAL_x4_MODE)
    {
        configSlaveSel = CY_SMIF_SLAVE_SELECT_0;
        flashMode = SPI_FLASH_0;
    }
    else if(mode == PASSIVE_SERIAL_x8_MODE)
    {
        configSlaveSel = (cy_en_smif_slave_select_t)(CY_SMIF_SLAVE_SELECT_0 | CY_SMIF_SLAVE_SELECT_1);
        flashMode = DUAL_SPI_FLASH;
    }
    /*Re-init SMIF pins. If HW is not DVK or FPGA config completes*/
    Cy_QSPI_ConfigureSMIFPins(true);
    /* Before accessing the Flash over QSPI from FX device, FPGA should be in RESET*/
    Cy_GPIO_Clr(FPGA_CRESET_PORT, FPGA_CRESET_PIN);
    Cy_GPIO_Clr(FPGA_SSN_PORT, FPGA_SSN_PIN);
    
    status = Cy_QSPI_Read(CY_APP_QSPI_METADATA_ADDRESS, (uint8_t *)&glFpgaFileMetadata,
                    CY_APP_QSPI_METADATA_SIZE, flashMode);
    APP_DBG_TRACE1("QSPI Read status: 0x%x, glFpgaFileMetadata Signature: 0x%x\r\n", status, glFpgaFileMetadata.startSignature);
    /*  
        This project expects the FPGA binary to be programmed through EZ-USB FX Control Center.
        This tool prepends a metadata to the FPGA binary during programming.
        This metadata will be used for getting details such as FPGA file size and start address.
    */
    if (glFpgaFileMetadata.startSignature == CY_APP_QSPI_METADATA_SIGNATURE)
    {
        APP_DBG_TRACE1("Metadata details: FPGA File Size:0x%x FPGA File Start:0x%x\r\n",
                        glFpgaFileMetadata.fpgaFileSize,
                        glFpgaFileMetadata.fpgaFileStartAddress);
        bitFileStartAddress = glFpgaFileMetadata.fpgaFileStartAddress;
        bitFileSize = glFpgaFileMetadata.fpgaFileSize;
    }
    else
    {
        bitFileStartAddress = 0x00;
        APP_DBG_ERR("FPGA Config metadata not detected. Check if the FPGA binary is programmed using EZ-USB FX control center."
                            "Using default FPGA file start address 0x%x\r\n",
                            bitFileStartAddress);
    }

    APP_DBG_TRACE("Passive Serial FPGA Config: offset 0x%x, size 0x%x\r\n", bitFileStartAddress, bitFileSize);
    Cy_SPI_AddressToArray(bitFileStartAddress, addrArray, CY_APP_QSPI_NUM_ADDRESS_BYTES);
    vTaskDelay(pdMS_TO_TICKS(2));

    Cy_FPGA_IsCDoneVal(false, 100); /* wait for 1000 ms, cdone val may be true before fpga config start, it should be false initially*/

    status = Cy_SMIF_TransmitCommand(SMIF0,
                            CY_APP_QSPI_QREAD_CMD,
                            CY_SMIF_WIDTH_SINGLE,
                            addrArray,
                            CY_APP_QSPI_NUM_ADDRESS_BYTES,
                            CY_SMIF_WIDTH_QUAD,
                            configSlaveSel,
                            CY_SMIF_TX_NOT_LAST_BYTE,
                            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_TransmitCommand(SMIF0,
                            CY_APP_QSPI_QREAD_MODE_CMD,
                            CY_SMIF_WIDTH_QUAD,
                            NULL,
                            CY_SMIF_CMD_WITHOUT_PARAM,
                            CY_SMIF_WIDTH_NA,
                            configSlaveSel,
                            CY_SMIF_TX_NOT_LAST_BYTE,
                            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_SendDummyCycles(SMIF0, CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFL);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);


    Cy_GPIO_Set(FPGA_CRESET_PORT, FPGA_CRESET_PIN);

    vTaskDelay(pdMS_TO_TICKS(6));/*Minimum time between deassertion of CRESET_N to first valid configuration data.*/
    
    /* Passive x4 - One clock cycle shifts out 4 bits. Two clock cycles => 1
        * byte. For X bytes, X *2 clock cycles are required */
    uint32_t cycles = MAX_DUMMY_CYCLES_COUNT;
    uint32_t remainingCycles = (mode == PASSIVE_SERIAL_x4_MODE)? bitFileSize*2 : bitFileSize;
            
    /* Extra clocks to get the FPGA into user mode */
    remainingCycles += DUMMY_CYCLE;

    while (remainingCycles > 0)
    {
        cycles = CY_USB_MIN(remainingCycles, MAX_DUMMY_CYCLES_COUNT);
        if (Cy_SMIF_GetCmdFifoStatus(SMIF0) < 3)
        {
            status = Cy_SMIF_SendDummyCycles(SMIF0, cycles);
            ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
            remainingCycles -= cycles;
        }
    }
    
    glIsFPGAConfigured = Cy_FPGA_IsCDoneVal(true, 100); /* wait for 1000 ms, till fpga config completes and cdone value becomes true*/
    if (glIsFPGAConfigured)
    {
        glIsFPGAConfigured = true;
        Cy_GPIO_Clr(FPGA_SOFTRESET_PORT, FPGA_SOFTRESET_PIN);
        vTaskDelay(pdMS_TO_TICKS(10));
        Cy_GPIO_Set(FPGA_SOFTRESET_PORT, FPGA_SOFTRESET_PIN);
        APP_DBG_FPGA("FPGA Soft Reset Done\r\n");
    }
}
/*****************************************************************************
 * Function Name:Cy_FPGA_ConfigActiveSerial
 ******************************************************************************
 * Summary:
 * Initiate FPGA configuration process
 *
 * Parameters:
 *  \param mode
 *  FPGA configuration mode (Active Serial, Passive Serial x4/x8)
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_FPGA_ConfigActiveSerial(void)
{
    APP_DBG_TRACE("Trying FPGA Configuration in ACTIVE_SERIAL_MODE\r\n");

    /*  In Active Mode of configuration FX device sets up the pins as defined by Efinix, release control of the SPI pins
        and release the bus for FPGA to configure itself
     */
    Cy_QSPI_ConfigureSMIFPins(false);
    Cy_GPIO_Set(FPGA_SSN_PORT, FPGA_SSN_PIN);
    Cy_GPIO_Set(FPGA_CRESET_PORT, FPGA_CRESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(500)); /* Required to for FPGA config in Active serial mode */
    glIsFPGAConfigured = Cy_FPGA_IsCDoneVal(true, 150); /* wait for 1500 ms, till fpga config completes and cdone value becomes true*/
}

/*****************************************************************************
 * Function Name:Cy_FPGA_InitiateConfiguration
 ******************************************************************************
 * Summary:
 * Initiate FPGA configuration process
 *
 * Parameters:
 *  \param mode
 *  FPGA configuration mode (Active Serial, Passive Serial x4/x8)
 *
 * Return:
 *  None
 *****************************************************************************/
 
void Cy_FPGA_InitiateConfiguration(cy_en_fpgaConfigMode_t mode)
{
    if(!glIsFPGAConfigured && ((mode == ACTIVE_SERIAL_MODE) || (mode == FPGA_CFG_TRY_ALL_MODE)))
    {
        Cy_FPGA_ConfigActiveSerial();
        glFpgaConfigMode = ACTIVE_SERIAL_MODE;
    }
    if(!glIsFPGAConfigured && ((mode == PASSIVE_SERIAL_x4_MODE) || (mode == FPGA_CFG_TRY_ALL_MODE)))
    {
        Cy_FPGA_ConfigPassiveSerial(PASSIVE_SERIAL_x4_MODE);
        glFpgaConfigMode = PASSIVE_SERIAL_x4_MODE;
    }
    if(!glIsFPGAConfigured && ((mode == PASSIVE_SERIAL_x8_MODE) || (mode == FPGA_CFG_TRY_ALL_MODE)))
    {
        Cy_FPGA_ConfigPassiveSerial(PASSIVE_SERIAL_x8_MODE);
        glFpgaConfigMode = PASSIVE_SERIAL_x8_MODE;
    }
    if(!glIsFPGAConfigured)
    {
        APP_DBG_ERR("FPGA configuration failed !!\r\n");
        glFpgaConfigMode = FPGA_CFG_MODE_NONE;
    }
    else
    {
        APP_DBG_MISC("FPGA configuration completed in %s\r\n", glFpgaConfigModeString[glFpgaConfigMode]);
    }
}
/*****************************************************************************
 * Function Name:Cy_FPGA_WriteRegister
 ******************************************************************************
 * Summary:
 * Writes FPGA parameter registers
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param regAddr
 *   fpga parameter address
 *   \param data
 *   data to be written in parameter reg
 *   \param printFlag
 *   flag to enable debug print
 *
 * Return:
 *  returns cy_stc_fpga_reg_rd_wr_ret_t
 *****************************************************************************/
cy_stc_fpga_reg_rd_wr_ret_t Cy_FPGA_WriteRegister(uint16_t fpgaAddr, uint16_t regAddr, uint8_t data, bool printFlag)
{
    return (cy_stc_fpga_reg_rd_wr_ret_t)Cy_I2C_Write(fpgaAddr, regAddr, data, FPGA_REG_ADDR_WIDTH, FPGA_REG_DATA_WIDTH, printFlag);
}

/*****************************************************************************
 * Function Name:Cy_FPGA_ReadRegister
 ******************************************************************************
 * Summary:
 * Writes FPGA parameter registers
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param regAddr
 *   fpga parameter address
 *   \param data
 *   pointer to data to be read from parameter reg
 *   \param printFlag
 *   flag to enable debug print
 *
 * Return:
 *  returns cy_stc_fpga_reg_rd_wr_ret_t
 *****************************************************************************/
cy_stc_fpga_reg_rd_wr_ret_t Cy_FPGA_ReadRegister(uint16_t fpgaAddr, uint16_t regAddr, uint8_t *data, bool printFlag)
{
    return (cy_stc_fpga_reg_rd_wr_ret_t)Cy_I2C_Read(fpgaAddr, regAddr, data, FPGA_REG_ADDR_WIDTH, FPGA_REG_DATA_WIDTH, printFlag);
}

/*****************************************************************************
 * Function Name:Cy_FPGA_UpdateFrameWidth
 ******************************************************************************
 * Summary:
 * This function updates FPGA parameter for image frame width
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param val
 *   width
 *
 * Return:
 *  returns void
 *****************************************************************************/
void Cy_FPGA_UpdateFrameWidth(uint16_t fpgaAddr, uint32_t val)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_IMG_WIDTH_LSB_ADDR, CY_GET_LSB(val), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_IMG_WIDTH_MSB_ADDR, CY_GET_MSB(val), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

/*****************************************************************************
 * Function Name:Cy_FPGA_UpdateFrameHeight
 ******************************************************************************
 * Summary:
 * This function updates FPGA parameter for image frame heigth
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param val
 *   width
 *
 * Return:
 *  returns void
 *****************************************************************************/
void Cy_FPGA_UpdateFrameHeight(uint16_t fpgaAddr, uint32_t val)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_IMG_HEIGHT_LSB_ADDR, CY_GET_LSB(val), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_IMG_HEIGHT_MSB_ADDR, CY_GET_MSB(val), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

/*****************************************************************************
 * Function Name:Cy_FPGA_AppStreamAcqMode
 ******************************************************************************
 * Summary:
 * This function updates FPGA parameter for stream FPS
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param val
 *   width
 *
 * Return:
 *  returns void
 *****************************************************************************/
void  Cy_FPGA_AppStreamAcqMode(uint16_t fpgaAddr, uint32_t val)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    if (val == STREAM_APP_DISABLE)
    {
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_STREAM_CTRL_ADDR, STREAM_APP_DISABLE, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }
    else if (val == STREAM_APP_EN)
    {
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_STREAM_CTRL_ADDR, STREAM_APP_EN, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }
    else if (val == STREAM_APP_NOTIFY_STOP)
    {
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_STREAM_CTRL_ADDR, STREAM_APP_NOTIFY_STOP, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }
    else
    {
        APP_DBG_ERR("Stream mode selection not available\r\n");
    }
}

/*****************************************************************************
 * Function Name:Cy_FPGA_UpdateStreamingFps
 ******************************************************************************
 * Summary:
 * This function updates FPGA parameter for stream FPS
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param val
 *   width
 *
 * Return:
 *  \return void
 *****************************************************************************/
void Cy_FPGA_UpdateStreamingFps(uint16_t fpgaAddr, uint8_t val)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_FPS_ADDR, CY_GET_LSB(val), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

void  Cy_FPGA_UpdateTrainingBytes(uint16_t fpgaAddr)
{
#if TRAINING_SEQ_REQ
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    APP_DBG_TRACE("LINK TRAINING I2C CMD START\n\r\r\n");
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_STREAM_CTRL_ADDR, STREAM_APP_DISABLE, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if (!LVCMOS_EN) /* LVDS is enabled; train phy + link both */
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_PHY_LINK_CTRL_ADDR, FPGA_REQ_LINK_TRAINING | FPGA_REQ_PHY_TRAINING, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /* Inform Gearing ratio to the FPGA */
    status = Cy_FPGA_WriteRegister(fpgaAddr,FPGA_LVDS_GEAR_RATIO_ADDRESS, 1 << cy_lvds_lvcmos_config.phyConfig->gearingRatio, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_PHY_LINK_CTRL_ADDR, FPGA_REQ_LINK_TRAINING, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* !LVCMOS_EN*/
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_PHY_TRAIN_DATA_ADDR, PHY_TRAINING_PATTERN_BYTE, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_LINK_TRAIN_BLK_P0_ADDR, CY_DWORD_GET_BYTE0(LINK_TRAINING_PATTERN_BYTE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_LINK_TRAIN_BLK_P1_ADDR, CY_DWORD_GET_BYTE1(LINK_TRAINING_PATTERN_BYTE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_LINK_TRAIN_BLK_P2_ADDR, CY_DWORD_GET_BYTE2(LINK_TRAINING_PATTERN_BYTE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_LINK_TRAIN_BLK_P3_ADDR, CY_DWORD_GET_BYTE3(LINK_TRAINING_PATTERN_BYTE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_TRACE("LINK TRAINING I2C CMD END\n\r\r\n");
#endif /*TRAINING_SEQ_REQ*/
}

/*****************************************************************************
 * Function Name:Cy_FPGA_InitializeRegisters
 ******************************************************************************
 * Summary:
 * This function updates fpga registers to default application parameters
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *
 * Return:
 *  \return void
 *****************************************************************************/
void Cy_FPGA_InitializeRegisters(uint16_t fpgaAddr)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    uint16_t width = 3840;
    uint16_t height = 2160;
    uint8_t fps = 30;

    APP_DBG_FPGA("FPGA (ADDR: 0x%x) I2C REG INIT STARTED\r\n", fpgaAddr);
    Cy_FPGA_UpdateTrainingBytes(fpgaAddr);
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_STREAM_CTRL_ADDR, STREAM_APP_DISABLE, PRINT_FPGA_I2C_LOG); /* Disable stream */
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_FUNC_SELECTION_ADDR, U3V_APP_EN, PRINT_FPGA_I2C_LOG);     /* Write FPGA register to enable U3V */
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#if FPGA_ADDS_HEADER
    /* Enable adding U3V header by FPGA. U3V header is added by FPGA */
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_U3V_HEADER_CTRL_ADDR, U3V_HEADER_BY_FPGA, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_TRACE("U3V_HEADER_BY_FPGA\r\n");

#elif INMD_EN  && (!LVCMOS_EN)
    /* Enable adding U3V header by Insert Metadata */
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_U3V_HEADER_CTRL_ADDR, U3V_HEADER_BY_INMD, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_FPGA("U3V_HEADER_BY_INMD\r\n");
    uint8_t devCounterCrcInfoVal = 0x01;
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_COUNTER_CRC_INFO_ADDR, devCounterCrcInfoVal, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_FPGA("DEV_COUNTER_CRC_INFO_ADDR : 0x%x\r\n", devCounterCrcInfoVal);
#else
    /* Disable adding U3V header by FPGA. U3V header is added by FX10 */
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_U3V_HEADER_CTRL_ADDR, U3V_HEADER_BY_FW, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_FPGA("U3V_HEADER_BY_FX\r\n");
#endif /* FPGA_ADDS_HEADER */

    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_ACTIVE_DEV_ADDR, ACTIVE_DEVICE_MASK, PRINT_FPGA_I2C_LOG);
    /* write FPGA register to Disable format conversion */
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_IMG_CONVERSION_ADDR, NO_CONVERSION, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_FPGA("FPGA GENERATED COLORBAR is default Image Source !!\r\n");
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_STREAM_SOURCE_ADDR, FPGA_GENERATED_CLRBAR, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#if INTERLEAVE_EN
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_ACTIVE_THREAD_INFO_ADDR, ACTIVE_DUAL_THREAD, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_ACTIVE_THREAD_INFO_ADDR, ACTIVE_SINGLE_THREAD, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif
#if PORT1_EN
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD1_INFO_ADDR, CY_LVDS_GPIF_THREAD_2, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD1_INFO_ADDR, CY_LVDS_GPIF_THREAD_0, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif

#if INTERLEAVE_EN
#if PORT1_EN
    /*A device can be connected to maximum 2 threads at once. Device 0 is connected to Thread 0 and Thread 1 */
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD2_INFO_ADDR, CY_LVDS_GPIF_THREAD_3, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    /*A device can be connected to maximum 2 threads at once. Device 0 is connected to Thread 0 and Thread 1 */
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD2_INFO_ADDR, CY_LVDS_GPIF_THREAD_1, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif
#else
    /*A device can be connected to maximum 2 threads at once. Device 0 is connected to Thread 0 only*/
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD2_INFO_ADDR, CY_LVDS_GPIF_THREAD_0, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif

    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD1_SOCKET_INFO_ADDR, THREAD_0_SOCKET_0, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if INTERLEAVE_EN
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD2_SOCKET_INFO_ADDR, THREAD_1_SOCKET_0, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_THREAD2_SOCKET_INFO_ADDR, THREAD_0_SOCKET_0, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif
    /* Clear FPGA register during power up*/
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_SRC_PAR_CFG_ADDR, SRC_PAR_RESET, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if RGB24 || RGB32
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_SRC_PAR_CFG_ADDR, SRC_CLR_FMT_YUV444, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif
#if RGB32
    status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_PIXEL_WIDTH_ADDR, _32BIT_PIXEL, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#elif RGB24
    status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_PIXEL_WIDTH_ADDR, _24BIT_PIXEL, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#else
    status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_PIXEL_WIDTH_ADDR, _16BIT_PIXEL, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* RGB24 */

    if (status == CY_SCB_I2C_SUCCESS)
    {
        APP_DBG_FPGA("FPGA_DMA_BUFFER_SIZE is configured: %d [0x%x] Bytes\r\n", FPGA_DMA_BUFFER_SIZE, FPGA_DMA_BUFFER_SIZE);
    }
    /* Update DMA buffer size used by Firmware */
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_BUFFER_SIZE_MSB_ADDR, CY_GET_MSB(FPGA_DMA_BUFFER_SIZE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_BUFFER_SIZE_LSB_ADDR, CY_GET_LSB(FPGA_DMA_BUFFER_SIZE), PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_FPGA_UpdateFrameWidth(fpgaAddr, width);
    Cy_FPGA_UpdateFrameHeight(fpgaAddr, height);
    Cy_FPGA_UpdateStreamingFps(fpgaAddr, fps);
    
    APP_DBG_FPGA("FPGA I2C REG INIT END\r\n\r\n");
}

/*****************************************************************************
 * Function Name:Cy_FPGA_UpdateFrameParameters
 ******************************************************************************
 * Summary:
 * Updates fpga reg for image frame parameters
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param width
 *   image frame width
 *   \param height
 *   image frame height
 *   \param fps
 *   stream fps
 *   \param sensorType
 *   image sensor connected to fpga mipi port
 *
 * Return:
 *  returns void
 *****************************************************************************/
void Cy_FPGA_UpdateFrameParameters(uint16_t fpgaAddr, uint32_t width, uint32_t height, uint8_t fps, cy_en_sensorType_t sensorType)
{

    /*Update U3V Image parameters for colorbar generation on FPGA*/
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    APP_DBG_FPGA("FPGA I2C REG UPDATE FOR STREAM START\r\n");
    status = Cy_FPGA_WriteRegister(fpgaAddr, FPGA_FUNC_SELECTION_ADDR, U3V_APP_EN, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_FPGA_UpdateFrameWidth(fpgaAddr, width);
    Cy_FPGA_UpdateFrameHeight(fpgaAddr, height);
    Cy_FPGA_UpdateStreamingFps(fpgaAddr, fps);

    status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_U3V_STREAM_MODE_ADDR, STREAM_MODE_CONTINUOUS, PRINT_FPGA_I2C_LOG);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    APP_DBG_FPGA("FRAME(Default): [%d x %d] @ %d FPS\r\n", width, height, fps);

    /* Update video source*/
#if MIPI_INTERFACE_EN
    if (sensorType == SRC_SENSOR_IMX)
    {
        APP_DBG_FPGA("MIPI_SENSOR_IMX_4K\r\n");
        status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_STREAM_SOURCE_ADDR, MIPI_SOURCE, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

        status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_SRC_PAR_CFG_ADDR, VAL_MIPI_CROP_ALGO, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }
    else if (sensorType == SRC_SENSOR_THINE)
    {
        APP_DBG_FPGA("MIPI_SENSOR_THINE\r\n");
        status = Cy_FPGA_WriteRegister(fpgaAddr, DEV_STREAM_SOURCE_ADDR, MIPI_SOURCE, PRINT_FPGA_I2C_LOG);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    }
#endif /* MIPI_INTERFACE_EN */
    APP_DBG_FPGA("FPGA I2C REG UPDATE FOR STREAM END\r\n\r\n");
}

/*****************************************************************************
 * Function Name:Cy_FPGA_GetRtlRevision
 ******************************************************************************
 * Summary:
 * reads fpga rtl revision
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param data
 *   pointer to variable which gets rtl rev
 *
 * Return:
 *  \return void
 *****************************************************************************/
void Cy_FPGA_GetRtlRevision(uint16_t fpgaAddr, uint8_t *data)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_FPGA_ReadRegister(fpgaAddr, FPGA_VER_ADDR, data, false);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

/*****************************************************************************
 * Function Name:Cy_FPGA_PrintStatus
 ******************************************************************************
 * Summary:
 * This function reads fpga current status, fpga sdram and mipi register status
 *
 * Parameters:
 *   \param fpgaAddr
 *   fpga slave I2C address
 *   \param sensorType
 *   image sensor connected to fpga mipi port
 *
 * Return:
 *  \return void
 *****************************************************************************/
void Cy_FPGA_PrintStatus(uint16_t fpgaAddr, cy_en_sensorType_t sensorType)
{
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;
    uint8_t statusByte;
    static uint8_t statusByte1;
    static uint8_t statusByte2;
#if MIPI_INTERFACE_EN
    uint8_t dev0MipiStatus;
    static uint8_t dev0MipiStatus1;
#endif /* MIPI_INTERFACE_EN*/

    bool fpgaConfigFlag = Cy_FPGA_IsConfigurationCompleted();
    if (fpgaConfigFlag)
    {
#if (FPGA_STATUS_PRINT || FX10_FPGA_DETAILED_STATUS)
        {
            status = Cy_FPGA_ReadRegister(fpgaAddr, DEV_STATUS_ADDR, &statusByte, false);
            ASSERT_NON_BLOCK(CY_SCB_I2C_SUCCESS == status, status);
            if (statusByte1 != statusByte)
            {
                statusByte1 = statusByte;
#if FX10_FPGA_DETAILED_STATUS
                APP_DBG_FPGA("FPGA[0x%x]: [DDR_RD: %d, DDR_WR: %d]  [Y_FULL: %d, Y_EMPTY: %d, SL_EMPTY: %d]\n\r",
                             ((statusByte)),
                             ((statusByte >> DDR_RD_FLAG) & 0x01) ? 1 : 0,
                             ((statusByte >> DDR_WR_STATUS) & 0x01) ? 1 : 0,
                             ((statusByte >> Y_FIFO_FULL) & 0x01) ? 1 : 0,
                             ((statusByte >> Y_FIFO_EMPTY) & 0x01) ? 1 : 0,
                             (statusByte >> SF_ALMOST_EMPTY) & 0x01 ? 1 : 0);
#else
                APP_DBG_FPGA("FPGA: [0x%x]\r\n", statusByte);
#endif /* FX10_FPGA_DETAILED_STATUS */
            }
            status = Cy_FPGA_ReadRegister(fpgaAddr, FPGA_EXT_CTLR_STATUS_ADDR, &statusByte, false);
            ASSERT_NON_BLOCK(CY_SCB_I2C_SUCCESS == status, status);
            if (statusByte2 != statusByte)
            {
                statusByte2 = statusByte;
#if FX10_FPGA_DETAILED_STATUS
                APP_DBG_FPGA("EXTC[0x%x]:[PATH_IDLE: %d] [Q_FULL: %d, DDR_BSY: %d, DDR_CFG: %d, DMA_RDY: %d]\n\r",
                             ((statusByte)),
                             ((statusByte >> PATH_IDLE_FLAG) & 0x01) ? 1 : 0,
                             ((statusByte >> Q_FULL_FLAG) & 0x01) ? 1 : 0,
                             ((statusByte >> DDR_BSY_FLAG) & 0x01) ? 1 : 0,
                             ((statusByte >> DDR_CFG_FLAG) & 0x01) ? 1 : 0,
                             (statusByte >> DMA_RDY_FLAG) & 0x01 ? 1 : 0);
#else
                APP_DBG_FPGA("EXTC: [0x%x]\r\n", statusByte);
#endif /* FX10_FPGA_DETAILED_STATUS */
            }
#if MIPI_INTERFACE_EN
            if (sensorType < SRC_FPGA_CLRBAR)
            { /* Only read and print MIPI status when an image sensor is connected*/
                status = Cy_FPGA_ReadRegister(fpgaAddr, DEV_MIPI_STATUS_ADDR, &dev0MipiStatus, false);
                ASSERT_NON_BLOCK(CY_SCB_I2C_SUCCESS == status, status);
                if (dev0MipiStatus1 != dev0MipiStatus)
                {
                    dev0MipiStatus1 = dev0MipiStatus;
                    APP_DBG_FPGA("MIPI: 0x%x \n\r", dev0MipiStatus);
                }
            }
#endif /* MIPI_INTERFACE_EN*/
        }
#endif /* FPGA_STATUS_PRINT || FX10_FPGA_DETAILED_STATUS */
    }
    else
    {
        APP_DBG_ERR("FPGA Configuration failed !!\r\n Check FPGA power supply & make sure binary is flashed properly\r\n");
    }
} /* end of function  */

#endif /* FPGA_ENABLE */
