/***************************************************************************/
/*
 * \file cy_app_fpga.h
 * \version 1.0
 *
 * Provided API declarations for the FPGA interface implementation.
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

#ifndef __CY_APP_FPGA_H__
#define __CY_APP_FPGA_H__
#include "cy_pdl.h"
#include "cy_app_scb.h"
#include "cy_u3v_device_config.h"
#include "cy_app_qspi.h"
#if defined(__cplusplus)
extern "C"
{
#endif
/* Enables detailed info of I2C communications*/
#if (DBG_LEVEL > 2)
#define PRINT_FPGA_I2C_LOG (1)
#else
#define PRINT_FPGA_I2C_LOG (0)
#endif

#define FPGA_SEL_DEV_NUM   (0) /* set 0, or 1 , or 2, or 3 */

#define MAX_DUMMY_CYCLES_COUNT (8192)
#define FPGA_MAX_CONFIG_FILE_SIZE (6262080)

#define FPGA_REG_ADDR_WIDTH (2)
#define FPGA_REG_DATA_WIDTH (1)

#define FPGA_CDONE_PIN (P4_4_PIN)
#define FPGA_CDONE_PORT (P4_4_PORT)

#define FPGA_CRESET_PORT (P4_3_PORT)
#define FPGA_CRESET_PIN (P4_3_PIN)

#define FPGA_CSI_PORT (P6_4_PORT)
#define FPGA_CSI_PIN (P6_4_PIN)

#define FPGA_SSN_PORT (P6_3_PORT)
#define FPGA_SSN_PIN (P6_3_PIN)

#define DEMO_KIT_USER_LED_PORT (P13_1_PORT)
#define DEMO_KIT_USER_LED_PIN (P13_1_PIN)

#define FPGA_SOFTRESET_PORT (P11_5_PORT)
#define FPGA_SOFTRESET_PIN (P11_5_PIN)

#define FPGA_ACTIVE_DEV_OFFSET                  (0x20 + (FPGA_SEL_DEV_NUM * 0x1C))
#define DUMMY_CYCLE                             (150)

#define MAX_DUMMY_CYCLES_COUNT                  (8192)
#define EFINIX_MAX_CONFIG_FILE_SIZE             (6262080)

typedef enum cy_en_fpgaConfigMode
{
        FPGA_CFG_MODE_NONE,
        ACTIVE_SERIAL_MODE,
        PASSIVE_SERIAL_x4_MODE,
        PASSIVE_SERIAL_x8_MODE,
        FPGA_CFG_TRY_ALL_MODE,
        FPGA_CFG_MODE_MAX
}cy_en_fpgaConfigMode_t;

extern cy_en_fpgaConfigMode_t glFpgaConfigMode;
extern char *glFpgaConfigModeString[FPGA_CFG_MODE_MAX];

/* EZ-USB FX Control Center application appends a metadata section to any file it programs to the external flash.
 * This can be used by the application using the flash to find out details such as start address, size etc. about the programmed file */

typedef struct cy_stc_externalFlashMetadata
{
    uint32_t startSignature;
    uint32_t fpgaFileStartAddress;
    uint32_t fpgaFileSize;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t endSignature;
}cy_stc_externalFlashMetadata_t;


#define CY_APP_QSPI_METADATA_ADDRESS            (0x00)
#define CY_APP_QSPI_METADATA_SIZE               (24)
#define CY_APP_QSPI_METADATA_SIGNATURE          (0x23584649) /*IFX#*/
#define CY_FPGA_MAX_CONFIG_WAIT                 (5000)       /*5 seconds*/

typedef cy_en_scb_i2c_status_t cy_stc_fpga_reg_rd_wr_ret_t;

