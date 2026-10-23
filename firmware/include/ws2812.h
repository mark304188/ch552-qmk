/*==========================================================================*
 * ws2812.h - WS2812 output through the CH552 SPI0 peripheral
 *==========================================================================*/

#ifndef __WS2812_H__
#define __WS2812_H__

#include <stdint.h>
#include "keyboard_config.h"

void ws2812_init(void);

/* rgb[] is WS2812_LED_COUNT * 3 bytes, already in wire order
 * (GRB when WS2812_GRB is 1).  Interrupts are masked while the
 * bit-expanded stream is shifted out. */
void ws2812_show(const uint8_t __xdata *rgb, uint8_t count);

#endif /* __WS2812_H__ */
