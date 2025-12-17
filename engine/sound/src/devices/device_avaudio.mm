// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include "sound.h"
#include "sound_private.h"

#import <TargetConditionals.h>
#import <AVFoundation/AVFoundation.h>

namespace dmDeviceAVAudio
{
    struct AVAudioDevice
    {
        AVAudioEngine*              m_Engine;
        AVAudioPlayerNode*          m_Player;
        AVAudioFormat*              m_Format;
        dmArray<AVAudioPCMBuffer*>  m_AllBuffers;
        dmArray<AVAudioPCMBuffer*>  m_FreeBuffers;
        uint32_t                    m_MixRate;
        uint32_t                    m_FramesPerBuffer;
        bool                        m_Started;
        dmMutex::HMutex             m_Mutex;
        id                          m_EngineConfigObserver;
#if TARGET_OS_IPHONE
        id                          m_SessionRouteObserver;
#endif
        bool                        m_PendingReconfigure;

        AVAudioDevice()
        : m_Engine(nil)
        , m_Player(nil)
        , m_Format(nil)
        , m_MixRate(0)
        , m_FramesPerBuffer(0)
        , m_Started(false)
        , m_Mutex(0)
        , m_EngineConfigObserver(nil)
        , m_PendingReconfigure(false)
        {
#if TARGET_OS_IPHONE
            m_SessionRouteObserver = nil;
#endif
        }
    };

    static uint32_t GetSystemSampleRate() {
    // It's not used for now because we don't have a way to notify the sound system
    // that the sample rate has changed in the middle of a session (e.g., when headphones are turned off).
    // I’m keeping it here and forcing 48000 instead of dynamic detection for now.
    // AVAudioEngine will resample to hardware.
    double sr = 48000.0;

    // #if TARGET_OS_IPHONE
    //     // On iOS, ensure audio session sample rate is used
    //     sr = [[AVAudioSession sharedInstance] sampleRate];
    // #else
    //     // On macOS, derive sample rate from output node format
    //     AVAudioFormat* sysfmt = [[[AVAudioEngine alloc] init].outputNode outputFormatForBus:0];
    //     sr = sysfmt ? sysfmt.sampleRate : 0.0;
    // #endif

    //     if (sr <= 0.0) {
    //         sr = 44100.0;
    //     }
        return (uint32_t)sr;
    }

    static void DeviceAVAudioReconfigureIfNeeded(AVAudioDevice* device)
    {
        // Check and clear the pending reconfigure flag under the device mutex
        bool do_reconfigure = false;
        {
            DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
            if (device->m_PendingReconfigure)
            {
                device->m_PendingReconfigure = false;
                do_reconfigure = true;
            }
        }
        if (!do_reconfigure)
            return;

        float prevVol = device->m_Engine.mainMixerNode.outputVolume;
        device->m_Engine.mainMixerNode.outputVolume = 0.0f; // avoid pops

        [device->m_Player stop];
        [device->m_Player reset];
        [device->m_Engine stop];

        //GetSystemSampleRate();

        {
            DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
            device->m_FreeBuffers.SetSize(0);
            for (uint32_t i = 0; i < device->m_AllBuffers.Size(); ++i)
            {
                if (device->m_FreeBuffers.Size() < device->m_FreeBuffers.Capacity())
                    device->m_FreeBuffers.Push(device->m_AllBuffers[i]);
            }
        }

        NSError* err = nil;
        if (![device->m_Engine startAndReturnError:&err])
        {
            dmLogError("Failed to restart AVAudioEngine after reconfigure (%ld)", (long)err.code);
            device->m_Started = false;
            device->m_Engine.mainMixerNode.outputVolume = prevVol;
            return;
        }
        [device->m_Player play];
        device->m_Engine.mainMixerNode.outputVolume = prevVol;
        device->m_Started = true;
    }

    static void ReturnBufferToPool(AVAudioDevice* device, AVAudioPCMBuffer* buffer)
    {
        DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
        // Avoid overfilling in case of late completion callbacks during resets/route changes
        if (device->m_FreeBuffers.Size() < device->m_FreeBuffers.Capacity())
        {
            device->m_FreeBuffers.Push(buffer);
        }
    }

