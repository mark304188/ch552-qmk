/*==========================================================================*
 * board_4key.h - 4-key direct-wired board with a WS2812 chain
 *
 * Wiring from the CH552G schematic:
 *
 *   But1  P1.7
 *   But2  P1.6
 *   But3  P1.4
 *   But4  P3.2
 *   RGB   P1.5   (WS2812 DIN, chain of one or more pixels)
 *
 * Each button connects its pin to GND; the firmware enables the internal
 * pull-up.  P1.5 is bit-banged, so P1.7 (SPI0 SCK) stays free for But1.
 * CONSUMER_ENABLE is 0 here, so interface 0 is a plain 8-byte boot
 * keyboard without report IDs (best Windows compatibility).
 *
 * Select this profile with:
 *     make BOARD=4key
 * The default 3x4 configuration in keyboard_config.h is not modified.
 *==========================================================================*/

#ifndef __BOARD_4KEY_H__
#define __BOARD_4KEY_H__

/*---------------------------------------------------------------*
 * USB identity                                                  *
 *---------------------------------------------------------------*/
#define USB_VENDOR_ID       0x1209
#define USB_PRODUCT_ID      0x5524

/* Use a plain 8-byte boot keyboard on interface 0 for maximum Windows
 * compatibility.  Media keys are not needed on this 4-key board. */
#define CONSUMER_ENABLE     0
#define USB_DEVICE_VERSION  0x0002
#define USB_MANUFACTURER    "CH552"
#define USB_PRODUCT         "CH552 QMK 4Key"

/*---------------------------------------------------------------*
 * Clock                                                         *
 *                                                               *
 * The bit-banged WS2812 timing is calibrated for 16 MHz.  12 MHz *
 * is also acceptable; 24 MHz is rejected by ws2812.c.           *
 *---------------------------------------------------------------*/
#ifndef FREQ_SYS
#define FREQ_SYS            16000000UL
#endif

/*---------------------------------------------------------------*
 * Direct-wired keys                                             *
 *---------------------------------------------------------------*/
#define KEYBOARD_MODE_MATRIX 0
#define KEYBOARD_MODE_DIRECT 1

#define KEYBOARD_MODE       KEYBOARD_MODE_DIRECT

#define DIRECT_KEY_COUNT    4
#define DIRECT_PINS         { {1, 7}, {1, 6}, {1, 4}, {3, 2} }

#define MATRIX_ROWS         1
#define MATRIX_COLS         DIRECT_KEY_COUNT
#define KEY_COUNT           (MATRIX_ROWS * MATRIX_COLS)

#if MATRIX_ROWS < 1 || MATRIX_COLS < 1
#error "MATRIX_ROWS and MATRIX_COLS must be at least 1"
#endif

#if KEY_COUNT > 64
#error "KEY_COUNT > 64"
#endif

/*---------------------------------------------------------------*
 * Layers and debounce                                           *
 *---------------------------------------------------------------*/
#define DYNAMIC_KEYMAP_LAYERS 2
#if DYNAMIC_KEYMAP_LAYERS < 1
#error "DYNAMIC_KEYMAP_LAYERS must be at least 1"
#endif

#define DEBOUNCE_MS           5
#if DEBOUNCE_MS < 1
#error "DEBOUNCE_MS must be at least 1"
#endif

/*---------------------------------------------------------------*
 * Default is 4 pixels (one per key).  A single pixel also works;  *
 * extra bits are simply shifted out of its DOUT.                  *
 *---------------------------------------------------------------*/
#define RGB_ENABLE             1
#define WS2812_LED_COUNT       4
#define WS2812_DRIVER_BITBANG  1
#define WS2812_PIN_PORT        1
#define WS2812_PIN_BIT         5
#define WS2812_GRB             1
#define RGB_DEFAULT_BRIGHTNESS 64
#define RGB_DEFAULT_HUE        0
#define RGB_DEFAULT_SAT        255
#define RGB_DEFAULT_EFFECT     1
#define RGB_DEFAULT_SPEED      128
#define RGB_UPDATE_MS          20

/*---------------------------------------------------------------*
 * Default keymap                                                *
 *                                                               *
 * But1 -> 1, But2 -> 2, But3 -> 3, But4 -> momentary layer 1.   *
 * Hold But4: But1..But3 become F1..F3.                          *
 *---------------------------------------------------------------*/
#define DEFAULT_KEYMAP {                                             \
    { /* layer 0 */                                                  \
        { KC_1, KC_2, KC_3, MO(1) }                                  \
    },                                                               \
    { /* layer 1 */                                                  \
        { KC_F1, KC_F2, KC_F3, KC_TRNS }                             \
    }                                                                \
}

#endif /* __BOARD_4KEY_H__ */
