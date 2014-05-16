#ifndef DM_SOUND_PRIVATE_H
#define DM_SOUND_PRIVATE_H

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config, const InitializeParams* params);

    Result PlatformFinalize();

    bool PlatformIsMusicPlaying();
}

#endif // #ifndef DM_SOUND_PRIVATE_H
