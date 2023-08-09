// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_JNI_TESTAPI_H
#define DM_JNI_TESTAPI_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>

namespace dmJniTest
{
    const int       CONSTANT_INT = 10;
    const float     CONSTANT_FLOAT = 2.5f;
    const double    CONSTANT_DOUBLE = 1.5;

    enum TestEnumDefault
    {
        TED_VALUE_A,
        TED_VALUE_B,
        TED_VALUE_C,
    };

    enum TestEnum
    {
        TE_VALUE_A = 2,
        TE_VALUE_B,
        TE_VALUE_C = -4,
        TE_VALUE_D = -(-5),
        TE_VALUE_E = CONSTANT_INT,
    };

    struct Vec2i
    {
        int x, y;
    };

    struct Recti
    {
        Vec2i m_Min;
        Vec2i m_Max;
    };

    struct Arrays
    {
        uint8_t*            m_Data;
        uint32_t            m_DataCount;
        dmArray<uint8_t>    m_Data2;

        Recti*              m_Rects;
        uint32_t            m_RectsCount;
        dmArray<Recti>      m_Rects2;

        Recti**             m_RectPtrs1;
        uint32_t            m_RectPtrs1Count;
        dmArray<Recti*>     m_RectPtrs2;
    };

    struct Misc
    {
        TestEnum m_TestEnum;
        const char* m_String;
    };
}

#endif // DM_JNI_TESTAPI_H
