/*==========================================================================*
 * keyboard.c - scan, debounce, layers and HID reports
 *
 * The dynamic keymap is stored in the CH552's 128-byte Data Flash as:
 *
 *   0x00  'C' 'Q' 'M' 'K'
 *   0x04  layout version
 *   0x05  rows
 *   0x06  columns
 *   0x07  layers
 *   0x08  keymap bytes, layer-major, row-major, little-endian keycodes
 *   ....  one checksum byte
 *   0x78  RGB settings (see rgb.c)
 *
 * If the header or checksum does not match the firmware configuration,
 * the default keymap from keyboard_config.h is loaded and written back.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "keyboard.h"
#include "matrix.h"
#include "eeprom.h"
#include "rgb.h"
#include "debug.h"

#define KEYMAP_EE_VERSION 2
#define KEYMAP_BYTES      ((uint16_t)(KEY_COUNT * DYNAMIC_KEYMAP_LAYERS * 2))
#define KEYMAP_CHECKSUM   ((uint8_t)(EE_KEYMAP_BASE + KEYMAP_BYTES))

#if (EE_KEYMAP_BASE + KEY_COUNT * DYNAMIC_KEYMAP_LAYERS * 2 + 1) > EE_LAYOUT_LIMIT
#error "Keymap does not fit in Data Flash. Reduce KEY_COUNT or DYNAMIC_KEYMAP_LAYERS."
#endif

/*---------------------------------------------------------------*
 * Static keymap (read from flash, writable through VIA)         *
 *---------------------------------------------------------------*/

static __code const uint16_t default_keymap[DYNAMIC_KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS] = DEFAULT_KEYMAP;

static __xdata uint16_t dynamic_keymap[DYNAMIC_KEYMAP_LAYERS][KEY_COUNT];
static __xdata uint16_t keycode_at[KEY_COUNT];
static __xdata uint8_t  debounce[KEY_COUNT];
static __xdata uint8_t  key_down[KEY_COUNT];
static __xdata uint8_t  scan_buf[KEY_COUNT];

static uint8_t layer_state;
static uint8_t layer_toggled;
static uint8_t report_dirty;

/*---------------------------------------------------------------*
 * Data Flash helpers                                            *
 *---------------------------------------------------------------*/

static void ee_update(uint8_t offset, uint8_t value)
{
    if (ee_read_byte(offset) != value) {
        ee_write_byte(offset, value);
    }
}

static uint8_t keymap_checksum(void)
{
    uint8_t sum = 0;
    uint8_t layer;
    uint8_t key;

    for (layer = 0; layer < DYNAMIC_KEYMAP_LAYERS; layer++) {
        for (key = 0; key < KEY_COUNT; key++) {
            uint16_t kc = dynamic_keymap[layer][key];
            sum = (uint8_t)(sum + (uint8_t)(kc & 0xFF));
            sum = (uint8_t)(sum + (uint8_t)(kc >> 8));
        }
    }

    return sum;
}

static void keymap_write_header(void)
{
    ee_update(0, 'C');
    ee_update(1, 'Q');
    ee_update(2, 'M');
    ee_update(3, 'K');
    ee_update(4, KEYMAP_EE_VERSION);
    ee_update(5, MATRIX_ROWS);
    ee_update(6, MATRIX_COLS);
    ee_update(7, DYNAMIC_KEYMAP_LAYERS);
}

static void keymap_save_all(void)
{
    uint8_t layer;
    uint8_t key;
    uint16_t offset = 0;

    keymap_write_header();

    for (layer = 0; layer < DYNAMIC_KEYMAP_LAYERS; layer++) {
        for (key = 0; key < KEY_COUNT; key++) {
            uint16_t kc = dynamic_keymap[layer][key];
            ee_update((uint8_t)(EE_KEYMAP_BASE + offset), (uint8_t)(kc & 0xFF));
            ee_update((uint8_t)(EE_KEYMAP_BASE + offset + 1), (uint8_t)(kc >> 8));
            offset += 2;
        }
    }

    ee_update(KEYMAP_CHECKSUM, keymap_checksum());
}

