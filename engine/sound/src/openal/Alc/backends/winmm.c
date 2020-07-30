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

#include <windows.h>
#include <mmsystem.h>

#include "alMain.h"
#include "alu.h"

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT  0x0003
#endif


typedef struct {
    // MMSYSTEM Device
    volatile ALboolean killNow;
    HANDLE  WaveThreadEvent;
    HANDLE  WaveThread;
    DWORD   WaveThreadID;
    volatile LONG WaveBuffersCommitted;
    WAVEHDR WaveBuffer[4];

    union {
        HWAVEIN  In;
        HWAVEOUT Out;
    } WaveHandle;

    WAVEFORMATEX Format;

    RingBuffer *Ring;
} WinMMData;


static ALCchar **PlaybackDeviceList;
static ALuint  NumPlaybackDevices;
static ALCchar **CaptureDeviceList;
static ALuint  NumCaptureDevices;


static void ProbePlaybackDevices(void)
{
    ALuint i;

    for(i = 0;i < NumPlaybackDevices;i++)
        free(PlaybackDeviceList[i]);

    NumPlaybackDevices = waveOutGetNumDevs();
    PlaybackDeviceList = realloc(PlaybackDeviceList, sizeof(ALCchar*) * NumPlaybackDevices);
    for(i = 0;i < NumPlaybackDevices;i++)
    {
        WAVEOUTCAPS WaveCaps;

        PlaybackDeviceList[i] = NULL;
        if(waveOutGetDevCaps(i, &WaveCaps, sizeof(WaveCaps)) == MMSYSERR_NOERROR)
        {
            char name[1024];
            ALuint count, j;

            count = 0;
            do {
                if(count == 0)
                    snprintf(name, sizeof(name), "%s", WaveCaps.szPname);
                else
                    snprintf(name, sizeof(name), "%s #%d", WaveCaps.szPname, count+1);
                count++;

                for(j = 0;j < i;j++)
                {
                    if(strcmp(name, PlaybackDeviceList[j]) == 0)
                        break;
                }
            } while(j != i);

            PlaybackDeviceList[i] = strdup(name);
        }
    }
}

static void ProbeCaptureDevices(void)
{
    ALuint i;

    for(i = 0;i < NumCaptureDevices;i++)
        free(CaptureDeviceList[i]);

    NumCaptureDevices = waveInGetNumDevs();
    CaptureDeviceList = realloc(CaptureDeviceList, sizeof(ALCchar*) * NumCaptureDevices);
    for(i = 0;i < NumCaptureDevices;i++)
    {
        WAVEINCAPS WaveInCaps;

        CaptureDeviceList[i] = NULL;
        if(waveInGetDevCaps(i, &WaveInCaps, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR)
        {
            char name[1024];
            ALuint count, j;

            count = 0;
            do {
                if(count == 0)
                    snprintf(name, sizeof(name), "%s", WaveInCaps.szPname);
                else
                    snprintf(name, sizeof(name), "%s #%d", WaveInCaps.szPname, count+1);
                count++;

                for(j = 0;j < i;j++)
                {
                    if(strcmp(name, CaptureDeviceList[j]) == 0)
                        break;
                }
            } while(j != i);

            CaptureDeviceList[i] = strdup(name);
        }
    }
}


/*
    WaveOutProc

    Posts a message to 'PlaybackThreadProc' everytime a WaveOut Buffer is completed and
    returns to the application (for more data)
*/
static void CALLBACK WaveOutProc(HWAVEOUT device, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2)
{
    ALCdevice *Device = (ALCdevice*)instance;
    WinMMData *data = Device->ExtraData;

    (void)device;
    (void)param2;

    if(msg != WOM_DONE)
        return;

    InterlockedDecrement(&data->WaveBuffersCommitted);
    PostThreadMessage(data->WaveThreadID, msg, 0, param1);
}

/*
    PlaybackThreadProc

    Used by "MMSYSTEM" Device.  Called when a WaveOut buffer has used up its
    audio data.
*/
static DWORD WINAPI PlaybackThreadProc(LPVOID param)
{
    ALCdevice *Device = (ALCdevice*)param;
    WinMMData *data = Device->ExtraData;
    LPWAVEHDR WaveHdr;
    ALuint FrameSize;
    MSG msg;

    FrameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);

    SetRTPriority();

    while(GetMessage(&msg, NULL, 0, 0))
    {
        if(msg.message != WOM_DONE)
            continue;

        if(data->killNow)
        {
            if(data->WaveBuffersCommitted == 0)
                break;
            continue;
        }

        WaveHdr = ((LPWAVEHDR)msg.lParam);
        aluMixData(Device, WaveHdr->lpData, WaveHdr->dwBufferLength/FrameSize);

        // Send buffer back to play more data
        waveOutWrite(data->WaveHandle.Out, WaveHdr, sizeof(WAVEHDR));
        InterlockedIncrement(&data->WaveBuffersCommitted);
    }

    // Signal Wave Thread completed event
    if(data->WaveThreadEvent)
        SetEvent(data->WaveThreadEvent);

    ExitThread(0);
    return 0;
}

