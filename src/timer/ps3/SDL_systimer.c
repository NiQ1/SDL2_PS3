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

#ifdef SDL_TIMER_PS3
#include <sys/time.h>
#include <sys/timer.h>
#include <sys/sys_time.h>
#include <unistd.h>

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "../SDL_timer_c.h"

static struct timeval start;
static SDL_bool ticks_started = SDL_FALSE;

int gettimeofday(struct timeval* tv, void* unused)
{
	sys_time_sec_t sec;
	sys_time_nsec_t nsec;
	int rv = -1;

	rv = sys_time_get_current_time(&sec, &nsec);
	if (rv < 0) {
		return -1;
	}

	tv->tv_sec = sec;
	tv->tv_usec = nsec / 1000;

	return 0;
}

void
SDL_TicksInit(void)
{
    if (ticks_started) {
        return;
    }
    ticks_started = SDL_TRUE;

    gettimeofday(&start, NULL);
}

void
SDL_TicksQuit(void)
{
    ticks_started = SDL_FALSE;
}

Uint32 SDL_GetTicks(void)
{
    if (!ticks_started) {
        SDL_TicksInit();
    }

    struct timeval now;
    Uint32 ticks;
    gettimeofday(&now, NULL);
    ticks=(now.tv_sec-start.tv_sec)*1000+(now.tv_usec-start.tv_usec)/1000;
    return(ticks);
}

Uint64
SDL_GetPerformanceCounter(void)
{
    return SDL_GetTicks();
}

Uint64
SDL_GetPerformanceFrequency(void)
{
    return 1000;
}

void
SDL_Delay(Uint32 ms)
{
    sys_timer_usleep(ms * 1000);
}

#endif /* SDL_TIMER_PS3 */
/* vi: set ts=4 sw=4 expandtab: */
