/***************************************************************************/
/*
 * \file cy_image_sensor.c
 * \version 1.0
 *
 *  Implements the Image sensor related APIs; I2C interface.
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


#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usb_app.h"
#include "cy_u3v_device_config.h"
#include "cy_image_sensor.h"

char *glDataSourceName[DATA_SRC_MAX] = {"SRC_SENSOR_IMX", "SRC_SENSOR_THINE", "SRC_FPGA_CLRBAR", "SRC_MIPI_SENSOR_NA", "SRC_RESERVED"};

/*Set to 1 to enable debug messages during Sensor configuration*/
 #define SENSOR_DEBUG (0)
/*Set to 1 to enable IMX sensor generated color bar*/
#define SENSOR_CLRBAR (0)

/* Misc Macros; Fill the correct I2C addresses below */
#define THINE_I2C_WR_ADDR (0x61)
#define IMX_I2C_WR_ADDR (0x1A)
#define SENSOR_I2C_ADDR_W (2)
#define SENSOR_I2C_DATA_W (1)

#define THINE_CMD_SETUP_DELAY_ms (20)

#if MIPI_INTERFACE_EN

    
/* SonyIMXxxx 4K */
/* Enable only one Data rate option at a time*/
#define DR_2376 (1)
#define DR_1782 (0)
#define DR_1485 (0)
#define DR_594 (0)

/* Image sensor clock*/
#define INCK_27 (1)
#define INCK_74_25 (!INCK_27)

/* Image sensor OUTPUT Bit Width*/
#define RAW10 (DR_2376)
#define RAW12 (!RAW10)

/* Structure to store the Address/Value pair for each
 * register to be updated.
 */
typedef struct cy_stc_sensorReg8
{
    uint16_t regAddr;
    uint8_t regValue;
} cy_stc_sensorReg8_t;

