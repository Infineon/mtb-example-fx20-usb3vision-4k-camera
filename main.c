/***************************************************************************//**
* \file main.c
* \version 1.0
*
* Main source file of the FX20 USB Vision class application.
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
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usbss_cal_drv.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_usbd_version.h"
#include "cy_hbdma_version.h"
#include "cy_lvds.h"
#include "cy_fw_version.h"
#include "timers.h"
#include "cy_fx_common.h"
#include "cy_app_fpga.h"
#include "cybsp.h"
#include "cy_app_qspi.h"
#include "cy_app_lvds.h"

static uint32_t g_UsbEvtLogBuf[512u];
static uint16_t gCurUsbEvtLogIndex = 0;
/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t HBW_DrvCtxt;     /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t HBW_DscrList;  /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t HBW_BufMgr;      /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t HBW_MgrCtxt; /* High BandWidth DMA manager context. */
cy_stc_usb_usbd_ctxt_t usbdCtxt;
cy_stc_usb_app_ctxt_t appCtxt;
cy_stc_usbss_cal_ctxt_t ssCalCtxt;
cy_stc_usb_cal_ctxt_t hsCalCtxt;

/* CPU DMA register pointers. */
DMAC_Type *pCpuDmacBase;
DW_Type *pCpuDw0Base;
DW_Type *pCpuDw1Base;

uint32_t hfclkFreq = BCLK__BUS_CLK__HZ;

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

