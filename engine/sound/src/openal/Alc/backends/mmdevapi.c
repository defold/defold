/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by authors.
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

#define COBJMACROS
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cguid.h>
#include <devpropdef.h>
#include <mmreg.h>
#include <propsys.h>
#include <propkey.h>
#include <devpkey.h>
#ifndef _WAVEFORMATEXTENSIBLE_
#include <ks.h>
#include <ksmedia.h>
#endif

#include "alMain.h"
#include "alu.h"


DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80,0x20, 0x67,0xd1,0x46,0xa8,0x50,0xe0, 14);

#define MONO SPEAKER_FRONT_CENTER
#define STEREO (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT)
#define QUAD (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT)
#define X5DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT)
#define X5DOT1SIDE (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)
#define X6DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)
#define X7DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)


typedef struct {
    WCHAR *devid;

    IMMDevice *mmdev;
    IAudioClient *client;
    IAudioRenderClient *render;
    HANDLE NotifyEvent;

    HANDLE MsgEvent;

    volatile UINT32 Padding;

    volatile int killNow;
    ALvoid *thread;
} MMDevApiData;


typedef struct {
    ALCchar *name;
    WCHAR *devid;
} DevMap;

static DevMap *PlaybackDeviceList;
static ALuint NumPlaybackDevices;
static DevMap *CaptureDeviceList;
static ALuint NumCaptureDevices;


static HANDLE ThreadHdl;
static DWORD ThreadID;

typedef struct {
    HANDLE FinishedEvt;
    HRESULT result;
} ThreadRequest;

#define WM_USER_First       (WM_USER+0)
#define WM_USER_OpenDevice  (WM_USER+0)
#define WM_USER_ResetDevice (WM_USER+1)
#define WM_USER_StartDevice (WM_USER+2)
#define WM_USER_StopDevice  (WM_USER+3)
#define WM_USER_CloseDevice (WM_USER+4)
#define WM_USER_Enumerate   (WM_USER+5)
#define WM_USER_Last        (WM_USER+5)

static HRESULT WaitForResponse(ThreadRequest *req)
{
    if(WaitForSingleObject(req->FinishedEvt, INFINITE) == WAIT_OBJECT_0)
        return req->result;
    ERR("Message response error: %lu\n", GetLastError());
    return E_FAIL;
}


static ALCchar *get_device_name(IMMDevice *device)
{
    ALCchar *name = NULL;
    IPropertyStore *ps;
    PROPVARIANT pvname;
    HRESULT hr;
    int len;

    hr = IMMDevice_OpenPropertyStore(device, STGM_READ, &ps);
    if(FAILED(hr))
    {
        WARN("OpenPropertyStore failed: 0x%08lx\n", hr);
        return calloc(1, 1);
    }

    PropVariantInit(&pvname);

    hr = IPropertyStore_GetValue(ps, (const PROPERTYKEY*)&DEVPKEY_Device_FriendlyName, &pvname);
    if(FAILED(hr))
    {
        WARN("GetValue failed: 0x%08lx\n", hr);
        name = calloc(1, 1);
    }
    else
    {
        if((len=WideCharToMultiByte(CP_ACP, 0, pvname.pwszVal, -1, NULL, 0, NULL, NULL)) > 0)
        {
            name = calloc(1, len);
            WideCharToMultiByte(CP_ACP, 0, pvname.pwszVal, -1, name, len, NULL, NULL);
        }
    }

    PropVariantClear(&pvname);
    IPropertyStore_Release(ps);

    return name;
}

static void add_device(IMMDevice *device, DevMap *devmap)
{
    LPWSTR devid;
    HRESULT hr;

    hr = IMMDevice_GetId(device, &devid);
    if(SUCCEEDED(hr))
    {
        devmap->devid = strdupW(devid);
        devmap->name = get_device_name(device);
        TRACE("Got device \"%s\", \"%ls\"\n", devmap->name, devmap->devid);
        CoTaskMemFree(devid);
    }
}

