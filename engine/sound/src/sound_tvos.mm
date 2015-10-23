#include <dlib/log.h>
#include "sound.h"
#include "sound_private.h"

#import <AVFoundation/AVAudioSession.h>

namespace dmSound
{
    bool AudioSessionInitialized = false;

    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        // NOTE: We actually ignore errors here. "Should never happen"
        BOOL success = NO;
		NSError *error = nil;

    	AVAudioSession *session = [AVAudioSession sharedInstance];

    	success = [session setCategory:AVAudioSessionCategoryAmbient error:&error];
    	if (!success) {
            dmLogError("Failed to set ambient sound category");
    	}
    	success = [session setActive:YES error:&error];
    	if (!success) {
    		dmLogError("Failed to activate audio session.");
    	}

        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
		NSError *deactivationError = nil;
    	BOOL success = [[AVAudioSession sharedInstance] setActive:NO error:&deactivationError];
    	if (!success) {
    		dmLogError("Failed to deactivate audio session.");
    	}

        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying()
    {
        return (bool) [[AVAudioSession sharedInstance] isOtherAudioPlaying];
    }
}
