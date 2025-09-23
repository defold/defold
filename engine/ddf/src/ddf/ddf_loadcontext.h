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

#ifndef DDF_LOADCONTEXT_H
#define DDF_LOADCONTEXT_H

#include <stdint.h>
#include <dlib/hashtable.h>
#include "ddf.h"
#include "ddf_message.h"

namespace dmDDF
{
    class Message;

    class LoadContext
    {
    public:
        LoadContext(char* buffer, int buffer_size, bool dry_run, uint32_t options);
        Message     AllocMessage(const Descriptor* desc);
        Message     AllocMessageRaw(const Descriptor* desc, uint32_t size);
        void*       AllocRepeated(const FieldDescriptor* field_desc, int count, int data_size);
        char*       AllocString(int length);
        char*       AllocBytes(int length);
        uint32_t    GetOffset(void* memory);
        void*       GetPointer(uint32_t offset);

        void        SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run);
        int         GetMemoryUsage();

        uint32_t    IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number);
        void        GetArrayInfo(uint32_t buffer_pos, uint32_t field_number, uint32_t* count, uint32_t* data_size);
        uint32_t    IncreaseArrayDataSize(uint32_t info_hash, uint32_t data_size);

        void        AddDynamicTypeOffset(uint32_t offset);
        uint32_t    NextDynamicTypeOffset();
        void        ResetDynamicOffsetCursor();

        inline uint32_t GetOptions()
        {
            return m_Options;
        }

        inline bool GetIsDryRun()
        {
            return m_DryRun;
        }

    private:
        struct ArrayInfo
        {
            uint32_t m_Count;
            uint32_t m_DataSize;
        };

        dmHashTable32<ArrayInfo> m_ArrayInfo;

        dmArray<uint32_t> m_DynamicOffsets;

        uintptr_t   m_Start;
        uintptr_t   m_End;
        uintptr_t   m_Current;
        bool        m_DryRun;
        uint32_t    m_Options;
        uint32_t    m_OffsetCursor;
    };
}

#endif // DDF_LOADCONTEXT_H
