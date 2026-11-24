/***************************************************************************
*
* \file cy_usb_app.h
* \version 1.0
*
* Defines the interfaces used in the FX20 USB3Vision application.
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

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_common.h"
#include "cy_image_sensor.h"
#include "cy_app_lvds.h"
#include "cy_app_scb.h"
#if defined(__cplusplus)
extern "C"
{
#endif

#if DEBUG_INFRA_EN
#define DBG_PRINT_TASK_STACK_DEPTH (512)
#define DBG_PRINT_TASK_PRIORITY (5)
#endif /* DEBUG_INFRA_EN */
#define U3V_APP_TASK_STACK_DEPTH (2048)
#define U3V_APP_TASK_PRIORITY (11)

#define PHY_TRAINING_PATTERN_BYTE (0x9C)
#define LINK_TRAINING_PATTERN_BYTE (0x125A4B78)

#if (((LVCMOS_EN) && (LVCMOS_DDR_EN) && (WL_EN)) || (!LVCMOS_EN)) /* In case of LVCMOS DDR in Wide-Link or LVDS*/
#define TRAINING_SEQ_REQ (1)
#define PORT_VAL (PORT1_EN)    /* PORT 0 or PORT1 (WideLink will have PORT0) */
#define LINK_READY_CTL_PIN (6) /* LVCMOS CTRL PIN */
#else
#define TRAINING_SEQ_REQ (0)
#endif /* (((LVCMOS_EN) && (LVCMOS_DDR_EN) && (WL_EN)) || (!LVCMOS_EN)) */

#define FX_DMA_RDY_IO    (CY_LVDS_PHY_GPIO_CTL5) /* LVCMOS IO used as DMA-Ready signal to FPGA. */
/*
Enable to commit HBDMA data to USB in case of Data committed by FX (Header added by FX or DBG_HEADER_BY_FPGA=1),
- this must be 1 for streaming
- If it is '0', then FW will discard the DMA buffer
- Use for debugging LVDS/LVCMOS interface Xfer
*/
#define COMMIT_HBDMA_BUFFER (1)

/* Set DBG_HEADER_BY_FPGA=1 to enable dma transfer interrupt when FPGA is adding leader/trailer. */
/* CPU intervention will be enabled for commiting dma buffers for debugging */
#define DBG_HEADER_BY_FPGA (0)

/* Set to 1 to print produce events dbg messages for every xfer*/
#define PROD_EVNT_DBG (0)

/* Enable it to get Hex Code prints of FPGA status flags*/
#define FPGA_STATUS_PRINT (1)

/* Enable prod event prints to debug HBDMA data to USB in case of Mannual DMA, helps debugging */
#if (DBG_LEVEL > 3)
#define PRINT_FRAME_INFO (1) /* Enables detailed info of data frames*/
#define FX10_FPGA_DETAILED_STATUS (1) /* Enables detailed info of FPGA status, FPGA_STATUS_PRINT must be set to 1 */
#else
#define PRINT_FRAME_INFO (0)
#define FX10_FPGA_DETAILED_STATUS (0)
#endif /* (DBG_LEVEL > 3) */

#define CMD_RSP_BUFF_SIZE (0x400)
#define CY_APP_NO_OF_MSG_Q (20)
#define CY_APP_MSG_Q_SIZE (sizeof(cy_stc_usbd_app_msg_t))

#define CY_U3V_EP_DCI_CMD (0x01)    /* U3V Command (OUT) endpoint index. */
#define CY_U3V_EP_DCI_RSP (0x01)    /* U3V Response (IN) endpoint index. */
#define CY_U3V_EP_DSI_STREAM (0x03) /* U3V Streaming (IN) endpoint index. */

