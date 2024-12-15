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

#include "SDL_PS3video.h"
#include "../SDL_sysvideo.h"
#include "SDL_timer.h"

#include <sysutil/sysutil_sysparam.h>

#include <assert.h>

void
PS3_InitModes(_THIS)
{
	int rv = 0;
	(void)rv;
    deprintf(1, "+PS3_InitModes()\n");
    SDL_DisplayMode mode;
    PS3_DisplayModeData *modedata;
    CellVideoOutState state;

    modedata = (PS3_DisplayModeData *) SDL_malloc(sizeof(*modedata));
    if (!modedata) {
        return;
    }

	rv = cellVideoOutGetState(0, 0, &state);
    assert( rv == 0); // Get the state of the display
    assert(state.state == 0); // Make sure display is enabled

    // Get the current resolution
	CellVideoOutResolution res;
	rv = cellVideoOutGetResolution(state.displayMode.resolutionId, &res);
    assert(rv == 0);

    /* Setting up the DisplayMode based on current settings */
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.refresh_rate = 0;
    mode.w = res.width;
    mode.h = res.height;

    modedata->vconfig.resolutionId = state.displayMode.resolutionId;
    modedata->vconfig.format = (enum CellVideoOutBufferColorFormat)CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
    modedata->vconfig.pitch = res.width * 4;
    mode.driverdata = modedata;

    /* Setup the display to it's  default mode */
	rv = cellVideoOutConfigure(0, &modedata->vconfig, NULL, 1);
    assert(rv == 0);

	// Wait until RSX is ready
	do{
		SDL_Delay(10);
		rv = cellVideoOutGetState(0, 0, &state);
		assert( rv == 0);
	}while ( state.state == 3);

    /* Set display's videomode and add it */
    SDL_AddBasicVideoDisplay(&mode);

    deprintf(1, "-PS3_InitModes()\n");
}

/* DisplayModes available on the PS3 */
static SDL_DisplayMode ps3fb_modedb[] = {
    /* Native resolutions (progressive, "fullscreen") */
    {SDL_PIXELFORMAT_ARGB8888, 1920, 1080, 0, NULL}, // 1080p
    {SDL_PIXELFORMAT_ARGB8888, 1280, 720, 0, NULL}, // 720p
    {SDL_PIXELFORMAT_ARGB8888, 720, 480, 0, NULL}, // 480p
    {SDL_PIXELFORMAT_ARGB8888, 720, 576, 0, NULL}, // 576p
};

/* PS3 videomode number according to ps3fb_modedb */
static PS3_DisplayModeData ps3fb_data[] = {
	// { resolution, format, aspect, padding, pitch }
	{{
		(enum CellVideoOutResolutionId)CELL_VIDEO_OUT_RESOLUTION_1080, 
		(enum CellVideoOutBufferColorFormat)CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8,
		(enum CellVideoOutDisplayAspect)CELL_VIDEO_OUT_ASPECT_16_9, 
		{0, 0, 0, 0, 0, 0, 0, 0, 0},
		1920 * 4
	}},
	{{
		(enum CellVideoOutResolutionId)CELL_VIDEO_OUT_RESOLUTION_720, 
		(enum CellVideoOutBufferColorFormat)CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8,
		(enum CellVideoOutDisplayAspect)CELL_VIDEO_OUT_ASPECT_16_9, 
		{0, 0, 0, 0, 0, 0, 0, 0, 0},
		1280 * 4
	}},
	{{
		(enum CellVideoOutResolutionId)CELL_VIDEO_OUT_RESOLUTION_480, 
		(enum CellVideoOutBufferColorFormat)CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8,
		(enum CellVideoOutDisplayAspect)CELL_VIDEO_OUT_ASPECT_16_9, 
		{0, 0, 0, 0, 0, 0, 0, 0, 0},
		720 * 4
	}},
	{{
		(enum CellVideoOutResolutionId)CELL_VIDEO_OUT_RESOLUTION_576, 
		(enum CellVideoOutBufferColorFormat)CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8,
		(enum CellVideoOutDisplayAspect)CELL_VIDEO_OUT_ASPECT_16_9, 
		{0, 0, 0, 0, 0, 0, 0, 0, 0},
		720 * 4
	}},
};

void
PS3_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    deprintf(1, "+PS3_GetDisplayModes()\n");
    unsigned int nummodes;

    nummodes = sizeof(ps3fb_modedb) / sizeof(SDL_DisplayMode);

    int n;
    for (n=0; n<nummodes; ++n) {
        /* Get driver specific mode data */
        ps3fb_modedb[n].driverdata = &ps3fb_data[n];

        /* Add DisplayMode to list */
        deprintf(2, "Adding resolution %u x %u\n", ps3fb_modedb[n].w, ps3fb_modedb[n].h);
        SDL_AddDisplayMode(display, &ps3fb_modedb[n]);
    }
    deprintf(1, "-PS3_GetDisplayModes()\n");
}

int
PS3_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
	int rv = 0;

    deprintf(1, "+PS3_SetDisplayMode()\n");
    PS3_DisplayModeData *dispdata = (PS3_DisplayModeData *) mode->driverdata;
	CellVideoOutState state;

    /* Set the new DisplayMode */
    deprintf(2, "Setting PS3_MODE to %u\n", dispdata->vconfig.resolutionId);
    if ( cellVideoOutConfigure(0, &dispdata->vconfig, NULL, 0) != 0)
	{
        deprintf(2, "Could not set PS3FB_MODE\n");
        SDL_SetError("Could not set PS3FB_MODE\n");
        return -1;
    }

	// Wait until RSX is ready
	do{
		SDL_Delay(10);
		rv = cellVideoOutGetState(0, 0, &state);
		assert( rv == 0);
	}while ( state.state == 3);

    deprintf(1, "-PS3_SetDisplayMode()\n");
    return 0;
}

void
PS3_QuitModes(_THIS)
{
    deprintf(1, "+PS3_QuitModes()\n");

    /* There was no mem allocated for driverdata */
    int i, j;
    for (i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
        SDL_VideoDisplay *display = &_this->displays[i];
        for (j = display->num_display_modes; j--;) {
            display->display_modes[j].driverdata = NULL;
        }
    }

    deprintf(1, "-PS3_QuitModes()\n");
}

/* vi: set ts=4 sw=4 expandtab: */
