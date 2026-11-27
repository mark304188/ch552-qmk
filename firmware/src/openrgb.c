/*==========================================================================*
 * openrgb.c - OpenRGB QMK protocol, revision D
 *
 * OpenRGB's QMK driver uses a 64-byte raw HID report and the usage page
 * 0xFF60 / usage 0x61 interface.  It cannot share the interface with VIA
 * (both use command IDs 0x01-0x09), so the OpenRGB build excludes via.c.
 *
 * The firmware exposes one zone with WS2812_LED_COUNT LEDs.  Direct mode
 * is implemented; OpenRGB streams per-LED RGB values to it.  Onboard effect
 * IDs are not advertised because this firmware has its own small effect
 * set, and OpenRGB's effect name table would not match it.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "openrgb.h"
#include "usb.h"
#include "keyboard.h"
#include "rgb.h"
#include "keyboard_config.h"

#if !RGB_ENABLE || (WS2812_LED_COUNT == 0)
#error "OpenRGB support requires RGB_ENABLE=1 and WS2812_LED_COUNT>0"
#endif

#define OR_GET_PROTOCOL_VERSION       1
#define OR_GET_QMK_VERSION            2
#define OR_GET_DEVICE_INFO            3
#define OR_GET_MODE_INFO              4
#define OR_GET_LED_INFO               5
#define OR_GET_ENABLED_MODES          6
#define OR_SET_MODE                   7
#define OR_DIRECT_SET_SINGLE          8
#define OR_DIRECT_SET_LEDS            9

#define OR_FAILURE                    25
#define OR_SUCCESS                    50
#define OR_END_OF_MESSAGE             100

#define OR_PROTOCOL_VERSION           0x0D
#define OR_LED_FLAG_KEYLIGHT          0x04

static void zero_buffer(uint8_t *data)
{
    uint8_t i;

    for (i = 0; i < RAW_HID_SIZE; i++) {
        data[i] = 0;
    }
}

static void send_reply(uint8_t *data, uint8_t command, uint8_t status)
{
    data[0] = command;
    data[RAW_HID_SIZE - 2] = status;
    data[RAW_HID_SIZE - 1] = OR_END_OF_MESSAGE;
    usb_raw_send(data, RAW_HID_SIZE);
}

static void led_position(uint8_t led, uint8_t *x, uint8_t *y)
{
    uint8_t row;
    uint8_t col;

    *x = 0;
    *y = 0;
    if (led >= KEY_COUNT) {
        return;
    }

    row = (uint8_t)(led / MATRIX_COLS);
    col = (uint8_t)(led % MATRIX_COLS);
#if MATRIX_COLS > 1
    *x = (uint8_t)((uint16_t)col * 224 / (MATRIX_COLS - 1));
#else
    *x = 0;
#endif
#if MATRIX_ROWS > 1
    *y = (uint8_t)((uint16_t)row * 224 / (MATRIX_ROWS - 1));
#else
    *y = 0;
#endif
}

static uint8_t led_keycode(uint8_t led)
{
    uint16_t kc;

    if (led < KEY_COUNT) {
        kc = keyboard_get_keycode(0, (uint8_t)(led / MATRIX_COLS),
                                  (uint8_t)(led % MATRIX_COLS));
        if (kc > 0 && kc <= 0x00FF) {
            return (uint8_t)kc;
        }
    }

    /* OpenRGB builds its LED name list only when this byte is non-zero.
     * Layer keys and unmapped positions have a 16-bit QMK keycode (or
     * KC_NO); give those LEDs a harmless digit name so the LED count in
     * OpenRGB still matches WS2812_LED_COUNT. */
    return (uint8_t)(KC_1 + (led % 9));
}

static void device_info(uint8_t *data)
{
    uint8_t pos = 3;
    uint8_t i;

    zero_buffer(data);
    data[1] = WS2812_LED_COUNT;
    data[2] = MATRIX_ROWS * MATRIX_COLS;

    for (i = 0; USB_PRODUCT[i] != 0 && pos < ((RAW_HID_SIZE - 2) / 2); i++) {
        data[pos++] = (uint8_t)USB_PRODUCT[i];
    }
    data[pos++] = 0;

    for (i = 0; USB_MANUFACTURER[i] != 0 && pos < (RAW_HID_SIZE - 2); i++) {
        data[pos++] = (uint8_t)USB_MANUFACTURER[i];
    }
}

