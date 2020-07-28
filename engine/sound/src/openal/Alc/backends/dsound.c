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

#include <dsound.h>
#include <cguid.h>
#include <mmreg.h>
#ifndef _WAVEFORMATEXTENSIBLE_
#include <ks.h>
#include <ksmedia.h>
#endif

#include "alMain.h"
#include "alu.h"

#ifndef DSSPEAKER_5POINT1
#   define DSSPEAKER_5POINT1          0x00000006
#endif
#ifndef DSSPEAKER_7POINT1
#   define DSSPEAKER_7POINT1          0x00000007
#endif
#ifndef DSSPEAKER_7POINT1_SURROUND
#   define DSSPEAKER_7POINT1_SURROUND 0x00000008
#endif
#ifndef DSSPEAKER_5POINT1_SURROUND
#   define DSSPEAKER_5POINT1_SURROUND 0x00000009
#endif


DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


static void *ds_handle;
static HRESULT (WINAPI *pDirectSoundCreate)(LPCGUID pcGuidDevice, IDirectSound **ppDS, IUnknown *pUnkOuter);
static HRESULT (WINAPI *pDirectSoundEnumerateA)(LPDSENUMCALLBACKA pDSEnumCallback, void *pContext);
static HRESULT (WINAPI *pDirectSoundCaptureCreate)(LPCGUID pcGuidDevice, IDirectSoundCapture **ppDSC, IUnknown *pUnkOuter);
static HRESULT (WINAPI *pDirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA pDSEnumCallback, void *pContext);

#define DirectSoundCreate            pDirectSoundCreate
#define DirectSoundEnumerateA        pDirectSoundEnumerateA
#define DirectSoundCaptureCreate     pDirectSoundCaptureCreate
#define DirectSoundCaptureEnumerateA pDirectSoundCaptureEnumerateA


typedef struct {
    // DirectSound Playback Device
    IDirectSound       *DS;
    IDirectSoundBuffer *PrimaryBuffer;
    IDirectSoundBuffer *Buffer;
    IDirectSoundNotify *Notifies;
    HANDLE             NotifyEvent;

    volatile int killNow;
    ALvoid *thread;
} DSoundPlaybackData;

typedef struct {
    // DirectSound Capture Device
    IDirectSoundCapture *DSC;
    IDirectSoundCaptureBuffer *DSCbuffer;
    DWORD BufferBytes;
    DWORD Cursor;
    RingBuffer *Ring;
} DSoundCaptureData;


typedef struct {
    ALCchar *name;
    GUID guid;
} DevMap;

static DevMap *PlaybackDeviceList;
static ALuint NumPlaybackDevices;
static DevMap *CaptureDeviceList;
static ALuint NumCaptureDevices;

#define MAX_UPDATES 128

static ALCboolean DSoundLoad(void)
{
    if(!ds_handle)
    {
        ds_handle = LoadLib("dsound.dll");
        if(ds_handle == NULL)
        {
            ERR("Failed to load dsound.dll\n");
            return ALC_FALSE;
        }

#define LOAD_FUNC(f) do {                                                     \
    p##f = GetSymbol(ds_handle, #f);                                          \
    if(p##f == NULL) {                                                        \
        CloseLib(ds_handle);                                                  \
        ds_handle = NULL;                                                     \
        return ALC_FALSE;                                                     \
    }                                                                         \
} while(0)
        LOAD_FUNC(DirectSoundCreate);
        LOAD_FUNC(DirectSoundEnumerateA);
        LOAD_FUNC(DirectSoundCaptureCreate);
        LOAD_FUNC(DirectSoundCaptureEnumerateA);
#undef LOAD_FUNC
    }
    return ALC_TRUE;
}


