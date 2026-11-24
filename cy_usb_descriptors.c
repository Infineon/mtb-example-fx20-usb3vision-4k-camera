/***************************************************************************/
/*
 * \file cy_usb_descriptors.c
 * \version 1.0
 *
 * Defines USB descriptors used by the FX10 firmware application.
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

#include "cy_usb_descriptors.h"

/* Standard device descriptor for full speed (FS) */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_DeviceFS[32] __attribute__((aligned(32)));
static const uint8_t DeviceFS[] =
    {
        0x12,                          /* Descriptor size */
        0x01,                          /* Device descriptor type */
        0x00, 0x02,                    /* USB 2.1 */
        0x00,                          /* Device class: Defined in interface */
        0x00,                          /* Device Sub-class:  Defined in interface */
        0x00,                          /* Device protocol: Defined in interface */
        0x40,                          /* Maxpacket size for EP0 */
        CY_USB_GET_LSB(CY_FX_USB_VID), /* Vendor ID */
        CY_USB_GET_MSB(CY_FX_USB_VID),
        CY_USB_GET_LSB(CY_FX_USB_PID), /* Product ID */
        CY_USB_GET_MSB(CY_FX_USB_PID),
        0x00, 0x00,          /* Device release number */
        STR_INDEX_VENDOR,    /* Manufacture string index */
        STR_INDEX_MODEL,     /* Product string index */
        STR_INDEX_SERIAL_NO, /* Serial number string index */
        0x01                 /* Number of configurations */
};

/* Standard device descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_DeviceHS[32] __attribute__((aligned(32)));
static const uint8_t DeviceHS[] =
    {
        0x12,                          /* Descriptor size */
        CY_USB_DSCR_TYPE_DEVICE,       /* Device descriptor type */
        0x00, 0x02,                    /* USB 2.00 (Avoiding LPM-L1 support at present). */
        0xEF,                          /* Device class */
        0x02,                          /* Device sub-class */
        0x01,                          /* Device protocol */
        0x40,                          /* Maxpacket size for EP0 : 64 bytes */
        CY_USB_GET_LSB(CY_FX_USB_VID), /* Vendor ID */
        CY_USB_GET_MSB(CY_FX_USB_VID),
        CY_USB_GET_LSB(CY_FX_USB_PID), /* Product ID */
        CY_USB_GET_MSB(CY_FX_USB_PID),
        0x00, 0x00,          /* Device release number */
        STR_INDEX_VENDOR,    /* Manufacture string index */
        STR_INDEX_MODEL,     /* Product string index */
        STR_INDEX_SERIAL_NO, /* Serial number string index */
        0x01                 /* Number of configurations */
};

/* Standard device descriptor for USB 3.2 */
/* R-3cd:  Multi-interface Function Device Class Codes to indicate IAD*/
/* R-4cd:
Specified fields of Device Descriptor

Field                   Specified Value
bDeviceClass            Constant value EFh. (Miscellaneous Device Class)
bDeviceSubClass         Constant value 02h. (Common Class)
bDeviceProtocol         Constant value 01h. (Interface Association Descriptor)
*/
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_DeviceSS[32] __attribute__((aligned(32)));
static const uint8_t DeviceSS[] =
    {
        0x12,                          /* Descriptor size */
        CY_USB_DSCR_TYPE_DEVICE,       /* Device descriptor type */
        0x20, 0x03,                    /* USB 3.2*/
        0xEF,                          /* Device class */
        0x02,                          /* Device Sub-class */
        0x01,                          /* Device protocol */
        0x09,                          /* Maxpacket size for EP0 : 2^9 */
        CY_USB_GET_LSB(CY_FX_USB_VID), /* Vendor ID */
        CY_USB_GET_MSB(CY_FX_USB_VID),
        CY_USB_GET_LSB(CY_FX_USB_PID), /* Product ID */
        CY_USB_GET_MSB(CY_FX_USB_PID),
        0x00, 0x00,          /* Device release number */
        STR_INDEX_VENDOR,    /* Manufacture string index */
        STR_INDEX_MODEL,     /* Product string index */
        STR_INDEX_SERIAL_NO, /* Serial number string index */
        0x01                 /* Number of configurations */
};

