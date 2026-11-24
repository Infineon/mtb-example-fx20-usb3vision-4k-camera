/***************************************************************************/
/*
 * \file cy_u3v_device_config.h
 * \version 1.0
 *
 * Provided macros for u3v device config.
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

#ifndef _CY_U3V_DEVICE_CONFIG_H_
#define _CY_U3V_DEVICE_CONFIG_H_

#include "COMPONENT_U3VHEADER/cy_u3v_interface.h"
#include "cy_u3v_xml.h"
#include "cy_usb_descriptors.h"
#if defined(__cplusplus)
extern "C"
{
#endif
#if FPGA_ADDS_HEADER
#define U3V_LEADER_SIZE (56)
#define U3V_TRAILER_SIZE (40)
#else
#define U3V_LEADER_SIZE (52)
#define U3V_TRAILER_SIZE (32)
#endif

#define U3V_HRES_MAX (3840 * 2)
#define U3V_VRES_MAX (2160 * 2)

#define U3V_HRES (3840) /* Image Width  */
#define U3V_VRES (2160) /* Image Height */

#define U3V_FPS (30)

#define STREAM_DMA_BUFFER_SIZE (63 * 1024)            /* Dev lvds is configured for 16 lane*/
#define FPGA_DMA_BUFFER_SIZE (STREAM_DMA_BUFFER_SIZE) /* FPGA is sending as 16 lane; 65k is max limit*/
#if RGB24
#define IMAGE_FORMAT (RGB8)
#define BITS_PER_PIXEL (24)
#else
#if RGB32
#define IMAGE_FORMAT (RGBa8)
#define BITS_PER_PIXEL (32)
#else
#define IMAGE_FORMAT (YUV422_8)
#define BITS_PER_PIXEL (16)
#endif /* RGB32 */
#endif /* RGB24 */

#define STREAM_DMA_BUFFER_COUNT (3)
#define U3V_FRAME_SIZE ((U3V_HRES * U3V_VRES * BITS_PER_PIXEL) / 8)
#define U3V_FINAL_TRANSFER_1_SIZE ((U3V_FRAME_SIZE) % (STREAM_DMA_BUFFER_SIZE)) /*Partial buffer size*/
#define U3V_NUM_FULL_BUFFERS ((U3V_FRAME_SIZE) / (STREAM_DMA_BUFFER_SIZE))

#if (STREAM_DMA_BUFFER_SIZE % 16 != 0)
#error "Buffer size should be multiple of 16"
#endif

#if (((U3V_HRES * U3V_VRES * BITS_PER_PIXEL) % 8) != 0)
#warning "Frame size is not divisible by 8"
#endif

#define U3V_ABRM_MAX_RESPONSE_TIME (300)
#define U3V_WAIT_FOR_STOP_TIMEOUT_MS (U3V_ABRM_MAX_RESPONSE_TIME - 10)

#define NUM_OF_STREAM_EP_SUPPORTED (1)

    extern cy_stc_u3v_dev_context_t u3vDevCtxt;
    extern cy_stc_u3v_brm_cfg_t bootstrapConfig;
    extern cy_stc_u3v_reg_base_t regBaseConfig;

#if defined(__cplusplus)
}
#endif

#endif /* _CY_U3V_DEVICE_CONFIG_H_ */