/*
Sensor color bar pattern generator values
    0h: All 000h Pattern
    1h: All FFFh Pattern
    2h: All 555h Pattern
    3h: All AAAh Pattern
    4h: 555h / AAAh Toggle Pattern
    5h: AAAh / 555h Toggle Pattern
    6h: 000h / 555h Toggle Pattern
    7h: 555h / 000h Toggle Pattern
    8h: 000h / FFFh Toggle Pattern
    9h: FFFh / 000h Toggle Pattern
    Ah: Horizontal Color-bar Chart
    Bh: Vertical Color-bar Chart
    {0x30E6, 0x0A},  / TPG_PATSEL_DUOUT [4:0] ( values 0h to Bh)
*/
const cy_stc_sensorReg8_t Cy_ImgSensor_InitRegIMX[] =
{
    /* IMX 4K Image sensor Initialization*/
    {0x3000, 0x01}, /* Enter standby mode */
    /* INCK Settings for 27 MHz*/
#if INCK_27
    {0x3008, 0x5D}, /* BCWAIT_TIME[9:0], 27 MHz */
    {0x300A, 0x42}, /* CPWAIT_TIME[9:0], 27 MHz */
#endif /* INCK_27 */
    {0x301C, 0x00}, /* WINMODE[3:0], All Pixels (4K) */
    {0x3022, 0x00}, /* ADDMODE[1:0], All Pixels (4K) */
    {0x3024, 0xCA}, /* VMAX[19:0], This should be fixed to 0x08CA (2250) */
    {0x3025, 0x08},
    {0x3028, 0x4C}, /* HMAX[15:0], [0x044C = 30 FPS], [0x0294 = 50 FPS], [0x0226 = 60 FPS] */
    {0x3029, 0x04},
    {0x3030, 0x00}, /* HREVERSE[0], VREVERSE[1]; 0 Normal, 1 Inverted */

#if RAW10
    {0x3031, 0x00}, /* ADBIT[1:0] */
    {0x3032, 0x00}, /* MDBIT[1:0] */
#else
    {0x3031, 0x01}, /* ADBIT[1:0] */
    {0x3032, 0x01}, /* MDBIT[1:0] */
#endif /* RAW10 */

#if DR_2376
    {0x3033, 0x00}, /* SYS_MODE[3:0] */
#elif DR_1782
    {0x3033, 0x04}, /* SYS_MODE[3:0] */
#elif DR_1485
    {0x3033, 0x08}, /* SYS_MODE[3:0] */
#else  /* DR_594 */
    {0x3033, 0x07}, /* SYS_MODE[3:0] */
#endif /* DR_2376 */

    {0x3050, 0x4B}, /* SHR0[19:0] */
    {0x3051, 0x00}, /* SHR0[19:0] */
    {0x3052, 0x00}, /* SHR0[19:0] */
    {0x30C1, 0x00}, /* XVS_DRV[1:0] */
    {0x30CC, 0x00}, /* XVS_LNG[1:0] [5:4], 1h: 0x08 2h:0x10 3h : 0x18 */
    {0x30CD, 0x00}, /* XHS_LNG[1:0] [5:4] */

#if SENSOR_CLRBAR
    {0x30E2, 0x00}, /* BLKLEVEL [7:0], for PG mode set to "000h" */
    {0x30E3, 0x00}, /* BLKLEVEL [1:0] */
    {0x30E4, 0x01}, /* TPG_EN_DUOUT [0] */
    {0x30E6, 0x0A}, /* TPG_PATSEL_DUOUT [4:0] ( values 0h to Bh) */
    {0x30E8, 0x00}, /* TPG_COLORWIDTH [1:0](0 :120, 1:240, 2:480) pxl */
    {0x3110, 0x20}, /* TESTCLKEN_MIPI [5] */
    {0x32C8, 0x00}, /* DIG_CLP_MODE [0] */
    {0x3390, 0x00}, /* WRJ_OPEN [0] */
#endif  /* SENSOR_CLRBAR */

#if INCK_27
    /* INCK Settings for 27 MHz*/
    {0x3115, 0x00}, /* INCKSEL2[7:0] */
    {0x3116, 0x23}, /* INCKSEL2[7:0] */
#if DR_2376
    {0x3118, 0x08}, /* INCKSEL3[10:0] */
    {0x3119, 0x01}, /* INCKSEL3[10:0] */
#elif DR_1782
    {0x3118, 0xC6}, /* INCKSEL3[10:0] */
#elif DR_1485
    {0x3118, 0xA5}, /* INCKSEL3[10:0] */
#else /* DR_594 */
    {0x3118, 0x84}, /* INCKSEL3[10:0] */
#endif /* DR_2376 */
    {0x311A, 0xE7}, /* INCKSEL4[10:0] */
    {0x311E, 0x23}, /* INCKSEL5[7:0] */
#endif /* INCK_27 */

    {0x32D4, 0x21},
    {0x32EC, 0xA1},
    {0x344C, 0x2B},
    {0x344D, 0x01},
    {0x344E, 0xED},
    {0x344F, 0x01},
    {0x3450, 0xF6},
    {0x3451, 0x02},
    {0x3452, 0x7F},
    {0x3453, 0x03},
    {0x358A, 0x04},
    {0x35A1, 0x02},
    {0x35EC, 0x27},
    {0x35EE, 0x8D},
    {0x35F0, 0x8D},
    {0x35F2, 0x29},
    {0x36BC, 0x0C},
    {0x36CC, 0x53},
    {0x36CD, 0x00},
    {0x36CE, 0x3C},
    {0x36D0, 0x8C},
    {0x36D1, 0x00},
    {0x36D2, 0x71},
    {0x36D4, 0x3C},
    {0x36D6, 0x53},
    {0x36D7, 0x00},
    {0x36D8, 0x71},
    {0x36DA, 0x8C},
    {0x36DB, 0x00},

#if RAW10
    {0x3701, 0x00}, /* ADBIT1[7:0] */
#else
    {0x3701, 0x01}, /* ADBIT1[7:0] */
#endif /* RAW10 */

    /* Unknown reg name; provided by sony */
    {0x3720, 0x00},
    {0x3724, 0x02},
    {0x3726, 0x02},
    {0x3732, 0x02},
    {0x3734, 0x03},
    {0x3736, 0x03},
    {0x3742, 0x03},
    {0x3862, 0xE0},
    {0x38CC, 0x30},
    {0x38CD, 0x2F},
    {0x395C, 0x0C},
    {0x39A4, 0x07},
    {0x39A8, 0x32},
    {0x39AA, 0x32},
    {0x39AC, 0x32},
    {0x39AE, 0x32},
    {0x39B0, 0x32},
    {0x39B2, 0x2F},
    {0x39B4, 0x2D},
    {0x39B6, 0x28},
    {0x39B8, 0x30},
    {0x39BA, 0x30},
    {0x39BC, 0x30},
    {0x39BE, 0x30},
    {0x39C0, 0x30},
    {0x39C2, 0x2E},
    {0x39C4, 0x2B},
    {0x39C6, 0x25},
    {0x3A42, 0xD1},
    {0x3A4C, 0x77},
    {0x3AE0, 0x02},
    {0x3AEC, 0x0C},
    {0x3B00, 0x2E},
    {0x3B06, 0x29},
    {0x3B98, 0x25},
    {0x3B99, 0x21},
    {0x3B9B, 0x13},
    {0x3B9C, 0x13},
    {0x3B9D, 0x13},
    {0x3B9E, 0x13},
    {0x3BA1, 0x00},
    {0x3BA2, 0x06},
    {0x3BA3, 0x0B},
    {0x3BA4, 0x10},
    {0x3BA5, 0x14},
    {0x3BA6, 0x18},
    {0x3BA7, 0x1A},
    {0x3BA8, 0x1A},
    {0x3BA9, 0x1A},
    {0x3BAC, 0xED},
    {0x3BAD, 0x01},
    {0x3BAE, 0xF6},
    {0x3BAF, 0x02},
    {0x3BB0, 0xA2},
    {0x3BB1, 0x03},
    {0x3BB2, 0xE0},
    {0x3BB3, 0x03},
    {0x3BB4, 0xE0},
    {0x3BB5, 0x03},
    {0x3BB6, 0xE0},
    {0x3BB7, 0x03},
    {0x3BB8, 0xE0},
    {0x3BBA, 0xE0},
    {0x3BBC, 0xDA},
    {0x3BBE, 0x88},
    {0x3BC0, 0x44},
    {0x3BC2, 0x7B},
    {0x3BC4, 0xA2},
    {0x3BC8, 0xBD},
    {0x3BCA, 0xBD},
    {0x4000, 0x11},

    /* Lane mode: 2 lane o4 4 lane*/
    {0x4001, 0x03}, /* LANEMODE[2:0], 4 Lane */

    /*  Global Timing settings; need to update INCKSEL6 and INCKSEL7 reg for INCK settings
        Below val are for 27 MHz
    */
    {0x4004, 0xC0}, /* TXCLKESC_FREQ[15:0] */
    {0x4005, 0x06},
#if DR_2376
    {0x400C, 0x01}, /* INCKSEL6 */
    {0x4018, 0xE7}, /* TCLKPOST[15:0] */
    {0x401A, 0x8F}, /* TCLKPREPARE[15:0] */
    {0x401C, 0x8F}, /* TCLKTRAIL[15:0] */
    {0x401E, 0x7F}, /* TCLKZERO[15:0] */
    {0x401F, 0x02},
    {0x4020, 0x97}, /* THSPREPARE[15:0] */
    {0x4022, 0x0F}, /* THSZERO[15:0] */
    {0x4023, 0x01},
    {0x4024, 0x97}, /* THSTRAIL[15:0] */
    {0x4026, 0xF7}, /* THSEXIT[15:0] */
    {0x4028, 0x7F}, /* TLPX[15:0] */
    {0x4074, 0x00}, /* INCKSEL7 */
#elif DR_1782
    {0x400C, 0x01}, /* INCKSEL6 */
    {0x4018, 0xB7}, /* TCLKPOST[15:0] */
    {0x401A, 0x67}, /* TCLKPREPARE[15:0] */
    {0x401C, 0x6F}, /* TCLKTRAIL[15:0] */
    {0x401E, 0xDF}, /* TCLKZERO[15:0] */
    {0x401F, 0x01},
    {0x4020, 0x6F}, /* THSPREPARE[15:0] */
    {0x4022, 0xCF}, /* THSZERO[15:0] */
    {0x4024, 0x6F}, /* THSTRAIL[15:0] */
    {0x4026, 0xB7}, /* THSEXIT[15:0] */
    {0x4028, 0x5F}, /* TLPX[15:0] */
    {0x4074, 0x00}, /* INCKSEL7 */
#elif DR_1485
    {0x400C, 0x01}, /* INCKSEL6 */
    {0x4018, 0xA7}, /* TCLKPOST[15:0] */
    {0x401A, 0x57}, /* TCLKPREPARE[15:0] */
    {0x401C, 0x5F}, /* TCLKTRAIL[15:0] */
    {0x401E, 0x97}, /* TCLKZERO[15:0] */
    {0x401F, 0x01},
    {0x4020, 0x5F}, /* THSPREPARE[15:0] */
    {0x4022, 0xAF}, /* THSZERO[15:0] */
    {0x4024, 0x5F}, /* THSTRAIL[15:0] */
    {0x4026, 0x9F}, /* THSEXIT[15:0] */
    {0x4028, 0x4F}, /* TLPX[15:0] */
    {0x4074, 0x00}, /* INCKSEL7 */
#else /* DR_594 */
    {0x400C, 0x00}, /* INCKSEL6 */
    {0x4018, 0x67}, /* TCLKPOST[15:0] */
    {0x401A, 0x27}, /* TCLKPREPARE[15:0] */
    {0x401C, 0x27}, /* TCLKTRAIL[15:0] */
    {0x401E, 0xB7}, /* TCLKZERO[15:0] */
    {0x401F, 0x00},
    {0x4020, 0x2F}, /* THSPREPARE[15:0] */
    {0x4022, 0x4F}, /* THSZERO[15:0] */
    {0x4024, 0x2F}, /* THSTRAIL[15:0] */
    {0x4026, 0x47}, /* THSEXIT[15:0] */
    {0x4028, 0x27}, /* TLPX[15:0] */
    {0x4074, 0x01}, /* INCKSEL7 */
#endif /* DR_2376 */
    {0x3000, 0x00}, /* Exit standby mode */
};

