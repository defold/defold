#include <assert.h>
#include <string.h>

#include "ddf.h"
#include "ddf_private.h"

int DDFScalarTypeSize(uint32_t type)
{
    switch(type)
    {
    case DDF_TYPE_BOOL:
        assert(false && "Bools not supported."); // TODO: Fix?
        return -1;

    case DDF_TYPE_INT32:
    case DDF_TYPE_FLOAT:
    case DDF_TYPE_FIXED32:
    case DDF_TYPE_UINT32:
    case DDF_TYPE_ENUM: // TODO: Enum are 32-bit? Verify with test.
    case DDF_TYPE_SFIXED32:
    case DDF_TYPE_SINT32:
        return 4;

    case DDF_TYPE_SFIXED64:
    case DDF_TYPE_SINT64:
    case DDF_TYPE_DOUBLE:
    case DDF_TYPE_INT64:
    case DDF_TYPE_UINT64:
    case DDF_TYPE_FIXED64:
        return 8;

    /*
      DDF_TYPE_STRING
      DDF_TYPE_GROUP
      DDF_TYPE_MESSAGE
      DDF_TYPE_BYTES
      ...
    */
    default:
        assert(false && "Internal error");
        return -1;
    }
}

CDDFMessageBuilder::CDDFMessageBuilder(char *buffer, uint32_t buffer_size, bool dry_run, bool clear_memory)
{
    m_Start = buffer;
    m_Current = buffer;
    m_End = buffer + buffer_size;
    m_DryRun = dry_run;

    // Clear memory. eg "ArrayCount" depends on this
    // Memory is also cleared again when Reserve is called...
    if (!m_DryRun && clear_memory)
        memset(m_Start, 0, m_End - m_Start);
}

void CDDFMessageBuilder::WriteBuffer(const char *buf, uint32_t buf_size)
{
    assert(m_Current <= m_End);
    if (!m_DryRun)
        memcpy((void*) m_Current, buf, buf_size);
    m_Current += buf_size;
    assert(m_Current <= m_End);
}

void CDDFMessageBuilder::WriteField(const SDDFFieldDescriptor *field, const char *buf, uint32_t buf_size)
{
    assert(m_Current + field->m_Offset + buf_size <= m_End);
    // TODO: assert "sizeof(field)" == buf_size

    if (!m_DryRun)
        memcpy((void*) &m_Current[field->m_Offset], buf, buf_size);
    //m_Current += buf_size;
    assert(m_Current <= m_End);
}

void CDDFMessageBuilder::AddArrayCount_(const SDDFFieldDescriptor *field, uint32_t amount)
{
    assert(!m_DryRun);
    assert(field->m_Label == DDF_LABEL_REPEATED);
    void*p = &m_Current[field->m_Offset + sizeof(void*)];
    *((uint32_t*) p) += amount;
}

uint32_t CDDFMessageBuilder::GetArrayCount(const SDDFFieldDescriptor *field)
{
    assert(!m_DryRun);
    assert(field->m_Label == DDF_LABEL_REPEATED);

    void*p = &m_Current[field->m_Offset + sizeof(void*)];
    return *((uint32_t*) p);
}

void CDDFMessageBuilder::SetArrayCount(const SDDFFieldDescriptor *field, uint32_t count)
{
    assert(!m_DryRun);
    assert(field->m_Label == DDF_LABEL_REPEATED);

    void*p = &m_Current[field->m_Offset + sizeof(void*)];
    *((uint32_t*) p) = count;
}

void CDDFMessageBuilder::SetArrayPointer(const SDDFFieldDescriptor *field, uintptr_t ptr)
{
    assert(!m_DryRun);
    assert(field->m_Label == DDF_LABEL_REPEATED);

    uintptr_t* p = (uintptr_t*) &m_Current[field->m_Offset];
    *p = ptr;
}

uint32_t CDDFMessageBuilder::GetNextArrayElementOffset(const SDDFFieldDescriptor *field)
{
    if (m_DryRun) // TODO: hmmm, ok?
        return 0;

//      assert(!m_DryRun);
    assert(field->m_Label == DDF_LABEL_REPEATED);
    // TODO: Does not work for scalar values

    uint32_t size = 0xffffffff;
    if (field->m_Type == DDF_TYPE_MESSAGE)
    {
        assert(field->m_MessageDescriptor);
        size = field->m_MessageDescriptor->m_Size;
    }
    else
    {
        size = DDFScalarTypeSize(field->m_Type);
    }

    uintptr_t* array = (uintptr_t*) &m_Current[field->m_Offset];
    uint32_t* count = (uint32_t*) &m_Current[field->m_Offset + sizeof(void*)];

    char* p = (char*) (*array + (*count) * size);
    *count = *count + 1;

    assert(p >= m_Start);
    //assert(p + field->m_MessageDescriptor->m_Size <= m_End );

    return (uint32_t) (p - m_Start);
}

void CDDFMessageBuilder::Align(uint32_t align)
{
    assert(m_Current <= m_End);
    uintptr_t next_current = (uintptr_t) m_Current;
    next_current += align - 1;
    next_current &= ~(align - 1);
    m_Current = (char*) next_current;
    assert(m_Current <= m_End);
}

CDDFMessageBuilder CDDFMessageBuilder::Reserve(uint32_t size)
{
    assert(m_Current <= m_End);

    CDDFMessageBuilder ret(m_Current, size, m_DryRun);
//      char* ret = m_Current;
    m_Current += size;
    assert(m_Current <= m_End);
    return ret;
}

CDDFMessageBuilder CDDFMessageBuilder::SubMessageBuilder(uint32_t offset)
{
    assert(m_Current <= m_End);

    CDDFMessageBuilder ret(m_Start + offset, m_End - (m_Start + offset), m_DryRun, false);
    assert(ret.m_Start <= m_End);
    return ret;
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
    assert(m_ArrayCount.find(key) != m_ArrayCount.end());
    return m_ArrayCount[key];
}
