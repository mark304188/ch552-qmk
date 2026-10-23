/*==========================================================================*
 * keyboard.h - keymap, layers, debounce and HID report generation
 *==========================================================================*/

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <stdint.h>
#include "keyboard_config.h"

void keyboard_init(void);
void keyboard_task(void);

/* Dynamic keymap access (VIA protocol). */
uint8_t  keyboard_layer_count(void);
uint16_t keyboard_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
void     keyboard_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);
void     keyboard_reset_keymap(void);

/* Byte-oriented view used by VIA's get/set buffer commands.
 * Layout: layer-major, row-major, column-major, little-endian keycodes. */
uint8_t  keyboard_keymap_byte(uint16_t offset);
void     keyboard_set_keymap_byte(uint16_t offset, uint8_t value);
uint16_t keyboard_keymap_size(void);

/* Debounced switch state, used by VIA (bit set = pressed). */
uint8_t  keyboard_matrix_row(uint8_t row);
uint8_t  keyboard_layer_state(void);

/* True once after a keymap/layer change, so USB can refresh its report. */
uint8_t  keyboard_report_dirty(void);
void     keyboard_clear_report_dirty(void);

/* Report builder.  Returns length (with or without report ID) and fills
 * report[].  Called by main when the report is dirty. */
uint8_t  keyboard_build_report(uint8_t *report, uint8_t boot_protocol) __reentrant;
uint8_t  keyboard_build_consumer(uint8_t *report) __reentrant;

#endif /* __KEYBOARD_H__ */
