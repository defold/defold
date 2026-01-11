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

#ifndef DDF_OUTPUTSTREAM
#define DDF_OUTPUTSTREAM

#include "ddf.h"

namespace dmDDF
{
    class OutputStream
    {
    public:
        OutputStream(SaveFunction save_function, void* context);

        bool Write(const void* buffer, int length);

        bool WriteTag(uint32_t number, WireType type);

        bool WriteVarInt32SignExtended(int32_t value);

        bool WriteVarInt32(uint32_t value);
        bool WriteVarInt64(uint64_t value);
        bool WriteFixed32(uint32_t value);
        bool WriteFixed64(uint64_t value);

        bool WriteFloat(float value);
        bool WriteDouble(double value);
        bool WriteInt32(int32_t value);
        bool WriteUInt32(uint32_t value);
        bool WriteInt64(int64_t value);
        bool WriteUInt64(uint64_t value);
        bool WriteBool(bool value);

        bool WriteString(const char* str);

        SaveFunction m_SaveFunction;
        void*           m_Context;
    };
}

#endif // DDF_OUTPUTSTREAM
