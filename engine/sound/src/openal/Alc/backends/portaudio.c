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

#include <portaudio.h>


static const ALCchar pa_device[] = "PortAudio Default";


#ifdef HAVE_DYNLOAD
static void *pa_handle;
#define MAKE_FUNC(x) static typeof(x) * p##x
MAKE_FUNC(Pa_Initialize);
MAKE_FUNC(Pa_Terminate);
MAKE_FUNC(Pa_GetErrorText);
MAKE_FUNC(Pa_StartStream);
MAKE_FUNC(Pa_StopStream);
MAKE_FUNC(Pa_OpenStream);
MAKE_FUNC(Pa_CloseStream);
MAKE_FUNC(Pa_GetDefaultOutputDevice);
MAKE_FUNC(Pa_GetStreamInfo);
#undef MAKE_FUNC

#define Pa_Initialize                  pPa_Initialize
#define Pa_Terminate                   pPa_Terminate
#define Pa_GetErrorText                pPa_GetErrorText
#define Pa_StartStream                 pPa_StartStream
#define Pa_StopStream                  pPa_StopStream
#define Pa_OpenStream                  pPa_OpenStream
#define Pa_CloseStream                 pPa_CloseStream
#define Pa_GetDefaultOutputDevice      pPa_GetDefaultOutputDevice
#define Pa_GetStreamInfo               pPa_GetStreamInfo
#endif

static ALCboolean pa_load(void)
{
    PaError err;

#ifdef HAVE_DYNLOAD
    if(!pa_handle)
    {
#ifdef _WIN32
# define PALIB "portaudio.dll"
#elif defined(__APPLE__) && defined(__MACH__)
# define PALIB "libportaudio.2.dylib"
#elif defined(__OpenBSD__)
# define PALIB "libportaudio.so"
#else
# define PALIB "libportaudio.so.2"
#endif

        pa_handle = LoadLib(PALIB);
        if(!pa_handle)
            return ALC_FALSE;

#define LOAD_FUNC(f) do {                                                     \
    p##f = GetSymbol(pa_handle, #f);                                          \
    if(p##f == NULL)                                                          \
    {                                                                         \
        CloseLib(pa_handle);                                                  \
        pa_handle = NULL;                                                     \
        return ALC_FALSE;                                                     \
    }                                                                         \
} while(0)
        LOAD_FUNC(Pa_Initialize);
        LOAD_FUNC(Pa_Terminate);
        LOAD_FUNC(Pa_GetErrorText);
        LOAD_FUNC(Pa_StartStream);
        LOAD_FUNC(Pa_StopStream);
        LOAD_FUNC(Pa_OpenStream);
        LOAD_FUNC(Pa_CloseStream);
        LOAD_FUNC(Pa_GetDefaultOutputDevice);
        LOAD_FUNC(Pa_GetStreamInfo);
#undef LOAD_FUNC

        if((err=Pa_Initialize()) != paNoError)
        {
            ERR("Pa_Initialize() returned an error: %s\n", Pa_GetErrorText(err));
            CloseLib(pa_handle);
            pa_handle = NULL;
            return ALC_FALSE;
        }
    }
#else
    if((err=Pa_Initialize()) != paNoError)
    {
        ERR("Pa_Initialize() returned an error: %s\n", Pa_GetErrorText(err));
        return ALC_FALSE;
    }
#endif
    return ALC_TRUE;
}


typedef struct {
    PaStream *stream;
    PaStreamParameters params;
    ALuint update_size;

    RingBuffer *ring;
} pa_data;


static int pa_callback(const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo,
                       const PaStreamCallbackFlags statusFlags, void *userData)
{
    ALCdevice *device = (ALCdevice*)userData;

    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    aluMixData(device, outputBuffer, framesPerBuffer);
    return 0;
}

static int pa_capture_cb(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo,
                         const PaStreamCallbackFlags statusFlags, void *userData)
{
    ALCdevice *device = (ALCdevice*)userData;
    pa_data *data = (pa_data*)device->ExtraData;

    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    WriteRingBuffer(data->ring, inputBuffer, framesPerBuffer);
    return 0;
}


