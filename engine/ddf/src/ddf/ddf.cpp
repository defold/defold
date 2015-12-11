#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <dlib/profile.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>

#include <sol/runtime.h>
#include <sol/reflect.h>

#include "ddf.h"
#include "ddf_inputbuffer.h"
#include "ddf_load.h"
#include "ddf_save.h"
#include "ddf_util.h"
#include "config.h"

namespace dmDDF
{
    struct SolTypeInfo
    {
        ::Type* type;
        ::Type* array_type;
    };

    Descriptor* g_FirstDescriptor = 0;
    dmHashTable64<const Descriptor*> g_Descriptors;
    dmHashTable<uintptr_t, const Descriptor*> g_SolTypes;
    dmHashTable<uint64_t, SolTypeInfo> g_HashToSolTypes;

    void RegisterSolMessageType(const Descriptor* desc, ::Type* type, ::Type* array_type)
    {
        if (g_SolTypes.Full())
        {
            g_SolTypes.SetCapacity(587, g_SolTypes.Capacity() + 128);
        }
        if (g_HashToSolTypes.Full())
        {
            g_HashToSolTypes.SetCapacity(587, g_HashToSolTypes.Capacity() + 128);
        }

        SolTypeInfo sti;
        sti.type = type;
        sti.array_type = array_type;
        g_SolTypes.Put((uintptr_t)type, desc);
        g_HashToSolTypes.Put(desc->m_NameHash, sti);
    }

    static void RegisterSolModule(void* context, const dmhash_t* name_hash, const char **module_name)
    {
        ::Module* mod = reflect_get_module(*module_name);
        if (!mod)
        {
            dmLogError("Sol module '%s' could not be resolved!", *module_name);
            return;
        }

        // There is a generated special struct DdfModuleTypeRefs in the .sol file which
        // contains all the reference & arrray types that are needed for ddf lodaing. So
        // look up that struct + the one to register.
        dmHashTable<dmhash_t, ::Type*> array_refs;
        dmHashTable<dmhash_t, ::Type*> struct_refs;
        array_refs.SetCapacity(17, 512);
        struct_refs.SetCapacity(17, 512);

        // 1. Look for DdfModuleTypeRefs
        //      Add all types to tmp
        // 2. Match up with all descriptors
        uint32_t size = runtime_array_length(mod->types);
        for (uint32_t i=0;i!=size;i++)
        {
            ::Type* t = mod->types[i];
            if (t->kind != KIND_STRUCT)
                continue;

            if (t->struct_type && !strcmp(t->struct_type->name, "DdfModuleTypeRefs"))
            {
                ::StructType* s = t->struct_type;
                for (uint32_t i=0;i!=s->member_count;i++)
                {
                    ::Type* st = s->members[i].type;
                    if (st->referenced_type)
                    {
                        if (st->referenced_type->struct_type)
                            struct_refs.Put(dmHashString64(st->referenced_type->struct_type->name), st);
                        else if (st->referenced_type->array_type && st->referenced_type->array_type->element_type)
                            array_refs.Put(dmHashString64(st->referenced_type->array_type->element_type->struct_type->name), st);
                    }
                }
            }
        }

        const Descriptor* d = g_FirstDescriptor;
        while (d)
        {
            if (d->m_SolName && !strcmp(d->m_SolModule, *module_name))
            {
                dmhash_t name_hash = dmHashString64(d->m_SolName);
                ::Type** struct_ref = struct_refs.Get(name_hash);
                ::Type** array_ref = array_refs.Get(name_hash);
                if (struct_ref && array_ref)
                {
                    RegisterSolMessageType(d, *struct_ref, *array_ref);
                }
            }
            d = (const Descriptor*) d->m_NextDescriptor;
        }
    }

    void RegisterAllTypes()
    {
        const Descriptor* d = g_FirstDescriptor;
        g_Descriptors.Clear();

        dmHashTable<dmhash_t, const char*> sol_modules;
        sol_modules.SetCapacity(17, 512);

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

            if (d->m_SolModule)
            {
                sol_modules.Put(dmHashString64(d->m_SolModule), d->m_SolModule);
            }

            d = (const Descriptor*) d->m_NextDescriptor;
        }

        sol_modules.Iterate(&RegisterSolModule, (void*)0);
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