typedef enum cy_en_fpga_register_map
{
        FPGA_VER_ADDR = 0x00,

        FPGA_FUNC_SELECTION_ADDR = 0x01,
        U3V_APP_EN = 0,

        FPGA_U3V_HEADER_CTRL_ADDR = 0x02,
        U3V_HEADER_BY_FW = 0x00,
        U3V_HEADER_BY_FPGA = 0x01,
        U3V_HEADER_BY_INMD = 0x02, /* INMD implemented for LVDS */

        FPGA_PHY_TRAIN_DATA_ADDR = 0x03,
        FPGA_LINK_TRAIN_BLK_P0_ADDR = 0x04,
        FPGA_LINK_TRAIN_BLK_P1_ADDR = 0x05,
        FPGA_LINK_TRAIN_BLK_P2_ADDR = 0x06,
        FPGA_LINK_TRAIN_BLK_P3_ADDR = 0x07,

        FPGA_ACTIVE_DEV_ADDR = 0x08,
        ACTIVE_DEVICE_MASK = 0x01 << FPGA_SEL_DEV_NUM, /* each bit enable correspoding dev, U3V fw supports 1 dev at a time */

        FPGA_LOW_PWR_MODE_ADDR = 0x09,
        VAL_LPM_SUPPORT = 1,

        FPGA_PHY_LINK_CTRL_ADDR = 0x0A,
        FPGA_TRAINING_DISABLE = 0x00,
        FPGA_REQ_PHY_TRAINING = 0x01,
        FPGA_REQ_LINK_TRAINING = 0x02,
        FPGA_REQ_LINK_TRAINING_START = 0x20,
        PORT0_LINK_TRAINING_DONE = 0x42,
        PORT1_LINK_TRAINING_DONE = 0x82,

        FPGA_EXT_CTLR_STATUS_ADDR = 0x0B,
        DMA_RDY_FLAG = 0,
        DDR_CFG_FLAG = 1,
        DDR_BSY_FLAG = 2,
        Q_FULL_FLAG = 3,
        PATH_IDLE_FLAG = 4,
        
        FPGA_RESERVED_ADDRESS                  = 0X0C,
        
        FPGA_LVDS_GEAR_RATIO_ADDRESS           = 0X0D,
        GEAR_RATIO_DEFLT_2_1                   = 0x00,
        GEAR_RATIO_2_1                         = 0x02,
        GEAR_RATIO_4_1                         = 0x04,
        GEAR_RATIO_8_1                         = 0x08,

        DEV_STREAM_CTRL_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x00,
        STREAM_APP_DISABLE = 0x0,
        DMA_CH_RESET = 0x01,
        STREAM_APP_EN = 0x02,
        STREAM_APP_NOTIFY_STOP = 0x06,

        DEV_IMG_CONVERSION_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x01,
        NO_CONVERSION = 0,
        INTERLEAVED_MODE = 0x01,
        STILL_CAPTURE = 0x02,
        MONO_8_CONVERSION = 0x04,
        YUV422_420_CONVERSION = 0x08,

        DEV_IMG_HEIGHT_LSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x02,
        DEV_IMG_HEIGHT_MSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x03,
        DEV_IMG_WIDTH_LSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x04,
        DEV_IMG_WIDTH_MSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x05,

        DEV_FPS_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x06,

        DEV_PIXEL_WIDTH_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x07,
        _8_BIT_PIXEL = 8,
        _12BIT_PIXEL = 12,
        _16BIT_PIXEL = 16,
        _24BIT_PIXEL = 24,
        _32BIT_PIXEL = 32,

        DEV_STREAM_SOURCE_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x08,
        FPGA_GENERATED_CLRBAR = 0x00,
        RESERVE_SOURCE = 0x01,
        MIPI_SOURCE = 0x02,
        OTHER_SOURCE = 3,

        DEV_STATUS_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x09,
        SF_ALMOST_EMPTY = 0,
        Y_FIFO_EMPTY = 1,
        Y_FIFO_FULL = 2,
        DDR_WR_STATUS = 4,
        DDR_RD_FLAG = 5,

        DEV_MIPI_STATUS_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0A,

        DEV_SRC_PAR_CFG_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0B,
        SRC_PAR_RESET = 0x00,
        SRC_CLR_FMT_YUV444 = 0x04,
        VAL_MIPI_CROP_ALGO = 0x20,

        DEV_U3V_STREAM_MODE_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0C,
        STREAM_MODE_CONTINUOUS = 0x01,
        STREAM_MODE_SINGLE = 0x02,
        STREAM_MODE_MULTIFRAME = 0x04,
        MULTIFRAME_DEFAULT_CNT = 0x01,
        MULTIFRAME_SHIFT_VAL = 0x03,
        MAX_MULTIFRAME_COUNT = 0x1F, /* Max multi mode stream cound: bit 7-3 = 31 counts only */

        DEV_U3V_CHUNK_MODE_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0D,
        CHUNK_MODE_DISABLE = 0x00,
        CHUNK_MODE_EN = 0x01,
        CHUNK_TYPE_FRAME_ID = 0x02,
        CHUNK_TYPE_TIME_STAMP = 0x04,

        DEV_U3V_TRIGGER_MODE_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0E,
        TRIGGER_MODE_EN = 0x01,
        TRIGGER_TYPE_SELECT = 0x02, /* 0 for ACQ, 1 for Frame */
        TRIGGER_TYPE_ACQ = 0,
        TRIGGER_TYPE_FRAME = 1,
        TRIGGER_HW_0 = 0x08,
        TRIGGER_HW_1 = 0x10,
        TRIGGER_HW_2 = 0x20,
        TRIGGER_HW_3 = 0x40,
        TRIGGER_SW_EN = 0x00,
        TRIGGER_SW = 0x80,
        TRIGGER_MASK = 0x07,

        DEV_ACTIVE_THREAD_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x0F,
        ACTIVE_SINGLE_THREAD = 0x01,
        ACTIVE_DUAL_THREAD = 0x02,

        DEV_THREAD1_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x10,
        DEV_THREAD2_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x11,
        DEV_THREAD1_SOCKET_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x12,
        DEV_THREAD2_SOCKET_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x13,
        DEV_FLAG_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x14,
        DEV_COUNTER_CRC_INFO_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x15,
        DEV_BUFFER_SIZE_LSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x16,
        DEV_BUFFER_SIZE_MSB_ADDR = FPGA_ACTIVE_DEV_OFFSET + 0x17,

        THREAD_0_SOCKET_0 = 0,
        THREAD_1_SOCKET_0 = 0,
} cy_en_fpga_register_map_t;

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
bool Cy_FPGA_IsConfigurationCompleted(void);

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
void Cy_FPGA_InitConfigurationPins(void);

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
void Cy_FPGA_InitiateConfiguration(cy_en_fpgaConfigMode_t mode);

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
*  \return cy_stc_fpga_reg_rd_wr_ret_t
*****************************************************************************/
cy_stc_fpga_reg_rd_wr_ret_t Cy_FPGA_WriteRegister(uint16_t fpgaAddr, uint16_t regAddr, uint8_t data, bool printFlag);

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
        *  \return cy_stc_fpga_reg_rd_wr_ret_t
        *****************************************************************************/
