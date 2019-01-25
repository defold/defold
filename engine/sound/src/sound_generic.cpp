#include "sound.h"
#include "sound_private.h"

namespace dmSound
{
    Result PlatformInitialize(dmConfigFile::HConfig config,
            const InitializeParams* params)
    {
        return RESULT_OK;
    }

    Result PlatformFinalize()
    {
        return RESULT_OK;
    }

    bool PlatformIsMusicPlaying()
    {
        return false;
    }

    bool PlatformIsPhoneCallActive()
    {
        return false;
    }
}
