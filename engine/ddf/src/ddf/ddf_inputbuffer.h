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

#ifndef DDFINPUTSTREAM_H
#define DDFINPUTSTREAM_H

#include <stdint.h>

namespace dmDDF
{
    const int WIRE_MAXVARINTBYTES = 10;

    class InputBuffer
    {
    public:
        InputBuffer();
        InputBuffer(const char* buffer, uint32_t buffer_size);

        uint32_t            Tell();
        void                Seek(uint32_t pos);
        bool                Skip(uint32_t amount);
        bool                SubBuffer(uint32_t length, InputBuffer* sub_buffer);
        bool                Eof();

        bool                Read(int length, const char** buffer_out);

        bool                ReadVarInt32(uint32_t* value);
        bool                ReadVarInt64(uint64_t* value);
        bool                ReadFixed32(uint32_t* value);
        bool                ReadFixed64(uint64_t* value);

        bool                ReadFloat(float* value);
        bool                ReadDouble(double* value);
        bool                ReadInt32(int32_t* value);
        bool                ReadUInt32(uint32_t* value);
        bool                ReadInt64(int64_t* value);
        bool                ReadUInt64(uint64_t* value);
        bool                ReadBool(bool* value);

    private:
        const char* m_Start;
        const char* m_End;
        const char* m_Current;
    };
}

#endif // DDFINPUTSTREAM_H