/* Binary device object store descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_BOS[64] __attribute__((aligned(32)));
static const uint8_t BOS[] =
    {
        0x05,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_BOS, /* Device descriptor type */
        0x16, 0x00,           /* Length of this descriptor and all sub descriptors */
        0x02,                 /* Number of device capability descriptors */

        /* USB 2.0 Extension */
        0x07,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_DEVICE_CAP, /* Device capability type descriptor */
        CY_USB_USB2_EXTN_CAPB_TYPE,  /* USB 2.0 extension capability type */
        0x1E, 0x64, 0x00, 0x00,      /* Supported device level features: LPM support, BESL supported,
                                        Baseline BESL=400 us, Deep BESL=1000 us. */

        /* SuperSpeed Device Capability */
        0x0A,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_DEVICE_CAP, /* Device capability type descriptor */
        CY_USB_SS_CAPB_TYPE,         /* SuperSpeed device capability type */
        0x00,                        /* Supported device level features  */
        0x0E, 0x00,                  /* Speeds supported by the device : SS, HS */
        0x03,                        /* Functionality support */
        0x0A,                        /* U1 device exit latency */
        0xFF, 0x07                   /* U2 device exit latency */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeedPlus connection. */
USB3_DESC_ATTRIBUTES uint8_t BOS_Gen2[] =
    {
        0x05,       /* Descriptor size */
        0x0F,       /* BOS descriptor. */
        0x2D, 0x00, /* Length of this descriptor and all sub-descriptors */
        0x04,       /* Number of device capability descriptors */

        /* USB 2.0 extension */
        0x07,                   /* Descriptor size */
        0x10,                   /* Device capability type descriptor */
        0x02,                   /* USB 2.0 extension capability type */
        0x1E, 0x64, 0x00, 0x00, /* Supported device level features: LPM support, BESL supported,
                                   Baseline BESL=400 us, Deep BESL=1000 us. */

        /* SuperSpeed device capability */
        0x0A,       /* Descriptor size */
        0x10,       /* Device capability type descriptor */
        0x03,       /* SuperSpeed device capability type */
        0x00,       /* Supported device level features: Not LTM capable.  */
        0x0E, 0x00, /* Speeds supported by the device : SS Gen1, HS and FS */
        0x03,       /* Functionality support */
        0x0A,       /* U1 Device Exit latency */
        0xFF, 0x07, /* U2 Device Exit latency */

        /* SuperSpeedPlus USB device capability */
        0x14,                   /* Descriptor size */
        0x10,                   /* Device capability type descriptor */
        0x0A,                   /* SuperSpeedPlus Device capability */
        0x00,                   /* Reserved */
        0x01, 0x00, 0x00, 0x00, /* SSAC=1, SSIC=0 */
        0x00, 0x11,             /* SSID=0, Min. RX Lane = 1, Min. Tx Lane = 1 */
        0x00, 0x00,             /* Reserved */
        0x30, 0x40, 0x0A, 0x00, /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Rx), LP=1(SSPlus), LSM=10 */
        0xB0, 0x40, 0x0A, 0x00, /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Tx), LP=1(SSPlus), LSM=10 */

        /* PTM device capability*/
        0x03, /* Descriptor size */
        0x10, /* Device capability type descriptor */
        0x0B  /* Precision Time Measurement (PTM) Capability Descriptor */
};

/* Full Speed Configuration Descriptor*/
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_ConfigFS[64] __attribute__((aligned(32)));
static const uint8_t ConfigFS[] =
    {
        /* Configuration descriptor */
        0x09,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_CFG, /* Configuration descriptor type */
        0x12, 0x00,           /* Total length of the descriptor. */
        0x01,                 /* Number of interfaces */
        0x01,                 /* Configuration number */
        0x09,                 /* Configuration string index */
        0x80,                 /* Config characteristics - bus powered */
        0x32,                 /* Max power consumption of device (in 2mA unit) : 100mA */

        /* interface descriptor */
        0x09, /* Descriptor size */
        0x04, /* Interface descriptor type */
        0x00, /* Interface number */
        0x00, /* Alternate setting number */
        0x00, /* Number of end points */
        0xFF, /* Interface class: Vendor defined */
        0x00, /* Interface sub class: Vendor defined */
        0x00, /* Interface protocol: Vendor defined */
        0x00  /* Interface descriptor string index */
};

