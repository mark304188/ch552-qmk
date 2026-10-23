/*==========================================================================*
 * matrix.c - key matrix / direct-pin scanner
 *
 * Matrix mode: each row is a push-pull output driven low in turn; each
 * column is a quasi-bidirectional input with its pull-up enabled.  A
 * pressed key pulls its column low.  Diodes are recommended if three or
 * more keys can be pressed at once, otherwise ghosting is possible.
 *
 * Direct mode: every key connects its own pin to GND, no row pin.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "matrix.h"
#include "debug.h"

#if KEYBOARD_MODE == KEYBOARD_MODE_MATRIX

static __code pin_t row_pins[MATRIX_ROWS] = ROW_PINS;
static __code pin_t col_pins[MATRIX_COLS] = COL_PINS;

#else

static __code pin_t direct_pins[DIRECT_KEY_COUNT] = DIRECT_PINS;

#endif

static __xdata uint8_t raw_state[KEY_COUNT];

/*---------------------------------------------------------------*
 * Pin helpers.  CH552 ports are SFRs, so a port index is turned  *
 * into the register access with a switch.                        *
 *---------------------------------------------------------------*/

static void pin_output(uint8_t port, uint8_t bit)
{
    uint8_t mask = (uint8_t)(1u << bit);

    if (port == 1) {
        P1_MOD_OC &= ~mask; /* push-pull */
        P1_DIR_PU |= mask;  /* output     */
    } else if (port == 3) {
        P3_MOD_OC &= ~mask;
        P3_DIR_PU |= mask;
    }
}

static void pin_input_pullup(uint8_t port, uint8_t bit)
{
    uint8_t mask = (uint8_t)(1u << bit);

    if (port == 1) {
        P1_MOD_OC |= mask; /* open drain / quasi-bidirectional */
        P1_DIR_PU |= mask; /* pull-up enabled                  */
        P1 |= mask;        /* release, weak pull-up active     */
    } else if (port == 3) {
        P3_MOD_OC |= mask;
        P3_DIR_PU |= mask;
        P3 |= mask;
    }
}

static void pin_write(uint8_t port, uint8_t bit, uint8_t value)
{
    uint8_t mask = (uint8_t)(1u << bit);

    if (port == 1) {
        if (value) {
            P1 |= mask;
        } else {
            P1 &= ~mask;
        }
    } else if (port == 3) {
        if (value) {
            P3 |= mask;
        } else {
            P3 &= ~mask;
        }
    }
}

static uint8_t pin_read(uint8_t port, uint8_t bit)
{
    uint8_t mask = (uint8_t)(1u << bit);

    if (port == 1) {
        return (P1 & mask) ? 1 : 0;
    }
    if (port == 3) {
        return (P3 & mask) ? 1 : 0;
    }
    return 1;
}

/*---------------------------------------------------------------*
 * Public interface                                              *
 *---------------------------------------------------------------*/

void matrix_init(void)
{
#if KEYBOARD_MODE == KEYBOARD_MODE_MATRIX
    uint8_t i;

    for (i = 0; i < MATRIX_ROWS; i++) {
        pin_output(row_pins[i].port, row_pins[i].bit);
        pin_write(row_pins[i].port, row_pins[i].bit, 1);
    }
    for (i = 0; i < MATRIX_COLS; i++) {
        pin_input_pullup(col_pins[i].port, col_pins[i].bit);
    }
#else
    uint8_t i;

    for (i = 0; i < DIRECT_KEY_COUNT; i++) {
        pin_input_pullup(direct_pins[i].port, direct_pins[i].bit);
    }
#endif
}

void matrix_read(uint8_t __xdata *state)
{
#if KEYBOARD_MODE == KEYBOARD_MODE_MATRIX
    uint8_t r;
    uint8_t c;

    for (r = 0; r < MATRIX_ROWS; r++) {
        if (r != 0) {
            pin_write(row_pins[r - 1].port, row_pins[r - 1].bit, 1);
        }
        pin_write(row_pins[r].port, row_pins[r].bit, 0);

        /* Give the column pull-up and the wiring time to settle. */
        mDelayuS(2);

        for (c = 0; c < MATRIX_COLS; c++) {
            uint8_t pressed = pin_read(col_pins[c].port, col_pins[c].bit) ? 0 : 1;
            uint8_t index = (uint8_t)(r * MATRIX_COLS + c);

            state[index] = pressed;
            raw_state[index] = pressed;
        }
    }

    pin_write(row_pins[MATRIX_ROWS - 1].port, row_pins[MATRIX_ROWS - 1].bit, 1);
#else
    uint8_t i;

    for (i = 0; i < DIRECT_KEY_COUNT; i++) {
        uint8_t pressed = pin_read(direct_pins[i].port, direct_pins[i].bit) ? 0 : 1;

        state[i] = pressed;
        raw_state[i] = pressed;
    }
#endif
}

uint8_t matrix_row_bits(uint8_t row)
{
    uint8_t bits = 0;
    uint8_t c;

#if KEYBOARD_MODE == KEYBOARD_MODE_MATRIX
    if (row >= MATRIX_ROWS) {
        return 0;
    }
    for (c = 0; c < MATRIX_COLS && c < 8; c++) {
        if (raw_state[row * MATRIX_COLS + c]) {
            bits |= (uint8_t)(1u << c);
        }
    }
#else
    if (row != 0) {
        return 0;
    }
    for (c = 0; c < DIRECT_KEY_COUNT && c < 8; c++) {
        if (raw_state[c]) {
            bits |= (uint8_t)(1u << c);
        }
    }
#endif

    return bits;
}
