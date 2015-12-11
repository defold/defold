#include <string.h>
#include <dlib/align.h>

#include <sol/runtime.h>

#include "ddf_loadcontext.h"
#include "ddf_util.h"

namespace dmDDF
{
    LoadContext::LoadContext(char* buffer, int buffer_size, bool dry_run, uint32_t options)
    {
        m_Start = buffer;
        m_Current = buffer;
        m_End = buffer + buffer_size;
        m_DryRun = dry_run;
        m_Options = options;
        if (!dry_run)
        {
            memset(buffer, 0, buffer_size);
        }
    }

    Message LoadContext::AllocMessage(const Descriptor* desc)
    {
        m_Current = (char*) DM_ALIGN(m_Current, 4);
        char* b = m_Current;
        m_Current += desc->m_Size;
        assert(m_DryRun || m_Current <= m_End);

        return Message(desc, b, desc->m_Size, m_DryRun);
    }

    // When loading a message into SOL, the root message object will have type information that lets the gc
    // track allocations out to whatever extra is allocated, so for sub-objects, they can be unpinned
    // (objects and arrays allocated through runtime* functions are automatically pinned)
    static inline void* UnpinAndReturn(void* ptr)
    {
        runtime_unpin(ptr);
        return ptr;
    }

    void* LoadContext::AllocRepeated(const FieldDescriptor* field_desc, int count)
    {
        if ((m_Options & OPTION_SOL) && m_DryRun)
        {
            // Separate allocation
            return m_Current;
        }

        Type type = (Type) field_desc->m_Type;

        m_Current = (char*) DM_ALIGN(m_Current, 4);
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

        char* b = m_Current;

        if (m_Options & OPTION_SOL)
        {
            if (m_DryRun)
            {
                return m_Current;
            }

            if (field_desc->m_Type == TYPE_MESSAGE)
            {
                ::Type* arr_type = GetSolArrayTypeFromHash(field_desc->m_MessageDescriptor->m_NameHash);
                assert(arr_type);
                assert(arr_type->referenced_type);
                return UnpinAndReturn(runtime_alloc_typed_array(arr_type->referenced_type, count));
            }
            else
            {
                return UnpinAndReturn(runtime_alloc_array(element_size, count));
            }
        }
        else
        {
            m_Current += count * element_size;
            assert(m_DryRun || m_Current <= m_End);
            return (void*) b;
        }
    }

    char* LoadContext::AllocString(int length)
    {
        char* b = m_Current;

        if (m_Options & OPTION_SOL)
        {
            if (m_DryRun)
            {
                return m_Current;
            }
            return (char*)UnpinAndReturn(runtime_alloc_array(1, length));
        }
        else
        {
            m_Current += length;
            assert(m_DryRun || m_Current <= m_End);
        }

        return b;
    }

    char* LoadContext::AllocBytes(int length)
    {
        m_Current = (char*) DM_ALIGN(m_Current, 4);
        char* b = m_Current;

        if (m_Options & OPTION_SOL)
        {
            if (m_DryRun)
            {
                return m_Current;
            }

            return (char*)UnpinAndReturn(runtime_alloc_array(1, length));
        }
        else
        {
            m_Current += length;
            assert(m_DryRun || m_Current <= m_End);
        }

        assert(m_DryRun || m_Current <= m_End);
        return b;
    }

    uint32_t LoadContext::GetOffset(void* memory)
    {
        return ((uintptr_t) memory) - ((uintptr_t) m_Start);
    }

    void LoadContext::SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run)
    {
        m_Start = buffer;
        m_Current = buffer;
        m_End = buffer + buffer_size;
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

    void LoadContext::IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number)
    {
        uint64_t key = ((uint64_t) buffer_pos) << 32 | field_number;
        //assert(m_ArrayCount.find(key) != m_ArrayCount.end());
        m_ArrayCount[key]++;
    }

    uint32_t LoadContext::GetArrayCount(uint32_t buffer_pos, uint32_t field_number)
    {
        uint64_t key = ((uint64_t) buffer_pos) << 32 | field_number;
        if (m_ArrayCount.find(key) != m_ArrayCount.end())
            return m_ArrayCount[key];
        else
            return 0;
    }
}