static DevMap *ProbeDevices(IMMDeviceEnumerator *devenum, EDataFlow flowdir, ALuint *numdevs)
{
    IMMDeviceCollection *coll;
    IMMDevice *defdev = NULL;
    DevMap *devlist = NULL;
    HRESULT hr;
    UINT count;
    UINT idx;
    UINT i;

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(devenum, flowdir, DEVICE_STATE_ACTIVE, &coll);
    if(FAILED(hr))
    {
        ERR("Failed to enumerate audio endpoints: 0x%08lx\n", hr);
        return NULL;
    }

    idx = count = 0;
    hr = IMMDeviceCollection_GetCount(coll, &count);
    if(SUCCEEDED(hr) && count > 0)
    {
        devlist = calloc(count, sizeof(*devlist));
        if(!devlist)
        {
            IMMDeviceCollection_Release(coll);
            return NULL;
        }

        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devenum, flowdir,
                                                         eMultimedia, &defdev);
    }
    if(SUCCEEDED(hr) && defdev != NULL)
        add_device(defdev, &devlist[idx++]);

    for(i = 0;i < count && idx < count;++i)
    {
        IMMDevice *device;

        if(FAILED(IMMDeviceCollection_Item(coll, i, &device)))
            continue;

        if(device != defdev)
            add_device(device, &devlist[idx++]);

        IMMDevice_Release(device);
    }

    if(defdev) IMMDevice_Release(defdev);
    IMMDeviceCollection_Release(coll);

    *numdevs = idx;
    return devlist;
}


static ALuint MMDevApiProc(ALvoid *ptr)
{
    ALCdevice *device = ptr;
    MMDevApiData *data = device->ExtraData;
    UINT32 buffer_len, written;
    ALuint update_size, len;
    BYTE *buffer;
    HRESULT hr;

    hr = CoInitialize(NULL);
    if(FAILED(hr))
    {
        ERR("CoInitialize(NULL) failed: 0x%08lx\n", hr);
        ALCdevice_Lock(device);
        aluHandleDisconnect(device);
        ALCdevice_Unlock(device);
        return 0;
    }

    SetRTPriority();

    update_size = device->UpdateSize;
    buffer_len = update_size * device->NumUpdates;
    while(!data->killNow)
    {
        hr = IAudioClient_GetCurrentPadding(data->client, &written);
        if(FAILED(hr))
        {
            ERR("Failed to get padding: 0x%08lx\n", hr);
            ALCdevice_Lock(device);
            aluHandleDisconnect(device);
            ALCdevice_Unlock(device);
            break;
        }
        data->Padding = written;

        len = buffer_len - written;
        if(len < update_size)
        {
            DWORD res;
            res = WaitForSingleObjectEx(data->NotifyEvent, 2000, FALSE);
            if(res != WAIT_OBJECT_0)
                ERR("WaitForSingleObjectEx error: 0x%lx\n", res);
            continue;
        }
        len -= len%update_size;

        hr = IAudioRenderClient_GetBuffer(data->render, len, &buffer);
        if(SUCCEEDED(hr))
        {
            ALCdevice_Lock(device);
            aluMixData(device, buffer, len);
            data->Padding = written + len;
            ALCdevice_Unlock(device);
            hr = IAudioRenderClient_ReleaseBuffer(data->render, len, 0);
        }
        if(FAILED(hr))
        {
            ERR("Failed to buffer data: 0x%08lx\n", hr);
            ALCdevice_Lock(device);
            aluHandleDisconnect(device);
            ALCdevice_Unlock(device);
            break;
        }
    }
    data->Padding = 0;

    CoUninitialize();
    return 0;
}