/*
    WaveInProc

    Posts a message to 'CaptureThreadProc' everytime a WaveIn Buffer is completed and
    returns to the application (with more data)
*/
static void CALLBACK WaveInProc(HWAVEIN device, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2)
{
    ALCdevice *Device = (ALCdevice*)instance;
    WinMMData *data = Device->ExtraData;

    (void)device;
    (void)param2;

    if(msg != WIM_DATA)
        return;

    InterlockedDecrement(&data->WaveBuffersCommitted);
    PostThreadMessage(data->WaveThreadID, msg, 0, param1);
}

/*
    CaptureThreadProc

    Used by "MMSYSTEM" Device.  Called when a WaveIn buffer had been filled with new
    audio data.
*/
static DWORD WINAPI CaptureThreadProc(LPVOID param)
{
    ALCdevice *Device = (ALCdevice*)param;
    WinMMData *data = Device->ExtraData;
    LPWAVEHDR WaveHdr;
    ALuint FrameSize;
    MSG msg;

    FrameSize = FrameSizeFromDevFmt(Device->FmtChans, Device->FmtType);

    while(GetMessage(&msg, NULL, 0, 0))
    {
        if(msg.message != WIM_DATA)
            continue;
        /* Don't wait for other buffers to finish before quitting. We're
         * closing so we don't need them. */
        if(data->killNow)
            break;

        WaveHdr = ((LPWAVEHDR)msg.lParam);
        WriteRingBuffer(data->Ring, (ALubyte*)WaveHdr->lpData, WaveHdr->dwBytesRecorded/FrameSize);

        // Send buffer back to capture more data
        waveInAddBuffer(data->WaveHandle.In, WaveHdr, sizeof(WAVEHDR));
        InterlockedIncrement(&data->WaveBuffersCommitted);
    }

    // Signal Wave Thread completed event
    if(data->WaveThreadEvent)
        SetEvent(data->WaveThreadEvent);

    ExitThread(0);
    return 0;
}


