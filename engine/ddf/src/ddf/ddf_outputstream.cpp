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

#include <string.h>
#include "ddf_outputstream.h"

namespace dmDDF
{

    OutputStream::OutputStream(SaveFunction save_function, void* context)
    {
        m_SaveFunction = save_function;
        m_Context = context;
    }

    bool OutputStream::Write(const void* buffer, int length)
    {
        return m_SaveFunction(m_Context, buffer, length);
    }

    bool OutputStream::WriteTag(uint32_t number, WireType type)
    {
        uint32_t tag = number << 3;
        tag |= type;
        return WriteVarInt32(tag);
    }

    bool OutputStream::WriteVarInt32SignExtended(int32_t value)
    {
        if (value < 0)
            return WriteVarInt64((uint64_t) value);
        else
            return WriteVarInt32((uint32_t) value);
    }

    bool OutputStream::WriteVarInt32(uint32_t value)
    {
        uint8_t bytes[5];
        int size = 0;
        while (value > 0x7F)
        {
            bytes[size++] = (uint8_t(value) & 0x7F) | 0x80;
            value >>= 7;
        }
        bytes[size++] = uint8_t(value) & 0x7F;
        return Write(bytes, size);
    }

    bool OutputStream::WriteVarInt64(uint64_t value)
    {
        uint8_t bytes[10];
        int size = 0;
        while (value > 0x7F)
        {
            bytes[size++] = (uint8_t(value) & 0x7F) | 0x80;
            value >>= 7;
        }
        bytes[size++] = uint8_t(value) & 0x7F;
        return Write(bytes, size);
    }

    bool OutputStream::WriteFixed32(uint32_t value)
    {
        uint8_t buf[4];
        buf[0] = (value >> 0) & 0xff;
        buf[1] = (value >> 8) & 0xff;
        buf[2] = (value >> 16) & 0xff;
        buf[3] = (value >> 24) & 0xff;
        return Write(buf, sizeof(buf));
    }

    bool OutputStream::WriteFixed64(uint64_t value)
    {
        uint8_t buf[8];
        buf[0] = (value >> 0) & 0xff;
        buf[1] = (value >> 8) & 0xff;
        buf[2] = (value >> 16) & 0xff;
        buf[3] = (value >> 24) & 0xff;
        buf[4] = (value >> 32) & 0xff;
        buf[5] = (value >> 40) & 0xff;
        buf[6] = (value >> 48) & 0xff;
        buf[7] = (value >> 56) & 0xff;
        return Write(buf, sizeof(buf));
    }

    bool OutputStream::WriteFloat(float value)
    {
        union
        {
            float    v;
            uint32_t i;
        };
        v = value;
        return WriteFixed32(i);
    }

    bool OutputStream::WriteDouble(double value)
    {
        union
        {
            double   v;
            uint64_t i;
        };
        v = value;
        return WriteFixed64(i);
    }

    bool OutputStream::WriteInt32(int32_t value)
    {
    //    return WriteVarint32SignExtended(value);
        // TODO: Fix this to signextended!!!
        return WriteVarInt32(value);
    }

    bool OutputStream::WriteUInt32(uint32_t value)
    {
        return WriteVarInt32(value);
    }

    bool OutputStream::WriteInt64(int64_t value)
    {
        return WriteVarInt64(value);
    }

    bool OutputStream::WriteUInt64(uint64_t value)
    {
        return WriteVarInt64(value);
    }

    bool OutputStream::WriteBool(bool value)
    {
        return WriteVarInt32(value);
    }

    bool OutputStream::WriteString(const char* str)
    {
        if (!str) {
            return WriteVarInt32(0);
        }
        uint32_t len = strlen(str);
        return WriteVarInt32(len) && Write(str, len);
    }
}