static ALCenum pa_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    pa_data *data;
    PaError err;

    if(!deviceName)
        deviceName = pa_device;
    else if(strcmp(deviceName, pa_device) != 0)
        return ALC_INVALID_VALUE;

    data = (pa_data*)calloc(1, sizeof(pa_data));
    data->update_size = device->UpdateSize;

    data->params.device = -1;
    if(!ConfigValueInt("port", "device", &data->params.device) ||
       data->params.device < 0)
        data->params.device = Pa_GetDefaultOutputDevice();
    data->params.suggestedLatency = (device->UpdateSize*device->NumUpdates) /
                                    (float)device->Frequency;
    data->params.hostApiSpecificStreamInfo = NULL;

    data->params.channelCount = ((device->FmtChans == DevFmtMono) ? 1 : 2);

    switch(device->FmtType)
    {
        case DevFmtByte:
            data->params.sampleFormat = paInt8;
            break;
        case DevFmtUByte:
            data->params.sampleFormat = paUInt8;
            break;
        case DevFmtUShort:
            /* fall-through */
        case DevFmtShort:
            data->params.sampleFormat = paInt16;
            break;
        case DevFmtUInt:
            /* fall-through */
        case DevFmtInt:
            data->params.sampleFormat = paInt32;
            break;
        case DevFmtFloat:
            data->params.sampleFormat = paFloat32;
            break;
    }

retry_open:
    err = Pa_OpenStream(&data->stream, NULL, &data->params, device->Frequency,
                        device->UpdateSize, paNoFlag, pa_callback, device);
    if(err != paNoError)
    {
        if(data->params.sampleFormat == paFloat32)
        {
            data->params.sampleFormat = paInt16;
            goto retry_open;
        }
        ERR("Pa_OpenStream() returned an error: %s\n", Pa_GetErrorText(err));
        free(data);
        return ALC_INVALID_VALUE;
    }

    device->ExtraData = data;
    device->DeviceName = strdup(deviceName);

    return ALC_NO_ERROR;
}

static void pa_close_playback(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    PaError err;

    err = Pa_CloseStream(data->stream);
    if(err != paNoError)
        ERR("Error closing stream: %s\n", Pa_GetErrorText(err));

    free(data);
    device->ExtraData = NULL;
}

