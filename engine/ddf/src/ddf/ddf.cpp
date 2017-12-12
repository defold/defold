#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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

    static Result CalculateRepeated(LoadContext* load_context, InputBuffer* ib, const Descriptor* desc)
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
                        load_context->IncreaseArrayCount(start, field->m_Number);
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

                        #if 1
                        InputBuffer sub_ib;
                        if (!ib->SubBuffer(length, &sub_ib))
                        {
                            return RESULT_WIRE_FORMAT_ERROR;
                        }

                        Result e = CalculateRepeated(load_context, &sub_ib, field->m_MessageDescriptor);
                        #else
                        InputBuffer sub_ib = ib;
                        sub_ib->m_End = sub_ib->m_Current + length;
                        Result e = CalculateRepeated(load_context, &sub_ib, field->m_MessageDescriptor);
                        #endif
                        if (e != RESULT_OK)
                            return e;
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
        uint32_t size;
        return LoadMessage(buffer, buffer_size, desc, out_message, 0, &size);
    }

    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** out_message, uint32_t options, uint32_t* size)
    {
        DM_PROFILE(DDF, "LoadMessage");
        assert(buffer);
        assert(desc);
        assert(out_message);

        *size = 0;

        if (desc->m_MajorVersion != DDF_MAJOR_VERSION)
            return RESULT_VERSION_MISMATCH;

        LoadContext load_context(0, 0, true, options);
        Message dry_message = load_context.AllocMessage(desc);

        InputBuffer input_buffer((const char*) buffer, buffer_size);

        Result e = CalculateRepeated(&load_context, &input_buffer, desc);
        if (e != RESULT_OK)
        {
            return e;
        }

        input_buffer.Seek(0);
        e = DoLoadMessage(&load_context, &input_buffer, desc, &dry_message);

        int message_buffer_size = load_context.GetMemoryUsage();
        char* message_buffer = 0;
        dmMemory::AlignedMalloc((void**)&message_buffer, 16, message_buffer_size);
        assert(message_buffer);
        load_context.SetMemoryBuffer(message_buffer, message_buffer_size, false);
        Message message = load_context.AllocMessage(desc);

        input_buffer.Seek(0);
        e = DoLoadMessage(&load_context, &input_buffer, desc, &message);
        if ( e == RESULT_OK )
        {
            *size = message_buffer_size;
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
}
