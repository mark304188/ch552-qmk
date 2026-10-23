/*==========================================================================*
 * usb.c - CH552 USB full-speed composite HID device
 *
 * Interfaces:
 *   0: HID keyboard (report ID 1) + consumer control (report ID 2),
 *      interrupt IN endpoint 0x81
 *   1: HID raw vendor channel (usage page 0xFF60, usage 0x61),
 *      interrupt IN 0x82 / interrupt OUT 0x03, 32-byte reports
 *
 * The raw HID interface is the transport used by VIA/QMK.
 *
 * xRAM map:
 *   0x0000 ep0_buf[64]
 *   0x0040 ep1_buf[64]
 *   0x0080 ep2_buf[64]
 *   0x00C0 ep3_buf[64]
 *   0x0100 ... compiler xdata (Makefile sets --xram-loc 0x0100)
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "ch554_usb.h"
#include "usb.h"
#include "keyboard_config.h"

/*---------------------------------------------------------------*
 * Descriptors                                                   *
 *---------------------------------------------------------------*/

#define CFG_TOTAL_LEN 66
#if CONSUMER_ENABLE
#define KBD_REPORT_LEN 90
#define KBD_EP_SIZE    9
#else
#define KBD_REPORT_LEN 63
#define KBD_EP_SIZE    8
#endif
#define RAW_REPORT_LEN 34

static __code uint8_t dev_desc[] = {
    0x12, 0x01,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    EP0_SIZE,
    (uint8_t)(USB_VENDOR_ID & 0xFF), (uint8_t)(USB_VENDOR_ID >> 8),
    (uint8_t)(USB_PRODUCT_ID & 0xFF), (uint8_t)(USB_PRODUCT_ID >> 8),
    (uint8_t)(USB_DEVICE_VERSION & 0xFF), (uint8_t)(USB_DEVICE_VERSION >> 8),
    0x01, 0x02, 0x03,
    0x01
};

static __code uint8_t cfg_desc[] = {
    /* configuration */
    0x09, 0x02,
    CFG_TOTAL_LEN, 0x00,
    0x02,             /* bNumInterfaces */
    0x01,             /* bConfigurationValue */
    0x00,             /* iConfiguration */
    0x80,             /* bus powered */
    0x32,             /* 100 mA */

    /* interface 0: keyboard */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    /* HID descriptor for interface 0 */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    KBD_REPORT_LEN, 0x00,
    /* keyboard interrupt IN */
    0x07, 0x05, 0x81, 0x03, KBD_EP_SIZE, 0x00, 0x01,

    /* interface 1: VIA raw HID */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    /* HID descriptor for interface 1 */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    RAW_REPORT_LEN, 0x00,
    /* raw HID interrupt IN / OUT */
    0x07, 0x05, 0x82, 0x03, 0x20, 0x00, 0x01,
    0x07, 0x05, 0x03, 0x03, 0x20, 0x00, 0x01
};

/* Keyboard report descriptor: report ID 1 = 8-byte boot-style keyboard
 * report, report ID 2 = 16-bit consumer usage. */
static __code uint8_t kbd_report_desc[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x06,        /* Usage (Keyboard) */
    0xA1, 0x01,        /* Collection (Application) */
#if CONSUMER_ENABLE
    0x85, 0x01,        /*   Report ID 1 */
#endif
    0x05, 0x07,        /*   Usage Page (Keyboard) */
    0x19, 0xE0,        /*   Usage Minimum 0xE0 */
    0x29, 0xE7,        /*   Usage Maximum 0xE7 */
    0x15, 0x00,        /*   Logical Minimum 0 */
    0x25, 0x01,        /*   Logical Maximum 1 */
    0x75, 0x01,        /*   Report Size 1 */
    0x95, 0x08,        /*   Report Count 8 */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    0x75, 0x08,        /*   Report Size 8 */
    0x95, 0x01,        /*   Report Count 1 */
    0x81, 0x01,        /*   Input (Const) */
    0x05, 0x07,        /*   Usage Page (Keyboard) */
    0x19, 0x00,        /*   Usage Minimum 0 */
    0x29, 0x65,        /*   Usage Maximum 0x65 */
    0x15, 0x00,        /*   Logical Minimum 0 */
    0x25, 0x65,        /*   Logical Maximum 0x65 */
    0x75, 0x08,        /*   Report Size 8 */
    0x95, 0x06,        /*   Report Count 6 */
    0x81, 0x00,        /*   Input (Data,Array,Abs) */
    /* Standard keyboard LED output report (Caps/Num/Scroll/Compose/Kana).
     * The firmware accepts and ignores it, but its presence makes the
     * descriptor a proper boot keyboard for BIOS and Windows. */
    0x05, 0x08,        /*   Usage Page (LEDs) */
    0x19, 0x01,        /*   Usage Minimum 1 */
    0x29, 0x05,        /*   Usage Maximum 5 */
    0x95, 0x05,        /*   Report Count 5 */
    0x75, 0x01,        /*   Report Size 1 */
    0x91, 0x02,        /*   Output (Data,Var,Abs) */
    0x95, 0x01,        /*   Report Count 1 */
    0x75, 0x03,        /*   Report Size 3 */
    0x91, 0x01,        /*   Output (Const) */
    0xC0,              /* End Collection */

#if CONSUMER_ENABLE
    0x85, 0x02,        /* Report ID 2 */
    0x05, 0x0C,        /* Usage Page (Consumer) */
    0x09, 0x01,        /* Usage (Consumer Control) */
    0xA1, 0x01,        /* Collection (Application) */
    0x19, 0x00,        /*   Usage Minimum 0 */
    0x2A, 0xFF, 0x03,  /*   Usage Maximum 0x03FF */
    0x15, 0x00,        /*   Logical Minimum 0 */
    0x26, 0xFF, 0x03,  /*   Logical Maximum 0x03FF */
    0x75, 0x10,        /*   Report Size 16 */
    0x95, 0x01,        /*   Report Count 1 */
    0x81, 0x00,        /*   Input (Data,Array,Abs) */
    0xC0               /* End Collection */
#endif
};