    static dmSound::Result DeviceAVAudioOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* out_device)
    {
        assert(params);
        assert(out_device);

        AVAudioDevice* device = new AVAudioDevice();

        device->m_Engine = [[AVAudioEngine alloc] init];
        device->m_Player = [[AVAudioPlayerNode alloc] init];

        device->m_Mutex = dmMutex::New();
        device->m_MixRate = GetSystemSampleRate();

        // Respect engine-provided frame count if set; otherwise use engine default
        device->m_FramesPerBuffer = params->m_FrameCount ? params->m_FrameCount : dmSound::GetDefaultFrameCount(device->m_MixRate);

        device->m_Format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                         sampleRate:device->m_MixRate
                                                           channels:2
                                                        interleaved:NO];

        if (!device->m_Format)
        {
            dmLogError("Failed to create AVAudioFormat");
            delete device;
            return dmSound::RESULT_INIT_ERROR;
        }

        [device->m_Engine attachNode:device->m_Player];
        [device->m_Engine connect:device->m_Player to:[device->m_Engine mainMixerNode] format:device->m_Format];

        device->m_Player.renderingAlgorithm = AVAudio3DMixingRenderingAlgorithmEqualPowerPanning;

        // Observe engine configuration changes
        __block AVAudioDevice* bdev_cfg = device;
        device->m_EngineConfigObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:AVAudioEngineConfigurationChangeNotification
            object:device->m_Engine
            queue:nil
            usingBlock:^(NSNotification* n){
                DM_MUTEX_SCOPED_LOCK(bdev_cfg->m_Mutex);
                bdev_cfg->m_PendingReconfigure = true;
            }];

#if TARGET_OS_IPHONE
        // Observe route changes on iOS in case engine isn't emitting config change
        __block AVAudioDevice* bdev_route = device;
        device->m_SessionRouteObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:AVAudioSessionRouteChangeNotification
            object:[AVAudioSession sharedInstance]
            queue:nil
            usingBlock:^(NSNotification* n){
                DM_MUTEX_SCOPED_LOCK(bdev_route->m_Mutex);
                bdev_route->m_PendingReconfigure = true;
            }];