/* Standard High Speed Configuration Descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_ConfigHS[512] __attribute__((aligned(32)));
static const uint8_t ConfigHS[] =
    {
        /* Configuration descriptor */
        0x09,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_CFG, /* Configuration descriptor type */
        CY_USB_GET_LSB(USBHS_CFG_DSCR_LENGTH),
        CY_USB_GET_MSB(USBHS_CFG_DSCR_LENGTH), /* Total config descriptor length. */
        USBHS_NUM_INTERFACES,                  /* Number of interfaces */
        0x01,                                  /* Configuration number */
        0x08,                                  /* Configuration string index */
        0x80,                                  /* Config characteristics - bus powered */
        0xC8,                                  /* Max power consumption of device (in 2mA unit) : 400mA */

        /* Interface association descriptor */
        0x08,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF_ASSOC, /* Interface association descr type */
        U3V_DEV_DCINTF_INDEX,        /* I/f number of first VideoControl i/f */
        U3V_INTERFACE_NUM,           /* Number of U3V related interfaces. */
        0xEF,                        /* Miscellaneous Class */
        0x05,                        /* USB3 Vision */
        0x00,                        /* Protocol : Not used */
        STR_INDEX_FUNCTION_DEF,      /* String desc index for iFunction */

        /* R-9cd: The Device Control Interface Descriptor values listed in Table 3-3 are mandatory
            Table 3-3: Specified fields of Device Control Interface Descriptor
            Field                   Specified Value
            bInterfaceClass         Constant value EFh (Miscellaneous Class)
            bInterfaceSubClass      Constant value 05h (USB3 Vision).
            bInterfaceProtocol      Constant value 00h (Device Control)
        */
        /* Device control interface descriptor */
        0x09,                   /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF,  /* Interface descriptor type */
        U3V_DEV_DCINTF_INDEX,   /* Interface number [R-10cd: The default Device Control interface MUST be the first IAD interface]*/
        0x00,                   /* Alternate setting number, [R-13cd: MUST NOT have alternate Device Control interfaces] */
        0x02,                   /* Number of end points [R-11cd:  default Device Control interface requires two endpoints]*/
        0xEF,                   /* Miscellaneous class */
        0x05,                   /* USB3 Vision */
        0x00,                   /* Protocol : Device Control */
        STR_INDEX_FUNCTION_DEF, /* String desc index for iFunction */

        /*
            [R-15cd]: A single instance of the Device Info descriptor MUST occur after the default
                    Device Control Interface descriptor.
            [R-16cd]:All strings referenced in the descriptor MUST be able to be represented as 7-bit
                    ASCII strings.
        */
        /* Class specific device info descriptor */
        0x14,                   /* Descriptor Size */
        0x24,                   /* Descriptor type = U3V_INTERFACE */
        0x01,                   /* Descriptor Subtype = U3V_DEVICEINFO */
        0x00, 0x00, 0x01, 0x00, /* GenCP Version */
        0x00, 0x00, 0x01, 0x00, /* U3V Version */
        STR_INDEX_GUID,         /* U3V GUID String index */
        STR_INDEX_VENDOR,       /* Vendor Name String index */
        STR_INDEX_MODEL,        /* Model Name String index */
        STR_INDEX_FAMILY,       /* Family Name String index */
        STR_INDEX_DEV_VER,      /* Device Version String index */
        STR_INDEX_VENDOR,       /* Manufacturer info string index */
        STR_INDEX_SERIAL_NO,    /* Serial Number string index */
        STR_INDEX_MODEL,        /* User Defined Name string index */
        USB_SPEED_SUPPORT,      /* USB Speed Support for U3V- GEN2x2, GEN2, GEN1x2,GEN1x1 and HS supported */

        /* endpoint descriptor : Command */
        0x07,                  /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP, /* Endpoint descriptor type */
        CY_U3V_EP_DCI_CMD,     /* Endpoint address and description */
        CY_USB_EP_BULK,        /* Interrupt end point type */
        0x00, 0x02,            /* Max packet size = 512 bytes */
        0x00,                  /* Servicing interval : NA */

        /* endpoint descriptor : Response */
        0x07,                     /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP,    /* Endpoint descriptor type */
        0x80 | CY_U3V_EP_DCI_RSP, /* Endpoint address and description */
        CY_USB_EP_BULK,           /* Interrupt end point type */
        0x00, 0x02,               /* Max packet size = 512 bytes */
        0x00,                     /* Servicing interval : NA */

        /* Device Streaming Interface Descriptor */
        0x09,                  /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF, /* Interface Descriptor Type */
        U3V_DEV_DSINTF_INDEX,  /* Interface Number */
        0x00,                  /* Alternate Setting */
        0x01,                  /* Number of Endpoints */
        0xEF,                  /* Miscellaneous Class */
        0x05,                  /* USB3 Vision */
        0x02,                  /* Device Streaming */
        0x00,                  /* Interface String index */

        /* Endpoint descriptor for streaming video data */
        0x07,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP,       /* Endpoint descriptor type */
        0x80 | CY_U3V_EP_DSI_STREAM, /* Endpoint address and description */
        CY_USB_EP_BULK,              /* Bulk End Point */
        0x00, 0x02,                  /* Maximum packet size. */
        0x00,                        /* Servicing interval for data transfers */
