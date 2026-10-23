/*==========================================================================*
 * usb.h - CH552 USB HID device (keyboard + VIA raw HID)
 *==========================================================================*/

#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>
#include "ch554_usb.h"
#include "keyboard_config.h"

#define EP0_SIZE        64

/* Endpoint assignment:
 *   EP1 IN  0x81  keyboard / consumer reports
 *   EP2 IN  0x82  VIA raw HID input
 *   EP3 OUT 0x03  VIA raw HID output
 */
#define KBD_EP_IN       0x81
#define RAW_EP_IN       0x82
#define RAW_EP_OUT      0x03

#define RAW_HID_SIZE    32
#define KBD_REPORT_SIZE 9   /* report ID + 8 bytes */

void usb_init(void);

/* Keyboard report transport.  Returns 0 if the previous report is still
 * in flight; the caller retries. */
uint8_t usb_kbd_busy(void);
uint8_t usb_kbd_send(const uint8_t *report, uint8_t len);

/* Raw HID transport.  usb_raw_send() may be called from the main loop;
 * if the IN endpoint is busy the data is queued (one pending reply). */
void usb_raw_send(const uint8_t *data, uint8_t len);
void usb_raw_task(void);

/* Raw HID OUT handshake.  The ISR stores the received 32-byte report;
 * main consumes it and then calls usb_raw_release(). */
uint8_t usb_raw_out_ready(void);
uint8_t *usb_raw_out_data(void);
void usb_raw_release(void);

/* Keyboard boot protocol state from SET_PROTOCOL.  In boot protocol the
 * 8-byte report is sent without a report ID and consumer reports are
 * suppressed. */
uint8_t usb_boot_protocol(void);

#endif /* __USB_H__ */