static BOOL CALLBACK DSoundEnumPlaybackDevices(LPGUID guid, LPCSTR desc, LPCSTR drvname, LPVOID data)
{
    LPOLESTR guidstr = NULL;
    char str[1024];
    HRESULT hr;
    void *temp;
    int count;
    ALuint i;

    (void)data;
    (void)drvname;

    if(!guid)
        return TRUE;

    count = 0;
    do {
        if(count == 0)
            snprintf(str, sizeof(str), "%s", desc);
        else
            snprintf(str, sizeof(str), "%s #%d", desc, count+1);
        count++;

        for(i = 0;i < NumPlaybackDevices;i++)
        {
            if(strcmp(str, PlaybackDeviceList[i].name) == 0)
                break;
        }
    } while(i != NumPlaybackDevices);

    hr = StringFromCLSID(guid, &guidstr);
    if(SUCCEEDED(hr))
    {
        TRACE("Got device \"%s\", GUID \"%ls\"\n", str, guidstr);
        CoTaskMemFree(guidstr);
    }

    temp = realloc(PlaybackDeviceList, sizeof(DevMap) * (NumPlaybackDevices+1));
    if(temp)
    {
        PlaybackDeviceList = temp;
        PlaybackDeviceList[NumPlaybackDevices].name = strdup(str);
        PlaybackDeviceList[NumPlaybackDevices].guid = *guid;
        NumPlaybackDevices++;
    }

    return TRUE;
}


static BOOL CALLBACK DSoundEnumCaptureDevices(LPGUID guid, LPCSTR desc, LPCSTR drvname, LPVOID data)
{
    LPOLESTR guidstr = NULL;
    char str[1024];
    HRESULT hr;
    void *temp;
    int count;
    ALuint i;

    (void)data;
    (void)drvname;

    if(!guid)
        return TRUE;

    count = 0;
    do {
        if(count == 0)
            snprintf(str, sizeof(str), "%s", desc);
        else
            snprintf(str, sizeof(str), "%s #%d", desc, count+1);
        count++;

        for(i = 0;i < NumCaptureDevices;i++)
        {
            if(strcmp(str, CaptureDeviceList[i].name) == 0)
                break;
        }
    } while(i != NumCaptureDevices);

    hr = StringFromCLSID(guid, &guidstr);
    if(SUCCEEDED(hr))
    {
        TRACE("Got device \"%s\", GUID \"%ls\"\n", str, guidstr);
        CoTaskMemFree(guidstr);
    }

    temp = realloc(CaptureDeviceList, sizeof(DevMap) * (NumCaptureDevices+1));
    if(temp)
    {
        CaptureDeviceList = temp;
        CaptureDeviceList[NumCaptureDevices].name = strdup(str);
        CaptureDeviceList[NumCaptureDevices].guid = *guid;
        NumCaptureDevices++;
    }

    return TRUE;
}


static ALuint DSoundPlaybackProc(ALvoid *ptr)
{
    ALCdevice *Device = (ALCdevice*)ptr;
    DSoundPlaybackData *data = (DSoundPlaybackData*)Device->ExtraData;
    DSBCAPS DSBCaps;
    DWORD LastCursor = 0;
    DWORD PlayCursor;
    VOID *WritePtr1, *WritePtr2;
    DWORD WriteCnt1,  WriteCnt2;
    BOOL Playing = FALSE;
    DWORD FrameSize;
    DWORD FragSize;
    DWORD avail;
    HRESULT err;

    SetRTPriority();

    memset(&DSBCaps, 0, sizeof(DSBCaps));
    DSBCaps.dwSize = sizeof(DSBCaps);
    err = IDirectSoundBuffer_GetCaps(data->Buffer, &DSBCaps);
    if(FAILED(err))
    {
        ERR("Failed to get buffer caps: 0x%lx\n", err);
        ALCdevice_Lock(Device);
        aluHandleDisconnect(Device);
        ALCdevice_Unlock(Device);
        return 1;
    }

    FrameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);
    FragSize = Device->UpdateSize * FrameSize;

    IDirectSoundBuffer_GetCurrentPosition(data->Buffer, &LastCursor, NULL);
    while(!data->killNow)
    {
        // Get current play cursor
        IDirectSoundBuffer_GetCurrentPosition(data->Buffer, &PlayCursor, NULL);
        avail = (PlayCursor-LastCursor+DSBCaps.dwBufferBytes) % DSBCaps.dwBufferBytes;

        if(avail < FragSize)
        {
            if(!Playing)
            {
                err = IDirectSoundBuffer_Play(data->Buffer, 0, 0, DSBPLAY_LOOPING);
                if(FAILED(err))
                {
                    ERR("Failed to play buffer: 0x%lx\n", err);
                    ALCdevice_Lock(Device);
                    aluHandleDisconnect(Device);
                    ALCdevice_Unlock(Device);
                    return 1;
                }
                Playing = TRUE;
            }

            avail = WaitForSingleObjectEx(data->NotifyEvent, 2000, FALSE);
            if(avail != WAIT_OBJECT_0)
                ERR("WaitForSingleObjectEx error: 0x%lx\n", avail);
            continue;
        }
        avail -= avail%FragSize;

        // Lock output buffer
        WriteCnt1 = 0;
        WriteCnt2 = 0;
        err = IDirectSoundBuffer_Lock(data->Buffer, LastCursor, avail, &WritePtr1, &WriteCnt1, &WritePtr2, &WriteCnt2, 0);

        // If the buffer is lost, restore it and lock
        if(err == DSERR_BUFFERLOST)
        {
            WARN("Buffer lost, restoring...\n");
            err = IDirectSoundBuffer_Restore(data->Buffer);
            if(SUCCEEDED(err))
            {
                Playing = FALSE;
                LastCursor = 0;
                err = IDirectSoundBuffer_Lock(data->Buffer, 0, DSBCaps.dwBufferBytes, &WritePtr1, &WriteCnt1, &WritePtr2, &WriteCnt2, 0);
            }
        }

        // Successfully locked the output buffer
        if(SUCCEEDED(err))
        {
            // If we have an active context, mix data directly into output buffer otherwise fill with silence
            aluMixData(Device, WritePtr1, WriteCnt1/FrameSize);
            aluMixData(Device, WritePtr2, WriteCnt2/FrameSize);

            // Unlock output buffer only when successfully locked
            IDirectSoundBuffer_Unlock(data->Buffer, WritePtr1, WriteCnt1, WritePtr2, WriteCnt2);
        }
        else
        {
            ERR("Buffer lock error: %#lx\n", err);
            ALCdevice_Lock(Device);
            aluHandleDisconnect(Device);
            ALCdevice_Unlock(Device);
            return 1;
        }

        // Update old write cursor location
        LastCursor += WriteCnt1+WriteCnt2;
        LastCursor %= DSBCaps.dwBufferBytes;
    }

    return 0;
}

