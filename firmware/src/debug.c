/*==========================================================================*
 * debug.c - system clock, delays and the 1 ms tick
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "debug.h"
#include "keyboard_config.h"

/* Timer0 runs from Fsys/12 (standard 8051 clock).  Mode 1 is reloaded by
 * the ISR, which is accurate enough for debounce and RGB animation. */
#define TIMER0_RELOAD ((uint16_t)(65536UL - (FREQ_SYS / 12UL / 1000UL)))

volatile __xdata uint16_t g_ms;

void CfgFsys(void)
{
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;

#if FREQ_SYS == 32000000
    CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | SYS_CK_32M;
#elif FREQ_SYS == 24000000
    CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | SYS_CK_24M;
#elif FREQ_SYS == 16000000
    CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | SYS_CK_16M;
#elif FREQ_SYS == 12000000
    CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | SYS_CK_12M;
#elif FREQ_SYS == 6000000
    CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | SYS_CK_6M;
#else
#error "FREQ_SYS must be 24/16/12/6 MHz"
#endif

    SAFE_MOD = 0x00;
}

/* Coarse "at least" delay, same style as the WCH examples.  Do not use
 * for WS2812 timing; the LED driver uses hardware SPI. */
void mDelayuS(uint16_t n)
{
    while (n--) {
#if FREQ_SYS >= 24000000
        SAFE_MOD++; SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;
        SAFE_MOD++; SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;
#elif FREQ_SYS >= 12000000
        SAFE_MOD++; SAFE_MOD++; SAFE_MOD++; SAFE_MOD++;
#else
        SAFE_MOD++; SAFE_MOD++;
#endif
    }
}

void mDelaymS(uint16_t n)
{
    while (n--) {
        mDelayuS(1000);
    }
}

void timer_init(void)
{
    g_ms = 0;

    /* Timer0 mode 1, 16-bit, standard Fsys/12 clock. */
    T2MOD &= ~bT0_CLK;
    TMOD = (TMOD & 0xF0) | 0x01;
    TH0 = (uint8_t)(TIMER0_RELOAD >> 8);
    TL0 = (uint8_t)(TIMER0_RELOAD & 0xFF);
    TR0 = 1;
    ET0 = 1;
}

void timer0_isr(void) __interrupt(INT_NO_TMR0) __using(2)
{
    TH0 = (uint8_t)(TIMER0_RELOAD >> 8);
    TL0 = (uint8_t)(TIMER0_RELOAD & 0xFF);
    g_ms++;
}

uint16_t timer_ms(void)
{
    uint16_t value;
    uint8_t  ea = EA;

    /* The ISR can update the 16-bit value between the two byte reads. */
    EA = 0;
    value = g_ms;
    EA = ea;
    return value;
}
