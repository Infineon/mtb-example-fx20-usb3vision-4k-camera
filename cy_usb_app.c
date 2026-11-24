/***************************************************************************
* \file cy_usb_app.c
* \version 1.0
*
* Implements the USB data handling part of the USB3 Vision application.
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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usbss_cal_drv.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_u3v_device_config.h"
#include "cy_usb_app.h"
#include "cy_u3v_xml.h"
#include "cy_debug.h"
#include "cy_lvds.h"
#include "cy_app_qspi.h"
#include "cy_fw_version.h"
#include "cy_usb_descriptors.h"
#include "cy_app_fpga.h"

extern cy_stc_usb_app_ctxt_t appCtxt;
extern cy_stc_hbdma_buf_mgr_t HBW_BufMgr;
extern cy_stc_u3v_brm_cfg_t bootstrapConfig;

extern uint8_t Cy_USB_Dscr_DeviceHS[];

#ifdef PRINT_FRAME_INFO
/*Variables to debug print the frame information like Producer Count, Consumer Count, Partial Packet size (in Bytes), Frame size (in Bytes) */
uint32_t glProd = 0;
uint32_t glCons = 0;
uint32_t glProdCount = 0;
uint32_t glConsCount = 0;
uint32_t glFrameSize = 0;
uint32_t glBlockIDLeader = 0;
uint32_t glBlockIDTrailer = 0;
volatile bool glPrintFlag = false;
volatile uint32_t glPartialBufSize = 0;
#endif /* PRINT_FRAME_INFO */

static volatile bool glIsDeviceConfigured = false;
static volatile bool glIsStreamingActive = false;

static bool glIsLeader = true;   /* Leader Block Tracker */
static bool glIsTrailer = false; /* Trailer Block Tracker */
uint32_t glFrameSizeTransferred = 0;
volatile uint16_t glFps = 0;
volatile uint16_t glFpsPrint = 0;

static uint8_t *glU3vCmdBuffer = NULL;      /* U3V Command Buffer */
static uint8_t *glU3vResponseBuffer = NULL; /* U3V Response Buffer */
static uint64_t glU3VDSIBlockID = 0;        /* Frame ID Counter */
volatile glFlags_t glFlags;
glFrameParameters_t glFrameParams = {
    .bitsPerPixel = BITS_PER_PIXEL,
    .height = U3V_VRES,
    .width = U3V_HRES,
    .fps = U3V_FPS,
    .imageSize = (U3V_HRES * U3V_VRES * BITS_PER_PIXEL) / 8};

/* Global HBWSS DMA channel handle */
static cy_stc_hbdma_channel_t *glChHandleDSI;    /* Stream Channel pointer. */
static cy_stc_hbdma_channel_t *glChHandleDCIRsp; /* U3V Response Channel pointer. */
static cy_stc_hbdma_channel_t *glChHandleDCICmd; /* U3V Command Channel pointer. */

void
Cy_U3V_AppSendResponse (
        uint8_t* rspBuffer,
        uint16_t length,
        void *userCtxt,
        uint8_t u3vChNum);

static void Cy_U3V_AppFlushAndResetDma(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_hbdma_channel_t *dmaChannel,
    uint8_t endpoint,
    uint8_t dir,
    bool flag);
#if (!USE_WINUSB)
static uint32_t __attribute__((section(".descSection"), used)) __attribute__((aligned(32))) Ep0TestBuffer[32U];
__attribute__((section(".descSection"), used)) uint32_t SetSelDataBuffer[8] __attribute__((aligned(32)));
#endif /* (!USE_WINUSB) */

/*****************************************************************************
 * Function Name: Cy_USB_AppSendMsgToTask
 ******************************************************************************
 * Summary:
 * Function that sends a message to the USB application task.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pMsg
 * message pointer to be sent to the task.
 *
 * \return
 * None
 *
 *******************************************************************************/
static void Cy_USB_AppSendMsgToTask(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usbd_app_msg_t *pMsg)
{
    BaseType_t xStatus;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if ((pAppCtxt != NULL) && (pMsg != NULL))
    {
        /* Send the message to the task. */
        xStatus = xQueueSendFromISR(pAppCtxt->appMsgQ, pMsg, &xHigherPriorityTaskWoken);
        (void)xStatus;
        (void)xHigherPriorityTaskWoken;
    }
}

/*****************************************************************************
 * Function Name: Cy_USB_App_KeepLinkActive
 ******************************************************************************
 * Summary:
 * Function to keep USB link active
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * Return:
 * void
 *****************************************************************************/
static void Cy_USB_App_KeepLinkActive(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
#if USB3_LPM_ENABLE
    if (pAppCtxt->isLpmEnabled)
    {
        /* Disable LPM for 100 ms after any DMA transfers have been completed. */
        APP_DBG_TRACE("Disabling LPM\r\n");
        Cy_USBD_LpmDisable(pAppCtxt->pUsbdCtxt);
        pAppCtxt->isLpmEnabled = false;
    }
    pAppCtxt->lpmEnableTime = Cy_USBD_GetTimerTick() + 100;
#endif /* USB3_LPM_ENABLE */
}

/*****************************************************************************
 * Function Name: Cy_U3V_WaitForLastFrameConsume
 ******************************************************************************
 * Summary:
 * Function to wait for last U3V frame to be consumed after STOP command received
 *
 * Parameters:
 * \param
 * None
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_U3V_WaitForLastFrameConsume(void)
{
    uint16_t timeout = 0;
    while (!glFlags.lastFrameConsumed)
    {
        timeout++;
        vTaskDelay(pdMS_TO_TICKS(1));
        if (timeout > U3V_WAIT_FOR_STOP_TIMEOUT_MS)
        {
            APP_DBG_TRACE1("Last frame timed out\r\n");
            glFlags.lastFrameConsumed = true;
            break;
        }
    }
    if (timeout < U3V_WAIT_FOR_STOP_TIMEOUT_MS)
    {
        APP_DBG_TRACE1("Last packet consumption completed in [%d] ms after STOP Notify\r\n", timeout);
    }
}

/*****************************************************************************
 * Function Name: CY_U3V_AppDevEvent
 ******************************************************************************
 * Summary:
 * Function to handle events from U3V library
 *
 * Parameters:
 * \param regAddr
 *  register address
 *
 * \param regVal
 *  register value
 *
 * \param event
 *  U3V event
 *
 * Return:
 * void
 *****************************************************************************/
static void CY_U3V_AppDevEvent (unsigned long regAddr, unsigned long regVal, cy_en_u3v_dev_event_t event, 
                                void *userCtxt, uint8_t u3vChNum)
{
    /* Create event using regAddr if not supported in lib*/
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t xStatus;
    cy_stc_usb_app_ctxt_t* pAppCtxt = (cy_stc_usb_app_ctxt_t*)userCtxt;
    static bool heightChanged = false;
    static bool widthChanged = false;
    static bool firstTimeTrgSelRcvd = true;
    static uint8_t triggerCondition = 0;
    static uint8_t chunkCondition = 0;
    cy_stc_fpga_reg_rd_wr_ret_t status = CY_SCB_I2C_SUCCESS;

    APP_DBG_TRACE1("U3V Events: regAddr: 0x%x, regVal: 0x%x, event: 0x%x\r\n", regAddr, regVal, event);
    switch (event)
    {
    case U3V_EVT_STREAM_ENABLE:
        /* Enable streaming */
        APP_DBG_INFO("Stream Enabled\r\n");
        break;

    case U3V_EVT_STREAM_DISABLE:
        /* Disable streaming */
#if (INMD_EN  && (!LVCMOS_EN)) || ((FPGA_ADDS_HEADER && (!DBG_HEADER_BY_FPGA)))
#else
        Cy_U3V_WaitForLastFrameConsume();
#endif /* (INMD_EN  && (!LVCMOS_EN)) || ((FPGA_ADDS_HEADER && (!DBG_HEADER_BY_FPGA))) */
        Cy_U3V_AppStreamAcqStop(pAppCtxt, pAppCtxt->pUsbdCtxt);
        APP_DBG_INFO("Stream Disabled\r\n");
        break;

    case U3V_EVT_IMG_FORMAT_HEIGHT:
        /* Set height */
        glFrameParams.height = regVal;
        heightChanged = true;
        if (widthChanged && heightChanged)
        {
            heightChanged = false;
            widthChanged = false;
            glFrameParams.imageSize = (glFrameParams.width * glFrameParams.height * glFrameParams.bitsPerPixel) / 8;
#if MIPI_INTERFACE_EN
            xMsg.type = CY_APP_DEV_RESOLUTION_CONFIG_EVENT;
            xStatus = xQueueSendFromISR(appCtxt.appMsgQ, &(xMsg), &(xHigherPriorityTaskWoken));
            ASSERT_NON_BLOCK(pdTRUE == xStatus, xStatus);
#else
            Cy_FPGA_UpdateFrameWidth(FPGASLAVE_ADDR, glFrameParams.width);
            Cy_FPGA_UpdateFrameHeight(FPGASLAVE_ADDR, glFrameParams.height);
#if INMD_EN   && (!LVCMOS_EN)          
            Cy_LVDS_UpdateMetadata(glFrameParams.height, glFrameParams.width, glFrameParams.imageSize);
#endif /* INMD_EN  && (!LVCMOS_EN)*/
#endif /*MIPI_INTERFACE_EN*/
        }
        break;
    case U3V_EVT_IMG_FORMAT_WIDTH:
        /* Set Width */
        glFrameParams.width = regVal;
        widthChanged = true;
        if (widthChanged && heightChanged)
        {
            heightChanged = false;
            widthChanged = false;
            glFrameParams.imageSize = (glFrameParams.width * glFrameParams.height * glFrameParams.bitsPerPixel) / 8;
#if MIPI_INTERFACE_EN
            xMsg.type = CY_APP_DEV_RESOLUTION_CONFIG_EVENT;
            xStatus = xQueueSendFromISR(appCtxt.appMsgQ, &(xMsg), &(xHigherPriorityTaskWoken));
            ASSERT_NON_BLOCK(pdTRUE == xStatus, xStatus);
#else
            Cy_FPGA_UpdateFrameWidth(FPGASLAVE_ADDR, glFrameParams.width);
            Cy_FPGA_UpdateFrameHeight(FPGASLAVE_ADDR, glFrameParams.height);
#if INMD_EN  && (!LVCMOS_EN)           
            Cy_LVDS_UpdateMetadata(glFrameParams.height, glFrameParams.width, glFrameParams.imageSize);
#endif /* INMD_EN  && (!LVCMOS_EN) */            
#endif /*MIPI_INTERFACE_EN*/
        }
        break;

    case U3V_EVT_IMG_FORMAT_PXL_FORMAT:
        /* Set Pixel format */

        break;
    case U3V_EVT_ACQUISITION_START:
        /* Acquition Started */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        Cy_U3V_AppStreamAcqStart(pAppCtxt, pAppCtxt->pUsbdCtxt);
        break;

    case U3V_EVT_ACQUISITION_STOP:
        /* Acquition Stopped */ /* Send fpga to stop notification*/
        Cy_FPGA_AppStreamAcqMode(FPGASLAVE_ADDR, STREAM_APP_NOTIFY_STOP);
        break;

    case U3V_EVT_ACQ_MODE_SINGLE:
        /* Single frame acquition */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_STREAM_MODE_ADDR, STREAM_MODE_SINGLE, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;

    case U3V_EVT_ACQ_MODE_MULTI:
        /* Multiple frame acquition */
        if (!glFrameParams.multiFrameCount)
        {
            APP_DBG_INFO("Multi-frame mode: setting default count %d\r\n", MULTIFRAME_DEFAULT_CNT);
            glFrameParams.multiFrameCount = (MULTIFRAME_DEFAULT_CNT << MULTIFRAME_SHIFT_VAL) + STREAM_MODE_MULTIFRAME;
        }
        else
        {
            APP_DBG_INFO("Multi-frame mode: setting previous count %d\r\n", glFrameParams.multiFrameCount);
        }
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_STREAM_MODE_ADDR, glFrameParams.multiFrameCount, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;

    case U3V_EVT_ACQ_MODE_CONTINUOUS:
        /* Continuous frame acquition */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_STREAM_MODE_ADDR, STREAM_MODE_CONTINUOUS, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_ACQUISITION_FRAME_COUNT:
        /* Multi frame acquition frame count */
        glFrameParams.multiFrameCount = (regVal << MULTIFRAME_SHIFT_VAL) + STREAM_MODE_MULTIFRAME;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_STREAM_MODE_ADDR, glFrameParams.multiFrameCount, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;

    case U3V_EVT_ACQUISITION_SET_FPS:
        /* Set FPS */
        glFrameParams.fps = (regVal & 0x00FFF) / 10;
#if MIPI_INTERFACE_EN
        xMsg.type = CY_APP_DEV_FPS_CONFIG_EVENT;
        xStatus = xQueueSendFromISR(appCtxt.appMsgQ, &(xMsg), &(xHigherPriorityTaskWoken));
        ASSERT_NON_BLOCK(pdTRUE == xStatus, xStatus);
#else
        Cy_FPGA_UpdateStreamingFps(FPGASLAVE_ADDR, glFrameParams.fps);
#endif /*MIPI_INTERFACE_EN*/
        break;
    case U3V_EVT_TRIGGER_MODE_TRG_ON:
        /* Trigger On */
        if (firstTimeTrgSelRcvd)
        {
            firstTimeTrgSelRcvd = false;
            triggerCondition = TRIGGER_MODE_EN;                           /* Trigger mode enable */
            triggerCondition |= (TRIGGER_TYPE_SELECT & TRIGGER_TYPE_ACQ); /* Type ACQ */
            triggerCondition |= TRIGGER_SW_EN;                            /* Default SW trig En */
            status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        else
        {
            triggerCondition |= TRIGGER_MODE_EN; /* set to enable trig mode */
            triggerCondition &= (~TRIGGER_SW); /* clear bit of SW trigger */
            status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        break;
    case U3V_EVT_TRIGGER_MODE_TRG_OFF:
        /* Trigger off */
        triggerCondition &= (~TRIGGER_MODE_EN); /* clear to disable trig mode */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, 0, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_MODE_SEL_ACQ:
        /* Select trigger mode to acquition start */
        triggerCondition &= (~TRIGGER_TYPE_SELECT); /* clear bit to select ACQ */
        triggerCondition &= (~TRIGGER_SW); /*clear bit of SW trigger */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_MODE_SEL_FRAME:
        /* Select trigger mode to Frame start */
        triggerCondition |= (TRIGGER_TYPE_SELECT); /* set bit to select Frame start */
        triggerCondition &= (~TRIGGER_SW); /* clear bit of SW trigger */
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_MODE_SEL_NA:
        /* Select trigger mode to Frame start */
        APP_DBG_INFO("No valid trigger selector\r\n");
        break;
    case U3V_EVT_TRIGGER_ACTV_FALL_EDGE:
        /* Config trigger mode to falling edge */
        APP_DBG_INFO("Trigger Activation set to Falling Edge\r\n");
        break;
    case U3V_EVT_TRIGGER_ACTV_FALL_RISE:
        /* Config trigger mode to rising edge */
        APP_DBG_INFO("Trigger Activation set to Rising Edge\r\n");
        break;
    case U3V_EVT_TRIGGER_SOFTWARE_RCVD:
        /* Software trigger received */
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        Cy_SysLib_DelayUs(40000); /* should be less than U3V_ABRM_MAX_RESPONSE_TIME */
        triggerCondition |= (TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        triggerCondition &= (~TRIGGER_SW);
        break;
    case U3V_EVT_TRIGGER_SRC_SOFTWARE:
        /* Select trigger source: Software */
        triggerCondition &= (TRIGGER_MASK);
        triggerCondition |= (TRIGGER_SW_EN);
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_SRC_HARDWARE_L0:
        /* Select trigger source: Hardware L0 */
        triggerCondition &= (TRIGGER_MASK);
        triggerCondition |= TRIGGER_HW_0;
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_SRC_HARDWARE_L1:
        /* Select trigger source: Hardware L1 */
        triggerCondition &= (TRIGGER_MASK);
        triggerCondition |= TRIGGER_HW_1;
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_SRC_HARDWARE_L2:
        /* Select trigger source: Hardware L2 */
        triggerCondition &= (TRIGGER_MASK);
        triggerCondition |= TRIGGER_HW_2;
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_TRIGGER_SRC_HARDWARE_L3:
        /* Select trigger source: Hardware L3 */
        triggerCondition &= (TRIGGER_MASK);
        triggerCondition |= TRIGGER_HW_3;
        triggerCondition &= (~TRIGGER_SW);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_TRIGGER_MODE_ADDR, triggerCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_MODE_ENABLE:
        /* Enable Chunk mode */
        chunkCondition = CHUNK_MODE_EN;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_MODE_DISABLE:
        /* Disable chunk mode */
        chunkCondition = CHUNK_MODE_DISABLE;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_SEL_FRAME_ID:
        /* Chunk mode selected: Frame ID */
        APP_DBG_INFO("Chunk: Frame ID Selected\r\n");
        break;
    case U3V_EVT_CHUNK_SEL_TIMESTAMP:
        /* Chunk mode selected: Timestamp */
        APP_DBG_INFO("Chunk: Timestamp Selected\r\n");
        break;
    case U3V_EVT_CHUNK_FRAME_ID_ENABLE:
        /* Enable Frame ID chunk mode */
        chunkCondition |= CHUNK_TYPE_FRAME_ID;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_FRAME_ID_DISABLE:
        /* Disable Frame ID chunk mode */
        chunkCondition &= (~CHUNK_TYPE_FRAME_ID);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_TIMESTAMP_ENABLE:
        /* Enable Timestamp chunk mode */
        chunkCondition |= CHUNK_TYPE_TIME_STAMP;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_TIMESTAMP_DISABLE:
        /* Disable Timestamp chunk mode */
        chunkCondition &= (~CHUNK_TYPE_TIME_STAMP);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_FRAMEID_TIMESTAMP_ENABLE:
        /* Enable Timestamp chunk mode */
        chunkCondition |= CHUNK_TYPE_FRAME_ID;
        chunkCondition |= CHUNK_TYPE_TIME_STAMP;
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;
    case U3V_EVT_CHUNK_FRAMEID_TIMESTAMP_DISABLE:
        /* Disable Timestamp chunk mode */
        chunkCondition &= (~CHUNK_TYPE_FRAME_ID);
        chunkCondition &= (~CHUNK_TYPE_TIME_STAMP);
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, DEV_U3V_CHUNK_MODE_ADDR, chunkCondition, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        break;

    default:

        break;
    }
}

/*****************************************************************************
 * Function Name: Cy_U3V_DebugLog
 ******************************************************************************
 * Summary:
 * Function to prints U3V library messages
 *
 * Parameters:
 * \param level
 *  debug level
 *
 * \param message
 *  pointer to message
 *
 * Return:
 * void
 *****************************************************************************/
static void Cy_U3V_DebugLog(unsigned char level, char *message, ...)
{
    Cy_Debug_AddToLog(level, message);
}

/*****************************************************************************
 * Function Name: Cy_App_InitializeU3vInterface
 ******************************************************************************
 * Summary:
 * Function to initialize U3V
 *
 * Parameters:
 * \param pAppCtxt
 * pointer to app context
 *
 * Return:
 * void
 *****************************************************************************/
static void Cy_App_InitializeU3vInterface(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_u3v_interface_config_t u3vInterface;
    cy_en_u3v_return_status_t ret;
    cy_en_usb_speed_t currentSpeed = pAppCtxt->devSpeed;
    if(currentSpeed == CY_USBD_USB_DEV_SS_GEN2X2){
        bootstrapConfig.usbCurrentSpeed = 0x40;
    }
    else if(currentSpeed == CY_USBD_USB_DEV_SS_GEN2){
        bootstrapConfig.usbCurrentSpeed = 0x10;
    }
    else if(currentSpeed == CY_USBD_USB_DEV_SS_GEN1X2){
        bootstrapConfig.usbCurrentSpeed = 0x20;
    }
    else if(currentSpeed == CY_USBD_USB_DEV_SS_GEN1){
        bootstrapConfig.usbCurrentSpeed = 0x08;
    }
    else{
        bootstrapConfig.usbCurrentSpeed = 0x04;
    }
    u3vInterface.u3vChannelNum = 0;
    u3vInterface.BRM_Config = &bootstrapConfig;
    u3vInterface.regBaseAddr = &regBaseConfig;
    u3vInterface.CY_U3V_DbgLog = Cy_U3V_DebugLog;
    u3vInterface.Cy_U3V_DevEvent = CY_U3V_AppDevEvent;         /* Update FPGA/ external device parameters */
    u3vInterface.CY_U3V_SendResponse = Cy_U3V_AppSendResponse; /* Cb to send DCI response data to Host */
    u3vInterface.userCtx = (void *)(&appCtxt);

    ret = Cy_U3V_InterfaceConfig(&u3vInterface, &u3vDevCtxt);
    if (ret)
    {
        APP_DBG_ERR("U3V Interface init failed, Register all func pointers, Return Status: 0x%x", ret);
    }
}

/*******************************************************************************
 * Function name: Cy_CheckStatus
 ****************************************************************************/
/**
 *
 * Description: Function that handles prints error log
 *
 * \param function
 * Pointer to function
 *
 * \param line
 * Line number where error is seen
 *
 * \param condition
 *  condition of failure
 *
 * \param value
 *  error code
 *
 * \param isBlocking
 *  blocking function
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking)
{
    if (!condition)
    {
        /* Application failed with the error code status */
        APP_DBG_ERR("Function: [%s] Line: [%d] Status: [0x%x]\r\n", function, line, value);
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/*******************************************************************************
 * Function name: Cy_CheckStatusHandleFailure
 ****************************************************************************/
/**
 *
 * Description: Function that handles prints error log
 *
 * \param function
 * Pointer to function
 *
 * \param line
 * LineNumber where error is seen
 *
 * \param condition
 * Line number where error is seen
 *
 * \param value
 *  error code
 *
 * \param isBlocking
 *  blocking function
 *
 * \param failureHandler
 *  failure handler function
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)(void))
{
    if (!condition)
    {
        /* Application failed with the error code status */
        APP_DBG_ERR("Function %s failed at line %d with status = 0x%x\r\n", function, line, value);

        if (failureHandler != NULL)
        {
            (*failureHandler)();
        }

        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/*******************************************************************************
 * Function name: Cy_USB_FailHandler
 ****************************************************************************/
/**
 *
 * Description: Error Handler
 *
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_FailHandler(void)
{
    APP_DBG_ERR("Fail Handler\r\n");
}

/*****************************************************************************
 * Function Name: Cy_USB_AppDisableEndpDma
 ******************************************************************************
 * Summary:
 * This function de-inits all active USB DMA channels as part of USB disconnect process.
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    uint8_t i;
    /* If any DMA channels have been created, disable and destroy them. */
    for (i = 0; i < CY_USB_MAX_ENDP_NUMBER; i++)
    {
        if (pAppCtxt->endpInDma[i].valid)
        {
            APP_DBG_TRACE("HBDMA destroy EP%d-In\r\n", i);
            Cy_HBDma_Channel_Disable(&(pAppCtxt->endpInDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpInDma[i].hbDmaChannel));
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_IN);
            Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_IN, false);
            pAppCtxt->endpInDma[i].valid = false;
        }

        if (pAppCtxt->endpOutDma[i].valid)
        {
            APP_DBG_TRACE("HBDMA destroy EP%d-Out\r\n", i);
            Cy_HBDma_Channel_Disable(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_OUT);
            Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, i, CY_USB_ENDP_DIR_OUT, false);
            pAppCtxt->endpOutDma[i].valid = false;
        }
    }
}