static ALCenum DSoundOpenPlayback(ALCdevice *device, const ALCchar *deviceName)
{
    DSoundPlaybackData *data = NULL;
    LPGUID guid = NULL;
    HRESULT hr;

    if(!PlaybackDeviceList)
    {
        hr = DirectSoundEnumerateA(DSoundEnumPlaybackDevices, NULL);
        if(FAILED(hr))
            ERR("Error enumerating DirectSound devices (%#x)!\n", (unsigned int)hr);
    }

    if(!deviceName && NumPlaybackDevices > 0)
    {
        deviceName = PlaybackDeviceList[0].name;
        guid = &PlaybackDeviceList[0].guid;
    }
    else
    {
        ALuint i;

        for(i = 0;i < NumPlaybackDevices;i++)
        {
            if(strcmp(deviceName, PlaybackDeviceList[i].name) == 0)
            {
                guid = &PlaybackDeviceList[i].guid;
                break;
            }
        }
        if(i == NumPlaybackDevices)
            return ALC_INVALID_VALUE;
    }

    //Initialise requested device
    data = calloc(1, sizeof(DSoundPlaybackData));
    if(!data)
        return ALC_OUT_OF_MEMORY;

    hr = DS_OK;
    data->NotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(data->NotifyEvent == NULL)
        hr = E_FAIL;

    //DirectSound Init code
    if(SUCCEEDED(hr))
        hr = DirectSoundCreate(guid, &data->DS, NULL);
    if(SUCCEEDED(hr))
        hr = IDirectSound_SetCooperativeLevel(data->DS, GetForegroundWindow(), DSSCL_PRIORITY);
    if(FAILED(hr))
    {
        if(data->DS)
            IDirectSound_Release(data->DS);
        if(data->NotifyEvent)
            CloseHandle(data->NotifyEvent);
        free(data);
        ERR("Device init failed: 0x%08lx\n", hr);
        return ALC_INVALID_VALUE;
    }

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;
    return ALC_NO_ERROR;
}

static void DSoundClosePlayback(ALCdevice *device)
{
    DSoundPlaybackData *data = device->ExtraData;

    if(data->Notifies)
        IDirectSoundNotify_Release(data->Notifies);
    data->Notifies = NULL;
    if(data->Buffer)
        IDirectSoundBuffer_Release(data->Buffer);
    data->Buffer = NULL;
    if(data->PrimaryBuffer != NULL)
        IDirectSoundBuffer_Release(data->PrimaryBuffer);
    data->PrimaryBuffer = NULL;

    IDirectSound_Release(data->DS);
    CloseHandle(data->NotifyEvent);
    free(data);
    device->ExtraData = NULL;
}

