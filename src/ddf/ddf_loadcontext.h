#ifndef DDF_LOADCONTEXT_H
#define DDF_LOADCONTEXT_H

#include <stdint.h>
#include "ddf.h"
#include "ddf_message.h"

#include <map> // TODO: Remove map...!!!

namespace dmDDF
{
    class Message;

    class LoadContext
    {
    public:
        LoadContext(char* buffer, int buffer_size, bool dry_run);
        Message     AllocMessage(const Descriptor* desc);
        void*       AllocRepeated(const FieldDescriptor* field_desc, int count);
        char*       AllocString(int length);

        void        SetMemoryBuffer(char* buffer, int buffer_size, bool dry_run);
        int         GetMemoryUsage();

        void        IncreaseArrayCount(uint32_t buffer_pos, uint32_t field_number);
        uint32_t    GetArrayCount(uint32_t buffer_pos, uint32_t field_number);

    private:
        std::map<uint64_t, uint32_t> m_ArrayCount;

        char* m_Start;
        char* m_End;
        char* m_Current;
        bool  m_DryRun;
    };
}

#endif // DDF_LOADCONTEXT_H 