#define CY_U3V_EP_DCI_CMD_ADDR (CY_U3V_EP_DCI_CMD)
#define CY_U3V_EP_DCI_RSP_ADDR (0x80 | CY_U3V_EP_DCI_RSP)
#define CY_U3V_EP_DSI_STREAM_ADDR (0x80 | CY_U3V_EP_DSI_STREAM)

    typedef enum
    {
        CY_USB_U3V_CMD_EVT_FLAG,
        CY_USB_U3V_SETCONF_EVT_FLAG,
        CY_USB_U3V_DMA_RST_EVT_FLAG,
        CY_USB_U3V_USB_SUSP_EVT_FLAG,
        CY_USB_U3V_RESP_SENT_EVT_FLAG,
        CY_USB_U3V_EPHALT_DCI_RSP_EVT_FLAG,
        CY_USB_U3V_EPHALT_DCI_CMD_EVT_FLAG,
        CY_USB_U3V_EPHALT_DSI_EVT_FLAG,
        CY_USB_U3V_STARTUP_EVENT_FLAG,
        CY_USB_U3V_LEADER_RXD_FLAG,
        CY_USB_U3V_TRAILER_RXD_FLAG,
        CY_USB_U3V_FRAME_MISSING_FLAG,
        CY_USB_U3V_DMA_BLOCKED_FLAG,
        CY_USB_U3V_ZLP_DISCARDED,
        CY_USB_U3V_NO_LEADER_RECVD,
        CY_USB_U3V_PIB_OVERFLOW_ERROR_0,
        CY_USB_U3V_PIB_OVERFLOW_ERROR_3,
        CY_USB_U3V_FPGA_STATUS_CHECK_POLL,
        CY_USB_U3V_VBUS_CHANGE_INTR,
        CY_USB_U3V_VBUS_CHANGE_DEBOUNCED,
        CY_USB_U3V_FPGA_STATUS_POLL_EVENT,
        CY_USB_U3V_NO_TRAILER_RECVD,
        CY_USB_U3V_FPS_POLL_EVENT,
        CY_USB_U3V_EPCLR_DCI_RSP_EVT_FLAG,
        CY_USB_U3V_EPCLR_DCI_CMD_EVT_FLAG,
        CY_USB_U3V_EPCLR_DSI_EVT_FLAG,
        CY_APP_DEV_FPS_CONFIG_EVENT,
        CY_APP_DEV_RESOLUTION_CONFIG_EVENT,
        CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER,
        CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE,
        CY_USB_VCOM_DEVICE_MSG_SLP_OUT,
        CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE,
        CY_USB_UART_MSG_WRITE_COMPLETE,
        CY_USB_UART_MSG_READ_COMPLETE,
        CY_USB_VCOM_SETLINECODING_CMD,
        CY_USB_U3V_FPGA_CONFIGURATION,
        CY_USB_U3V_FPGA_PARAMETER_INIT,
        CY_USB_U3V_FPGA_STREAM_INFO_UPDATE,
        CY_USB_U3V_LVDS_LVCMOS_INIT,
        CY_USB_U3V_APP_BUILD_INFO,
        CY_USB_U3V_APP_USB_CONNECT,
    } Cy_App_TaskEvents_t;

    typedef struct
    {
        uint32_t width;
        uint32_t height;
        uint32_t fps;
        uint32_t bitsPerPixel;
        uint32_t totalFrameSize;
        uint32_t imageSize;
        uint32_t chunkSize;
        uint32_t payloadTransferCount;
        uint32_t payloadTransferSize;
        uint32_t finalTransfer2Size;
        uint32_t pixelFormat;
        uint32_t multiFrameCount;
    } glFrameParameters_t;

    typedef struct glFlags
    {
        volatile uint32_t frameOngoing;
        volatile uint32_t lastFrameProduced;
        volatile uint32_t lastFrameConsumed;
        volatile uint32_t acqStopReceived;
        volatile uint32_t blockFrame;
    } glFlags_t;

/* Debug prints in color*/
#define BLK "\033[0;30m"
#define RED "\033[0;31m"
#define GRN "\033[0;32m"
#define YEL "\033[0;33m"
#define BLU "\033[0;34m"
#define MAG "\033[0;35m"
#define CYN "\033[0;36m"
#define WHT "\033[0;37m"
#define HBLK "\033[0;90m"
#define HWHT "\033[0;97m"
#define COLOR_RESET "\033[0m"

#define APP_DBG_INFO(...)              \
    Cy_Debug_AddToLog(2, WHT);         \
    Cy_Debug_AddToLog(2, __VA_ARGS__); \
    Cy_Debug_AddToLog(2, COLOR_RESET);

#define APP_DBG_MISC(...)              \
    Cy_Debug_AddToLog(2, GRN);         \
    Cy_Debug_AddToLog(2, __VA_ARGS__); \
    Cy_Debug_AddToLog(2, COLOR_RESET);

#define APP_DBG_FPGA(...)              \
    Cy_Debug_AddToLog(2, CYN);         \
    Cy_Debug_AddToLog(2, __VA_ARGS__); \
    Cy_Debug_AddToLog(2, COLOR_RESET);

#define APP_DBG_SENSOR(...)            \
    Cy_Debug_AddToLog(2, BLU);         \
    Cy_Debug_AddToLog(2, __VA_ARGS__); \
    Cy_Debug_AddToLog(2, COLOR_RESET);

