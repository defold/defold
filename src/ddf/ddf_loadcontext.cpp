#include <string.h>
#include "ddf_loadcontext.h"
#include "ddf_util.h"

CDDFLoadContext::CDDFLoadContext(char* buffer, int buffer_size, bool dry_run)
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

CDDFMessage CDDFLoadContext::AllocMessage(const SDDFDescriptor* desc)
{
    // TODO: Align here!
    char* b = m_Current;
    m_Current += desc->m_Size;
    assert(m_DryRun || m_Current <= m_End);
    
    return CDDFMessage(desc, b, desc->m_Size, m_DryRun);
}

void* CDDFLoadContext::AllocRepeated(const SDDFFieldDescriptor* field_desc, int count)
{
    // TODO: Align here!
    DDFType type = (DDFType) field_desc->m_Type;
 
    int element_size = 0;
    if ( field_desc->m_Type == DDF_TYPE_MESSAGE )
    {
        element_size = field_desc->m_MessageDescriptor->m_Size;
    }
    else if ( field_desc->m_Type == DDF_TYPE_STRING )
    {
        element_size = sizeof(const char*);
    }
    else
    {
        element_size = DDFScalarTypeSize(type);
    }

    char* b = m_Current;
    m_Current += count * element_size;
    assert(m_DryRun || m_Current <= m_End);
    return (void*) b;
}

char* CDDFLoadContext::AllocString(int length)
{
    char* b = m_Current;
    m_Current += length;
    assert(m_DryRun || m_Current <= m_End);

    return b;
}

void CDDFLoadContext::SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run)
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

int CDDFLoadContext::GetMemoryUsage()
{
    return (int) (m_Current - m_Start);
}

void CDDFLoadContext::IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number)
{
    uint64_t key = ((uint64_t) buffer_pos) << 32 | field_number;
    //assert(m_ArrayCount.find(key) != m_ArrayCount.end());
    m_ArrayCount[key]++;
}

uint32_t CDDFLoadContext::GetArrayCount(uint32_t buffer_pos, uint32_t field_number)
{
    uint64_t key = ((uint64_t) buffer_pos) << 32 | field_number;
    if (m_ArrayCount.find(key) != m_ArrayCount.end())
        return m_ArrayCount[key];
    else
        return 0;
}


