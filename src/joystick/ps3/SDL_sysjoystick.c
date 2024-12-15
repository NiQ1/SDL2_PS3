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

#ifdef SDL_JOYSTICK_PS3

/* This is the system specific header for the SDL joystick API */

#include "SDL_events.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include <cell/pad/libpad.h>
#include "SDL_PS3_PadData.h"

#define pdprintf(x) printf(x)

#define NAMESIZE 10

typedef struct SDL_PS3_JoyData
{
	char name[NAMESIZE];
} SDL_PS3_JoyData;

struct joystick_hwdata
{
	padData old_pad_data;
};

static SDL_PS3_JoyData joy_data[CELL_MAX_PADS];
int numberOfJoysticks = 0;

void SDL_SYS_JoystickDetect(void);

/* Function to scan the system for joysticks.
 * This function should set numberOfJoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
	int iReturn = 0;
    numberOfJoysticks = CELL_MAX_PADS;

	pdprintf("SDL_SYS_JoystickInit\n");

	SDL_zero( joy_data);

	if( iReturn == 0)
	{
		iReturn =  cellPadInit(CELL_MAX_PADS) ;
		pdprintf("\tPad initialized\n");
		if( iReturn != 0)
		{
			SDL_SetError("SDL_SYS_JoystickInit() : Couldn't initialize PS3 pads");
		}
	}
	
	SDL_SYS_JoystickDetect();

	return numberOfJoysticks;	
}

int
SDL_SYS_JoystickGetCount(void)
{
    return numberOfJoysticks;
}

void
SDL_SYS_JoystickDetect(void)
{
	CellPadInfo2 padinfo;

	int iReturn = cellPadGetInfo2(&padinfo);
	pdprintf("\tGot info\n");
	if( iReturn != 0)
	{
		SDL_SetError("SDL_SYS_JoystickInit() : Couldn't get PS3 pads information ");
	}

	if( iReturn == 0)
	{
		unsigned int i;
		numberOfJoysticks = padinfo.now_connect;

		for(i = 0; i < padinfo.now_connect; i++)
		{
			if( padinfo.port_status[i])
			{
				sprintf( joy_data[i].name, "PAD%02X", i);
			}
		} 
	}
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickGetDeviceName(int device_index)
{
	char * name = NULL;
	if (device_index < numberOfJoysticks)
		name = "PS1LIGHT Controller";
	else
		SDL_SetError("No joystick available with that index");
    return name;
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID
SDL_SYS_JoystickGetDevicePlayerIndex(int device_index)
{
    return device_index;
}

void
SDL_SYS_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
	
}

SDL_JoystickID
SDL_SYS_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index;
}

SDL_JoystickGUID
SDL_SYS_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SDL_SYS_JoystickGetDeviceName( device_index );
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    if (!(joystick->hwdata = (struct joystick_hwdata*)SDL_malloc(sizeof(struct joystick_hwdata))))
    {
        return -1;
    }

	joystick->naxes = 4;
	joystick->nhats = 0;
	joystick->nballs = 0;
	joystick->nbuttons = 16;

    return 0;
}

// TODO: No excuse not to support this.
static int
SDL_SYS_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

#define CheckPS3Axis( btn, bnum) \
	if( new_pad_data.btn != joystick->hwdata->old_pad_data.btn) {\
		SDL_PrivateJoystickAxis( joystick, (bnum), ((new_pad_data.btn-0x80)<<8)|new_pad_data.btn); \
	} \
	joystick->hwdata->old_pad_data.btn = new_pad_data.btn;

#define CheckPS3Button( btn, bnum) \
	if( new_pad_data.btn != joystick->hwdata->old_pad_data.btn) {\
		if( new_pad_data.btn == 0) \
		SDL_PrivateJoystickButton( joystick, (bnum), SDL_RELEASED); \
		else \
		SDL_PrivateJoystickButton( joystick, (bnum), SDL_PRESSED); \
	} \
	joystick->hwdata->old_pad_data.btn = new_pad_data.btn;


/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
	padData new_pad_data;
	int joystickIndex = SDL_JoystickGetDeviceIndexFromInstanceID(joystick->instance_id);
	
	if (cellPadGetData(joystickIndex, (CellPadData*)&new_pad_data) != 0)
	{
		SDL_SetError("No joystick available with that index");
	}
	else if(new_pad_data.len >= 8)
	{
		// Update axes
		CheckPS3Axis( ANA_L_H, 0);
		CheckPS3Axis( ANA_L_V, 1);
		CheckPS3Axis( ANA_R_H, 2);
		CheckPS3Axis( ANA_R_V, 3);
		
		// Update buttons
		CheckPS3Button( BTN_LEFT, 0);
		CheckPS3Button( BTN_DOWN, 1);
		CheckPS3Button( BTN_RIGHT, 2);
		CheckPS3Button( BTN_UP, 3);

		CheckPS3Button( BTN_START, 4);
		CheckPS3Button( BTN_R3, 5);
		CheckPS3Button( BTN_L3, 6);
		CheckPS3Button( BTN_SELECT, 7);

		CheckPS3Button( BTN_SQUARE, 8);
		CheckPS3Button( BTN_CROSS, 9);
		CheckPS3Button( BTN_CIRCLE, 10);
		CheckPS3Button( BTN_TRIANGLE, 11);

		CheckPS3Button( BTN_R1, 12);
		CheckPS3Button( BTN_L1, 13);
		CheckPS3Button( BTN_R2, 14);
		CheckPS3Button( BTN_L2, 15);
	}

    return;
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata)
        SDL_free(joystick->hwdata);
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    numberOfJoysticks = 0;
}

static SDL_bool
SDL_SYS_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
	return SDL_FALSE;
}

SDL_JoystickDriver SDL_PS3_JoystickDriver =
{
    SDL_SYS_JoystickInit,
    SDL_SYS_JoystickGetCount,
    SDL_SYS_JoystickDetect,
    SDL_SYS_JoystickGetDeviceName,
    SDL_SYS_JoystickGetDevicePlayerIndex,
    SDL_SYS_JoystickSetDevicePlayerIndex,
    SDL_SYS_JoystickGetDeviceGUID,
    SDL_SYS_JoystickGetDeviceInstanceID,
    SDL_SYS_JoystickOpen,
    SDL_SYS_JoystickRumble,
    SDL_SYS_JoystickUpdate,
    SDL_SYS_JoystickClose,
    SDL_SYS_JoystickQuit,
    SDL_SYS_JoystickGetGamepadMapping,
};

#endif /* SDL_JOYSTICK_PS3 */

/* vi: set ts=4 sw=4 expandtab: */
