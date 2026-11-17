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
#include "sound_dsp_simd8.cpp"

namespace dmSound
{
    const DSPImpl* GetDSPImplTest8()
    {
        static const DSPImpl impl =
        {
            8,
            Dsp::MixScaledMonoToStereo_Simd8,
            Dsp::MixScaledStereoToStereo_Simd8,
            Dsp::MixAndResampleMonoToStereo_Polyphase_Simd8,
            Dsp::MixAndResampleStereoToStereo_Polyphase_Simd8,
            Dsp::ApplyClampedGain_Simd8,
            Dsp::ApplyGainAndInterleaveToS16_Simd8,
            Dsp::ApplyGain_Simd8,
            Dsp::GatherPowerData_Simd8,
            Dsp::ConvertFromS16_Simd8,
            Dsp::ConvertFromS8_Simd8,
            Dsp::Deinterleave_Simd8,
            Dsp::DeinterleaveFromS16_Simd8,
            Dsp::DeinterleaveFromS8_Simd8,
        };
        return &impl;
    }

} // namespace dmSound
