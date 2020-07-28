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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "alMain.h"
#include "alu.h"

#include <sys/soundcard.h>

/*
 * The OSS documentation talks about SOUND_MIXER_READ, but the header
 * only contains MIXER_READ. Play safe. Same for WRITE.
 */
#ifndef SOUND_MIXER_READ
#define SOUND_MIXER_READ MIXER_READ
#endif
#ifndef SOUND_MIXER_WRITE
#define SOUND_MIXER_WRITE MIXER_WRITE
#endif

static const ALCchar oss_device[] = "OSS Default";

static const char *oss_driver = "/dev/dsp";
static const char *oss_capture = "/dev/dsp";

typedef struct {
    int fd;
    volatile int killNow;
    ALvoid *thread;

    ALubyte *mix_data;
    int data_size;

    RingBuffer *ring;
    int doCapture;
} oss_data;


static int log2i(ALCuint x)
{
    int y = 0;
    while (x > 1)
    {
        x >>= 1;
        y++;
    }
    return y;
}


static ALuint OSSProc(ALvoid *ptr)
{
    ALCdevice *Device = (ALCdevice*)ptr;
    oss_data *data = (oss_data*)Device->ExtraData;
    ALint frameSize;
    ssize_t wrote;

    SetRTPriority();

    frameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);

    while(!data->killNow && Device->Connected)
    {
        ALint len = data->data_size;
        ALubyte *WritePtr = data->mix_data;

        aluMixData(Device, WritePtr, len/frameSize);
        while(len > 0 && !data->killNow)
        {
            wrote = write(data->fd, WritePtr, len);
            if(wrote < 0)
            {
                if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                {
                    ERR("write failed: %s\n", strerror(errno));
                    ALCdevice_Lock(Device);
                    aluHandleDisconnect(Device);
                    ALCdevice_Unlock(Device);
                    break;
                }

                Sleep(1);
                continue;
            }

            len -= wrote;
            WritePtr += wrote;
        }
    }

    return 0;
}

static ALuint OSSCaptureProc(ALvoid *ptr)
{
    ALCdevice *Device = (ALCdevice*)ptr;
    oss_data *data = (oss_data*)Device->ExtraData;
    int frameSize;
    int amt;

    SetRTPriority();

    frameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);

    while(!data->killNow)
    {
        amt = read(data->fd, data->mix_data, data->data_size);
        if(amt < 0)
        {
            ERR("read failed: %s\n", strerror(errno));
            ALCdevice_Lock(Device);
            aluHandleDisconnect(Device);
            ALCdevice_Unlock(Device);
            break;
        }
        if(amt == 0)
        {
            Sleep(1);
            continue;
        }
        if(data->doCapture)
            WriteRingBuffer(data->ring, data->mix_data, amt/frameSize);
    }

    return 0;
}

static ALCenum oss_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    oss_data *data;

    if(!deviceName)
        deviceName = oss_device;
    else if(strcmp(deviceName, oss_device) != 0)
        return ALC_INVALID_VALUE;

    data = (oss_data*)calloc(1, sizeof(oss_data));
    data->killNow = 0;

    data->fd = open(oss_driver, O_WRONLY);
    if(data->fd == -1)
    {
        free(data);
        ERR("Could not open %s: %s\n", oss_driver, strerror(errno));
        return ALC_INVALID_VALUE;
    }

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;
    return ALC_NO_ERROR;
}

static void oss_close_playback(ALCdevice *device)
{
    oss_data *data = (oss_data*)device->ExtraData;

    close(data->fd);
    free(data);
    device->ExtraData = NULL;
}

