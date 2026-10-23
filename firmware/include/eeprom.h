/*==========================================================================*
 * eeprom.h - CH552 Data Flash access (128 bytes, offsets 0x00-0x7F)
 *==========================================================================*/

#ifndef __EEPROM_H__
#define __EEPROM_H__

#include <stdint.h>

/* Data Flash layout.  Keep the keymap below the RGB block at 0x78. */
#define EE_KEYMAP_BASE      0x08
#define EE_LAYOUT_LIMIT     0x78
#define EE_RGB_OFFSET       0x78

uint8_t ee_read_byte(uint8_t offset);
void    ee_write_byte(uint8_t offset, uint8_t value);
void    ee_read_block(uint8_t offset, uint8_t *dst, uint8_t len);
void    ee_write_block(uint8_t offset, const uint8_t *src, uint8_t len);

#endif /* __EEPROM_H__ */
