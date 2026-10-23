/*==========================================================================*
 * keycodes.h - QMK/VIA compatible 16-bit keycode values
 *
 * The numeric values are the ones QMK uses, so keycodes sent by VIA can be
 * stored and interpreted without any translation table.  Only the subset
 * supported by this small firmware is listed; unknown values are ignored.
 *==========================================================================*/

#ifndef __KEYCODES_H__
#define __KEYCODES_H__

/* Basic keycodes (HID Keyboard/Keypad usage page, 0x00-0xFF). */
#define KC_NO               0x0000
#define KC_TRNS             0x0001
#define KC_TRANSPARENT      0x0001
#define XXXXXXX             KC_NO
#define _______             KC_TRNS

#define KC_A                0x0004
#define KC_B                0x0005
#define KC_C                0x0006
#define KC_D                0x0007
#define KC_E                0x0008
#define KC_F                0x0009
#define KC_G                0x000A
#define KC_H                0x000B
#define KC_I                0x000C
#define KC_J                0x000D
#define KC_K                0x000E
#define KC_L                0x000F
#define KC_M                0x0010
#define KC_N                0x0011
#define KC_O                0x0012
#define KC_P                0x0013
#define KC_Q                0x0014
#define KC_R                0x0015
#define KC_S                0x0016
#define KC_T                0x0017
#define KC_U                0x0018
#define KC_V                0x0019
#define KC_W                0x001A
#define KC_X                0x001B
#define KC_Y                0x001C
#define KC_Z                0x001D

#define KC_1                0x001E
#define KC_2                0x001F
#define KC_3                0x0020
#define KC_4                0x0021
#define KC_5                0x0022
#define KC_6                0x0023
#define KC_7                0x0024
#define KC_8                0x0025
#define KC_9                0x0026
#define KC_0                0x0027

#define KC_ENTER            0x0028
#define KC_ENT              0x0028
#define KC_ESCAPE           0x0029
#define KC_ESC              0x0029
#define KC_BACKSPACE        0x002A
#define KC_BSPC             0x002A
#define KC_TAB              0x002B
#define KC_SPACE            0x002C
#define KC_SPC              0x002C
#define KC_MINUS            0x002D
#define KC_MINS             0x002D
#define KC_EQUAL            0x002E
#define KC_EQL              0x002E
#define KC_LEFT_BRACKET     0x002F
#define KC_LBRC             0x002F
#define KC_RIGHT_BRACKET    0x0030
#define KC_RBRC             0x0030
#define KC_BACKSLASH        0x0031
#define KC_BSLS             0x0031
#define KC_NONUS_HASH       0x0032
#define KC_NUHS             0x0032
#define KC_SEMICOLON        0x0033
#define KC_SCLN             0x0033
#define KC_QUOTE            0x0034
#define KC_QUOT             0x0034
#define KC_GRAVE            0x0035
#define KC_GRV              0x0035
#define KC_COMMA            0x0036
#define KC_COMM             0x0036
#define KC_DOT              0x0037
#define KC_SLASH            0x0038
#define KC_SLSH             0x0038
#define KC_CAPS_LOCK        0x0039
#define KC_CAPS             0x0039

#define KC_F1               0x003A
#define KC_F2               0x003B
#define KC_F3               0x003C
#define KC_F4               0x003D
#define KC_F5               0x003E
#define KC_F6               0x003F
#define KC_F7               0x0040
#define KC_F8               0x0041
#define KC_F9               0x0042
#define KC_F10              0x0043
#define KC_F11              0x0044
#define KC_F12              0x0045

#define KC_PRINT_SCREEN     0x0046
#define KC_PSCR             0x0046
#define KC_SCROLL_LOCK      0x0047
#define KC_SCRL             0x0047
#define KC_PAUSE            0x0048
#define KC_PAUS             0x0048
#define KC_INSERT           0x0049
#define KC_INS              0x0049
#define KC_HOME             0x004A
#define KC_PAGE_UP          0x004B
#define KC_PGUP             0x004B
#define KC_DELETE           0x004C
#define KC_DEL              0x004C
#define KC_END              0x004D
#define KC_PAGE_DOWN        0x004E
#define KC_PGDN             0x004E
#define KC_RIGHT            0x004F
#define KC_RGHT             0x004F
#define KC_LEFT             0x0050
#define KC_DOWN             0x0051
#define KC_UP               0x0052