static void keymap_load_defaults(void)
{
    uint8_t layer;
    uint8_t row;
    uint8_t col;

    for (layer = 0; layer < DYNAMIC_KEYMAP_LAYERS; layer++) {
        for (row = 0; row < MATRIX_ROWS; row++) {
            for (col = 0; col < MATRIX_COLS; col++) {
                dynamic_keymap[layer][row * MATRIX_COLS + col] =
                    default_keymap[layer][row][col];
            }
        }
    }
}

/* Accept both byte orders for a 16-bit keycode.  Some Windows VIA/HID
 * paths deliver a value such as 0x5900 for KC_KP_1 (0x0059) or 0x0400 for
 * KC_A (0x0004).  If the received value is not a keycode this firmware can
 * use but its byte-reversed form is, store the reversed form. */
static uint8_t keycode_supported(uint16_t kc)
{
    if (kc <= 0x00E7) {
        return 1;
    }
    if (kc >= 0x5100 && kc <= 0x511F) {
        return 1;
    }
    if (kc >= 0x5200 && kc <= 0x521F) {
        return 1;
    }
    if (kc >= 0x5220 && kc <= 0x523F) {
        return 1;
    }
    if (kc >= 0x5260 && kc <= 0x527F) {
        return 1;
    }
    if (kc == QK_BOOTLOADER || kc == QK_CLEAR_EEPROM) {
        return 1;
    }
    return 0;
}

static uint16_t keycode_normalize(uint16_t kc)
{
    uint16_t swapped = (uint16_t)((kc >> 8) | (kc << 8));

    if (keycode_supported(kc)) {
        return kc;
    }
    if (keycode_supported(swapped)) {
        return swapped;
    }
    return kc;
}

static uint8_t keymap_load(void) __reentrant
{
    uint8_t layer;
    uint8_t key;
    uint16_t offset = 0;
    uint8_t sum = 0;
    uint8_t changed = 0;

    if (ee_read_byte(0) != 'C' || ee_read_byte(1) != 'Q' ||
        ee_read_byte(2) != 'M' || ee_read_byte(3) != 'K' ||
        ee_read_byte(4) != KEYMAP_EE_VERSION ||
        ee_read_byte(5) != MATRIX_ROWS ||
        ee_read_byte(6) != MATRIX_COLS ||
        ee_read_byte(7) != DYNAMIC_KEYMAP_LAYERS) {
        return 0;
    }

    for (layer = 0; layer < DYNAMIC_KEYMAP_LAYERS; layer++) {
        for (key = 0; key < KEY_COUNT; key++) {
            uint8_t lo = ee_read_byte((uint8_t)(EE_KEYMAP_BASE + offset));
            uint8_t hi = ee_read_byte((uint8_t)(EE_KEYMAP_BASE + offset + 1));
            uint16_t kc = (uint16_t)(lo | ((uint16_t)hi << 8));
            uint16_t normalized = keycode_normalize(kc);

            dynamic_keymap[layer][key] = normalized;
            if (normalized != kc) {
                changed = 1;
            }
            sum = (uint8_t)(sum + lo + hi);
            offset += 2;
        }
    }

    if (sum != ee_read_byte(KEYMAP_CHECKSUM)) {
        return 0;
    }
    if (changed) {
        keymap_save_all();
    }
    return 1;
}

/*---------------------------------------------------------------*
 * Keycode classification                                        *
 *---------------------------------------------------------------*/

/* Current QMK uses 0x5220..0x523F for MO(); some older VIA builds use
 * 0x5100..0x511F.  Accept both. */
static uint8_t mo_layer(uint16_t kc)
{
    if (kc >= 0x5100 && kc <= 0x511F) {
        return (uint8_t)(kc & 0x1F);
    }
    if (kc >= 0x5220 && kc <= 0x523F) {
        return (uint8_t)(kc & 0x1F);
    }
    return 0xFF;
}

