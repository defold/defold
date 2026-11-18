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

#define DM_TEST_SOUND_DSP
#include "sound_dsp_simd128.cpp"

namespace dmSound
{
    template <unsigned Unroll>
    static const DSPImpl* GetTestImpl()
    {
        static const DSPImpl impl =
        {
            4,
            Dsp::MixScaledMonoToStereo_Simd128<Unroll>,
            Dsp::MixScaledStereoToStereo_Simd128<Unroll>,
            Dsp::MixAndResampleMonoToStereo_Polyphase_Simd128<Unroll>,
            Dsp::MixAndResampleStereoToStereo_Polyphase_Simd128<Unroll>,
            Dsp::ApplyClampedGain_Simd128<Unroll>,
            Dsp::ApplyGainAndInterleaveToS16_Simd128<Unroll>,
            Dsp::ApplyGain_Simd128<Unroll>,
            Dsp::GatherPowerData_Simd128<Unroll>,
            Dsp::ConvertFromS16_Simd128<Unroll>,
            Dsp::ConvertFromS8_Simd128<Unroll>,
            Dsp::Deinterleave_Simd128<Unroll>,
            Dsp::DeinterleaveFromS16_Simd128<Unroll>,
            Dsp::DeinterleaveFromS8_Simd128<Unroll>,
        };
        return &impl;
    }

    const DSPImpl* GetDSPImplTest128(unsigned unroll)
    {
        switch (unroll)
        {
            case 1:
            {
                return GetTestImpl<1>();
            }
            case 2:
            {
                return GetTestImpl<2>();
            }
            case 4:
            {
                return GetTestImpl<4>();
            }
            case 8:
            {
                return GetTestImpl<8>();
            }
        }

        return 0;
    }

} // namespace dmSound