static ALCboolean MakeExtensible(WAVEFORMATEXTENSIBLE *out, const WAVEFORMATEX *in)
{
    memset(out, 0, sizeof(*out));
    if(in->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        *out = *(const WAVEFORMATEXTENSIBLE*)in;
    else if(in->wFormatTag == WAVE_FORMAT_PCM)
    {
        out->Format = *in;
        out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        out->Format.cbSize = sizeof(*out) - sizeof(*in);
        if(out->Format.nChannels == 1)
            out->dwChannelMask = MONO;
        else if(out->Format.nChannels == 2)
            out->dwChannelMask = STEREO;
        else
            ERR("Unhandled PCM channel count: %d\n", out->Format.nChannels);
        out->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    }
    else if(in->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        out->Format = *in;
        out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        out->Format.cbSize = sizeof(*out) - sizeof(*in);
        if(out->Format.nChannels == 1)
            out->dwChannelMask = MONO;
        else if(out->Format.nChannels == 2)
            out->dwChannelMask = STEREO;
        else
            ERR("Unhandled IEEE float channel count: %d\n", out->Format.nChannels);
        out->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        ERR("Unhandled format tag: 0x%04x\n", in->wFormatTag);
        return ALC_FALSE;
    }
    return ALC_TRUE;
}

static HRESULT DoReset(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;
    WAVEFORMATEXTENSIBLE OutputType;
    WAVEFORMATEX *wfx = NULL;
    REFERENCE_TIME min_per, buf_time;
    UINT32 buffer_len, min_len;
    HRESULT hr;

    hr = IAudioClient_GetMixFormat(data->client, &wfx);
    if(FAILED(hr))
    {
        ERR("Failed to get mix format: 0x%08lx\n", hr);
        return hr;
    }

    if(!MakeExtensible(&OutputType, wfx))
    {
        CoTaskMemFree(wfx);
        return E_FAIL;
    }
    CoTaskMemFree(wfx);
    wfx = NULL;

    buf_time = ((REFERENCE_TIME)device->UpdateSize*device->NumUpdates*10000000 +
                                device->Frequency-1) / device->Frequency;

    if(!(device->Flags&DEVICE_FREQUENCY_REQUEST))
        device->Frequency = OutputType.Format.nSamplesPerSec;
    if(!(device->Flags&DEVICE_CHANNELS_REQUEST))
    {
        if(OutputType.Format.nChannels == 1 && OutputType.dwChannelMask == MONO)
            device->FmtChans = DevFmtMono;
        else if(OutputType.Format.nChannels == 2 && OutputType.dwChannelMask == STEREO)
            device->FmtChans = DevFmtStereo;
        else if(OutputType.Format.nChannels == 4 && OutputType.dwChannelMask == QUAD)
            device->FmtChans = DevFmtQuad;
        else if(OutputType.Format.nChannels == 6 && OutputType.dwChannelMask == X5DOT1)
            device->FmtChans = DevFmtX51;
        else if(OutputType.Format.nChannels == 6 && OutputType.dwChannelMask == X5DOT1SIDE)
            device->FmtChans = DevFmtX51Side;
        else if(OutputType.Format.nChannels == 7 && OutputType.dwChannelMask == X6DOT1)
            device->FmtChans = DevFmtX61;
        else if(OutputType.Format.nChannels == 8 && OutputType.dwChannelMask == X7DOT1)
            device->FmtChans = DevFmtX71;
        else
            ERR("Unhandled channel config: %d -- 0x%08lx\n", OutputType.Format.nChannels, OutputType.dwChannelMask);
    }

    switch(device->FmtChans)
    {
        case DevFmtMono:
            OutputType.Format.nChannels = 1;
            OutputType.dwChannelMask = MONO;
            break;
        case DevFmtStereo:
            OutputType.Format.nChannels = 2;
            OutputType.dwChannelMask = STEREO;
            break;
        case DevFmtQuad:
            OutputType.Format.nChannels = 4;
            OutputType.dwChannelMask = QUAD;
            break;
        case DevFmtX51:
            OutputType.Format.nChannels = 6;
            OutputType.dwChannelMask = X5DOT1;
            break;
        case DevFmtX51Side:
            OutputType.Format.nChannels = 6;
            OutputType.dwChannelMask = X5DOT1SIDE;
            break;
        case DevFmtX61:
            OutputType.Format.nChannels = 7;
            OutputType.dwChannelMask = X6DOT1;
            break;
        case DevFmtX71:
            OutputType.Format.nChannels = 8;
            OutputType.dwChannelMask = X7DOT1;
            break;
    }
    switch(device->FmtType)
    {
        case DevFmtByte:
            device->FmtType = DevFmtUByte;
            /* fall-through */
        case DevFmtUByte:
            OutputType.Format.wBitsPerSample = 8;
            OutputType.Samples.wValidBitsPerSample = 8;
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case DevFmtUShort:
            device->FmtType = DevFmtShort;
            /* fall-through */
        case DevFmtShort:
            OutputType.Format.wBitsPerSample = 16;
            OutputType.Samples.wValidBitsPerSample = 16;
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case DevFmtUInt:
            device->FmtType = DevFmtInt;
            /* fall-through */
        case DevFmtInt:
            OutputType.Format.wBitsPerSample = 32;
            OutputType.Samples.wValidBitsPerSample = 32;
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case DevFmtFloat:
            OutputType.Format.wBitsPerSample = 32;
            OutputType.Samples.wValidBitsPerSample = 32;
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
    }
    OutputType.Format.nSamplesPerSec = device->Frequency;

    OutputType.Format.nBlockAlign = OutputType.Format.nChannels *
                                    OutputType.Format.wBitsPerSample / 8;
    OutputType.Format.nAvgBytesPerSec = OutputType.Format.nSamplesPerSec *
                                        OutputType.Format.nBlockAlign;

    hr = IAudioClient_IsFormatSupported(data->client, AUDCLNT_SHAREMODE_SHARED, &OutputType.Format, &wfx);
    if(FAILED(hr))
    {
        ERR("Failed to check format support: 0x%08lx\n", hr);
        hr = IAudioClient_GetMixFormat(data->client, &wfx);
    }
    if(FAILED(hr))
    {
        ERR("Failed to find a supported format: 0x%08lx\n", hr);
        return hr;
    }

    if(wfx != NULL)
    {
        if(!MakeExtensible(&OutputType, wfx))
        {
            CoTaskMemFree(wfx);
            return E_FAIL;
        }
        CoTaskMemFree(wfx);
        wfx = NULL;

        device->Frequency = OutputType.Format.nSamplesPerSec;
        if(OutputType.Format.nChannels == 1 && OutputType.dwChannelMask == MONO)
            device->FmtChans = DevFmtMono;
        else if(OutputType.Format.nChannels == 2 && OutputType.dwChannelMask == STEREO)
            device->FmtChans = DevFmtStereo;
        else if(OutputType.Format.nChannels == 4 && OutputType.dwChannelMask == QUAD)
            device->FmtChans = DevFmtQuad;
        else if(OutputType.Format.nChannels == 6 && OutputType.dwChannelMask == X5DOT1)
            device->FmtChans = DevFmtX51;
        else if(OutputType.Format.nChannels == 6 && OutputType.dwChannelMask == X5DOT1SIDE)
            device->FmtChans = DevFmtX51Side;
        else if(OutputType.Format.nChannels == 7 && OutputType.dwChannelMask == X6DOT1)
            device->FmtChans = DevFmtX61;
        else if(OutputType.Format.nChannels == 8 && OutputType.dwChannelMask == X7DOT1)
            device->FmtChans = DevFmtX71;
        else
        {
            ERR("Unhandled extensible channels: %d -- 0x%08lx\n", OutputType.Format.nChannels, OutputType.dwChannelMask);
            device->FmtChans = DevFmtStereo;
            OutputType.Format.nChannels = 2;
            OutputType.dwChannelMask = STEREO;
        }

        if(IsEqualGUID(&OutputType.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM))
        {
            if(OutputType.Format.wBitsPerSample == 8)
                device->FmtType = DevFmtUByte;
            else if(OutputType.Format.wBitsPerSample == 16)
                device->FmtType = DevFmtShort;
            else if(OutputType.Format.wBitsPerSample == 32)
                device->FmtType = DevFmtInt;
            else
            {
                device->FmtType = DevFmtShort;
                OutputType.Format.wBitsPerSample = 16;
            }
        }
        else if(IsEqualGUID(&OutputType.SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
        {
            device->FmtType = DevFmtFloat;
            OutputType.Format.wBitsPerSample = 32;
        }
        else
        {
            ERR("Unhandled format sub-type\n");
            device->FmtType = DevFmtShort;
            OutputType.Format.wBitsPerSample = 16;
            OutputType.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        }
        OutputType.Samples.wValidBitsPerSample = OutputType.Format.wBitsPerSample;
    }

    SetDefaultWFXChannelOrder(device);

    hr = IAudioClient_Initialize(data->client, AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 buf_time, 0, &OutputType.Format, NULL);
    if(FAILED(hr))
    {
        ERR("Failed to initialize audio client: 0x%08lx\n", hr);
        return hr;
    }

    hr = IAudioClient_GetDevicePeriod(data->client, &min_per, NULL);
    if(SUCCEEDED(hr))
    {
        min_len = (UINT32)((min_per*device->Frequency + 10000000-1) / 10000000);
        /* Find the nearest multiple of the period size to the update size */
        if(min_len < device->UpdateSize)
            min_len *= (device->UpdateSize + min_len/2)/min_len;
        hr = IAudioClient_GetBufferSize(data->client, &buffer_len);
    }
    if(FAILED(hr))
    {
        ERR("Failed to get audio buffer info: 0x%08lx\n", hr);
        return hr;
    }

    device->UpdateSize = min_len;
    device->NumUpdates = buffer_len / device->UpdateSize;
    if(device->NumUpdates <= 1)
    {
        ERR("Audio client returned buffer_len < period*2; expect break up\n");
        device->NumUpdates = 2;
        device->UpdateSize = buffer_len / device->NumUpdates;
    }

    return hr;
}


static DWORD CALLBACK MMDevApiMsgProc(void *ptr)
{
    ThreadRequest *req = ptr;
    IMMDeviceEnumerator *Enumerator;
    ALuint deviceCount = 0;
    MMDevApiData *data;
    ALCdevice *device;
    HRESULT hr, cohr;
    MSG msg;

    TRACE("Starting message thread\n");

    cohr = CoInitialize(NULL);
    if(FAILED(cohr))
    {
        WARN("Failed to initialize COM: 0x%08lx\n", cohr);
        req->result = cohr;
        SetEvent(req->FinishedEvt);
        return 0;
    }

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &ptr);
    if(FAILED(hr))
    {
        WARN("Failed to create IMMDeviceEnumerator instance: 0x%08lx\n", hr);
        CoUninitialize();
        req->result = hr;
        SetEvent(req->FinishedEvt);
        return 0;
    }
    Enumerator = ptr;
    IMMDeviceEnumerator_Release(Enumerator);
    Enumerator = NULL;

    CoUninitialize();

    req->result = S_OK;
    SetEvent(req->FinishedEvt);

    TRACE("Starting message loop\n");
    while(GetMessage(&msg, NULL, WM_USER_First, WM_USER_Last))
    {
        TRACE("Got message %u\n", msg.message);
        switch(msg.message)
        {
        case WM_USER_OpenDevice:
            req = (ThreadRequest*)msg.wParam;
            device = (ALCdevice*)msg.lParam;
            data = device->ExtraData;

            hr = cohr = S_OK;
            if(++deviceCount == 1)
                hr = cohr = CoInitialize(NULL);
            if(SUCCEEDED(hr))
                hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &ptr);
            if(SUCCEEDED(hr))
            {
                Enumerator = ptr;
                if(!data->devid)
                    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(Enumerator, eRender, eMultimedia, &data->mmdev);
                else
                    hr = IMMDeviceEnumerator_GetDevice(Enumerator, data->devid, &data->mmdev);
                IMMDeviceEnumerator_Release(Enumerator);
                Enumerator = NULL;
            }
            if(SUCCEEDED(hr))
                hr = IMMDevice_Activate(data->mmdev, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, &ptr);
            if(SUCCEEDED(hr))
            {
                data->client = ptr;
                device->DeviceName = get_device_name(data->mmdev);
            }

            if(FAILED(hr))
            {
                if(data->mmdev)
                    IMMDevice_Release(data->mmdev);
                data->mmdev = NULL;
                if(--deviceCount == 0 && SUCCEEDED(cohr))
                    CoUninitialize();
            }

            req->result = hr;
            SetEvent(req->FinishedEvt);
            continue;

        case WM_USER_ResetDevice:
            req = (ThreadRequest*)msg.wParam;
            device = (ALCdevice*)msg.lParam;

            req->result = DoReset(device);
            SetEvent(req->FinishedEvt);
            continue;

        case WM_USER_StartDevice:
            req = (ThreadRequest*)msg.wParam;
            device = (ALCdevice*)msg.lParam;
            data = device->ExtraData;

            ResetEvent(data->NotifyEvent);
            hr = IAudioClient_SetEventHandle(data->client, data->NotifyEvent);
            if(FAILED(hr))
                ERR("Failed to set event handle: 0x%08lx\n", hr);
            else
            {
                hr = IAudioClient_Start(data->client);
                if(FAILED(hr))
                    ERR("Failed to start audio client: 0x%08lx\n", hr);
            }

            if(SUCCEEDED(hr))
                hr = IAudioClient_GetService(data->client, &IID_IAudioRenderClient, &ptr);
            if(SUCCEEDED(hr))
            {
                data->render = ptr;
                data->thread = StartThread(MMDevApiProc, device);
                if(!data->thread)
                {
                    if(data->render)
                        IAudioRenderClient_Release(data->render);
                    data->render = NULL;
                    IAudioClient_Stop(data->client);
                    ERR("Failed to start thread\n");
                    hr = E_FAIL;
                }
            }

            req->result = hr;
            SetEvent(req->FinishedEvt);
            continue;

        case WM_USER_StopDevice:
            req = (ThreadRequest*)msg.wParam;
            device = (ALCdevice*)msg.lParam;
            data = device->ExtraData;

            if(data->thread)
            {
                data->killNow = 1;
                StopThread(data->thread);
                data->thread = NULL;

                data->killNow = 0;

                IAudioRenderClient_Release(data->render);
                data->render = NULL;
                IAudioClient_Stop(data->client);
            }

            req->result = S_OK;
            SetEvent(req->FinishedEvt);
            continue;

        case WM_USER_CloseDevice:
            req = (ThreadRequest*)msg.wParam;
            device = (ALCdevice*)msg.lParam;
            data = device->ExtraData;

            IAudioClient_Release(data->client);
            data->client = NULL;

            IMMDevice_Release(data->mmdev);
            data->mmdev = NULL;

            if(--deviceCount == 0)
                CoUninitialize();

            req->result = S_OK;
            SetEvent(req->FinishedEvt);
            continue;

        case WM_USER_Enumerate:
            req = (ThreadRequest*)msg.wParam;

            hr = cohr = S_OK;
            if(++deviceCount == 1)
                hr = cohr = CoInitialize(NULL);
            if(SUCCEEDED(hr))
                hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &ptr);
            if(SUCCEEDED(hr))
            {
                EDataFlow flowdir;
                DevMap **devlist;
                ALuint *numdevs;
                ALuint i;

                Enumerator = ptr;
                if(msg.lParam == CAPTURE_DEVICE_PROBE)
                {
                    flowdir = eCapture;
                    devlist = &CaptureDeviceList;
                    numdevs = &NumCaptureDevices;
                }
                else
                {
                    flowdir = eRender;
                    devlist = &PlaybackDeviceList;
                    numdevs = &NumPlaybackDevices;
                }

                for(i = 0;i < *numdevs;i++)
                {
                    free((*devlist)[i].name);
                    free((*devlist)[i].devid);
                }
                free(*devlist);
                *devlist = NULL;
                *numdevs = 0;

                *devlist = ProbeDevices(Enumerator, flowdir, numdevs);

                IMMDeviceEnumerator_Release(Enumerator);
                Enumerator = NULL;
            }

            if(--deviceCount == 0 && SUCCEEDED(cohr))
                CoUninitialize();

            req->result = S_OK;
            SetEvent(req->FinishedEvt);
            continue;

        default:
            ERR("Unexpected message: %u\n", msg.message);
            continue;
        }
    }
    TRACE("Message loop finished\n");

    return 0;
}


