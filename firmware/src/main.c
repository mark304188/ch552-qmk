/*==========================================================================*
 * main.c - CH552 QMK/VIA-compatible keyboard
 *
 * Main loop:
 *   - 1 ms key scan + debounce
 *   - send keyboard / consumer reports when they change
 *   - service VIA raw HID reports
 *   - refresh WS2812 effects
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "keyboard_config.h"
#include "debug.h"
#include "usb.h"
#include "keyboard.h"
#include "via.h"
#include "rgb.h"

/* SDCC emits the interrupt vector table in the module that defines main(),
 * and it needs the prototypes here. */
void usb_isr(void) __interrupt(INT_NO_USB) __using(1);
void timer0_isr(void) __interrupt(INT_NO_TMR0) __using(2);

static void send_reports(void) __reentrant
{
#if CONSUMER_ENABLE
    static uint8_t last_consumer[3] = {2, 0, 0};
#endif
    uint8_t report[KBD_REPORT_SIZE];
    uint8_t len;

    if (keyboard_report_dirty() && !usb_kbd_busy()) {
        len = keyboard_build_report(report, usb_boot_protocol());
        if (usb_kbd_send(report, len)) {
            keyboard_clear_report_dirty();
        }
    }

#if CONSUMER_ENABLE
    if (!usb_boot_protocol() && !usb_kbd_busy()) {
        uint8_t consumer[3];
        uint8_t i;
        uint8_t changed = 0;

        keyboard_build_consumer(consumer);
        for (i = 0; i < 3; i++) {
            if (consumer[i] != last_consumer[i]) {
                changed = 1;
            }
        }

        if (changed) {
            if (usb_kbd_send(consumer, 3)) {
                for (i = 0; i < 3; i++) {
                    last_consumer[i] = consumer[i];
                }
            }
        }
    }
#endif
}

void main(void)
{
    uint16_t last_scan_ms;

    CfgFsys();
    mDelaymS(5); /* oscillator settle */

    keyboard_init();
    timer_init();
    rgb_init();
    usb_init();
    EA = 1;

    last_scan_ms = timer_ms();

    while (1) {
        uint16_t now;

        /* Raw HID: a received 32-byte VIA report and a queued reply. */
        if (usb_raw_out_ready()) {
            via_handle(usb_raw_out_data(), RAW_HID_SIZE);
            usb_raw_release();
        }
        usb_raw_task();

        now = timer_ms();
        if (now != last_scan_ms) {
            last_scan_ms = now;
            keyboard_task();
        }

        send_reports();
        rgb_task();
    }
}
