#include <scripts/script_window.h>

#import "UIKit/UIKit.h"
#include <assert.h>

namespace dmGameSystem
{

    void SetSleepMode(SleepMode mode)
    {
        if (mode == SLEEP_MODE_NORMAL)
        {
            [UIApplication sharedApplication].idleTimerDisabled = false;
        }
        else if (mode == SLEEP_MODE_AWAKE)
        {
            [UIApplication sharedApplication].idleTimerDisabled = true;
        }
        else
        {
            assert("Unknown sleep mode" || false);
        }
    }

    SleepMode GetSleepMode()
    {
        return [UIApplication sharedApplication].idleTimerDisabled
            ? SLEEP_MODE_AWAKE : SLEEP_MODE_NORMAL;
    }

}