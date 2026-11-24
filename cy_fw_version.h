/***************************************************************************/
/*
 * \file cy_app_fw.h
 * \version 1.0
 *
 * Provided u3v application firmware version.
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

#ifndef _FW_VERSION_H_
#define _FW_VERSION_H_
#if defined(__cplusplus)
extern "C"
{
#endif
#define FW_VERSION_MAJOR (1u)
#define FW_VERSION_MINOR (0u)
#define FW_VERSION_PATCH (0u)
#define FW_VERSION_BUILD (1)

#define FW_VERSION_NUM ((FW_VERSION_MAJOR << 28u) | (FW_VERSION_MINOR << 24u) | \
                        (FW_VERSION_PATCH << 16u) | (FW_VERSION_BUILD))

    /*
    1.0.0.00
        - FW has support for FX20, FX10, FX5N & FX5 silicon
        - Has support for image sensors:  VC IMX415, THine, FPGA Generated Colorbar
        - Has support of U3V features: Single/Multi/Continuous, Trigger (software/hardware), chunk data (frame, timestamp)
        - U3V DCI and DSI implementation is in U3V lib
        - Support both the Hardwares : DVK and Solution Demo
        - Can be compiled for LVDS, LVCMOS (DDR, SDR), Widelink/Narrow link Receiver only
        - Can be compiled for USB 3.2 Gen 1x1, or 1x2, or 2x1, or 2x2 or USB HS
    */

#if defined(__cplusplus)
}
#endif

#endif /* _FW_VERSION_H_ */