static ALCboolean DSoundResetPlayback(ALCdevice *device)
{
    DSoundPlaybackData *data = (DSoundPlaybackData*)device->ExtraData;
    DSBUFFERDESC DSBDescription;
    WAVEFORMATEXTENSIBLE OutputType;
    DWORD speakers;
    HRESULT hr;

    memset(&OutputType, 0, sizeof(OutputType));

    if(data->Notifies)
        IDirectSoundNotify_Release(data->Notifies);
    data->Notifies = NULL;
    if(data->Buffer)
        IDirectSoundBuffer_Release(data->Buffer);
    data->Buffer = NULL;
    if(data->PrimaryBuffer != NULL)
        IDirectSoundBuffer_Release(data->PrimaryBuffer);
    data->PrimaryBuffer = NULL;

    switch(device->FmtType)
    {
        case DevFmtByte:
            device->FmtType = DevFmtUByte;
            break;
        case DevFmtFloat:
            if((device->Flags&DEVICE_SAMPLE_TYPE_REQUEST))
                break;
            /* fall-through */
        case DevFmtUShort:
            device->FmtType = DevFmtShort;
            break;
        case DevFmtUInt:
            device->FmtType = DevFmtInt;
            break;
        case DevFmtUByte:
        case DevFmtShort:
        case DevFmtInt:
            break;
    }

    hr = IDirectSound_GetSpeakerConfig(data->DS, &speakers);
    if(SUCCEEDED(hr))
    {
        if(!(device->Flags&DEVICE_CHANNELS_REQUEST))
        {
            speakers = DSSPEAKER_CONFIG(speakers);
            if(speakers == DSSPEAKER_MONO)
                device->FmtChans = DevFmtMono;
            else if(speakers == DSSPEAKER_STEREO || speakers == DSSPEAKER_HEADPHONE)
                device->FmtChans = DevFmtStereo;
            else if(speakers == DSSPEAKER_QUAD)
                device->FmtChans = DevFmtQuad;
            else if(speakers == DSSPEAKER_5POINT1 || speakers == DSSPEAKER_5POINT1_SURROUND)
                device->FmtChans = DevFmtX51;
            else if(speakers == DSSPEAKER_7POINT1 || speakers == DSSPEAKER_7POINT1_SURROUND)
                device->FmtChans = DevFmtX71;
            else
                ERR("Unknown system speaker config: 0x%lx\n", speakers);
        }

        switch(device->FmtChans)
        {
            case DevFmtMono:
                OutputType.dwChannelMask = SPEAKER_FRONT_CENTER;
                break;
            case DevFmtStereo:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT;
                break;
            case DevFmtQuad:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT |
                                           SPEAKER_BACK_LEFT |
                                           SPEAKER_BACK_RIGHT;
                break;
            case DevFmtX51:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT |
                                           SPEAKER_FRONT_CENTER |
                                           SPEAKER_LOW_FREQUENCY |
                                           SPEAKER_BACK_LEFT |
                                           SPEAKER_BACK_RIGHT;
                break;
            case DevFmtX51Side:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT |
                                           SPEAKER_FRONT_CENTER |
                                           SPEAKER_LOW_FREQUENCY |
                                           SPEAKER_SIDE_LEFT |
                                           SPEAKER_SIDE_RIGHT;
                break;
            case DevFmtX61:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT |
                                           SPEAKER_FRONT_CENTER |
                                           SPEAKER_LOW_FREQUENCY |
                                           SPEAKER_BACK_CENTER |
                                           SPEAKER_SIDE_LEFT |
                                           SPEAKER_SIDE_RIGHT;
                break;
            case DevFmtX71:
                OutputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                           SPEAKER_FRONT_RIGHT |
                                           SPEAKER_FRONT_CENTER |
                                           SPEAKER_LOW_FREQUENCY |
                                           SPEAKER_BACK_LEFT |
                                           SPEAKER_BACK_RIGHT |
                                           SPEAKER_SIDE_LEFT |
                                           SPEAKER_SIDE_RIGHT;
                break;
        }

retry_open:
        hr = S_OK;
        OutputType.Format.wFormatTag = WAVE_FORMAT_PCM;
        OutputType.Format.nChannels = ChannelsFromDevFmt(device->FmtChans);
        OutputType.Format.wBitsPerSample = BytesFromDevFmt(device->FmtType) * 8;
        OutputType.Format.nBlockAlign = OutputType.Format.nChannels*OutputType.Format.wBitsPerSample/8;
        OutputType.Format.nSamplesPerSec = device->Frequency;
        OutputType.Format.nAvgBytesPerSec = OutputType.Format.nSamplesPerSec*OutputType.Format.nBlockAlign;
        OutputType.Format.cbSize = 0;
    }

    if(OutputType.Format.nChannels > 2 || device->FmtType == DevFmtFloat)
    {
        OutputType.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        OutputType.Samples.wValidBitsPerSample = OutputType.Format.wBitsPerSample;
        OutputType.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        if(device->FmtType == DevFmtFloat)
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        else
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

        if(data->PrimaryBuffer)
            IDirectSoundBuffer_Release(data->PrimaryBuffer);
        data->PrimaryBuffer = NULL;
    }
    else
    {
        if(SUCCEEDED(hr) && !data->PrimaryBuffer)
        {
            memset(&DSBDescription,0,sizeof(DSBUFFERDESC));
            DSBDescription.dwSize=sizeof(DSBUFFERDESC);
            DSBDescription.dwFlags=DSBCAPS_PRIMARYBUFFER;
            hr = IDirectSound_CreateSoundBuffer(data->DS, &DSBDescription, &data->PrimaryBuffer, NULL);
        }
        if(SUCCEEDED(hr))
            hr = IDirectSoundBuffer_SetFormat(data->PrimaryBuffer,&OutputType.Format);
    }

    if(SUCCEEDED(hr))
    {
        if(device->NumUpdates > MAX_UPDATES)
        {
            device->UpdateSize = (device->UpdateSize*device->NumUpdates +
                                  MAX_UPDATES-1) / MAX_UPDATES;
            device->NumUpdates = MAX_UPDATES;
        }

        memset(&DSBDescription,0,sizeof(DSBUFFERDESC));
        DSBDescription.dwSize=sizeof(DSBUFFERDESC);
        DSBDescription.dwFlags=DSBCAPS_CTRLPOSITIONNOTIFY|DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
        DSBDescription.dwBufferBytes=device->UpdateSize * device->NumUpdates *
                                     OutputType.Format.nBlockAlign;
        DSBDescription.lpwfxFormat=&OutputType.Format;
        hr = IDirectSound_CreateSoundBuffer(data->DS, &DSBDescription, &data->Buffer, NULL);
        if(FAILED(hr) && device->FmtType == DevFmtFloat)
        {
            device->FmtType = DevFmtShort;
            goto retry_open;
        }
    }

    if(SUCCEEDED(hr))
    {
        hr = IDirectSoundBuffer_QueryInterface(data->Buffer, &IID_IDirectSoundNotify, (LPVOID *)&data->Notifies);
        if(SUCCEEDED(hr))
        {
            DSBPOSITIONNOTIFY notifies[MAX_UPDATES];
            ALuint i;

            for(i = 0;i < device->NumUpdates;++i)
            {
                notifies[i].dwOffset = i * device->UpdateSize *
                                       OutputType.Format.nBlockAlign;
                notifies[i].hEventNotify = data->NotifyEvent;
            }
            if(IDirectSoundNotify_SetNotificationPositions(data->Notifies, device->NumUpdates, notifies) != DS_OK)
                hr = E_FAIL;
        }
    }

    if(FAILED(hr))
    {
        if(data->Notifies != NULL)
            IDirectSoundNotify_Release(data->Notifies);
        data->Notifies = NULL;
        if(data->Buffer != NULL)
            IDirectSoundBuffer_Release(data->Buffer);
        data->Buffer = NULL;
        if(data->PrimaryBuffer != NULL)
            IDirectSoundBuffer_Release(data->PrimaryBuffer);
        data->PrimaryBuffer = NULL;
        return ALC_FALSE;
    }

    ResetEvent(data->NotifyEvent);
    SetDefaultWFXChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean DSoundStartPlayback(ALCdevice *device)
{
    DSoundPlaybackData *data = (DSoundPlaybackData*)device->ExtraData;

    data->thread = StartThread(DSoundPlaybackProc, device);
    if(data->thread == NULL)
        return ALC_FALSE;

    return ALC_TRUE;
}

static void DSoundStopPlayback(ALCdevice *device)
{
    DSoundPlaybackData *data = device->ExtraData;

    if(!data->thread)
        return;

    data->killNow = 1;
    StopThread(data->thread);
    data->thread = NULL;

    data->killNow = 0;
    IDirectSoundBuffer_Stop(data->Buffer);
}


static ALCenum DSoundOpenCapture(ALCdevice *device, const ALCchar *deviceName)
{
    DSoundCaptureData *data = NULL;
    WAVEFORMATEXTENSIBLE InputType;
    DSCBUFFERDESC DSCBDescription;
    LPGUID guid = NULL;
    HRESULT hr, hrcom;
    ALuint samples;

    if(!CaptureDeviceList)
    {
        /* Initialize COM to prevent name truncation */
        hrcom = CoInitialize(NULL);
        hr = DirectSoundCaptureEnumerateA(DSoundEnumCaptureDevices, NULL);
        if(FAILED(hr))
            ERR("Error enumerating DirectSound devices (%#x)!\n", (unsigned int)hr);
        if(SUCCEEDED(hrcom))
            CoUninitialize();
    }

    if(!deviceName && NumCaptureDevices > 0)
    {
        deviceName = CaptureDeviceList[0].name;
        guid = &CaptureDeviceList[0].guid;
    }
    else
    {
        ALuint i;

        for(i = 0;i < NumCaptureDevices;i++)
        {
            if(strcmp(deviceName, CaptureDeviceList[i].name) == 0)
            {
                guid = &CaptureDeviceList[i].guid;
                break;
            }
        }
        if(i == NumCaptureDevices)
            return ALC_INVALID_VALUE;
    }

    switch(device->FmtType)
    {
        case DevFmtByte:
        case DevFmtUShort:
        case DevFmtUInt:
            WARN("%s capture samples not supported\n", DevFmtTypeString(device->FmtType));
            return ALC_INVALID_ENUM;

        case DevFmtUByte:
        case DevFmtShort:
        case DevFmtInt:
        case DevFmtFloat:
            break;
    }

    //Initialise requested device
    data = calloc(1, sizeof(DSoundCaptureData));
    if(!data)
        return ALC_OUT_OF_MEMORY;

    hr = DS_OK;

    //DirectSoundCapture Init code
    if(SUCCEEDED(hr))
        hr = DirectSoundCaptureCreate(guid, &data->DSC, NULL);
    if(SUCCEEDED(hr))
    {
        memset(&InputType, 0, sizeof(InputType));

        switch(device->FmtChans)
        {
            case DevFmtMono:
                InputType.dwChannelMask = SPEAKER_FRONT_CENTER;
                break;
            case DevFmtStereo:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT;
                break;
            case DevFmtQuad:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT |
                                          SPEAKER_BACK_LEFT |
                                          SPEAKER_BACK_RIGHT;
                break;
            case DevFmtX51:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT |
                                          SPEAKER_FRONT_CENTER |
                                          SPEAKER_LOW_FREQUENCY |
                                          SPEAKER_BACK_LEFT |
                                          SPEAKER_BACK_RIGHT;
                break;
            case DevFmtX51Side:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT |
                                          SPEAKER_FRONT_CENTER |
                                          SPEAKER_LOW_FREQUENCY |
                                          SPEAKER_SIDE_LEFT |
                                          SPEAKER_SIDE_RIGHT;
                break;
            case DevFmtX61:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT |
                                          SPEAKER_FRONT_CENTER |
                                          SPEAKER_LOW_FREQUENCY |
                                          SPEAKER_BACK_CENTER |
                                          SPEAKER_SIDE_LEFT |
                                          SPEAKER_SIDE_RIGHT;
                break;
            case DevFmtX71:
                InputType.dwChannelMask = SPEAKER_FRONT_LEFT |
                                          SPEAKER_FRONT_RIGHT |
                                          SPEAKER_FRONT_CENTER |
                                          SPEAKER_LOW_FREQUENCY |
                                          SPEAKER_BACK_LEFT |
                                          SPEAKER_BACK_RIGHT |
                                          SPEAKER_SIDE_LEFT |
                                          SPEAKER_SIDE_RIGHT;
                break;
        }

        InputType.Format.wFormatTag = WAVE_FORMAT_PCM;
        InputType.Format.nChannels = ChannelsFromDevFmt(device->FmtChans);
        InputType.Format.wBitsPerSample = BytesFromDevFmt(device->FmtType) * 8;
        InputType.Format.nBlockAlign = InputType.Format.nChannels*InputType.Format.wBitsPerSample/8;
        InputType.Format.nSamplesPerSec = device->Frequency;
        InputType.Format.nAvgBytesPerSec = InputType.Format.nSamplesPerSec*InputType.Format.nBlockAlign;
        InputType.Format.cbSize = 0;

        if(InputType.Format.nChannels > 2 || device->FmtType == DevFmtFloat)
        {
            InputType.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            InputType.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            InputType.Samples.wValidBitsPerSample = InputType.Format.wBitsPerSample;
            if(device->FmtType == DevFmtFloat)
                InputType.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            else
                InputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        }

        samples = device->UpdateSize * device->NumUpdates;
        samples = maxu(samples, 100 * device->Frequency / 1000);

        memset(&DSCBDescription, 0, sizeof(DSCBUFFERDESC));
        DSCBDescription.dwSize = sizeof(DSCBUFFERDESC);
        DSCBDescription.dwFlags = 0;
        DSCBDescription.dwBufferBytes = samples * InputType.Format.nBlockAlign;
        DSCBDescription.lpwfxFormat = &InputType.Format;

        hr = IDirectSoundCapture_CreateCaptureBuffer(data->DSC, &DSCBDescription, &data->DSCbuffer, NULL);
    }
    if(SUCCEEDED(hr))
    {
         data->Ring = CreateRingBuffer(InputType.Format.nBlockAlign, device->UpdateSize * device->NumUpdates);
         if(data->Ring == NULL)
             hr = DSERR_OUTOFMEMORY;
    }

    if(FAILED(hr))
    {
        ERR("Device init failed: 0x%08lx\n", hr);

        DestroyRingBuffer(data->Ring);
        data->Ring = NULL;
        if(data->DSCbuffer != NULL)
            IDirectSoundCaptureBuffer_Release(data->DSCbuffer);
        data->DSCbuffer = NULL;
        if(data->DSC)
            IDirectSoundCapture_Release(data->DSC);
        data->DSC = NULL;

        free(data);
        return ALC_INVALID_VALUE;
    }

    data->BufferBytes = DSCBDescription.dwBufferBytes;
    SetDefaultWFXChannelOrder(device);

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;

    return ALC_NO_ERROR;
}

static void DSoundCloseCapture(ALCdevice *device)
{
    DSoundCaptureData *data = device->ExtraData;

    DestroyRingBuffer(data->Ring);
    data->Ring = NULL;

    if(data->DSCbuffer != NULL)
    {
        IDirectSoundCaptureBuffer_Stop(data->DSCbuffer);
        IDirectSoundCaptureBuffer_Release(data->DSCbuffer);
        data->DSCbuffer = NULL;
    }

    IDirectSoundCapture_Release(data->DSC);
    data->DSC = NULL;

    free(data);
    device->ExtraData = NULL;
}

static void DSoundStartCapture(ALCdevice *device)
{
    DSoundCaptureData *data = device->ExtraData;
    HRESULT hr;

    hr = IDirectSoundCaptureBuffer_Start(data->DSCbuffer, DSCBSTART_LOOPING);
    if(FAILED(hr))
    {
        ERR("start failed: 0x%08lx\n", hr);
        aluHandleDisconnect(device);
    }
}

static void DSoundStopCapture(ALCdevice *device)
{
    DSoundCaptureData *data = device->ExtraData;
    HRESULT hr;

    hr = IDirectSoundCaptureBuffer_Stop(data->DSCbuffer);
    if(FAILED(hr))
    {
        ERR("stop failed: 0x%08lx\n", hr);
        aluHandleDisconnect(device);
    }
}

static ALCenum DSoundCaptureSamples(ALCdevice *Device, ALCvoid *pBuffer, ALCuint lSamples)
{
    DSoundCaptureData *data = Device->ExtraData;
    ReadRingBuffer(data->Ring, pBuffer, lSamples);
    return ALC_NO_ERROR;
}

static ALCuint DSoundAvailableSamples(ALCdevice *Device)
{
    DSoundCaptureData *data = Device->ExtraData;
    DWORD ReadCursor, LastCursor, BufferBytes, NumBytes;
    VOID *ReadPtr1, *ReadPtr2;
    DWORD ReadCnt1,  ReadCnt2;
    DWORD FrameSize;
    HRESULT hr;

    if(!Device->Connected)
        goto done;

    FrameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);
    BufferBytes = data->BufferBytes;
    LastCursor = data->Cursor;

    hr = IDirectSoundCaptureBuffer_GetCurrentPosition(data->DSCbuffer, NULL, &ReadCursor);
    if(SUCCEEDED(hr))
    {
        NumBytes = (ReadCursor-LastCursor + BufferBytes) % BufferBytes;
        if(NumBytes == 0)
            goto done;
        hr = IDirectSoundCaptureBuffer_Lock(data->DSCbuffer, LastCursor, NumBytes,
                                            &ReadPtr1, &ReadCnt1,
                                            &ReadPtr2, &ReadCnt2, 0);
    }
    if(SUCCEEDED(hr))
    {
        WriteRingBuffer(data->Ring, ReadPtr1, ReadCnt1/FrameSize);
        if(ReadPtr2 != NULL)
            WriteRingBuffer(data->Ring, ReadPtr2, ReadCnt2/FrameSize);
        hr = IDirectSoundCaptureBuffer_Unlock(data->DSCbuffer,
                                              ReadPtr1, ReadCnt1,
                                              ReadPtr2, ReadCnt2);
        data->Cursor = (LastCursor+ReadCnt1+ReadCnt2) % BufferBytes;
    }

    if(FAILED(hr))
    {
        ERR("update failed: 0x%08lx\n", hr);
        aluHandleDisconnect(Device);
    }

done:
    return RingBufferSize(data->Ring);
}


