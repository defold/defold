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

#include <CoreServices/CoreServices.h>
#include <unistd.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>


typedef struct {
    AudioUnit audioUnit;

    ALuint frameSize;
    ALdouble sampleRateRatio;              // Ratio of hardware sample rate / requested sample rate
    AudioStreamBasicDescription format;    // This is the OpenAL format as a CoreAudio ASBD

    AudioConverterRef audioConverter;      // Sample rate converter if needed
    AudioBufferList *bufferList;           // Buffer for data coming from the input device
    ALCvoid *resampleBuffer;               // Buffer for returned RingBuffer data when resampling

    RingBuffer *ring;
} ca_data;

static const ALCchar ca_device[] = "CoreAudio Default";


static void destroy_buffer_list(AudioBufferList* list)
{
    if(list)
    {
        UInt32 i;
        for(i = 0;i < list->mNumberBuffers;i++)
            free(list->mBuffers[i].mData);
        free(list);
    }
}

static AudioBufferList* allocate_buffer_list(UInt32 channelCount, UInt32 byteSize)
{
    AudioBufferList *list;

    list = calloc(1, sizeof(AudioBufferList) + sizeof(AudioBuffer));
    if(list)
    {
        list->mNumberBuffers = 1;

        list->mBuffers[0].mNumberChannels = channelCount;
        list->mBuffers[0].mDataByteSize = byteSize;
        list->mBuffers[0].mData = malloc(byteSize);
        if(list->mBuffers[0].mData == NULL)
        {
            free(list);
            list = NULL;
        }
    }
    return list;
}

static OSStatus ca_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                            UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    ALCdevice *device = (ALCdevice*)inRefCon;
    ca_data *data = (ca_data*)device->ExtraData;

    aluMixData(device, ioData->mBuffers[0].mData,
               ioData->mBuffers[0].mDataByteSize / data->frameSize);

    return noErr;
}

static OSStatus ca_capture_conversion_callback(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets,
        AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void* inUserData)
{
    ALCdevice *device = (ALCdevice*)inUserData;
    ca_data *data = (ca_data*)device->ExtraData;

    // Read from the ring buffer and store temporarily in a large buffer
    ReadRingBuffer(data->ring, data->resampleBuffer, (ALsizei)(*ioNumberDataPackets));

    // Set the input data
    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0].mNumberChannels = data->format.mChannelsPerFrame;
    ioData->mBuffers[0].mData = data->resampleBuffer;
    ioData->mBuffers[0].mDataByteSize = (*ioNumberDataPackets) * data->format.mBytesPerFrame;

    return noErr;
}

static OSStatus ca_capture_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                                    UInt32 inNumberFrames, AudioBufferList *ioData)
{
    ALCdevice *device = (ALCdevice*)inRefCon;
    ca_data *data = (ca_data*)device->ExtraData;
    AudioUnitRenderActionFlags flags = 0;
    OSStatus err;

    // fill the bufferList with data from the input device
    err = AudioUnitRender(data->audioUnit, &flags, inTimeStamp, 1, inNumberFrames, data->bufferList);
    if(err != noErr)
    {
        ERR("AudioUnitRender error: %d\n", err);
        return err;
    }

    WriteRingBuffer(data->ring, data->bufferList->mBuffers[0].mData, inNumberFrames);

    return noErr;
}

static ALCenum ca_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    ComponentDescription desc;
    Component comp;
    ca_data *data;
    OSStatus err;

    if(!deviceName)
        deviceName = ca_device;
    else if(strcmp(deviceName, ca_device) != 0)
        return ALC_INVALID_VALUE;

    /* open the default output unit */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = FindNextComponent(NULL, &desc);
    if(comp == NULL)
    {
        ERR("FindNextComponent failed\n");
        return ALC_INVALID_VALUE;
    }

    data = calloc(1, sizeof(*data));

    err = OpenAComponent(comp, &data->audioUnit);
    if(err != noErr)
    {
        ERR("OpenAComponent failed\n");
        free(data);
        return ALC_INVALID_VALUE;
    }

    /* init and start the default audio unit... */
    err = AudioUnitInitialize(data->audioUnit);
    if(err != noErr)
    {
        ERR("AudioUnitInitialize failed\n");
        CloseComponent(data->audioUnit);
        free(data);
        return ALC_INVALID_VALUE;
    }

    device->DeviceName = strdup(deviceName);
    device->ExtraData = data;
    return ALC_NO_ERROR;
}