#endif

        device->m_AllBuffers.SetCapacity(params->m_BufferCount);
        device->m_AllBuffers.SetSize(params->m_BufferCount);
        device->m_FreeBuffers.SetCapacity(params->m_BufferCount);
        device->m_FreeBuffers.SetSize(0);

        for (uint32_t i = 0; i < params->m_BufferCount; ++i)
        {
            AVAudioPCMBuffer* buf = [[AVAudioPCMBuffer alloc] initWithPCMFormat:device->m_Format
                                                                    frameCapacity:device->m_FramesPerBuffer];
            device->m_AllBuffers[i] = buf;
            device->m_FreeBuffers.Push(buf);
        }

        *out_device = device;
        return dmSound::RESULT_OK;
    }

    static void DeviceAVAudioClose(dmSound::HDevice _device)
    {
        AVAudioDevice* device = (AVAudioDevice*)_device;

        if (device->m_Started)
        {
            [device->m_Player stop];
            [device->m_Engine stop];
            device->m_Started = false;
        }

        if (device->m_Player)
        {
            [device->m_Engine detachNode:device->m_Player];
        }

        for (uint32_t i = 0; i < device->m_AllBuffers.Size(); ++i)
        {
            [device->m_AllBuffers[i] release];
        }
        device->m_AllBuffers.SetSize(0);
        device->m_FreeBuffers.SetSize(0);

        [device->m_Player release];
        [device->m_Engine release];
        [device->m_Format release];

        if (device->m_EngineConfigObserver)
        {
            [[NSNotificationCenter defaultCenter] removeObserver:device->m_EngineConfigObserver];
            device->m_EngineConfigObserver = nil;
        }
#if TARGET_OS_IPHONE
        if (device->m_SessionRouteObserver)
        {
            [[NSNotificationCenter defaultCenter] removeObserver:device->m_SessionRouteObserver];
            device->m_SessionRouteObserver = nil;
        }
#endif

        dmMutex::Delete(device->m_Mutex);
        delete device;
    }

    static dmSound::Result DeviceAVAudioQueue(dmSound::HDevice _device, const void* samples, uint32_t frame_count)
    {
        assert(_device);
        AVAudioDevice* device = (AVAudioDevice*)_device;

        // If a route/config change happened, reconfigure immediately and drop any leftover audio
        DeviceAVAudioReconfigureIfNeeded(device);

        if (frame_count > device->m_FramesPerBuffer)
        {
            frame_count = device->m_FramesPerBuffer;
        }

        AVAudioPCMBuffer* buffer = 0;
        {
            DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
            if (device->m_FreeBuffers.Empty())
                return dmSound::RESULT_OUT_OF_BUFFERS;
            buffer = device->m_FreeBuffers[device->m_FreeBuffers.Size() - 1];
            device->m_FreeBuffers.SetSize(device->m_FreeBuffers.Size() - 1);
        }

        buffer.frameLength = frame_count;

        const int16_t* in = (const int16_t*)samples;

        // Convert from interleaved S16 stereo to deinterleaved float32
        float* left  = buffer.floatChannelData[0];
        float* right = buffer.floatChannelData[1];
        for (uint32_t i = 0; i < frame_count; ++i)
        {
            float l = (float)in[i*2+0] / 32768.0f;
            float r = (float)in[i*2+1] / 32768.0f;
            l = dmMath::Clamp(l, -1.0f, 1.0f);
            r = dmMath::Clamp(r, -1.0f, 1.0f);
            left[i]  = l;
            right[i] = r;
        }

        // Schedule and recycle buffer on completion
        __block AVAudioDevice* bdev = device;
        AVAudioNodeCompletionHandler handler = ^{
            ReturnBufferToPool(bdev, buffer);
        };

        [device->m_Player scheduleBuffer:buffer completionHandler:handler];

        return dmSound::RESULT_OK;
    }

    static uint32_t DeviceAVAudioFreeBufferSlots(dmSound::HDevice _device)
    {
        assert(_device);
        AVAudioDevice* device = (AVAudioDevice*)_device;
        // Apply pending reconfigure here as well, so the engine can resume pushing buffers
        DeviceAVAudioReconfigureIfNeeded(device);
        DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
        return device->m_FreeBuffers.Size();
    }


    static void DeviceAVAudioDeviceInfo(dmSound::HDevice _device, dmSound::DeviceInfo* info)
    {
        assert(_device);
        assert(info);
        AVAudioDevice* device = (AVAudioDevice*)_device;
        info->m_MixRate = device->m_MixRate;
        info->m_FrameCount = device->m_FramesPerBuffer;
        info->m_DSPImplementation = dmSound::DSPIMPL_TYPE_CPU;
        info->m_UseNonInterleaved = 0;
        info->m_UseFloats = 0;
        info->m_UseNormalized = 0;
    }

    static void DeviceAVAudioStart(dmSound::HDevice _device)
    {
        assert(_device);
        AVAudioDevice* device = (AVAudioDevice*)_device;
        // Handle any pending route/config change before starting playback
        DeviceAVAudioReconfigureIfNeeded(device);
        if (!device->m_Started)
        {
            NSError* error = nil;
            if (![device->m_Engine startAndReturnError:&error])
            {
                dmLogError("Failed to start AVAudioEngine (%ld)", (long)error.code);
                return;
            }
            [device->m_Player play];
            device->m_Started = true;
        }
    }

    static void DeviceAVAudioStop(dmSound::HDevice _device)
    {
        assert(_device);
        AVAudioDevice* device = (AVAudioDevice*)_device;
        if (device->m_Started)
        {
            // Allow queued audio to finish similar to OpenAL stop behavior
            int iter = 0;
            while ([device->m_Player isPlaying] && iter < 20)
            {
                ++iter;
                dmTime::Sleep(10 * 1000); // 10 ms
            }

            [device->m_Player stop];
            [device->m_Player reset];
            [device->m_Engine stop];
            device->m_Started = false;

            // After reset, callbacks won't fire; restore pool to full
            DM_MUTEX_SCOPED_LOCK(device->m_Mutex);
            device->m_FreeBuffers.SetSize(0);
            for (uint32_t i = 0; i < device->m_AllBuffers.Size(); ++i)
            {
                device->m_FreeBuffers.Push(device->m_AllBuffers[i]);
            }
        }
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default",
                            DeviceAVAudioOpen,
                            DeviceAVAudioClose,
                            DeviceAVAudioQueue,
                            DeviceAVAudioFreeBufferSlots,
                            0,
                            DeviceAVAudioDeviceInfo,
                            DeviceAVAudioStart,
                            DeviceAVAudioStop);
}