/*****************************************************************************
 * Function Name: Cy_USB_AppGpifEventCb
 ******************************************************************************
 * Summary:
 * GPIF error handler
 *
 * Parameters:
 * \param pApp
 * application layer context pointer
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_USB_AppGpifEventCb(void *pApp)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pApp;

    APP_DBG_TRACE("GPIFSM Interrupt\r\n");

    glFlags.blockFrame = false;
    Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDSI, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, true);
    if (pAppCtxt->isLvdsWltoUsbHs) 
    {
        APP_DBG_TRACE1("CLR FWRDY\r\n");
        pAppCtxt->fwDmaReadyStatus = false;
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }    
    if (glIsStreamingActive)
    {
        glIsStreamingActive = false;
    }
    Cy_USBD_SendACkSetupDataStatusStage(pAppCtxt->pUsbdCtxt);
    return;
}

void Cy_App_LVDS_REG_Print(void)
{
    APP_DBG_TRACE("\r\nStart LVDS Register Prints\r\n");
#if (WL_EN || (!PORT1_EN))
    if (LVDSSS_LVDS->LINK_TRAINING_STS[0] != 0x1FF)
    {
        APP_DBG_TRACE("LINK_TRAINING_STS[0]:%x\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[0]);
    }
    else
    {
        APP_DBG_TRACE("LINK_TRAINING_STS[0]:%x\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[0]);
    }
#endif
#if (WL_EN || (PORT1_EN))
    if (LVDSSS_LVDS->LINK_TRAINING_STS[1] != 0x1FF)
    {
        APP_DBG_TRACE("LINK_TRAINING_STS[1]:%x\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[1]);
    }
    else
    {
        APP_DBG_TRACE("LINK_TRAINING_STS[1]:%x\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[1]);
    }
#endif
    APP_DBG_TRACE("LVDS_INTR_WD0       :%x \r\n", LVDSSS_LVDS->LVDS_INTR_WD0);
    APP_DBG_TRACE("LVDS_ERROR          :%x \r\n", LVDSSS_LVDS->LVDS_ERROR);
    APP_DBG_TRACE("Stop LVDS Register Prints\r\n");
}

void Cy_App_TrainingStatus(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    bool fpgaConfigFlag = Cy_FPGA_IsConfigurationCompleted();
    if (fpgaConfigFlag)
    {
#if TRAINING_SEQ_REQ
#if (WL_EN || (!PORT1_EN))
#if (!LVCMOS_EN)
        if (!(Cy_LVDS_GetTrainingStatus() & P0_PHY_TRAINING_DONE))
        {
            APP_DBG_ERR("Port0 Phy Training Failed, Reset the Hardware Kit\r\n");
        }
#endif /*(!LVCMOS_EN)*/
        if (!(Cy_LVDS_GetTrainingStatus() & P0_TRAINING_DONE))
        {
            APP_DBG_ERR("Port0 Link Training Failed, Reset the Hardware Kit.\r\n");
            APP_DBG_ERR("Is build config and FPGA bitfile correct?\r\n");
            Cy_App_PrintBuildConfigInErrState(pAppCtxt->sensorType);
        }
#endif
#if (WL_EN || (PORT1_EN))
#if (!LVCMOS_EN)
        if (!(Cy_LVDS_GetTrainingStatus() & P1_PHY_TRAINING_DONE))
        {
            APP_DBG_ERR("Port1 Phy Training Failed, Reset the Hardware Kit\r\n");
        }
#endif /*(!LVCMOS_EN)*/
        if (!(Cy_LVDS_GetTrainingStatus() & P1_TRAINING_DONE))
        {
            APP_DBG_ERR("Port1 Link Training Failed, Reset the Hardware Kit.\r\n");
            APP_DBG_ERR("Is build config and FPGA bitfile correct?\r\n");
            Cy_App_PrintBuildConfigInErrState(pAppCtxt->sensorType);
        }
#endif
#endif /*TRAINING_SEQ_REQ*/
    }
    else
    {
        APP_DBG_ERR("FPGA is not configured !!\r\n");
    }
}

/***********************************************************************************************
 * Function Name: Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
 **************************************************************************************************
 * Summary:
 * This Function will be called when timer expires.This function print event log.
 *
 * Parameters:
 * \param xTimer
 * timer handle
 *
 * Return:
 * void
 ************************************************************************************************/
void Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    /* retrieve pAppCtxt */
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED)
    {
        /*
         * Print out the contents of the USB event log buffer which has
         * logged event related to enumeration and line training.
         */
        Cy_USB_AppPrintUsbEventLog(pAppCtxt, pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    }
} /* end of function() */

/*****************************************************************************
 * Function Name: Cy_USB_UserTimerCallback
 ******************************************************************************
 * Summary:
 * Timer used to handle prints in every 1 seconds.
 *
 * Parameters:
 * \param xTimer
 * timer handle
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_USB_UserTimerCallback(TimerHandle_t xTimer)
{
    cy_stc_usbd_app_msg_t xMsg;
    
    if (glIsStreamingActive == true)
    {
#if ((!DBG_HEADER_BY_FPGA) && FPGA_ADDS_HEADER) || (INMD_EN  && (!LVCMOS_EN))
        /* Read the current socket count and then clear it. Make sure use same sck which  is configured in hbdma*/
        uint32_t sckCount = 0;
#if PORT1_EN
        uint32_t adp1Sck0Count = 0;
        adp1Sck0Count = LVDSSS_LVDS->ADAPTER_DMA[1].SCK[0].SCK_COUNT;
        LVDSSS_LVDS->ADAPTER_DMA[1].SCK[0].SCK_COUNT = 0;
#if INTERLEAVE_EN
        uint32_t adp1Sck1Count = 0;
        adp1Sck1Count = LVDSSS_LVDS->ADAPTER_DMA[1].SCK[1].SCK_COUNT;
        LVDSSS_LVDS->ADAPTER_DMA[1].SCK[1].SCK_COUNT = 0;
        sckCount = adp1Sck0Count + adp1Sck1Count;
#else
        sckCount = adp1Sck0Count;
#endif /* INTERLEAVE_EN */
#else /* !PORT1_EN || WL_EN*/
        uint32_t adp0Sck0Count = 0;
        adp0Sck0Count = LVDSSS_LVDS->ADAPTER_DMA[0].SCK[0].SCK_COUNT;
        LVDSSS_LVDS->ADAPTER_DMA[0].SCK[0].SCK_COUNT = 0;
