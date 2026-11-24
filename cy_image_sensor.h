/***************************************************************************/
/*
 * \file cy_image_sensor.h
 * \version 1.0
 *
 * Provided API declarations for the image sensor interface implementation.
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

#ifndef _CY_IMAGESENSOR_H_
#define _CY_IMAGESENSOR_H_

#include "cy_pdl.h"
#if defined(__cplusplus)
extern "C"
{
#endif
/* Enables detailed info of I2C communications*/
#if (DBG_LEVEL > 2)
#define IMG_SENSOR_LOG_EN (1)
#else
#define IMG_SENSOR_LOG_EN (0)
#endif
    typedef enum cy_en_sensorType_t
    {
        SRC_SENSOR_IMX,
        SRC_SENSOR_THINE,
        SRC_FPGA_CLRBAR,
        SRC_MIPI_SENSOR_NA,
        SRC_RESERVED,
        DATA_SRC_MAX
    } cy_en_sensorType_t;

    extern char *glDataSourceName[DATA_SRC_MAX];

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
    cy_en_sensorType_t Cy_ImgSensor_GetConnectedSensor(void);

#if MIPI_INTERFACE_EN
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
    cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateResIMX(uint16_t width, uint16_t height, uint8_t fps);

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
    cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateResTHine(uint16_t width, uint16_t height, uint8_t fps);

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
    void Cy_ImgSensor_StartStreamingTHine(void);

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
    void Cy_ImgSensor_StopStreamingTHine(void);

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
    cy_en_scb_i2c_status_t Cy_ImgSensor_UpdateIMXFps(uint32_t fps);

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
    cy_en_scb_i2c_status_t Cy_ImgSensor_EnterStandbyModeIMX(void);

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
    cy_en_scb_i2c_status_t Cy_ImgSensor_StartStreamingIMX(void);

    /*****************************************************************************
     * Function Name: Cy_ImgSensor_UpdateResTHine
     ******************************************************************************
     * Summary:
     * This function writes frame info to sensor register
     *
     * Parameters:
     * \param
     * void
     *
     * Return:
     * cy_en_scb_i2c_status_t
     *****************************************************************************/
    void Cy_ImgSensor_THine_Init(void);

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
    void Cy_ImgSensor_Init(cy_en_sensorType_t sensorType, uint16_t width, uint16_t height, uint8_t fps);
#endif /* MIPI_INTERFACE_EN*/

#if defined(__cplusplus)
}
#endif

#endif /* _INCLUDED_CY__IMAGESENSOR_H_ */

/*[]*/
