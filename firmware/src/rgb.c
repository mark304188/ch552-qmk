/*==========================================================================*
 * rgb.c - WS2812 effects with QMK/VIA rgblight-compatible settings
 *
 * VIA's rgblight channel (2) and rgb_matrix channel (3) both map here.
 * Effect numbers follow QMK's RGBLIGHT_MODE_* ordering closely enough
 * that the common entries in VIA's dropdown do something sensible:
 *
 *   0      off
 *   1      solid
 *   2-4    breathing
 *   5-6    rainbow mood
 *   7-13   rainbow swirl
 *   14-19  snake
 *   20-21  knight
 *   22     christmas
 *   23-33  static gradient
 *   34     RGB test
 *   35     alternating
 *   36-41  twinkle
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "rgb.h"
#include "ws2812.h"
#include "eeprom.h"
#include "debug.h"

#if RGB_ENABLE && (WS2812_LED_COUNT > 0)

static __xdata uint8_t led_buf[WS2812_LED_COUNT * 3];

static uint8_t  rgb_enabled;
static uint8_t  rgb_brightness;
static uint8_t  rgb_hue;
static uint8_t  rgb_sat;
static uint8_t  rgb_effect;
static uint8_t  rgb_speed;
static uint16_t rgb_phase;
static uint16_t rgb_last_ms;
static uint8_t  rgb_off_sent;

/*---------------------------------------------------------------*
 * HSV -> RGB                                                    *
 *---------------------------------------------------------------*/

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b) __reentrant
{
    uint8_t region;
    uint8_t rem;
    uint8_t p;
    uint8_t q;
    uint8_t t;

    if (v == 0) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }
    if (s == 0) {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    region = (uint8_t)(h / 43);
    rem = (uint8_t)((h - region * 43) * 6); /* 0..252 */

    /* The >>8 form avoids a 16/8 division in the hot path. */
    p = (uint8_t)(((uint16_t)v * (255 - s)) >> 8);
    q = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * rem) >> 8))) >> 8);
    t = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * (255 - rem)) >> 8))) >> 8);

    switch (region) {
    case 0:
        *r = v; *g = t; *b = p;
        break;
    case 1:
        *r = q; *g = v; *b = p;
        break;
    case 2:
        *r = p; *g = v; *b = t;
        break;
    case 3:
        *r = p; *g = q; *b = v;
        break;
    case 4:
        *r = t; *g = p; *b = v;
        break;
    default:
        *r = v; *g = p; *b = q;
        break;
    }
}

/* Write one LED at wire position `index`; led_buf is already GRB/RGB
 * ordered according to WS2812_GRB. */
static void set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= WS2812_LED_COUNT) {
        return;
    }

#if WS2812_GRB
    led_buf[index * 3 + 0] = g;
    led_buf[index * 3 + 1] = r;
    led_buf[index * 3 + 2] = b;
#else
    led_buf[index * 3 + 0] = r;
    led_buf[index * 3 + 1] = g;
    led_buf[index * 3 + 2] = b;
#endif
}

static void set_led_hsv(uint8_t index, uint8_t h, uint8_t s, uint8_t v)
{
    uint8_t r, g, b;

    hsv_to_rgb(h, s, v, &r, &g, &b);
    set_led(index, r, g, b);
}

/* A triangle wave 0..255 from an 8-bit phase. */
static uint8_t triangle(uint8_t x)
{
    if (x < 128) {
        return (uint8_t)(x << 1);
    }
    return (uint8_t)((255 - x) << 1);
}

static uint8_t scale8(uint8_t value, uint8_t scale)
{
    return (uint8_t)(((uint16_t)value * scale) >> 8);
}

/*---------------------------------------------------------------*
 * Effect frame                                                  *
 *---------------------------------------------------------------*/

