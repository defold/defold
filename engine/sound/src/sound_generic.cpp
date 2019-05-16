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

    bool PlatformIsMusicPlaying(bool is_device_started, bool has_window_focus)
    {
        (void)is_device_started;
        (void)has_window_focus;
        return false;
    }

    bool PlatformIsPhoneCallActive()
    {
        return false;
    }
}
