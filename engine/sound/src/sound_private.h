#ifndef DM_SOUND_PRIVATE_H
#define DM_SOUND_PRIVATE_H

namespace dmSound
{

    enum CallStatus {

        CALL_STATUS_CONNECTED      = 1,
        CALL_STATUS_CALLING        = 2,
        CALL_STATUS_DISCONNECTED   = 3,
        CALL_STATUS_UNKNOWN        = 1000
    };

    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params);

    Result PlatformFinalize();

    bool PlatformAcquireAudioFocus();

    bool PlatformReleaseAudioFocus();

    bool PlatformIsMusicPlaying();

    bool PlatformIsPhonePlaying();
}

#endif // #ifndef DM_SOUND_PRIVATE_H
