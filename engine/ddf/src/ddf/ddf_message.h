// Copyright 2020-2025 The Defold Foundation
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
        void     SetOneOf(const Descriptor* desc, const FieldDescriptor* field);

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

        char* GetBuffer(uint32_t offset)     { return (char*)(m_Start + offset); }

        const Descriptor*     m_MessageDescriptor;
        uintptr_t             m_Start;
        uintptr_t             m_End;
        bool                  m_DryRun;
    };


    Result DoResolvePointers(const Descriptor* message_descriptor, void* message);
}

#endif // DDF_MESSAGE_H
