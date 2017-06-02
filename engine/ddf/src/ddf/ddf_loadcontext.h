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
        void*       AllocRepeated(const FieldDescriptor* field_desc, int count);
        char*       AllocString(int length);
        char*       AllocBytes(int length);
        uint32_t    GetOffset(void* memory);

        void        SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run);
        int         GetMemoryUsage();

        void        IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number);
        uint32_t    GetArrayCount(uint32_t buffer_pos, uint32_t field_number);

        inline uint32_t GetOptions()
        {
            return m_Options;
        }

    private:
        dmHashTable32<uint32_t> m_ArrayCount;

        char* m_Start;
        char* m_End;
        char* m_Current;
        bool  m_DryRun;
        uint32_t m_Options;
    };
}

#endif // DDF_LOADCONTEXT_H