#if INTERLEAVE_EN
        uint32_t adp0Sck1Count = 0;
        adp0Sck1Count = LVDSSS_LVDS->ADAPTER_DMA[0].SCK[1].SCK_COUNT;
        LVDSSS_LVDS->ADAPTER_DMA[0].SCK[1].SCK_COUNT = 0;
        sckCount = adp0Sck0Count + adp0Sck1Count;
#else
        sckCount = adp0Sck0Count;
#endif /* INTERLEAVE_EN */
#endif /* PORT1_EN */

        /* Socket count represents number of bytes transferred.
         * Divide by number of buffer in frame size to calculate the number of frames
         * transferred.
         */
        glFps = (sckCount / (glFrameParams.imageSize/STREAM_DMA_BUFFER_SIZE));
#endif /* ((!DBG_HEADER_BY_FPGA) && FPGA_ADDS_HEADER) || (INMD_EN  && (!LVCMOS_EN)) */

        glFpsPrint = glFps;
        xMsg.type = CY_USB_U3V_FPS_POLL_EVENT;
        Cy_USB_AppSendMsgToTask(&appCtxt, &xMsg);
        glFps = 0;
        xMsg.type = CY_USB_U3V_FPGA_STATUS_POLL_EVENT;;
        Cy_USB_AppSendMsgToTask(&appCtxt, &xMsg);
    }

} /* end of function  */

/*****************************************************************************
 * Function Name: Cy_U3V_AppFlushAndResetDma
 ******************************************************************************
 * Summary:
 * Function to flush & reset DMA channel
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param dmaChannel
 * pointert to DMA channle handle
 *
 * \param endpoint
 * endpoint number
 *
 * \param dir
 * endpoint direction
 *
 * \param flag
 *
 * Return:
 * void
 *****************************************************************************/
static void Cy_U3V_AppFlushAndResetDma(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_hbdma_channel_t *dmaChannel,
    uint8_t endpoint,
    uint8_t dir,
    bool flag)
{
    /* Enable clock control across EP reset operation to ensure that the EP is properly flushed. */
    Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, true);
    Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpoint, dir, true);
    Cy_SysLib_DelayUs(50);
    Cy_HBDma_Channel_Reset(dmaChannel);
    Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, endpoint, dir);
    Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, endpoint, dir, flag);
    Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpoint, dir, false);
    Cy_SysLib_Delay(1);
    Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, false);
    APP_DBG_TRACE1("EP [0x%x]: DMA Channel (0x%x) flushed & reset\r\n", dir ? 0x80 | endpoint : endpoint, *dmaChannel);
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppDSIHaltEventHandler
 ******************************************************************************
 * Summary:
 * Data streaming Interface (DSI) Halt Event handler
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param setClear
 * Set endpoint HALT if true elase clear endpoint HALT condition
 *
 * Return:
 * void
 *****************************************************************************/
bool Cy_U3V_AppDSIHaltEventHandler(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
    bool setClear)
{
    bool isStall = false;

    if (setClear)
    {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, true);
    }
    else
    {
        Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDSI, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, false);
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, false);
        if (pAppCtxt->isLvdsWltoUsbHs) 
        {
            DBG_APP_TRACE("CLR FWRDY\r\n");
            pAppCtxt->fwDmaReadyStatus = false;
            Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        }
    }

    isStall = Cy_USBD_EndpIsStallSet(pUsbdCtxt, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN);
    APP_DBG_TRACE1("Stream EP is %s\r\n", isStall ? "Stall" : "Not Stall");
    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
    return isStall;
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppSendResponse
 ******************************************************************************
 * Summary:
 * Function to send response on DCI channel
 *
 * Parameters:
 * \param rspBuffer
 *  pointer to buffer
 *
 * \param length
 *  size of response
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_U3V_AppSendResponse(
    uint8_t *rspBuffer,
    uint16_t length,
        void *userCtxt,
        uint8_t u3vChNum)
{

    cy_stc_usb_app_ctxt_t* pAppCtxt = (cy_stc_usb_app_ctxt_t*)userCtxt;
    cy_en_hbdma_mgr_status_t  hbdma_stat = CY_HBDMA_MGR_SUCCESS;
    
    if(pAppCtxt == NULL)
    {
        DBG_APP_ERR("Cy_U3V_AppSendResponse: NULL user context \n\r");
        return;
    }
    
    hbdma_stat = Cy_HBDma_Channel_SendData(glChHandleDCIRsp, 0, rspBuffer, length);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS)
    {
        APP_DBG_ERR("DCIRsp SendData HBDMA Err:%x\r\n", hbdma_stat);
        Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDCIRsp, CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN, true);
    }
    return;
}

/*****************************************************************************
 * Function Name: Cy_USB_VbusDebounceTimerCallback
 ******************************************************************************
 * Summary:
 * Timer used to do debounce on VBus changed interrupt notification.
 *
 * \param xTimer
 * Timer Handle
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_VbusDebounceTimerCallback(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    cy_stc_usbd_app_msg_t xMsg;

    if (pAppCtxt->vbusChangeIntr)
    {
        /* Notify the VCOM task that VBus debounce is complete. */
        xMsg.type = CY_USB_U3V_VBUS_CHANGE_DEBOUNCED;
        Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);
    }
} /* end of function  */

/*****************************************************************************
 * Function Name: Cy_U3V_AppPendingAckTimerCb
 ******************************************************************************
 * Summary:
 * This Function will be called when timer expires. This function Send Pending Ack to Host.
 *
 * Parameters:
 * \param xTimer
 * Timer handle
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_U3V_AppPendingAckTimerCb(TimerHandle_t xTimer)
{
    uint16_t len = 0;
    uint8_t isBusy = 0;

    if(Cy_U3V_SendPendingAckResponse(&u3vDevCtxt,glU3vResponseBuffer, &len))
    {
        DBG_APP_ERR("Cy_U3V_SendPendingAckResponse failed with status\r\n");
    }
    APP_DBG_TRACE1("CMD Response Timeout: sending pending Ack len[%d]\r\n", len);
    if(Cy_U3V_IsDCIHandlerBusy(&u3vDevCtxt,&isBusy) == U3V_RET_STATUS_SUCCESS)
    {
        /* Restart timer if Command not processed */
        if (isBusy){
            xTimerReset(xTimer,0);
        }
    }
}


/*****************************************************************************
 * Function Name: Cy_U3V_AppStreamAcqStop
 ******************************************************************************
 * Summary:
 * This function stops the DSI stream channel
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_U3V_AppStreamAcqStop(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt)
{
    if (glIsStreamingActive)
    {
        Cy_FPGA_AppStreamAcqMode(FPGASLAVE_ADDR, STREAM_APP_DISABLE);
        if (pAppCtxt->isLvdsWltoUsbHs) 
        {
            APP_DBG_TRACE1("CLR FWRDY\r\n");
            pAppCtxt->fwDmaReadyStatus = false;
            Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        }
#if MIPI_INTERFACE_EN
        if(pAppCtxt->sensorType == SRC_SENSOR_IMX){
            Cy_ImgSensor_EnterStandbyModeIMX();
            APP_DBG_TRACE1("SonyIMX715 Standby Mode Enabled\r\n");
        }
        else if(pAppCtxt->sensorType == SRC_SENSOR_THINE)
        {
            Cy_ImgSensor_StopStreamingTHine();
        }
#endif /* MIPI_INTERFACE_EN */        
        /* Update the flag so that the application thread is notified of this. */
        glIsStreamingActive = false;

        /*Clear block IDs - restart from 0*/

        /* Next frame should start with Leader */
        glIsLeader = true;
        glIsTrailer = false;
        Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDSI, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, true);
        APP_DBG_INFO("STOP: %d x %d, %d fps [Total Frames: %d]\r\n", glFrameParams.width, glFrameParams.height, glFrameParams.fps, (uint32_t)glU3VDSIBlockID);
        glU3VDSIBlockID = 0;
    }
    else
    {
        APP_DBG_TRACE1("DSIStop already done\r\n");
    }
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppStreamAcqStart
 ******************************************************************************
 * Summary:
 * This function starts the DSI channel for streaming
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_U3V_AppStreamAcqStart(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt)
{
    cy_en_hbdma_mgr_status_t mgrStatus = CY_HBDMA_MGR_SUCCESS;
    Cy_USB_App_KeepLinkActive(pAppCtxt);
    if (!glIsStreamingActive)
    {
#ifdef PRINT_FRAME_INFO
        /*Variables to debug print the frame information like Producer Count, Consumer Count, Partial Packet size (in Bytes), Frame size (in Bytes) and FPS*/
        glProd = 0;
        glCons = 0;
        glPrintFlag = 0;
        glPartialBufSize = 0;
#endif /* PRINT_FRAME_INFO */
        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
        {
            /* Allow data from multiple DMA buffers to be combined into one burst. */
            Cy_USBD_SetEpBurstMode(pAppCtxt->pUsbdCtxt, CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, true);
        }

        /* Enable the DMA channel for data transfer. */
        mgrStatus = Cy_HBDma_Channel_Enable(glChHandleDSI, 0);
        if (mgrStatus != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("DSIStart:ChEnable Err = 0x%x\n\r", mgrStatus);
            return;
        }

#if INMD_EN  && (!LVCMOS_EN)
        /* Make sure the DMA callback does not drop data any more and allow the tester to re-start
         * the video stream.
         */
        glFlags.blockFrame = false;
        Cy_LVDS_GpifClearFwTrig(LVDSSS_LVDS, PORT1_EN);
        APP_DBG_TRACE("INMD_EN: Cy_LVDS_GpifClearFwTrig\r\n");
#endif /* INMD_EN  && (!LVCMOS_EN) */
#if MIPI_INTERFACE_EN
        if(pAppCtxt->sensorType == SRC_SENSOR_IMX)
        {
            Cy_ImgSensor_StartStreamingIMX();
            APP_DBG_TRACE("SonyIMX715 Standby Mode Exit, Enabled Streaming\r\n");
        }
        else if(pAppCtxt->sensorType == SRC_SENSOR_THINE)
        {
            Cy_ImgSensor_StartStreamingTHine();
        }
#endif /* MIPI_INTERFACE_EN */        
        glU3VDSIBlockID = 0;
        APP_DBG_INFO("START: %d x %d, %d fps\r\n", glFrameParams.width, glFrameParams.height, glFrameParams.fps);
        
        glFps = 0;
        glFpsPrint = 0;
        glFrameSizeTransferred = 0;
        glIsStreamingActive = true;
        
        /* FPGA I2C register update not required for current implementation */
        Cy_FPGA_AppStreamAcqMode(FPGASLAVE_ADDR, STREAM_APP_EN);
        if (pAppCtxt->isLvdsWltoUsbHs) 
        {
            APP_DBG_TRACE1("SET FWRDY\r\n");
            pAppCtxt->fwDmaReadyStatus = true;
            Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        }
    }
    else
    {
        APP_DBG_TRACE("DSI App already started..\r\n");
    }
    xTimerReset(pAppCtxt->userTimer, 0); /* Reset user timer which calculates FPS and FPGA status */
    return;
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppDCICmdHaltEventHandler
 ******************************************************************************
 * Summary:
 * This function handles DCI Cmd endpoint Halt Evt
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param setClear
 * Set endpoint HALT if true else clear endpoint HALT condition
 *
 * Return:
 * void
 ****************************************************************************/
void Cy_U3V_AppDCICmdHaltEventHandler(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
    bool setClear)
{
    bool isStall = false;
    if (setClear)
    { /* SET FEATURE */
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, true);
    }
    else
    { /* CLEAR FEATURE */
        Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDCICmd, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, true);
        Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, false);
    }
    isStall = Cy_USBD_EndpIsStallSet(pUsbdCtxt, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT);
    APP_DBG_TRACE1("Cy_U3V_AppDCICmdHaltEventHandler: isStall : %d \b\r", isStall);
    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
    return;
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppDCIRspHaltEventHandler
 ******************************************************************************
 * Summary:
 * This function handles DCI response endpoint event
 *
 * Parameters:
 * \param pAppCtxt
 * application layer context pointer
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param setClear
 * Set endpoint HALT if true elase clear endpoint HALT condition
 *
 * Return:
 * void
 ****************************************************************************/
void Cy_U3V_AppDCIRspHaltEventHandler(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
    bool setClear)
{
    bool isStall = false;
    if (setClear)
    { /* SET FEATURE */
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN, true);
    }
    else
    { /* CLEAR FEATURE */
        xTimerStop(pAppCtxt->dciTimerHandle, 0);
        Cy_U3V_AppFlushAndResetDma(pAppCtxt, glChHandleDCIRsp, CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN, true);
        Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN, false);
    }
    isStall = Cy_USBD_EndpIsStallSet(pUsbdCtxt, CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN);
    APP_DBG_TRACE1("Cy_U3V_AppDCICmdHaltEventHandler: isStall : %d \b\r", isStall);
    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);

    return;
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppCtrlDmaCb
 ******************************************************************************
 * Summary:
 * This function is HBDMA callback funxtion for DCI channels
 *
 * Parameters:
 * \param handle
 * HBDMA channel handle
 *
 * \param cy_en_hbdma_cb_type_t
 * HBDMA channel type
 *
 * \param pbufStat
 * fHBDMA buffer status
 *
 * \param userCtx
 * user context
 *
 * Return:
 * void
 ****************************************************************************/
void Cy_U3V_AppCtrlDmaCb(
    cy_stc_hbdma_channel_t *handle,
    cy_en_hbdma_cb_type_t type,
    cy_stc_hbdma_buff_status_t *pbufStat,
    void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;
    cy_stc_usbd_app_msg_t xMsg;

    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        xMsg.type = CY_USB_U3V_CMD_EVT_FLAG;
    }
    else if (type == CY_HBDMA_CB_XFER_CPLT)
    {
        xMsg.type = CY_USB_U3V_RESP_SENT_EVT_FLAG;
    }
    else
    {
        DBG_APP_ERR("DCI CB type:%x:\r\n", type);
        return;
    }
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    Cy_USB_App_KeepLinkActive(pAppCtxt);
    return;
} /* end of function  */

/*****************************************************************************
 * Function Name: Cy_U3V_AppSendBuffToUSB
 ******************************************************************************
 * Summary:
 *  Function that commits/send image buffers to USB
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pChHandle
 * Pointer to DMA channel structure.
 *
 * \param buffStat
 * Structure which has buffer information.
 *
 * \param isDiscardBuff
 * Flag which decides buffer to commit or discard.
 *
 * \return
 * None
 *
 *******************************************************************************/
static cy_en_hbdma_mgr_status_t
Cy_U3V_AppSendBuffToUSB(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_hbdma_channel_t *pChHandle,
    cy_stc_hbdma_buff_status_t buffStat,
    bool isCommitBuff)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;
    if (isCommitBuff)
    {
        status = Cy_HBDma_Channel_CommitBuffer(pChHandle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            APP_DBG_ERR("Cy_HBDma_Channel_CommitBuffer Error: %x\r\n", status);
            return status;
        }
    }
    else
    {
        status = Cy_HBDma_Channel_DiscardBuffer(pChHandle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            APP_DBG_ERR("Cy_HBDma_Channel_DiscardBuffer Error: %x\r\n", status);
            return status;
        }
    }
    return status;
}

