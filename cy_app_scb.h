/***************************************************************************/
/*
 * \file cy_app_scb.h
 * \version 1.0
 *
 * Provided API declarations for the I2C interface implementation.
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

#ifndef _APP_SCB_H_
#define _APP_SCB_H_

#include "cy_pdl.h"
#if defined(__cplusplus)
extern "C"
{
#endif
#define I2C_SCB SCB0
#define I2C_SCB_INDEX (0)
#define I2C_BUFF_SIZE (10)
#define I2C_DATARATE (100000)
#define I2C_INCLK_TARGET_FREQ (3200000)

    /*****************************************************************************
    * Function Name: Cy_USB_I2CInit(void)
    ******************************************************************************
    * Summary:
    * I2C Master Init function
    *
    * Parameters:
    *  void

    * Return:
    *  Does not return.
    *****************************************************************************/
    void Cy_USB_I2CInit(void);

    /*****************************************************************************
     * Function Name: Cy_I2C_Write
     ******************************************************************************
     * Summary:
     * Function to write data to i2c slave
     *
     * Parameters:
     * \param slaveAddress
     * 7-bit i2c slave address
     * \param registerAddress
     * i2c slave register address
     * \param data
     * pointer to data buffer
     * \param addressWidth
     * address size (in bytes)
     * \param dataWidth
     * data size (in bytes)
     *
     * Return:
     *  0 for read success, error code for unsuccess.
     *****************************************************************************/
    cy_en_scb_i2c_status_t Cy_I2C_Write(uint16_t slaveAddress, uint16_t registerAddress,
                                        uint8_t data,
                                        uint8_t addressWidth,
                                        uint8_t dataWidth,
                                        bool enablePrints);

    /*****************************************************************************
     * Function Name: Cy_I2C_Read
     ******************************************************************************
     * Summary:
     *  Function to read data from i2c slave
     *
     * Parameters:
     * \param slaveAddress
     * 7-bit i2c slave address
     * \param registerAddress
     * i2c slave register address
     * \param data
     * pointer to data receive buffer
     * \param addressWidth
     * address size (in bytes)
     * \param dataWidth
     * data size (in bytes)
     *
     * Return:
     *  0 for read success, error code for unsuccess.
     *****************************************************************************/
    cy_en_scb_i2c_status_t Cy_I2C_Read(uint16_t slaveAddress, uint16_t registerAddress,
                                       uint8_t *data,
                                       uint8_t addressWidth,
                                       uint8_t dataWidth,
                                       bool enablePrints);

#if defined(__cplusplus)
}
#endif

#endif /* _APP_SCB_H_ */
