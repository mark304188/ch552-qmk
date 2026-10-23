/*==========================================================================*
 * via.c - VIA/QMK raw HID protocol
 *
 * Implemented commands:
 *   0x01 protocol version
 *   0x02 keyboard value (uptime, layout, matrix state, versions)
 *   0x03 set keyboard value (layout options, device indication)
 *   0x04/0x05 dynamic keymap get/set keycode
 *   0x06 dynamic keymap reset
 *   0x07/0x08/0x09 custom RGB set/get/save
 *   0x0A EEPROM reset
 *   0x0B jump to WCH bootloader
 *   0x0C/0x0D/0x0E/0x0F/0x10 macro stubs (no macros in this build)
 *   0x11 layer count
 *   0x12/0x13 dynamic keymap buffer get/set
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "via.h"
#include "usb.h"
#include "keyboard.h"
#include "rgb.h"
#include "keycodes.h"
#include "debug.h"

/* VIA command IDs. */
#define id_get_protocol_version                 0x01
#define id_get_keyboard_value                   0x02
#define id_set_keyboard_value                   0x03
#define id_dynamic_keymap_get_keycode           0x04
#define id_dynamic_keymap_set_keycode           0x05
#define id_dynamic_keymap_reset                 0x06
#define id_custom_set_value                     0x07
#define id_custom_get_value                     0x08
#define id_custom_save                          0x09
#define id_eeprom_reset                         0x0A
#define id_bootloader_jump                      0x0B
#define id_dynamic_keymap_macro_get_count       0x0C
#define id_dynamic_keymap_macro_get_buffer_size 0x0D
#define id_dynamic_keymap_macro_get_buffer      0x0E
#define id_dynamic_keymap_macro_set_buffer      0x0F
#define id_dynamic_keymap_macro_reset           0x10
#define id_dynamic_keymap_get_layer_count       0x11
#define id_dynamic_keymap_get_buffer            0x12
#define id_dynamic_keymap_set_buffer            0x13

#define id_unhandled                            0xFF

/* Keyboard value IDs. */
#define id_uptime              0x01
#define id_layout_options      0x02
#define id_switch_matrix_state 0x03
#define id_firmware_version    0x04
#define id_device_indication   0x05
#define id_keycodes_version    0x06

/* Custom channels and values. */
#define id_qmk_backlight_channel  1
#define id_qmk_rgblight_channel   2
#define id_qmk_rgb_matrix_channel 3

#define id_qmk_rgb_brightness   1
#define id_qmk_rgb_effect       2
#define id_qmk_rgb_effect_speed 3
#define id_qmk_rgb_color        4

static void put_u32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void via_custom(uint8_t *data, uint8_t command) __reentrant
{
    uint8_t channel = data[1];
    uint8_t value_id = data[2];

    if (channel != id_qmk_rgblight_channel &&
        channel != id_qmk_rgb_matrix_channel &&
        channel != id_qmk_backlight_channel) {
        data[0] = id_unhandled;
        return;
    }

    if (command == id_custom_set_value) {
        switch (value_id) {
        case id_qmk_rgb_brightness:
            rgb_set_brightness(data[3]);
            break;
        case id_qmk_rgb_effect:
            rgb_set_effect(data[3]);
            break;
        case id_qmk_rgb_effect_speed:
            rgb_set_speed(data[3]);
            break;
        case id_qmk_rgb_color:
            rgb_set_color(data[3], data[4]);
            break;
        default:
            data[0] = id_unhandled;
            break;
        }
        return;
    }

    if (command == id_custom_get_value) {
        switch (value_id) {
        case id_qmk_rgb_brightness:
            data[3] = rgb_get_brightness();
            break;
        case id_qmk_rgb_effect:
            data[3] = rgb_get_effect();
            break;
        case id_qmk_rgb_effect_speed:
            data[3] = rgb_get_speed();
            break;
        case id_qmk_rgb_color:
            data[3] = rgb_get_hue();
            data[4] = rgb_get_sat();
            break;
        default:
            data[0] = id_unhandled;
            break;
        }
        return;
    }

    if (command == id_custom_save) {
        rgb_save();
        return;
    }

    data[0] = id_unhandled;
}