static ALCenum WinMMOpenPlayback(ALCdevice *Device, const ALCchar *deviceName)
{
    WinMMData *data = NULL;
    UINT DeviceID = 0;
    MMRESULT res;
    ALuint i = 0;

    if(!PlaybackDeviceList)
        ProbePlaybackDevices();

    // Find the Device ID matching the deviceName if valid
    for(i = 0;i < NumPlaybackDevices;i++)
    {
        if(PlaybackDeviceList[i] &&
           (!deviceName || strcmp(deviceName, PlaybackDeviceList[i]) == 0))
        {
            DeviceID = i;
            break;
        }
    }
    if(i == NumPlaybackDevices)
        return ALC_INVALID_VALUE;

    data = calloc(1, sizeof(*data));
    if(!data)
        return ALC_OUT_OF_MEMORY;
    Device->ExtraData = data;

retry_open:
    memset(&data->Format, 0, sizeof(WAVEFORMATEX));
    if(Device->FmtType == DevFmtFloat)
    {
        data->Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        data->Format.wBitsPerSample = 32;
    }
    else
    {
        data->Format.wFormatTag = WAVE_FORMAT_PCM;
        if(Device->FmtType == DevFmtUByte || Device->FmtType == DevFmtByte)
            data->Format.wBitsPerSample = 8;
        else
            data->Format.wBitsPerSample = 16;
    }
    data->Format.nChannels = ((Device->FmtChans == DevFmtMono) ? 1 : 2);
    data->Format.nBlockAlign = data->Format.wBitsPerSample *
                               data->Format.nChannels / 8;
    data->Format.nSamplesPerSec = Device->Frequency;
    data->Format.nAvgBytesPerSec = data->Format.nSamplesPerSec *
                                   data->Format.nBlockAlign;
    data->Format.cbSize = 0;

    if((res=waveOutOpen(&data->WaveHandle.Out, DeviceID, &data->Format, (DWORD_PTR)&WaveOutProc, (DWORD_PTR)Device, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
    {
        if(Device->FmtType == DevFmtFloat)
        {
            Device->FmtType = DevFmtShort;
            goto retry_open;
        }
        ERR("waveOutOpen failed: %u\n", res);
        goto failure;
    }

    data->WaveThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(data->WaveThreadEvent == NULL)
    {
        ERR("CreateEvent failed: %lu\n", GetLastError());
        goto failure;
    }

    Device->DeviceName = strdup(PlaybackDeviceList[DeviceID]);
    return ALC_NO_ERROR;

failure:
    if(data->WaveThreadEvent)
        CloseHandle(data->WaveThreadEvent);

    if(data->WaveHandle.Out)
        waveOutClose(data->WaveHandle.Out);

    free(data);
    Device->ExtraData = NULL;
    return ALC_INVALID_VALUE;
}

static void WinMMClosePlayback(ALCdevice *device)
{
    WinMMData *data = (WinMMData*)device->ExtraData;

    // Close the Wave device
    CloseHandle(data->WaveThreadEvent);
    data->WaveThreadEvent = 0;

    waveOutClose(data->WaveHandle.Out);
    data->WaveHandle.Out = 0;

    free(data);
    device->ExtraData = NULL;
}

static ALCboolean WinMMResetPlayback(ALCdevice *device)
{
    WinMMData *data = (WinMMData*)device->ExtraData;

    device->UpdateSize = (ALuint)((ALuint64)device->UpdateSize *
                                  data->Format.nSamplesPerSec /
                                  device->Frequency);
    device->UpdateSize = (device->UpdateSize*device->NumUpdates + 3) / 4;
    device->NumUpdates = 4;
    device->Frequency = data->Format.nSamplesPerSec;

    if(data->Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        if(data->Format.wBitsPerSample == 32)
            device->FmtType = DevFmtFloat;
        else
        {
            ERR("Unhandled IEEE float sample depth: %d\n", data->Format.wBitsPerSample);
            return ALC_FALSE;
        }
    }
    else if(data->Format.wFormatTag == WAVE_FORMAT_PCM)
    {
        if(data->Format.wBitsPerSample == 16)
            device->FmtType = DevFmtShort;
        else if(data->Format.wBitsPerSample == 8)
            device->FmtType = DevFmtUByte;
        else
        {
            ERR("Unhandled PCM sample depth: %d\n", data->Format.wBitsPerSample);
            return ALC_FALSE;
        }
    }
    else
    {
        ERR("Unhandled format tag: 0x%04x\n", data->Format.wFormatTag);
        return ALC_FALSE;
    }

    if(data->Format.nChannels == 2)
        device->FmtChans = DevFmtStereo;
    else if(data->Format.nChannels == 1)
        device->FmtChans = DevFmtMono;
    else
    {
        ERR("Unhandled channel count: %d\n", data->Format.nChannels);
        return ALC_FALSE;
    }
    SetDefaultWFXChannelOrder(device);

    return ALC_TRUE;
}

static ALCboolean WinMMStartPlayback(ALCdevice *device)
{
    WinMMData *data = (WinMMData*)device->ExtraData;
    ALbyte *BufferData;
    ALint BufferSize;
    ALuint i;

    data->WaveThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PlaybackThreadProc, (LPVOID)device, 0, &data->WaveThreadID);
    if(data->WaveThread == NULL)
        return ALC_FALSE;

    data->WaveBuffersCommitted = 0;

    // Create 4 Buffers
    BufferSize  = device->UpdateSize*device->NumUpdates / 4;
    BufferSize *= FrameSizeFromDevFmt(device->FmtChans, device->FmtType);

    BufferData = calloc(4, BufferSize);
    for(i = 0;i < 4;i++)
    {
        memset(&data->WaveBuffer[i], 0, sizeof(WAVEHDR));
        data->WaveBuffer[i].dwBufferLength = BufferSize;
        data->WaveBuffer[i].lpData = ((i==0) ? (LPSTR)BufferData :
                                      (data->WaveBuffer[i-1].lpData +
                                       data->WaveBuffer[i-1].dwBufferLength));
        waveOutPrepareHeader(data->WaveHandle.Out, &data->WaveBuffer[i], sizeof(WAVEHDR));
        waveOutWrite(data->WaveHandle.Out, &data->WaveBuffer[i], sizeof(WAVEHDR));
        InterlockedIncrement(&data->WaveBuffersCommitted);
    }

    return ALC_TRUE;
}

static void WinMMStopPlayback(ALCdevice *device)
{
    WinMMData *data = (WinMMData*)device->ExtraData;
    void *buffer = NULL;
    int i;

    if(data->WaveThread == NULL)
        return;

    // Set flag to stop processing headers
    data->killNow = AL_TRUE;

    // Wait for signal that Wave Thread has been destroyed
    WaitForSingleObjectEx(data->WaveThreadEvent, 5000, FALSE);

    CloseHandle(data->WaveThread);
    data->WaveThread = 0;

    data->killNow = AL_FALSE;

    // Release the wave buffers
    for(i = 0;i < 4;i++)
    {
        waveOutUnprepareHeader(data->WaveHandle.Out, &data->WaveBuffer[i], sizeof(WAVEHDR));
        if(i == 0) buffer = data->WaveBuffer[i].lpData;
        data->WaveBuffer[i].lpData = NULL;
    }
    free(buffer);
}


static ALCenum WinMMOpenCapture(ALCdevice *Device, const ALCchar *deviceName)
{
    ALbyte *BufferData = NULL;
    DWORD CapturedDataSize;
    WinMMData *data = NULL;
    UINT DeviceID = 0;
    ALint BufferSize;
    MMRESULT res;
    ALuint i;

    if(!CaptureDeviceList)
        ProbeCaptureDevices();

    // Find the Device ID matching the deviceName if valid
    for(i = 0;i < NumCaptureDevices;i++)
    {
        if(CaptureDeviceList[i] &&
           (!deviceName || strcmp(deviceName, CaptureDeviceList[i]) == 0))
        {
            DeviceID = i;
            break;
        }
    }
    if(i == NumCaptureDevices)
        return ALC_INVALID_VALUE;

    switch(Device->FmtChans)
    {
        case DevFmtMono:
        case DevFmtStereo:
            break;

        case DevFmtQuad:
        case DevFmtX51:
        case DevFmtX51Side:
        case DevFmtX61:
        case DevFmtX71:
            return ALC_INVALID_ENUM;
    }

    switch(Device->FmtType)
    {
        case DevFmtUByte:
        case DevFmtShort:
        case DevFmtInt:
        case DevFmtFloat:
            break;

        case DevFmtByte:
        case DevFmtUShort:
        case DevFmtUInt:
            return ALC_INVALID_ENUM;
    }

    data = calloc(1, sizeof(*data));
    if(!data)
        return ALC_OUT_OF_MEMORY;
    Device->ExtraData = data;

    memset(&data->Format, 0, sizeof(WAVEFORMATEX));
    data->Format.wFormatTag = ((Device->FmtType == DevFmtFloat) ?
                               WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM);
    data->Format.nChannels = ChannelsFromDevFmt(Device->FmtChans);
    data->Format.wBitsPerSample = BytesFromDevFmt(Device->FmtType) * 8;
    data->Format.nBlockAlign = data->Format.wBitsPerSample *
                               data->Format.nChannels / 8;
    data->Format.nSamplesPerSec = Device->Frequency;
    data->Format.nAvgBytesPerSec = data->Format.nSamplesPerSec *
                                   data->Format.nBlockAlign;
    data->Format.cbSize = 0;

    if((res=waveInOpen(&data->WaveHandle.In, DeviceID, &data->Format, (DWORD_PTR)&WaveInProc, (DWORD_PTR)Device, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
    {
        ERR("waveInOpen failed: %u\n", res);
        goto failure;
    }

    data->WaveThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(data->WaveThreadEvent == NULL)
    {
        ERR("CreateEvent failed: %lu\n", GetLastError());
        goto failure;
    }

    // Allocate circular memory buffer for the captured audio
    CapturedDataSize = Device->UpdateSize*Device->NumUpdates;

    // Make sure circular buffer is at least 100ms in size
    if(CapturedDataSize < (data->Format.nSamplesPerSec / 10))
        CapturedDataSize = data->Format.nSamplesPerSec / 10;

    data->Ring = CreateRingBuffer(data->Format.nBlockAlign, CapturedDataSize);
    if(!data->Ring)
        goto failure;

    data->WaveBuffersCommitted = 0;

    // Create 4 Buffers of 50ms each
    BufferSize = data->Format.nAvgBytesPerSec / 20;
    BufferSize -= (BufferSize % data->Format.nBlockAlign);

    BufferData = calloc(4, BufferSize);
    if(!BufferData)
        goto failure;

    for(i = 0;i < 4;i++)
    {
        memset(&data->WaveBuffer[i], 0, sizeof(WAVEHDR));
        data->WaveBuffer[i].dwBufferLength = BufferSize;
        data->WaveBuffer[i].lpData = ((i==0) ? (LPSTR)BufferData :
                                      (data->WaveBuffer[i-1].lpData +
                                       data->WaveBuffer[i-1].dwBufferLength));
        data->WaveBuffer[i].dwFlags = 0;
        data->WaveBuffer[i].dwLoops = 0;
        waveInPrepareHeader(data->WaveHandle.In, &data->WaveBuffer[i], sizeof(WAVEHDR));
        waveInAddBuffer(data->WaveHandle.In, &data->WaveBuffer[i], sizeof(WAVEHDR));
        InterlockedIncrement(&data->WaveBuffersCommitted);
    }

    data->WaveThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CaptureThreadProc, (LPVOID)Device, 0, &data->WaveThreadID);
    if (data->WaveThread == NULL)
        goto failure;

    Device->DeviceName = strdup(CaptureDeviceList[DeviceID]);
    return ALC_NO_ERROR;

failure:
    if(data->WaveThread)
        CloseHandle(data->WaveThread);

    if(BufferData)
    {
        for(i = 0;i < 4;i++)
            waveInUnprepareHeader(data->WaveHandle.In, &data->WaveBuffer[i], sizeof(WAVEHDR));
        free(BufferData);
    }

    if(data->Ring)
        DestroyRingBuffer(data->Ring);

    if(data->WaveThreadEvent)
        CloseHandle(data->WaveThreadEvent);

    if(data->WaveHandle.In)
        waveInClose(data->WaveHandle.In);

    free(data);
    Device->ExtraData = NULL;
    return ALC_INVALID_VALUE;
}

static void WinMMCloseCapture(ALCdevice *Device)
{
    WinMMData *data = (WinMMData*)Device->ExtraData;
    void *buffer = NULL;
    int i;

    /* Tell the processing thread to quit and wait for it to do so. */
    data->killNow = AL_TRUE;
    PostThreadMessage(data->WaveThreadID, WM_QUIT, 0, 0);

    WaitForSingleObjectEx(data->WaveThreadEvent, 5000, FALSE);

    /* Make sure capture is stopped and all pending buffers are flushed. */
    waveInReset(data->WaveHandle.In);

    CloseHandle(data->WaveThread);
    data->WaveThread = 0;

    // Release the wave buffers
    for(i = 0;i < 4;i++)
    {
        waveInUnprepareHeader(data->WaveHandle.In, &data->WaveBuffer[i], sizeof(WAVEHDR));
        if(i == 0) buffer = data->WaveBuffer[i].lpData;
        data->WaveBuffer[i].lpData = NULL;
    }
    free(buffer);

    DestroyRingBuffer(data->Ring);
    data->Ring = NULL;

    // Close the Wave device
    CloseHandle(data->WaveThreadEvent);
    data->WaveThreadEvent = 0;

    waveInClose(data->WaveHandle.In);
    data->WaveHandle.In = 0;

    free(data);
    Device->ExtraData = NULL;
}

static void WinMMStartCapture(ALCdevice *Device)
{
    WinMMData *data = (WinMMData*)Device->ExtraData;
    waveInStart(data->WaveHandle.In);
}

static void WinMMStopCapture(ALCdevice *Device)
{
    WinMMData *data = (WinMMData*)Device->ExtraData;
    waveInStop(data->WaveHandle.In);
}

static ALCenum WinMMCaptureSamples(ALCdevice *Device, ALCvoid *Buffer, ALCuint Samples)
{
    WinMMData *data = (WinMMData*)Device->ExtraData;
    ReadRingBuffer(data->Ring, Buffer, Samples);
    return ALC_NO_ERROR;
}

static ALCuint WinMMAvailableSamples(ALCdevice *Device)
{
    WinMMData *data = (WinMMData*)Device->ExtraData;
    return RingBufferSize(data->Ring);
}


static const BackendFuncs WinMMFuncs = {
    WinMMOpenPlayback,
    WinMMClosePlayback,
    WinMMResetPlayback,
    WinMMStartPlayback,
    WinMMStopPlayback,
    WinMMOpenCapture,
    WinMMCloseCapture,
    WinMMStartCapture,
    WinMMStopCapture,
    WinMMCaptureSamples,
    WinMMAvailableSamples,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};

ALCboolean alcWinMMInit(BackendFuncs *FuncList)
{
    *FuncList = WinMMFuncs;
    return ALC_TRUE;
}

void alcWinMMDeinit()
{
    ALuint i;

    for(i = 0;i < NumPlaybackDevices;i++)
        free(PlaybackDeviceList[i]);
    free(PlaybackDeviceList);
    PlaybackDeviceList = NULL;

    NumPlaybackDevices = 0;


    for(i = 0;i < NumCaptureDevices;i++)
        free(CaptureDeviceList[i]);
    free(CaptureDeviceList);
    CaptureDeviceList = NULL;

    NumCaptureDevices = 0;
}

void alcWinMMProbe(enum DevProbe type)
{
    ALuint i;

    switch(type)
    {
        case ALL_DEVICE_PROBE:
            ProbePlaybackDevices();
            for(i = 0;i < NumPlaybackDevices;i++)
            {
                if(PlaybackDeviceList[i])
                    AppendAllDevicesList(PlaybackDeviceList[i]);
            }
            break;

        case CAPTURE_DEVICE_PROBE:
            ProbeCaptureDevices();
            for(i = 0;i < NumCaptureDevices;i++)
            {
                if(CaptureDeviceList[i])
                    AppendCaptureDeviceList(CaptureDeviceList[i]);
            }
            break;
    }
}