/* Current QMK: 0x5260..0x527F.  Older QMK/VIA: 0x5200..0x521F. */
static uint8_t tg_layer(uint16_t kc)
{
    if (kc >= 0x5200 && kc <= 0x521F) {
        return (uint8_t)(kc & 0x1F);
    }
    if (kc >= 0x5260 && kc <= 0x527F) {
        return (uint8_t)(kc & 0x1F);
    }
    return 0xFF;
}

static uint16_t consumer_usage(uint16_t kc)
{
    switch (kc) {
    case KC_SYSTEM_POWER:      return 0x0030;
    case KC_SYSTEM_SLEEP:      return 0x0032;
    case KC_SYSTEM_WAKE:       return 0x0034;
    case KC_AUDIO_MUTE:        return 0x00E2;
    case KC_AUDIO_VOL_UP:      return 0x00E9;
    case KC_AUDIO_VOL_DOWN:    return 0x00EA;
    case KC_MEDIA_NEXT_TRACK:  return 0x00B5;
    case KC_MEDIA_PREV_TRACK:  return 0x00B6;
    case KC_MEDIA_STOP:        return 0x00B7;
    case KC_MEDIA_PLAY_PAUSE:  return 0x00CD;
    case KC_MEDIA_SELECT:      return 0x0183;
    case KC_MEDIA_EJECT:       return 0x00B8;
    case KC_MAIL:              return 0x018A;
    case KC_CALCULATOR:        return 0x0192;
    case KC_MY_COMPUTER:       return 0x0194;
    case KC_WWW_SEARCH:        return 0x0221;
    case KC_WWW_HOME:          return 0x0223;
    case KC_WWW_BACK:          return 0x0224;
    case KC_WWW_FORWARD:       return 0x0225;
    case KC_WWW_STOP:          return 0x0226;
    case KC_WWW_REFRESH:       return 0x0227;
    case KC_WWW_FAVORITES:     return 0x022A;
    case KC_MEDIA_FAST_FORWARD:return 0x00B3;
    case KC_MEDIA_REWIND:      return 0x00B4;
    case KC_BRIGHTNESS_UP:     return 0x006F;
    case KC_BRIGHTNESS_DOWN:   return 0x0070;
    default:                   return 0;
    }
}

/*---------------------------------------------------------------*
 * Layer resolution                                              *
 *---------------------------------------------------------------*/

static void update_layers(void)
{
    uint8_t state = (uint8_t)(layer_toggled | 0x01); /* layer 0 is always on */
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        uint8_t layer = mo_layer(keycode_at[i]);

        if (layer != 0xFF && layer < DYNAMIC_KEYMAP_LAYERS) {
            state |= (uint8_t)(1u << layer);
        }
    }

    layer_state = state;
}

static uint16_t resolve_keycode(uint8_t index)
{
    int8_t layer;

    for (layer = DYNAMIC_KEYMAP_LAYERS - 1; layer >= 0; layer--) {
        if (layer_state & (uint8_t)(1u << layer)) {
            uint16_t kc = dynamic_keymap[layer][index];

            if (kc != KC_TRNS) {
                return kc;
            }
        }
    }

    return KC_NO;
}

/*---------------------------------------------------------------*
 * Key events                                                    *
 *---------------------------------------------------------------*/

static void bootloader_jump(void)
{
    /* The WCH bootloader is at 0x3800 on a 16 KB CH552. */
    EA = 0;
    ((void (*)(void))0x3800)();
}