static void render_frame(void) __reentrant
{
    uint8_t i;
    uint8_t count = WS2812_LED_COUNT;
    uint8_t eff = rgb_effect;

    if (!rgb_enabled || rgb_brightness == 0) {
        for (i = 0; i < count; i++) {
            set_led(i, 0, 0, 0);
        }
        return;
    }

    if (eff == 1) {
        /* Solid colour. */
        for (i = 0; i < count; i++) {
            set_led_hsv(i, rgb_hue, rgb_sat, rgb_brightness);
        }
    } else if (eff >= 2 && eff <= 4) {
        /* Breathing. */
        uint8_t v = scale8(rgb_brightness, triangle((uint8_t)rgb_phase));
        for (i = 0; i < count; i++) {
            set_led_hsv(i, rgb_hue, rgb_sat, v);
        }
    } else if (eff >= 5 && eff <= 6) {
        /* Rainbow mood: one hue for the whole strip. */
        uint8_t h = (uint8_t)(rgb_hue + (uint8_t)rgb_phase);
        for (i = 0; i < count; i++) {
            set_led_hsv(i, h, rgb_sat, rgb_brightness);
        }
    } else if (eff >= 7 && eff <= 13) {
        /* Rainbow swirl. */
        for (i = 0; i < count; i++) {
            uint8_t h = (uint8_t)(rgb_hue + (uint8_t)rgb_phase +
                                  (uint8_t)((uint16_t)i * 256 / count));
            set_led_hsv(i, h, rgb_sat, rgb_brightness);
        }
    } else if (eff >= 14 && eff <= 19) {
        /* Snake: a bright head with a dim tail. */
        uint8_t head = (uint8_t)(rgb_phase % (count + 3));
        for (i = 0; i < count; i++) {
            uint8_t v = rgb_brightness / 12;
            if (i == head || (uint8_t)(i + 1) == head) {
                v = rgb_brightness;
            }
            set_led_hsv(i, rgb_hue, rgb_sat, v);
        }
    } else if (eff >= 20 && eff <= 21) {
        /* Knight: a dot bounces back and forth. */
        uint8_t span = (uint8_t)(count * 2 - 2);
        uint8_t pos = (uint8_t)(rgb_phase % (span ? span : 1));
        uint8_t head;

        if (pos >= count) {
            head = (uint8_t)(span - pos);
        } else {
            head = pos;
        }
        for (i = 0; i < count; i++) {
            set_led_hsv(i, rgb_hue, rgb_sat,
                        (i == head) ? rgb_brightness : (rgb_brightness / 16));
        }
    } else if (eff == 22) {
        /* Christmas: alternating red / green. */
        for (i = 0; i < count; i++) {
            set_led_hsv(i, (i & 1) ? 85 : 0, 255, rgb_brightness);
        }
    } else if (eff >= 23 && eff <= 33) {
        /* Static gradient across the strip. */
        for (i = 0; i < count; i++) {
            uint8_t h = (uint8_t)(rgb_hue + (uint8_t)((uint16_t)i * 256 / count));
            set_led_hsv(i, h, rgb_sat, rgb_brightness);
        }
    } else if (eff == 34) {
        /* RGB test: whole strip cycles red, green, blue. */
        uint8_t step = (uint8_t)((rgb_phase >> 5) % 6);
        uint8_t h = (step < 2) ? 0 : (step < 4) ? 85 : 170;
        for (i = 0; i < count; i++) {
            set_led_hsv(i, h, 255, rgb_brightness);
        }
    } else if (eff == 35) {
        /* Alternating halves. */
        for (i = 0; i < count; i++) {
            set_led_hsv(i, (i < count / 2) ? rgb_hue : (uint8_t)(rgb_hue + 128),
                        rgb_sat, rgb_brightness);
        }
    } else if (eff >= 36 && eff <= 41) {
        /* Twinkle: pseudo-random bright dots. */
        uint16_t seed = (uint16_t)(rgb_phase * 1103u + 12345u);
        for (i = 0; i < count; i++) {
            seed = (uint16_t)(seed * 25173u + 13849u);
            set_led_hsv(i, rgb_hue, rgb_sat,
                        (seed & 0x8000) ? rgb_brightness : (rgb_brightness / 10));
        }
    } else {
        /* Unknown mode: solid colour, better than a black keyboard. */
        for (i = 0; i < count; i++) {
            set_led_hsv(i, rgb_hue, rgb_sat, rgb_brightness);
        }
    }
}

/*---------------------------------------------------------------*
 * Persistence                                                   *
 *---------------------------------------------------------------*/

static uint8_t load_config(void)
{
    if (ee_read_byte(EE_RGB_OFFSET + 0) != 'R' ||
        ee_read_byte(EE_RGB_OFFSET + 1) != 'G' ||
        ee_read_byte(EE_RGB_OFFSET + 2) != 'B') {
        return 0;
    }

    rgb_brightness = ee_read_byte(EE_RGB_OFFSET + 3);
    rgb_hue        = ee_read_byte(EE_RGB_OFFSET + 4);
    rgb_sat        = ee_read_byte(EE_RGB_OFFSET + 5);
    rgb_effect     = ee_read_byte(EE_RGB_OFFSET + 6);
    rgb_speed      = ee_read_byte(EE_RGB_OFFSET + 7);

    rgb_enabled = (rgb_effect != 0);
    return 1;
}

