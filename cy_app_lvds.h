/***************************************************************************/
/*
 * \file cy_app_lvds.h
 * \version 1.0
 *
 * Provided API declarations for the lvds interface implementation.
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

#ifndef _APP_LVDS_H_
#define _APP_LVDS_H_

#include "cy_lvds.h"
#if defined(__cplusplus)
extern "C"
{
#endif

#define LVDS_INTR_WD0_MASK_PORT0 (LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_DONE_Msk |          \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DETECTED_Msk |  \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk | \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_ENTRY_Msk |               \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_EXIT_Msk |                \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L3_ENTRY_Msk |               \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_THREAD0_ERR_Msk |                 \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_THREAD1_ERR_Msk |                 \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK0_INTERRUPT_Msk |         \
                                  LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK0_FF_OVER_FLOW_Msk |      \
                                  LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk)

#define LVDS_INTR_WD0_MASK_PORT1 (LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_DONE_Msk |          \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DETECTED_Msk |  \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk | \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_ENTRY_Msk |               \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_EXIT_Msk |                \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L3_ENTRY_Msk |               \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_THREAD2_ERR_Msk |                 \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_THREAD3_ERR_Msk |                 \
                                  LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK1_INTERRUPT_Msk |         \
                                  LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK1_FF_OVER_FLOW_Msk |      \
                                  LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF1_INTERRUPT_Msk)


#define LVDS_AFE_PHY_INTR_MASK (LVDSSS_LVDS_AFE_PHY_INTR_MONITOR_ADJUST_FAIL_Msk)

    typedef enum
    {
        TRAINING_NOT_COMPLETED = 0x00,
        P0_PHY_TRAINING_DONE = 0x01,
        P1_PHY_TRAINING_DONE = 0x02,
        WL_PHY_TRAINING_DONE = 0x03,
        P0_TRAINING_DONE = 0x04,
        P1_TRAINING_DONE = 0x08,
        WL_TRAINING_DONE = 0x0C,
    } cy_en_lvdsTrainingStatus_t;

    extern cy_stc_lvds_config_t cy_lvds_lvcmos_config;
    extern cy_en_lvdsTrainingStatus_t lvdsTrainingStatus;
    extern cy_stc_lvds_context_t lvdsContext;
    /*****************************************************************************
     * Function Name: Cy_App_LVDS_LVCMOS_Init
     *****************************************************************************
     * Summary
     *  Initialize the LVDS interface. Currently, only the SIP #0 is being initialized
     *  and configured to allow transfers into the HBW SRAM through DMA.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_App_LVDS_LVCMOS_Init(void);

    /*****************************************************************************
     * Function Name: Cy_LVDS_GetTrainingStatus
     ******************************************************************************
     * Summary:
     *  Returs training completion status flags
     *
     * Parameters:
     *  None
     *
     * Return:
     *  cy_en_lvdsTrainingStatus_t
     *****************************************************************************/
    cy_en_lvdsTrainingStatus_t Cy_LVDS_GetTrainingStatus(void);

    /*****************************************************************************
     * Function Name: Cy_LVDS_ISR
     ******************************************************************************
     * Summary:
     *  Handler for LVDS Interrupts.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_LVDS_ISR(void);

    /*****************************************************************************
     * Function Name: Cy_LVDS_LPM_ISR
     ******************************************************************************
     * Summary:
     *  Handler for LVDS LPM Interrupts.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_LVDS_LPM_ISR(void);

#if (PORT1_EN || WL_EN)
    /*****************************************************************************
     * Function Name: Cy_LVDS_Port1Dma_ISR
     ******************************************************************************
     * Summary:
     *  Handler for LVDS PORT1 DMA Interrupts.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_LVDS_Port1Dma_ISR(void);
#endif /* (PORT1_EN|| WL_EN) */

#if ((!PORT1_EN) || WL_EN)
    /*****************************************************************************
     * Function Name: Cy_LVDS_Port0Dma_ISR
     ******************************************************************************
     * Summary:
     *  Handler for LVDS PORT0 DMA Interrupts.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_LVDS_Port0Dma_ISR(void);
#endif /* ((!PORT1_EN) || WL_EN) */

#if INMD_EN  && (!LVCMOS_EN)
/*****************************************************************************
 * Function Name: Cy_LVDS_UpdateMetadata
 *****************************************************************************
 * Summary
 *  This function updates the metadata
 *
 * Parameters:
 * \param height
 *  Image height
 *
 *  \param width
 *  Image width
 *
 *  \param imageSize
 *  image size (in bytes)
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_UpdateMetadata(uint32_t height, uint32_t width, uint32_t imageSize);
#endif /* INMD_EN  && (!LVCMOS_EN)*/

#if defined(__cplusplus)
}
#endif

#endif /* _APP_LVDS_H_ */