/*****************************************************************************
 * Function Name: Cy_U3V_AppHandleProduceEvent
 ******************************************************************************
 * Summary:
 *  Function that handles a produce event indicating receipt of data through
 *  the LVDS ingress socket.
 *  Cases: 
 *    - INMD enabled: Only block frame handled when stream stopped, data path is auto dma
 *    - FPGA_ADDS_HEADER enabled and DBG_HEADER_BY_FPGA enabled: header are added by fpga, just commit the data
 *    - FX is adding header: 
 *         header are added by device, add leader for every new frame, fpga sends ZLP
 *         add trailer after a partial buffer received, fpga sends ZLP
 *         commit all full bufferes recived
 *         catch error if payload size differs, expected ZLP not received to add leader/trailer
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pChHandle
 * Pointer to DMA channel structure.
 *
 * \return
 * None
 *
 *******************************************************************************/
static void
Cy_U3V_AppHandleProduceEvent(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_hbdma_channel_t *pChHandle)
{
    cy_en_hbdma_mgr_status_t status;
    cy_stc_hbdma_buff_status_t buffStat;
#if (!INMD_EN)
    uint8_t isFrameComplete = 0;
#endif /* (!INMD_EN) */
    cy_stc_usbd_app_msg_t xMsg;
    /* Wait for a free buffer. */
    status = Cy_HBDma_Channel_GetBuffer(pChHandle, &buffStat);
    if (status != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("HB-DMA GetBuffer Error\r\n");
        return;
    }
#ifdef PRINT_FRAME_INFO
    glProd++;
#endif /* PRINT_FRAME_INFO */
#if PROD_EVNT_DBG
    static uint16_t nHbdmaBuffRcvd = 0;
    if (glIsLeader)
    {
        nHbdmaBuffRcvd = 0;
    }
    if (buffStat.count != STREAM_DMA_BUFFER_SIZE)
    {
        APP_DBG_MISC("n_HBDMABytes: %d, [nHbdmaBuffRcvd: %d]\r\n", buffStat.count, nHbdmaBuffRcvd);
    }
    nHbdmaBuffRcvd++;
#endif /*PROD_EVNT_DBG*/

#if INMD_EN  && (!LVCMOS_EN)
    /* Data received from SIP is already correct. Forward it to the USB host as-is. */
    if (glFlags.blockFrame)
    {
        Cy_U3V_AppSendBuffToUSB(pAppCtxt, pChHandle, buffStat, false);
        xMsg.type = CY_USB_U3V_DMA_BLOCKED_FLAG;
        Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        APP_DBG_TRACE("Blocked Frame, n_HBDMABytes: %d.\r\n", buffStat.count);
        return;
    }
    else
    {
        Cy_U3V_AppSendBuffToUSB(pAppCtxt, pChHandle, buffStat, COMMIT_HBDMA_BUFFER);
    }
#else
    if (glFlags.blockFrame)
    {
        /* App stop already received from HOST APP. Ignore leader of next frame from LVDS. */
        Cy_U3V_AppSendBuffToUSB(pAppCtxt, pChHandle, buffStat, false);
        xMsg.type = CY_USB_U3V_DMA_BLOCKED_FLAG;
        Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        APP_DBG_TRACE("Blocked Frame, n_HBDMABytes: %d.\r\n", buffStat.count);
        return;
    }
#if FPGA_ADDS_HEADER
    else if (buffStat.count == (U3V_LEADER_SIZE))
    { /* Handle <<<<<<<<  LEADER   >>>>>>>>>>>*/
        glFlags.lastFrameConsumed = false;
        glFlags.lastFrameProduced = false;
        glIsLeader = false; /* leader data xfer from Device is in progress */
#ifdef PRINT_FRAME_INFO
        glBlockIDLeader = glU3VDSIBlockID; /*Copy of current block ID before setting the event*/
#endif /* PRINT_FRAME_INFO */
        xMsg.type = CY_USB_U3V_LEADER_RXD_FLAG;
        Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    }
    else if (buffStat.count == (U3V_TRAILER_SIZE))
    { /* Handle <<<<<<<<  TRAILER   >>>>>>>>>>>*/
#ifdef PRINT_FRAME_INFO
            glProdCount = glProd;
            glProd = 0;
#endif /* PRINT_FRAME_INFO*/        
        Cy_U3V_IsFullFrameCompleted(&u3vDevCtxt, glFrameSizeTransferred, &isFrameComplete);
        if(!isFrameComplete){
            xMsg.type = CY_USB_U3V_FRAME_MISSING_FLAG;
            xMsg.data[0] = glFrameSizeTransferred;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        }
        glFrameSizeTransferred = 0;
        glIsLeader = true;   /* expecting next data from Device will be a leader */
        glIsTrailer = false; /* trailer data xfer from Device is in progress */
        glFps++;
        glFlags.lastFrameProduced = true;
#ifdef PRINT_FRAME_INFO
        glBlockIDTrailer = glU3VDSIBlockID; /*Copy of current block ID before setting the event*/
#endif /* PRINT_FRAME_INFO */
        glU3VDSIBlockID++;
        xMsg.type = CY_USB_U3V_TRAILER_RXD_FLAG;
        Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    }
#else  /*  ! FPGA_ADDS_HEADER */
    else if (buffStat.count == 0x00)
    {
        if (glIsLeader)
        { /* Handle <<<<<<<<  LEADER   >>>>>>>>>>>*/
            glFlags.lastFrameConsumed = false;
            glFlags.lastFrameProduced = false;
            Cy_U3V_GetUpdatedLeaderData(&u3vDevCtxt,0, buffStat.pBuffer, (uint16_t *)&(buffStat.count));   
            glIsLeader = false; /* leader data xfer from Device is in progress */
#ifdef PRINT_FRAME_INFO
            glBlockIDLeader = glU3VDSIBlockID; /*Copy of current block ID before setting the event*/
#endif /* PRINT_FRAME_INFO */
            xMsg.type = CY_USB_U3V_LEADER_RXD_FLAG;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        }
        else if (glIsTrailer)
        { /* Handle <<<<<<<<  TRAILER   >>>>>>>>>>>*/
#ifdef PRINT_FRAME_INFO
            glProdCount = glProd;
            glProd = 0;
#endif /* PRINT_FRAME_INFO*/            
            glFps++;
            Cy_U3V_GetUpdatedTrailerData(&u3vDevCtxt, buffStat.pBuffer,(uint16_t *)&(buffStat.count));
            Cy_U3V_IsFullFrameCompleted(&u3vDevCtxt, glFrameSizeTransferred, &isFrameComplete);
            if(!isFrameComplete){
                xMsg.type = CY_USB_U3V_FRAME_MISSING_FLAG;
                xMsg.data[0] = glFrameSizeTransferred;
                Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
            }
            glFrameSizeTransferred = 0;
            glIsLeader = true;   /* expecting next data from Device will be a leader */
            glIsTrailer = false; /* trailer data xfer from Device is in progress */
            glFlags.lastFrameProduced = true;
#ifdef PRINT_FRAME_INFO
            glBlockIDTrailer = glBlockIDLeader; /*Copy of current block ID before setting the event*/
#endif /* PRINT_FRAME_INFO */
            Cy_U3V_GetBlockID(&u3vDevCtxt, &glU3VDSIBlockID); /*Copy of current block ID before setting the event*/
            xMsg.type = CY_USB_U3V_TRAILER_RXD_FLAG;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        }
        else
        {
            xMsg.type = CY_USB_U3V_ZLP_DISCARDED;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
            Cy_HBDma_Channel_DiscardBuffer(pChHandle, &buffStat);
            if (pAppCtxt->isLvdsWltoUsbHs) 
            {
                DBG_APP_TRACE("SET FWRDY\r\n");
                pAppCtxt->fwDmaReadyStatus = true;
                Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
            }
            return;
        }
    }
#endif /* FPGA_ADDS_HEADER */
    else
    {
        glFrameSizeTransferred += (buffStat.count);
        if (glIsLeader)
        {
            xMsg.type = CY_USB_U3V_NO_LEADER_RECVD;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        }
        else if (glIsTrailer)
        {
            xMsg.type = CY_USB_U3V_NO_TRAILER_RECVD;
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
        }
        /*Partial Buffer handling ; After partial buffer, send trailer as full frame completed*/
        if (buffStat.count != STREAM_DMA_BUFFER_SIZE)
        {
            glIsTrailer = true; /*Assuming full frame size is rcvd, next is trailer*/
#ifdef PRINT_FRAME_INFO
            glFrameSize = glFrameSizeTransferred;
            glPartialBufSize = (buffStat.count);
#endif /* PRINT_FRAME_INFO*/
        }
    }
    Cy_U3V_AppSendBuffToUSB(pAppCtxt, pChHandle, buffStat, COMMIT_HBDMA_BUFFER);
#endif /* INMD_EN  && (!LVCMOS_EN)*/
}



/*****************************************************************************
 * Function Name: Cy_U3V_AppStreamDmaCallback
 ******************************************************************************
 * Summary:
 * HBDMA callback function to add the UVC header to the frame/buffer coming
 * from LVDS and commits to USB.
 *
 * \param handle
 * HBDMA channel handle
 *
 * \param cy_en_hbdma_cb_type_t
 * HBDMA channel type
 *
 * \param pbufStat
 * fHBDMA buffer status
 *
 * \param userCtx
 * user context
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_U3V_AppStreamDmaCallback(
    cy_stc_hbdma_channel_t *handle,
    cy_en_hbdma_cb_type_t type,
    cy_stc_hbdma_buff_status_t *pbufStat,
    void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;
    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        Cy_U3V_AppHandleProduceEvent(pAppCtxt, handle);
    }
    else if (type == CY_HBDMA_CB_CONS_EVENT)
    {
        if (pAppCtxt->isLvdsWltoUsbHs) 
        {
            APP_DBG_TRACE1("SET FWRDY\r\n");
            pAppCtxt->fwDmaReadyStatus = true;
            Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
        }
#ifdef PRINT_FRAME_INFO
        glCons++;
        if(glFlags.lastFrameProduced)
        {
            glConsCount = glCons;
            glCons = 0;
            glPrintFlag = 1; /* Frame ends here*/
        }
#endif /* PRINT_FRAME_INFO*/  
        if (glFlags.lastFrameProduced)
        {
            glFlags.lastFrameProduced = false;
            glFlags.lastFrameConsumed = true;
        }
    }
    else
    {
        APP_DBG_ERR("DSI CB type:%x:\r\n", type);
    }
    Cy_USB_App_KeepLinkActive(pAppCtxt);
} /* end of function  */


/*****************************************************************************
 * Function Name: Cy_App_UpdateLinkTrainingStatus
 ******************************************************************************
 * Summary:
 * This function updates FPGA registers for training status and prints
 *
 * \param void
 * void
 *
 * \return
 * void
 *
 *******************************************************************************/