#if CY_VENDOR_IF_EN
        /* Interface descriptor */
        0x09, /* Descriptor size */
        0x04, /* Interface Descriptor type */
        0x02, /* Interface number */
        0x00, /* Alternate setting number */
        0x00, /* Number of end points */
        0xFF, /* Interface class */
        0x00, /* Interface sub class */
        0x00, /* Interface protocol code */
        0x00  /* Interface descriptor string index */
#endif        /* CY_VENDOR_IF_EN */
};

/* Standard super speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_ConfigSS[1024] __attribute__((aligned(32)));
static const uint8_t ConfigSS[] =
    {
        /* Configuration descriptor */
        0x09,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_CFG, /* Configuration descriptor type */
        CY_USB_GET_LSB(USBSS_CFG_DSCR_LENGTH),
        CY_USB_GET_MSB(USBSS_CFG_DSCR_LENGTH), /* Total config descriptor length. */
        USBSS_NUM_INTERFACES,                  /* Number of interfaces */
        0x01,                                  /* Configuration number */
        0x00,                                  /* Configuration string index */
        0x80,                                  /* Config characteristics - Bus powered */
        0x32,                                  /* Max power consumption of device (in 8mA unit) : 400mA */

        /* R-6cd: IAD immediately before Device control interface */
        /* R-7cd: IAD values listed in Table 3-2 are mandatory
            Table 3-2: Specified fields of Interface Association Descriptor
            Field                   Specified Value
            bFunctionClass          Constant value EFh. (Miscellaneous Class)
            bFunctionSubClass       Constant value 05h (USB3 Vision)
            bFunctionProtocol       Constant value 00h
            iFunction               The index of a string that equals “USB3 Vision Device”
        */
        /* Interface association descriptor */
        0x08,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF_ASSOC, /* Interface association descr type */
        U3V_DEV_DCINTF_INDEX,        /* I/f number of first VideoControl i/f */
        U3V_INTERFACE_NUM,           /* Number of U3V related interfaces. */
        0xEF,                        /* Miscellaneous Class */
        0x05,                        /* USB3 Vision */
        0x00,                        /* Protocol : Not used */
        STR_INDEX_FUNCTION_DEF,      /* String desc index for iFunction */

        /* R-9cd: The Device Control Interface Descriptor values listed in Table 3-3 are mandatory
            Table 3-3: Specified fields of Device Control Interface Descriptor
            Field                   Specified Value
            bInterfaceClass         Constant value EFh (Miscellaneous Class)
            bInterfaceSubClass      Constant value 05h (USB3 Vision).
            bInterfaceProtocol      Constant value 00h (Device Control)
        */
        /* Device control interface descriptor */
        0x09,                   /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF,  /* Interface descriptor type */
        U3V_DEV_DCINTF_INDEX,   /* Interface number [R-10cd: The default Device Control interface MUST be the first IAD interface]*/
        0x00,                   /* Alternate setting number, [R-13cd: MUST NOT have alternate Device Control interfaces] */
        0x02,                   /* Number of end points [R-11cd:  default Device Control interface requires two endpoints]*/
        0xEF,                   /* Miscellaneous class */
        0x05,                   /* USB3 Vision */
        0x00,                   /* Protocol : Device Control */
        STR_INDEX_FUNCTION_DEF, /* String desc index for iFunction */

        /*
            [R-15cd]: A single instance of the Device Info descriptor MUST occur after the default
                    Device Control Interface descriptor.
            [R-16cd]:All strings referenced in the descriptor MUST be able to be represented as 7-bit
                    ASCII strings.
        */
        /* Class specific device info descriptor */
        0x14,                   /* Descriptor Size */
        0x24,                   /* Descriptor type = U3V_INTERFACE */
        0x01,                   /* Descriptor Subtype = U3V_DEVICEINFO */
        0x00, 0x00, 0x01, 0x00, /* GenCP Version */
        0x00, 0x00, 0x01, 0x00, /* U3V Version */
        STR_INDEX_GUID,         /* U3V GUID String index */
        STR_INDEX_VENDOR,       /* Vendor Name String index */
        STR_INDEX_MODEL,        /* Model Name String index */
        STR_INDEX_FAMILY,       /* Family Name String index */
        STR_INDEX_DEV_VER,      /* Device Version String index */
        STR_INDEX_VENDOR,       /* Manufacturer info string index */
        STR_INDEX_SERIAL_NO,    /* Serial Number string index */
        STR_INDEX_MODEL,        /* User Defined Name string index */
        USB_SPEED_SUPPORT,      /* USB Speed Support for U3V- GEN2x2, GEN1x2, GEN2,GEN1 and HS supported */

        /* Device control status endpoint descriptor, [R-27cd]  */
        0x07,                  /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP, /* Endpoint descriptor type */
        CY_U3V_EP_DCI_CMD,     /* Endpoint address and description */
        CY_USB_EP_BULK,        /* Bulk end point type */
        0x00, 0x04,            /* Max packet size = 1024 bytes */
        0x00,                  /* Servicing interval */

        /* Super speed endpoint companion descriptor */
        0x06,                          /* Descriptor size */
        CY_USB_DSCR_TYPE_SS_ENDP_COMP, /* SS endpoint companion descriptor type */
        0x00,                          /* Max no. of packets in a Burst : 1 */
        0x00,                          /* Mult: Max number of packets : 1 */
        0x00, 0x00,                    /* Bytes per interval : N/A */

        /* Video control status endpoint descriptor */
        0x07,                     /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP,    /* Endpoint descriptor type */
        0x80 | CY_U3V_EP_DCI_RSP, /* Endpoint address and description */
        CY_USB_EP_BULK,           /* Bulk end point type */
        0x00, 0x04,               /* Max packet size = 1024 bytes */
        0x00,                     /* Servicing interval */

        /* Super speed endpoint companion descriptor */
        0x06,                          /* Descriptor size */
        CY_USB_DSCR_TYPE_SS_ENDP_COMP, /* SS endpoint companion descriptor type */
        0x00,                          /* Max no. of packets in a Burst : 1 */
        0x00,                          /* Mult: Max number of packets : 1 */
        0x00, 0x00,                    /* Bytes per interval : N/A */

        /* Device Streaming Interface Descriptor */
        0x09,                  /* Descriptor size */
        CY_USB_DSCR_TYPE_INTF, /* Interface Descriptor Type */
        U3V_DEV_DSINTF_INDEX,  /* Interface Number */
        0x00,                  /* Alternate Setting [R-25cd]*/
        0x01,                  /* Number of Endpoints [CR-24cd]*/
        0xEF,                  /* Miscellaneous Class [CR-23cd]*/
        0x05,                  /* USB3 Vision [CR-23cd]*/
        0x02,                  /* Device Streaming [CR-23cd]*/
        0x00,                  /* Interface String index */

        /* Endpoint descriptor for streaming video data */
        0x07,                        /* Descriptor size */
        CY_USB_DSCR_TYPE_ENDP,       /* Endpoint descriptor type */
        0x80 | CY_U3V_EP_DSI_STREAM, /* Endpoint address and description */
        CY_USB_EP_BULK,              /* Bulk End Point */
        0x00, 0x04,                  /* Maximum packet size. */
        0x00,                        /* Servicing interval for data transfers */

        /* Super speed endpoint companion descriptor */
        0x06,                          /* Descriptor size */
        CY_USB_DSCR_TYPE_SS_ENDP_COMP, /* SS endpoint companion descriptor type */
        0x0F,                          /* Max no. of packets in a Burst : 16 */
        0x00,                          /* Mult.: Max number of packets : 1 */
        0x00, 0x00,                    /* Field Valid only for Periodic Endpoints */
