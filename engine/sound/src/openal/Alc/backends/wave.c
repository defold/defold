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

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "alMain.h"
#include "alu.h"


typedef struct {
    FILE *f;
    long DataStart;

    ALvoid *buffer;
    ALuint size;

    volatile int killNow;
    ALvoid *thread;
} wave_data;


static const ALCchar waveDevice[] = "Wave File Writer";

static const ALubyte SUBTYPE_PCM[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
};
static const ALubyte SUBTYPE_FLOAT[] = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
};

static const ALuint channel_masks[] = {
    0, /* invalid */
    0x4, /* Mono */
    0x1 | 0x2, /* Stereo */
    0, /* 3 channel */
    0x1 | 0x2 | 0x10 | 0x20, /* Quad */
    0, /* 5 channel */
    0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20, /* 5.1 */
    0x1 | 0x2 | 0x4 | 0x8 | 0x100 | 0x200 | 0x400, /* 6.1 */
    0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x200 | 0x400, /* 7.1 */
};


static void fwrite16le(ALushort val, FILE *f)
{
    fputc(val&0xff, f);
    fputc((val>>8)&0xff, f);
}

static void fwrite32le(ALuint val, FILE *f)
{
    fputc(val&0xff, f);
    fputc((val>>8)&0xff, f);
    fputc((val>>16)&0xff, f);
    fputc((val>>24)&0xff, f);
}


static ALuint WaveProc(ALvoid *ptr)
{
    ALCdevice *Device = (ALCdevice*)ptr;
    wave_data *data = (wave_data*)Device->ExtraData;
    ALuint frameSize;
    ALuint now, start;
    ALuint64 avail, done;
    size_t fs;
    const ALuint restTime = (ALuint64)Device->UpdateSize * 1000 /
                            Device->Frequency / 2;

    frameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);

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
            aluMixData(Device, data->buffer, Device->UpdateSize);
            done += Device->UpdateSize;

            if(!IS_LITTLE_ENDIAN)
            {
                ALuint bytesize = BytesFromDevFmt(Device->FmtType);
                ALubyte *bytes = data->buffer;
                ALuint i;

                if(bytesize == 1)
                {
                    for(i = 0;i < data->size;i++)
                        fputc(bytes[i], data->f);
                }
                else if(bytesize == 2)
                {
                    for(i = 0;i < data->size;i++)
                        fputc(bytes[i^1], data->f);
                }
                else if(bytesize == 4)
                {
                    for(i = 0;i < data->size;i++)
                        fputc(bytes[i^3], data->f);
                }
            }
            else
            {
                fs = fwrite(data->buffer, frameSize, Device->UpdateSize,
                            data->f);
                (void)fs;
            }
            if(ferror(data->f))
            {
                ERR("Error writing to file\n");
                ALCdevice_Lock(Device);
                aluHandleDisconnect(Device);
                ALCdevice_Unlock(Device);
                break;
            }
        }
    }

    return 0;
}

static ALCenum wave_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    wave_data *data;
    const char *fname;

    fname = GetConfigValue("wave", "file", "");
    if(!fname[0])
        return ALC_INVALID_VALUE;

    if(!deviceName)
        deviceName = waveDevice;
    else if(strcmp(deviceName, waveDevice) != 0)
        return ALC_INVALID_VALUE;

    data = (wave_data*)calloc(1, sizeof(wave_data));

    data->f = fopen(fname, "wb");
    if(!data->f)
    {
        free(data);
        ERR("Could not open file '%s': %s\n", fname, strerror(errno));
        return ALC_INVALID_VALUE;
    }

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;
    return ALC_NO_ERROR;
}

static void wave_close_playback(ALCdevice *device)
{
    wave_data *data = (wave_data*)device->ExtraData;

    fclose(data->f);
    free(data);
    device->ExtraData = NULL;
}

