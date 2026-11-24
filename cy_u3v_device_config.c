/***************************************************************************
*
* \file cy_u3v_device_config.c
* \version 1.0
*
* Initializes macros and structure for U3V device configuration
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
*******************************************************************************/


#include "cy_u3v_device_config.h"

const char MANUFACTURER_NAME[] = "INFINEON";
const char FAMILY_NAME[] = "EZ-USB";
const char DEVICE_VER[] = "REV A";
const char SERIAL_NUM[] = "SN:04B400001";
#if (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2X2)
const char MODEL_NAME[] = "FX20";
#elif (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2)
const char MODEL_NAME[] = "FX10";
#elif (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1X2)
const char MODEL_NAME[] = "FX5N";
#elif (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1)
const char MODEL_NAME[] = "FX5";
#else
const char MODEL_NAME[] = "FX HS";
#endif /* USB_CONN_TYPE */

/* U3V device context */
cy_stc_u3v_dev_context_t u3vDevCtxt;

cy_stc_u3v_brm_cfg_t bootstrapConfig = {
    .manufacturerNameStr = MANUFACTURER_NAME,
    .modelNameStr = MODEL_NAME,
    .familyNameStr = FAMILY_NAME,
    .deviceVerStr = DEVICE_VER,
    .manufacturerInfoStr = MANUFACTURER_NAME,
    .serialNumberStr = SERIAL_NUM,
    .leaderSize = U3V_LEADER_SIZE,
    .trailerSize = U3V_TRAILER_SIZE,
    .payloadType = PAYLOAD_IMAGE,                  /* IMG*/
    .maxResponseTime = U3V_ABRM_MAX_RESPONSE_TIME, /* 300 ms*/
    .streamChannels = NUM_OF_STREAM_EP_SUPPORTED,
    .usbCurrentSpeed = USB_CURRENT_SPEED, /* Bits (7:0): (Gen2x2,Gen1x2, Gen2x1),  (Gen1x1, HS, FS, LS) */
    .oneXferSize = STREAM_DMA_BUFFER_SIZE,
    .pxlFormat = IMAGE_FORMAT,
    .maxWidth = U3V_HRES_MAX,  /* Maximum sensor width setting*/
    .maxHeight = U3V_VRES_MAX, /* Maximum sensor height setting*/
    .width = U3V_HRES,
    .height = U3V_VRES,
    .fps = U3V_FPS,
    .bitsPerPixel = BITS_PER_PIXEL,
    .xmlFileLen = XML_FILE_LEN,
    .xmlFileSHA1 = glU3vXMLSha1,
    .xmlFileReg = glU3vXML,
};

/* U3V register base address */
cy_stc_u3v_reg_base_t regBaseConfig = {
    .sbrmBaseAddr = 0x50000,
    .sirmBaseAddr = 0x62000,
    .acqModeCtrlBaseAddr = 0x30800,
    .imgCtrlBaseAddr = 0x30300,
    .transportCtrlBaseAddr = 0x31900,
    .devCtrlBaseAddr = 0x30000,
    .catInqBaseAddr = 0x10000,
    .chunkDataCtrlBaseAddr = 0x150000,
    .manifestTableBaseAddr = 0x60000,
    .xmlFileBaseAddr = 0x100000
};