/* SonyIMX 4K Image sensor Configuration settings for 3864 x 2229 video stream */
const cy_stc_sensorReg8_t Cy_ImgSensor_Res0ConfigIMX[] ={

    {0x3000, 0x01}, /* Enter standby mode */
    {0x301C, 0x00}, /* WINMODE[3:0] */
    {0x3022, 0x00}, /* ADDMODE[1:0] */
    {0x3000, 0x00}, /* Exit standby mode */

};

/* SonyIMX 4K Image sensor Configuration settings for 1944 x 1116 video stream */
const cy_stc_sensorReg8_t Cy_ImgSensor_Res1ConfigIMX[] ={
    {0x3000, 0x01}, /* Enter standby mode */
    {0x3020, 0x01}, /* HADD */
    {0x3021, 0x01}, /* VADD */
    {0x3022, 0x01}, /* ADDMODE[1:0] */
    {0x3000, 0x00}, /* Exit standby mode */
};

/* SonyIMX 4K Image sensor start video stream  */
const cy_stc_sensorReg8_t Cy_ImgSensor_StartOperationIMX [] ={
    {0x3002, 0x00},
};

/* SonyIMX 4K standby enable register */
const cy_stc_sensorReg8_t Cy_ImgSensor_StandbyEnableIMX [] ={
    {0x3000, 0x01}, /* Enter standby mode */
};