void Cy_App_UpdateLinkTrainingStatus(void)
{

#if TRAINING_SEQ_REQ
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
#if (WL_EN)
#if (!LVCMOS_EN)
    APP_DBG_INFO("\r\nPHY TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P0_PHY_TRAINING_DONE))
    {
        APP_DBG_ERR("P0 FAIL, ");
    }
    else
    {
        APP_DBG_MISC("P0 COMPLETE, ");
    }
    if (!(Cy_LVDS_GetTrainingStatus() & P1_PHY_TRAINING_DONE))
    {
        APP_DBG_ERR("P1 FAIL, ");
    }
    else
    {
        APP_DBG_MISC("P1 COMPLETE, ");
    }
#endif /* (!LVCMOS_EN) */
    APP_DBG_INFO("LINK TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P0_TRAINING_DONE))
    {
        APP_DBG_ERR("P0 FAIL [0x%x], ", LVDSSS_LVDS->LINK_TRAINING_STS[0]);
    }
    else
    {
        APP_DBG_MISC("P0 PASS, ");
    }
    if (!(Cy_LVDS_GetTrainingStatus() & P1_TRAINING_DONE))
    {
        APP_DBG_ERR("P1 FAIL [0x%x]\r\n\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[1]);
    }
    else
    {
        APP_DBG_MISC("P1 PASS\r\n\r\n");
    }
    if (Cy_LVDS_GetTrainingStatus() & WL_TRAINING_DONE)
    {
#if FPGA_ENABLE
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, FPGA_PHY_LINK_CTRL_ADDR, PORT0_LINK_TRAINING_DONE | PORT1_LINK_TRAINING_DONE, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* FPGA_ENABLE */
    }
#elif ((!PORT1_EN))
#if (!LVCMOS_EN)
    APP_DBG_INFO("\r\nPHY TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P0_PHY_TRAINING_DONE))
    {
        APP_DBG_ERR("P0 FAIL, ");
    }
    else
    {
        APP_DBG_MISC("P0 COMPLETE, ");
    }
#endif /* (!LVCMOS_EN) */
    APP_DBG_INFO("LINK TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P0_TRAINING_DONE))
    {
        APP_DBG_ERR("P0 FAIL [0x%x]\r\n\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[0]);
    }
    else
    {
        APP_DBG_MISC("P0 PASS\r\n\r\n");
#if FPGA_ENABLE
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, FPGA_PHY_LINK_CTRL_ADDR, PORT0_LINK_TRAINING_DONE, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* FPGA_ENABLE */
    }
#else  /* ((PORT1_EN)) */
#if (!LVCMOS_EN)
    APP_DBG_INFO("\r\nPHY TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P1_PHY_TRAINING_DONE))
    {
        APP_DBG_ERR("P1 FAIL, ");
    }
    else
    {
        APP_DBG_MISC("P1 COMPLETE, ");
    }
#endif /* (!LVCMOS_EN) */
    APP_DBG_INFO("LINK TRAINING: ");
    if (!(Cy_LVDS_GetTrainingStatus() & P1_TRAINING_DONE))
    {
        APP_DBG_ERR("P1 FAIL [0x%x]\r\n\r\n", LVDSSS_LVDS->LINK_TRAINING_STS[1]);
    }
    else
    {
        APP_DBG_MISC("P1 PASS\r\n\r\n");
#if FPGA_ENABLE
        status = Cy_FPGA_WriteRegister(FPGASLAVE_ADDR, FPGA_PHY_LINK_CTRL_ADDR, PORT1_LINK_TRAINING_DONE, true);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* FPGA_ENABLE */
    }
#endif /* WL */

#else
    APP_DBG_MISC("\r\nTraining Sequence NOT required\r\n\r\n");
#endif /* TRAINING_SEQ_REQ */
}


/*****************************************************************************
 * Function Name: Cy_App_LvdsLinkReadyPinConfig
 ******************************************************************************
 * Summary:
 * This function clears the Link Ready LVCMOS Control signal pin
 *
 * \param index
 * LVCMOS Control pin used for Link ready signal
 *
 * \return
 * void
 *
 *******************************************************************************/
void Cy_App_LvdsLinkReadyPinConfig(uint8_t index)
{
#if TRAINING_SEQ_REQ
    cy_en_lvds_phy_gpio_index_t indexLinkRdy;
    ASSERT_NON_BLOCK(index <= 9, index);
    indexLinkRdy = (cy_en_lvds_phy_gpio_index_t)(16+index);
    Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, PORT_VAL, indexLinkRdy,
                              CY_LVDS_PHY_GPIO_OUTPUT, CY_LVDS_PHY_GPIO_NO_INTERRUPT);
    Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, PORT_VAL, indexLinkRdy); /* Initailly reset the pin */
#endif /* #if TRAINING_SEQ_REQ */    
}
/*****************************************************************************
 * Function Name: Cy_App_LvdsLinkReadyPinClr
 ******************************************************************************
 * Summary:
 * This function clears the Link Ready LVCMOS Control signal pin
 *
 * \param index
 * LVCMOS Control pin used for Link ready signal
 *
 * \return
 * void
 *
 *******************************************************************************/
void Cy_App_LvdsLinkReadyPinClr(uint8_t index)
{
#if TRAINING_SEQ_REQ
    cy_en_lvds_phy_gpio_index_t indexLinkRdy;
    ASSERT_NON_BLOCK(index <= 9, index);
    indexLinkRdy = (cy_en_lvds_phy_gpio_index_t)(16+index);
    Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, PORT_VAL, indexLinkRdy);
#endif /* #if TRAINING_SEQ_REQ */
}

/*****************************************************************************
 * Function Name: Cy_App_LvdsLinkReadyPinSet
 ******************************************************************************
 * Summary:
 * This function sets the Link Ready LVCMOS Control signal pin
 *
 * \param index
 * LVCMOS Control pin used for Link ready signal
 *
 * \return
 * void
 *
 *******************************************************************************/
void Cy_App_LvdsLinkReadyPinSet(uint8_t index)
{
#if TRAINING_SEQ_REQ
    cy_en_lvds_phy_gpio_index_t indexLinkRdy;
    ASSERT_NON_BLOCK(index <= 9, index);
    indexLinkRdy = (cy_en_lvds_phy_gpio_index_t)(16+index);
    Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, PORT_VAL, indexLinkRdy);
#endif /* #if TRAINING_SEQ_REQ */
}

/*****************************************************************************
 * Function Name: Cy_App_GetDciCmdDmaBuffer
 ******************************************************************************
 * Summary:
 * This function clears the Link Ready LVCMOS Control signal pin
 *
 * \param pAppCtxt
 * Pointer to app context
 *
 * \return
 * none
 *
 *******************************************************************************/
void Cy_App_GetDciCmdDmaBuffer(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_buff_status_t buffStat;
    
    status = Cy_HBDma_Channel_GetBuffer(glChHandleDCICmd, &buffStat);
    if (status == CY_HBDMA_MGR_SUCCESS) {
        memcpy(glU3vCmdBuffer, (uint8_t *)buffStat.pBuffer, buffStat.count);
        status = Cy_HBDma_Channel_DiscardBuffer(glChHandleDCICmd, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS){
        APP_DBG_ERR("DCIcmd Discard Buffer Error:%x\r\n", status);
        }
    } else {
        APP_DBG_ERR("DCIcmd GetBuffer Error:%x\r\n", status);
    }
}

/*****************************************************************************
 * Function Name: Cy_App_EnableUSBSSConnection
 ******************************************************************************
 * Summary:
 * This function checkes VBus and enables USB SS connection 
 *
 * \param pAppCtxt
 * Pointer to app context
 *
 * \return
 * status
 *
 *******************************************************************************/
void Cy_App_EnableUSBSSConnection(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    bool connected = false;
    uint8_t maxWaitCounter = 255;
    /* Check VBus and enable the USB connection. */
    pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);
    if (pAppCtxt->vbusPresent)
    {
        connected = Cy_USB_SSConnectionEnable(pAppCtxt);
    }
    else
    {
        APP_DBG_ERR("vbus is not present, USB connection failed\r\n");
    }

#if USB3_LPM_ENABLE
    if ((pAppCtxt->isLpmEnabled == false) && (pAppCtxt->lpmEnableTime != 0))
    {
        Cy_USBSS_Cal_GetLinkPowerState(pAppCtxt->pUsbdCtxt->pSsCalCtxt, &curLinkState);
        if (
            (Cy_USBD_GetTimerTick() >= pAppCtxt->lpmEnableTime) &&
            (curLinkState != CY_USBSS_LPM_U3))
        {
            APP_DBG_TRACE1("LPM re-enable\r\n");
            pAppCtxt->isLpmEnabled = true;
            pAppCtxt->lpmEnableTime = 0;
            Cy_USBD_LpmEnable(pAppCtxt->pUsbdCtxt);
        }
    }
#endif /* USB3_LPM_ENABLE */

    /* Wait till USB device connection to host completed*/
    while (glIsDeviceConfigured)
    {
        /* This delay provides dbg task to print messages which are logged during USB connection */
        vTaskDelay(pdMS_TO_TICKS(50));
        maxWaitCounter--;
        if (maxWaitCounter == 0)
        {
            break;
        }
    }
    /* This delay provides dbg task to print messages which are logged during USB connection */
    vTaskDelay(pdMS_TO_TICKS(500));
    APP_DBG_INFO("\r\n\nUSB Connection[%s]: ",
                 pAppCtxt->devSpeed == 6   ? "20 Gbps"
                 : pAppCtxt->devSpeed == 5 ? "10 Gbps"
                 : pAppCtxt->devSpeed == 4 ? "5*2 Gbps"
                 : pAppCtxt->devSpeed == 3 ? "5 Gbps"
                                           : "480 Mbps");
    APP_DBG_MISC("%s\r\n\r\n", connected ? "SUCCESSFUL" : "FAILED");
}

/*****************************************************************************
 * Function Name: Cy_App_InitiateFPGAConfig
 ******************************************************************************
 * Summary:
 * This function call APIs to config and reset link ready signal, init QSPI interface and config FPGA 
 *
 * \param void
 * 
 *
 * \return
 * status
 *
 *******************************************************************************/
void Cy_App_InitiateFPGAConfig(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    cy_en_flashMode_t flashMode = QUAD_MODE;

#if TRAINING_SEQ_REQ /*In case of LVDS or LVCMOS DDR WL*/
    Cy_App_LvdsLinkReadyPinConfig(LINK_READY_CTL_PIN);
    Cy_App_LvdsLinkReadyPinClr(LINK_READY_CTL_PIN);
#endif /*TRAINING_SEQ_REQ*/
    Cy_FPGA_InitConfigurationPins();

    /*SMIF*/
    status = Cy_SMIF_Start(pAppCtxt);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if(mode == PASSIVE_SERIAL_x8_MODE)
    {
        flashMode = OCTAL_MODE;
    }
    status = Cy_SPI_FlashInit(SPI_FLASH_0, flashMode);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    Cy_FPGA_InitiateConfiguration(mode);
}


/*****************************************************************************
 * Function Name: Cy_App_EnableUSBSSConnection
 ******************************************************************************
 * Summary:
 * This function call APIs to initialize FPGA I2C registers, Detects the Image source and update stream parameters
 *
 * \param pAppCtxt
 * Pointer to app context
 *
 * \return
 * status
 *
 *******************************************************************************/
void Cy_App_InitiateFPGAI2CParameters(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    bool fpgaConfigFlag = false;
    fpgaConfigFlag = Cy_FPGA_IsConfigurationCompleted();
    if (fpgaConfigFlag)
    {
        Cy_FPGA_InitializeRegisters(FPGASLAVE_ADDR);
        APP_DBG_INFO("\r\nData source initialization started\r\n");
        pAppCtxt->sensorType = SRC_MIPI_SENSOR_NA;
        pAppCtxt->sensorType = Cy_ImgSensor_GetConnectedSensor();
        APP_DBG_INFO("MIPI SRC Detected: [%s]\r\n", glDataSourceName[pAppCtxt->sensorType]);
        /* If no mipi sensor is connected then switch to FPGA Internal generated colorbar*/
        if (pAppCtxt->sensorType != SRC_MIPI_SENSOR_NA)
        {
#if MIPI_INTERFACE_EN
            Cy_ImgSensor_Init(pAppCtxt->sensorType, glFrameParams.width, glFrameParams.height, glFrameParams.fps);
#endif /*MIPI_INTERFACE_EN*/
        }
        else
        {
            pAppCtxt->sensorType = SRC_FPGA_CLRBAR;
            APP_DBG_INFO("Streaming SRC Updated: [%s]\r\n", glDataSourceName[pAppCtxt->sensorType]);
        }
        APP_DBG_INFO("Data source initialization completed\r\n\r\n");
        Cy_FPGA_UpdateFrameParameters(FPGASLAVE_ADDR, glFrameParams.width, glFrameParams.height, glFrameParams.fps, pAppCtxt->sensorType);
    }
}
/***********************************************************************************************
 * Function Name: Cy_U3V_AppTaskHandler
 **************************************************************************************************
 * Summary:
 * Application task handler
 *
 * Parameters:
 * \param pTaskParam
 * task param
 *
 * Return:
 * void
 ************************************************************************************************/
void Cy_U3V_AppTaskHandler(void *pTaskParam)
{
    cy_stc_usbd_app_msg_t queueMsg;
    cy_en_hbdma_mgr_status_t mgrStatus;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    BaseType_t xStatus;
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    
    cy_en_usbss_lnk_power_mode_t curLinkState;
    uint32_t lpEntryTime = 0;
    bool isStall = false;
    
    uint8_t isRespSent = 0;
    uint8_t isDciIdle = 0;

    /* This delay provides dbg task to print messages which are logged before schedular starts*/
    vTaskDelay(pdMS_TO_TICKS(100));
    APP_DBG_INFO("\r\n\r\nAPP TASK STARTED\r\n\r\n");

#if FPGA_ENABLE  /* Configure FPGA and initialize I2C registers to default values*/
    vTaskDelay(pdMS_TO_TICKS(100)); /* Delay required to empty log buffer */
    cy_stc_usbd_app_msg_t xMsg;
    xMsg.type = CY_USB_U3V_FPGA_CONFIGURATION;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    xMsg.type = CY_USB_U3V_FPGA_PARAMETER_INIT;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    xMsg.type = CY_USB_U3V_LVDS_LVCMOS_INIT;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
#endif /* FPGA_ENABLE*/
    xMsg.type = CY_USB_U3V_APP_USB_CONNECT;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    xMsg.type = CY_USB_U3V_APP_BUILD_INFO;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    
    do
    {
#if PRINT_FRAME_INFO
        if (glPrintFlag)
        {
            glPrintFlag = 0;
            APP_DBG_TRACE("Prod: %d, Cons: %d, PartialBuf: %d, TotalSz: %d, BlkID: [0x%x] [%d]\r\n",
                          glProdCount, glConsCount, glPartialBufSize,
                          glFrameSize, glBlockIDTrailer, glBlockIDTrailer);
        }
#endif /* PRINT_FRAME_INFO */
        /*
         * Wait until some data is received from the queue.
         * Timeout after 20 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->appMsgQ, &queueMsg, 20);
        if (xStatus != pdPASS)
        {
            continue;
        }
        switch (queueMsg.type)
        {
#if FPGA_ENABLE
        case CY_USB_U3V_FPGA_CONFIGURATION:
            Cy_App_InitiateFPGAConfig(pAppCtxt, FPGA_CONFIG_MODE);
            xTimerStart(pAppCtxt->userTimer, 0); /* Start user timer to print calculated FPS and FPGA status*/
            break;
        case CY_USB_U3V_FPGA_PARAMETER_INIT:
            Cy_App_InitiateFPGAI2CParameters(pAppCtxt);
            break;
        case CY_USB_U3V_LVDS_LVCMOS_INIT:
            Cy_App_LVDS_LVCMOS_Init();
            break;
        case CY_USB_U3V_APP_USB_CONNECT:
            Cy_App_EnableUSBSSConnection(pAppCtxt);
            /* U3V interface init should be done as early as USB connection done, so that Host app can access xml asap device is selected*/
            Cy_App_InitializeU3vInterface(pAppCtxt);
            break;
#endif /* FPGA_ENABLE*/
        case CY_USB_U3V_APP_BUILD_INFO:
            Cy_App_PrintStartupLogs(pAppCtxt->sensorType);
#if TRAINING_SEQ_REQ
            Cy_App_LVDS_REG_Print();
            Cy_App_UpdateLinkTrainingStatus();
            Cy_App_TrainingStatus(pAppCtxt);
#endif /* TRAINING_SEQ_REQ */
            xMsg.type = CY_USB_U3V_FPGA_STATUS_POLL_EVENT; /* Read FPGA status once training/initialization is done*/
            Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
            break;

        case CY_USB_U3V_DMA_BLOCKED_FLAG:
            APP_DBG_TRACE1("CASE:App has already stopped. Blocked the next packet\r\n");
            break;

        case CY_USB_U3V_ZLP_DISCARDED:
            APP_DBG_ERR("CASE:Received a ZLP in-between payload.Discarded, Current BlkID: %d\r\n", glU3VDSIBlockID);
            break;

        case CY_USB_U3V_NO_LEADER_RECVD:
            APP_DBG_ERR("CASE:No leader received before payload, Current BlkID: %d\r\n", glU3VDSIBlockID);
            break;

        case CY_USB_U3V_NO_TRAILER_RECVD:
            APP_DBG_ERR("CASE:No trailer received after payload, Current BlkID: %d\r\n", glU3VDSIBlockID);
            break;

        case CY_USB_U3V_TRAILER_RXD_FLAG:
            APP_DBG_TRACE1("CASE:Trailer received BlockID:0x%x[%d]\r\n", glU3VDSIBlockID, glU3VDSIBlockID);
            break;

        case CY_USB_U3V_LEADER_RXD_FLAG:
            APP_DBG_TRACE1("CASE:Leader received BlockID:0x%x[%d]\r\n", glU3VDSIBlockID, glU3VDSIBlockID);
            break;

        case CY_USB_U3V_FRAME_MISSING_FLAG:
            APP_DBG_ERR("CASE:Incomplete frame sent to USB [%d]\r\n", queueMsg.data[0]);
            break;

        /* DCI Response EP Halt */
        case CY_USB_U3V_EPHALT_DCI_RSP_EVT_FLAG:
            Cy_U3V_AppDCIRspHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, true);
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_SET, EP_IN);
            break;

        /* DCI Response EP Clear */
        case CY_USB_U3V_EPCLR_DCI_RSP_EVT_FLAG:
            Cy_U3V_AppDCIRspHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, false);
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_CLEAR, EP_IN);
            break;

        /* DCI Command EP Halt */
        case CY_USB_U3V_EPHALT_DCI_CMD_EVT_FLAG:
            Cy_U3V_AppDCICmdHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, true);
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_SET, EP_OUT);
            break;

        /* DCI Command EP Clear */
        case CY_USB_U3V_EPCLR_DCI_CMD_EVT_FLAG:
            Cy_U3V_AppDCICmdHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, false);
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_CLEAR, EP_OUT);
            break;

        /* DSI EP Halt */
        case CY_USB_U3V_EPHALT_DSI_EVT_FLAG:
            isStall = Cy_U3V_AppDSIHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, true);
            Cy_U3V_DsiEphHandler(&u3vDevCtxt,isStall);
            break;

        /* DSI EP Clear */
        case CY_USB_U3V_EPCLR_DSI_EVT_FLAG:
            Cy_U3V_AppStreamAcqStop(pAppCtxt, pAppCtxt->pUsbdCtxt);
            isStall = Cy_U3V_AppDSIHaltEventHandler(pAppCtxt, pAppCtxt->pUsbdCtxt, false);
            Cy_U3V_DsiEphHandler(&u3vDevCtxt,isStall);
            break;

        case CY_USB_U3V_SETCONF_EVT_FLAG:
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_CLEAR, EP_OUT);
            Cy_U3V_DciEphHandler(&u3vDevCtxt,EPH_CLEAR, EP_IN);
            Cy_U3V_DsiEphHandler(&u3vDevCtxt,EPH_CLEAR);
            xTimerStop(pAppCtxt->dciTimerHandle, 0);
            break;
        case CY_USB_U3V_RESP_SENT_EVT_FLAG:
            /* Command send success.Cleanup buffer and restart fetching U3V Commands */
            Cy_U3V_IsSentRspPendingAck(&u3vDevCtxt,&isRespSent);
            if(isRespSent == 1u)
            {
                APP_DBG_INFO("Pending ACK Xfer Done\r\n");
            }
            else
            {
                Cy_U3V_UpdateDciStateAfterRspSent(&u3vDevCtxt);
                Cy_U3V_IsDCIStateIdle(&u3vDevCtxt, &isDciIdle);
                if(isDciIdle == 1u){
                xTimerStop(pAppCtxt->dciTimerHandle, 0);
                memset(glU3vResponseBuffer, 0, CMD_RSP_BUFF_SIZE);
                }
            }
            break;
        case CY_USB_U3V_CMD_EVT_FLAG:
            /* U3V Command Received by device */
            Cy_App_GetDciCmdDmaBuffer(pAppCtxt);
            xTimerReset(pAppCtxt->dciTimerHandle, 0);
            /* Move to Process Command state - U3V Spec: Device Control Interface State Diagram */
            Cy_U3V_DciCmdHandler(&u3vDevCtxt,glU3vCmdBuffer, glU3vResponseBuffer);
            break;
        case CY_USB_U3V_VBUS_CHANGE_INTR:
            /* Start the debounce timer. */
            xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
            break;

        case CY_USB_U3V_VBUS_CHANGE_DEBOUNCED:
            /* Check whether VBus state has changed. */
            pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);
            if (pAppCtxt->vbusPresent)
            {
                if (!pAppCtxt->usbConnectDone)
                {
                    APP_DBG_TRACE("Enabling USB connection due to VBus detect\r\n");
                    Cy_USB_SSConnectionEnable(pAppCtxt);
                }
            }
            else
            {
                if (pAppCtxt->usbConnectDone)
                {
                    Cy_USB_AppDisableEndpDma(pAppCtxt);
                    APP_DBG_TRACE("Disabling USB connection due to VBus removal\r\n");
                    Cy_USB_SSConnectionDisable(pAppCtxt);
                }
            }
            break;