static __code uint8_t raw_report_desc[] = {
    0x06, 0x60, 0xFF,  /* Usage Page (Vendor 0xFF60) */
    0x09, 0x61,        /* Usage (0x61) */
    0xA1, 0x01,        /* Collection (Application) */
    0x09, 0x62,        /*   Usage (0x62) */
    0x15, 0x00,        /*   Logical Minimum 0 */
    0x26, 0xFF, 0x00,  /*   Logical Maximum 255 */
    0x75, 0x08,        /*   Report Size 8 */
    0x95, 0x20,        /*   Report Count 32 */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    0x09, 0x63,        /*   Usage (0x63) */
    0x15, 0x00,        /*   Logical Minimum 0 */
    0x26, 0xFF, 0x00,  /*   Logical Maximum 255 */
    0x75, 0x08,        /*   Report Size 8 */
    0x95, 0x20,        /*   Report Count 32 */
    0x91, 0x02,        /*   Output (Data,Var,Abs) */
    0xC0               /* End Collection */
};

static __code uint8_t kbd_hid_desc[] = {
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(sizeof(kbd_report_desc) & 0xFF),
    (uint8_t)(sizeof(kbd_report_desc) >> 8)
};

static __code uint8_t raw_hid_desc[] = {
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(sizeof(raw_report_desc) & 0xFF),
    (uint8_t)(sizeof(raw_report_desc) >> 8)
};

static __code uint8_t lang_desc[] = {0x04, 0x03, 0x09, 0x04};
static __code uint8_t manu_desc[] = {
    0x0C, 0x03, 'C', 0, 'H', 0, '5', 0, '5', 0, '2', 0
};
#ifdef BOARD_4KEY
static __code uint8_t prod_desc[] = {
    0x1E, 0x03,
    'C', 0, 'H', 0, '5', 0, '5', 0, '2', 0,
    ' ', 0, 'Q', 0, 'M', 0, 'K', 0, ' ', 0,
    '4', 0, 'K', 0, 'e', 0, 'y', 0
};
#else
static __code uint8_t prod_desc[] = {
    0x1C, 0x03,
    'C', 0, 'H', 0, '5', 0, '5', 0, '2', 0,
    ' ', 0, 'Q', 0, 'M', 0, 'K', 0, ' ', 0,
    'P', 0, 'a', 0, 'd', 0
};
#endif
static __code uint8_t seri_desc[] = {
    0x0A, 0x03, '0', 0, '0', 0, '0', 0, '1', 0
};

/* Compile-time consistency checks.  A negative array size is an error. */
typedef char cfg_len_check[(sizeof(cfg_desc) == CFG_TOTAL_LEN) ? 1 : -1];
typedef char kbd_len_check[(sizeof(kbd_report_desc) == KBD_REPORT_LEN) ? 1 : -1];
typedef char raw_len_check[(sizeof(raw_report_desc) == RAW_REPORT_LEN) ? 1 : -1];

/*---------------------------------------------------------------*
 * Buffers                                                       *
 *---------------------------------------------------------------*/

