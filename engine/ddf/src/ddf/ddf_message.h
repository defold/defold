#ifndef DDF_MESSAGE_H
#define DDF_MESSAGE_H

#include <stdint.h>
#include "ddf.h"
#include "ddf_inputbuffer.h"
#include "ddf_loadcontext.h"

namespace dmDDF
{
    class LoadContext;

    class Message
    {
    public:
        Message(const Descriptor* message_descriptor, char* buffer, uint32_t buffer_size, bool dry_run);

        Result ReadField(LoadContext* load_context,
                         WireType wire_type,
                         const FieldDescriptor* field,
                         InputBuffer* input_buffer);

        void     SetScalar(const FieldDescriptor* field, const void* buffer, int buffer_size);
        void*    AddScalar(const FieldDescriptor* field, const void* buffer, int buffer_size);
        void*    AddMessage(const FieldDescriptor* field);
        void     AllocateRepeatedBuffer(LoadContext* load_context, const FieldDescriptor* field, int element_count);
        void     SetRepeatedBuffer(const FieldDescriptor* field, void* buffer);
        void     SetString(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len);
        void     AddString(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len);
        void     SetBytes(LoadContext* load_context, const FieldDescriptor* field, const char* buffer, int buffer_len);

        Message  SubMessage(const FieldDescriptor* field);

    private:
        Result ReadScalarField(LoadContext* load_context,
                                 WireType wire_type,
                                 const FieldDescriptor* field,
                                 InputBuffer* input_buffer);

        Result ReadStringField(LoadContext* load_context,
                                 WireType wire_type,
                                 const FieldDescriptor* field,
                                 InputBuffer* input_buffer);

        Result ReadMessageField(LoadContext* load_context,
                                  WireType wire_type,
                                  const FieldDescriptor* field,
                                  InputBuffer* input_buffer);

        Result ReadBytesField(LoadContext* load_context,
                                WireType wire_type,
                                const FieldDescriptor* field,
                                InputBuffer* input_buffer);

        const Descriptor*     m_MessageDescriptor;
        char*                 m_Start;
        char*                 m_End;
        bool                  m_DryRun;
    };


    Result DoResolvePointers(const Descriptor* message_descriptor, void* message);
}

#endif // DDF_MESSAGE_H
