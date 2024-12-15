/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"
#include "SDL_events.h"
#include "../../events/SDL_keyboard_c.h"

#include <cell/keyboard.h>

#include "SDL_PS3keyboard_c.h"
#include "SDL_PS3video.h"

// Copied from PSL1GHT because it's opaque in Cell SDK
/*! \brief Keyboard Modifier Key State. */
typedef struct KbMkey {
	union {
		u32 mkeys;
		struct {
		u32 reserved	: 24;	/*!< \brief Reserved MSB */
		u32 r_win		: 1;	/*!< \brief Modifier Key Right WIN 0:OFF 1:ON Bit7 */
		u32 r_alt		: 1;	/*!< \brief Modifier Key Right ALT 0:OFF 1:ON Bit6 */
		u32 r_shift		: 1;	/*!< \brief Modifier Key Right SHIFT 0:OFF 1:ON Bit5 */		
		u32 r_ctrl		: 1;	/*!< \brief Modifier Key Right CTRL 0:OFF 1:ON Bit4 */
		u32 l_win		: 1;	/*!< \brief Modifier Key Left WIN 0:OFF 1:ON Bit3 */
		u32 l_alt		: 1;	/*!< \brief Modifier Key Left ALT 0:OFF 1:ON Bit2 */
		u32 l_shift		: 1;	/*!< \brief Modifier Key Left SHIFT 0:OFF 1:ON Bit1 */
		u32 l_ctrl		: 1;	/*!< \brief Modifier Key Left CTRL 0:OFF 1:ON Bit0 LSB */
		/* For Macintosh Keyboard ALT & WIN correspond respectively to OPTION & APPLE keys */
		}_KbMkeyS;
	}_KbMkeyU;
} KbMkey;

/*! \brief Keyboard Led State. */
typedef struct KbLed {
	union {
		u32 leds;
		struct {
		u32 reserved	: 27;	/*!< \brief Reserved MSB */
		u32 kana		: 1;	/*!< \brief LED Kana 0:OFF 1:ON Bit4 */
		u32 compose		: 1;	/*!< \brief LED Compose 0:OFF 1:ON Bit3 */
		u32 scroll_lock	: 1;	/*!< \brief LED Scroll Lock 0:OFF 1:ON Bit2 */
		u32 caps_lock	: 1;	/*!< \brief LED Caps Lock 0:OFF 1:ON Bit1 */
		u32 num_lock	: 1;	/*!< \brief LED Num Lock 0:OFF 1:ON Bit0 LSB */
		}_KbLedS;
	}_KbLedU;
} KbLed;

static void unicodeToUtf8(Uint16 w, char *utf8buf)
{
    unsigned char *utf8s = (unsigned char *) utf8buf;

    if ( w < 0x0080 ) {
        utf8s[0] = ( unsigned char ) w;
        utf8s[1] = 0;
    }
    else if ( w < 0x0800 ) {
        utf8s[0] = 0xc0 | (( w ) >> 6 );
        utf8s[1] = 0x80 | (( w ) & 0x3f );
        utf8s[2] = 0;
    }
    else {
        utf8s[0] = 0xe0 | (( w ) >> 12 );
        utf8s[1] = 0x80 | (( ( w ) >> 6 ) & 0x3f );
        utf8s[2] = 0x80 | (( w ) & 0x3f );
        utf8s[3] = 0;
    }
}

