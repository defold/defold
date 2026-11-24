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

#ifndef DM_TEXC_RESIZE_H
#define DM_TEXC_RESIZE_H

#include <stddef.h>
#include <stdint.h>

namespace dmTexc
{
    struct ResizeTap
    {
        float    m_Weight;
        uint32_t m_Pixel;
    };

    struct ResizeSpan
    {
        uint32_t m_Offset;
        uint32_t m_Count;
    };

    struct ResizeAxis
    {
        ResizeSpan* m_Spans;
        ResizeTap*  m_Taps;
        uint32_t    m_TapCount;
        uint32_t    m_CacheRows;
    };

    // Backend callbacks process complete rows. SIMD registers never cross object-file boundaries.
    struct ResizeKernels
    {
        void (*m_ConvertRow)(const uint8_t*, float*, uint32_t);
        void (*m_FilterHorizontal)(const float*, float*, uint32_t, const ResizeAxis*);
        void (*m_AccumulateRow)(const float*, float*, size_t, float, bool);
        void (*m_StoreRow)(const float*, uint8_t*, uint32_t);
    };

    // Owns coefficients and scratch storage; callers own non-overlapping input/output RGBA8 buffers.
    // Returns false for invalid dimensions or allocation failure.
    bool ResizeRGBA8WithKernels(const uint8_t*, uint32_t, uint32_t, uint8_t*, uint32_t, uint32_t, const ResizeKernels*);

    namespace SSE3
    {
        extern const ResizeKernels g_ResizeKernels;
    }
    namespace SSE42
    {
        extern const ResizeKernels g_ResizeKernels;
    }
    namespace AVX2
    {
        extern const ResizeKernels g_ResizeKernels;
    }
    namespace AVX512
    {
        extern const ResizeKernels g_ResizeKernels;
    }
    namespace NEON
    {
        extern const ResizeKernels g_ResizeKernels;
    }
    namespace Wide
    {
        extern const ResizeKernels g_ResizeKernels;
    }
} // namespace dmTexc

#endif // DM_TEXC_RESIZE_H
