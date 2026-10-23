/*==========================================================================*
 * ws2812.c - WS2812 output for the CH552
 *
 * Two drivers are available:
 *
 *  - SPI0 driver (default 3x4 board): each WS2812 bit is encoded as three
 *    SPI bits (100 = 0, 110 = 1).  Pins are fixed: P1.5 = MOSI (data),
 *    P1.7 = SCK.  Stable timing, but P1.7 is occupied.
 *
 *  - GPIO bit-bang driver (4-key board): only P1.5 is used, so P1.7 stays
 *    free for a button.  The loop is calibrated for 12/16 MHz and based on
 *    the well-known ch554_sdcc WS2812 example.  24 MHz is rejected.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "ws2812.h"
#include "debug.h"

#if RGB_ENABLE && (WS2812_LED_COUNT > 0)

#if WS2812_LED_COUNT > 32
#error "WS2812_LED_COUNT too large for the 1 KB xRAM; keep it <= 32"
#endif

/*---------------------------------------------------------------*
 * GPIO bit-bang driver                                          *
 *---------------------------------------------------------------*/
#if WS2812_DRIVER_BITBANG

#if WS2812_PIN_PORT != 1
#error "The bit-bang WS2812 driver currently supports P1.x only"
#endif

#if FREQ_SYS > 16000000UL
#error "Bit-banged WS2812 supports 12/16 MHz only; use FREQ_SYS=12000000 or 16000000"
#endif

SBIT(ws2812_pin, 0x90, WS2812_PIN_BIT); /* P1.5 on the 4-key board */

#define WS_BB_MASK ((uint8_t)(1u << WS2812_PIN_BIT))

/* Keep the pointer in DPTR and count in R2.  The parameter layout is the
 * one SDCC uses for a small-model, non-reentrant function: the first
 * uint8_t arrives in DPL, the xdata pointer is in _PARM_2. */
static void ws2812_bitbang_send(uint8_t count, const uint8_t __xdata *data)
{
    (void)count;
    (void)data;

    __asm
    mov     r2, dpl
    mov     dpl, _ws2812_bitbang_send_PARM_2
    mov     dph, (_ws2812_bitbang_send_PARM_2 + 1)

00001$:
    /* Green byte */
    movx    a, @dptr
    inc     dptr
    mov     r3, #8
00002$:
    setb    _ws2812_pin
    nop
    nop
    rlc     a
    mov     _ws2812_pin, c
    nop
    nop
    clr     _ws2812_pin
    djnz    r3, 00002$

    /* Red byte */
    movx    a, @dptr
    inc     dptr
    mov     r3, #8
00003$:
    setb    _ws2812_pin
    nop
    nop
    rlc     a
    mov     _ws2812_pin, c
    nop
    nop
    clr     _ws2812_pin
    djnz    r3, 00003$

    /* Blue byte */
    movx    a, @dptr
    inc     dptr
    mov     r3, #8
00004$:
    setb    _ws2812_pin
    nop
    nop
    rlc     a
    mov     _ws2812_pin, c
    nop
    nop
    clr     _ws2812_pin
    djnz    r3, 00004$

    djnz    r2, 00001$
    __endasm;
}

void ws2812_init(void)
{
    P1_MOD_OC &= ~WS_BB_MASK; /* push-pull */
    P1_DIR_PU |= WS_BB_MASK;  /* output     */
    P1 &= ~WS_BB_MASK;        /* idle low   */
    mDelayuS(100);            /* reset      */
}

void ws2812_show(const uint8_t __xdata *rgb, uint8_t count)
{
    uint8_t ea;

    if (count > WS2812_LED_COUNT) {
        count = WS2812_LED_COUNT;
    }
    if (count == 0) {
        return;
    }

    ea = EA;
    EA = 0;
    ws2812_bitbang_send(count, rgb);
    EA = ea;

    mDelayuS(80); /* >50 us reset */
}

/*---------------------------------------------------------------*
 * SPI0 driver                                                   *
 *---------------------------------------------------------------*/
#else

#define WS_SPI_DIV ((uint8_t)(FREQ_SYS / 4800000UL))

static __xdata uint8_t encoded[WS2812_LED_COUNT * 9];

/* Expand one byte into three SPI bytes (24 encoded bits). */
static void encode_byte(uint8_t value, uint8_t __xdata *out)
{
    uint32_t bits = 0;
    uint8_t i;

    for (i = 0; i < 8; i++) {
        bits <<= 3;
        if (value & 0x80) {
            bits |= 0x06; /* 110 */
        } else {
            bits |= 0x04; /* 100 */
        }
        value <<= 1;
    }

    out[0] = (uint8_t)(bits >> 16);
    out[1] = (uint8_t)(bits >> 8);
    out[2] = (uint8_t)bits;
}

void ws2812_init(void)
{
    /* SCK and MOSI are push-pull outputs; both idle low. */
    P1_MOD_OC &= ~(uint8_t)(bSCK | bMOSI);
    P1_DIR_PU |= (uint8_t)(bSCK | bMOSI);
    P1 &= ~(uint8_t)(bSCK | bMOSI);

    SPI0_CTRL = bS0_CLR_ALL;
    SPI0_STAT = 0xFF;
    SPI0_SETUP = 0; /* master, MSB first */
    SPI0_CK_SE = WS_SPI_DIV ? WS_SPI_DIV : 1;
    SPI0_CTRL = bS0_SCK_OE | bS0_MOSI_OE; /* mode 0 */

    mDelayuS(100); /* WS2812 reset */
}

void ws2812_show(const uint8_t __xdata *rgb, uint8_t count)
{
    uint8_t led;
    uint8_t ea;
    uint16_t n;
    uint16_t i;

    if (count > WS2812_LED_COUNT) {
        count = WS2812_LED_COUNT;
    }
    if (count == 0) {
        return;
    }

    n = 0;
    for (led = 0; led < count; led++) {
        encode_byte(rgb[led * 3 + 0], &encoded[n]);
        encode_byte(rgb[led * 3 + 1], &encoded[n + 3]);
        encode_byte(rgb[led * 3 + 2], &encoded[n + 6]);
        n += 9;
    }

    /* The whole frame must be shifted without gaps. */
    ea = EA;
    EA = 0;

    SPI0_DATA = encoded[0];
    while (S0_FREE == 0)
        ;
    SPI0_DATA; /* drain receive FIFO */

    for (i = 1; i < n; i++) {
        SPI0_DATA = encoded[i];
        while (S0_FREE == 0)
            ;
        SPI0_DATA;
    }

    /* One extra zero byte leaves MOSI low for the reset period. */
    SPI0_DATA = 0x00;
    while (S0_FREE == 0)
        ;
    SPI0_DATA;

    EA = ea;

    mDelayuS(80); /* >50 us reset latch */
}

#endif /* WS2812_DRIVER_BITBANG */

#else

void ws2812_init(void)
{
}

void ws2812_show(const uint8_t __xdata *rgb, uint8_t count)
{
    (void)rgb;
    (void)count;
}

#endif