static uint8_t __xdata __at(0x0000) ep0_buf[64];
static uint8_t __xdata __at(0x0040) ep1_buf[64];
static uint8_t __xdata __at(0x0080) ep2_buf[64];
static uint8_t __xdata __at(0x00C0) ep3_buf[64];

/*---------------------------------------------------------------*
 * State                                                         *
 *---------------------------------------------------------------*/

static volatile __xdata uint8_t kbd_busy;
static volatile __xdata uint8_t raw_in_busy;
static volatile __xdata uint8_t raw_out_ready_flag;
static volatile __xdata uint8_t raw_out_length;
static __xdata uint8_t raw_pending_length;
static __xdata uint8_t raw_pending[RAW_HID_SIZE];

static __xdata uint8_t dev_config;
static __xdata uint8_t dev_addr_pending;
static __xdata uint8_t kbd_boot_protocol;

static __xdata uint8_t ep0_req;
static __xdata uint8_t ep0_rem;
static __code const uint8_t *ep0_rom_src;

#define UsbSetupBuf ((USB_SETUP_REQ __xdata *)ep0_buf)

/* HID class requests. */
#define HID_GET_REPORT   0x01
#define HID_GET_IDLE     0x02
#define HID_GET_PROTOCOL 0x03
#define HID_SET_REPORT   0x09
#define HID_SET_IDLE     0x0A
#define HID_SET_PROTOCOL 0x0B

/*---------------------------------------------------------------*
 * Helpers                                                       *
 *---------------------------------------------------------------*/

static void ep0_send(uint8_t len)
{
    UEP0_T_LEN = len;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
}

static void ep0_stall(void)
{
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
}

/* Arm a descriptor / short reply.  `len` is the total descriptor length;
 * ep0_rem has already been clamped to it. */
static void ep0_start_rom(uint8_t len)
{
    uint8_t chunk;
    uint8_t i;

    if (ep0_rem > len) {
        ep0_rem = len;
    }
    chunk = (ep0_rem >= EP0_SIZE) ? EP0_SIZE : ep0_rem;

    for (i = 0; i < chunk; i++) {
        ep0_buf[i] = ep0_rom_src[i];
    }
    ep0_rom_src += chunk;
    ep0_rem = (uint8_t)(ep0_rem - chunk);

    ep0_send(chunk);
}

/*---------------------------------------------------------------*
 * Endpoint reset / configuration                                *
 *---------------------------------------------------------------*/

static void usb_endpoints_reset(void)
{
    kbd_busy = 0;
    raw_in_busy = 0;
    raw_pending_length = 0;
    raw_out_ready_flag = 0;
    raw_out_length = 0;

    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
    UEP3_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
}

/*---------------------------------------------------------------*
 * Endpoint 0                                                    *
 *---------------------------------------------------------------*/