/* SonyIMX 4K standby disable register */
const cy_stc_sensorReg8_t Cy_ImgSensor_StandbyDisableIMX [] ={
    {0x3000, 0x00}, /* Exit standby mode */
};

/*****************************************************************************
 * Function Name: cy_ImgSensor_ConfigReg8
 *****************************************************************************
 * Description: This function writes sensor parameters
 *
 * Parameters:
 * \param configSettings
 * sensors reg and value array
 *
 * \param configSettingsSize
 * size of configuration array
 *
 * \param addr
 * i2c address of sensor
 *
 * Return:
 *  void
 ****************************************************************************/
static cy_en_scb_i2c_status_t cy_ImgSensor_ConfigReg8(const cy_stc_sensorReg8_t *configSettings, uint16_t configSettingsSize, uint8_t addr)
{
    cy_en_scb_i2c_status_t tempStatus = CY_SCB_I2C_SUCCESS;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    uint16_t regCounter = 0;
    uint16_t sensor_reg_add = 0;
    uint8_t rwBuffer;

    for (regCounter = 0; regCounter < configSettingsSize; regCounter++)
    {
#if SENSOR_DEBUG
        APP_DBG_TRACE1("\r\nReg= 0x%x: Value= 0x%x\r\n", configSettings[regCounter].regAddr, configSettings[regCounter].regValue);
#endif /* SENSOR_DEBUG */
        sensor_reg_add = configSettings[regCounter].regAddr;
        rwBuffer = configSettings[regCounter].regValue;
        status = Cy_I2C_Write(addr, sensor_reg_add, rwBuffer, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        vTaskDelay(pdMS_TO_TICKS(1));
        tempStatus |= status;
    }
    return tempStatus;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_IMX_Init
 *****************************************************************************
 * Description: IMX sensor initialization
 *
 * Parameters:
 * \param void
 * none
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_IMX_Init(void)
{
    uint16_t configSize;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    configSize = (sizeof(Cy_ImgSensor_InitRegIMX)) / (sizeof(cy_stc_sensorReg8_t));
    status = cy_ImgSensor_ConfigReg8(Cy_ImgSensor_InitRegIMX, configSize, IMX_I2C_WR_ADDR);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_SetRes0IMX
 *****************************************************************************
 * Description: This function set 4k frame size and fps
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_SetRes0IMX(uint8_t fps)
{
    uint16_t configSize;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    configSize = (sizeof(Cy_ImgSensor_Res0ConfigIMX))/(sizeof(cy_stc_sensorReg8_t));
    status =  cy_ImgSensor_ConfigReg8(Cy_ImgSensor_Res0ConfigIMX, configSize, IMX_I2C_WR_ADDR);
    vTaskDelay (pdMS_TO_TICKS(1));
    Cy_ImgSensor_UpdateIMXFps(fps);
    return status;
}
/*****************************************************************************
 * Function Name: Cy_ImgSensor_SetRes1IMX
 *****************************************************************************
 * Description: This function set 4k/8k frame size and fps
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_SetRes1IMX(uint8_t fps)
{
    uint16_t configSize;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    configSize = (sizeof(Cy_ImgSensor_Res1ConfigIMX))/(sizeof(cy_stc_sensorReg8_t));
    status =  cy_ImgSensor_ConfigReg8(Cy_ImgSensor_Res1ConfigIMX, configSize, IMX_I2C_WR_ADDR);
    vTaskDelay (pdMS_TO_TICKS(1));
    Cy_ImgSensor_UpdateIMXFps(fps);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_UpdateResIMX
 ******************************************************************************
 * Summary:
 * This function writes frame info to sensor register
 *
 * Parameters:
 * \param width
 * frame width
 * \param height
 * frame height
 * \param fps
 * stream fps
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateResIMX(uint16_t width, uint16_t height, uint8_t fps)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_MASTER_MANUAL_TIMEOUT;
    uint8_t updateRes0 = 0;

    if(((height == 1080) && (width == 1920)) ||
        ((height == 2160) && (width == 3840)))
    {
        if (height == 2160){
            updateRes0 = 1;
        }else if (height == 1080){
            updateRes0 = 0;
        } else {
            APP_DBG_ERR("IMX SENSOR does not support %dp resolution\r\n", height);
            return CY_SCB_I2C_BAD_PARAM;
        }
    }

    APP_DBG_SENSOR("IMX Sensor configure[Res%d]: Height=%d, Width:=%d and FPS=%d\r\n", !(updateRes0), height, width, fps);

    if (updateRes0){
        status = Cy_ImgSensor_SetRes0IMX(fps);
    }else {
        status = Cy_ImgSensor_SetRes1IMX(fps);
    }
    if (status != CY_SCB_I2C_SUCCESS){
        APP_DBG_ERR("IMX SENSOR INIT FAILED\n\r\r\n");
    }else{
        APP_DBG_TRACE("IMX SENSOR INIT DONE\r\n\r\n");
    }
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_StartStreamingIMX
 ******************************************************************************
 * Summary:
 * This function starts sensor streaming
 *
 * Parameters:
 * \param void
 * none
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_StartStreamingIMX(void)
{
    uint16_t configSize;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    configSize = (sizeof(Cy_ImgSensor_StandbyDisableIMX)) / (sizeof(cy_stc_sensorReg8_t));
    status = cy_ImgSensor_ConfigReg8(Cy_ImgSensor_StandbyDisableIMX, configSize, IMX_I2C_WR_ADDR);
    vTaskDelay(pdMS_TO_TICKS(1));
    configSize = (sizeof(Cy_ImgSensor_StartOperationIMX)) / (sizeof(cy_stc_sensorReg8_t));
    status = cy_ImgSensor_ConfigReg8(Cy_ImgSensor_StartOperationIMX, configSize, IMX_I2C_WR_ADDR);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_EnterStandbyModeIMX
 ******************************************************************************
 * Summary:
 * This function puts sensor in to standby mode to save power
 *
 * Parameters:
 * \param void
 * none
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_EnterStandbyModeIMX(void)
{
    uint16_t configSize;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    configSize = (sizeof(Cy_ImgSensor_StandbyEnableIMX)) / (sizeof(cy_stc_sensorReg8_t));
    status = cy_ImgSensor_ConfigReg8(Cy_ImgSensor_StandbyEnableIMX, configSize, IMX_I2C_WR_ADDR);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_UpdateIMXFps
 ******************************************************************************
 * Summary:
 * This function writes stream fps to sensor register
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateIMXFps(uint32_t fps)
{
    cy_en_scb_i2c_status_t status;
#if RAW10
    uint8_t maxSupportedFps = 90;
#else
    uint8_t maxSupportedFps = 60;
#endif
    if (fps > maxSupportedFps)
    {
        APP_DBG_ERR("IMX Sensor does not support %d FPS, setting max %d fps\r\n", fps, maxSupportedFps);
        fps = maxSupportedFps;
    }
    uint16_t multiplier = (60 * (0x0226)); /* 60 FPS * hmax value for 60 fps */
    uint16_t hmax = (multiplier / fps);
    uint8_t temp = 0;
    temp = (hmax & 0xFF); /* LSB */
    status = Cy_I2C_Write(IMX_I2C_WR_ADDR, 0x3028, temp, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(1));

    temp = ((hmax >> 8) & 0xFF); /* MSB */
    status = Cy_I2C_Write(IMX_I2C_WR_ADDR, 0x3029, temp, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_SENSOR("IMX Sensor FPS changed to %d\r\n", fps);

    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_UpdateTHineFps
 *****************************************************************************
 * Description: This function set fps
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateTHineFps(uint32_t fps)
{
    cy_en_scb_i2c_status_t status;
    if (fps == 60)
    {
        status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF00F, 0x82, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    }
    else
    {
        fps = 30;
        status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF00F, 0x81, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    }
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_SENSOR("THine FPS changed to %d\r\n", fps);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_Set4kResTHine
 *****************************************************************************
 * Description: This function set 4k frame size and fps
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_Set4kResTHine(uint8_t fps)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF00D, 0x0D, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_ImgSensor_UpdateTHineFps(fps);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_SENSOR("THine SENSOR CONFIG: 2160p, %d FPS\r\n", fps);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_Set1080pResTHine
 *****************************************************************************
 * Description: This function set 1080p frame size and fps
 *
 * Parameters:
 * \param fps
 * stream fps
 *
 * Return:
 *  cy_en_scb_i2c_status_t
 ****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_Set1080pResTHine(uint8_t fps)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF00D, 0x0B, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_ImgSensor_UpdateTHineFps(fps);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    APP_DBG_SENSOR("THine SENSOR CONFIG: 1080p, %d FPS\r\n", fps);
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_UpdateResTHine
 ******************************************************************************
 * Summary:
 * This function writes frame info to sensor register
 *
 * Parameters:
 * \param width
 * frame width
 * \param height
 * frame height
 * \param fps
 * stream fps
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateResTHine(uint16_t width, uint16_t height, uint8_t fps)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF0F6, 0x1, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF008, 0x00, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    if (((height == 1080) && (width == 1920)) ||
        ((height == 2160) && (width == 3840)))
    {
        if (height == 1080)
        {
            if (fps > 60)
            {
                fps = 60;
            }
            status = Cy_ImgSensor_Set1080pResTHine(fps);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        else if (height == 2160)
        {
            fps = 30; /* 4k30 YUV422, at 4k THine fps is fixed to 30 */
            status = Cy_ImgSensor_Set4kResTHine(fps);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        else /* 3M 30 fps YUV422; Not implemented */
        { 
            APP_DBG_ERR("THine SENSOR res not supported\r\n");
        }
    }
    return status;
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_StartStreamingTHine
 ******************************************************************************
 * Summary:
 * This function writes THine reg to start streaming
 *
 * Parameters:
 * \param 
 * none
 *
 * Return:
 * none
 *****************************************************************************/
void Cy_ImgSensor_StartStreamingTHine(void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF008, 0x01, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_StopStreamingTHine
 ******************************************************************************
 * Summary:
 * This function writes THine reg to start streaming
 *
 * Parameters:
 * \param 
 * none
 *
 * Return:
 * none
 *****************************************************************************/
void Cy_ImgSensor_StopStreamingTHine(void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF008, 0x00, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_THine_Init
 ******************************************************************************
 * Summary:
 * This function writes frame info to sensor register
 *
 * Parameters:
 * \param 
 * none
 *
 * Return:
 * cy_en_scb_i2c_status_t
 *****************************************************************************/
void Cy_ImgSensor_THine_Init(void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    APP_DBG_TRACE("THine SENSOR CONFIG START [2160p]\r\n");
    /* THine sensor configuration requires a delay for booting/setting a register value, first bootup after power cycle is around 4s*/
    /* THine sensor provides status of settings written successfully over I2C read, but Not implemented yet*/
    /*Assign MIPI TX and RX lanes correctly given by THine*/
    /* Added a delay instead of reading I2C status reg after every config write*/
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF0F8, 0x2D, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF0F9, 0x36, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF0F6, 0x1, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF008, 0x00, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    /* 4k30 YUV422, at 4k THine fps is fixed to 30 */
    Cy_ImgSensor_Set4kResTHine(30);

    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF009, 0x04, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    vTaskDelay(pdMS_TO_TICKS(THINE_CMD_SETUP_DELAY_ms));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF010, 0x01, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, IMG_SENSOR_LOG_EN);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if (status != CY_SCB_I2C_SUCCESS)
    {
        APP_DBG_ERR("THine SENSOR CONFIG FAILED\n\r\r\n");
    }
    else
    {
        APP_DBG_TRACE("THine SENSOR CONFIG DONE\r\n\r\n");
    }
}

/*****************************************************************************
 * Function Name: Cy_ImgSensor_Init
 ******************************************************************************
 * Summary:
 * This function writes frame info to sensor register
 *
 * Parameters:
 * \param sensorType
 * sensor type connected to hardware
 * \param width
 * frame width
 * \param height
 * frame height
 * \param fps
 * stream fps
 *
 * Return:
 * none
 *****************************************************************************/
void Cy_ImgSensor_Init(cy_en_sensorType_t sensorType, uint16_t width, uint16_t height, uint8_t fps)
{
    if(sensorType == SRC_SENSOR_IMX){
        Cy_ImgSensor_IMX_Init();
        vTaskDelay(pdMS_TO_TICKS(10));
        Cy_ImgSensor_UpdateResIMX(width, height, fps);
    } else if (sensorType == SRC_SENSOR_THINE){
        Cy_ImgSensor_THine_Init();
        vTaskDelay(pdMS_TO_TICKS(10));
        Cy_ImgSensor_UpdateResTHine(width, height, fps);
    } else{
        APP_DBG_TRACE("\r\n");
    }
}
#endif /* MIPI_INTERFACE_EN*/

/*****************************************************************************
 * Function Name: Cy_ImgSensor_GetConnectedSensor
 ******************************************************************************
 * Summary:
 * This function returns info of MIPI image sensor connected to the hardware
 *
 * Parameters:
 * \param void
 * None
 *
 * Return:
 * cy_en_sensorType_t
 *****************************************************************************/
cy_en_sensorType_t Cy_ImgSensor_GetConnectedSensor(void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    APP_DBG_SENSOR("Scanning for connected MIPI-SENSOR\r\n");
    /* Read/write Sony IMX 4K Sensor */
    status = Cy_I2C_Write(IMX_I2C_WR_ADDR, 0x3025, 0x08, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, false);

    /* Since both sensors return success on known I2C addr, differentiate the sensor based upon build macro*/
    if (status == CY_SCB_I2C_SUCCESS){    
        APP_DBG_SENSOR("SENSOR TYPE: [%s]\r\n", glDataSourceName[SRC_SENSOR_IMX]);
        return SRC_SENSOR_IMX;
    }
    else
    {
        APP_DBG_SENSOR("MIPI-SENSOR @ 0x%x is not available\r\n", IMX_I2C_WR_ADDR);
    }

    /* Read/write THine Sensor */
    vTaskDelay(pdMS_TO_TICKS(1000));
    status = Cy_I2C_Write(THINE_I2C_WR_ADDR, 0xF0F8, 0x2D, SENSOR_I2C_ADDR_W, SENSOR_I2C_DATA_W, false);
    if (status == CY_SCB_I2C_SUCCESS)
    {
        APP_DBG_SENSOR("SENSOR TYPE: [%s]\r\n", glDataSourceName[SRC_SENSOR_THINE]);
        return SRC_SENSOR_THINE;
    }
    else
    {
        APP_DBG_SENSOR("MIPI-SENSOR @ 0x%x is not available\r\n", THINE_I2C_WR_ADDR);
    }
    return SRC_MIPI_SENSOR_NA;
}

/* End of file */