static BOOL MMDevApiLoad(void)
{
    static HRESULT InitResult;
    if(!ThreadHdl)
    {
        ThreadRequest req;
        InitResult = E_FAIL;

        req.FinishedEvt = CreateEvent(NULL, FALSE, FALSE, NULL);
        if(req.FinishedEvt == NULL)
            ERR("Failed to create event: %lu\n", GetLastError());
        else
        {
            ThreadHdl = CreateThread(NULL, 0, MMDevApiMsgProc, &req, 0, &ThreadID);
            if(ThreadHdl != NULL)
                InitResult = WaitForResponse(&req);
            CloseHandle(req.FinishedEvt);
        }
    }
    return SUCCEEDED(InitResult);
}


static ALCenum MMDevApiOpenPlayback(ALCdevice *device, const ALCchar *deviceName)
{
    MMDevApiData *data = NULL;
    HRESULT hr;

    //Initialise requested device
    data = calloc(1, sizeof(MMDevApiData));
    if(!data)
        return ALC_OUT_OF_MEMORY;
    device->ExtraData = data;

    hr = S_OK;
    data->NotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    data->MsgEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(data->NotifyEvent == NULL || data->MsgEvent == NULL)
        hr = E_FAIL;

    if(SUCCEEDED(hr))
    {
        if(deviceName)
        {
            ALuint i;

            if(!PlaybackDeviceList)
            {
                ThreadRequest req = { data->MsgEvent, 0 };
                if(PostThreadMessage(ThreadID, WM_USER_Enumerate, (WPARAM)&req, ALL_DEVICE_PROBE))
                    (void)WaitForResponse(&req);
            }

            hr = E_FAIL;
            for(i = 0;i < NumPlaybackDevices;i++)
            {
                if(strcmp(deviceName, PlaybackDeviceList[i].name) == 0)
                {
                    data->devid = strdupW(PlaybackDeviceList[i].devid);
                    hr = S_OK;
                    break;
                }
            }
        }
    }

    if(SUCCEEDED(hr))
    {
        ThreadRequest req = { data->MsgEvent, 0 };

        hr = E_FAIL;
        if(PostThreadMessage(ThreadID, WM_USER_OpenDevice, (WPARAM)&req, (LPARAM)device))
            hr = WaitForResponse(&req);
    }

    if(FAILED(hr))
    {
        if(data->NotifyEvent != NULL)
            CloseHandle(data->NotifyEvent);
        data->NotifyEvent = NULL;
        if(data->MsgEvent != NULL)
            CloseHandle(data->MsgEvent);
        data->MsgEvent = NULL;

        free(data->devid);
        data->devid = NULL;

        free(data);
        device->ExtraData = NULL;

        ERR("Device init failed: 0x%08lx\n", hr);
        return ALC_INVALID_VALUE;
    }

    return ALC_NO_ERROR;
}