#if CY_VENDOR_IF_EN
        /* Interface descriptor */
        0x09,                 /* Descriptor size */
        0x04,                 /* Interface Descriptor type */
        U3V_DEV_VENDOR_INDEX, /* Interface number */
        0x00,                 /* Alternate setting number */
        0x00,                 /* Number of end points */
        0xFF,                 /* Interface class */
        0x00,                 /* Interface sub class */
        0x00,                 /* Interface protocol code */
        0x00,                 /* Interface descriptor string index */
#endif                        /* CY_VENDOR_IF_EN */
};

/* Standard device qualifier descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_DeviceQuality[16] __attribute__((aligned(32)));
static const uint8_t DeviceQuality[] =
    {
        0x0A,                              /* Descriptor size */
        CY_USB_DSCR_TYPE_DEVICE_QUALIFIER, /* Device qualifier descriptor type */
        0x00, 0x02,                        /* USB 2.0 */
        0xEF,                              /* Device class */
        0x02,                              /* Device sub-class */
        0x01,                              /* Device protocol */
        0x40,                              /* Maxpacket size for EP0 : 64 bytes */
        0x01,                              /* Number of configurations */
        0x00                               /* Reserved */
};

/* Standard language ID string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_LanguageID[16] __attribute__((aligned(32)));
static const uint8_t LanguageID[] =
    {
        0x04,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        0x09, 0x04            /* Language ID supported */
};