static void ca_close_playback(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;

    AudioUnitUninitialize(data->audioUnit);
    CloseComponent(data->audioUnit);

    free(data);
    device->ExtraData = NULL;
}

static ALCboolean ca_reset_playback(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;
    AudioStreamBasicDescription streamFormat;
    AURenderCallbackStruct input;
    OSStatus err;
    UInt32 size;

    err = AudioUnitUninitialize(data->audioUnit);
    if(err != noErr)
        ERR("-- AudioUnitUninitialize failed.\n");

    /* retrieve default output unit's properties (output side) */
    size = sizeof(AudioStreamBasicDescription);
    err = AudioUnitGetProperty(data->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &streamFormat, &size);
    if(err != noErr || size != sizeof(AudioStreamBasicDescription))
    {
        ERR("AudioUnitGetProperty failed\n");
        return ALC_FALSE;
    }

#if 0
    TRACE("Output streamFormat of default output unit -\n");
    TRACE("  streamFormat.mFramesPerPacket = %d\n", streamFormat.mFramesPerPacket);
    TRACE("  streamFormat.mChannelsPerFrame = %d\n", streamFormat.mChannelsPerFrame);
    TRACE("  streamFormat.mBitsPerChannel = %d\n", streamFormat.mBitsPerChannel);
    TRACE("  streamFormat.mBytesPerPacket = %d\n", streamFormat.mBytesPerPacket);
    TRACE("  streamFormat.mBytesPerFrame = %d\n", streamFormat.mBytesPerFrame);
    TRACE("  streamFormat.mSampleRate = %5.0f\n", streamFormat.mSampleRate);
#endif

    /* set default output unit's input side to match output side */
    err = AudioUnitSetProperty(data->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, size);
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        return ALC_FALSE;
    }

    if(device->Frequency != streamFormat.mSampleRate)
    {
        device->UpdateSize = (ALuint)((ALuint64)device->UpdateSize *
                                      streamFormat.mSampleRate /
                                      device->Frequency);
        device->Frequency = streamFormat.mSampleRate;
    }

    /* FIXME: How to tell what channels are what in the output device, and how
     * to specify what we're giving?  eg, 6.0 vs 5.1 */
    switch(streamFormat.mChannelsPerFrame)
    {
        case 1:
            device->FmtChans = DevFmtMono;
            break;
        case 2:
            device->FmtChans = DevFmtStereo;
            break;
        case 4:
            device->FmtChans = DevFmtQuad;
            break;
        case 6:
            device->FmtChans = DevFmtX51;
            break;
        case 7:
            device->FmtChans = DevFmtX61;
            break;
        case 8:
            device->FmtChans = DevFmtX71;
            break;
        default:
            ERR("Unhandled channel count (%d), using Stereo\n", streamFormat.mChannelsPerFrame);
            device->FmtChans = DevFmtStereo;
            streamFormat.mChannelsPerFrame = 2;
            break;
    }
    SetDefaultWFXChannelOrder(device);

    /* use channel count and sample rate from the default output unit's current
     * parameters, but reset everything else */
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mFormatFlags = 0;
    switch(device->FmtType)
    {
        case DevFmtUByte:
            device->FmtType = DevFmtByte;
            /* fall-through */
        case DevFmtByte:
            streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
            streamFormat.mBitsPerChannel = 8;
            break;
        case DevFmtUShort:
            device->FmtType = DevFmtShort;
            /* fall-through */
        case DevFmtShort:
            streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
            streamFormat.mBitsPerChannel = 16;
            break;
        case DevFmtUInt:
            device->FmtType = DevFmtInt;
            /* fall-through */
        case DevFmtInt:
            streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
            streamFormat.mBitsPerChannel = 32;
            break;
        case DevFmtFloat:
            streamFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
            streamFormat.mBitsPerChannel = 32;
            break;
    }
    streamFormat.mBytesPerFrame = streamFormat.mChannelsPerFrame *
                                  streamFormat.mBitsPerChannel / 8;
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags |= kAudioFormatFlagsNativeEndian |
                                 kLinearPCMFormatFlagIsPacked;

    err = AudioUnitSetProperty(data->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        return ALC_FALSE;
    }

    /* setup callback */
    data->frameSize = FrameSizeFromDevFmt(device->FmtChans, device->FmtType);
    input.inputProc = ca_callback;
    input.inputProcRefCon = device;

    err = AudioUnitSetProperty(data->audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(AURenderCallbackStruct));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        return ALC_FALSE;
    }

    /* init the default audio unit... */
    err = AudioUnitInitialize(data->audioUnit);
    if(err != noErr)
    {
        ERR("AudioUnitInitialize failed\n");
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static ALCboolean ca_start_playback(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;
    OSStatus err;

    err = AudioOutputUnitStart(data->audioUnit);
    if(err != noErr)
    {
        ERR("AudioOutputUnitStart failed\n");
        return ALC_FALSE;
    }

    return ALC_TRUE;
}

static void ca_stop_playback(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;
    OSStatus err;

    err = AudioOutputUnitStop(data->audioUnit);
    if(err != noErr)
        ERR("AudioOutputUnitStop failed\n");
}

static ALCenum ca_open_capture(ALCdevice *device, const ALCchar *deviceName)
{
    AudioStreamBasicDescription requestedFormat;  // The application requested format
    AudioStreamBasicDescription hardwareFormat;   // The hardware format
    AudioStreamBasicDescription outputFormat;     // The AudioUnit output format
    AURenderCallbackStruct input;
    ComponentDescription desc;
    AudioDeviceID inputDevice;
    UInt32 outputFrameCount;
    UInt32 propertySize;
    UInt32 enableIO;
    Component comp;
    ca_data *data;
    OSStatus err;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    // Search for component with given description
    comp = FindNextComponent(NULL, &desc);
    if(comp == NULL)
    {
        ERR("FindNextComponent failed\n");
        return ALC_INVALID_VALUE;
    }

    data = calloc(1, sizeof(*data));
    device->ExtraData = data;

    // Open the component
    err = OpenAComponent(comp, &data->audioUnit);
    if(err != noErr)
    {
        ERR("OpenAComponent failed\n");
        goto error;
    }

    // Turn off AudioUnit output
    enableIO = 0;
    err = AudioUnitSetProperty(data->audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enableIO, sizeof(ALuint));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        goto error;
    }

    // Turn on AudioUnit input
    enableIO = 1;
    err = AudioUnitSetProperty(data->audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enableIO, sizeof(ALuint));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        goto error;
    }

    // Get the default input device
    propertySize = sizeof(AudioDeviceID);
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &propertySize, &inputDevice);
    if(err != noErr)
    {
        ERR("AudioHardwareGetProperty failed\n");
        goto error;
    }

    if(inputDevice == kAudioDeviceUnknown)
    {
        ERR("No input device found\n");
        goto error;
    }

    // Track the input device
    err = AudioUnitSetProperty(data->audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &inputDevice, sizeof(AudioDeviceID));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        goto error;
    }

    // set capture callback
    input.inputProc = ca_capture_callback;
    input.inputProcRefCon = device;

    err = AudioUnitSetProperty(data->audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &input, sizeof(AURenderCallbackStruct));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        goto error;
    }

    // Initialize the device
    err = AudioUnitInitialize(data->audioUnit);
    if(err != noErr)
    {
        ERR("AudioUnitInitialize failed\n");
        goto error;
    }

    // Get the hardware format
    propertySize = sizeof(AudioStreamBasicDescription);
    err = AudioUnitGetProperty(data->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &hardwareFormat, &propertySize);
    if(err != noErr || propertySize != sizeof(AudioStreamBasicDescription))
    {
        ERR("AudioUnitGetProperty failed\n");
        goto error;
    }

    // Set up the requested format description
    switch(device->FmtType)
    {
        case DevFmtUByte:
            requestedFormat.mBitsPerChannel = 8;
            requestedFormat.mFormatFlags = kAudioFormatFlagIsPacked;
            break;
        case DevFmtShort:
            requestedFormat.mBitsPerChannel = 16;
            requestedFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            break;
        case DevFmtInt:
            requestedFormat.mBitsPerChannel = 32;
            requestedFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            break;
        case DevFmtFloat:
            requestedFormat.mBitsPerChannel = 32;
            requestedFormat.mFormatFlags = kAudioFormatFlagIsPacked;
            break;
        case DevFmtByte:
        case DevFmtUShort:
        case DevFmtUInt:
            ERR("%s samples not supported\n", DevFmtTypeString(device->FmtType));
            goto error;
    }

    switch(device->FmtChans)
    {
        case DevFmtMono:
            requestedFormat.mChannelsPerFrame = 1;
            break;
        case DevFmtStereo:
            requestedFormat.mChannelsPerFrame = 2;
            break;

        case DevFmtQuad:
        case DevFmtX51:
        case DevFmtX51Side:
        case DevFmtX61:
        case DevFmtX71:
            ERR("%s not supported\n", DevFmtChannelsString(device->FmtChans));
            goto error;
    }

    requestedFormat.mBytesPerFrame = requestedFormat.mChannelsPerFrame * requestedFormat.mBitsPerChannel / 8;
    requestedFormat.mBytesPerPacket = requestedFormat.mBytesPerFrame;
    requestedFormat.mSampleRate = device->Frequency;
    requestedFormat.mFormatID = kAudioFormatLinearPCM;
    requestedFormat.mReserved = 0;
    requestedFormat.mFramesPerPacket = 1;

    // save requested format description for later use
    data->format = requestedFormat;
    data->frameSize = FrameSizeFromDevFmt(device->FmtChans, device->FmtType);

    // Use intermediate format for sample rate conversion (outputFormat)
    // Set sample rate to the same as hardware for resampling later
    outputFormat = requestedFormat;
    outputFormat.mSampleRate = hardwareFormat.mSampleRate;

    // Determine sample rate ratio for resampling
    data->sampleRateRatio = outputFormat.mSampleRate / device->Frequency;

    // The output format should be the requested format, but using the hardware sample rate
    // This is because the AudioUnit will automatically scale other properties, except for sample rate
    err = AudioUnitSetProperty(data->audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, (void *)&outputFormat, sizeof(outputFormat));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed\n");
        goto error;
    }

    // Set the AudioUnit output format frame count
    outputFrameCount = device->UpdateSize * data->sampleRateRatio;
    err = AudioUnitSetProperty(data->audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Output, 0, &outputFrameCount, sizeof(outputFrameCount));
    if(err != noErr)
    {
        ERR("AudioUnitSetProperty failed: %d\n", err);
        goto error;
    }

    // Set up sample converter
    err = AudioConverterNew(&outputFormat, &requestedFormat, &data->audioConverter);
    if(err != noErr)
    {
        ERR("AudioConverterNew failed: %d\n", err);
        goto error;
    }

    // Create a buffer for use in the resample callback
    data->resampleBuffer = malloc(device->UpdateSize * data->frameSize * data->sampleRateRatio);

    // Allocate buffer for the AudioUnit output
    data->bufferList = allocate_buffer_list(outputFormat.mChannelsPerFrame, device->UpdateSize * data->frameSize * data->sampleRateRatio);
    if(data->bufferList == NULL)
        goto error;

    data->ring = CreateRingBuffer(data->frameSize, (device->UpdateSize * data->sampleRateRatio) * device->NumUpdates);
    if(data->ring == NULL)
        goto error;

    return ALC_NO_ERROR;