void via_handle(uint8_t *data, uint8_t length) __reentrant
{
    uint8_t command = data[0];
    uint8_t i;

    (void)length;

    switch (command) {
    case id_get_protocol_version:
        data[1] = (uint8_t)(VIA_PROTOCOL_VERSION >> 8);
        data[2] = (uint8_t)(VIA_PROTOCOL_VERSION & 0xFF);
        break;

    case id_get_keyboard_value:
        switch (data[1]) {
        case id_uptime: {
            uint16_t ms = timer_ms();
            put_u32_be(&data[2], (uint32_t)ms);
            break;
        }
        case id_layout_options:
            put_u32_be(&data[2], 0);
            break;
        case id_switch_matrix_state: {
            /* QMK/VIA layout: d[0]=cmd, d[1]=value id, d[2]=offset,
             * then the row bytes start at d[3].  d[2] must be echoed
             * unchanged or VIA rejects the response. */
            uint8_t offset = data[2];
            uint8_t out = 3;

            for (i = offset; i < MATRIX_ROWS && out < 32; i++) {
                data[out++] = keyboard_matrix_row(i);
            }
            break;
        }
        case id_firmware_version:
            put_u32_be(&data[2], 0x00010001UL);
            break;
        case id_keycodes_version:
            put_u32_be(&data[2], 0x00000009UL);
            break;
        default:
            data[0] = id_unhandled;
            break;
        }
        break;

    case id_set_keyboard_value:
        /* layout options and device indication are accepted and ignored */
        if (data[1] != id_layout_options && data[1] != id_device_indication) {
            data[0] = id_unhandled;
        }
        break;

    case id_dynamic_keymap_get_keycode: {
        uint16_t kc = keyboard_get_keycode(data[1], data[2], data[3]);
        data[4] = (uint8_t)(kc >> 8);
        data[5] = (uint8_t)(kc & 0xFF);
        break;
    }

    case id_dynamic_keymap_set_keycode:
        keyboard_set_keycode(data[1], data[2], data[3],
                             (uint16_t)(((uint16_t)data[4] << 8) | data[5]));
        break;

    case id_dynamic_keymap_reset:
        keyboard_reset_keymap();
        break;

    case id_custom_set_value:
    case id_custom_get_value:
    case id_custom_save:
        via_custom(data, command);
        break;

    case id_eeprom_reset:
        keyboard_reset_keymap();
        rgb_reset();
        break;

    case id_bootloader_jump:
        /* Same entry point as the WCH ISP bootloader. */
        EA = 0;
        ((void (*)(void))0x3800)();
        break;

    case id_dynamic_keymap_macro_get_count:
        data[1] = 0;
        break;

    case id_dynamic_keymap_macro_get_buffer_size:
        data[1] = 0;
        data[2] = 0;
        break;

    case id_dynamic_keymap_macro_get_buffer: {
        uint8_t size = data[3];
        for (i = 0; i < size && (uint8_t)(4 + i) < 32; i++) {
            data[4 + i] = 0;
        }
        break;
    }

    case id_dynamic_keymap_macro_set_buffer:
        /* Macros are not stored; accept the write and discard it. */
        break;

    case id_dynamic_keymap_macro_reset:
        break;

    case id_dynamic_keymap_get_layer_count:
        data[1] = keyboard_layer_count();
        break;

    case id_dynamic_keymap_get_buffer: {
        uint16_t offset = (uint16_t)(((uint16_t)data[1] << 8) | data[2]);
        uint8_t size = data[3];

        for (i = 0; i < size && (uint8_t)(4 + i) < 32; i++) {
            data[4 + i] = keyboard_keymap_byte((uint16_t)(offset + i));
        }
        break;
    }

    case id_dynamic_keymap_set_buffer: {
        uint16_t offset = (uint16_t)(((uint16_t)data[1] << 8) | data[2]);
        uint8_t size = data[3];

        for (i = 0; i < size && (uint8_t)(4 + i) < 32; i++) {
            keyboard_set_keymap_byte((uint16_t)(offset + i), data[4 + i]);
        }
        break;
    }

    default:
        data[0] = id_unhandled;
        break;
    }

    usb_raw_send(data, RAW_HID_SIZE);
}
