// Copyright 2020-2026 The Defold Foundation
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
#include <string.h>
#include <assert.h>

#include <dlib/array.h>
#include <dlib/mutex.h>

#include "../sound.h"

#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

#if defined(DM_PLATFORM_MACOS)

namespace dmSound
{
    extern DeviceType* g_FirstDevice;
}

namespace
{
    // Layout mirror of dmDeviceAVAudio::AVAudioDevice. The test needs to mark
    // the backend as pending reconfigure while still calling through DeviceType.
    struct TestAVAudioDevice
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
        bool                        m_PendingReconfigure;
    };

    struct MethodPatch
    {
        Class  m_Class;
        SEL    m_Selector;
        Method m_Method;
        IMP    m_OriginalImp;
    };

    static AVAudioEngine*   g_StopEngineAfterStart      = nil;
    static AVAudioMixerNode* g_FakeMainMixerNode         = nil;
    static bool             g_EngineStoppedAfterRestart = false;
    static bool             g_FakeEngineRunning         = false;
    static bool             g_FakePlayerPlaying         = false;
    static float            g_FakeOutputVolume          = 1.0f;

    static MethodPatch PatchMethod(Class cls, SEL selector, IMP replacement)
    {
        Method method = class_getInstanceMethod(cls, selector);
        assert(method);
        MethodPatch patch = { cls, selector, method, method_getImplementation(method) };
        method_setImplementation(method, replacement);
        return patch;
    }

    static void RestoreMethod(const MethodPatch& patch)
    {
        method_setImplementation(patch.m_Method, patch.m_OriginalImp);
    }

    static id TestInit(id self, SEL selector)
    {
        return self;
    }

    static void TestDealloc(id self, SEL selector)
    {
    }

    static void TestNoop(id self, SEL selector)
    {
    }

    static void TestNoopId(id self, SEL selector, id object)
    {
    }

    static void TestConnect(id self, SEL selector, id node1, id node2, id format)
    {
    }

    static BOOL TestEngineStartAndReturnError(id self, SEL selector, NSError** error)
    {
        g_FakeEngineRunning = true;
        if (g_StopEngineAfterStart == self)
        {
            g_FakeEngineRunning = false;
            g_EngineStoppedAfterRestart = true;
        }
        return YES;
    }

    static BOOL TestEngineIsRunning(id self, SEL selector)
    {
        return g_FakeEngineRunning ? YES : NO;
    }

    static void TestEngineStop(id self, SEL selector)
    {
        g_FakeEngineRunning = false;
    }

    static AVAudioMixerNode* TestMainMixerNode(id self, SEL selector)
    {
        return g_FakeMainMixerNode;
    }

    static void TestPlayerPlay(id self, SEL selector)
    {
        if (!g_FakeEngineRunning)
        {
            [NSException raise:@"com.apple.coreaudio.avfaudio" format:@"required condition is false: _engine->IsRunning()"];
        }
        g_FakePlayerPlaying = true;
    }

    static void TestPlayerStop(id self, SEL selector)
    {
        g_FakePlayerPlaying = false;
    }

    static BOOL TestPlayerIsPlaying(id self, SEL selector)
    {
        return g_FakePlayerPlaying ? YES : NO;
    }

    static void TestSetRenderingAlgorithm(id self, SEL selector, AVAudio3DMixingRenderingAlgorithm algorithm)
    {
    }

    static float TestMixerOutputVolume(id self, SEL selector)
    {
        return g_FakeOutputVolume;
    }

    static void TestSetMixerOutputVolume(id self, SEL selector, float volume)
    {
        g_FakeOutputVolume = volume;
    }

    // Scoped fake AVAudio runtime. This keeps the regression deterministic on
    // hosts where AVFoundation cannot create real audio components.
    class FakeAVAudioRuntimeScope
    {
    public:
        FakeAVAudioRuntimeScope()
        : m_PatchCount(0)
        , m_FakeMixer(nil)
        {
            AddPatch([AVAudioPlayerNode class], @selector(init), (IMP)TestInit);
            AddPatch([AVAudioPlayerNode class], @selector(dealloc), (IMP)TestDealloc);
            AddPatch([AVAudioPlayerNode class], @selector(play), (IMP)TestPlayerPlay);
            AddPatch([AVAudioPlayerNode class], @selector(stop), (IMP)TestPlayerStop);
            AddPatch([AVAudioPlayerNode class], @selector(reset), (IMP)TestNoop);
            AddPatch([AVAudioPlayerNode class], @selector(isPlaying), (IMP)TestPlayerIsPlaying);
            AddPatch([AVAudioPlayerNode class], @selector(setRenderingAlgorithm:), (IMP)TestSetRenderingAlgorithm);

            AddPatch([AVAudioMixerNode class], @selector(init), (IMP)TestInit);
            AddPatch([AVAudioMixerNode class], @selector(dealloc), (IMP)TestDealloc);
            AddPatch([AVAudioMixerNode class], @selector(outputVolume), (IMP)TestMixerOutputVolume);
            AddPatch([AVAudioMixerNode class], @selector(setOutputVolume:), (IMP)TestSetMixerOutputVolume);

            m_FakeMixer = [[AVAudioMixerNode alloc] init];
            g_FakeMainMixerNode = m_FakeMixer;

            AddPatch([AVAudioEngine class], @selector(mainMixerNode), (IMP)TestMainMixerNode);
            AddPatch([AVAudioEngine class], @selector(attachNode:), (IMP)TestNoopId);
            AddPatch([AVAudioEngine class], @selector(connect:to:format:), (IMP)TestConnect);
            AddPatch([AVAudioEngine class], @selector(detachNode:), (IMP)TestNoopId);
            AddPatch([AVAudioEngine class], @selector(startAndReturnError:), (IMP)TestEngineStartAndReturnError);
            AddPatch([AVAudioEngine class], @selector(stop), (IMP)TestEngineStop);
            AddPatch([AVAudioEngine class], @selector(isRunning), (IMP)TestEngineIsRunning);

            g_StopEngineAfterStart = nil;
            g_EngineStoppedAfterRestart = false;
            g_FakeEngineRunning = false;
            g_FakePlayerPlaying = false;
            g_FakeOutputVolume = 1.0f;
        }

        ~FakeAVAudioRuntimeScope()
        {
            [m_FakeMixer release];
            for (uint32_t i = m_PatchCount; i > 0; --i)
            {
                RestoreMethod(m_Patches[i - 1]);
            }
            g_StopEngineAfterStart = nil;
            g_FakeMainMixerNode = nil;
            g_EngineStoppedAfterRestart = false;
            g_FakeEngineRunning = false;
            g_FakePlayerPlaying = false;
            g_FakeOutputVolume = 1.0f;
        }

    private:
        void AddPatch(Class cls, SEL selector, IMP replacement)
        {
            assert(m_PatchCount < sizeof(m_Patches) / sizeof(m_Patches[0]));
            m_Patches[m_PatchCount++] = PatchMethod(cls, selector, replacement);
        }

        MethodPatch      m_Patches[32];
        uint32_t         m_PatchCount;
        AVAudioMixerNode* m_FakeMixer;
    };

    static dmSound::DeviceType* FindSoundDevice(const char* name)
    {
        dmSound::DeviceType* device_type = dmSound::g_FirstDevice;
        while (device_type)
        {
            if (strcmp(device_type->m_Name, name) == 0)
            {
                return device_type;
            }
            device_type = device_type->m_Next;
        }
        return 0;
    }
}