static ALCboolean pa_reset_playback(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    const PaStreamInfo *streamInfo;

    streamInfo = Pa_GetStreamInfo(data->stream);
    device->Frequency = streamInfo->sampleRate;
    device->UpdateSize = data->update_size;

    if(data->params.sampleFormat == paInt8)
        device->FmtType = DevFmtByte;
    else if(data->params.sampleFormat == paUInt8)
        device->FmtType = DevFmtUByte;
    else if(data->params.sampleFormat == paInt16)
        device->FmtType = DevFmtShort;
    else if(data->params.sampleFormat == paInt32)
        device->FmtType = DevFmtInt;
    else if(data->params.sampleFormat == paFloat32)
        device->FmtType = DevFmtFloat;
    else
    {
        ERR("Unexpected sample format: 0x%lx\n", data->params.sampleFormat);
        return ALC_FALSE;
    }

    if(data->params.channelCount == 2)
        device->FmtChans = DevFmtStereo;
    else if(data->params.channelCount == 1)
        device->FmtChans = DevFmtMono;
    else
    {
        ERR("Unexpected channel count: %u\n", data->params.channelCount);
        return ALC_FALSE;
    }
    SetDefaultChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean pa_start_playback(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    PaError err;

    err = Pa_StartStream(data->stream);
    if(err != paNoError)
    {
        ERR("Pa_StartStream() returned an error: %s\n", Pa_GetErrorText(err));
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static void pa_stop_playback(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    PaError err;

    err = Pa_StopStream(data->stream);
    if(err != paNoError)
        ERR("Error stopping stream: %s\n", Pa_GetErrorText(err));
}


static ALCenum pa_open_capture(ALCdevice *device, const ALCchar *deviceName)
{
    ALuint frame_size;
    pa_data *data;
    PaError err;

    if(!deviceName)
        deviceName = pa_device;
    else if(strcmp(deviceName, pa_device) != 0)
        return ALC_INVALID_VALUE;

    data = (pa_data*)calloc(1, sizeof(pa_data));
    if(data == NULL)
        return ALC_OUT_OF_MEMORY;

    frame_size = FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
    data->ring = CreateRingBuffer(frame_size, device->UpdateSize*device->NumUpdates);
    if(data->ring == NULL)
        goto error;

    data->params.device = -1;
    if(!ConfigValueInt("port", "capture", &data->params.device) ||
       data->params.device < 0)
        data->params.device = Pa_GetDefaultOutputDevice();
    data->params.suggestedLatency = 0.0f;
    data->params.hostApiSpecificStreamInfo = NULL;

    switch(device->FmtType)
    {
        case DevFmtByte:
            data->params.sampleFormat = paInt8;
            break;
        case DevFmtUByte:
            data->params.sampleFormat = paUInt8;
            break;
        case DevFmtShort:
            data->params.sampleFormat = paInt16;
            break;
        case DevFmtInt:
            data->params.sampleFormat = paInt32;
            break;
        case DevFmtFloat:
            data->params.sampleFormat = paFloat32;
            break;
        case DevFmtUInt:
        case DevFmtUShort:
            ERR("%s samples not supported\n", DevFmtTypeString(device->FmtType));
            goto error;
    }
    data->params.channelCount = ChannelsFromDevFmt(device->FmtChans);

    err = Pa_OpenStream(&data->stream, &data->params, NULL, device->Frequency,
                        paFramesPerBufferUnspecified, paNoFlag, pa_capture_cb, device);
    if(err != paNoError)
    {
        ERR("Pa_OpenStream() returned an error: %s\n", Pa_GetErrorText(err));
        goto error;
    }

    device->DeviceName = strdup(deviceName);

    device->ExtraData = data;
    return ALC_NO_ERROR;

error:
    DestroyRingBuffer(data->ring);
    free(data);
    return ALC_INVALID_VALUE;
}

static void pa_close_capture(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    PaError err;

    err = Pa_CloseStream(data->stream);
    if(err != paNoError)
        ERR("Error closing stream: %s\n", Pa_GetErrorText(err));

    DestroyRingBuffer(data->ring);
    data->ring = NULL;

    free(data);
    device->ExtraData = NULL;
}

static void pa_start_capture(ALCdevice *device)
{
    pa_data *data = device->ExtraData;
    PaError err;

    err = Pa_StartStream(data->stream);
    if(err != paNoError)
        ERR("Error starting stream: %s\n", Pa_GetErrorText(err));
}

static void pa_stop_capture(ALCdevice *device)
{
    pa_data *data = (pa_data*)device->ExtraData;
    PaError err;

    err = Pa_StopStream(data->stream);
    if(err != paNoError)
        ERR("Error stopping stream: %s\n", Pa_GetErrorText(err));
}

static ALCenum pa_capture_samples(ALCdevice *device, ALCvoid *buffer, ALCuint samples)
{
    pa_data *data = device->ExtraData;
    ReadRingBuffer(data->ring, buffer, samples);
    return ALC_NO_ERROR;
}

static ALCuint pa_available_samples(ALCdevice *device)
{
    pa_data *data = device->ExtraData;
    return RingBufferSize(data->ring);
}


static const BackendFuncs pa_funcs = {
    pa_open_playback,
    pa_close_playback,
    pa_reset_playback,
    pa_start_playback,
    pa_stop_playback,
    pa_open_capture,
    pa_close_capture,
    pa_start_capture,
    pa_stop_capture,
    pa_capture_samples,
    pa_available_samples,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};

ALCboolean alc_pa_init(BackendFuncs *func_list)
{
    if(!pa_load())
        return ALC_FALSE;
    *func_list = pa_funcs;
    return ALC_TRUE;
}

void alc_pa_deinit(void)
{
#ifdef HAVE_DYNLOAD
    if(pa_handle)
    {
        Pa_Terminate();
        CloseLib(pa_handle);
        pa_handle = NULL;
    }
#else
    Pa_Terminate();
#endif
}

void alc_pa_probe(enum DevProbe type)
{
    switch(type)
    {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(pa_device);
            break;
        case CAPTURE_DEVICE_PROBE:
            AppendCaptureDeviceList(pa_device);
            break;
    }
}
