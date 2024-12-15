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
#include "../../events/SDL_mouse_c.h"

#include <cell/mouse.h>

#include "SDL_PS3mouse_c.h"

void checkMouseConnected(_THIS) {
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    CellMouseInfo mouseinfo;
    cellMouseGetInfo(&mouseinfo);

    if (mouseinfo.status[0] == 1 && !data->_mouseConnected) // Connected
    {
        data->_mouseConnected = true;
        data->_mouseButtons = 0;

        // Old events in the queue are discarded
        cellMouseClearBuf(0);
    }
    else if (mouseinfo.status[0] != 1 && data->_mouseConnected) // Disconnected
    {
        data->_mouseConnected = false;
        data->_mouseButtons = 0;
    }
}

void updateMouseButtons(_THIS, const CellMouseData *mouse) {
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;
    // There should only be one window
    SDL_Window *window = _this->windows;

    // Check left mouse button changes
    bool oldLMB = data->_mouseButtons & 1;
    bool newLMB = mouse->buttons & 1;
    if (newLMB != oldLMB) {
        SDL_SendMouseButton(window, 0, newLMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
    }

    // Check rigth mouse button changes
    bool oldRMB = data->_mouseButtons & 2;
    bool newRMB = mouse->buttons & 2;
    if (newRMB != oldRMB) {
        SDL_SendMouseButton(window, 0, newRMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_RIGHT);
    }

    // Check middle mouse button changes
    bool oldMMB = data->_mouseButtons & 4;
    bool newMMB = mouse->buttons & 4;
    if (newMMB != oldMMB) {
        SDL_SendMouseButton(window, 0, newMMB ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_MIDDLE);
    }

    data->_mouseButtons = mouse->buttons;
}

void updateMousePosition(_THIS, const CellMouseData *mouse) {
    // There should only be one window
    SDL_Window *window = _this->windows;

    // Mouse movement is relative
    SDL_SendMouseMotion(window, 0, 1, mouse->x_axis, mouse->y_axis);
}

void updateMouseWheel(_THIS, const CellMouseData *mouse) {
    // There should only be one window
    SDL_Window *window = _this->windows;

    SDL_SendMouseWheel(window, 0, mouse->tilt, mouse->wheel, SDL_MOUSEWHEEL_NORMAL);
}

void
PS3_PumpMouse(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    // Check if a mouse has been connected / disconnected
    checkMouseConnected(_this);

    if (data->_mouseConnected)
    {
        CellMouseDataList datalist;
        cellMouseGetDataList(0, &datalist);

        int i;
        for (i = 0; i < datalist.list_num; i++) {
            // Send SDL events
            updateMouseButtons(_this, &datalist.list[i]);
            updateMousePosition(_this, &datalist.list[i]);
            updateMouseWheel(_this, &datalist.list[i]);
        }
    }
}

void
PS3_InitMouse(_THIS)
{
    SDL_DeviceData *data =
        (SDL_DeviceData *) _this->driverdata;

    // Support only one mouse for now
    cellMouseInit(1);

    data->_mouseConnected = false;
}

void
PS3_QuitMouse(_THIS)
{
    cellMouseEnd();
}

/* vi: set ts=4 sw=4 expandtab: */