extern "C" int dmSoundTestAVAudioReconfigureHandlesEngineStoppedAfterRestart()
{
    FakeAVAudioRuntimeScope fake_avaudio_runtime;

    dmSound::DeviceType* device_type = FindSoundDevice("default");
    if (!device_type)
    {
        return -1;
    }

    dmSound::OpenDeviceParams params;
    params.m_BufferCount = 3;
    params.m_FrameCount = 512;

    dmSound::HDevice device = 0;
    dmSound::Result open_result = device_type->m_Open(&params, &device);
    if (open_result != dmSound::RESULT_OK || !device)
    {
        return 0;
    }

    TestAVAudioDevice* av_device = (TestAVAudioDevice*)device;
    device_type->m_DeviceStart(device);
    if (!av_device->m_Started)
    {
        device_type->m_Close(device);
        return 0;
    }

    {
        DM_MUTEX_SCOPED_LOCK(av_device->m_Mutex);
        // Drive DeviceAVAudioFreeBufferSlots through the same reconfigure path
        // as the sound thread stack reported in #12516.
        av_device->m_PendingReconfigure = true;
    }

    {
        g_StopEngineAfterStart = av_device->m_Engine;
        g_EngineStoppedAfterRestart = false;
        uint32_t free_slots = device_type->m_FreeBufferSlots(device);
        int16_t samples[512 * 2];
        memset(samples, 0, sizeof(samples));
        dmSound::Result queue_result = device_type->m_Queue(device, samples, params.m_FrameCount);
        g_StopEngineAfterStart = nil;
        bool     failed     = !g_EngineStoppedAfterRestart || av_device->m_Started || free_slots == 0 || queue_result != dmSound::RESULT_DEVICE_LOST;
        device_type->m_Close(device);
        return failed ? -1 : 1;
    }
}

#endif // defined(DM_PLATFORM_MACOS)
