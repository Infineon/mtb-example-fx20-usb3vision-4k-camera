/***************************************************************************/
/*
 * \file cy_usb_descriptors.h
 * \version 1.0
 *
 * Defines USB descriptors used by the u3v firmware application.
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

#ifndef _CY_USB_DESCRIPTORS_H_
#define _CY_USB_DESCRIPTORS_H_

#include "cy_pdl.h"
#include "cy_u3v_device_config.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define USB3_DESC_ATTRIBUTES __attribute__((section(".descSection"), used)) __attribute__((aligned(32)))
#define HBDMA_BUF_ATTRIBUTES __attribute__((section(".hbBufSection"), used)) __attribute__((aligned(32)))

/* U3V Device0 should be enabled by default. */
#define U3V_DEV_ENABLE (1)

#define CY_FX_USB_VID (0x04B4)
#define CY_FX_USB_PID (0x4882)
#define USB_CURRENT_SPEED (0x40)
#define USB_SPEED_SUPPORT (0x7C) /* USB Speed Bits: Gen2x2,Gen1x2,Gen2x1, Gen1x1,HS,FS,LS supported */

#define CY_USB_USB2_EXTN_CAPB_TYPE (0x02) /* USB 2.0 extension descriptor. */
#define CY_USB_SS_CAPB_TYPE (0x03)        /* Super speed USB specific device level capabilities. */
#define CY_USB_EP_BULK (0x02)             /* Bulk Endpoint Type */

#define U3V_INTERFACE_NUM (2)     /* Control and Stream */
#define TOTAL_U3V_INTERFACES_FOR_DEV0 (U3V_INTERFACE_NUM * U3V_DEV_ENABLE)

/* Interface numbers*/
#define U3V_DEV_DCINTF_INDEX (0)
#define U3V_DEV_DSINTF_INDEX (U3V_DEV_DCINTF_INDEX + 1)
#define U3V_DEV_VENDOR_INDEX (U3V_DEV_DSINTF_INDEX + (1 * CY_VENDOR_IF_EN))

#define NUM_INTERFACES_CDC (2)

/* Minimum length of USB-SS Configuration Descriptor for U3V device. */
#define USB_CFGDSCR_BASE_LENGTH (9u)
#define U3V_SS_CFGDSCR_BASE_LENGTH (85u)

/* Minimum length of USB-HS Configuration Descriptor for U3V device. */
#define U3V_HS_CFGDSCR_BASE_LENGTH (67u)

#define USB_CONFIG_DSCR_VENDOR_LEN (9)

#define NUM_INTERFACES_VENDOR (1)

/* Total number of interfaces supported by the SS device. */
#define USBSS_NUM_INTERFACES (TOTAL_U3V_INTERFACES_FOR_DEV0 + \
                              (CY_VENDOR_IF_EN * NUM_INTERFACES_VENDOR))

/* Total number of interfaces supported by the HS device. */
#define USBHS_NUM_INTERFACES (TOTAL_U3V_INTERFACES_FOR_DEV0 + \
                              (CY_VENDOR_IF_EN * NUM_INTERFACES_VENDOR))

/* USB-SS configuration descriptor length. */
#define USBSS_CFG_DSCR_LENGTH (                                \
    USB_CFGDSCR_BASE_LENGTH +                                  \
    (U3V_DEV_ENABLE * U3V_SS_CFGDSCR_BASE_LENGTH) +            \
    (CY_VENDOR_IF_EN * USB_CONFIG_DSCR_VENDOR_LEN))

/* USB-HS configuration descriptor length. */
#define USBHS_CFG_DSCR_LENGTH (                                \
    USB_CFGDSCR_BASE_LENGTH +                                  \
    (U3V_DEV_ENABLE * U3V_HS_CFGDSCR_BASE_LENGTH) +            \
    (CY_VENDOR_IF_EN * USB_CONFIG_DSCR_VENDOR_LEN))

    typedef enum
    {
        STR_INDEX_LANG_ID,
        STR_INDEX_GUID,
        STR_INDEX_VENDOR,
        STR_INDEX_MODEL,
        STR_INDEX_FAMILY,
        STR_INDEX_DEV_VER,
        STR_INDEX_SERIAL_NO,
        STR_INDEX_FUNCTION_DEF,
        STR_INDEX_SS_STR,
        STR_INDEX_HS_STR,
        STR_INDEX_FS_STR = 1,
    } Cy_Usb_Dscr_Str_Index_t;

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_DESCRIPTORS_H_ */