static ALCboolean wave_reset_playback(ALCdevice *device)
{
    wave_data *data = (wave_data*)device->ExtraData;
    ALuint channels=0, bits=0;
    size_t val;

    fseek(data->f, 0, SEEK_SET);
    clearerr(data->f);

    switch(device->FmtType)
    {
        case DevFmtByte:
            device->FmtType = DevFmtUByte;
            break;
        case DevFmtUShort:
            device->FmtType = DevFmtShort;
            break;
        case DevFmtUInt:
            device->FmtType = DevFmtInt;
            break;
        case DevFmtUByte:
        case DevFmtShort:
        case DevFmtInt:
        case DevFmtFloat:
            break;
    }
    bits = BytesFromDevFmt(device->FmtType) * 8;
    channels = ChannelsFromDevFmt(device->FmtChans);

    fprintf(data->f, "RIFF");
    fwrite32le(0xFFFFFFFF, data->f); // 'RIFF' header len; filled in at close

    fprintf(data->f, "WAVE");

    fprintf(data->f, "fmt ");
    fwrite32le(40, data->f); // 'fmt ' header len; 40 bytes for EXTENSIBLE

    // 16-bit val, format type id (extensible: 0xFFFE)
    fwrite16le(0xFFFE, data->f);
    // 16-bit val, channel count
    fwrite16le(channels, data->f);
    // 32-bit val, frequency
    fwrite32le(device->Frequency, data->f);
    // 32-bit val, bytes per second
    fwrite32le(device->Frequency * channels * bits / 8, data->f);
    // 16-bit val, frame size
    fwrite16le(channels * bits / 8, data->f);
    // 16-bit val, bits per sample
    fwrite16le(bits, data->f);
    // 16-bit val, extra byte count
    fwrite16le(22, data->f);
    // 16-bit val, valid bits per sample
    fwrite16le(bits, data->f);
    // 32-bit val, channel mask
    fwrite32le(channel_masks[channels], data->f);
    // 16 byte GUID, sub-type format
    val = fwrite(((bits==32) ? SUBTYPE_FLOAT : SUBTYPE_PCM), 1, 16, data->f);
    (void)val;

    fprintf(data->f, "data");
    fwrite32le(0xFFFFFFFF, data->f); // 'data' header len; filled in at close

    if(ferror(data->f))
    {
        ERR("Error writing header: %s\n", strerror(errno));
        return ALC_FALSE;
    }
    data->DataStart = ftell(data->f);

    SetDefaultWFXChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean wave_start_playback(ALCdevice *device)
{
    wave_data *data = (wave_data*)device->ExtraData;

    data->size = device->UpdateSize * FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
    data->buffer = malloc(data->size);
    if(!data->buffer)
    {
        ERR("Buffer malloc failed\n");
        return ALC_FALSE;
    }

    data->thread = StartThread(WaveProc, device);
    if(data->thread == NULL)
    {
        free(data->buffer);
        data->buffer = NULL;
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static void wave_stop_playback(ALCdevice *device)
{
    wave_data *data = (wave_data*)device->ExtraData;
    ALuint dataLen;
    long size;

    if(!data->thread)
        return;

    data->killNow = 1;
    StopThread(data->thread);
    data->thread = NULL;

    data->killNow = 0;

    free(data->buffer);
    data->buffer = NULL;

    size = ftell(data->f);
    if(size > 0)
    {
        dataLen = size - data->DataStart;
        if(fseek(data->f, data->DataStart-4, SEEK_SET) == 0)
            fwrite32le(dataLen, data->f); // 'data' header len
        if(fseek(data->f, 4, SEEK_SET) == 0)
            fwrite32le(size-8, data->f); // 'WAVE' header len
    }
}


static const BackendFuncs wave_funcs = {
    wave_open_playback,
    wave_close_playback,
    wave_reset_playback,
    wave_start_playback,
    wave_stop_playback,
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

ALCboolean alc_wave_init(BackendFuncs *func_list)
{
    *func_list = wave_funcs;
    return ALC_TRUE;
}

void alc_wave_deinit(void)
{
}

void alc_wave_probe(enum DevProbe type)
{
    if(!ConfigValueExists("wave", "file"))
        return;

    switch(type)
    {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(waveDevice);
            break;
        case CAPTURE_DEVICE_PROBE:
            break;
    }
}