#if FPGA_ENABLE
        case CY_USB_U3V_FPGA_STATUS_POLL_EVENT:
            Cy_FPGA_PrintStatus(FPGASLAVE_ADDR, pAppCtxt->sensorType);
            break;
#endif
        case CY_USB_U3V_FPS_POLL_EVENT:
            if (glFpsPrint)
            {
                APP_DBG_MISC("FPS: %d, BW: ~%d Mbps\r\n\r\n", glFpsPrint, glFpsPrint * glFrameParams.imageSize / 125000); /* Byte to Mb(Mega bits) conversion (8/1000000) = 1/125000*/
            }
            else 
            {
                APP_DBG_ERR("Frame not received, FPS: %d\r\n", glFpsPrint);
                APP_DBG_ERR("Is build config and FPGA bitfile correct?\r\n");
                Cy_App_PrintBuildConfigInErrState(pAppCtxt->sensorType);
            }
            break;
#if FPGA_ENABLE && MIPI_INTERFACE_EN
        case CY_APP_DEV_FPS_CONFIG_EVENT:
                status = CY_SCB_I2C_SUCCESS;
                if(pAppCtxt->sensorType == SRC_SENSOR_IMX){
                    status = Cy_ImgSensor_UpdateIMXFps(glFrameParams.fps);
                    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                } 
                else if (pAppCtxt->sensorType == SRC_SENSOR_THINE)
                {
                    status = Cy_ImgSensor_UpdateResTHine(glFrameParams.width, glFrameParams.height, glFrameParams.fps);
                    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                }
                if (status == CY_SCB_I2C_SUCCESS){
                    Cy_FPGA_UpdateStreamingFps(FPGASLAVE_ADDR,glFrameParams.fps);
                    APP_DBG_TRACE("%dx%d Sensor resolution is set\r\n", glFrameParams.width, glFrameParams.height);
                }  
            break;
            case CY_APP_DEV_RESOLUTION_CONFIG_EVENT:
                status = CY_SCB_I2C_SUCCESS;
                if(pAppCtxt->sensorType == SRC_SENSOR_IMX){
                    status = Cy_ImgSensor_UpdateResIMX(glFrameParams.width, glFrameParams.height, glFrameParams.fps);
                    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                } 
                else if (pAppCtxt->sensorType == SRC_SENSOR_THINE)
                {
                    status = Cy_ImgSensor_UpdateResTHine(glFrameParams.width, glFrameParams.height, glFrameParams.fps);
                    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                }
                if (status == CY_SCB_I2C_SUCCESS){
                    Cy_FPGA_UpdateFrameHeight(FPGASLAVE_ADDR,glFrameParams.height);
                    Cy_FPGA_UpdateFrameWidth(FPGASLAVE_ADDR,glFrameParams.width);
#if INMD_EN  && (!LVCMOS_EN)          
                    Cy_LVDS_UpdateMetadata(glFrameParams.height, glFrameParams.width, glFrameParams.imageSize);
#endif /* INMD_EN  && (!LVCMOS_EN)*/
                    APP_DBG_TRACE("%dx%d Sensor resolution is set\r\n", glFrameParams.width, glFrameParams.height);
                }                
            break;
#endif /* FPGA_ENABLE && MIPI_INTERFACE_EN */               
            default:
                APP_DBG_TRACE1("CASE:U3VDefault %d\r\n", queueMsg.type);
                break;

        } /* end of switch() */

        curLinkState = CY_USBSS_LPM_UNKNOWN;
        if ((glIsDeviceConfigured) && (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1))
        {
            Cy_USBSS_Cal_GetLinkPowerState(pAppCtxt->pUsbdCtxt->pSsCalCtxt, &curLinkState);
        }
        /*
         * If the link has been in USB2-L1 or in USB3-U2 for more than 0.5 second, initiate LPM exit so that
         * transfers do not get delayed significantly.
         */
        if (
            ((curLinkState == CY_USBSS_LPM_U2) && (!(pAppCtxt->pUsbdCtxt->pSsCalCtxt->forceLPMAccept))) ||
            ((pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS) &&
             ((MXS40USBHSDEV_USBHSDEV->DEV_PWR_CS & USBHSDEV_DEV_PWR_CS_L1_SLEEP) != 0)))
        {
            if ((Cy_USBD_GetTimerTick() - lpEntryTime) > 500UL)
            {
                lpEntryTime = Cy_USBD_GetTimerTick();
                Cy_USBD_GetUSBLinkActive(pAppCtxt->pUsbdCtxt);
            }
        }
        else
        {
            lpEntryTime = Cy_USBD_GetTimerTick();
        }

        /* Enable to get next DCI CMD DMA channel if DCI state is IDLE */
        Cy_U3V_IsDCIStateIdle(&u3vDevCtxt, &isDciIdle);
        if (isDciIdle == 1u){
            mgrStatus = CY_HBDMA_MGR_SUCCESS;
            mgrStatus = Cy_HBDma_Channel_Enable(glChHandleDCICmd, 0);
            if ((mgrStatus == CY_HBDMA_MGR_SUCCESS) || (mgrStatus == CY_HBDMA_MGR_SEQUENCE_ERROR))
            {
                Cy_U3V_UpdateDCIStateIdle_N(&u3vDevCtxt);
            }
            else
            {
                APP_DBG_ERR("DCIcmd Channel_Enable Error:%x\r\n", mgrStatus);
                Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, true);
                Cy_SysLib_DelayUs(100);
                Cy_HBDma_Channel_Reset(glChHandleDCICmd);
                APP_DBG_TRACE("DMA Channel reset: cmd\r\n");
                Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, false);
            }
        }
        if (pAppCtxt->dmaInterruptDisabled)
        {
            pAppCtxt->dmaInterruptDisabled = false;
#if (PORT1_EN || WL_EN)
            NVIC_EnableIRQ(lvds2usb32ss_lvds_dma_adap1_int_o_IRQn);
#endif /* (PORT1_EN || WL_EN) */
            NVIC_EnableIRQ(lvds2usb32ss_lvds_dma_adap0_int_o_IRQn);
        }
    } while (1);
} /* End of function  */

/*****************************************************************************
 * Function Name: Cy_USB_AppSetAddressCallback
 ******************************************************************************
 * Summary:
 *  This Function will be called by USBD layer when  a USB address has been assigned to the device.
 *
 * \param pUsbApp
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD layer context pointer.
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppSetAddressCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Update the state variables. */
    pAppCtxt->devState = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Check the type of USB connection and register appropriate descriptors. */
    Cy_USB_RegisterDscr(pAppCtxt, pAppCtxt->devSpeed);
}

/*****************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParamsSs
 ******************************************************************************
 * Summary:
 *  Perform high bandwidth DMA initialization associated with a USB endpoint.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 *  \param pEndpDscr
 * Endpoint descriptor
 *
 * Return:
 *  void
 *****************************************************************************/
static void
Cy_USB_AppSetupEndpDmaParamsSs(cy_stc_usb_app_ctxt_t *pAppCtxt,
                               uint8_t *pEndpDscr)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    cy_en_hbdma_mgr_status_t mgrStatus = CY_HBDMA_MGR_BAD_PARAM;
    cy_stc_app_endp_dma_set_t *pEndpDmaSet;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize=0;

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);
    APP_DBG_TRACE("EP:0x%x maxPktSize: %d dir:%s\r\n", endpNumber, maxPktSize, dir?"IN":"OUT");

    dmaConfig.count = 1;                            /* DMA Buffer Count */
    dmaConfig.bufferMode = false;                   /* DMA buffer mode disabled */
    dmaConfig.prodHdrSize = 0;                      /* No header to be added within the buffer */
    dmaConfig.prodSckCount = 1;                     /* No. of producer sockets */
    dmaConfig.consSckCount = 1;                     /* No. of consumer Sockets */
    dmaConfig.prodSck[1] = (cy_hbdma_socket_id_t)0; /* Producer Socket ID: None */
    dmaConfig.consSck[1] = (cy_hbdma_socket_id_t)0; /* Consumer Socket ID: None */
    dmaConfig.eventEnable = 0x00;                   /* No events to be sent. */
    dmaConfig.userCtx = (void *)(pAppCtxt);         /* Pass the application context as user context. */
    dmaConfig.usbMaxPktSize  = maxPktSize;

    if (dir)
    {
        /* Its IN endpoint which means DATA dir device->Host */
        pEndpDmaSet = &(pAppCtxt->endpInDma[endpNumber]);
        /* EP_DCI_RSP */
        /* Create channel which will move data from SRAM to USB endpoint. */
        if (endpNumber == CY_U3V_EP_DCI_RSP)
        {
            if (glChHandleDCIRsp != NULL) {
                /* Make sure channel is created afresh when SET_CONFIG is handled. */
                Cy_HBDma_Channel_Disable(glChHandleDCIRsp);
                Cy_HBDma_Channel_Destroy(glChHandleDCIRsp);
                glChHandleDCIRsp = NULL;
            }
            dmaConfig.size = CMD_RSP_BUFF_SIZE; /* DMA Buffer size in bytes */
            dmaConfig.prodBufSize = CMD_RSP_BUFF_SIZE;
            dmaConfig.chType = CY_HBDMA_TYPE_MEM_TO_IP; /* DMA Channel type: from HB-RAM to USB3-IP */
            dmaConfig.prodSck[0] = CY_HBDMA_VIRT_SOCKET_WR;
            dmaConfig.consSck[0] = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
            dmaConfig.cb = Cy_U3V_AppCtrlDmaCb; /* HB-DMA callback */
            dmaConfig.intrEnable    = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk |
                                      LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_TRANS_DONE_Msk;
                                      
            /* Change consumer socket for USB-HS case. */
            if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
            {
                dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
            }
            mgrStatus = Cy_HBDma_Channel_Create(pAppCtxt->pUsbdCtxt->pHBDmaMgr, &(pEndpDmaSet->hbDmaChannel), &dmaConfig);
            if (mgrStatus != CY_HBDMA_MGR_SUCCESS) 
            {
                APP_DBG_ERR("DMA DCI RSP: ep=0x%x BulkIn channel create failed 0x%x\r\n", endpNumber, mgrStatus);
                return;
            } else {
                pEndpDmaSet->valid = true;
                glChHandleDCIRsp = &(pEndpDmaSet->hbDmaChannel);
                APP_DBG_TRACE1("DMA DCI RSP: channel Created, prodSckCount: %d, consSckCount: %d\r\n", dmaConfig.prodSckCount, dmaConfig.consSckCount);
            }
        }
        else if (endpNumber == CY_U3V_EP_DSI_STREAM)
        {
            /* If channel had already been created, make sure to disable and destroy it. */
            if (glChHandleDSI != NULL) {
                DBG_APP_INFO("Destroying previously created streaming channel\r\n");
                Cy_HBDma_Channel_Disable(glChHandleDSI);
                Cy_HBDma_Channel_Destroy(glChHandleDSI);
                glChHandleDSI = NULL;
            }
            /* Create channel which will move data from LVDS to USB endpoint. */
            dmaConfig.size = STREAM_DMA_BUFFER_SIZE;   /* DMA Buffer size in bytes */
            dmaConfig.count = STREAM_DMA_BUFFER_COUNT; /* DMA Buffer Count */
            dmaConfig.prodBufSize = STREAM_DMA_BUFFER_SIZE;
            dmaConfig.chType = CY_HBDMA_TYPE_IP_TO_IP; /* DMA Channel type: from LVDS to USB3-IP */
            dmaConfig.bufferMode = true;
            dmaConfig.consSck[0] = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
#if PORT1_EN
            dmaConfig.prodSck[0] = CY_HBDMA_LVDS_SOCKET_16;
#if INTERLEAVE_EN
            dmaConfig.prodSckCount = 2;
            dmaConfig.prodSck[1] = CY_HBDMA_LVDS_SOCKET_17;
#endif /* INTERLEAVE_EN */
#else  /* PORT0 and WIDE LINK*/
            dmaConfig.prodSck[0] = CY_HBDMA_LVDS_SOCKET_00;
#if INTERLEAVE_EN
            dmaConfig.prodSckCount = 2;
            dmaConfig.prodSck[1] = CY_HBDMA_LVDS_SOCKET_01;
#endif /* INTERLEAVE_EN */
#endif /* PORT1_EN */
            
            /* Initial config for auto dma*/
            dmaConfig.eventEnable = 0x01;
            dmaConfig.intrEnable = 0x00;
            dmaConfig.cb = NULL; /* HB-DMA callback */

#if INMD_EN  && (!LVCMOS_EN)
            APP_DBG_TRACE("INMD (AUTO DMA): %s\r\n", dmaConfig.prodSck[1] ? "INTERLEAVED ENABLED" : "INTERLEAVED DISABLED");
#elif ((FPGA_ADDS_HEADER && (!DBG_HEADER_BY_FPGA)))
            APP_DBG_TRACE("FPGA ADDS HEADER (AUTO DMA): %s\r\n", dmaConfig.prodSck[1] ? "INTERLEAVED ENABLED" : "INTERLEAVED DISABLED");
#else
            /* Config update for manual dma (event will be generated for every produce/consume event)*/
            dmaConfig.eventEnable = 0x00;
            dmaConfig.intrEnable = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk | 
                                   LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
            dmaConfig.cb = Cy_U3V_AppStreamDmaCallback; /* HB-DMA callback */
#if DBG_HEADER_BY_FPGA
            APP_DBG_TRACE("FPGA ADDS HEADER (MANUAL DMA): %s\r\n", dmaConfig.prodSck[1] ? "INTERLEAVED ENABLED" : "INTERLEAVED DISABLED");
#else
            APP_DBG_TRACE("FX ADDS HEADER (MANUAL DMA): %s\r\n", dmaConfig.prodSck[1] ? "INTERLEAVED ENABLED" : "INTERLEAVED DISABLED");
#endif /* DBG_HEADER_BY_FPGA */
#endif /* INMD_EN  && (!LVCMOS_EN)*/
            /* Change consumer socket for USB-HS case. */
            if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
            {
                dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
                dmaConfig.eventEnable = 0x00; /* Event signalling cannot be used with USBHS endpoint. */
                dmaConfig.cb = Cy_U3V_AppStreamDmaCallback; /* HB-DMA callback */
                dmaConfig.intrEnable = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk | 
                                       LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
                APP_DBG_MISC("USBHS DMA Config (DSI)\r\n");
            }
            mgrStatus = Cy_HBDma_Channel_Create(pAppCtxt->pUsbdCtxt->pHBDmaMgr, &(pEndpDmaSet->hbDmaChannel), &dmaConfig);
            if (mgrStatus != CY_HBDMA_MGR_SUCCESS) 
            {
                APP_DBG_ERR("DMA DSI: ep=0x%x BulkIn channel create failed 0x%x\r\n", endpNumber, mgrStatus);
                return;
            } else {
                pEndpDmaSet->valid = true;
                glChHandleDSI = &(pEndpDmaSet->hbDmaChannel);
                APP_DBG_TRACE1("DMA DSI: channel Created, prodSckCount: %d, consSckCount: %d\r\n", dmaConfig.prodSckCount, dmaConfig.consSckCount);
            }
        }
        else
        {
            APP_DBG_ERR("Wrong BulkIn EP=0x%x\r\n", endpNumber);
            return;
        }
    }
    else
    {
        if (endpNumber == CY_U3V_EP_DCI_CMD)
        {
            /*  EP_DCI_CMD  */
            /* Its OUT endpoint which means data Host->Device. */
            pEndpDmaSet = &(pAppCtxt->endpOutDma[endpNumber]);

            if (glChHandleDCICmd != NULL) {
            Cy_HBDma_Channel_Disable(glChHandleDCICmd);
            Cy_HBDma_Channel_Destroy(glChHandleDCICmd);
            glChHandleDCICmd = NULL;
            }
            /* Create channel which moves data from USB ingress endpoint into HBW SRAM. */
            dmaConfig.size = CMD_RSP_BUFF_SIZE; /* DMA Buffer size in bytes */
            dmaConfig.bufferMode = true;        /* DMA buffer mode  */
            dmaConfig.prodBufSize = CMD_RSP_BUFF_SIZE;
            dmaConfig.chType = CY_HBDMA_TYPE_IP_TO_MEM;
            dmaConfig.prodSck[0] = (cy_hbdma_socket_id_t)(CY_HBDMA_USBIN_SOCKET_00 + endpNumber);
            dmaConfig.consSck[0] = CY_HBDMA_VIRT_SOCKET_RD;
            dmaConfig.cb = Cy_U3V_AppCtrlDmaCb;     /* HB-DMA callback */
            dmaConfig.eventEnable    = 0;
            dmaConfig.intrEnable = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk; /* Enable for Producer event */
            /* Change consumer socket for USB-HS case. */
            if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
            {
                dmaConfig.prodSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_OUT_EP_00 + endpNumber);
                APP_DBG_TRACE1("USBHS DMA Config (DCI CMD)\r\n");
            }
            mgrStatus = Cy_HBDma_Channel_Create(pAppCtxt->pUsbdCtxt->pHBDmaMgr, &(pEndpDmaSet->hbDmaChannel), &dmaConfig);
            if (mgrStatus != CY_HBDMA_MGR_SUCCESS) 
            {
                APP_DBG_ERR("DMA DCI CMD: ep=0x%x BulkOut channel create failed 0x%x\r\n", endpNumber, mgrStatus);
                return;
            } else {
                pEndpDmaSet->valid = true;
                glChHandleDCICmd = &(pEndpDmaSet->hbDmaChannel);
                APP_DBG_TRACE1("DMA DCI CMD: channel Created, prodSckCount: %d, consSckCount: %d\r\n", dmaConfig.prodSckCount, dmaConfig.consSckCount);
            }
        }
        else
        {
            APP_DBG_ERR("Wrong BulkOut EP=0x%x\r\n", endpNumber);
            return;
        }
     
    }
    return;
} /* end of function  */