        if (desc->m_MajorVersion != DDF_MAJOR_VERSION)
            return RESULT_VERSION_MISMATCH;

        LoadContext load_context(0, 0, true, options);
        Message dry_message = load_context.AllocMessage(desc);

        InputBuffer input_buffer((const char*) buffer, buffer_size);

        Result e = CalculateRepeated(&load_context, &input_buffer, desc);
        if (e != RESULT_OK)
        {
            *size = 0;
            return e;
        }

        input_buffer.Seek(0);
        e = DoLoadMessage(&load_context, &input_buffer, desc, &dry_message);

        int message_buffer_size = load_context.GetMemoryUsage();

        char* message_buffer;

        if (options & OPTION_PRE_ALLOCATED)
        {
            message_buffer = (char*)(*out_message);
            if (message_buffer_size > *size)
            {
                *out_message = 0;
                *size = message_buffer_size;
                return RESULT_BUFFER_TOO_SMALL;
            }
        }
        else
        {
            if (options & OPTION_SOL)
            {
                ::Type* type = GetSolTypeFromHash(desc->m_NameHash);
                assert(type);
                assert(type->referenced_type);
                assert(type->referenced_type->struct_type);
                message_buffer = (char*) runtime_alloc_struct(type->referenced_type);
                assert(message_buffer_size == desc->m_Size);
                assert(message_buffer_size == type->referenced_type->struct_type->size);
            }
            else
            {
                message_buffer = (char*) malloc(message_buffer_size);
            }
        }


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
            if (!(options & OPTION_PRE_ALLOCATED))
            {
                if (options & OPTION_SOL)
                {
                    runtime_unpin((void*) message_buffer);
                }
                else
                {
                    free((void*) message_buffer);
                }
            }
            *size = 0;
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

            void* buffer = malloc(size);
            if ( fread(buffer, 1, size, f) != (size_t) size )
            {
                free(buffer);
                fclose(f);
                return RESULT_IO_ERROR;
            }

            Result e = LoadMessage(buffer, (uint32_t) size, desc, message);
            fclose(f);
            free(buffer);
            return e;
        }
        else
        {
            return RESULT_IO_ERROR;
        }
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
        free(message);
    }

    ::Type* GetSolTypeFromHash(dmhash_t hash)
    {
        const SolTypeInfo* res = g_HashToSolTypes.Get(hash);
        return res ? res->type : 0;
    }

    ::Type* GetSolArrayTypeFromHash(dmhash_t hash)
    {
        const SolTypeInfo* res = g_HashToSolTypes.Get(hash);
        return res ? res->array_type : 0;
    }

    const Descriptor* GetDescriptorFromSolType(::Type* sol_type)
    {
        const Descriptor** res = g_SolTypes.Get((uintptr_t)sol_type);
        return res ? *res : 0;
    }

    // SOL support wrappers.
    extern "C"
    {
        Result SolDDFLoadMessage(char* buffer, uint32_t offset, uint32_t length, ::Type* type, struct Any* out_message)
        {
            if (!buffer || !out_message)
            {
                dmLogError("Invalid parameters passed for DDF loading");
                return RESULT_INTERNAL_ERROR;
            }
            
            uint32_t buflen = runtime_array_length((void*)buffer);
            if (length > buflen || (offset + length) > buflen)
            {
                dmLogError("Invalid buffer range passed for DDF loading");
                return RESULT_INTERNAL_ERROR;
            }
        
            // options are not exposed here because it must always be OPTION_SOL or crashes will happen.
            const Descriptor** desc = g_SolTypes.Get((uintptr_t) type);
            if (desc)
            {
                void *msg = 0;
                uint32_t size = 0;
                Result r = LoadMessage(buffer + offset, length, *desc, &msg, OPTION_SOL, &size);
                if (msg)
                {
                    // hand over to sol
                    runtime_unpin(msg);
                }
                *out_message = reflect_create_reference_any(type, msg);
                return r;
            }
            else
            {
                reflect_create_reference_any(type, 0);
                return RESULT_INTERNAL_ERROR;
            }
        }

        const ::Type* SolDDFGetTypeFromHash(uint64_t hash)
        {
            return GetSolTypeFromHash(hash);
        }
    }
}