static void key_press(uint8_t index)
{
    uint16_t kc = resolve_keycode(index);
    uint8_t layer;

    keycode_at[index] = kc;
    key_down[index] = 1;

    layer = mo_layer(kc);
    if (layer != 0xFF) {
        update_layers();
        report_dirty = 1;
        return;
    }

    layer = tg_layer(kc);
    if (layer != 0xFF) {
        if (layer < DYNAMIC_KEYMAP_LAYERS) {
            layer_toggled ^= (uint8_t)(1u << layer);
        }
        update_layers();
        report_dirty = 1;
        return;
    }

    if (kc == QK_BOOTLOADER) {
        bootloader_jump();
        return;
    }

    if (kc == QK_CLEAR_EEPROM) {
        keyboard_reset_keymap();
        rgb_reset();
        report_dirty = 1;
        return;
    }

    report_dirty = 1;
    rgb_key_event(index, 1);
}

static void key_release(uint8_t index)
{
    uint16_t kc = keycode_at[index];

    key_down[index] = 0;
    keycode_at[index] = KC_NO;

    if (mo_layer(kc) != 0xFF) {
        update_layers();
    }

    report_dirty = 1;
    rgb_key_event(index, 0);
}

/*---------------------------------------------------------------*
 * Report building                                               *
 *---------------------------------------------------------------*/

uint8_t keyboard_build_report(uint8_t *report, uint8_t boot_protocol) __reentrant
{
    uint8_t mods = 0;
    uint8_t keys[6];
    uint8_t n = 0;
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        uint16_t kc = keycode_at[i];

        if (kc >= KC_LEFT_CTRL && kc <= KC_RIGHT_GUI) {
            mods |= (uint8_t)(1u << (kc - KC_LEFT_CTRL));
        } else if (kc >= 0x04 && kc <= 0xE7) {
            /* Media keycodes live in this numeric range but are sent in
             * the Consumer report instead. */
            if (consumer_usage(kc) != 0) {
                continue;
            }
            if (n < 6) {
                keys[n++] = (uint8_t)kc;
            }
        }
    }

#if CONSUMER_ENABLE
    if (boot_protocol) {
        report[0] = mods;
        report[1] = 0;
        for (i = 0; i < n; i++) {
            report[2 + i] = keys[i];
        }
        for (; i < 6; i++) {
            report[2 + i] = 0;
        }
        return 8;
    }

    report[0] = 1; /* report ID */
    report[1] = mods;
    report[2] = 0;
    for (i = 0; i < n; i++) {
        report[3 + i] = keys[i];
    }
    for (; i < 6; i++) {
        report[3 + i] = 0;
    }
    return 9;
#else
    /* Plain 8-byte boot keyboard report: no report ID, no consumer. */
    (void)boot_protocol;
    report[0] = mods;
    report[1] = 0;
    for (i = 0; i < n; i++) {
        report[2 + i] = keys[i];
    }
    for (; i < 6; i++) {
        report[2 + i] = 0;
    }
    return 8;
#endif
}

uint8_t keyboard_build_consumer(uint8_t *report) __reentrant
{
#if CONSUMER_ENABLE
    uint16_t usage = 0;
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        uint16_t u = consumer_usage(keycode_at[i]);

        if (u) {
            usage = u;
        }
    }

    report[0] = 2; /* report ID */
    report[1] = (uint8_t)(usage & 0xFF);
    report[2] = (uint8_t)(usage >> 8);
    return 3;
#else
    (void)report;
    return 0;
#endif
}

uint8_t keyboard_report_dirty(void)
{
    return report_dirty;
}

void keyboard_clear_report_dirty(void)
{
    report_dirty = 0;
}

/*---------------------------------------------------------------*
 * Public init / task                                            *
 *---------------------------------------------------------------*/

void keyboard_init(void)
{
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        debounce[i] = 0;
        key_down[i] = 0;
        keycode_at[i] = KC_NO;
    }

    layer_toggled = 0;
    layer_state = 0x01;
    report_dirty = 1;

    matrix_init();

    if (!keymap_load()) {
        keymap_load_defaults();
        keymap_save_all();
    }
}

