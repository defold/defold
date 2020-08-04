// Copyright 2020 The Defold Foundation
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

#include <dlib/log.h>
#include "sound.h"
#include "sound_private.h"

#include <graphics/glfw/glfw.h>

#include <AudioToolbox/AudioSession.h>

#import <UIKit/UIKit.h>
#import <CoreTelephony/CTCallCenter.h>
#import <CoreTelephony/CTCall.h>

// ---------------------------------------------------------------------------
// Anonymous namespace
// ---------------------------------------------------------------------------
namespace {

    CTCallCenter* g_callCenter = NULL;
    bool g_phoneCallActive = false;
    id<UIApplicationDelegate> g_soundApplicationDelegate;

    bool checkPhoneCallActive() {
        for (CTCall* call in ::g_callCenter.currentCalls)  {
            if (call.callState == CTCallStateConnected
                || call.callState == CTCallStateDialing
                || call.callState == CTCallStateIncoming) {
                return true;
            }
        }

        return false;
    }

};

// ---------------------------------------------------------------------------
// SoundApplicationDelegate, iOS specific delegate
// ---------------------------------------------------------------------------
@interface SoundApplicationDelegate : NSObject <UIApplicationDelegate>

    - (void) applicationDidBecomeActive:(UIApplication *) application;

@end

@implementation SoundApplicationDelegate

    - (void) applicationDidBecomeActive:(UIApplication *) application {

        if (::g_callCenter != NULL)
        {
            [::g_callCenter release];
            ::g_callCenter = [[CTCallCenter alloc] init];
            ::g_callCenter.callEventHandler = ^(CTCall *call) {
                ::g_phoneCallActive = (call.callState == CTCallStateConnected
                                        || call.callState == CTCallStateDialing
                                        || call.callState == CTCallStateIncoming);
            };
        }

        ::g_phoneCallActive = ::checkPhoneCallActive();
    }

@end


// ---------------------------------------------------------------------------
// dmSound namespace
// ---------------------------------------------------------------------------
namespace dmSound
{
    bool AudioSessionInitialized = false;

    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        // NOTE: We actually ignore errors here. "Should never happen"
        ::g_soundApplicationDelegate = [[SoundApplicationDelegate alloc] init];
        glfwRegisterUIApplicationDelegate(::g_soundApplicationDelegate);

        ::g_callCenter = [[CTCallCenter alloc] init];
        ::g_phoneCallActive = ::checkPhoneCallActive();
        ::g_callCenter.callEventHandler = ^(CTCall *call) {
            ::g_phoneCallActive = (call.callState == CTCallStateConnected
                                    || call.callState == CTCallStateDialing
                                    || call.callState == CTCallStateIncoming);
        };

        OSStatus status = 0;
        if (!AudioSessionInitialized) {
            status = AudioSessionInitialize(0, 0, 0, 0);
            if (status != 0) {
                dmLogError("Failed to initialize AudioSession (%x)", (uint32_t) status);
            } else {
                AudioSessionInitialized = true;
            }
        }

        UInt32 sessionCategory = kAudioSessionCategory_AmbientSound;
        status = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory), &sessionCategory);
        if (status != 0) {
            dmLogError("Failed to set ambient sound category (%x)", (uint32_t) status);
        }

        UInt32 allowMixing = 1;
        status = AudioSessionSetProperty( kAudioSessionProperty_OverrideCategoryMixWithOthers, sizeof(allowMixing), &allowMixing);
        if (status != 0) {
            dmLogError("Failed to set category 'MixWithOthers' category (%x)", (uint32_t) status);
        }

        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
        glfwUnregisterUIApplicationDelegate(::g_soundApplicationDelegate);
        [::g_soundApplicationDelegate release];
        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus)
    {
        (void)is_device_started;
        (void)has_window_focus;
        UInt32 other_playing;
        UInt32 size = sizeof(other_playing);
        AudioSessionGetProperty(kAudioSessionProperty_OtherAudioIsPlaying, &size, &other_playing);
        return (bool) other_playing;
    }

    bool PlatformIsPhoneCallActive()
    {
        return ::g_phoneCallActive;
    }

}

