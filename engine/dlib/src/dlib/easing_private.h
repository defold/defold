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

#ifndef DM_EASING_PRIVATE_H
#define DM_EASING_PRIVATE_H

#include "easing.h"
#include <simd/simd.h>

namespace dmEasing
{
    #include "easing_lookup.h"

    // Load exactly two neighbouring samples. The unused lanes are zeroed;
    // reading a full four-float vector could pass the final curve's endpoint.
    static inline dsimd_float4_t LoadSamplePair(const float* samples)
    {
        const dsimd_byte16_t bytes = SimdLoadBytesPartial(samples, 2 * sizeof(float));
        return SimdUintBits(SimdBytesToUint(bytes));
    }

    // Built-in curves only: each lane must be less than TYPE_FLOAT_VECTOR.
    // Progress must already be in [0, 1]; the animation evaluator clamps before
    // reversing/folding playback, which preserves that range.
    // Inline the sampler so the animation evaluator can keep its vectors in registers.
    static inline dsimd_float4_t GetValue4Normalized(dsimd_uint4_t types, dsimd_float4_t t)
    {
        // 1. Find the lower sample index for each lane from normalized progress.
        const int last_sample = EASING_SAMPLES - 1;
        const dsimd_float4_t one = SimdSplat(1.0f);
        const dsimd_float4_t sample_scale = SimdSplat((float)last_sample);
        const dsimd_int4_t sample_indices = SimdTruncateToInt(SimdMul(t, sample_scale));

        // 2. Gather the two neighbouring samples from each lane's curve table.
        // The generated built-in tables duplicate their endpoint, so index + 1
        // remains valid at t=1. Custom vector curves use the scalar sampler.
        int32_t sample_index[4];
        uint32_t curve_type[4];
        SimdStoreInt(sample_index, sample_indices);
        SimdStoreUint(curve_type, types);
        const int curve_stride = EASING_SAMPLES + 1;
        const float* lookup0 = EASING_LOOKUP + curve_type[0] * curve_stride;
        const float* lookup1 = EASING_LOOKUP + curve_type[1] * curve_stride;
        const float* lookup2 = EASING_LOOKUP + curve_type[2] * curve_stride;
        const float* lookup3 = EASING_LOOKUP + curve_type[3] * curve_stride;
        // Read both neighbours together, then unpack one sample per curve/lane.
        const dsimd_float4_t pair0 = LoadSamplePair(lookup0 + sample_index[0]);
        const dsimd_float4_t pair1 = LoadSamplePair(lookup1 + sample_index[1]);
        const dsimd_float4_t pair2 = LoadSamplePair(lookup2 + sample_index[2]);
        const dsimd_float4_t pair3 = LoadSamplePair(lookup3 + sample_index[3]);
        const dsimd_float4_t pairs01 = SimdShuffle2(pair0, pair1, 0, 1, 4, 5);
        const dsimd_float4_t pairs23 = SimdShuffle2(pair2, pair3, 0, 1, 4, 5);
        const dsimd_float4_t first = SimdShuffle2(pairs01, pairs23, 0, 2, 4, 6);
        const dsimd_float4_t second = SimdShuffle2(pairs01, pairs23, 1, 3, 5, 7);

        // 3. Interpolate between samples. Keep the scalar sampler's operation order:
        // weight = (t - index / last_sample) * last_sample. Reordering changes rounding.
        const dsimd_float4_t sample_index_float = SimdConvertIntToFloat(sample_indices);
        const dsimd_float4_t remainder = SimdMulAdd(sample_index_float, SimdSplat(-1.0f / last_sample), t);
        const dsimd_float4_t weight = SimdMul(remainder, sample_scale);
        const dsimd_float4_t weighted_second = SimdMul(second, weight);
        return SimdMulAdd(first, SimdSub(one, weight), weighted_second);
    }
}

#endif // DM_EASING_PRIVATE_H
