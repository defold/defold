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

#include <dmsdk/graphics/glfw/glfw.h>

#import <AVFAudio/AVFAudio.h>
#import <UIKit/UIKit.h>

// ---------------------------------------------------------------------------
// Anonymous namespace
// ---------------------------------------------------------------------------
namespace {

    bool g_audioInterrupted = false;
    bool g_isSessionRouteChangeReasonCategoryChange = false;

    id<UIApplicationDelegate> g_soundApplicationDelegate;

    void activateAudioSession() {
        g_isSessionRouteChangeReasonCategoryChange = false;
        NSError *error = nil;
        [[AVAudioSession sharedInstance] setActive:YES error:&error];
        if(error != nil){
            dmLogError("Failed to activate AudioSession (%ld)", error.code);
            return;
        }
    }
};

// ---------------------------------------------------------------------------
// SoundApplicationDelegate, iOS specific delegate
// ---------------------------------------------------------------------------
@interface SoundApplicationDelegate : NSObject <UIApplicationDelegate>

    - (void) applicationDidBecomeActive:(UIApplication *) application;
    - (void) applicationWillResignActive:(UIApplication *) application;

    - (void) handleInterruption:(NSNotification *) notification;
    - (void) handleSecondaryAudio:(NSNotification *) notification;
    - (void) handleRouteChange:(NSNotification *) notification;

@end

/* We should hadle different cases of sound interruption:
 / There are 3 different Siri cases handled in a bit different way:
 / 1. User open Siri by mistake and close it fast
 /   (we can't restore audio by applicationDidBecomeActive, it will break audio context)
 / 2. User open Siri, ask somehthing then close it
 /   (applicationWillResignActive and applicationDidBecomeActive)
 / 3. User asked Siri to open another app
 /    (applicationWillResignActive and applicationDidBecomeActive)
 / Phone call:
 / 4. User get call when plays the game, answers it and keep playing the game
 /    (AVAudioSessionInterruptionTypeBegan and AVAudioSessionSilenceSecondaryAudioHintTypeEnd)
 / 5. User declines call
 /    (AVAudioSessionInterruptionTypeBegan and AVAudioSessionInterruptionTypeEnded)
 / 6. User tap to call notification (leave game)
 / 7. User get call outside of the game, and then launch the game
 / Alarm clock:
 / 8. Close notification
 / (AVAudioSessionInterruptionTypeBegan and AVAudioSessionInterruptionTypeEnded)
 / 9. Tap to notification
*/

@implementation SoundApplicationDelegate

    - (void) applicationDidBecomeActive:(UIApplication *) application {
        if (!::g_isSessionRouteChangeReasonCategoryChange)
        {
            ::activateAudioSession();
            ::g_audioInterrupted = false;
        }
    }

    - (void) applicationWillResignActive:(UIApplication *) application {
        ::g_audioInterrupted = true;
    }

    // This should be handled for Alarm Clock and Phone Call
    - (void) handleInterruption:(NSNotification *) notification {
        NSInteger type = [[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] integerValue];
        if (type == AVAudioSessionInterruptionTypeBegan)
        {
            // When Call-in and Alarm Clock
            // Also for Siri but we paused audio earlier in applicationWillResignActive in this case
            ::g_audioInterrupted = true;
        }

        if (type == AVAudioSessionInterruptionTypeEnded)
        {
            if ([UIApplication sharedApplication].applicationState == UIApplicationStateActive)
            {
                //Alarm clock, call-in declined
                ::activateAudioSession();
                ::g_audioInterrupted = false;
            }
            else
            {
                // When ask Siri to open another app
                ::g_isSessionRouteChangeReasonCategoryChange = false;
            }
        }
    }

    - (void) handleSecondaryAudio:(NSNotification *) notification {
        NSInteger type = [[[notification userInfo] objectForKey:AVAudioSessionSilenceSecondaryAudioHintTypeKey] integerValue];
        if (type == AVAudioSessionSilenceSecondaryAudioHintTypeEnd)
        {
            if ([UIApplication sharedApplication].applicationState == UIApplicationStateActive)
            {
                ::activateAudioSession();
                ::g_audioInterrupted = false;
            }
        }
    }

    - (void) handleRouteChange:(NSNotification *) notification {
        NSInteger reason = [[[notification userInfo] objectForKey:AVAudioSessionRouteChangeReasonKey] integerValue];
        if (reason == AVAudioSessionRouteChangeReasonCategoryChange)
        {
            if ([UIApplication sharedApplication].applicationState == UIApplicationStateActive)
            {
                if (::g_isSessionRouteChangeReasonCategoryChange)
                {
                    ::activateAudioSession();
                    ::g_audioInterrupted = false;
                }
            }
            else
            {
                ::g_isSessionRouteChangeReasonCategoryChange = true;
            }
        }
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
        ::g_soundApplicationDelegate = [[SoundApplicationDelegate alloc] init];
        glfwRegisterUIApplicationDelegate(::g_soundApplicationDelegate);

        [[NSNotificationCenter defaultCenter] addObserver:g_soundApplicationDelegate
                                              selector:@selector(handleInterruption:)
                                              name:AVAudioSessionInterruptionNotification
                                              object:[AVAudioSession sharedInstance]];
        [[NSNotificationCenter defaultCenter] addObserver:g_soundApplicationDelegate
                                              selector:@selector(handleSecondaryAudio:)
                                              name:AVAudioSessionSilenceSecondaryAudioHintNotification
                                              object:[AVAudioSession sharedInstance]];
        [[NSNotificationCenter defaultCenter] addObserver:g_soundApplicationDelegate
                                              selector:@selector(handleRouteChange:)
                                              name:AVAudioSessionRouteChangeNotification
                                              object:[AVAudioSession sharedInstance]];

        NSError *error = nil;
        BOOL success = [[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryAmbient error: &error];
        if (success)
        {
            AudioSessionInitialized = false;
        }
        else
        {
            dmLogError("Failed to initialize AudioSession (%d)", (int)error.code);
        }

        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
        glfwUnregisterUIApplicationDelegate(::g_soundApplicationDelegate);
        [[NSNotificationCenter defaultCenter] removeObserver:g_soundApplicationDelegate
                                              name:AVAudioSessionInterruptionNotification
                                              object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:g_soundApplicationDelegate
                                              name:AVAudioSessionSilenceSecondaryAudioHintNotification
                                              object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:g_soundApplicationDelegate
                                              name:AVAudioSessionRouteChangeNotification
                                              object:nil];
        [::g_soundApplicationDelegate release];
        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus)
    {
        return [[AVAudioSession sharedInstance] secondaryAudioShouldBeSilencedHint];
    }

    bool PlatformIsPhoneCallActive()
    {
        return ::g_audioInterrupted;
    }

}

