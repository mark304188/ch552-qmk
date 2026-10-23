/*==========================================================================*
 * via.h - VIA raw HID protocol handler
 *==========================================================================*/

#ifndef __VIA_H__
#define __VIA_H__

#include <stdint.h>

#define VIA_PROTOCOL_VERSION 0x000D

void via_handle(uint8_t *data, uint8_t length) __reentrant;

#endif /* __VIA_H__ */
