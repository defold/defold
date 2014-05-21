#ifndef DM_DLIB
#define DM_DLIB

#include <stdint.h>

namespace dLib
{
    #define DM_FEATURE_BIT_NONE                  0
    #define DM_FEATURE_BIT_SOCKET_CLIENT_TCP  (1 << 0)
    #define DM_FEATURE_BIT_SOCKET_CLIENT_UDP  (1 << 1)
    #define DM_FEATURE_BIT_SOCKET_SERVER_TCP  (1 << 2)
    #define DM_FEATURE_BIT_SOCKET_SERVER_UDP  (1 << 3)
    #define DM_FEATURE_BIT_THREADS            (1 << 4)

    /**
     * Enable/disable debug mode. Debug mode enables:
     *  - Reverse hash
     *  - Profiler
     *  - Log server
     *  Default value is true
     *  Other system might use IsDebugMode() to determine whether debug-mode is enabled or not.
     * @param debug_mode
     */
    void SetDebugMode(bool debug_mode);

    /**
     * Check if debug mode is enabled
     * @return true if debug-mode is enabled
     */
    bool IsDebugMode();

    /**
     * Check if a feature (or more) is/are available for current target platform.
     * @param feature Feature bit (see FEATURE_BIT_* defines)
     */
    bool FeaturesSupported(uint32_t features);
}

#endif // DM_DLIB
