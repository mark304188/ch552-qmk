/*==========================================================================*
 * eeprom.c - CH552 Data Flash (128 bytes at 0xC000..0xC0FF)
 *
 * The datasheet exposes Data Flash through ROM_ADDR/ROM_DATA/ROM_CTRL.
 * Only even byte addresses are valid, so byte offset N is programmed at
 * 0xC000 + (N << 1).  Each byte can be written individually; no sector
 * erase is required for the small settings blob used here.
 *==========================================================================*/

#include <ch552.h>
#include <stdint.h>

#include "eeprom.h"

#define EE_ADDR(offset) ((uint16_t)(DATA_FLASH_ADDR + ((uint16_t)(offset) << 1)))

static void ee_write_enable(void)
{
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG |= bDATA_WE;
    SAFE_MOD = 0x00;
}

static void ee_write_disable(void)
{
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG &= ~bDATA_WE;
    SAFE_MOD = 0x00;
}

uint8_t ee_read_byte(uint8_t offset)
{
    if (offset > 0x7F) {
        return 0xFF;
    }

    ROM_ADDR = EE_ADDR(offset);
    ROM_CTRL = ROM_CMD_READ;
    return ROM_DATA_L;
}

void ee_write_byte(uint8_t offset, uint8_t value)
{
    uint8_t ea;

    if (offset > 0x7F) {
        return;
    }

    /* Safe mode has a short timing window; an interrupt between the
     * SAFE_MOD writes and the protected register access would abort it.
     * The whole byte program is short (well under one USB frame). */
    ea = EA;
    EA = 0;

    ee_write_enable();
    ROM_ADDR = EE_ADDR(offset);
    ROM_DATA_L = value;
    ROM_CTRL = ROM_CMD_WRITE;
    ee_write_disable();

    EA = ea;
}

void ee_read_block(uint8_t offset, uint8_t *dst, uint8_t len)
{
    while (len--) {
        *dst++ = ee_read_byte(offset++);
    }
}

void ee_write_block(uint8_t offset, const uint8_t *src, uint8_t len)
{
    while (len--) {
        ee_write_byte(offset++, *src++);
    }
}