/* Standard manufacturer string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_VendorNameStr[32] __attribute__((aligned(32)));
static const uint8_t VendorName[] =
    {
        0x12,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        'I', 0x00,
        'n', 0x00,
        'f', 0x00,
        'i', 0x00,
        'n', 0x00,
        'e', 0x00,
        'o', 0x00,
        'n', 0x00};

/* Standard product string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_ModelNameStr[16] __attribute__((aligned(32)));
static const uint8_t ModelName[] =
    {
        0x0A,                 /* Descriptor Size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        'F', 0x00,
        'X', 0x00,
        0x00, 0x00,
        0x00, 0x00};

/* Function string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_Function[64] __attribute__((aligned(32)));
static const uint8_t Function[] =
    {
        0x26,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        'U', 0x00,
        'S', 0x00,
        'B', 0x00,
        '3', 0x00,
        ' ', 0x00,
        'V', 0x00,
        'i', 0x00,
        's', 0x00,
        'i', 0x00,
        'o', 0x00,
        'n', 0x00,
        ' ', 0x00,
        'D', 0x00,
        'e', 0x00,
        'v', 0x00,
        'i', 0x00,
        'c', 0x00,
        'e', 0x00};

/* Standard GUID string descriptor */
/* R-1cd: 04B4 + 8 unique numer ID*/
/* CR-2cd: Add U3V before GUID on device/box*/
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_GUID[32] __attribute__((aligned(32)));
static const uint8_t GUID[] =
    {
        0x1A,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        '0',
        0x00,
        '4',
        0x00,
        'B',
        0x00,
        '4',
        0x00,
};

/* Serial Number string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_SerialNum[32] __attribute__((aligned(32)));
static const uint8_t SerialNum[] =
    {
        0x1A,                 /* Descriptor size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        '0',
        0x00,
        '4',
        0x00,
        'B',
        0x00,
        '4',
        0x00,
};

/* Family Name String Descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_FamilyName[16] __attribute__((aligned(32)));
static const uint8_t FamilyName[] =
    {
        0x0E,
        CY_USB_DSCR_TYPE_STR,
        'E', 0x00,
        'Z', 0x00,
        '-', 0x00,
        'U', 0x00,
        'S', 0x00,
        'B', 0x00};

/* Device Version String Descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_DeviceVer[16] __attribute__((aligned(32)));
static const uint8_t DeviceVer[] =
    {
        0x0C,
        CY_USB_DSCR_TYPE_STR,
        'R', 0x00,
        'E', 0x00,
        'V', 0x00,
        '-', 0x00,
        'C', 0x00};

/* SS Configuration string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_SSStr[16] __attribute__((aligned(32)));
static const uint8_t SSStr[] =
    {
        0x10,                 /* Descriptor Size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        'U', 0x00,            /* Super-Speed Configuration Descriptor */
        'S', 0x00,
        'B', 0x00,
        '-', 0x00,
        '3', 0x00,
        '.', 0x00,
        '2', 0x00};
