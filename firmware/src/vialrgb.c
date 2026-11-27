/*==========================================================================*
 * vialrgb.c - Vial keyboard-ID + VialRGB lighting commands
 *
 * OpenRGB's VialRGB driver uses the same 32-byte raw HID endpoint as VIA.
 * It first asks VIA for its protocol version, then asks Vial for its
 * protocol version and VialRGB flag, and finally uses VIA lighting
 * commands 0x07/0x08 with sub-commands 0x40..0x44.
 *
 * This file implements only that subset.  VIA remains responsible for the
 * dynamic keymap, so key remapping and OpenRGB lighting work together.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "vialrgb.h"
#include "usb.h"
#include "rgb.h"
#include "keyboard_config.h"

#if !RGB_ENABLE || (WS2812_LED_COUNT == 0)
#error "VialRGB support requires RGB_ENABLE=1 and WS2812_LED_COUNT>0"
#endif

/* OpenRGB only requires Vial protocol >= 4 and VialRGB flag bit 0. */
#define VIAL_PROTOCOL_VERSION      4
#define VIALRGB_PROTOCOL_VERSION   1
#define VIALRGB_MAX_BRIGHTNESS     255

/* Effects advertised to OpenRGB.  OpenRGB adds "Off" itself, so only
 * non-zero IDs are listed here.  They must be in ascending order. */
static __code const uint16_t supported_effects[] = {
    1,   /* Direct */
    2    /* Solid colour */
};

#define SUPPORTED_EFFECT_COUNT \
    ((uint8_t)(sizeof(supported_effects) / sizeof(supported_effects[0])))

static __code const uint8_t vial_uid[8] = {
    'C', 'H', '5', '5', '2', 'Q', 'M', 'K'
};

#define PUT_U16_LE(p, value) \
    do { \
        (p)[0] = (uint8_t)((value) & 0xFF); \
        (p)[1] = (uint8_t)((value) >> 8); \
    } while (0)

#define GET_U16_LE(p) \
    ((uint16_t)((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8)))

uint8_t vialrgb_is_get_command(uint8_t subcommand)
{
    return (uint8_t)(subcommand >= VIALRGB_GET_INFO &&
                     subcommand <= VIALRGB_GET_LED_INFO);
}

uint8_t vialrgb_is_set_command(uint8_t subcommand)
{
    return (uint8_t)(subcommand == VIALRGB_SET_MODE ||
                     subcommand == VIALRGB_DIRECT_FASTSET);
}

static uint8_t vialrgb_to_effect(uint16_t mode) __reentrant
{
    return (mode == 2) ? 1 : 0;
}

/* OpenRGB indexes its mode array with this value.  Keep the returned mode
 * within the two advertised entries (Direct = 1, Static = 2).  Effects set
 * through VIA are reported as Static. */
static uint16_t effect_to_vialrgb(uint8_t effect) __reentrant
{
    return (effect == 0) ? 0 : 2;
}

void vialrgb_handle_vial(uint8_t *data, uint8_t length) __reentrant
{
    uint8_t i;

    /* Only the keyboard-ID query is needed by OpenRGB. */
    if (length < 13 || data[1] != 0x00) {
        return;
    }

    for (i = 0; i < length; i++) {
        data[i] = 0;
    }

    PUT_U16_LE(&data[0], VIAL_PROTOCOL_VERSION);
    for (i = 0; i < 8; i++) {
        data[4 + i] = vial_uid[i];
    }
    data[12] = 1; /* VialRGB flag bit 0 */
}

void vialrgb_get_value(uint8_t *data, uint8_t length) __reentrant
{
    uint8_t subcommand = data[1];
    uint8_t i;

    if (length < 8) {
        return;
    }

    switch (subcommand) {
    case VIALRGB_GET_INFO:
        PUT_U16_LE(&data[2], VIALRGB_PROTOCOL_VERSION);
        data[4] = VIALRGB_MAX_BRIGHTNESS;
        break;

    case VIALRGB_GET_MODE: {
        uint16_t mode = rgb_get_direct_mode() ? 1
                                              : effect_to_vialrgb(rgb_get_effect());

        PUT_U16_LE(&data[2], mode);
        data[4] = rgb_get_speed();
        data[5] = rgb_get_hue();
        data[6] = rgb_get_sat();
        data[7] = rgb_get_brightness();
        break;
    }

    case VIALRGB_GET_SUPPORTED: {
        uint16_t greater_than = GET_U16_LE(&data[2]);
        uint8_t out = 2;

        for (i = 2; i < length; i++) {
            data[i] = 0xFF;
        }
        for (i = 0; i < SUPPORTED_EFFECT_COUNT; i++) {
            uint16_t effect = supported_effects[i];

            if (effect > greater_than) {
                if ((uint8_t)(out + 2) > length) {
                    break;
                }
                PUT_U16_LE(&data[out], effect);
                out = (uint8_t)(out + 2);
            }
        }
        break;
    }

    case VIALRGB_GET_LED_COUNT:
        PUT_U16_LE(&data[2], WS2812_LED_COUNT);
        break;

    case VIALRGB_GET_LED_INFO: {
        uint16_t led = GET_U16_LE(&data[2]);

        if (led < WS2812_LED_COUNT) {
            /* Point coordinates are informational; OpenRGB uses the
             * matrix row/column to lay out the LEDs. */
            data[2] = (uint8_t)((led % MATRIX_COLS) * 32);
            data[3] = (uint8_t)((led / MATRIX_COLS) * 32);
            data[4] = 0x04; /* QMK LED_FLAG_KEYLIGHT */
            data[5] = (uint8_t)(led / MATRIX_COLS);
            data[6] = (uint8_t)(led % MATRIX_COLS);
            data[7] = 1; /* valid */
        } else {
            data[2] = 0;
            data[3] = 0;
            data[4] = 0;
            data[5] = 0xFF;
            data[6] = 0xFF;
            data[7] = 0;
        }
        break;
    }

    default:
        break;
    }
}

void vialrgb_set_value(uint8_t *data, uint8_t length) __reentrant
{
    uint8_t subcommand = data[1];

    if (length < 8) {
        return;
    }

    switch (subcommand) {
    case VIALRGB_SET_MODE: {
        uint16_t mode = GET_U16_LE(&data[2]);
        uint8_t effect;

        rgb_set_speed(data[4]);
        rgb_set_color(data[5], data[6]);
        rgb_set_brightness(data[7]);

        if (mode == 0) {
            rgb_set_direct_mode(0);
            rgb_set_effect(0);
        } else if (mode == 1) {
            rgb_set_direct_mode(1);
        } else {
            effect = vialrgb_to_effect(mode);
            rgb_set_direct_mode(0);
            rgb_set_effect(effect ? effect : 1);
        }
        break;
    }

    case VIALRGB_DIRECT_FASTSET: {
        uint16_t start = GET_U16_LE(&data[2]);
        uint8_t count = data[4];
        uint8_t i;

        if ((uint16_t)count * 3 > (uint16_t)(length - 5)) {
            count = (uint8_t)((length - 5) / 3);
        }

        for (i = 0; i < count; i++) {
            uint16_t index = (uint16_t)(start + i);
            uint8_t off = (uint8_t)(5 + i * 3);

            if (index >= WS2812_LED_COUNT) {
                break;
            }
            rgb_set_direct_hsv((uint8_t)index,
                               data[off], data[off + 1], data[off + 2]);
        }
        break;
    }

    default:
        break;
    }
}
