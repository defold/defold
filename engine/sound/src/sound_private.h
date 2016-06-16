#ifndef DM_SOUND_PRIVATE_H
#define DM_SOUND_PRIVATE_H

namespace dmSound
{

    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params);

    Result PlatformFinalize();

    bool PlatformAcquireAudioFocus();

    bool PlatformReleaseAudioFocus();

    bool PlatformIsMusicPlaying();

    bool PlatformIsPhonePlaying();
}

#endif // #ifndef DM_SOUND_PRIVATE_H
