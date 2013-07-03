/**
 * OpenAL cross platform audio library
 * Copyright (C) 2010 by Chris Robinson
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

#include <stdlib.h>
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "alMain.h"
#include "alu.h"


typedef struct {
    volatile int killNow;
    ALvoid *thread;
} null_data;


static const ALCchar nullDevice[] = "No Output";

static ALuint NullProc(ALvoid *ptr)
{
    ALCdevice *Device = (ALCdevice*)ptr;
    null_data *data = (null_data*)Device->ExtraData;
    ALuint now, start;
    ALuint64 avail, done;
    const ALuint restTime = (ALuint64)Device->UpdateSize * 1000 /
                            Device->Frequency / 2;

    done = 0;
    start = timeGetTime();
    while(!data->killNow && Device->Connected)
    {
        now = timeGetTime();

        avail = (ALuint64)(now-start) * Device->Frequency / 1000;
        if(avail < done)
        {
            /* Timer wrapped (50 days???). Add the remainder of the cycle to
             * the available count and reset the number of samples done */
            avail += ((ALuint64)1<<32)*Device->Frequency/1000 - done;
            done = 0;
        }
        if(avail-done < Device->UpdateSize)
        {
            Sleep(restTime);
            continue;
        }

        while(avail-done >= Device->UpdateSize)
        {
            aluMixData(Device, NULL, Device->UpdateSize);
            done += Device->UpdateSize;
        }
    }

    return 0;
}

static ALCenum null_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    null_data *data;

    if(!deviceName)
        deviceName = nullDevice;
    else if(strcmp(deviceName, nullDevice) != 0)
        return ALC_INVALID_VALUE;

    data = (null_data*)calloc(1, sizeof(*data));

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;
    return ALC_NO_ERROR;
}

static void null_close_playback(ALCdevice *device)
{
    null_data *data = (null_data*)device->ExtraData;

    free(data);
    device->ExtraData = NULL;
}

static ALCboolean null_reset_playback(ALCdevice *device)
{
    SetDefaultWFXChannelOrder(device);
    return ALC_TRUE;
}

static ALCboolean null_start_playback(ALCdevice *device)
{
    null_data *data = (null_data*)device->ExtraData;

    data->thread = StartThread(NullProc, device);
    if(data->thread == NULL)
        return ALC_FALSE;

    return ALC_TRUE;
}

static void null_stop_playback(ALCdevice *device)
{
    null_data *data = (null_data*)device->ExtraData;

    if(!data->thread)
        return;

    data->killNow = 1;
    StopThread(data->thread);
    data->thread = NULL;

    data->killNow = 0;
}


static const BackendFuncs null_funcs = {
    null_open_playback,
    null_close_playback,
    null_reset_playback,
    null_start_playback,
    null_stop_playback,
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

ALCboolean alc_null_init(BackendFuncs *func_list)
{
    *func_list = null_funcs;
    return ALC_TRUE;
}

void alc_null_deinit(void)
{
}

void alc_null_probe(enum DevProbe type)
{
    switch(type)
    {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(nullDevice);
            break;
        case CAPTURE_DEVICE_PROBE:
            break;
    }
}
