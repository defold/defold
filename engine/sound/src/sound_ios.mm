#include <dlib/log.h>
#include "sound.h"
#include "sound_private.h"

#include <AudioToolbox/AudioSession.h>

#import <CoreTelephony/CTCallCenter.h>
#import <CoreTelephony/CTCall.h>

namespace dmSound
{
    bool AudioSessionInitialized = false;
    CTCallCenter* g_callcenter = NULL;
    bool g_call_active = false;

    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        // NOTE: We actually ignore errors here. "Should never happen"

        g_callcenter = [[CTCallCenter alloc] init];
        // Check the initial state, whether a phone call is active when the
        // application is being initialized or not.
        for (CTCall* call in g_callcenter.currentCalls)
        {
            if (call.callState != CTCallStateDisconnected)
            {
                g_call_active = true;
                break;
            }
        }

        g_callcenter.callEventHandler = ^(CTCall* call) {
            g_call_active = !([call.callState isEqualToString: CTCallStateDisconnected]);
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
        return RESULT_OK;
    }

    bool PlatformAcquireAudioFocus() {
        return true;
    }

    bool PlatformReleaseAudioFocus()
    {
        return true;
    }

    bool PlatformIsMusicPlaying()
    {
        UInt32 other_playing;
        UInt32 size = sizeof(other_playing);
        AudioSessionGetProperty(kAudioSessionProperty_OtherAudioIsPlaying, &size, &other_playing);
        return (bool) other_playing;
    }

    bool PlatformIsPhoneCallActive()
    {
        return g_call_active;
    }

}

