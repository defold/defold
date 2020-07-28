/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alMain.h"
#include "alu.h"

#include <sndio.h>


static const ALCchar sndio_device[] = "SndIO Default";


static ALCboolean sndio_load(void)
{
    return ALC_TRUE;
}


typedef struct {
    struct sio_hdl *sndHandle;

    ALvoid *mix_data;
    ALsizei data_size;

    volatile int killNow;
    ALvoid *thread;
} sndio_data;


static ALuint sndio_proc(ALvoid *ptr)
{
    ALCdevice *device = ptr;
    sndio_data *data = device->ExtraData;
    ALsizei frameSize;
    size_t wrote;

    SetRTPriority();

    frameSize = FrameSizeFromDevFmt(device->FmtChans, device->FmtType);

    while(!data->killNow && device->Connected)
    {
        ALsizei len = data->data_size;
        ALubyte *WritePtr = data->mix_data;

        aluMixData(device, WritePtr, len/frameSize);
        while(len > 0 && !data->killNow)
        {
            wrote = sio_write(data->sndHandle, WritePtr, len);
            if(wrote == 0)
            {
                ERR("sio_write failed\n");
                ALCdevice_Lock(device);
                aluHandleDisconnect(device);
                ALCdevice_Unlock(device);
                break;
            }

            len -= wrote;
            WritePtr += wrote;
        }
    }

    return 0;
}



static ALCenum sndio_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    sndio_data *data;

    if(!deviceName)
        deviceName = sndio_device;
    else if(strcmp(deviceName, sndio_device) != 0)
        return ALC_INVALID_VALUE;

    data = calloc(1, sizeof(*data));
    data->killNow = 0;

    data->sndHandle = sio_open(NULL, SIO_PLAY, 0);
    if(data->sndHandle == NULL)
    {
        free(data);
        ERR("Could not open device\n");
        return ALC_INVALID_VALUE;
    }

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;

    return ALC_NO_ERROR;
}

static void sndio_close_playback(ALCdevice *device)
{
    sndio_data *data = device->ExtraData;

    sio_close(data->sndHandle);
    free(data);
    device->ExtraData = NULL;
}

static ALCboolean sndio_reset_playback(ALCdevice *device)
{
    sndio_data *data = device->ExtraData;
    struct sio_par par;

    sio_initpar(&par);

    par.rate = device->Frequency;
    par.pchan = ((device->FmtChans != DevFmtMono) ? 2 : 1);

    switch(device->FmtType)
    {
        case DevFmtByte:
            par.bits = 8;
            par.sig = 1;
            break;
        case DevFmtUByte:
            par.bits = 8;
            par.sig = 0;
            break;
        case DevFmtFloat:
        case DevFmtShort:
            par.bits = 16;
            par.sig = 1;
            break;
        case DevFmtUShort:
            par.bits = 16;
            par.sig = 0;
            break;
        case DevFmtInt:
            par.bits = 32;
            par.sig = 1;
            break;
        case DevFmtUInt:
            par.bits = 32;
            par.sig = 0;
            break;
    }
    par.le = SIO_LE_NATIVE;

    par.round = device->UpdateSize;
    par.appbufsz = device->UpdateSize * (device->NumUpdates-1);
    if(!par.appbufsz) par.appbufsz = device->UpdateSize;

    if(!sio_setpar(data->sndHandle, &par) || !sio_getpar(data->sndHandle, &par))
    {
        ERR("Failed to set device parameters\n");
        return ALC_FALSE;
    }

    if(par.bits != par.bps*8)
    {
        ERR("Padded samples not supported (%u of %u bits)\n", par.bits, par.bps*8);
        return ALC_FALSE;
    }

    device->Frequency = par.rate;
    device->FmtChans = ((par.pchan==1) ? DevFmtMono : DevFmtStereo);

    if(par.bits == 8 && par.sig == 1)
        device->FmtType = DevFmtByte;
    else if(par.bits == 8 && par.sig == 0)
        device->FmtType = DevFmtUByte;
    else if(par.bits == 16 && par.sig == 1)
        device->FmtType = DevFmtShort;
    else if(par.bits == 16 && par.sig == 0)
        device->FmtType = DevFmtUShort;
    else if(par.bits == 32 && par.sig == 1)
        device->FmtType = DevFmtInt;
    else if(par.bits == 32 && par.sig == 0)
        device->FmtType = DevFmtUInt;
    else
    {
        ERR("Unhandled sample format: %s %u-bit\n", (par.sig?"signed":"unsigned"), par.bits);
        return ALC_FALSE;
    }

    device->UpdateSize = par.round;
    device->NumUpdates = (par.bufsz/par.round) + 1;

    SetDefaultChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean sndio_start_playback(ALCdevice *device)
{
    sndio_data *data = device->ExtraData;

    if(!sio_start(data->sndHandle))
    {
        ERR("Error starting playback\n");
        return ALC_FALSE;
    }

    data->data_size = device->UpdateSize * FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
    data->mix_data = calloc(1, data->data_size);

    data->thread = StartThread(sndio_proc, device);
    if(data->thread == NULL)
    {
        sio_stop(data->sndHandle);
        free(data->mix_data);
        data->mix_data = NULL;
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static void sndio_stop_playback(ALCdevice *device)
{
    sndio_data *data = device->ExtraData;

    if(!data->thread)
        return;

    data->killNow = 1;
    StopThread(data->thread);
    data->thread = NULL;

    data->killNow = 0;
    if(!sio_stop(data->sndHandle))
        ERR("Error stopping device\n");

    free(data->mix_data);
    data->mix_data = NULL;
}


static const BackendFuncs sndio_funcs = {
    sndio_open_playback,
    sndio_close_playback,
    sndio_reset_playback,
    sndio_start_playback,
    sndio_stop_playback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};

ALCboolean alc_sndio_init(BackendFuncs *func_list)
{
    if(!sndio_load())
        return ALC_FALSE;
    *func_list = sndio_funcs;
    return ALC_TRUE;
}

void alc_sndio_deinit(void)
{
}

void alc_sndio_probe(enum DevProbe type)
{
    switch(type)
    {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(sndio_device);
            break;
        case CAPTURE_DEVICE_PROBE:
            break;
    }
}