static ALCboolean oss_reset_playback(ALCdevice *device)
{
    oss_data *data = (oss_data*)device->ExtraData;
    int numFragmentsLogSize;
    int log2FragmentSize;
    unsigned int periods;
    audio_buf_info info;
    ALuint frameSize;
    int numChannels;
    int ossFormat;
    int ossSpeed;
    char *err;

    switch(device->FmtType)
    {
        case DevFmtByte:
            ossFormat = AFMT_S8;
            break;
        case DevFmtUByte:
            ossFormat = AFMT_U8;
            break;
        case DevFmtUShort:
        case DevFmtInt:
        case DevFmtUInt:
        case DevFmtFloat:
            device->FmtType = DevFmtShort;
            /* fall-through */
        case DevFmtShort:
            ossFormat = AFMT_S16_NE;
            break;
    }

    periods = device->NumUpdates;
    numChannels = ChannelsFromDevFmt(device->FmtChans);
    frameSize = numChannels * BytesFromDevFmt(device->FmtType);

    ossSpeed = device->Frequency;
    log2FragmentSize = log2i(device->UpdateSize * frameSize);

    /* according to the OSS spec, 16 bytes are the minimum */
    if (log2FragmentSize < 4)
        log2FragmentSize = 4;
    /* Subtract one period since the temp mixing buffer counts as one. Still
     * need at least two on the card, though. */
    if(periods > 2) periods--;
    numFragmentsLogSize = (periods << 16) | log2FragmentSize;

#define CHECKERR(func) if((func) < 0) {                                       \
    err = #func;                                                              \
    goto err;                                                                 \
}
    /* Don't fail if SETFRAGMENT fails. We can handle just about anything
     * that's reported back via GETOSPACE */
    ioctl(data->fd, SNDCTL_DSP_SETFRAGMENT, &numFragmentsLogSize);
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_SETFMT, &ossFormat));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_CHANNELS, &numChannels));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_SPEED, &ossSpeed));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_GETOSPACE, &info));
    if(0)
    {
    err:
        ERR("%s failed: %s\n", err, strerror(errno));
        return ALC_FALSE;
    }
#undef CHECKERR

    if((int)ChannelsFromDevFmt(device->FmtChans) != numChannels)
    {
        ERR("Failed to set %s, got %d channels instead\n", DevFmtChannelsString(device->FmtChans), numChannels);
        return ALC_FALSE;
    }

    if(!((ossFormat == AFMT_S8 && device->FmtType == DevFmtByte) ||
         (ossFormat == AFMT_U8 && device->FmtType == DevFmtUByte) ||
         (ossFormat == AFMT_S16_NE && device->FmtType == DevFmtShort)))
    {
        ERR("Failed to set %s samples, got OSS format %#x\n", DevFmtTypeString(device->FmtType), ossFormat);
        return ALC_FALSE;
    }

    device->Frequency = ossSpeed;
    device->UpdateSize = info.fragsize / frameSize;
    device->NumUpdates = info.fragments + 1;

    SetDefaultChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean oss_start_playback(ALCdevice *device)
{
    oss_data *data = (oss_data*)device->ExtraData;

    data->data_size = device->UpdateSize * FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
    data->mix_data = calloc(1, data->data_size);

    data->thread = StartThread(OSSProc, device);
    if(data->thread == NULL)
    {
        free(data->mix_data);
        data->mix_data = NULL;
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static void oss_stop_playback(ALCdevice *device)
{
    oss_data *data = (oss_data*)device->ExtraData;

    if(!data->thread)
        return;

    data->killNow = 1;
    StopThread(data->thread);
    data->thread = NULL;

    data->killNow = 0;
    if(ioctl(data->fd, SNDCTL_DSP_RESET) != 0)
        ERR("Error resetting device: %s\n", strerror(errno));

    free(data->mix_data);
    data->mix_data = NULL;
}


static ALCenum oss_open_capture(ALCdevice *device, const ALCchar *deviceName)
{
    int numFragmentsLogSize;
    int log2FragmentSize;
    unsigned int periods;
    audio_buf_info info;
    ALuint frameSize;
    int numChannels;
    oss_data *data;
    int ossFormat;
    int ossSpeed;
    char *err;

    if(!deviceName)
        deviceName = oss_device;
    else if(strcmp(deviceName, oss_device) != 0)
        return ALC_INVALID_VALUE;

    data = (oss_data*)calloc(1, sizeof(oss_data));
    data->killNow = 0;

    data->fd = open(oss_capture, O_RDONLY);
    if(data->fd == -1)
    {
        free(data);
        ERR("Could not open %s: %s\n", oss_capture, strerror(errno));
        return ALC_INVALID_VALUE;
    }

    switch(device->FmtType)
    {
        case DevFmtByte:
            ossFormat = AFMT_S8;
            break;
        case DevFmtUByte:
            ossFormat = AFMT_U8;
            break;
        case DevFmtShort:
            ossFormat = AFMT_S16_NE;
            break;
        case DevFmtUShort:
        case DevFmtInt:
        case DevFmtUInt:
        case DevFmtFloat:
            free(data);
            ERR("%s capture samples not supported\n", DevFmtTypeString(device->FmtType));
            return ALC_INVALID_VALUE;
    }

    periods = 4;
    numChannels = ChannelsFromDevFmt(device->FmtChans);
    frameSize = numChannels * BytesFromDevFmt(device->FmtType);
    ossSpeed = device->Frequency;
    log2FragmentSize = log2i(device->UpdateSize * device->NumUpdates *
                             frameSize / periods);

    /* according to the OSS spec, 16 bytes are the minimum */
    if (log2FragmentSize < 4)
        log2FragmentSize = 4;
    numFragmentsLogSize = (periods << 16) | log2FragmentSize;

#define CHECKERR(func) if((func) < 0) {                                       \
    err = #func;                                                              \
    goto err;                                                                 \
}
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_SETFRAGMENT, &numFragmentsLogSize));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_SETFMT, &ossFormat));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_CHANNELS, &numChannels));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_SPEED, &ossSpeed));
    CHECKERR(ioctl(data->fd, SNDCTL_DSP_GETISPACE, &info));
    if(0)
    {
    err:
        ERR("%s failed: %s\n", err, strerror(errno));
        close(data->fd);
        free(data);
        return ALC_INVALID_VALUE;
    }