static void MMDevApiClosePlayback(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;
    ThreadRequest req = { data->MsgEvent, 0 };

    if(PostThreadMessage(ThreadID, WM_USER_CloseDevice, (WPARAM)&req, (LPARAM)device))
        (void)WaitForResponse(&req);

    CloseHandle(data->MsgEvent);
    data->MsgEvent = NULL;

    CloseHandle(data->NotifyEvent);
    data->NotifyEvent = NULL;

    free(data->devid);
    data->devid = NULL;

    free(data);
    device->ExtraData = NULL;
}

static ALCboolean MMDevApiResetPlayback(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;
    ThreadRequest req = { data->MsgEvent, 0 };
    HRESULT hr = E_FAIL;

    if(PostThreadMessage(ThreadID, WM_USER_ResetDevice, (WPARAM)&req, (LPARAM)device))
        hr = WaitForResponse(&req);

    return SUCCEEDED(hr) ? ALC_TRUE : ALC_FALSE;
}

static ALCboolean MMDevApiStartPlayback(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;
    ThreadRequest req = { data->MsgEvent, 0 };
    HRESULT hr = E_FAIL;

    if(PostThreadMessage(ThreadID, WM_USER_StartDevice, (WPARAM)&req, (LPARAM)device))
        hr = WaitForResponse(&req);

    return SUCCEEDED(hr) ? ALC_TRUE : ALC_FALSE;
}