static void ep0_setup(void)
{
    uint8_t req;
    uint8_t type;
    uint8_t len = 0;

    if (USB_RX_LEN != 8) {
        ep0_stall();
        return;
    }

    req = UsbSetupBuf->bRequest;
    ep0_req = req;
    ep0_rem = UsbSetupBuf->wLengthH ? 0xFF : UsbSetupBuf->wLengthL;
    type = (uint8_t)(UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK);

    if (type == USB_REQ_TYP_CLASS) {
        switch (req) {
        case HID_GET_REPORT:
            ep0_buf[0] = 0;
            len = 1;
            break;
        case HID_GET_IDLE:
            ep0_buf[0] = 0;
            len = 1;
            break;
        case HID_GET_PROTOCOL:
            ep0_buf[0] = kbd_boot_protocol ? 1 : 0;
            len = 1;
            break;
        case HID_SET_REPORT:
            /* Keyboard LED report; accepted and ignored. */
            break;
        case HID_SET_IDLE:
            break;
        case HID_SET_PROTOCOL:
            if (UsbSetupBuf->wIndexL == 0) {
                kbd_boot_protocol = (UsbSetupBuf->wValueL == 0) ? 1 : 0;
            }
            break;
        default:
            ep0_stall();
            return;
        }

        if (ep0_rem > len) {
            ep0_rem = len;
        }
        ep0_send((ep0_rem >= EP0_SIZE) ? EP0_SIZE : ep0_rem);
        return;
    }

    if (type != USB_REQ_TYP_STANDARD) {
        ep0_stall();
        return;
    }

    switch (req) {
    case USB_GET_DESCRIPTOR:
        switch (UsbSetupBuf->wValueH) {
        case 0x01:
            ep0_rom_src = dev_desc;
            len = sizeof(dev_desc);
            break;
        case 0x02:
            ep0_rom_src = cfg_desc;
            len = sizeof(cfg_desc);
            break;
        case 0x03:
            switch (UsbSetupBuf->wValueL) {
            case 0x00:
                ep0_rom_src = lang_desc;
                len = sizeof(lang_desc);
                break;
            case 0x01:
                ep0_rom_src = manu_desc;
                len = sizeof(manu_desc);
                break;
            case 0x02:
                ep0_rom_src = prod_desc;
                len = sizeof(prod_desc);
                break;
            case 0x03:
                ep0_rom_src = seri_desc;
                len = sizeof(seri_desc);
                break;
            default:
                ep0_stall();
                return;
            }
            break;
        case 0x21: /* HID descriptor */
            if (UsbSetupBuf->wIndexL == 0) {
                ep0_rom_src = kbd_hid_desc;
                len = sizeof(kbd_hid_desc);
            } else if (UsbSetupBuf->wIndexL == 1) {
                ep0_rom_src = raw_hid_desc;
                len = sizeof(raw_hid_desc);
            } else {
                ep0_stall();
                return;
            }
            break;
        case 0x22: /* HID report descriptor */
            if (UsbSetupBuf->wIndexL == 0) {
                ep0_rom_src = kbd_report_desc;
                len = sizeof(kbd_report_desc);
            } else if (UsbSetupBuf->wIndexL == 1) {
                ep0_rom_src = raw_report_desc;
                len = sizeof(raw_report_desc);
            } else {
                ep0_stall();
                return;
            }
            break;
        default:
            ep0_stall();
            return;
        }
        ep0_start_rom(len);
        return;

    case USB_SET_ADDRESS:
        dev_addr_pending = UsbSetupBuf->wValueL;
        ep0_send(0);
        return;

    case USB_SET_CONFIGURATION:
        dev_config = UsbSetupBuf->wValueL;
        usb_endpoints_reset();
        ep0_send(0);
        return;

    case USB_GET_CONFIGURATION:
        ep0_buf[0] = dev_config;
        ep0_rem = 1;
        ep0_send(1);
        return;

    case USB_SET_INTERFACE:
        if (UsbSetupBuf->wIndexL < 2) {
            ep0_send(0);
        } else {
            ep0_stall();
        }
        return;

    case USB_GET_INTERFACE:
        ep0_buf[0] = 0;
        ep0_send(1);
        return;

    case USB_GET_STATUS:
        ep0_buf[0] = 0;
        ep0_buf[1] = 0;
        ep0_send((ep0_rem >= 2) ? 2 : ep0_rem);
        return;

    case USB_CLEAR_FEATURE:
    case USB_SET_FEATURE:
        /* No endpoint halt support; acknowledge. */
        ep0_send(0);
        return;

    default:
        ep0_stall();
        return;
    }
}

static void ep0_in_done(void)
{
    if (ep0_req == USB_GET_DESCRIPTOR && ep0_rem) {
        uint8_t chunk;
        uint8_t i;

        chunk = (ep0_rem >= EP0_SIZE) ? EP0_SIZE : ep0_rem;
        for (i = 0; i < chunk; i++) {
            ep0_buf[i] = ep0_rom_src[i];
        }
        ep0_rom_src += chunk;
        ep0_rem = (uint8_t)(ep0_rem - chunk);

        UEP0_T_LEN = chunk;
        UEP0_CTRL = UEP0_CTRL ^ bUEP_T_TOG;
        return;
    }

    if (ep0_req == USB_SET_ADDRESS) {
        USB_DEV_AD = (uint8_t)((USB_DEV_AD & bUDA_GP_BIT) | dev_addr_pending);
    }

    UEP0_T_LEN = 0;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
}

static void ep0_out_done(void)
{
    /* A control write (for example HID SET_REPORT) has an OUT data stage
     * followed by a zero-length IN status stage.  Arm that ZLP here;
     * clearing T_RES to NAK would leave the host waiting forever. */
    UEP0_T_LEN = 0;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
}

/*---------------------------------------------------------------*
 * USB interrupt                                                 *
 *---------------------------------------------------------------*/

