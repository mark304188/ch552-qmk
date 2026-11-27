/*==========================================================================*
 * vialrgb.h - Vial keyboard-ID + VialRGB lighting commands
 *
 * OpenRGB's "QMK VialRGB Device" driver talks to the normal QMK/VIA raw
 * HID interface.  This is only the small subset needed for OpenRGB:
 *
 *   0xFE 0x00   Vial keyboard ID (protocol version + UID + VialRGB flag)
 *   0x08 0x40   VialRGB info
 *   0x08 0x41   get current lighting mode
 *   0x08 0x42   get supported effect IDs
 *   0x08 0x43   get LED count
 *   0x08 0x44   get LED position / matrix position
 *   0x07 0x41   set lighting mode
 *   0x07 0x42   direct-mode fast set (HSV triples)
 *
 * The normal VIA commands (dynamic keymap, layers, matrix state, ...) are
 * untouched, so VIA and OpenRGB can use the same 32-byte interface.
 *==========================================================================*/

#ifndef __VIALRGB_H__
#define __VIALRGB_H__

#include <stdint.h>

#define VIALRGB_GET_INFO        0x40
#define VIALRGB_GET_MODE        0x41
#define VIALRGB_GET_SUPPORTED   0x42
#define VIALRGB_GET_LED_COUNT   0x43
#define VIALRGB_GET_LED_INFO    0x44

#define VIALRGB_SET_MODE        0x41
#define VIALRGB_DIRECT_FASTSET  0x42

/* True for the VialRGB sub-commands that share VIA's custom lighting
 * command IDs.  VIA's own custom channels are 1, 2 and 3. */
uint8_t vialrgb_is_get_command(uint8_t subcommand);
uint8_t vialrgb_is_set_command(uint8_t subcommand);

/* VIA command 0xFE: Vial keyboard ID query. */
void vialrgb_handle_vial(uint8_t *data, uint8_t length) __reentrant;

/* VIA commands 0x08 / 0x07. */
void vialrgb_get_value(uint8_t *data, uint8_t length) __reentrant;
void vialrgb_set_value(uint8_t *data, uint8_t length) __reentrant;

#endif /* __VIALRGB_H__ */
