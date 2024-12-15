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
#include "../../SDL_internal.h"

/* PS3 SDL video driver implementation, Based on PSL1GHT implementation,
 * itself based on dummy driver.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_PS3video.h"
#include "SDL_PS3events_c.h"
#include "SDL_PS3modes_c.h"


#include <stdlib.h>
#include <assert.h>

#include <cell/gcm.h>
#include <cell/resc.h>

#define PS3VID_DRIVER_NAME "ps3"

/* Initialization/Query functions */
static int PS3_VideoInit(_THIS);
static void PS3_VideoQuit(_THIS);

/* PS3GUI init functions : */
static void initializeGPU(SDL_DeviceData * devdata);

/* PS3 driver bootstrap functions */

static int
PS3_Available(void)
{
    return (1);
}

static void
PS3_DeleteDevice(SDL_VideoDevice * device)
{
    deprintf (1, "PS3_DeleteDevice( %p)\n", device);
    SDL_free(device);
}

int
PS3_VideoInit(_THIS)
{
    SDL_DeviceData *devdata = NULL;

    devdata = (SDL_DeviceData*) SDL_calloc(1, sizeof(SDL_DeviceData));
    if (devdata == NULL) { 
        /* memory allocation problem */  
        SDL_OutOfMemory();
        return -1;
    } 

    _this->driverdata = devdata;

    PS3_InitSysEvent(_this);

    initializeGPU(devdata);
    PS3_InitModes(_this);

    cellGcmSetFlipMode((enum CellRescFlipMode)CELL_RESC_DISPLAY_VSYNC); // Wait for VSYNC to flip

    /* We're done! */
    return 0;
}

void
PS3_VideoQuit(_THIS)
{
    deprintf (1, "PS3_VideoQuit()\n");
    PS3_QuitModes(_this);
    PS3_QuitSysEvent(_this);
    SDL_free( _this->driverdata);

}

void initializeGPU( SDL_DeviceData * devdata)
{
    deprintf (1, "initializeGPU()\n");
   // Allocate a 1Mb buffer, alligned to a 1Mb boundary to be our shared IO memory with the RSX.
    void *host_addr = memalign(1024*1024, 1024*1024);
    assert(host_addr != NULL);

    // Initilise Reality, which sets up the command buffer and shared IO memory
    devdata->_CommandBuffer = (CellGcmContextData*)cellGcmInit(0x10000, 1024*1024, host_addr);
    assert(devdata->_CommandBuffer != NULL);
}

int
PS3_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return SDL_OutOfMemory();
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

int
PS3_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return SDL_Unsupported();
}

void
PS3_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
PS3_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
PS3_SetWindowPosition(_THIS, SDL_Window * window)
{
}
void
PS3_SetWindowSize(_THIS, SDL_Window * window)
{
}
void
PS3_ShowWindow(_THIS, SDL_Window * window)
{
}
void
PS3_HideWindow(_THIS, SDL_Window * window)
{
}
void
PS3_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
PS3_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
PS3_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
PS3_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
PS3_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}
void
PS3_DestroyWindow(_THIS, SDL_Window * window)
{
}

SDL_bool PS3_HasScreenKeyboardSupport(_THIS)
{
    return SDL_FALSE;
}
void PS3_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
}
void PS3_HideScreenKeyboard(_THIS, SDL_Window *window)
{
}

SDL_bool PS3_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    return SDL_FALSE;
}

static SDL_VideoDevice *
PS3_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    deprintf (1, "PS3_CreateDevice( %16X)\n", devindex);

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device) {
        SDL_memset(device, 0, (sizeof *device));
    }
    else {
        SDL_OutOfMemory();
        SDL_free(device);
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = PS3_VideoInit;
    device->VideoQuit = PS3_VideoQuit;
    device->GetDisplayModes = PS3_GetDisplayModes;
    device->SetDisplayMode = PS3_SetDisplayMode;
    device->CreateSDLWindow = PS3_CreateWindow;
    device->CreateSDLWindowFrom = PS3_CreateWindowFrom;
    device->SetWindowTitle = PS3_SetWindowTitle;
    device->SetWindowIcon = PS3_SetWindowIcon;
    device->SetWindowPosition = PS3_SetWindowPosition;
    device->SetWindowSize = PS3_SetWindowSize;
    device->ShowWindow = PS3_ShowWindow;
    device->HideWindow = PS3_HideWindow;
    device->RaiseWindow = PS3_RaiseWindow;
    device->MaximizeWindow = PS3_MaximizeWindow;
    device->MinimizeWindow = PS3_MinimizeWindow;
    device->RestoreWindow = PS3_RestoreWindow;
    device->SetWindowGrab = PS3_SetWindowGrab;
    device->DestroyWindow = PS3_DestroyWindow;
    device->HasScreenKeyboardSupport = PS3_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = PS3_ShowScreenKeyboard;
    device->HideScreenKeyboard = PS3_HideScreenKeyboard;
    device->IsScreenKeyboardShown = PS3_IsScreenKeyboardShown;

    device->PumpEvents = PS3_PumpEvents;

    device->free = PS3_DeleteDevice;

    return device;
}

VideoBootStrap PS3_bootstrap = {
    PS3VID_DRIVER_NAME, "SDL ps3 video driver",
    PS3_Available, PS3_CreateDevice
};

/* vi: set ts=4 sw=4 expandtab: */
