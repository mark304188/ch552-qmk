/*==========================================================================*
 * rgb.h - WS2812 effects and VIA-compatible RGB settings
 *==========================================================================*/

#ifndef __RGB_H__
#define __RGB_H__

#include <stdint.h>
#include "keyboard_config.h"

void rgb_init(void) __reentrant;
void rgb_task(void) __reentrant;

/* VIA rgblight / rgb_matrix compatible values. */
void    rgb_set_brightness(uint8_t value);
uint8_t rgb_get_brightness(void);
void    rgb_set_effect(uint8_t value);   /* 0 = off */
uint8_t rgb_get_effect(void);
void    rgb_set_speed(uint8_t value);
uint8_t rgb_get_speed(void);
void    rgb_set_color(uint8_t hue, uint8_t sat);
uint8_t rgb_get_hue(void);
uint8_t rgb_get_sat(void);

void rgb_reset(void);
void rgb_save(void);

/* OpenRGB / VialRGB direct per-LED mode.  Colors are RGB, independent of
 * the WS2812 GRB/RGB wire order.  rgb_set_direct_hsv() is used by
 * VialRGB's direct fast-set packet, which carries HSV triples. */
void    rgb_set_direct_mode(uint8_t active);
uint8_t rgb_get_direct_mode(void);
void    rgb_set_direct_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void    rgb_set_direct_hsv(uint8_t index, uint8_t h, uint8_t s, uint8_t v) __reentrant;
void    rgb_get_direct_led(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) __reentrant;

/* Notify the effect engine that a key changed (reactive effects). */
void rgb_key_event(uint8_t key_index, uint8_t pressed);

#endif /* __RGB_H__ */