/****************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParams
 ******************************************************************************
 * Summary:
 *  Configure and enable DMA channels.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pEndpDscr
 * Endpoint descriptor pointer
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                  uint8_t *pEndpDscr)
{
    /* Create the HBW DMA channels in all cases. */
    Cy_USB_AppSetupEndpDmaParamsSs(pAppCtxt, pEndpDscr);
}

/****************************************************************************
 * Function Name: Cy_USB_AppConfigureEndp
 ******************************************************************************
 * Summary:
 *  Configure all endpoints used by application (except EP0)
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param pEndpDscr
 * Endpoint descriptor pointer
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t *pCompDscr = NULL;
    cy_en_usbd_ret_code_t usbdRetCode;
    uint8_t interval = 0x00;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr))
    {
        return;
    }
    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType))
    {
        /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
        isoPkts = ((*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK) >> CY_USB_ENDP_ADDL_XN_POS) + 1;
    }

    valid = true;
    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS)
    {
        /* Get companion descriptor and from there get burstSize. */
        pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbdCtxt, pEndpDscr);
        Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);
        Cy_USBD_GetEndpCompnMaxStream(pCompDscr, &maxStream);
        if (CY_USB_ENDP_TYPE_ISO == endpType)
        {
            Cy_USBD_GetEndpCompnAttribute(pCompDscr, &isoPkts);
            isoPkts = (isoPkts & 0x03U) + 0x01U;
            isoPkts *= burstSize;

            /* Fetch the endpoint service interval. */
            Cy_USBD_GetEndpInterval(pEndpDscr, &interval);
        }
    }
    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.interval = interval;
    endpConfig.allowNakTillDmaRdy = false;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);
    /* Print status of the endpoint configuration to help debug. */
    APP_DBG_TRACE("#ENDPCFG: %d, %d\r\n", endpNumber, usbdRetCode);
    return;
} /* end of function */

/****************************************************************************
 * Function Name: Cy_USB_AppSetCfgCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when set configuration is received
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD layer context pointer.
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppSetCfgCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{

    cy_stc_usb_app_ctxt_t *pAppCtxt;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
    cy_en_usb_speed_t devSpeed;
    cy_stc_usbd_app_msg_t xMsg;

    APP_DBG_TRACE1("AppSetCfgCbStart\r\n");

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    devSpeed = pAppCtxt->devSpeed;
    /* Save the configuration number. */
    pAppCtxt->activeCfgNum = pUsbdCtxt->activeCfgNum;

    /* Destroy any active DMA channels. */
    Cy_USB_AppDisableEndpDma(pAppCtxt);
    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pAppCtxt->pCpuDw0Base);
    Cy_DMA_Enable(pAppCtxt->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg)
    {
        /* Set config should be called when active config value > 0x00. */
        DBG_APP_ERR("pActiveCfg:%d  return\r\n", pActiveCfg);
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00)
    {
        DBG_APP_ERR("numOfIntf:%d  return\r\n", numOfIntf);
        return;
    }
#if FPGA_ENABLE
    /* Identify whether we are operating in LVDS WideLink to USB 2.x transfer mode. */
    if (
            (lvdsContext.phyConfigP0->modeSelect == CY_LVDS_PHY_MODE_LVDS) &&
            (lvdsContext.phyConfigP0->wideLink) &&
            (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
       ) {
        pAppCtxt->isLvdsWltoUsbHs = true;
    } else {
        pAppCtxt->isLvdsWltoUsbHs = false;
    }
#else
    pAppCtxt->isLvdsWltoUsbHs = false;
#endif /* FPGA_ENABLE */
    for (index = 0x00; index < numOfIntf; index++)
    {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL)
        {
            DBG_APP_ERR("pIntfDscrNull\r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00)
        {
            APP_DBG_TRACE("numOfEndp: 0\r\n");
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;
            if (devSpeed > CY_USBD_USB_DEV_HS)
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            }
            else
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }
    }


    /* In USB 2.0 connections, register ISRs for and enable the DataWire channel interrupts. */
    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
    {
        Cy_USB_AppInitDmaIntr(CY_U3V_EP_DCI_CMD, CY_USB_ENDP_DIR_OUT, Cy_U3V_CommandChannel_ISR);
        Cy_USB_AppInitDmaIntr(CY_U3V_EP_DCI_RSP, CY_USB_ENDP_DIR_IN, Cy_U3V_ResponseChannel_ISR);
        Cy_USB_AppInitDmaIntr(CY_U3V_EP_DSI_STREAM, CY_USB_ENDP_DIR_IN, Cy_U3V_StreamChannel_ISR);
    }

    /* Zero out the command and response buffers. */
    memset(glU3vCmdBuffer, 0, CMD_RSP_BUFF_SIZE);
    memset(glU3vResponseBuffer, 0, CMD_RSP_BUFF_SIZE);

#if USB3_LPM_ENABLE
    /* Schedule LPM enable after 1 second. */
    APP_DBG_TRACE("Schedule LPM enable\r\n");
    pAppCtxt->isLpmEnabled = false;
    pAppCtxt->lpmEnableTime = Cy_USBD_GetTimerTick() + 1000;
#endif /* USB3_LPM_ENABLE */
    if (pAppCtxt->isLvdsWltoUsbHs) 
    {
        /* LVDS WL to USB 2.x transfer: We need to override the DMA_RDY signal as GPIO so
         * that LVDS ingress and USB egress transfers can be made non-overlapping.
         */
        Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, 0, FX_DMA_RDY_IO, CY_LVDS_PHY_GPIO_OUTPUT,
                CY_LVDS_PHY_GPIO_NO_INTERRUPT);

        DBG_APP_TRACE("CLR FWRDY\r\n");
        pAppCtxt->fwDmaReadyStatus = false;
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    } 
    else 
    {
        Cy_LVDS_PhyGpioModeDisable(LVDSSS_LVDS, 0, FX_DMA_RDY_IO);
    }
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_CONFIGURED;    
    APP_DBG_TRACE("AppSetCfgCbEnd Done\r\n");
    glIsDeviceConfigured = true;
    xMsg.type = CY_USB_U3V_SETCONF_EVT_FLAG;
    Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
    return;
} /* end of function */

/*******************************************************************************
 * Function name: Cy_USB_AppSuspendCallback
 ****************************************************************************/
/**
 *
 * Callback function will be invoked by USBD when Suspend signal/message is detected
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppSuspendCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    pAppCtxt->prevDevState = pAppCtxt->devState;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_SUSPEND;
} /* end of function. */

/*******************************************************************************
 * Function name: Cy_USB_AppResumeCallback
 ****************************************************************************/
/**
 *
 * Callback function will be invoked by USBD when Resume signal/message is detected
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppResumeCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    cy_en_usb_device_state_t tempState;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;

    tempState = pAppCtxt->devState;
    pAppCtxt->devState = pAppCtxt->prevDevState;
    pAppCtxt->prevDevState = tempState;
    return;
} /* end of function. */

/****************************************************************************
 * Function Name: Cy_USB_AppSetIntfCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when SET Interface is called
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppSetIntfCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_setup_req_t *pSetupReq;
    uint8_t intfNum, altSetting;
    int8_t numOfEndp;
    uint8_t *pIntfDscr, *pEndpDscr;
    uint32_t endpNumber;
    cy_en_usb_endp_dir_t endpDirection;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;

    APP_DBG_TRACE1("AppSetIntfCallback Start\r\n");
    pSetupReq = &(pUsbdCtxt->setupReq);
    /*
     * Get interface and alt setting info. If new setting same as previous
     * then return.
     * If new alt setting came then first Unconfigure previous settings
     * and then configure new settings.
     */
    intfNum = pSetupReq->wIndex;
    altSetting = pSetupReq->wValue;

    if (altSetting == pAppCtxt->prevAltSetting)
    {
        /* Nothing needs to be done. */
        APP_DBG_TRACE("SameAltSetting\r\n");
        return;
    }

    /* New altSetting is different than previous one so unconfigure previous. */
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, intfNum, pAppCtxt->prevAltSetting);
    APP_DBG_TRACE("unconfigPrevAltSet\r\n");
    if (pIntfDscr == NULL)
    {
        DBG_APP_ERR("pIntfDscrNull\r\n");
        return;
    }
    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        APP_DBG_TRACE("SetIntf:prevNumEp 0\r\n");
    }
    else
    {
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80)
            {
                endpDirection = CY_USB_ENDP_DIR_IN;
            }
            else
            {
                endpDirection = CY_USB_ENDP_DIR_OUT;
            }
            endpNumber =
                (uint32_t)((*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);

            /* with FALSE, unconfgure previous settings. */
            Cy_USBD_EnableEndp(pUsbdCtxt, endpNumber, endpDirection, FALSE);

            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
        }
    }

    /* Now take care of different config with new alt setting. */
    pAppCtxt->prevAltSetting = altSetting;
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, intfNum, altSetting);
    if (pIntfDscr == NULL)
    {
        DBG_APP_ERR("pIntfDscrNull\r\n");
        return;
    }

    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        APP_DBG_TRACE("SetIntf:numEp 0\r\n");
    }
    else
    {
        APP_DBG_TRACE("SETINTF: Configuring %d endpoints\r\n", numOfEndp);
        pAppCtxt->prevAltSetting = altSetting;
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
        }
    }
    APP_DBG_TRACE("AppSetIntfCallback done\r\n");
    return;
} /* end of function. */

/*******************************************************************************
 * Function name: Cy_USB_AppZlpCallback
 ****************************************************************************/
/**
 *
 * This Function will be called by USBD layer when ZLP message comes
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppZlpCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    APP_DBG_TRACE1("AppZlpCb\r\n");
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;

    /* If we have received a ZLP on OUT endpoint, pass it on to the DMA manager. */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_ZLP) {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), 0);
    }
    return;
} /* end of function. */