void keyboard_task(void)
{
    uint8_t i;

    matrix_read(scan_buf);

    for (i = 0; i < KEY_COUNT; i++) {
        if (scan_buf[i]) {
            if (debounce[i] < DEBOUNCE_MS) {
                debounce[i]++;
                if (debounce[i] == DEBOUNCE_MS) {
                    key_press(i);
                }
            }
        } else {
            if (debounce[i] > 0) {
                debounce[i]--;
                if (debounce[i] == 0) {
                    key_release(i);
                }
            }
        }
    }
}

/*---------------------------------------------------------------*
 * Dynamic keymap API                                            *
 *---------------------------------------------------------------*/

uint8_t keyboard_layer_count(void)
{
    return DYNAMIC_KEYMAP_LAYERS;
}

uint16_t keyboard_get_keycode(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= DYNAMIC_KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }
    return dynamic_keymap[layer][row * MATRIX_COLS + col];
}

void keyboard_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    uint16_t index;
    uint16_t offset;

    if (layer >= DYNAMIC_KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return;
    }

    keycode = keycode_normalize(keycode);

    index = (uint16_t)(layer * KEY_COUNT + row * MATRIX_COLS + col);
    offset = (uint16_t)(index * 2);
    dynamic_keymap[layer][row * MATRIX_COLS + col] = keycode;

    ee_update((uint8_t)(EE_KEYMAP_BASE + offset), (uint8_t)(keycode & 0xFF));
    ee_update((uint8_t)(EE_KEYMAP_BASE + offset + 1), (uint8_t)(keycode >> 8));
    ee_update(KEYMAP_CHECKSUM, keymap_checksum());

    report_dirty = 1;
}

uint16_t keyboard_keymap_size(void)
{
    return KEYMAP_BYTES;
}

uint8_t keyboard_keymap_byte(uint16_t offset)
{
    uint16_t index;
    uint16_t kc;
    uint8_t layer;

    if (offset >= KEYMAP_BYTES) {
        return 0;
    }

    index = (uint16_t)(offset >> 1);
    layer = (uint8_t)(index / KEY_COUNT);
    index = (uint16_t)(index % KEY_COUNT);
    kc = dynamic_keymap[layer][index];

    return (offset & 1) ? (uint8_t)(kc >> 8) : (uint8_t)(kc & 0xFF);
}

void keyboard_set_keymap_byte(uint16_t offset, uint8_t value)
{
    uint16_t index;
    uint16_t kc;
    uint8_t layer;
    uint8_t old;

    if (offset >= KEYMAP_BYTES) {
        return;
    }

    index = (uint16_t)(offset >> 1);
    layer = (uint8_t)(index / KEY_COUNT);
    index = (uint16_t)(index % KEY_COUNT);
    kc = dynamic_keymap[layer][index];

    if (offset & 1) {
        old = (uint8_t)(kc >> 8);
        kc = (uint16_t)((kc & 0x00FF) | ((uint16_t)value << 8));
    } else {
        old = (uint8_t)(kc & 0xFF);
        kc = (uint16_t)((kc & 0xFF00) | value);
    }

    dynamic_keymap[layer][index] = kc;

    if (old != value) {
        ee_update((uint8_t)(EE_KEYMAP_BASE + offset), value);
        ee_update(KEYMAP_CHECKSUM, keymap_checksum());
    }

    report_dirty = 1;
}

void keyboard_reset_keymap(void)
{
    keymap_load_defaults();
    keymap_save_all();
    layer_toggled = 0;
    update_layers();
    report_dirty = 1;
}

uint8_t keyboard_matrix_row(uint8_t row)
{
    uint8_t bits = 0;
    uint8_t col;

    if (row >= MATRIX_ROWS) {
        return 0;
    }

    for (col = 0; col < MATRIX_COLS && col < 8; col++) {
        if (key_down[row * MATRIX_COLS + col]) {
            bits |= (uint8_t)(1u << col);
        }
    }

    return bits;
}

uint8_t keyboard_layer_state(void)
{
    return layer_state;
}
