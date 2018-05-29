#include "easing.h"

namespace dmEasing
{
    #include "easing_lookup.h"

    float GetValue(Type type, float t)
    {
        return GetValue(Curve(type), t);
    }

    float GetValue(Curve curve, float t)
    {
        t = dmMath::Clamp(t, 0.0f, 1.0f);
        int sample_count;
        int index1, index2;
        float val1, val2;
        float* lookup;

        if (curve.type == dmEasing::TYPE_FLOAT_VECTOR)
        {
            sample_count = curve.vector->size;
            lookup       = curve.vector->values;

            if (sample_count == 0)
            {
                return 0.0f;
            } else if (sample_count == 1) {
                return lookup[0];
            }

        } else {
            sample_count = EASING_SAMPLES; // 64 samples for built in curves
            lookup       = EASING_LOOKUP;
            lookup      += curve.type * (EASING_SAMPLES + 1); // NOTE: + 1 as the last sample is duplicated
        }

        index1 = (int) (t * (sample_count-1));
        index2 = dmMath::Min(index1 + 1, sample_count-1);

        val1 = lookup[index1];
        val2 = lookup[index2];

        float diff = (t - index1 * (1.0f / (sample_count-1))) * (sample_count-1);
        return val1 * (1.0f - diff) + val2 * diff;
    }
}

