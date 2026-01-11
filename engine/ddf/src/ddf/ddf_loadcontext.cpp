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
#include <dlib/align.h>
#include <dlib/hash.h>
#include "ddf_loadcontext.h"
#include "ddf_util.h"

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
        m_ArrayCount.SetCapacity(2048, 2048);

        m_DynamicOffsetCursor = 0;
        m_DynamicTypeMemoryTotal = 0;
    }

    Message LoadContext::AllocMessage(const Descriptor* desc)
    {
        m_Current = (uintptr_t) DM_ALIGN(m_Current, 16);
        uintptr_t b = m_Current;
        m_Current += desc->m_Size;
        assert(m_DryRun || m_Current <= m_End);
        return Message(desc, (char*)b, desc->m_Size, m_DryRun);
    }

    void* LoadContext::AllocRepeated(const FieldDescriptor* field_desc, int count)
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

    uint32_t LoadContext::GetDynamicTypeMemorySize()
    {
        return m_DynamicTypeMemoryTotal;
    }

    int LoadContext::GetMemoryUsage()
    {
        return (int) (m_Current - m_Start);
    }

    uint32_t LoadContext::IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number)
    {
        uint32_t key[] = {field_number, buffer_pos};
        uint32_t hash = dmHashBufferNoReverse32((void*)key, sizeof(key));
        if(m_ArrayCount.Full())
        {
            m_ArrayCount.SetCapacity(2048, m_ArrayCount.Capacity() + 1024);
        }

        uint32_t* value_p = m_ArrayCount.Get(hash);
        if(value_p)
        {
            (*value_p)++;
        }
        else
        {
            m_ArrayCount.Put(hash, 1);
        }

        return hash;
    }

    uint32_t LoadContext::GetArrayCount(uint32_t buffer_pos, uint32_t field_number)
    {
        uint32_t key[] = {field_number, buffer_pos};
        uint32_t hash = dmHashBufferNoReverse32((void*)key, sizeof(key));
        uint32_t *info_ptr = m_ArrayCount.Get(hash);

        if (info_ptr == 0)
        {
            return 0;
        }
        return *info_ptr;
    }

    void* LoadContext::GetDynamicTypePointer(uint32_t offset)
    {
        return (void*)(m_Start + m_DynamicTypeOffset + offset);
    }

    void LoadContext::SetDynamicTypeBase(uint32_t offset)
    {
        m_DynamicTypeOffset = offset;
    }

    uint32_t LoadContext::AddDynamicMessageSize(uint32_t message_size)
    {
        if (m_DynamicOffsets.Full())
        {
            m_DynamicOffsets.OffsetCapacity(32);
        }
        m_DynamicOffsets.Push(m_DynamicTypeMemoryTotal);

        m_DynamicTypeMemoryTotal += message_size;

        return m_DynamicTypeMemoryTotal;
    }

    uint32_t LoadContext::NextDynamicTypeOffset()
    {
        return m_DynamicOffsets[m_DynamicOffsetCursor++];
    }

    void LoadContext::ResetDynamicOffsetCursor()
    {
        m_DynamicOffsetCursor = 0;
    }
}
