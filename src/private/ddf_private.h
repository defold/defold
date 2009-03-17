#ifndef DDF_PRIVATE_H
#define DDF_PRIVATE_H

#include <stdint.h>

/** 
 * Calculates scalar type size. Only valid for scalar types.
 * @param type Type
 * @return Type size
 */
int DDFScalarTypeSize(uint32_t type);

class CDDFMessageBuilder
{
public:
    CDDFMessageBuilder(char* buffer, uint32_t buffer_size, bool dry_run, bool clear_memory = true);

    void                WriteBuffer(const char* buf, uint32_t buf_size);
    void                WriteField(const SDDFFieldDescriptor* field, const char* buf, uint32_t buf_size);

    void                AddArrayCount_(const SDDFFieldDescriptor* field, uint32_t amount);
    uint32_t            GetArrayCount(const SDDFFieldDescriptor* field);
    void                SetArrayCount(const SDDFFieldDescriptor* field, uint32_t count);
    void                SetArrayPointer(const SDDFFieldDescriptor* field, uintptr_t ptr);
    uint32_t            GetNextArrayElementOffset(const SDDFFieldDescriptor* field);

    void                Align(uint32_t align);
    CDDFMessageBuilder  Reserve(uint32_t size);
    CDDFMessageBuilder  SubMessageBuilder(uint32_t offset);

    char* m_Start;
    char* m_End;
    char* m_Current;
    bool  m_DryRun;
};

#include <map> // TODO: Remove map...!!!

class CDDFLoadContext
{
public:

    void     IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number);
    uint32_t GetArrayCount(uint32_t buffer_pos, uint32_t field_number);

    std::map<uint64_t, uint32_t> m_ArrayCount;
};

#endif // DDF_PRIVATE_H
