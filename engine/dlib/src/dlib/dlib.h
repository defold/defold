#ifndef DM_DLIB
#define DM_DLIB

namespace dLib
{
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
}

#endif // DM_DLIB
