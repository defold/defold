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

#include "sound_dsp.h"

namespace dmSound
{
#include "sound_pfb.h"

#if DM_SOUND_DSP_MAX_LANES >= 4
    extern const DSPImpl simd128_impl;
#endif
#if DM_SOUND_DSP_MAX_LANES >= 8
    extern const DSPImpl simd256_impl;
#endif
#if DM_SOUND_DSP_MAX_LANES >= 16
    extern const DSPImpl simd512_impl;
#endif

    // Only reference widths compiled into this build; unsupported widths use scalar.
    static const DSPImpl* GetDSPImpl(dsimd_version_t version)
    {
        switch (SimdGetLaneCount(version))
        {
#if DM_SOUND_DSP_MAX_LANES >= 16
            case 16:
            {
                return &simd512_impl;
            }
#endif
#if DM_SOUND_DSP_MAX_LANES >= 8
            case 8:
            {
                return &simd256_impl;
            }
#endif
#if DM_SOUND_DSP_MAX_LANES >= 4
            case 4:
            {
                return &simd128_impl;
            }
#endif
            default:
            {
                break;
            }
        }

        return &fallback_impl;
    }

    static bool IsX86Version(dsimd_version_t version)
    {
        return (version >= DSIMD_VERSION_SSE2 && version <= DSIMD_VERSION_SSE42) ||
               version == DSIMD_VERSION_AVX2 || version == DSIMD_VERSION_AVX512;
    }

    void SelectDSPImpl(DSPContext* context, DSPImplType type)
    {
        dsimd_version_t available = SimdGetVersion();
        dsimd_version_t version = type == DSPIMPL_TYPE_DEFAULT ? available : (dsimd_version_t)type;
        // Ordering is meaningful only within the x86 capability chain.
        if (version != available && version != DSIMD_VERSION_SCALAR &&
            !(IsX86Version(version) && IsX86Version(available) && version <= available))
        {
            version = DSIMD_VERSION_SCALAR;
        }

        // All supported x86 four-lane requests use the SSE3 kernel.
        if (version >= DSIMD_VERSION_SSE3 && version <= DSIMD_VERSION_SSE42)
        {
            version = DSIMD_VERSION_SSE3;
        }
        else if (version == DSIMD_VERSION_SSE2)
        {
            version = DSIMD_VERSION_SCALAR;
        }

        const DSPImpl* impl = GetDSPImpl(version);
        context->m_Impl = *impl;
        context->m_Type = type;
        context->m_Version = impl->m_Lanes == 1 ? DSIMD_VERSION_SCALAR : version;
    }

} // namespace dmSound