static void MMDevApiStopPlayback(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;
    ThreadRequest req = { data->MsgEvent, 0 };

    if(PostThreadMessage(ThreadID, WM_USER_StopDevice, (WPARAM)&req, (LPARAM)device))
        (void)WaitForResponse(&req);
}


static ALint64 MMDevApiGetLatency(ALCdevice *device)
{
    MMDevApiData *data = device->ExtraData;

    return (ALint64)data->Padding * 1000000000 / device->Frequency;
}


static const BackendFuncs MMDevApiFuncs = {
    MMDevApiOpenPlayback,
    MMDevApiClosePlayback,
    MMDevApiResetPlayback,
    MMDevApiStartPlayback,
    MMDevApiStopPlayback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    MMDevApiGetLatency
};


ALCboolean alcMMDevApiInit(BackendFuncs *FuncList)
{
    if(!MMDevApiLoad())
        return ALC_FALSE;
    *FuncList = MMDevApiFuncs;
    return ALC_TRUE;
}

void alcMMDevApiDeinit(void)
{
    ALuint i;

    for(i = 0;i < NumPlaybackDevices;i++)
    {
        free(PlaybackDeviceList[i].name);
        free(PlaybackDeviceList[i].devid);
    }
    free(PlaybackDeviceList);
    PlaybackDeviceList = NULL;
    NumPlaybackDevices = 0;

    for(i = 0;i < NumCaptureDevices;i++)
    {
        free(CaptureDeviceList[i].name);
        free(CaptureDeviceList[i].devid);
    }
    free(CaptureDeviceList);
    CaptureDeviceList = NULL;
    NumCaptureDevices = 0;

    if(ThreadHdl)
    {
        TRACE("Sending WM_QUIT to Thread %04lx\n", ThreadID);
        PostThreadMessage(ThreadID, WM_QUIT, 0, 0);
        CloseHandle(ThreadHdl);
        ThreadHdl = NULL;
    }
}

void alcMMDevApiProbe(enum DevProbe type)
{
    ThreadRequest req = { NULL, 0 };
    HRESULT hr = E_FAIL;

    switch(type)
    {
        case ALL_DEVICE_PROBE:
            req.FinishedEvt = CreateEvent(NULL, FALSE, FALSE, NULL);
            if(req.FinishedEvt == NULL)
                ERR("Failed to create event: %lu\n", GetLastError());
            else if(PostThreadMessage(ThreadID, WM_USER_Enumerate, (WPARAM)&req, type))
                hr = WaitForResponse(&req);
            if(SUCCEEDED(hr))
            {
                ALuint i;
                for(i = 0;i < NumPlaybackDevices;i++)
                {
                    if(PlaybackDeviceList[i].name)
                        AppendAllDevicesList(PlaybackDeviceList[i].name);
                }
            }
            break;

        case CAPTURE_DEVICE_PROBE:
            break;
    }
    if(req.FinishedEvt != NULL)
        CloseHandle(req.FinishedEvt);
    req.FinishedEvt = NULL;
}