void rgb_save(void)
{
    /* Write the payload first and the magic last, so a power loss while
     * saving cannot leave a valid-looking but half-written RGB block. */
    ee_write_byte(EE_RGB_OFFSET + 3, rgb_brightness);
    ee_write_byte(EE_RGB_OFFSET + 4, rgb_hue);
    ee_write_byte(EE_RGB_OFFSET + 5, rgb_sat);
    ee_write_byte(EE_RGB_OFFSET + 6, rgb_effect);
    ee_write_byte(EE_RGB_OFFSET + 7, rgb_speed);
    ee_write_byte(EE_RGB_OFFSET + 0, 'R');
    ee_write_byte(EE_RGB_OFFSET + 1, 'G');
    ee_write_byte(EE_RGB_OFFSET + 2, 'B');
}

void rgb_reset(void)
{
    rgb_brightness = RGB_DEFAULT_BRIGHTNESS;
    rgb_hue        = RGB_DEFAULT_HUE;
    rgb_sat        = RGB_DEFAULT_SAT;
    rgb_effect     = RGB_DEFAULT_EFFECT;
    rgb_speed      = RGB_DEFAULT_SPEED;
    rgb_enabled    = (rgb_effect != 0);
    rgb_phase      = 0;
    rgb_save();
}

void rgb_init(void) __reentrant
{
    ws2812_init();

    if (!load_config()) {
        rgb_reset();
    }

    rgb_phase = 0;
    rgb_last_ms = timer_ms();
    render_frame();
    ws2812_show(led_buf, WS2812_LED_COUNT);
    rgb_off_sent = (uint8_t)(!rgb_enabled || rgb_brightness == 0);
}

void rgb_task(void) __reentrant
{
    uint16_t now;

    if (!rgb_enabled || rgb_brightness == 0) {
        if (!rgb_off_sent) {
            render_frame();
            ws2812_show(led_buf, WS2812_LED_COUNT);
            rgb_off_sent = 1;
        }
        return;
    }
    rgb_off_sent = 0;

    now = timer_ms();
    if ((uint16_t)(now - rgb_last_ms) < RGB_UPDATE_MS) {
        return;
    }
    rgb_last_ms = now;

    /* Speed 0..255 -> about 1..9 phase steps per update. */
    rgb_phase = (uint16_t)(rgb_phase + 1 + (rgb_speed >> 5));

    render_frame();
    ws2812_show(led_buf, WS2812_LED_COUNT);
}

/*---------------------------------------------------------------*
 * VIA-compatible getters / setters                              *
 *---------------------------------------------------------------*/

void rgb_set_brightness(uint8_t value)
{
    rgb_brightness = value;
}

uint8_t rgb_get_brightness(void)
{
    return rgb_brightness;
}

void rgb_set_effect(uint8_t value)
{
    if (value == 0) {
        rgb_enabled = 0;
        rgb_effect = 0;
    } else {
        rgb_enabled = 1;
        rgb_effect = value;
    }
}

uint8_t rgb_get_effect(void)
{
    return rgb_enabled ? rgb_effect : 0;
}

void rgb_set_speed(uint8_t value)
{
    rgb_speed = value;
}

uint8_t rgb_get_speed(void)
{
    return rgb_speed;
}

void rgb_set_color(uint8_t hue, uint8_t sat)
{
    rgb_hue = hue;
    rgb_sat = sat;
}

uint8_t rgb_get_hue(void)
{
    return rgb_hue;
}

uint8_t rgb_get_sat(void)
{
    return rgb_sat;
}

void rgb_key_event(uint8_t key_index, uint8_t pressed)
{
    (void)key_index;
    (void)pressed;
}

#else /* RGB disabled */

void rgb_init(void) __reentrant {}
void rgb_task(void) __reentrant {}
void rgb_set_brightness(uint8_t value) { (void)value; }
uint8_t rgb_get_brightness(void) { return 0; }
void rgb_set_effect(uint8_t value) { (void)value; }
uint8_t rgb_get_effect(void) { return 0; }
void rgb_set_speed(uint8_t value) { (void)value; }
uint8_t rgb_get_speed(void) { return 0; }
void rgb_set_color(uint8_t hue, uint8_t sat) { (void)hue; (void)sat; }
uint8_t rgb_get_hue(void) { return 0; }
uint8_t rgb_get_sat(void) { return 0; }
void rgb_reset(void) {}
void rgb_save(void) {}
void rgb_key_event(uint8_t key_index, uint8_t pressed) { (void)key_index; (void)pressed; }

#endif
