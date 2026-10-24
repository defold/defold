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

#include "render/text_arc.h"
#include <math.h>

// Exercise the arc API from C, returning the failing line for the C++ test runner.
int TestTextArcC(void)
{
    const FontTextArc arc = { 100.0f, 20.0f, 100.0f, 10.0f, 1.5707963267948966f };
    const float       frame_expected[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 100, 0, 1
    };
    const float rotated_expected[16] = {
        0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, -50, 0, 0, 1
    };
    const float transform_expected[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -70, 90, 0, 1
    };
    float matrix[16];
    FontGetTextArcFrame(0.0f, arc.m_Radius, matrix);
    for (uint32_t i = 0; i < 16; ++i)
        if (!(fabsf(matrix[i] - frame_expected[i]) < 0.00001f))
            return __LINE__;
    FontGetTextArcFrame(arc.m_Span, -50.0f, matrix);
    for (uint32_t i = 0; i < 16; ++i)
        if (!(fabsf(matrix[i] - rotated_expected[i]) < 0.00001f))
            return __LINE__;
    FontGetTextArcTransform(&arc, 70.0f, arc.m_FirstBaseline, matrix);
    for (uint32_t i = 0; i < 16; ++i)
        if (!(fabsf(matrix[i] - transform_expected[i]) < 0.00001f))
            return __LINE__;
    return 0;
}
