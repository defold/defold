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

        Vec2i() {}
        Vec2i(int _x, int _y) : x(_x), y(_y) {}
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

        float m_Array1D_float[3];
        Vec2i m_Array1D_vec2i[2];
        Vec2i* m_Array1D_vec2i_ptr[2];

        Arrays();
        ~Arrays();
    };

    struct Forward; // testing forward declaration

    struct Misc
    {
        TestEnum    m_TestEnum;
        const char* m_String;
        void*       m_Opaque;

        Vec2i*              m_NullPtr; // Keep this 0 to make sure we can handle it
        Forward*            m_Forward1;

        Misc() {
            memset(this, 0, sizeof(*this));
        }
    };

    struct Forward
    {
        int8_t   i8;
        uint8_t  u8;
        int16_t  i16;
        uint16_t u16;
        int32_t  i32;
        uint32_t u32;
        int64_t  i64;
        uint64_t u64;

        bool     b;
    };

}

#endif // DM_JNI_TESTAPI_H
