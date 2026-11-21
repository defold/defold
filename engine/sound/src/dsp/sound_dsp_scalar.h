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

#ifndef DM_SOUND_DSP_SCALAR_H
#define DM_SOUND_DSP_SCALAR_H

#include <stdint.h>
#include <dlib/math.h>

namespace dmSound
{
    // Scalar operations shared by the fallback and SIMD tails.
    namespace CPU
    {
        static inline void ApplyClampedGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            for (; num > 0; --num)
            {
                float scale_clamped = dmMath::Clamp(scale, 0.0f, 1.0f);
                float sl = *(in_l++);
                float sr = *(in_r++);
                *(out_l++) += sl * scale_clamped;
                *(out_r++) += sr * scale_clamped;
                scale += scale_delta;
            }
        }

        static inline void ApplyGainAndInterleaveToS16(int16_t* out, float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            for (; num > 0; --num)
            {
                float sl = *(in_l++);
                float sr = *(in_r++);
                out[0] = (int16_t)dmMath::Clamp(sl * scale, -32768.0f, 32767.0f);
                out[1] = (int16_t)dmMath::Clamp(sr * scale, -32768.0f, 32767.0f);
                out += 2;
                scale += scale_delta;
            }
        }

        static inline void ApplyGain(float* out[], float* in[], uint32_t num, float scale, float scale_delta)
        {
            float* in_l = in[0];
            float* in_r = in[1];
            float* out_l = out[0];
            float* out_r = out[1];
            for (; num > 0; --num)
            {
                float sl = *(in_l++);
                float sr = *(in_r++);
                *(out_l++) = sl * scale;
                *(out_r++) = sr * scale;
                scale += scale_delta;
            }
        }

        static inline void ConvertFromS16(float* out, const int16_t* in, uint32_t num)
        {
            for (; num > 0; --num)
            {
                *(out++) = (float)*(in++);
            }
        }

        static inline void ConvertFromS8(float* out, const int8_t* in, uint32_t num)
        {
            for (; num > 0; --num)
            {
                int16_t s = *(in++);
                *(out++) = (float)(s * 256 + (s & 0xff));
            }
        }

        static inline void Deinterleave(float* out[], const float* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            for (; num > 0; --num)
            {
                *(out_l++) = *(in++);
                *(out_r++) = *(in++);
            }
        }

        static inline void DeinterleaveFromS16(float* out[], const int16_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            for (; num > 0; --num)
            {
                *(out_l++) = (float)*(in++);
                *(out_r++) = (float)*(in++);
            }
        }

        static inline void DeinterleaveFromS8(float* out[], const int8_t* in, uint32_t num)
        {
            float* out_l = out[0];
            float* out_r = out[1];
            for (; num > 0; --num)
            {
                int16_t sl = *(in++);
                int16_t sr = *(in++);
                *(out_l++) = (float)(sl * 256 + (sl & 0xff));
                *(out_r++) = (float)(sr * 256 + (sr & 0xff));
            }
        }

    } // namespace CPU
} // namespace dmSound

#endif // DM_SOUND_DSP_SCALAR_H
