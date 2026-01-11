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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <dlib/align.h>
#include <dlib/memory.h>
#include <dlib/profile.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include "ddf.h"
#include "ddf_inputbuffer.h"
#include "ddf_load.h"
#include "ddf_save.h"
#include "ddf_util.h"
#include "config.h"

namespace dmDDF
{
    #define DDF_CHECK_RESULT(e) \
        if (e != RESULT_OK) \
            return e;

    Descriptor* g_FirstDescriptor = 0;
    dmHashTable64<const Descriptor*> g_Descriptors;

    void RegisterAllTypes()
    {
        const Descriptor* d = g_FirstDescriptor;
        g_Descriptors.Clear();
        while (d)
        {
            if (g_Descriptors.Full())
            {
                g_Descriptors.SetCapacity(587, g_Descriptors.Capacity() + 128);
            }

            dmhash_t name_hash = dmHashString64(d->m_Name);
            if (g_Descriptors.Get(name_hash) != 0)
            {
                // Logging is disabled. See case https://defold.fogbugz.com/default.asp?740
                //dmLogError("Name clash. Type %s already registered.", d->m_Name)
            }
            else
            {
                g_Descriptors.Put(name_hash, d);
            }

            d = (const Descriptor*) d->m_NextDescriptor;
        }
    }

    InternalRegisterDescriptor::InternalRegisterDescriptor(Descriptor* descriptor)
    {
        descriptor->m_NextDescriptor = g_FirstDescriptor;
        g_FirstDescriptor = descriptor;
    }


    const Descriptor* GetDescriptorFromHash(dmhash_t hash)
    {
        const Descriptor** d = g_Descriptors.Get(hash);
        if (d)
            return *d;
        else
            return 0;
    }

    const Descriptor* GetDescriptor(const char* name)
    {
        return GetDescriptorFromHash(dmHashString64(name));
    }