static void updateKeymap(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    SDL_Scancode scancode;
    SDL_Keycode keymap[SDL_NUM_SCANCODES];
    CellKbConfig kbConfig;
    KbMkey kbMkey;
    KbLed kbLed;
    Uint16 unicode;

    SDL_GetDefaultKeymap(keymap);

    cellKbGetConfiguration(0, &kbConfig);

    data->_keyboardMapping = kbConfig.arrange;

    kbMkey._KbMkeyU.mkeys = 0;
    kbLed._KbLedU.leds = 1; // Num lock

    // Update SDL keycodes according to the keymap
    for (scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {

        // Make sure this scancode is a valid character scancode
        if (scancode == SDL_SCANCODE_UNKNOWN ||
            scancode == SDL_SCANCODE_ESCAPE ||
            scancode == SDL_SCANCODE_RETURN ||
            (keymap[scancode] & SDLK_SCANCODE_MASK)) {
            continue;
        }

		unicode = cellKbCnvRawCode(data->_keyboardMapping, kbMkey._KbMkeyU.mkeys, kbLed._KbLedU.leds, scancode);

        // Ignore Keypad flag
        unicode &= ~CELL_KB_KEYPAD;

        // Exclude raw keys
        if (unicode != 0 && unicode < CELL_KB_RAWDAT) {
            keymap[scancode] = unicode;
        }
    }
    SDL_SetKeymap(0, keymap, SDL_NUM_SCANCODES);
}

static void checkKeyboardConnected(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    CellKbInfo kbInfo;
    cellKbGetInfo(&kbInfo);

    if (kbInfo.status[0] == 1 && !data->_keyboardConnected) // Connected
    {
        data->_keyboardConnected = true;

        // Old events in the queue are discarded
        cellKbClearBuf(0);

        // Set raw keyboard code types to get scan codes
        cellKbSetCodeType(0, CELL_KB_CODETYPE_RAW);
        cellKbSetReadMode(0, CELL_KB_RMODE_INPUTCHAR);

        updateKeymap(_this);
    }
    else if (kbInfo.status[0] != 1 && data->_keyboardConnected) // Disconnected
    {
        data->_keyboardConnected = false;

        SDL_ResetKeyboard();
    }
}

static void updateModifierKey(bool oldState, bool newState, SDL_Scancode scancode)
{
    if (!oldState ^ !newState) {
        SDL_SendKeyboardKey(newState ? SDL_PRESSED : SDL_RELEASED, scancode);
    }
}

static void updateModifiers(_THIS, const CellKbData *Keys)
{
    SDL_Keymod modstate = SDL_GetModState();

    updateModifierKey(modstate & KMOD_LSHIFT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_shift, SDL_SCANCODE_LSHIFT);
    updateModifierKey(modstate & KMOD_RSHIFT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_shift, SDL_SCANCODE_RSHIFT);
    updateModifierKey(modstate & KMOD_LCTRL, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_ctrl, SDL_SCANCODE_LCTRL);
    updateModifierKey(modstate & KMOD_RCTRL, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_ctrl, SDL_SCANCODE_RCTRL);
    updateModifierKey(modstate & KMOD_LALT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_alt, SDL_SCANCODE_LALT);
    updateModifierKey(modstate & KMOD_RALT, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_alt, SDL_SCANCODE_RALT);
    updateModifierKey(modstate & KMOD_LGUI, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.l_win, SDL_SCANCODE_LGUI);
    updateModifierKey(modstate & KMOD_RGUI, ((KbMkey*)(&Keys->mkey))->_KbMkeyU._KbMkeyS.r_win, SDL_SCANCODE_RGUI);
}

static void updateKeys(_THIS, const CellKbData *Keys)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    int x = 0;
    int numKeys = 0;
    Uint8 newkeystate[SDL_NUM_SCANCODES];
    const Uint8 * keystate = SDL_GetKeyboardState(&numKeys);
    Uint16 unicode;
    SDL_Scancode scancode;

    for (scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {
        newkeystate[scancode] = SDL_RELEASED;
    }

    for (x = 0; x < Keys->len; x++) {
        if (Keys->keycode[0] != 0)
            newkeystate[Keys->keycode[x]] = SDL_PRESSED;
    }

    for (scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {
        if ((newkeystate[scancode] != keystate[scancode])
                && (scancode < SDL_SCANCODE_LCTRL || scancode > SDL_SCANCODE_RGUI)) {

            // Send new key state
            SDL_SendKeyboardKey(newkeystate[scancode], scancode);

            // Send the text corresponding to the keypress
            if (newkeystate[scancode] == SDL_PRESSED) {
                // Convert scancode
                unicode = cellKbCnvRawCode(data->_keyboardMapping, Keys->mkey, Keys->led, scancode);

                // Ignore Keypad flag
                unicode &= ~CELL_KB_KEYPAD;

                // Exclude raw keys
                if (unicode != 0 && unicode < CELL_KB_RAWDAT) {
                    char utf8[SDL_TEXTINPUTEVENT_TEXT_SIZE];

                    // Convert from Unicode to UTF-8
                    unicodeToUtf8(unicode, utf8);
                    SDL_SendKeyboardText(utf8);
                }
            }
        }
    }
}

void
PS3_PumpKeyboard(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    checkKeyboardConnected(_this);

    if (data->_keyboardConnected) {
        CellKbData Keys;

        // Read data from the keyboard buffer
        if (cellKbRead(0, &Keys) == 0 && Keys.len > 0) {
            updateModifiers(_this, &Keys);
            updateKeys(_this, &Keys);
        }
    }
}

void
PS3_InitKeyboard(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    // Init the PS3 Keyboard
    cellKbInit(1);

    data->_keyboardConnected = false;
}

void
PS3_QuitKeyboard(_THIS)
{
    cellKbEnd();
}

/* vi: set ts=4 sw=4 expandtab: */