#undef CHECKERR

    if((int)ChannelsFromDevFmt(device->FmtChans) != numChannels)
    {
        ERR("Failed to set %s, got %d channels instead\n", DevFmtChannelsString(device->FmtChans), numChannels);
        close(data->fd);
        free(data);
        return ALC_INVALID_VALUE;
    }

    if(!((ossFormat == AFMT_S8 && device->FmtType == DevFmtByte) ||
         (ossFormat == AFMT_U8 && device->FmtType == DevFmtUByte) ||
         (ossFormat == AFMT_S16_NE && device->FmtType == DevFmtShort)))
    {
        ERR("Failed to set %s samples, got OSS format %#x\n", DevFmtTypeString(device->FmtType), ossFormat);
        close(data->fd);
        free(data);
        return ALC_INVALID_VALUE;
    }

    data->ring = CreateRingBuffer(frameSize, device->UpdateSize * device->NumUpdates);
    if(!data->ring)
    {
        ERR("Ring buffer create failed\n");
        close(data->fd);
        free(data);
        return ALC_OUT_OF_MEMORY;
    }

    data->data_size = info.fragsize;
    data->mix_data = calloc(1, data->data_size);

    device->ExtraData = data;
    data->thread = StartThread(OSSCaptureProc, device);
    if(data->thread == NULL)
    {
        device->ExtraData = NULL;
        free(data->mix_data);
        free(data);
        return ALC_OUT_OF_MEMORY;
    }

    device->DeviceName = strdup(deviceName);
    return ALC_NO_ERROR;
}

static void oss_close_capture(ALCdevice *device)
{
    oss_data *data = (oss_data*)device->ExtraData;
    data->killNow = 1;
    StopThread(data->thread);

    close(data->fd);

    DestroyRingBuffer(data->ring);

    free(data->mix_data);
    free(data);
    device->ExtraData = NULL;
}

static void oss_start_capture(ALCdevice *Device)
{
    oss_data *data = (oss_data*)Device->ExtraData;
    data->doCapture = 1;
}

static void oss_stop_capture(ALCdevice *Device)
{
    oss_data *data = (oss_data*)Device->ExtraData;
    data->doCapture = 0;
}

static ALCenum oss_capture_samples(ALCdevice *Device, ALCvoid *pBuffer, ALCuint lSamples)
{
    oss_data *data = (oss_data*)Device->ExtraData;
    ReadRingBuffer(data->ring, pBuffer, lSamples);
    return ALC_NO_ERROR;
}

static ALCuint oss_available_samples(ALCdevice *Device)
{
    oss_data *data = (oss_data*)Device->ExtraData;
    return RingBufferSize(data->ring);
}


static const BackendFuncs oss_funcs = {
    oss_open_playback,
    oss_close_playback,
    oss_reset_playback,
    oss_start_playback,
    oss_stop_playback,
    oss_open_capture,
    oss_close_capture,
    oss_start_capture,
    oss_stop_capture,
    oss_capture_samples,
    oss_available_samples,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};

ALCboolean alc_oss_init(BackendFuncs *func_list)
{
    ConfigValueStr("oss", "device", &oss_driver);
    ConfigValueStr("oss", "capture", &oss_capture);

    *func_list = oss_funcs;
    return ALC_TRUE;
}

void alc_oss_deinit(void)
{
}

void alc_oss_probe(enum DevProbe type)
{
    switch(type)
    {
        case ALL_DEVICE_PROBE:
        {
#ifdef HAVE_STAT
            struct stat buf;
            if(stat(oss_driver, &buf) == 0)
#endif
                AppendAllDevicesList(oss_device);
        }
        break;

        case CAPTURE_DEVICE_PROBE:
        {
#ifdef HAVE_STAT
            struct stat buf;
            if(stat(oss_capture, &buf) == 0)
#endif
                AppendCaptureDeviceList(oss_device);
        }
        break;
    }
}