cy_stc_fpga_reg_rd_wr_ret_t Cy_FPGA_ReadRegister(uint16_t fpgaAddr, uint16_t regAddr, uint8_t *data, bool printFlag);

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
        *  \return void
        *****************************************************************************/
void Cy_FPGA_UpdateFrameWidth(uint16_t fpgaAddr, uint32_t val);

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
        *  \return void
        *****************************************************************************/
void Cy_FPGA_UpdateFrameHeight(uint16_t fpgaAddr, uint32_t val);

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
void Cy_FPGA_UpdateStreamingFps(uint16_t fpgaAddr, uint8_t val);

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
        *  \return void
        *****************************************************************************/
void Cy_FPGA_AppStreamAcqMode(uint16_t fpgaAddr, uint32_t val);

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
void Cy_FPGA_InitializeRegisters(uint16_t fpgaAddr);

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
void Cy_FPGA_PrintStatus(uint16_t fpgaAddr, cy_en_sensorType_t sensorType);

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
        *  \return void
        *****************************************************************************/
void Cy_FPGA_UpdateFrameParameters(uint16_t fpgaAddr, uint32_t width, uint32_t height, uint8_t fps, cy_en_sensorType_t sensorType);

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
void Cy_FPGA_GetRtlRevision(uint16_t fpgaAddr, uint8_t *data);

#if defined(__cplusplus)
}
#endif

#endif /* __CY_APP_FPGA_H__ */
