/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by Chris Robinson
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

#include "alMain.h"
#include "alu.h"


static ALCenum loopback_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    device->DeviceName = strdup(deviceName);
    return ALC_NO_ERROR;
}

static void loopback_close_playback(ALCdevice *device)
{
    (void)device;
}

static ALCboolean loopback_reset_playback(ALCdevice *device)
{
    SetDefaultWFXChannelOrder(device);
    return ALC_TRUE;
}

static ALCboolean loopback_start_playback(ALCdevice *device)
{
    return ALC_TRUE;
    (void)device;
}

static void loopback_stop_playback(ALCdevice *device)
{
    (void)device;
}


static const BackendFuncs loopback_funcs = {
    loopback_open_playback,
    loopback_close_playback,
    loopback_reset_playback,
    loopback_start_playback,
    loopback_stop_playback,
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

ALCboolean alc_loopback_init(BackendFuncs *func_list)
{
    *func_list = loopback_funcs;
    return ALC_TRUE;
}

void alc_loopback_deinit(void)
{
}

void alc_loopback_probe(enum DevProbe type)
{
    (void)type;
}