static const BackendFuncs DSoundFuncs = {
    DSoundOpenPlayback,
    DSoundClosePlayback,
    DSoundResetPlayback,
    DSoundStartPlayback,
    DSoundStopPlayback,
    DSoundOpenCapture,
    DSoundCloseCapture,
    DSoundStartCapture,
    DSoundStopCapture,
    DSoundCaptureSamples,
    DSoundAvailableSamples,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};


ALCboolean alcDSoundInit(BackendFuncs *FuncList)
{
    if(!DSoundLoad())
        return ALC_FALSE;
    *FuncList = DSoundFuncs;
    return ALC_TRUE;
}

void alcDSoundDeinit(void)
{
    ALuint i;

    for(i = 0;i < NumPlaybackDevices;++i)
        free(PlaybackDeviceList[i].name);
    free(PlaybackDeviceList);
    PlaybackDeviceList = NULL;
    NumPlaybackDevices = 0;

    for(i = 0;i < NumCaptureDevices;++i)
        free(CaptureDeviceList[i].name);
    free(CaptureDeviceList);
    CaptureDeviceList = NULL;
    NumCaptureDevices = 0;

    if(ds_handle)
        CloseLib(ds_handle);
    ds_handle = NULL;
}

void alcDSoundProbe(enum DevProbe type)
{
    HRESULT hr, hrcom;
    ALuint i;

    switch(type)
    {
        case ALL_DEVICE_PROBE:
            for(i = 0;i < NumPlaybackDevices;++i)
                free(PlaybackDeviceList[i].name);
            free(PlaybackDeviceList);
            PlaybackDeviceList = NULL;
            NumPlaybackDevices = 0;

            hr = DirectSoundEnumerateA(DSoundEnumPlaybackDevices, NULL);
            if(FAILED(hr))
                ERR("Error enumerating DirectSound playback devices (%#x)!\n", (unsigned int)hr);
            else
            {
                for(i = 0;i < NumPlaybackDevices;i++)
                    AppendAllDevicesList(PlaybackDeviceList[i].name);
            }
            break;

        case CAPTURE_DEVICE_PROBE:
            for(i = 0;i < NumCaptureDevices;++i)
                free(CaptureDeviceList[i].name);
            free(CaptureDeviceList);
            CaptureDeviceList = NULL;
            NumCaptureDevices = 0;

            /* Initialize COM to prevent name truncation */
            hrcom = CoInitialize(NULL);
            hr = DirectSoundCaptureEnumerateA(DSoundEnumCaptureDevices, NULL);
            if(FAILED(hr))
                ERR("Error enumerating DirectSound capture devices (%#x)!\n", (unsigned int)hr);
            else
            {
                for(i = 0;i < NumCaptureDevices;i++)
                    AppendCaptureDeviceList(CaptureDeviceList[i].name);
            }
            if(SUCCEEDED(hrcom))
                CoUninitialize();
            break;
    }
}
