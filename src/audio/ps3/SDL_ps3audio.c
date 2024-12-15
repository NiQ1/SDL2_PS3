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

    This file written by Ryan C. Gordon (icculus@icculus.org)
*/
#include "SDL_config.h"

/* Output audio to PS3 */

#include "SDL.h"


#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_ps3audio.h"
#include <sys/event.h>

#define SHW64(X) (u32)(((u64)(uintptr_t)X)>>32), (u32)(((u64)(uintptr_t)X)&0xFFFFFFFF)

#define AUDIO_DEBUG

#ifdef AUDIO_DEBUG
#define deprintf(...) printf(__VA_ARGS__)
#else
#define deprintf(...)
#endif

static int
PS3_AUD_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
	deprintf( "PS3_AUD_OpenDevice(%08X.%08X, %s, %d)\n", SHW64(this), devname, iscapture);
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    int valid_datatype = 0;

    this->hidden = SDL_malloc(sizeof(*(this->hidden)));
    if (!this->hidden) {
        SDL_OutOfMemory();
        return 0;
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));


	// PS3 Libaudio only handles floats
    while ((!valid_datatype) && (test_format)) {
        this->spec.format = test_format;
        switch (test_format) {
        case AUDIO_F32MSB:
            valid_datatype = 1;
            break;
        default:
            test_format = SDL_NextAudioFormat();
            break;
        }
    }

    int ret=cellAudioInit();

	//set some parameters we want
	//either 2 or 8 channel
	_params.nChannel = CELL_AUDIO_PORT_2CH;
	//8 16 or 32 block buffer
	_params.nBlock = CELL_AUDIO_BLOCK_8;
	//extended attributes
	_params.attr = 0;
	//sound level (1 is default)
	_params.level = 1;

	ret=cellAudioPortOpen(&_params, &_portNum);
	deprintf("audioPortOpen: %d\n",ret);
	deprintf("  portNum: %d\n",_portNum);

	ret=cellAudioGetPortConfig(_portNum, &_config);
	deprintf("audioGetPortConfig: %d\n",ret);
	deprintf("  readIndex: 0x%8X\n",_config.readIndexAddr);
	deprintf("  status: %d\n",_config.status);
	deprintf("  channelCount: %llu\n",_config.nChannel);
	deprintf("  numBlocks: %llu\n",_config.nBlock);
	deprintf("  portSize: %d\n",_config.portSize);
	deprintf("  portAddr: 0x%8X\n",_config.portAddr);

	// create an event queue that will tell when a block is read
	ret=cellAudioCreateNotifyEventQueue( &_snd_queue, &_snd_queue_key);
	printf("audioCreateNotifyEventQueue: %d\n",ret);

	// Set it to the sprx
	ret = cellAudioSetNotifyEventQueue(_snd_queue_key);
	printf("audioSetNotifyEventQueue: %d\n",ret);

	// clears the event queue
	ret = sys_event_queue_drain(_snd_queue);
	printf("sysEentQueueDrain: %d\n",ret);

	ret=cellAudioPortStart(_portNum);
	deprintf("audioPortStart: %d\n",ret);

	_last_filled_buf = _config.nBlock - 1;

	this->spec.format = test_format;
	this->spec.size = sizeof(float) * CELL_AUDIO_BLOCK_SAMPLES * _config.nChannel;
	this->spec.freq = 48000;
	this->spec.samples = CELL_AUDIO_BLOCK_SAMPLES;
	this->spec.channels = _config.nChannel;

    return ret == 0;
}

#if 0
static void
PS3_AUD_PlayDevice(_THIS)
{
	deprintf( "PS3_AUD_PlayDevice(%08X.%08X)\n", SHW64(this));
	
	/*
	while( _config.readIndex == _last_filled_buf) // FIXME this is a mess to remove when queued event will me integrated
	{
		deprintf( "\tplaying too fast... waiting a ms\n");
		//sleep(1);
	}*/
    /*TransferSoundData *sound = SDL_malloc(sizeof(TransferSoundData));
    if (!sound) {
        SDL_OutOfMemory();
    }

    playGenericSound(this->hidden->mixbuf, this->hidden->mixlen);*/
}
#endif

static void
PS3_AUD_CloseDevice(_THIS)
{
	deprintf( "PS3_AUD_CloseDevice(%08X.%08X)\n", SHW64(this));
	int ret = 0;
	ret=cellAudioPortStop(_portNum);
	deprintf("audioPortStop: %d\n",ret);
	ret=cellAudioRemoveNotifyEventQueue(_snd_queue_key);
	deprintf("audioRemoveNotifyEventQueue: %d\n",ret);
	ret=cellAudioPortClose(_portNum);
	deprintf("audioPortClose: %d\n",ret);
	ret=sys_event_queue_destroy(_snd_queue, 0);
	deprintf("sysEventQueueDestroy: %d\n",ret);
	ret=cellAudioQuit();
	deprintf("audioQuit: %d\n",ret);

    SDL_free(this->hidden);
}

static Uint8 *
PS3_AUD_GetDeviceBuf(_THIS)
{
	
	//deprintf( "PS3_AUD_GetDeviceBuf(%08X.%08X) at %d ms\n", SHW64(this), SDL_GetTicks());

    //int playing = _config.readIndex;
    // int playing = *((u64*)(u64)_config.readIndexAddr);
    int filling = (_last_filled_buf + 1) % _config.nBlock;
	Uint8 * dma_buf = (Uint8 *)_config.portAddr;
	//deprintf( "\tWriting to buffer %d \n", filling);
	// deprintf( "\tbuffer address (%08X.%08X => %08X.%08X)\n", SHW64(_config.portAddr), SHW64(dma_buf));

	_last_filled_buf = filling;
    return (dma_buf + (filling * this->spec.size));
}

/* This function waits until it is possible to write a full sound buffer */
static void
PS3_WaitDevice(_THIS)
{
    /* We're in blocking mode, so there's nothing to do here */
	//deprintf( "ALSA_WaitDevice(%08X.%08X)\n", SHW64(this));
	
	sys_event_t event;
	//s32 ret = 
	sys_event_queue_receive( _snd_queue, &event, 20 * 1000);
	//deprintf( "sysEventQueueReceive: %08X\n", ret);
}


	static int
PS3_AUD_Init(SDL_AudioDriverImpl * impl)
{
	deprintf( "PS3_AUD_Init(%08X.%08X)\n", SHW64(impl));
	/* Set the function pointers */
	impl->OpenDevice = PS3_AUD_OpenDevice;
	//impl->PlayDevice = PS3_AUD_PlayDevice;
    impl->WaitDevice = PS3_WaitDevice;
	impl->CloseDevice = PS3_AUD_CloseDevice;
	impl->GetDeviceBuf = PS3_AUD_GetDeviceBuf;

    /* and the capabilities */
    impl->OnlyHasDefaultOutputDevice = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap PS3AUDIO_bootstrap = {
    "ps3", "SDL PS3 audio driver", PS3_AUD_Init, 0       /*1? */
};

/* vi: set ts=4 sw=4 expandtab: */
