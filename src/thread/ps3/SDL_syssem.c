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

/* Semaphores in the BeOS environment */

//#include <be/kernel/OS.h>
#include <semaphore.h>
#include <pthread_types.h>
#include <sys/sys_time.h>

#include "SDL_thread.h"


struct SDL_semaphore
{
    sem_t id;
};

// Created using ChatGPT
void add_milliseconds_to_timespec(struct timespec *ts, long ms) {
    // Add milliseconds to the seconds field
    ts->tv_sec += ms / 1000;

    // Add the remainder (converted to nanoseconds) to the nanoseconds field
    ts->tv_nsec += (ms % 1000) * 1000000;

    // Normalize if nanoseconds overflow (1 second = 1,000,000,000 nanoseconds)
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec += ts->tv_nsec / 1000000000;
        ts->tv_nsec %= 1000000000;
    }
}

/* Create a counting semaphore */
SDL_sem *
SDL_CreateSemaphore(Uint32 initial_value)
{
	SDL_sem *sem;
	//sys_sem_attr_t attr;

	//SDL_zero( attr);
	//attr.attr_protocol = SYS_SEM_ATTR_PROTOCOL;
	//attr.attr_pshared = SYS_SEM_ATTR_PSHARED;

	sem = (SDL_sem *) SDL_malloc(sizeof(*sem));
	if (sem) {
		//sysSemCreate( &sem->id, &attr, initial_value, 32 * 1024);
		sem_init(&(sem->id), 0x0200, initial_value);
	} else {
		SDL_OutOfMemory();
	}
	return (sem);
}

/* Free the semaphore */
void
SDL_DestroySemaphore(SDL_sem * sem)
{
    if (sem) {
		sem_destroy(&(sem->id));
        SDL_free(sem);
    }
}

int
SDL_SemWaitTimeout(SDL_sem * sem, Uint32 timeout)
{
	int32_t val;
	int retval;
	sys_time_sec_t sec;
	sys_time_nsec_t nsec;
	struct timespec abstime;

	if (!sem) {
		SDL_SetError("Passed a NULL semaphore");
		return -1;
	}

	int finished = 0;
	while( finished == 0)
	{
		// Do not wait
		if( timeout == 0) {
			val = sem_trywait(&(sem->id));
		// Wait Forever
		} else if (timeout == SDL_MUTEX_MAXWAIT) {
			val = sem_wait(&(sem->id));
		// Wait until timeout
		} else {
			if (sys_time_get_current_time(&sec, &nsec) < 0) {
				return -1;
			}
			abstime.tv_sec = sec;
			abstime.tv_nsec = nsec;
			add_milliseconds_to_timespec(&abstime, timeout);
			val = sem_timedwait(&(sem->id), &abstime);
		}
		switch (val) {
			case EINTR:
				break;
			case 0:
				retval = 0;
				finished = 1;
				break;
			case ETIMEDOUT:
				retval = SDL_MUTEX_TIMEDOUT;
				finished = 1;
				break;
			default:
				SDL_SetError("sysSem[Try]Wait() failed");
				retval = -1;
				finished = 1;
				break;
		}
	}
	return retval;
}

int
SDL_SemTryWait(SDL_sem * sem)
{
    return SDL_SemWaitTimeout(sem, 0);
}

int
SDL_SemWait(SDL_sem * sem)
{
    return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

/* Returns the current count of the semaphore */
Uint32
SDL_SemValue(SDL_sem * sem)
{
    int32_t count;
    Uint32 value;

    value = 0;
    if (sem) {
		sem_getvalue (&(sem->id), &count);
        if (count > 0) {
            value = (Uint32) count;
        }
    }
    return value;
}

/* Atomically increases the semaphore's count (not blocking) */
int
SDL_SemPost(SDL_sem * sem)
{
    if (!sem) {
        SDL_SetError("Passed a NULL semaphore");
        return -1;
    }

    if (sem_post(&(sem->id)) != 0) {
        SDL_SetError("release_sem() failed");
        return -1;
    }
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
