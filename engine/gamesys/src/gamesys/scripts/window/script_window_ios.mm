#include <scripts/script_window.h>

#import "UIKit/UIKit.h"
#include <assert.h>

namespace dmGameSystem
{

    void PlatformSetDimMode(DimMode mode)
    {
        if (mode == DIMMING_ON)
        {
            [UIApplication sharedApplication].idleTimerDisabled = false;
        }
        else if (mode == DIMMING_OFF)
        {
            [UIApplication sharedApplication].idleTimerDisabled = true;
        }
    }

    DimMode PlatformGetDimMode()
    {
        return [UIApplication sharedApplication].idleTimerDisabled ? DIMMING_OFF : DIMMING_ON;
    }

}
