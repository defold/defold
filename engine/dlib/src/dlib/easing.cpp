// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <assert.h>
#include <stdio.h>
#include <dlib/math.h>
#include "easing.h"

namespace dmEasing
{
    #define DM_EASING_LOOKUP_IMPLEMENTATION
    #include "easing_lookup.h"
    #undef DM_EASING_LOOKUP_IMPLEMENTATION

    static float SampleCurve(const float* lookup, int sample_count, float t)
    {
        t = dmMath::Clamp(t, 0.0f, 1.0f);
        int index1 = (int)(t * (sample_count - 1));
        int index2 = dmMath::Min(index1 + 1, sample_count - 1);
        float val1 = lookup[index1];
        float val2 = lookup[index2];
        float diff = (t - index1 * (1.0f / (sample_count - 1))) * (sample_count - 1);
        return val1 * (1.0f - diff) + val2 * diff;
    }

    float GetValue(Type type, float t)
    {
        // Avoid constructing and copying the custom-curve payload for built-in curves.
        return SampleCurve(EASING_LOOKUP + type * (EASING_SAMPLES + 1), EASING_SAMPLES, t);
    }

    float GetValue(Curve curve, float t)
    {
        if (curve.type != TYPE_FLOAT_VECTOR)
        {
            return GetValue(curve.type, t);
        }
        int sample_count = curve.vector->size;
        if (sample_count == 0)
        {
            return 0.0f;
        }
        if (sample_count == 1)
        {
            return curve.vector->values[0];
        }
        return SampleCurve(curve.vector->values, sample_count, t);
    }
}
