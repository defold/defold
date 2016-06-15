#include <dlib/log.h>
#include "sound.h"
#include "sound_private.h"

#include <AudioToolbox/AudioSession.h>

#import <CoreTelephony/CTCallCenter.h>
#import <CoreTelephony/CTCall.h>

CTCallCenter* g_call_center = NULL;
dmSound::CallStatus g_call_status = dmSound::CALL_STATUS_UNKNOWN;

namespace dmSound
{
    bool AudioSessionInitialized = false;

    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        // NOTE: We actually ignore errors here. "Should never happen"

        g_call_center = [[CTCallCenter alloc] init];
        g_call_center.callEventHandler = ^(CTCall *call) {
            if ([call.callState isEqualToString: CTCallStateConnected])
            {
                g_call_status = dmSound::CALL_STATUS_CONNECTED;
            }
            else if ([call.callState isEqualToString: CTCallStateDialing])
            {
                g_call_status = dmSound::CALL_STATUS_CALLING;
            }
            else if ([call.callState isEqualToString: CTCallStateDisconnected])
            {
                g_call_status = dmSound::CALL_STATUS_DISCONNECTED;
            }
            else if ([call.callState isEqualToString: CTCallStateIncoming])
            {
                g_call_status = dmSound::CALL_STATUS_CALLING;
            }
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

    bool PlatformIsPhonePlaying()
    {
        return g_call_status != dmSound::CALL_STATUS_DISCONNECTED;
    }

}