#define APP_DBG_TRACE(...)             \
    Cy_Debug_AddToLog(3, HBLK);        \
    Cy_Debug_AddToLog(3, __VA_ARGS__); \
    Cy_Debug_AddToLog(3, COLOR_RESET);

#define APP_DBG_TRACE1(...)            \
    Cy_Debug_AddToLog(4, HBLK);        \
    Cy_Debug_AddToLog(4, __VA_ARGS__); \
    Cy_Debug_AddToLog(4, COLOR_RESET);

#define APP_DBG_ERR(...)               \
    Cy_Debug_AddToLog(1, RED);         \
    Cy_Debug_AddToLog(1, "ERR: ");         \
    Cy_Debug_AddToLog(1, __VA_ARGS__); \
    Cy_Debug_AddToLog(1, COLOR_RESET);

    typedef enum
    {
        EPH_CLEAR,
        EPH_SET
    } Cy_EPH_State_t;

    typedef enum
    {
        EP_OUT,
        EP_IN
    } Cy_EPH_Type_t;

#define ASSERT(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) Cy_CheckStatusAndHandleFailure(__func__, __LINE__, condition, value, false, Cy_FailHandler);

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE (0u)

    typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

    /* USBD layer return code shared between USBD layer and Application layer. */
    typedef enum cy_en_usb_app_ret_code_
    {
        CY_USB_APP_STATUS_SUCCESS = 0,
        CY_USB_APP_STATUS_FAILURE,
    } cy_en_usb_app_ret_code_t;
/* Get the LS byte from a 16-bit number */
#define CY_GET_LSB(w) ((uint8_t)((w) & UINT8_MAX))

/* Get the MS byte from a 16-bit number */
#define CY_GET_MSB(w) ((uint8_t)((w) >> 8))

/* Retrieves byte 0 from a 32 bit number */
#define CY_DWORD_GET_BYTE0(d) ((uint8_t)((d) & 0xFF))

/* Retrieves byte 1 from a 32 bit number */
#define CY_DWORD_GET_BYTE1(d) ((uint8_t)(((d) >> 8) & 0xFF))

/* Retrieves byte 2 from a 32 bit number */
#define CY_DWORD_GET_BYTE2(d) ((uint8_t)(((d) >> 16) & 0xFF))

/* Retrieves byte 3 from a 32 bit number */
#define CY_DWORD_GET_BYTE3(d) ((uint8_t)(((d) >> 24) & 0xFF))

#define CHARMAP(c) (((c) >= 10) ? ('A' + (c) - 10) : ('0' + (c)))


/* Retrieves LS 2 bytes from a 32 bit number */
#define CY_GET_LS_HWORD(d)                                          ((uint16_t)(((d) >> 0) & 0xFFFF))

/* Retrieves MS 2 bytes from a 32 bit number */
#define CY_GET_MS_HWORD(d)                                          ((uint16_t)(((d) >> 16) & 0xFFFF))

#define FPGASLAVE_ADDR (0x0D) /* FPGA i2c address (Do not change) */

    /*
     * USB application data structure which is bridge between USB system and device
     * functionality.
     * It maintains some usb system information which comes from USBD and it also
     * maintains info about functionality.
     */
    struct cy_stc_usb_app_ctxt_
    {
        uint8_t firstInitDone;
        cy_en_usb_device_state_t devState;
        cy_en_usb_device_state_t prevDevState;
        cy_en_usb_speed_t devSpeed;
        cy_en_sensorType_t sensorType;
        uint8_t devAddr;
        uint8_t activeCfgNum;
        cy_en_usb_enum_method_t enumMethod;
        uint8_t prevAltSetting;
        bool gen2Enabled;
        bool usbConnectDone;

        cy_stc_app_endp_dma_set_t endpInDma[CY_USB_MAX_ENDP_NUMBER];
        cy_stc_app_endp_dma_set_t endpOutDma[CY_USB_MAX_ENDP_NUMBER];
        DMAC_Type *pCpuDmacBase;
        DW_Type *pCpuDw0Base;
        DW_Type *pCpuDw1Base;

        cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt;
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt;
        cy_stc_hbdma_mgr_context_t *pHbDmaMgr;

        /* Global Task handles */
        TaskHandle_t appTaskHandle;
        QueueHandle_t appMsgQ;

        /* Timer functionality for DCI */
        TimerHandle_t dciTimerHandle;
        uint32_t *pUsbEvtLogBuf;
        TimerHandle_t evtLogTimer; /** User timer handle. */
        bool dmaInterruptDisabled;
        bool vbusChangeIntr;             /** VBus change interrupt received flag. */
        bool vbusPresent;                /** VBus presence indicator flag. */
        bool isLpmEnabled;               /** Whether LPM transitions are enabled. */
        uint32_t lpmEnableTime;          /** Timestamp at which LPM should be re-enabled. */
        TimerHandle_t vbusDebounceTimer; /** VBus change debounce timer handle. */
        TimerHandle_t userTimer;         /** User timer handle. */
        uint8_t fpgaVersion;
        bool isLvdsWltoUsbHs;                  /* Whether we are streaming from LVDS WideLink to USBHS. should not be true before lvds training */
        bool fwDmaReadyStatus;                 /* Firmware based DMA ready status. */
    };

    extern cy_stc_hbdma_context_t HBW_DrvCtxt; /* High BandWidth DMA driver context. */
    extern cy_stc_usb_app_ctxt_t appCtxt;
    extern cy_stc_hbdma_mgr_context_t HBW_MgrCtxt; /* High BandWidth DMA manager context. */
