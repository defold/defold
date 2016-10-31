#include <scripts/script_window.h>

namespace dmGameSystem
{

    void PlatformSetDimMode(DimMode mode)
    {
        (void) mode;
    }

    DimMode PlatformGetDimMode()
    {
        return DIMMING_UNKNOWN;
    }

}