#define KC_NUM_LOCK         0x0053
#define KC_NUM              0x0053
#define KC_KP_SLASH         0x0054
#define KC_PSLS             0x0054
#define KC_KP_ASTERISK      0x0055
#define KC_PAST             0x0055
#define KC_KP_MINUS         0x0056
#define KC_PMNS             0x0056
#define KC_KP_PLUS          0x0057
#define KC_PPLS             0x0057
#define KC_KP_ENTER         0x0058
#define KC_PENT             0x0058
#define KC_KP_1             0x0059
#define KC_KP_2             0x005A
#define KC_KP_3             0x005B
#define KC_KP_4             0x005C
#define KC_KP_5             0x005D
#define KC_KP_6             0x005E
#define KC_KP_7             0x005F
#define KC_KP_8             0x0060
#define KC_KP_9             0x0061
#define KC_KP_0             0x0062
#define KC_KP_DOT           0x0063
#define KC_KP_EQUAL         0x0067

/* Media / system keycodes.  QMK keeps these in the basic range, but they
 * are transmitted through the Consumer Control report. */
#define KC_SYSTEM_POWER     0x00A5
#define KC_PWR              0x00A5
#define KC_SYSTEM_SLEEP     0x00A6
#define KC_SLEP             0x00A6
#define KC_SYSTEM_WAKE      0x00A7
#define KC_WAKE             0x00A7
#define KC_AUDIO_MUTE       0x00A8
#define KC_MUTE             0x00A8
#define KC_AUDIO_VOL_UP     0x00A9
#define KC_VOLU             0x00A9
#define KC_AUDIO_VOL_DOWN   0x00AA
#define KC_VOLD             0x00AA
#define KC_MEDIA_NEXT_TRACK 0x00AB
#define KC_MNXT             0x00AB
#define KC_MEDIA_PREV_TRACK 0x00AC
#define KC_MPRV             0x00AC
#define KC_MEDIA_STOP       0x00AD
#define KC_MSTP             0x00AD
#define KC_MEDIA_PLAY_PAUSE 0x00AE
#define KC_MPLY             0x00AE
#define KC_MEDIA_SELECT     0x00AF
#define KC_MSEL             0x00AF
#define KC_MEDIA_EJECT      0x00B0
#define KC_EJCT             0x00B0
#define KC_MAIL             0x00B1
#define KC_CALCULATOR       0x00B2
#define KC_MY_COMPUTER      0x00B3
#define KC_WWW_SEARCH       0x00B4
#define KC_WWW_HOME         0x00B5
#define KC_WWW_BACK         0x00B6
#define KC_WWW_FORWARD      0x00B7
#define KC_WWW_STOP         0x00B8
#define KC_WWW_REFRESH      0x00B9
#define KC_WWW_FAVORITES    0x00BA
#define KC_MEDIA_FAST_FORWARD 0x00BB
#define KC_MFFD             0x00BB
#define KC_MEDIA_REWIND     0x00BC
#define KC_MRWD             0x00BC
#define KC_BRIGHTNESS_UP    0x00BD
#define KC_BRIU             0x00BD
#define KC_BRIGHTNESS_DOWN  0x00BE
#define KC_BRID             0x00BE

/* Modifier keycodes (HID usage 0xE0-0xE7). */
#define KC_LEFT_CTRL        0x00E0
#define KC_LCTL             0x00E0
#define KC_LEFT_SHIFT       0x00E1
#define KC_LSFT             0x00E1
#define KC_LEFT_ALT         0x00E2
#define KC_LALT             0x00E2
#define KC_LEFT_GUI         0x00E3
#define KC_LGUI             0x00E3
#define KC_RIGHT_CTRL       0x00E4
#define KC_RCTL             0x00E4
#define KC_RIGHT_SHIFT      0x00E5
#define KC_RSFT             0x00E5
#define KC_RIGHT_ALT        0x00E6
#define KC_RALT             0x00E6
#define KC_RIGHT_GUI        0x00E7
#define KC_RGUI             0x00E7

/* Quantum keycodes.  Both the current QMK range and the older range used
 * by some VIA builds are accepted by the key processor. */
#define QK_LAYER_TAP        0x4000
#define QK_LAYER_MOD        0x5000
#define QK_TO               0x5200
#define QK_MOMENTARY        0x5220
#define QK_DEF_LAYER        0x5240
#define QK_TOGGLE_LAYER     0x5260
#define QK_ONE_SHOT_LAYER   0x5280
#define QK_ONE_SHOT_MOD     0x52A0
#define QK_LAYER_TAP_TOGGLE 0x52C0

#define MO(layer)           (0x5220 | ((layer) & 0x1F))
#define TG(layer)           (0x5260 | ((layer) & 0x1F))
#define TO(layer)           (0x5200 | ((layer) & 0x1F))
#define DF(layer)           (0x5240 | ((layer) & 0x1F))
#define OSL(layer)          (0x5280 | ((layer) & 0x1F))

#define QK_BOOTLOADER       0x7C00
#define QK_REBOOT           0x7C01
#define QK_CLEAR_EEPROM     0x7C03

#endif /* __KEYCODES_H__ */
