/*==========================================================================*
 * openrgb.h - OpenRGB QMK protocol (Revision D) handler
 *==========================================================================*/

#ifndef __OPENRGB_H__
#define __OPENRGB_H__

#include <stdint.h>

void openrgb_handle(uint8_t *data, uint8_t length);

#endif /* __OPENRGB_H__ */