void Cy_SysTickIntrWrapper(void)
{
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}
void vPortSetupTimerInterrupt(void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, Cy_SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(hfclkFreq / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

#if DEBUG_INFRA_EN
/* Debug log related initilization */
/* RAM buffer used to hold debug log data. */
#define LOG_BUF_SIZE (2048U)
uint8_t logBuf[LOG_BUF_SIZE];
TaskHandle_t printLogTaskHandle;

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
void Cy_USB_PrintTaskHandler(void *pTaskParam)
{
    while (1)
    {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/*****************************************************************************
 * Function Name: Cy_PrintVersionInfo
 ******************************************************************************
 * Summary:
 *  Function to print version information to UART console.
 *
 * Parameters:
 *  \param type: Type of version string.
 *  \param version: Version number including major, minor, patch and build number.
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_PrintVersionInfo(const char *type, uint32_t version)
{
    uint16_t vBuild;
    uint8_t vMajor;
    uint8_t vMinor;
    uint8_t vPatch;

    if(NULL == type) { return; }

    uint8_t typeLen = strlen(type);
    char tString[20+typeLen]; /* This avoids buffer overflow when concatenating the version string */

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    APP_DBG_INFO("%s", tString);
}

/***********************************************************************************************
 * Function Name: Cy_App_BasicConfigPrints
 **************************************************************************************************
 * Summary:
 * Debug prints of LVDS/LVCMOS
 *
 * Parameters:
 * \param flag
 * a flag to enable prints of Phy, configured link modes of LVDS/LVCMOS
 *
 * Return:
 * void
 ************************************************************************************************/
static void Cy_App_BasicConfigPrints(bool flag)
{
    APP_DBG_INFO("  MODE & LINK: %s %s\r\n",
                 LVCMOS_EN ? LVCMOS_DDR_EN ? "LVCMOS DDR" : "LVCMOS SDR" : "LVDS",
                 WL_EN ? "WIDE-LINK" : PORT1_EN ? "NARROW-LINK PORT1"
                                                : "NARROW-LINK PORT0");
    APP_DBG_INFO("  PHY_CLK_KHZ: %d\r\n", cy_lvds_lvcmos_config.phyConfig->interfaceClock_kHz);
    APP_DBG_INFO("  GEAR RATIO : GEAR_RATIO_%d_1\r\n", 1 << cy_lvds_lvcmos_config.phyConfig->gearingRatio);
    if (flag)
    {
#if TRAINING_SEQ_REQ
#if (!LVCMOS_EN)
        APP_DBG_INFO("  PHY PATTERN: 0x%x\r\n", PHY_TRAINING_PATTERN_BYTE);
#endif
        APP_DBG_INFO("  LNK PATTERN: 0x%x\r\n", LINK_TRAINING_PATTERN_BYTE);
#if (LVCMOS_DDR_EN)
        APP_DBG_INFO("  DDR MDLL PH: %d\r\n", CY_LVDS_DDR_MDLL_PHASE_VAL);
#endif
#endif /* TRAINING_SEQ_REQ */
    }
#if FPGA_ENABLE    
    APP_DBG_FPGA("  FPGA_CFG   : %s\r\n",glFpgaConfigModeString[glFpgaConfigMode]);
#endif /* FPGA_ENABLE */                                        
}

/***********************************************************************************************
 * Function Name: Cy_App_NamePrints
 **************************************************************************************************
 * Summary:
 * Debug prints of Application name
 *
 * Parameters:
 * \param flag
 * a flag to enable prints of LVDS/LVCMOS  mode config
 *
 * Return:
 * void
 ************************************************************************************************/
void Cy_App_NamePrints(bool flag)
{
    APP_DBG_INFO("\r\n\r\n-------------------------------------------\r\n");
    const char* connectionType = USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2X2 ? "20" 
                                    : USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2 ? "10"
                                    : USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1X2 ? "5N"
                                    : USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1 ? "5"
                                    : "HS";
    APP_DBG_MISC("\r\nU3V FW EXAMPLE: EZ-USB FX %s\r\n\r\n", connectionType);
    if (flag)
    {
        Cy_App_BasicConfigPrints(false);
    }
    APP_DBG_INFO("\r\n-------------------------------------------\r\n\r\n");
}

/***********************************************************************************************
 * Function Name: Cy_App_PrintBuildConfigs
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
void Cy_App_PrintBuildConfigs(cy_en_sensorType_t sensorType)
{
    Cy_App_BasicConfigPrints(true);
    APP_DBG_INFO("  THRD_INTLVD: %s\r\n", INTERLEAVE_EN ? "ENABLED" : "DISABLED");

    APP_DBG_INFO("  IMG_INTRFCE: %s_4K\r\n", glDataSourceName[sensorType]);

#if (FPGA_ADDS_HEADER)
#if (DBG_HEADER_BY_FPGA)
    APP_DBG_INFO("  U3V_HEADER : BY FPGA w/MANUAL DMA\r\n");
#else
    APP_DBG_INFO("  U3V_HEADER : BY FPGA w/AUTO DMA\r\n");
#endif /* DBG_HEADER_BY_FPGA */
#elif INMD_EN  && (!LVCMOS_EN)
    APP_DBG_INFO("  U3V_HEADER : BY INMD w/AUTO DMA\r\n");
#else
    APP_DBG_INFO("  U3V_HEADER : BY FX DEVICE w/MANUAL DMA\r\n");
#endif /* FPGA_ADDS_HEADER */

#if (!BLOAD_ENABLE)
    APP_DBG_INFO("  BOOTLOADER : DISABLED\r\n");
#endif /* BLOAD_ENABLE */
#if (RGB24)
    APP_DBG_INFO("  PXL_FORMAT : RGB24\r\n");
#elif (RGB32)
    APP_DBG_INFO("  PXL_FORMAT : RGBA32\r\n");
#else
    APP_DBG_INFO("  PXL_FORMAT : YUV2\r\n");
#endif /* (RGB24) */
    cy_en_usb_speed_t currSpeed = appCtxt.devSpeed;
    APP_DBG_INFO("  FX CONN USB: ");
    APP_DBG_MISC("%s\r\n",
                 currSpeed == 6 ? "20 Gbps" : currSpeed == 5 ? "10 Gbps"
                                          : currSpeed == 4   ? "5*2 Gbps"
                                          : currSpeed == 3   ? "5 Gbps"
                                                             : "480 Mbps");
    APP_DBG_INFO("-------------------------------------------\r\n");
}

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
void Cy_App_PrintBuildConfigInErrState(cy_en_sensorType_t sensorType)
{
    APP_DBG_INFO("  MODE & LINK: %s %s\r\n",
                 LVCMOS_EN ? LVCMOS_DDR_EN ? "LVCMOS DDR" : "LVCMOS SDR" : "LVDS",
                 WL_EN ? "WIDE-LINK" : PORT1_EN ? "NARROW-LINK PORT1"
                                                : "NARROW-LINK PORT0");
    APP_DBG_INFO("  IMG_INTRFCE: %s\r\n", glDataSourceName[sensorType]);
#if (RGB24)
    APP_DBG_INFO("  PXL_FORMAT : RGB24\r\n");
#elif (RGB32)
    APP_DBG_INFO("  PXL_FORMAT : RGBA32\r\n");
#else
    APP_DBG_INFO("  PXL_FORMAT : YUV2\r\n");
#endif /* (RGB24) */
    APP_DBG_INFO("-------------------------------------------\r\n");
}

/*****************************************************************************
 * Function Name: Cy_App_PrintStartupLogs
 ******************************************************************************
 * Summary:
 * GPIF error handler
 *
 * Parameters:
 * \param sensorType
 * image sensor connected to the hardware
 *
 * Return:
 * void
 *****************************************************************************/
void Cy_App_PrintStartupLogs(cy_en_sensorType_t sensorType)
{
    Cy_App_NamePrints(0);
    Cy_PrintVersionInfo("\r\n  APP_VERSION:   ", FW_VERSION_NUM);
    Cy_PrintVersionInfo("  U3V_LIB_VER:   ", Cy_U3V_GetlibVersion());
    Cy_PrintVersionInfo("  USBD_VERSION:  ", USBD_VERSION_NUM);
    Cy_PrintVersionInfo("  HBDMA_VERSION: ", HBDMA_VERSION_NUM);
    APP_DBG_INFO("-------------------------------------------\r\n");
    vTaskDelay(pdMS_TO_TICKS(100)); /* Give time to Print Task Handler to empty log buffer */
    Cy_App_PrintBuildConfigs(sensorType);
#if FPGA_ENABLE
    bool fpgsConfigFlag = Cy_FPGA_IsConfigurationCompleted();
    if (fpgsConfigFlag)
    {
        Cy_FPGA_GetRtlRevision(FPGASLAVE_ADDR, &appCtxt.fpgaVersion);
        APP_DBG_INFO("-------------------------------------------\r\n");
        APP_DBG_INFO("  FPGA FW VER: [ %x.%x ]\r\n", CY_USB_GET_MSB(appCtxt.fpgaVersion), CY_USB_GET_LSB(appCtxt.fpgaVersion));
        APP_DBG_INFO("-------------------------------------------\r\n\r\n");
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(100)); /* Give time to Print Task Handler to empty log buffer */
}

/***********************************************************************************************
 * Function Name: Cy_App_InitDebugPrints
 **************************************************************************************************
 * Summary:
 * Debug prints when application execution starts
 *
 * Parameters:
 * \param void
 * none
 *
 * Return:
 * void
 ************************************************************************************************/
void Cy_App_InitDebugPrints(void)
{
    cy_stc_debug_config_t dbgCfg;

    dbgCfg.pBuffer = (uint8_t *)logBuf;
    dbgCfg.traceLvl = DBG_LEVEL;
    dbgCfg.bufSize = LOG_BUF_SIZE;
    dbgCfg.printNow = true; /* Delay output of messages until logging task runs. */
    dbgCfg.dbgIntfce = CY_DEBUG_INTFCE_USBFS_CDC;
    Cy_Debug_LogInit(&dbgCfg);
}
#endif /* DEBUG_INFRA_EN */

#if PTM_ENABLE
/* RTOS SW timer specific stuff */
#define AUTO_RELOAD_TIMER_PERIOD (pdMS_TO_TICKS(2000UL))
TimerHandle_t xAutoReloadTimer;
BaseType_t xTimerStarted;
/*****************************************************************************
* Function Name: ITP_EnableCB
******************************************************************************
* Summary:
*  Callback function for software timer
*
* Parameters:
*  \param xTimer

* Return:
*  Does not return.
*****************************************************************************/

void ITP_EnableCB(TimerHandle_t xTimer)
{
    Cy_USBSS_Cal_PTMConfig(&ssCalCtxt, true); /* Enable LDM*/
}

/*****************************************************************************
* Function Name: Software timer initialization
******************************************************************************
* Summary:
*  Does some init for running rtos based auto reload software timer
*  which periodically enables ITP interrupt for LDM exchange mechanism
*
* Parameters:
*  \param void

* Return:
*  Does not return.
*****************************************************************************/
void Cy_SoftwareTimerInit(void)
{
    APP_DBG_INFO("Initializing software timer (PTM): %d ms interval\r\n", AUTO_RELOAD_TIMER_PERIOD);
    xAutoReloadTimer = xTimerCreate("AutoReload",
                                    AUTO_RELOAD_TIMER_PERIOD,
                                    pdTRUE,
                                    0,
                                    ITP_EnableCB);

    xTimerStarted = xTimerStart(xAutoReloadTimer, 0);

    /* Configure to select 14 bits from Bus interval counter
       for both links (Widelink)
    */
    LVDSSS_LVDS->SCRSS_VALUE_CFG[0] = 0x0AUL;
    LVDSSS_LVDS->SCRSS_VALUE_CFG[1] = 0x0AUL;
}
#endif /* PTM_ENABLE */

/*******************************************************************************
 * Function name: Cy_Fx3g2_InitPeripheralClocks
 ****************************************************************************/
/**
 *
 * Function used to enable clocks to different peripherals on the FX10/FX20 device.
 *
 * \param adcClkEnable
 * Whether to enable clock to the ADC in the USBSS block.
 *
 * \param usbfsClkEnable
 * Whether to enable bus reset detect clock input to the USBFS block.
 *
 *******************************************************************************/
static void Cy_Fx3g2_InitPeripheralClocks(
    bool adcClkEnable,
    bool usbfsClkEnable)
{
    if (adcClkEnable)
    {
        /* Divide PERI clock at 75 MHz by 75 to get 1 MHz clock using 16-bit divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, 74);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR, CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable)
    {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
}

/*****************************************************************************
 * Function Name: Cy_USB_SS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-SS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_SS_ISR(void)
{
    /* Call the USB32DEV interrupt handler. */
    Cy_USBSS_Cal_IntrHandler(&ssCalCtxt);
}

/*****************************************************************************
 * Function Name: Cy_USB_HS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-HS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_HS_ISR(void)
{
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
}

/*****************************************************************************
 * Function Name: Cy_USB_IngressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Ingress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_IngressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_IN);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_USB_EgressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Egress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EgressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_EG);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_U3V_CommandChannel_ISR
 *****************************************************************************
 * Summary
 * Handler for interrupts from the DataWire channel used to receive U3V commands
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_U3V_CommandChannel_ISR(void)
{
    /* Pass the interrupt to the DMA manager. */
    Cy_HBDma_Mgr_HandleDW0Interrupt(&HBW_MgrCtxt);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_U3V_ResponseChannel_ISR
 *****************************************************************************
 * Summary
 * Handler for interrupts from the DataWire channel used to send U3V responses
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_U3V_ResponseChannel_ISR(void)
{
    /* Pass the interrupt to the DMA manager. */
    Cy_HBDma_Mgr_HandleDW1Interrupt(&HBW_MgrCtxt);
    portYIELD_FROM_ISR(true);
}

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
void Cy_U3V_StreamChannel_ISR(void)
{
    /* Pass the interrupt to the DMA manager. */
    Cy_HBDma_Mgr_HandleDW1Interrupt(&HBW_MgrCtxt);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_VbusDetGpio_ISR
 *****************************************************************************
 * Summary
 *  Interrupt handler for the Vbus detect GPIO transition detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_VbusDetGpio_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_U3V_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.appMsgQ, &(xMsg), &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
}

/*****************************************************************************
 * Function Name: Cy_VbusDetGpio_Init
 *****************************************************************************
 * Summary
 *  Interrupt handler for the Vbus detect GPIO transition detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_VbusDetGpio_Init(void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    pinCfg.intEdge = CY_GPIO_INTR_BOTH;
    pinCfg.intMask = 0x01UL;
    Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.cm0pSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrSrc = NvicMux5_IRQn;
    intrCfg.intrPriority = 3;
#endif
    Cy_SysInt_Init(&intrCfg, Cy_VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/*****************************************************************************
 * Function Name: Cy_InitSCBs
 *****************************************************************************
 * Summary
 *  SCB initialization for application.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_InitSCBs(void)
{
    Cy_USB_I2CInit();
}

/*****************************************************************************
 * Function Name: Cy_USB_DevInit
 *****************************************************************************
 * Summary
 *  Register ISR for USB SS and HS , LVDS.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_USB_DevInit(void)
{
    cy_stc_sysint_t intrCfg;

    /* Register the LVDS ISR and enable the interrupt for LVDS. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_int_o_IRQn;
    intrCfg.intrPriority = 6;
#else
    intrCfg.intrSrc = NvicMux0_IRQn;
    intrCfg.intrPriority = 2;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if (PORT1_EN || WL_EN)
    /* Register the ISR and enable the interrupt for SIP1 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port1Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
#endif /* (PORT1_EN || WL_EN) */

#if ((!PORT1_EN) || WL_EN)
    /* Register the ISR and enable the interrupt for SIP0 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port0Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
#endif

    /* Register the USBSS ISR and enable the interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Ingress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux3_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_IngressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Egress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux2_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_EgressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrSrc = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for LVDS LPM */
    intrCfg.intrSrc = lvds2usb32ss_lvds_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_LPM_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_wakeup_int_o_IRQn);
}

/*
 * Notes on DMA Buffer RAM Usage:
 * 1. The initial part of the buffer RAM is reserved for the descriptors used by the DMA manager.
 *    The space used for this is reserved using the gHbDmaDescriptorSpace array which is placed
 *    in a section named ".hbDmaDescriptor". This array should have a minimum size of 8192 (8 KB)
 *    and has a default size allocation of 16384 bytes (16 KB). No other data should be placed
 *    in this section.
 *
 * 2. The descriptor region is followed by RW data structures which are placed in the ".descSection".
 *    Only data members placed in this section will be initialized during the firmware load
 *    process.
 *
 * 3. The ".descSection" is followed by the ".hbBufSection" which will hold data structures
 *    which do not need to be explicitly initialized (equivalent of ".bss" section).
 *
 * 4. This is followed by the ".hbDmaBufferHeap" section which will be used to allocate all
 *    the DMA buffers from. The gHbDmaBufferHeap array represents the memory region which will
 *    be given to the DMA buffer manager to allocate buffers from and can be sized based on the
 *    available memory. No other data or variables should be placed in this section.
 *
 * Any pre-initialized data which is to be placed in the High BandWidth Buffer RAM should be
 * added to the ".descSection". Any non-initialized data which is to be placed in the High
 * BandWidth Buffer RAM should be added to the ".hbBufSection".
 */

/* Region of 16 KB reserved for High BandWidth DMA descriptors. */
static __attribute__ ((section(".hbDmaDescriptor"), used)) uint32_t gHbDmaDescriptorSpace[16384 / 4];

/* Region of 896 KB reserved for DMA buffer heap. */
/*
 * Note: Since this application requires a large amount of buffer RAM, it is only supported on parts
 * that support 1 MB of DMA buffer RAM.
 */
static __attribute__ ((section(".hbDmaBufferHeap"), used)) uint32_t gHbDmaBufferHeap[896 * 1024 / 4];

/*****************************************************************************
 * Function Name: Cy_InitHbDma
 *****************************************************************************
 * Summary
 *  Initialize HBDMA block
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static bool Cy_InitHBDMA(void)
{
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Verify that gHbDmaDescriptorSpace is located at the base of the DMA buffer SRAM. */
    if ((uint32_t)gHbDmaDescriptorSpace != CY_HBW_SRAM_BASE_ADDR) {
        DBG_APP_ERR("High BandWidth DMA descriptors not placed at the correct address\r\n");
        return false;
    }
    /* Setup a HBW DMA descriptor list using the space reserved in gHbDmaDescriptorSpace. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, sizeof(gHbDmaDescriptorSpace) / 16);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager to use the gHbDmaBufferHeap region. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)gHbDmaBufferHeap, sizeof(gHbDmaBufferHeap));
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }
    /* Both LVDS DMA adapters are used only for ingress transfers. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);

    /* Register the USB stack context with the HBW DMA manager. */
    Cy_HBDma_Mgr_RegisterUsbContext(&HBW_MgrCtxt, &usbdCtxt);
    
    /* Specify that DMA clock should be set to 240 MHz once USB 3.x connection is active. */
    Cy_USBD_SetDmaClkFreq(&usbdCtxt, CY_HBDMA_CLK_240_MHZ);
    
    /* Enable stall cycles between back-to-back AHB accesses to high bandwidth RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;

    return true;
}

/*****************************************************************************
 * Function Name: Cy_USBSS_DeInit
 ******************************************************************************
 * Summary:
 *  Function to de initialize USBSS device
 *
 * Parameters:
 *  \param pCalCtxt
 * USB CAL layer context pointer
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt)
{
    USB32DEV_Type *base = pCalCtxt->regBase;
    USB32DEV_MAIN_Type *USB32DEV_MAIN = &base->USB32DEV_MAIN;

    /* Disable the clock for USB3.2 function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_CLK_EN_Msk;

    /* Disable PHYSS */
    base->USB32DEV_PHYSS.USB40PHY[0].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    base->USB32DEV_PHYSS.USB40PHY[1].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
          USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    /* Disable the SuperSpeed Device function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_SSDEV_ENABLE_Msk;
}

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
void Cy_USB_DisableUsbBlock(void)
{
    /* Disable the USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL &= ~USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Disable HBDMA adapter interrupts and the adapter itself. */
    NVIC_DisableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_DisableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);
    Cy_HBDma_DeInit(&HBW_DrvCtxt);

    APP_DBG_TRACE("Disabled HBWSS DMA adapters\r\n");
}

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
void Cy_USB_EnableUsbBlock(void)
{
    /* Enable the USB DMA adapters and respective interrupts. */
    Cy_HBDma_Init(NULL, USB32DEV, &HBW_DrvCtxt, 0, 0);

    NVIC_EnableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_EnableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);

    /* Make sure to enable USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;
}

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionEnable
 *****************************************************************************
 * Summary
 *  USB connect function.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 ****************************************************************************/
bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, USB_CONN_TYPE);
    pAppCtxt->usbConnectDone = true;
    APP_DBG_INFO("Desired USB Speed: %d\r\n", USB_CONN_TYPE);
    return true;
}

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionDisable
 *****************************************************************************
 * Summary
 *  USB disconnect function.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    Cy_USBSS_DeInit(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    pAppCtxt->usbConnectDone = false;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
}

/*****************************************************************************
* Function Name: main(void)
******************************************************************************
* Summary:
*  Entry to the application.
*
* Parameters:
*  void

* Return:
*  Does not return.
*****************************************************************************/
int main(void)
{
    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init (&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device clocks to the desired values. */
    cybsp_init();
    
    Cy_Fx3g2_InitPeripheralClocks(true, true);
    hfclkFreq = Cy_SysClk_ClkFastGetFrequency();

    /*
     * Unlock and then disable the watchdog.
     * This is required to stop resetting the device in every 4.3 seconds.
     * If not disabled then watchdog needs to be cleared in application code
     */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

#if CY_CPU_CORTEX_M4
    __set_BASEPRI(0);
#endif /* CY_CPU_CORTEX_M4 */

    /* Enable interrupts. */
    __enable_irq();

    /* Initialize contexts with Null*/
    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&ssCalCtxt, 0, sizeof(ssCalCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    /* Initialize CPU DMA pointer with base address*/
    pCpuDmacBase = ((DMAC_Type *)DMAC_BASE);
    pCpuDw0Base = ((DW_Type *)DW0_BASE);
    pCpuDw1Base = ((DW_Type *)DW1_BASE);

    /* Store IP base address in CAL context. */
    ssCalCtxt.regBase = USB32DEV;
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

#if DEBUG_INFRA_EN
    Cy_App_InitDebugPrints();
#endif /* DEBUG_INFRA_EN */

    /* Add print to display Application name over log messages*/
    Cy_App_NamePrints(true);

    APP_DBG_INFO("Clock and PDL init done\n\r");

    Cy_InitSCBs();

    /*
     * Make sure any previous USB connection state is cleared. Give some delay to allow the host to process
     * disconnection.
     */
    Cy_USBSS_DeInit(&ssCalCtxt);
    Cy_SysLib_Delay(500);

    /* Initialize GPIO used for Vbus line status read*/
    Cy_VbusDetGpio_Init();

    /* Initialize register ISR for USB and LVDSS IP blocks. */
    Cy_USB_DevInit();

    /* Initialize the HbDma IP and DMA Manager */
    Cy_InitHBDMA();
    APP_DBG_INFO("HBDMA Init Done\n\r");

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt, &ssCalCtxt, &HBW_MgrCtxt);
    APP_DBG_INFO("USBD Init Done\n\r");
    /*
     * Make sure InitEventLog called before AppInit or before creating
     * thread for application.
     * Allocate a memory block and register it as USB event log buffer.
     */
    appCtxt.pUsbEvtLogBuf = (uint32_t *)g_UsbEvtLogBuf;
    Cy_USBD_InitEventLog(&usbdCtxt, appCtxt.pUsbEvtLogBuf, 512u);

    /* Initialize the application context and EPs */
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base, &HBW_MgrCtxt);

    /* Create FreeRTOS tasks for user application */
    Cy_USB_AppTaskCreate(&appCtxt);

    /* Create FreeRTOS Message Queue for data transaction between tasks/ISR*/
    Cy_USB_AppQueueCreate(&appCtxt);

    /* Create FreeRTOS Timers for periodic call of application specific handlers*/
    Cy_USB_AppTimerCreate(&appCtxt);

    /* Register USB events callbacks*/
    Cy_USB_AppRegisterCallback(&appCtxt);

    /* Memory allocation in HBDMA RAM*/
    Cy_USB_AppHbdmaBufAllocation(&HBW_MgrCtxt);

    /* Register USB descriptors with the stack. */
    Cy_USB_RegisterDscr(&appCtxt, CY_USBD_USB_DEV_SS_GEN1); /* speed shaould be HS or greater than for U3V */

#if PTM_ENABLE
    Cy_SoftwareTimerInit();
#endif /* PTM_ENABLE */

    /* Invokes scheduler: Not expected to return. */
    vTaskStartScheduler();
    while (1)
    {
    };
    return 0;
}

/*****************************************************************************
 * Function Name: Cy_OnResetUser
 ******************************************************************************
 * Summary:
 *  Init function which is executed before the load regions in RAM are updated.
 *  The High BandWidth subsystem needs to be enable here to allow variables
 *  placed in the High BandWidth SRAM to be updated.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}

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
void Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal)
{
    uint16_t prevLogIdx = gCurUsbEvtLogIndex;

    (void)pSSCal;

    /* Print out any pending USB event log data. */
    gCurUsbEvtLogIndex = Cy_USBD_GetEvtLogIndex(pAppCtxt->pUsbdCtxt);
    while (gCurUsbEvtLogIndex != prevLogIdx)
    {
        APP_DBG_INFO("USBEVT: %x\r\n", pAppCtxt->pUsbEvtLogBuf[prevLogIdx]);
        prevLogIdx++;
        if (prevLogIdx == 512u)
        {
            prevLogIdx = 0u;
        }
    }
}
/* [] END OF FILE */