/*******************************************************************************
 * Function name: Cy_USB_AppL1SleepCallback
 ****************************************************************************/
/**
 *
 * This Function will be called by USBD layer when L1 Sleep message comes.
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppL1SleepCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    APP_DBG_TRACE1("AppL1SleepCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
 * Function name: Cy_USB_AppL1ResumeCallback
 ****************************************************************************/
/**
 *
 * This Function will be called by USBD layer when L1 Resume message comes.
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppL1ResumeCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    APP_DBG_TRACE1("AppL1ResumeCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
 * Function name: Cy_USB_AppSlpCallback
 ****************************************************************************/
/**
 *
 * This Function will be called by USBD layer when SLP message comes.
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppSlpCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    if (pMsg->type == CY_USB_CAL_MSG_OUT_SLP) 
    {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), pMsg->data[1]);
    }
} /* end of function. */


/*******************************************************************************
 * Function name: Cy_USB_AppInitDmaIntr
 ****************************************************************************/
/**
 *
 * Function to register an ISR for the DMA channel associated with an endpoint
 *
 * \param endpNumber
 * USB endpoint number
 *
 * \param endpDirection
 * Endpoint direction
 *
 * \param userIsr
 *  ISR function pointer. Can be NULL if interrupt is to be disabled.
 *
 * \return
 * None
 *
 ********************************************************************************/
void Cy_USB_AppInitDmaIntr(
    uint32_t endpNumber,
    cy_en_usb_endp_dir_t endpDirection,
    cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;
    if ((endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER))
    {
        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }

        if (userIsr != NULL)
        {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        }
        else
        {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
}

/****************************************************************************
 * Function Name: Cy_USB_AppSetupCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when SETUP packet is received
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppSetupCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType;
    uint8_t bTarget;
    uint16_t wValue, wIndex, wLength;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_stc_usbd_app_msg_t xMsg;

    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function. */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    bType = ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;
    APP_DBG_TRACE1("AppSetupCallback: bType = 0x%x, bTarget= 0x%x\r\n", bType, bTarget);
    if (bType == CY_USB_CTRL_REQ_STD)
    {
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (bRequest == CY_USB_SC_CLEAR_FEATURE))
        {
            /* Clear Feature(Endpoint_Halt) received on Endpoint */
            APP_DBG_TRACE("EP:0x%x Clear Feature 0x%x\r\n", wIndex, wValue);
            if (wValue == CY_USB_FEATURE_ENDP_HALT)
            {
                if (wIndex == CY_U3V_EP_DSI_STREAM_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPCLR_DSI_EVT_FLAG;
                }
                else if (wIndex == CY_U3V_EP_DCI_CMD_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPCLR_DCI_CMD_EVT_FLAG;
                }
                else if (wIndex == CY_U3V_EP_DCI_RSP_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPCLR_DCI_RSP_EVT_FLAG;
                }
                Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
                isReqHandled = true;
            }
        }
        else if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (bRequest == CY_USB_SC_SET_FEATURE))
        {
            /* SET Feature(Endpoint_Halt) received on Endpoint */
            APP_DBG_TRACE("EP:0x%x Set Feature 0x%x\r\n", wIndex, wValue);
            if (wValue == CY_USB_FEATURE_ENDP_HALT)
            {
                if (wIndex == CY_U3V_EP_DCI_CMD_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPHALT_DCI_CMD_EVT_FLAG;
                }
                else if (wIndex == CY_U3V_EP_DCI_RSP_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPHALT_DCI_RSP_EVT_FLAG;
                }
                else if (wIndex == CY_U3V_EP_DSI_STREAM_ADDR)
                {
                    xMsg.type = CY_USB_U3V_EPHALT_DSI_EVT_FLAG;
                }
                Cy_USB_AppSendMsgToTask(pAppCtxt, &xMsg);
                isReqHandled = true;
            }
        }
        else if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && ((bRequest == CY_USB_SC_SET_FEATURE) 
                    || (bRequest == CY_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsDeviceConfigured)
            {
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            }
            else
            {
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            }
            isReqHandled = true;
        }
        else if ((bRequest == CY_USB_SC_GET_STATUS) && (bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF))
        {
            /* We support only interface 0 */
            if (wIndex == 0)
            {
                Ep0TestBuffer[0] = 0;
                Ep0TestBuffer[1] = 0;
                Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)Ep0TestBuffer, 0x02);
            }
            else
            {
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            }
            isReqHandled = true;
        }
        else if ((bRequest == CY_USB_SC_SET_SEL) && (wLength == 6))
        {
            /* SET_SEL request is only received in USBSS case and the Cy_USB_USBD_RecvEndp0Data is blocking. */
            retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)SetSelDataBuffer, wLength);
            APP_DBG_TRACE("SET_SEL: EP0 recv stat = %d, Data=%x:%x\r\n",
                          retStatus, SetSelDataBuffer[0], SetSelDataBuffer[1]);
            isReqHandled = true;
        }
        else if ((bRequest == CY_USB_SC_SET_FEATURE) && (bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                 ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE)))
        {

#if USB3_LPM_ENABLE
            if (pAppCtxt->isLpmEnabled == false)
            {
                APP_DBG_TRACE("Enabling LPM\r\n");
                pAppCtxt->isLpmEnabled = true;
                Cy_USBD_LpmEnable(pUsbdCtxt);
            }
#endif /* USB3_LPM_ENABLE */

            Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }
        else if ((bRequest == CY_USB_SC_CLEAR_FEATURE) && (bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                 ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE)))
        {

#if USB3_LPM_ENABLE
            APP_DBG_TRACE("Disabling LPM\r\n");
            pAppCtxt->isLpmEnabled = false;
            pAppCtxt->lpmEnableTime = 0;
            Cy_USBD_LpmDisable(pUsbdCtxt);
#endif /* USB3_LPM_ENABLE */

            Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }
    }
    if (bType == CY_USB_CTRL_REQ_VENDOR)
    {
        APP_DBG_TRACE("CY_USB_CTRL_REQ_VENDOR Class Requests\r\n");
        isReqHandled = true;
    }
    if (bType == CY_USB_CTRL_REQ_CLASS)
    {
        APP_DBG_TRACE("CY_USB_CTRL_REQ_CLASS Class Requests\r\n");
    }

    /*
     * If Request is not handled by the callback, Stall the command.
     */
    if (!isReqHandled)
    {
        DBG_APP_ERR("SetupRequest is not handled\r\n");
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00,
                                      CY_USB_ENDP_DIR_IN, TRUE);
    }

} /* end of function. */

/****************************************************************************
 * Function Name: Cy_USB_AppBusSpeedCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when speed is identified or
 * speed change is detected
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD context
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppBusSpeedCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)Cy_USB_Dscr_DeviceHS);
    return;
} /* end of function. */

/****************************************************************************
 * Function Name: Cy_USB_AppBusResetDoneCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when RESET is completed
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppBusResetDoneCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    APP_DBG_TRACE("ppBusResetDoneCallback\r\n");

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->prevDevState = pAppCtxt->devState;
    return;
} /* end of function. */

/****************************************************************************
 * Function Name: Cy_USB_AppBusResetCallback
 ******************************************************************************
 * Summary:
 * Callback function will be invoked by USBD when bus detects RESET
 *
 * \param pUserCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD layer context pointer
 *
 * \param pMsg
 * USB Message
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppBusResetCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    uint8_t i;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUserCtxt;

    APP_DBG_TRACE1("AppBusResetCallback\r\n");

    for (i = 0; i < CY_USB_MAX_ENDP_NUMBER; i++)
    {
        if (pAppCtxt->endpInDma[i].valid)
        {
            APP_DBG_TRACE1("HBDMA destroy EP%d-In\r\n", i);
            Cy_HBDma_Channel_Disable(&(pAppCtxt->endpInDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpInDma[i].hbDmaChannel));
            pAppCtxt->endpInDma[i].valid = false;
        }
        if (pAppCtxt->endpOutDma[i].valid)
        {
            APP_DBG_TRACE1("HBDMA destroy EP%d-Out\r\n", i);
            Cy_HBDma_Channel_Disable(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pAppCtxt->endpOutDma[i].hbDmaChannel));
            pAppCtxt->endpOutDma[i].valid = false;
        }
    }
    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pAppCtxt, pUsbdCtxt, pAppCtxt->pCpuDmacBase,
                   pAppCtxt->pCpuDw0Base, pAppCtxt->pCpuDw1Base,
                   pAppCtxt->pHbDmaMgrCtxt);
    pAppCtxt->devState = CY_USB_DEVICE_STATE_RESET;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_RESET;
    pAppCtxt->activeCfgNum = 0;

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {

#if USB3_LPM_ENABLE
        APP_DBG_TRACE("Disabling LPM\r\n");
        Cy_USBSS_Cal_LPMDisable(pUsbdCtxt->pSsCalCtxt);
#endif /* USB3_LPM_ENABLE */
    }
    glIsDeviceConfigured = false;
    glIsStreamingActive = false;
    return;
} /* end of function. */

/*****************************************************************************
 * Function Name: Cy_USB_AppRegisterCallback
 ******************************************************************************
 * Summary:
 *  This function will register all calback with USBD layer.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET, Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE, Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED, Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP, Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND, Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME, Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG, Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF, Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP, Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME, Cy_USB_AppL1ResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP, Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP, Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR, Cy_USB_AppSetAddressCallback);
    return;
} /* end of function. */

/*****************************************************************************
 * Function Name: Cy_USB_AppTaskCreate
 ******************************************************************************
 * Summary:
 *  This function will create RTOS tasks required for application.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppTaskCreate(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    BaseType_t xStatus = pdFALSE;
    /* Create task and check status to confirm task created properly. */
#if DEBUG_INFRA_EN
    /* Create FreeRTOS tasks for serial debug logger */
    xTaskCreate(Cy_USB_PrintTaskHandler, "PrintTask", DBG_PRINT_TASK_STACK_DEPTH, NULL,
                DBG_PRINT_TASK_PRIORITY, &printLogTaskHandle);
#endif /* DEBUG_INFRA_EN */

    /* Create FreeRTOS tasks for U3V application */
    xStatus = xTaskCreate(Cy_U3V_AppTaskHandler, "AppTask", U3V_APP_TASK_STACK_DEPTH,
                         (void *)pAppCtxt, U3V_APP_TASK_PRIORITY, &(pAppCtxt->appTaskHandle));
    if (xStatus != pdPASS)
    {
        DBG_APP_ERR("Task create Fail, 0x%x\r\n", xStatus);
        return;
    }
    APP_DBG_TRACE("RTOS Tasks created\r\n");
}

/*****************************************************************************
 * Function Name: Cy_USB_AppQueueCreate
 ******************************************************************************
 * Summary:
 *  This function will create RTOS Queue required for application.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppQueueCreate(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    /* create queue and register it to kernel. */
    pAppCtxt->appMsgQ = xQueueCreate(CY_APP_NO_OF_MSG_Q, CY_APP_MSG_Q_SIZE);
    if (pAppCtxt->appMsgQ == NULL)
    {
        DBG_APP_ERR("Queue create Fail\r\n");
        return;
    }
    APP_DBG_TRACE("RTOS Queue created\r\n");
    vQueueAddToRegistry(pAppCtxt->appMsgQ, "AppMsgQueue");
}

/*****************************************************************************
 * Function Name: Cy_USB_AppTimerCreate
 ******************************************************************************
 * Summary:
 *  This function creates RTOS Timers for handling event.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppTimerCreate(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    pAppCtxt->dciTimerHandle = xTimerCreate("DCITimer", U3V_ABRM_MAX_RESPONSE_TIME, pdFALSE, (void *)pAppCtxt, Cy_U3V_AppPendingAckTimerCb);
    pAppCtxt->vbusDebounceTimer = xTimerCreate("VbusTimer", 200, pdFALSE, (void *)pAppCtxt, Cy_USB_VbusDebounceTimerCallback);
    pAppCtxt->userTimer = xTimerCreate("UserTimer", 1000, pdTRUE, (void *)pAppCtxt, Cy_USB_UserTimerCallback);
    pAppCtxt->evtLogTimer = xTimerCreate("EventLogTimer", 10000, pdTRUE, (void *)pAppCtxt, Cy_USB_PrintEvtLogTimerCb);
    if ((pAppCtxt->dciTimerHandle == NULL) || (pAppCtxt->vbusDebounceTimer == NULL) || (pAppCtxt->userTimer == NULL) || (pAppCtxt->evtLogTimer == NULL))
    {
        DBG_APP_ERR("Timer Create Fail\r\n");
        return;
    }
    APP_DBG_TRACE("RTOS Timers created\r\n");
    xTimerStart(pAppCtxt->evtLogTimer, 0);
}

/*****************************************************************************
 * Function Name: Cy_USB_AppHbdmaBufAllocation
 ******************************************************************************
 * Summary:
 *  This function allocates buffer size in high bandwidth DMA RAM.
 *
 * \param pHbDmaMgrCtxt
 * hbdma manager context pointer.
 *
 * \return
 * None
 *
 *******************************************************************************/
void Cy_USB_AppHbdmaBufAllocation(cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    /* Allocate DMA Memory to CMD and Response Buffer for DCI */
    glU3vCmdBuffer = (uint8_t *)Cy_HBDma_BufMgr_Alloc(pHbDmaMgrCtxt->pBufMgr, CMD_RSP_BUFF_SIZE);
    glU3vResponseBuffer = (uint8_t *)Cy_HBDma_BufMgr_Alloc(pHbDmaMgrCtxt->pBufMgr, CMD_RSP_BUFF_SIZE);
    APP_DBG_TRACE("HBDMA memory allocation done for U3V DCI buffers [size: %d B]\r\n", CMD_RSP_BUFF_SIZE);
}

/*****************************************************************************************
 * Function Name: Cy_USB_AppInit
 *****************************************************************************************
 * Summary:
 * This function Initializes application related data structures, register callback
 * creates task for device function.
 *
 * \param pAppCtxt
 * application layer context pointer.
 *
 * \param pUsbdCtxt
 * USBD layer Context pointer
 *
 * \param pCpuDmacBase
 * DMAC base address
 *
 * \param pCpuDw0Base
 * DataWire 0 base address
 *
 * \param pCpuDw1Base
 * DataWire 1 base address
 *
 * \param pHbDmaMgrCtxt
 * HBDMA Manager Context
 *
 * \return
 * None
 *
 ************************************************************************************ */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
                    DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    uint32_t index;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;

    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->enumMethod = CY_USB_ENUM_METHOD_FAST;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;
    pAppCtxt->dmaInterruptDisabled = false;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++)
    {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }
    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;

    if (!(pAppCtxt->firstInitDone))
    {
        pAppCtxt->vbusChangeIntr = false;
        pAppCtxt->vbusPresent = false;
        pAppCtxt->usbConnectDone = false;
        pAppCtxt->firstInitDone = 0x01;
    }
    /* Zero out the EP0 test buffer. */
    memset((uint8_t *)Ep0TestBuffer, 0, sizeof(Ep0TestBuffer));
    return;
} /* end of function. */

/*[]*/
