#include <assert.h>
#include <stdio.h>
#include <dlib/math.h>
#include "easing.h"

namespace dmEasing
{
    #include "easing_lookup.h"

    const float EASING_SAMPLES_MINUS_ONE_RECIP = (1.0f / (EASING_SAMPLES-1));

    float GetValue(Type type, float t)
    {
        t = dmMath::Clamp(t, 0.0f, 1.0f);
        int index1 = t * (EASING_SAMPLES-1);
        int index2 = index1 + 1;
        float diff = (t - index1 * EASING_SAMPLES_MINUS_ONE_RECIP) * (EASING_SAMPLES-1);

        // NOTE: + 1 as the last sample is duplicated
        const int offset = type * (EASING_SAMPLES + 1);
        index1 += offset;
        index2 += offset;

        float val1 = EASING_LOOKUP[index1];
        float val2 = EASING_LOOKUP[index2];
        return val1 * (1.0f - diff) + val2 * diff;
    }
}