error:
    DestroyRingBuffer(data->ring);
    free(data->resampleBuffer);
    destroy_buffer_list(data->bufferList);

    if(data->audioConverter)
        AudioConverterDispose(data->audioConverter);
    if(data->audioUnit)
        CloseComponent(data->audioUnit);

    free(data);
    device->ExtraData = NULL;

    return ALC_INVALID_VALUE;
}

static void ca_close_capture(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;

    DestroyRingBuffer(data->ring);
    free(data->resampleBuffer);
    destroy_buffer_list(data->bufferList);

    AudioConverterDispose(data->audioConverter);
    CloseComponent(data->audioUnit);

    free(data);
    device->ExtraData = NULL;
}

static void ca_start_capture(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;
    OSStatus err = AudioOutputUnitStart(data->audioUnit);
    if(err != noErr)
        ERR("AudioOutputUnitStart failed\n");
}

static void ca_stop_capture(ALCdevice *device)
{
    ca_data *data = (ca_data*)device->ExtraData;
    OSStatus err = AudioOutputUnitStop(data->audioUnit);
    if(err != noErr)
        ERR("AudioOutputUnitStop failed\n");
}

static ALCenum ca_capture_samples(ALCdevice *device, ALCvoid *buffer, ALCuint samples)
{
    ca_data *data = (ca_data*)device->ExtraData;
    AudioBufferList *list;
    UInt32 frameCount;
    OSStatus err;

    // If no samples are requested, just return
    if(samples == 0)
        return ALC_NO_ERROR;

    // Allocate a temporary AudioBufferList to use as the return resamples data
    list = alloca(sizeof(AudioBufferList) + sizeof(AudioBuffer));

    // Point the resampling buffer to the capture buffer
    list->mNumberBuffers = 1;
    list->mBuffers[0].mNumberChannels = data->format.mChannelsPerFrame;
    list->mBuffers[0].mDataByteSize = samples * data->frameSize;
    list->mBuffers[0].mData = buffer;

    // Resample into another AudioBufferList
    frameCount = samples;
    err = AudioConverterFillComplexBuffer(data->audioConverter, ca_capture_conversion_callback,
                                          device, &frameCount, list, NULL);
    if(err != noErr)
    {
        ERR("AudioConverterFillComplexBuffer error: %d\n", err);
        return ALC_INVALID_VALUE;
    }
    return ALC_NO_ERROR;
}

static ALCuint ca_available_samples(ALCdevice *device)
{
    ca_data *data = device->ExtraData;
    return RingBufferSize(data->ring) / data->sampleRateRatio;
}


static const BackendFuncs ca_funcs = {
    ca_open_playback,
    ca_close_playback,
    ca_reset_playback,
    ca_start_playback,
    ca_stop_playback,
    ca_open_capture,
    ca_close_capture,
    ca_start_capture,
    ca_stop_capture,
    ca_capture_samples,
    ca_available_samples,
    ALCdevice_LockDefault,
    ALCdevice_UnlockDefault,
    ALCdevice_GetLatencyDefault
};

ALCboolean alc_ca_init(BackendFuncs *func_list)
{
    *func_list = ca_funcs;
    return ALC_TRUE;
}

void alc_ca_deinit(void)
{
}

void alc_ca_probe(enum DevProbe type)
{
    switch(type)
    {
        case ALL_DEVICE_PROBE:
            AppendAllDevicesList(ca_device);
            break;
        case CAPTURE_DEVICE_PROBE:
            AppendCaptureDeviceList(ca_device);
            break;
    }
}
