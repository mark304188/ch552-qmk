/*==========================================================================*
 * debug.h - clock setup, coarse delays and the 1 ms system tick
 *==========================================================================*/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdint.h>

void CfgFsys(void);
void mDelayuS(uint16_t n);
void mDelaymS(uint16_t n);

/* Timer0 free-running millisecond counter. */
void     timer_init(void);
uint16_t timer_ms(void);

#endif /* __DEBUG_H__ */