/*  HS Configuration string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_HSStr[16] __attribute__((aligned(32)));
static const uint8_t HSStr[] =
    {
        0x10,                 /* Descriptor Size */
        CY_USB_DSCR_TYPE_STR, /* Device descriptor type */
        'U', 0x00,            /* High-Speed Configuration Descriptor */
        'S', 0x00,
        'B', 0x00,
        '-', 0x00,
        '2', 0x00,
        '.', 0x00,
        '1', 0x00};

/* FS Not Supported string descriptor */
USB3_DESC_ATTRIBUTES uint8_t Cy_USB_Dscr_FSStr[64] __attribute__((aligned(32)));
static const uint8_t FSStr[] =
    {
        0x22, /* Descriptor Size */
        0x03, /* Device descriptor type */
        'F', 0x00,
        'S', 0x00,
        ' ', 0x00,
        'N', 0x00,
        'O', 0x00,
        'T', 0x00,
        ' ', 0x00,
        'S', 0x00,
        'U', 0x00,
        'P', 0x00,
        'P', 0x00,
        'O', 0x00,
        'R', 0x00,
        'T', 0x00,
        'E', 0x00,
        'D', 0x00};

static uint8_t siliconUniqueId[16] = {0};
static uint8_t siliconName[4] = {0};

/*****************************************************************************
* Function Name: Cy_USB_GetSiliconUniqueId
******************************************************************************
* Summary:
* Read silicon id and being stored in serial number and guid of u3v device, NOT mandatory for any U3V application
*
* Parameters:
* \param void
* None
*
* \return void
*  none.
*****************************************************************************/
void Cy_USB_GetSiliconUniqueId(void)
{
    uint8_t i = 0;
    uint64_t serialNo;
    serialNo = Cy_SysLib_GetUniqueId();
    APP_DBG_TRACE1("Silicon unique ID: 0x%x%x\r\n", ((serialNo >> 32) & 0xFFFFFFFF), ((serialNo) & 0xFFFFFFFF));
    siliconUniqueId[0] = CHARMAP((USB_CONN_TYPE)); /* store compiled/build USB conn speed */
    siliconUniqueId[1] = 0x00;
    i = 0;
    while (i < 28) /* only need 7 pair of characters */
    {
        siliconUniqueId[2 + (i / 2)] = CHARMAP(((serialNo >> (60 - i) & (0xF)))); /* 64 bit wide serial no, shifting 4 bit in each loop */
        siliconUniqueId[3 + (i / 2)] = 0x00;
        i += 4;
    }
    if (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2X2)
    {
        siliconName[0] = '2';
        siliconName[2] = '0';
    }
    else if (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN2)
    {
        siliconName[0] = '1';
        siliconName[2] = '0';
    }
    else if (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1X2)
    {
        siliconName[0] = '5';
        siliconName[2] = 'N';
    }
    else if (USB_CONN_TYPE == CY_USBD_USB_DEV_SS_GEN1)
    {
        siliconName[0] = '5';
        siliconName[2] = ' ';
    }
    else
    {
        siliconName[0] = 'H';
        siliconName[2] = 'S';
    }
}

