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

#include <string.h>
#include <dlib/align.h>
#include <dlib/hash.h>
#include "ddf_loadcontext.h"
#include "ddf_util.h"

#include <dlib/log.h> // REMOVE

namespace dmDDF
{
    LoadContext::LoadContext(char* buffer, int buffer_size, bool dry_run, uint32_t options)
    {
        m_Start = (uintptr_t)buffer;
        m_Current = (uintptr_t)buffer;
        m_End = (uintptr_t)buffer + buffer_size;
        m_DryRun = dry_run;
        m_Options = options;
        if (!dry_run)
        {
            memset(buffer, 0, buffer_size);
        }
        m_ArrayInfo.SetCapacity(2048, 2048);

        m_OffsetCursor = 0;
    }

    Message LoadContext::AllocMessage(const Descriptor* desc)
    {
        m_Current = (uintptr_t) DM_ALIGN(m_Current, 16);
        uintptr_t b = m_Current;
        m_Current += desc->m_Size;
        assert(m_DryRun || m_Current <= m_End);
        return Message(desc, (char*)b, desc->m_Size, m_DryRun);
    }

    Message LoadContext::AllocMessageRaw(const Descriptor* desc, uint32_t size)
    {
        m_Current = (uintptr_t) DM_ALIGN(m_Current, 16);
        uintptr_t b = m_Current;
        m_Current += size;
        assert(m_DryRun || m_Current <= m_End);
        return Message(desc, (char*)b, size, m_DryRun);
    }

    void* LoadContext::AllocRepeated(const FieldDescriptor* field_desc, int count, int data_size)
    {
        Type type = (Type) field_desc->m_Type;

        m_Current = (uintptr_t) DM_ALIGN(m_Current, 16);
        int element_size = 0;
        if ( field_desc->m_Type == TYPE_MESSAGE )
        {
            element_size = field_desc->m_MessageDescriptor->m_Size;
        }
        else if ( field_desc->m_Type == TYPE_STRING )
        {
            element_size = sizeof(const char*);
        }
        else
        {
            element_size = ScalarTypeSize(type);
        }

        uintptr_t b = m_Current;
        m_Current += count * element_size;
        assert(m_DryRun || m_Current <= m_End);
        return (void*) b;
    }

    char* LoadContext::AllocString(int length)
    {
        uintptr_t b = m_Current;
        m_Current += length;
        assert(m_DryRun || m_Current <= m_End);

        return (char*)b;
    }

    char* LoadContext::AllocBytes(int length)
    {
        m_Current = (uintptr_t) DM_ALIGN(m_Current, 16);
        uintptr_t b = m_Current;
        m_Current += length;
        assert(m_DryRun || m_Current <= m_End);
        return (char*)b;
    }

    uint32_t LoadContext::GetOffset(void* memory)
    {
        return ((uintptr_t) memory) - m_Start;
    }

    void* LoadContext::GetPointer(uint32_t offset)
    {
        return (void*)(m_Start+offset);
    }

    void LoadContext::SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run)
    {
        m_Start = (uintptr_t)buffer;
        m_Current = (uintptr_t)buffer;
        m_End = (uintptr_t)buffer + buffer_size;
        m_DryRun = dry_run;
        if (!dry_run)
        {
            memset(buffer, 0, buffer_size);
        }
    }

    int LoadContext::GetMemoryUsage()
    {
        return (int) (m_Current - m_Start);
    }

    uint32_t LoadContext::IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number)
    {
        uint32_t key[] = {field_number, buffer_pos};
        uint32_t hash = dmHashBufferNoReverse32((void*)key, sizeof(key));
        if(m_ArrayInfo.Full())
        {
            m_ArrayInfo.SetCapacity(2048, m_ArrayInfo.Capacity() + 1024);
        }

        ArrayInfo* info_ptr = m_ArrayInfo.Get(hash);
        if(info_ptr)
        {
            info_ptr->m_Count++;
        }
        else
        {
            ArrayInfo new_info;
            new_info.m_Count    = 1;
            new_info.m_DataSize = 0;
            m_ArrayInfo.Put(hash, new_info);
        }

        return hash;
    }

    void LoadContext::GetArrayInfo(uint32_t buffer_pos, uint32_t field_number, uint32_t* count, uint32_t* data_size)
    {
        uint32_t key[] = {field_number, buffer_pos};
        uint32_t hash = dmHashBufferNoReverse32((void*)key, sizeof(key));
        ArrayInfo *info_ptr = m_ArrayInfo.Get(hash);

        if (info_ptr == 0)
        {
            *count = 0;
            *data_size = 0;
        }
        else
        {
            *count = info_ptr->m_Count;
            *data_size = info_ptr->m_DataSize;
        }
    }

    uint32_t LoadContext::IncreaseArrayDataSize(uint32_t info_hash, uint32_t data_size)
    {
        ArrayInfo *info_ptr = m_ArrayInfo.Get(info_hash);
        assert(info_ptr);
        info_ptr->m_DataSize += data_size;

        dmLogInfo("IncreaseArrayDataSize: %d has size: %d", info_hash, info_ptr->m_DataSize);

        return info_ptr->m_DataSize;
    }

    void LoadContext::AddDynamicTypeOffset(uint32_t offset)
    {
        if (m_DynamicOffsets.Full())
        {
            m_DynamicOffsets.OffsetCapacity(32);
        }
        m_DynamicOffsets.Push(offset);
    }

    uint32_t LoadContext::NextDynamicTypeOffset()
    {
        return m_DynamicOffsets[m_OffsetCursor++];
    }

    void LoadContext::ResetDynamicOffsetCursor()
    {
        m_OffsetCursor = 0;
    }
}