    static Result CalculateDynamicDescriptorSize(LoadContext* load_context, InputBuffer* ib, const Descriptor* desc, bool is_dynamic_type)
    {
        while (!ib->Eof())
        {
            uint32_t tag;
            if (!ib->ReadVarInt32(&tag))
                return RESULT_WIRE_FORMAT_ERROR;

            uint32_t key  = tag >> 3;
            uint32_t type = tag & 0x7;

            if (key == 0)
                return RESULT_WIRE_FORMAT_ERROR;

            const FieldDescriptor* field = FindField(desc, key, 0);
            if (!field)
            {
                // Unknown field, just skip
                Result e = SkipField(ib, type);
                DDF_CHECK_RESULT(e);
                continue;
            }

            if (field->m_Type == TYPE_MESSAGE)
            {
                // All submessages are length-delimited
                uint32_t length;
                if (!ib->ReadVarInt32(&length))
                    return RESULT_WIRE_FORMAT_ERROR;

                // Create a view of just this submessage's bytes
                InputBuffer sub_ib;
                if (!ib->SubBuffer(length, &sub_ib))
                    return RESULT_WIRE_FORMAT_ERROR;

                is_dynamic_type = is_dynamic_type || !field->m_FullyDefinedType;
                if (is_dynamic_type)
                {
                    // We need to account for the injected oneof index value that we insert into the structs via ddfc.py!
                    uint32_t dynamic_size = field->m_MessageDescriptor->m_Size;
                    if (field->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
                    {
                        dynamic_size += sizeof(uint32_t);
                    }

                    load_context->AddDynamicMessageSize(dynamic_size);
                }

                // Recurse into the submessage descriptor
                Result e = CalculateDynamicDescriptorSize(load_context, &sub_ib, field->m_MessageDescriptor, is_dynamic_type);
                DDF_CHECK_RESULT(e);

                // Ensure the sub-buffer is fully consumed
                if (!sub_ib.Eof())
                {
                    return RESULT_WIRE_FORMAT_ERROR;
                }
            }
            else
            {
                // Primitive field, just skip its payload
                Result e = SkipField(ib, type);
                DDF_CHECK_RESULT(e);
            }
        }

        return RESULT_OK;
    }

    static Result CreateMessage(LoadContext* load_context, InputBuffer* ib, const Descriptor* desc, Message* message_out)
    {
        // If the descriptor contains any dynamic fields, we need to step through the input buffer
        // to pre-warm the load_context with all the dynamic field sizes.
        if (desc->m_ContainsDynamicFields)
        {
            Result e = CalculateDynamicDescriptorSize(load_context, ib, desc, false);
            DDF_CHECK_RESULT(e);
        }

        *message_out = load_context->AllocMessage(desc);

        return RESULT_OK;
    }

    static Result CalculateRepeated(LoadContext* load_context, InputBuffer* ib, const Descriptor* desc, uint32_t* array_info_hash, bool is_dynamic_type)
    {
        assert(desc);

        // Calculate number of entries in arrays, ie memory requirements for the entire message
        uint32_t start = ib->Tell();
        while (!ib->Eof())
        {
            uint32_t tag;
            if (ib->ReadVarInt32(&tag))
            {
                uint32_t key = tag >> 3;
                uint32_t type = tag & 0x7;

                if (key == 0)
                    return RESULT_WIRE_FORMAT_ERROR;

                const FieldDescriptor* field = FindField(desc, key, 0);

                if (field == 0)
                {
                    Result e = SkipField(ib, type);
                    if (e != RESULT_OK)
                        return e;
                }
                else
                {
                    if (field->m_Label == LABEL_REPEATED)
                    {
                        *array_info_hash = load_context->IncreaseArrayCount(start, field->m_Number);
                        is_dynamic_type = false;
                    }

                    if (field->m_Type != TYPE_MESSAGE)
                    {
                        Result e = SkipField(ib, type);
                        if (e != RESULT_OK)
                            return e;
                    }
                    else
                    {
                        assert(field->m_MessageDescriptor);
                        uint32_t length;
                        if (!ib->ReadVarInt32(&length))
                            return RESULT_WIRE_FORMAT_ERROR;

                        InputBuffer sub_ib;
                        if (!ib->SubBuffer(length, &sub_ib))
                        {
                            return RESULT_WIRE_FORMAT_ERROR;
                        }

                        is_dynamic_type = is_dynamic_type || !field->m_FullyDefinedType;
                        if (is_dynamic_type && *array_info_hash != 0)
                        {
                            // We need to account for the injected oneof index value that we insert into the structs via ddfc.py!
                            uint32_t dynamic_size = field->m_MessageDescriptor->m_Size;
                            if (field->m_OneOfIndex != DDF_NO_ONE_OF_INDEX)
                            {
                                dynamic_size += sizeof(uint32_t);
                            }

                            load_context->AddDynamicMessageSize(dynamic_size);
                        }

                        Result e = CalculateRepeated(load_context, &sub_ib, field->m_MessageDescriptor, array_info_hash, is_dynamic_type);

                        if (e != RESULT_OK)
                        {
                            return e;
                        }
                    }
                }
            }
            else
            {
                return RESULT_WIRE_FORMAT_ERROR;
            }
        }
        return RESULT_OK;
    }

    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** out_message)
    {
        return LoadMessage(buffer, buffer_size, desc, out_message, 0, 0);
    }

    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** out_message, uint32_t options, uint32_t* size)
    {
        DM_PROFILE("DdfLoadMessage");
        assert(buffer);
        assert(desc);
        assert(out_message);

        if (size)
            *size = 0;

        if (desc->m_MajorVersion != DDF_MAJOR_VERSION)
            return RESULT_VERSION_MISMATCH;

        LoadContext load_context(0, 0, true, options);
        InputBuffer input_buffer((const char*) buffer, buffer_size);

        // --- About DDF loading and message layout ---
        //
        // When loading a DDF message, we first compute how much memory is required
        // *without actually loading or resolving any of the data*. This size calculation
        // includes space for both static fields and any dynamically sized message fields.
        //
        // The final in-memory representation is split into two regions:
        //   1. A statically sized block for fields whose size is fully known at compile time.
        //   2. A separate dynamic block for fields whose size depends on other message types.
        //
        // A field becomes “dynamic” when its generated C++ type is a pointer rather than an
        // in-place struct. This happens when message definitions are recursive or mutually
        // dependent in the .proto file.
        //
        // Example:
        //   message MessageRecursiveA {
        //       optional int32 val_a = 1;
        //       optional MessageRecursiveB my_b = 2;
        //   }
        //   message MessageRecursiveB {
        //       optional int32 val_b = 1;
        //       optional MessageRecursiveA my_a = 2;
        //   }
        //
        // In this example, the field `my_b` in MessageRecursiveA is compiled as a pointer.
        // The actual data for that field must be allocated and resolved later during loading.
        //
        // Because such dynamic fields can form arbitrarily deep or cyclic nesting, we collect
        // all dynamic subtrees separately. This also ensures that repeated fields can be laid
        // out contiguously in memory, while still allowing pointer-based references between
        // dependent message types.

        Message dry_message(0, 0, 0, true);
        Result e = CreateMessage(&load_context, &input_buffer, desc, &dry_message);

        uint32_t array_info_hash = 0;
        input_buffer.Seek(0);
        e = CalculateRepeated(&load_context, &input_buffer, desc, &array_info_hash, false);
        DDF_CHECK_RESULT(e);

        input_buffer.Seek(0);
        e = DoLoadMessage(&load_context, &input_buffer, desc, &dry_message);

        // Once the dry run is done, we can calculate the actual size of the message including
        // the memory for the dynamic messages.
        int aligned_base_memory = DM_ALIGN(load_context.GetMemoryUsage(), 16);
        int message_buffer_size = aligned_base_memory + load_context.GetDynamicTypeMemorySize();
        char* message_buffer = 0;

        dmMemory::AlignedMalloc((void**)&message_buffer, 16, message_buffer_size);
        assert(message_buffer);
        load_context.SetMemoryBuffer(message_buffer, message_buffer_size, false);
        load_context.SetDynamicTypeBase(aligned_base_memory);

        Message message = load_context.AllocMessage(desc);

        // The dry-run has prepped the load_context with a list of offsets for all of the dynamic types,
        // which we will walk over in the same order again when the actual message is loaded.
        load_context.ResetDynamicOffsetCursor();

        input_buffer.Seek(0);
        e = DoLoadMessage(&load_context, &input_buffer, desc, &message);

        if ( e == RESULT_OK )
        {
            if (size)
            {
                *size = message_buffer_size;
            }
            *out_message = (void*) message_buffer;
        }
        else
        {
            dmMemory::AlignedFree((void*) message_buffer);
            *out_message = 0;
        }
        return e;
    }

    Result LoadMessageFromFile(const char* file_name, const Descriptor* desc, void** message)
    {
        FILE* f = fopen(file_name, "rb");
        if (f)
        {
            if (fseek(f, 0, SEEK_END) != 0)
            {
                fclose(f);
                return RESULT_IO_ERROR;
            }

            long size = ftell(f);

            if (fseek(f, 0, SEEK_SET) != 0)
            {
                fclose(f);
                return RESULT_IO_ERROR;
            }

            void* buffer = 0;
            assert(dmMemory::RESULT_OK == dmMemory::AlignedMalloc(&buffer, 16, size));
            if ( fread(buffer, 1, size, f) != (size_t) size )
            {
                dmMemory::AlignedFree(buffer);
                fclose(f);
                return RESULT_IO_ERROR;
            }

            Result e = LoadMessage(buffer, (uint32_t) size, desc, message);
            fclose(f);
            dmMemory::AlignedFree(buffer);
            return e;
        }
        else
        {
            return RESULT_IO_ERROR;
        }
    }

    Result ResolvePointers(const Descriptor* desc, void* message)
    {
        return DoResolvePointers(desc, message);
    }

    Result SaveMessage(const void* message, const Descriptor* desc, void* context, SaveFunction save_function)
    {
        return DoSaveMessage(message, desc, context, save_function);
    }

    static bool DDFFileSaveFunction(void* context, const void* buffer, uint32_t buffer_size)
    {
        size_t nwritten = fwrite(buffer, 1, buffer_size, (FILE*) context);
        return nwritten == buffer_size;
    }

    Result SaveMessageToFile(const void* message, const Descriptor* desc, const char* file_name)
    {
        FILE* file = fopen(file_name, "wb");
        if (!file)
            return RESULT_IO_ERROR;
        Result ret = SaveMessage(message, desc, file, &DDFFileSaveFunction);
        fclose(file);
        return ret;
    }

    static bool SaveMessageSizeFunction(void* context, const void* buffer, uint32_t buffer_size)
    {
        uint32_t* count = (uint32_t*) context;
        *count = *count + buffer_size;
        return true;
    }

    Result SaveMessageSize(const void* message, const Descriptor* desc, uint32_t* size)
    {
        uint32_t calc_size = 0;
        Result e = SaveMessage(message, desc, &calc_size, &SaveMessageSizeFunction);
        if (e == RESULT_OK)
        {
            *size = calc_size;
        }
        else
        {
            *size = 0;
        }

        return e;
    }

    static bool SaveArrayFunction(void* context, const void* buffer, uint32_t buffer_size)
    {
        dmArray<uint8_t>* array = (dmArray<uint8_t>*) context;
        if (array->Remaining() < buffer_size)
        {
            array->OffsetCapacity(buffer_size + 1024);
        }

        array->PushArray((uint8_t*) buffer, buffer_size);
        return true;
    }

    Result SaveMessageToArray(const void* message, const Descriptor* desc, dmArray<uint8_t>& array)
    {
        array.SetSize(0);
        Result ret = SaveMessage(message, desc, &array, &SaveArrayFunction);
        return ret;
    }

    Result CopyMessage(const void* message, const dmDDF::Descriptor* desc, void** out)
    {
        if (!message)
            return RESULT_INTERNAL_ERROR;

        dmArray<uint8_t> buffer;
        dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(message, desc, buffer);
        if (dmDDF::RESULT_OK != ddf_result)
        {
            return ddf_result;
        }

        ddf_result = dmDDF::LoadMessage((void*)&buffer[0], buffer.Size(), desc, out);
        if (dmDDF::RESULT_OK != ddf_result)
        {
            return ddf_result;
        }

        return RESULT_OK;
    }

    int32_t GetEnumValue(const EnumDescriptor* desc, const char* name)
    {
        assert(desc);
        assert(name);

        uint32_t n = desc->m_EnumValueCount;
        for (uint32_t i = 0; i < n; ++i)
        {
            if (strcmp(name, desc->m_EnumValues[i].m_Name) == 0)
            {
                return desc->m_EnumValues[i].m_Value;
            }
        }

        assert(false);
        return INT32_MIN;
    }

    const char* GetEnumName(const EnumDescriptor* desc, int32_t value)
    {
        uint32_t n = desc->m_EnumValueCount;
        for (uint32_t i = 0; i < n; ++i)
        {
            if (desc->m_EnumValues[i].m_Value == value)
            {
                return desc->m_EnumValues[i].m_Name;
            }
        }

        return 0;
    }

    void FreeMessage(void* message)
    {
        assert(message);
        dmMemory::AlignedFree(message);
    }

    #undef DDF_CHECK_RESULT
}
