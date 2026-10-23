/*==========================================================================*
 * matrix.h - key matrix / direct-pin scanner
 *==========================================================================*/

#ifndef __MATRIX_H__
#define __MATRIX_H__

#include <stdint.h>
#include "keyboard_config.h"

typedef struct {
    uint8_t port; /* 1 or 3 */
    uint8_t bit;  /* 0..7 */
} pin_t;

void matrix_init(void);

/* Fill state[0..KEY_COUNT-1] with 1 for pressed, 0 for released.
 * The caller supplies a debounce layer on top of this raw scan. */
void matrix_read(uint8_t __xdata *state);

/* Raw row bits from the most recent scan, used by VIA. */
uint8_t matrix_row_bits(uint8_t row);

#endif /* __MATRIX_H__ */