static void Cy_USB_CopyDscrToHbram(cy_en_usb_speed_t usbSpeed)
{
    memcpy(Cy_USB_Dscr_DeviceSS, DeviceSS, sizeof(DeviceSS));
    memcpy(Cy_USB_Dscr_DeviceHS, DeviceHS, sizeof(DeviceHS));
    memcpy(Cy_USB_Dscr_DeviceFS, DeviceFS, sizeof(DeviceFS));
    memcpy(Cy_USB_Dscr_DeviceQuality, DeviceQuality, sizeof(DeviceQuality));
    memcpy(Cy_USB_Dscr_ConfigSS, ConfigSS, sizeof(ConfigSS));
    memcpy(Cy_USB_Dscr_ConfigHS, ConfigHS, sizeof(ConfigHS));
    memcpy(Cy_USB_Dscr_ConfigFS, ConfigFS, sizeof(ConfigFS));
    memcpy(Cy_USB_Dscr_LanguageID, LanguageID, sizeof(LanguageID));
    memcpy(Cy_USB_Dscr_VendorNameStr, VendorName, sizeof(VendorName));
    memcpy(Cy_USB_Dscr_ModelNameStr, ModelName, sizeof(ModelName));
    memcpy(&Cy_USB_Dscr_ModelNameStr[6], siliconName, sizeof(siliconName));
    memcpy(Cy_USB_Dscr_FSStr, FSStr, sizeof(FSStr));
    memcpy(Cy_USB_Dscr_HSStr, HSStr, sizeof(HSStr));
    memcpy(Cy_USB_Dscr_SSStr, SSStr, sizeof(SSStr));
    memcpy(Cy_USB_Dscr_DeviceVer, DeviceVer, sizeof(DeviceVer));
    memcpy(Cy_USB_Dscr_FamilyName, FamilyName, sizeof(FamilyName));
    memcpy(Cy_USB_Dscr_GUID, GUID, sizeof(GUID));
    memcpy(Cy_USB_Dscr_SerialNum, SerialNum, sizeof(SerialNum));
    memcpy(&Cy_USB_Dscr_GUID[10], siliconUniqueId, sizeof(siliconUniqueId));
    memcpy(&Cy_USB_Dscr_SerialNum[10], siliconUniqueId, sizeof(siliconUniqueId));
    memcpy(Cy_USB_Dscr_Function, Function, sizeof(Function));
    if (usbSpeed >= CY_USBD_USB_DEV_SS_GEN2)
    {
        memcpy(Cy_USB_Dscr_BOS, BOS_Gen2, sizeof(BOS_Gen2));
    }
    else
    {
        memcpy(Cy_USB_Dscr_BOS, BOS, sizeof(BOS));
    }
}

void Cy_USB_RegisterDscr(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_usb_speed_t usbSpeed)
{
    /* Can be moved so that copy is only done once. */
    Cy_USB_GetSiliconUniqueId();
    Cy_USB_CopyDscrToHbram(usbSpeed);
    if ((pAppCtxt != NULL) && (pAppCtxt->pUsbdCtxt != NULL))
    {
        if ((usbSpeed >= CY_USBD_USB_DEV_HS))
        {
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)Cy_USB_Dscr_DeviceHS);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_DEVICE_DSCR, 0, (uint8_t *)Cy_USB_Dscr_DeviceSS);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t *)Cy_USB_Dscr_ConfigFS);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_CONFIG_DSCR, 0, (uint8_t *)Cy_USB_Dscr_ConfigHS);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_CONFIG_DSCR, 0, (uint8_t *)Cy_USB_Dscr_ConfigSS);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_DEVICE_QUAL_DSCR, 0, (uint8_t *)Cy_USB_Dscr_DeviceQuality);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t *)Cy_USB_Dscr_BOS);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0, (uint8_t *)BOS_Gen2);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_LANG_ID, (uint8_t *)Cy_USB_Dscr_LanguageID);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_GUID, (uint8_t *)Cy_USB_Dscr_GUID);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_VENDOR, (uint8_t *)Cy_USB_Dscr_VendorNameStr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_MODEL, (uint8_t *)Cy_USB_Dscr_ModelNameStr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_FAMILY, (uint8_t *)Cy_USB_Dscr_FamilyName);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_DEV_VER, (uint8_t *)Cy_USB_Dscr_DeviceVer);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_SERIAL_NO, (uint8_t *)Cy_USB_Dscr_SerialNum);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_FUNCTION_DEF, (uint8_t *)Cy_USB_Dscr_Function);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_SS_STR, (uint8_t *)Cy_USB_Dscr_SSStr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_HS_STR, (uint8_t *)Cy_USB_Dscr_HSStr);
        }
        else
        {
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)Cy_USB_Dscr_DeviceFS);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t *)Cy_USB_Dscr_ConfigFS);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t *)Cy_USB_Dscr_BOS);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_LANG_ID, (uint8_t *)Cy_USB_Dscr_LanguageID);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, STR_INDEX_FS_STR, (uint8_t *)Cy_USB_Dscr_FSStr);
        }
        APP_DBG_TRACE("USB Descriptor registeration done\r\n");
    }
}
/* [ ] */
