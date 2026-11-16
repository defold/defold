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
#include <simd/simd_runtime.h>

namespace dmSound
{
#include "sound_pfb.h"

#if defined(DM_SOUND_DSP_X86)
    const DSPImpl* GetDSPImplSSE3();
    const DSPImpl* GetDSPImplSSE42();
    const DSPImpl* GetDSPImplAVX2();
    const DSPImpl* GetDSPImplAVX512();
#elif defined(DM_SOUND_DSP_WASM)
    const DSPImpl* GetDSPImplWASM();
#elif defined(DM_SOUND_DSP_NEON)
    const DSPImpl* GetDSPImplNEON();
#if defined(DM_SOUND_DSP_NEON_WIDE)
    const DSPImpl* GetDSPImplNEON8();
    const DSPImpl* GetDSPImplNEON16();
#endif
#if defined(DM_SOUND_DSP_SME)
    const DSPImpl* GetDSPImplSME();
#endif
#endif

    const DSPImpl* GetDSPImpl(DSPImplType type)
    {
        if (type == DSPIMPL_TYPE_CPU)
        {
            return &fallback_impl;
        }

#if defined(DM_SOUND_DSP_X86)
        dsimd_version_t version = SimdGetVersion();
        if (version == DSIMD_VERSION_AVX512 && (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_AVX512))
        {
            return GetDSPImplAVX512();
        }

        if ((version == DSIMD_VERSION_AVX2 || version == DSIMD_VERSION_AVX512) && (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_AVX2))
        {
            return GetDSPImplAVX2();
        }

        bool x86 = (version >= DSIMD_VERSION_SSE3 && version <= DSIMD_VERSION_SSE42) || version == DSIMD_VERSION_AVX2 || version == DSIMD_VERSION_AVX512;
        if (x86 && version >= DSIMD_VERSION_SSE42 && (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_SSE42))
        {
            return GetDSPImplSSE42();
        }

        if (x86 && (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_SSE2))
        {
            return GetDSPImplSSE3();
        }

#elif defined(DM_SOUND_DSP_WASM)
        // The host selects a SIMD-capable module before instantiation.
        if (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_WASM_SIMD128)
        {
            return GetDSPImplWASM();
        }

#elif defined(DM_SOUND_DSP_NEON)
#if defined(DM_SOUND_DSP_SME)
        if (SimdGetSMEFloatLanes() >= 16 && type == DSPIMPL_TYPE_SME)
        {
            const DSPImpl* sme = GetDSPImplSME();
            if (sme)
            {
                return sme;
            }
        }

#endif
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (SimdGetVersion() == DSIMD_VERSION_NEON && type == DSPIMPL_TYPE_DEFAULT)
        {
            return GetDSPImplNEON16();
        }

#endif
        if (SimdGetVersion() == DSIMD_VERSION_NEON && (type == DSPIMPL_TYPE_DEFAULT || type == DSPIMPL_TYPE_NEON))
        {
            return GetDSPImplNEON();
        }

#endif
        return type == DSPIMPL_TYPE_DEFAULT ? &fallback_impl : 0;
    }

    bool SelectDSPImpl(DSPContext* context, DSPImplType type)
    {
        const DSPImpl* impl = GetDSPImpl(type);
        context->m_Type = impl ? type : DSPIMPL_TYPE_CPU;
        context->m_Lanes = impl ? impl->m_Lanes : 1;
        context->m_ResampleLanes = context->m_Lanes;
#if defined(DM_SOUND_DSP_NEON_WIDE)
        if (type == DSPIMPL_TYPE_DEFAULT && context->m_Lanes == 16)
        {
            // FIR benefits from the paired four-lane schedule on Apple Silicon.
            context->m_ResampleLanes = 4;
        }

#endif
#if defined(DM_SOUND_DSP_X86)
        if (context->m_Lanes == 4 && type == DSPIMPL_TYPE_DEFAULT)
        {
            context->m_Type = SimdGetVersion() >= DSIMD_VERSION_SSE42 ? DSPIMPL_TYPE_SSE42 : DSPIMPL_TYPE_SSE2;
        }

#endif
        dmLogOnceInfo("  DSP backend: %s (%u lanes)", impl ? impl->m_Name : "CPU", context->m_Lanes);
        if (context->m_ResampleLanes != context->m_Lanes)
        {
            dmLogOnceInfo("  DSP resampling: NEON (%u lanes)", context->m_ResampleLanes);
        }

        return impl != 0;
    }

} // namespace dmSound
