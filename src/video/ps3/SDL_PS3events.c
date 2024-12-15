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

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_PS3video.h"
#include "SDL_PS3events_c.h"
#include "SDL_PS3keyboard_c.h"
#include "SDL_PS3mouse_c.h"

#include <sysutil/sysutil_common.h>

static void eventHandle(u64 status, u64 param, void * userdata) {
    _THIS = (SDL_VideoDevice*)userdata;
    SDL_Window *window = NULL;
    
    // There should only be one window
    if (_this->num_displays == 1) {
        SDL_VideoDisplay *display = &_this->displays[0];
        if (display->fullscreen_window != NULL) {
            window = display->fullscreen_window;
        }
    }

    // Process event
    if (status == CELL_SYSUTIL_REQUEST_EXITGAME) {
	    deprintf(1, "Quit game requested\n");
	    SDL_SendQuit();
    } else if(status == CELL_SYSUTIL_SYSTEM_MENU_OPEN) {
	    // XMB opened
	    if (window) {
	        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_LEAVE, 0, 0);
	    }
    } else if(status == CELL_SYSUTIL_SYSTEM_MENU_CLOSE) {
		// XMB closed
	    if (window) {
	        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_ENTER, 0, 0);
	    }
    } else if(status == CELL_SYSUTIL_DRAWING_BEGIN) {
    } else if(status == CELL_SYSUTIL_DRAWING_END) {
    } else {
	    deprintf(1, "Unhandled event: %08llX\n", (unsigned long long int)status);
    }
}

void
PS3_PumpEvents(_THIS)
{
    cellSysutilCheckCallback();
    PS3_PumpKeyboard(_this);
    PS3_PumpMouse(_this);
}


void
PS3_InitSysEvent(_THIS)
{
    cellSysutilRegisterCallback(0, eventHandle, _this);
    PS3_InitKeyboard(_this);
    PS3_InitMouse(_this);
}

void
PS3_QuitSysEvent(_THIS)
{
    cellSysutilUnregisterCallback(0);
    PS3_QuitKeyboard(_this);
    PS3_QuitMouse(_this);
}

/* vi: set ts=4 sw=4 expandtab: */
