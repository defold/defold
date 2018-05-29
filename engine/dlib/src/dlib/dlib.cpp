#include "dlib.h"

namespace dLib
{
    bool g_DebugMode = true;
    void SetDebugMode(bool debug_mode)
    {
        g_DebugMode = debug_mode;
    }

    bool IsDebugMode()
    {
        return g_DebugMode;
    }

    bool FeaturesSupported(uint32_t features)
    {
#if defined(__EMSCRIPTEN__)
        return false;
#else
        return (features & (DM_FEATURE_BIT_SOCKET_CLIENT_TCP |
                            DM_FEATURE_BIT_SOCKET_CLIENT_UDP |
                            DM_FEATURE_BIT_SOCKET_SERVER_TCP |
                            DM_FEATURE_BIT_SOCKET_SERVER_UDP |
                            DM_FEATURE_BIT_THREADS)) == features;
#endif
    }
}