static void led_info(uint8_t *data, uint8_t first, uint8_t count)
{
    uint8_t i;
    uint8_t base;

    zero_buffer(data);

    for (i = 0; i < count; i++) {
        uint8_t led = (uint8_t)(first + i);
        uint8_t x;
        uint8_t y;
        uint8_t r;
        uint8_t g;
        uint8_t b;

        base = (uint8_t)(1 + i * 7);
        if ((uint8_t)(base + 6) >= RAW_HID_SIZE) {
            break;
        }

        if (led >= WS2812_LED_COUNT) {
            data[base + 2] = OR_FAILURE;
            continue;
        }

        led_position(led, &x, &y);
        rgb_get_direct_led(led, &r, &g, &b);

        data[base + 0] = x;
        data[base + 1] = y;
        data[base + 2] = OR_LED_FLAG_KEYLIGHT;
        data[base + 3] = r;
        data[base + 4] = g;
        data[base + 5] = b;
        data[base + 6] = led_keycode(led);
    }
}

void openrgb_handle(uint8_t *data, uint8_t length)
{
    uint8_t command = data[0];
    uint8_t a = data[1];
    uint8_t b = data[2];

    (void)length;

    switch (command) {
    case OR_GET_PROTOCOL_VERSION:
        zero_buffer(data);
        data[1] = OR_PROTOCOL_VERSION;
        send_reply(data, OR_GET_PROTOCOL_VERSION, 0);
        break;

    case OR_GET_QMK_VERSION: {
        static __code const char version[] = "CH552-1.4";
        uint8_t i;

        zero_buffer(data);
        for (i = 0; version[i] != 0 && (uint8_t)(i + 1) < (RAW_HID_SIZE - 2); i++) {
            data[1 + i] = (uint8_t)version[i];
        }
        send_reply(data, OR_GET_QMK_VERSION, 0);
        break;
    }

    case OR_GET_DEVICE_INFO:
        device_info(data);
        send_reply(data, OR_GET_DEVICE_INFO, 0);
        break;

    case OR_GET_MODE_INFO:
        zero_buffer(data);
        data[1] = rgb_get_effect(); /* 0 while in OpenRGB direct mode */
        data[2] = rgb_get_speed();
        data[3] = rgb_get_hue();
        data[4] = rgb_get_sat();
        data[5] = rgb_get_brightness();
        send_reply(data, OR_GET_MODE_INFO, 0);
        break;

    case OR_GET_LED_INFO:
        led_info(data, a, b);
        send_reply(data, OR_GET_LED_INFO, 0);
        break;

    case OR_GET_ENABLED_MODES:
        zero_buffer(data);
        data[1] = 1; /* direct mode only */
        send_reply(data, OR_GET_ENABLED_MODES, 0);
        break;

    case OR_SET_MODE: {
        uint8_t h = data[1];
        uint8_t s = data[2];
        uint8_t v = data[3];
        uint8_t mode = data[4];
        uint8_t speed = data[5];

        rgb_set_color(h, s);
        rgb_set_brightness(v);
        rgb_set_speed(speed);

        if (mode <= 1) {
            rgb_set_direct_mode(1);
        } else {
            rgb_set_direct_mode(0);
            rgb_set_effect(mode);
        }

        zero_buffer(data);
        send_reply(data, OR_SET_MODE, OR_SUCCESS);
        break;
    }

    case OR_DIRECT_SET_SINGLE:
        rgb_set_direct_led(a, data[2], data[3], data[4]);
        zero_buffer(data);
        send_reply(data, OR_DIRECT_SET_SINGLE, OR_SUCCESS);
        break;

    case OR_DIRECT_SET_LEDS: {
        uint8_t count = data[1];
        uint8_t i;

        for (i = 0; i < count; i++) {
            uint8_t off = (uint8_t)(2 + i * 4);
            if ((uint8_t)(off + 3) >= RAW_HID_SIZE) {
                break;
            }
            rgb_set_direct_led(data[off], data[off + 1],
                               data[off + 2], data[off + 3]);
        }
        /* OpenRGB does not wait for a reply to this fire-and-forget
         * command. */
        break;
    }

    default:
        break;
    }
}