void usb_isr(void) __interrupt(INT_NO_USB) __using(1)
{
    uint8_t token;

    if (UIF_TRANSFER) {
        token = (uint8_t)(USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP));

        switch (token) {
        case (UIS_TOKEN_SETUP | 0):
            ep0_setup();
            break;

        case (UIS_TOKEN_IN | 0):
            ep0_in_done();
            break;

        case (UIS_TOKEN_OUT | 0):
            ep0_out_done();
            break;

        case (UIS_TOKEN_IN | 1):
            kbd_busy = 0;
            UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
            break;

        case (UIS_TOKEN_IN | 2):
            raw_in_busy = 0;
            UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
            break;

        case (UIS_TOKEN_OUT | 3):
            if (U_TOG_OK) {
                raw_out_length = USB_RX_LEN;
                raw_out_ready_flag = 1;
                /* NAK until the main loop has consumed the report. */
                UEP3_CTRL = (UEP3_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_NAK;
            }
            break;

        default:
            break;
        }

        UIF_TRANSFER = 0;
    }

    if (UIF_BUS_RST) {
        dev_config = 0;
        dev_addr_pending = 0;
        kbd_boot_protocol = 0;
        usb_endpoints_reset();

        UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP0_T_LEN = 0;
        USB_DEV_AD = 0x00;

        UIF_SUSPEND = 0;
        UIF_TRANSFER = 0;
        UIF_BUS_RST = 0;
    }

    if (UIF_SUSPEND) {
        UIF_SUSPEND = 0;
    }
}

/*---------------------------------------------------------------*
 * Public endpoint API                                           *
 *---------------------------------------------------------------*/

uint8_t usb_kbd_busy(void)
{
    return kbd_busy;
}

uint8_t usb_kbd_send(const uint8_t *report, uint8_t len)
{
    uint8_t i;

    if (kbd_busy || !dev_config) {
        return 0;
    }
    if (len > 64) {
        len = 64;
    }

    for (i = 0; i < len; i++) {
        ep1_buf[i] = report[i];
    }

    kbd_busy = 1;
    UEP1_T_LEN = len;
    UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    return 1;
}

static void raw_start(const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (len > RAW_HID_SIZE) {
        len = RAW_HID_SIZE;
    }
    for (i = 0; i < len; i++) {
        ep2_buf[i] = data[i];
    }

    raw_in_busy = 1;
    UEP2_T_LEN = len;
    UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

void usb_raw_send(const uint8_t *data, uint8_t len)
{
    if (len > RAW_HID_SIZE) {
        len = RAW_HID_SIZE;
    }

    if (raw_in_busy) {
        uint8_t i;

        for (i = 0; i < len; i++) {
            raw_pending[i] = data[i];
        }
        raw_pending_length = len;
        return;
    }

    if (!dev_config) {
        return;
    }
    raw_start(data, len);
}

void usb_raw_task(void)
{
    uint8_t len;

    if (raw_in_busy || raw_pending_length == 0 || !dev_config) {
        return;
    }

    len = raw_pending_length;
    raw_pending_length = 0;
    raw_start(raw_pending, len);
}

uint8_t usb_raw_out_ready(void)
{
    return raw_out_ready_flag;
}

uint8_t *usb_raw_out_data(void)
{
    return ep3_buf;
}

void usb_raw_release(void)
{
    raw_out_ready_flag = 0;
    raw_out_length = 0;
    UEP3_CTRL = (UEP3_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
}

uint8_t usb_boot_protocol(void)
{
    return kbd_boot_protocol;
}

/*---------------------------------------------------------------*
 * Init                                                          *
 *---------------------------------------------------------------*/

void usb_init(void)
{
    kbd_busy = 0;
    raw_in_busy = 0;
    raw_out_ready_flag = 0;
    raw_out_length = 0;
    raw_pending_length = 0;
    dev_config = 0;
    dev_addr_pending = 0;
    kbd_boot_protocol = 0;

    PIN_FUNC |= bUSB_IO_EN;

    USB_CTRL = 0x00;
    USB_CTRL &= ~bUC_HOST_MODE;
    USB_CTRL |= bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;
    USB_DEV_AD = 0x00;

    USB_CTRL &= ~bUC_LOW_SPEED;
    UDEV_CTRL = bUD_PD_DIS;
    UDEV_CTRL |= bUD_PORT_EN;

    UEP0_DMA = (uint16_t)ep0_buf;
    UEP1_DMA = (uint16_t)ep1_buf;
    UEP2_DMA = (uint16_t)ep2_buf;
    UEP3_DMA = (uint16_t)ep3_buf;

    /* EP1 IN only; EP4 disabled.  EP2 IN; EP3 OUT. */
    UEP4_1_MOD = bUEP1_TX_EN;
    UEP2_3_MOD = bUEP2_TX_EN | bUEP3_RX_EN;

    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP0_T_LEN = 0;
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
    UEP1_T_LEN = 0;
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
    UEP2_T_LEN = 0;
    UEP3_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;

    USB_INT_FG = 0xFF;
    USB_INT_EN = bUIE_TRANSFER | bUIE_SUSPEND | bUIE_BUS_RST;
    IE_USB = 1;
    EA = 1;
}