#if DEBUG_INFRA_EN
    extern TaskHandle_t printLogTaskHandle;
#endif /* DEBUG_INFRA_EN */

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
                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                        DMAC_Type *pCpuDmacBase,
                        DW_Type *pCpuDw0Base,
                        DW_Type *pCpuDw1Base,
                        cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

    /*****************************************************************************
     * Function Name: Cy_USB_AppTaskCreate
     ******************************************************************************
     * Summary:
     *  This function creates RTOS tasks.
     *
     * \param pAppCtxt
     * application layer context pointer.
     *
     * \return
     * None
     *
     *******************************************************************************/
    void Cy_USB_AppTaskCreate(cy_stc_usb_app_ctxt_t *pAppCtxt);

    /*****************************************************************************
     * Function Name: Cy_USB_AppQueueCreate
     ******************************************************************************
     * Summary:
     *  This function creates RTOS Queue for IPC.
     *
     * \param pAppCtxt
     * application layer context pointer.
     *
     * \return
     * None
     *
     *******************************************************************************/
    void Cy_USB_AppQueueCreate(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
    void Cy_USB_AppTimerCreate(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
    void Cy_USB_AppHbdmaBufAllocation(cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

    /*****************************************************************************
     * Function Name: Cy_App_LvdsLinkReadyPinClr
     ******************************************************************************
     * Summary:
     *  This function clears the link ready LVCMOS control signal pin.
     *
     * \param index
     *  LVCMOS Control GPIO pin used for Link ready
     *
     * \return
     * None
     *
     *******************************************************************************/
    void Cy_App_LvdsLinkReadyPinClr(uint8_t index);

    /*****************************************************************************
     * Function Name: Cy_App_LvdsLinkReadyPinSet
     ******************************************************************************
     * Summary:
     *  This function sets the link ready LVCMOS control signal pin.
     *
     * \param index
     *  LVCMOS Control GPIO pin used for Link ready
     *
     * \return
     * None
     *
     *******************************************************************************/
    void Cy_App_LvdsLinkReadyPinSet(uint8_t index);

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
    void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
    void Cy_USB_AppSetCfgCallback(void *pAppCtxt,
                                  cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppBusResetCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
                                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                        cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppSetupCallback(void *pAppCtxt,
                                 cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                 cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppSuspendCallback(void *pAppCtxt,
                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                   cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppResumeCallback(void *pAppCtxt,
                                  cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppSetIntfCallback(void *pAppCtxt,
                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                   cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppL1SleepCallback(void *pUsbApp,
                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                   cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppL1ResumeCallback(void *pUsbApp,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

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
    void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

    /*******************************************************************************
     * Function name: CyApp_RegisterUsbDescriptors
     ****************************************************************************/
    /**
     *
     * This function register USB descriptor with USBD layer
     *
     * \param pAppCtxt
     * application layer context pointer.
     *
     * \param usbSpeed
     * USBD speed
     *
     * \return
     * None
     *
     ********************************************************************************/
    void Cy_USB_RegisterDscr(cy_stc_usb_app_ctxt_t *pAppCtxt,
                             cy_en_usb_speed_t usbSpeed);

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
    void Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t *pAppCtxt);

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
    void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                               cy_israddress userIsr);

    /*****************************************************************************
     * Function Name: Cy_USB_SSConnectionEnable
     *****************************************************************************
     * Summary
     *  PSVP specific USB connect function.
     *
     * Parameters:
     *  \param pAppCtxt
     *  Pointer to application context structure.
     *
     * Return:
     *  void
     ****************************************************************************/
    bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt);

    /*****************************************************************************
     * Function Name: Cy_USB_SSConnectionDisable
     *****************************************************************************
     * Summary
     *  PSVP specific USB disconnect function.
     *
     * Parameters:
     *  \param pAppCtxt
     *  Pointer to application context structure.
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt);

    /*****************************************************************************
     * Function Name: Cy_USB_DisableUsbBlock
     ******************************************************************************
     * Summary:
     *  Function to disable the USB32DEV IP block after terminating current
     *  connection.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_USB_DisableUsbBlock(void);

    /*****************************************************************************
     * Function Name: Cy_USB_EnableUsbBlock
     ******************************************************************************
     * Summary:
     *  Function to enable the USB32DEV IP block before enabling a new USB
     *  connection.
     *
     * Parameters:
     *  None
     *
     * Return:
     *  None
     *****************************************************************************/
    void Cy_USB_EnableUsbBlock(void);

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
    void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

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
    void Cy_FailHandler(void);

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
    void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)(void));

    /*****************************************************************************
     * Function Name: Cy_App_PrintStartupLogs
     ******************************************************************************
     * Summary:
     * Startup debug messages
     *
     * Parameters:
     * \param sensorType
     * image sensor connected to the hardware
     *
     * Return:
     * void
     *****************************************************************************/
    void Cy_App_PrintStartupLogs(cy_en_sensorType_t sensorType);

    /*****************************************************************************
     * Function Name: Cy_App_PrintBuildConfigs
     ******************************************************************************
     * Summary:
     * prints build configuration parameters
     *
     * Parameters:
     * \param sensorType
     * image sensor connected to the hardware
     *
     * Return:
     * void
     *****************************************************************************/
    void Cy_App_PrintBuildConfigs(cy_en_sensorType_t sensorType);

    /***********************************************************************************************
    * Function Name: Cy_App_PrintBuildConfigInErrState
    **************************************************************************************************
    * Summary:
    * Debug prints of compiled/build configuration
    *
    * Parameters:
    * \param sensorType
    * sensorType: image sensor connected to the hardware
    *
    * Return:
    * void
    ************************************************************************************************/
    void Cy_App_PrintBuildConfigInErrState(cy_en_sensorType_t sensorType);

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
    void Cy_U3V_AppTaskHandler(void *pTaskParam);

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
    void Cy_USB_AppGpifEventCb(void *pApp);

    /*****************************************************************************
     * Function Name: Cy_U3V_CommandChannel_ISR
     *****************************************************************************
     *
     * Handler for interrupts from the DataWire channel used to receive U3V commands
     *
     * Parameters:
     *  None
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_U3V_CommandChannel_ISR(void);

    /*****************************************************************************
     * Function Name: Cy_U3V_ResponseChannel_ISR
     *****************************************************************************
     *
     * Handler for interrupts from the DataWire channel used to send U3V responses
     *
     * Parameters:
     *  None
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_U3V_ResponseChannel_ISR(void);

    /*****************************************************************************
     * Function Name: Cy_U3V_StreamChannel_ISR
     *****************************************************************************
     * Summary
     * Handler for interrupts from the DataWire channel used to send U3V video stream
     *
     * Parameters:
     *  None
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_U3V_StreamChannel_ISR(void);

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
    void Cy_U3V_AppStreamAcqStart(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt);

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
    void Cy_U3V_AppStreamAcqStop(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt);

    /***********************************************************************************************
     * Function Name: Cy_USB_PrintTaskHandler
     **************************************************************************************************
     * Summary:
     * Debug prints task handler
     *
     * Parameters:
     * \param pTaskParam
     * task param
     *
     * Return:
     * void
     ************************************************************************************************/
    void Cy_USB_PrintTaskHandler(void *pTaskParam);

    /*****************************************************************************
     * Function Name: Cy_USB_AppPrintUsbEventLog
     ******************************************************************************
     * Summary:
     *  Function to print out the USB event log buffer content.
     *
     * Parameters:
     *  \param pAppCtxt
     *  Pointer to application context data structure.
     *  \param pSSCal
     *  Pointer to SSCAL context data structure.
     *
     * Return:
     *  void
     *****************************************************************************/
    void Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */
