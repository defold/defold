#include "gameobject_props.h"

#include <string.h>
#include <stdlib.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <script/script.h>

#include "gameobject_private.h"

namespace dmGameObject
{
    PropertySet::PropertySet()
    {
        memset(this, 0, sizeof(*this));
    }

    PropertyDesc::PropertyDesc()
    {
        memset(this, 0, sizeof(*this));
    }

    Properties::Properties()
    {
        memset(this, 0, sizeof(*this));
    }

    NewPropertiesParams::NewPropertiesParams()
    {
        memset(this, 0, sizeof(*this));
    }

    HProperties NewProperties(const NewPropertiesParams& params)
    {
        Properties* properties = new Properties();
        properties->m_ResolvePathCallback = params.m_ResolvePathCallback;
        properties->m_ResolvePathUserData = params.m_ResolvePathUserData;
        properties->m_GetURLCallback = params.m_GetURLCallback;
        return properties;
    }

    void DeleteProperties(HProperties properties)
    {
        if (properties != 0x0)
        {
            for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
            {
                PropertySet& set = properties->m_Set[i];
                if (set.m_FreeUserDataCallback != 0)
                {
                    set.m_FreeUserDataCallback(set.m_UserData);
                }
            }
            delete properties;
        }
    }

    void SetPropertySet(HProperties properties, PropertyLayer layer, const PropertySet& set)
    {
        properties->m_Set[layer] = set;
    }

    static void LogNotFound(dmhash_t id)
    {
        dmLogError("The property with id '%s' could not be found.", dmHashReverseSafe64(id));
    }

    PropertyResult GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var)
    {
        for (uint32_t i = 0; i < MAX_PROPERTY_LAYER_COUNT; ++i)
        {
            const PropertySet& set = properties->m_Set[i];
            if (set.m_GetPropertyCallback != 0x0)
            {
                PropertyResult result = set.m_GetPropertyCallback(properties, set.m_UserData, id, var);
                if (result != PROPERTY_RESULT_NOT_FOUND)
                {
                    return result;
                }
            }
        }
        LogNotFound(id);
        return PROPERTY_RESULT_NOT_FOUND;
    }

    enum PropertyContainerType
    {
        PROPERTY_CONTAINER_TYPE_NUMBER = 0,
        PROPERTY_CONTAINER_TYPE_HASH = 1,
        PROPERTY_CONTAINER_TYPE_URL = 2,
        PROPERTY_CONTAINER_TYPE_VECTOR3 = 3,
        PROPERTY_CONTAINER_TYPE_VECTOR4 = 4,
        PROPERTY_CONTAINER_TYPE_QUAT = 5,
        PROPERTY_CONTAINER_TYPE_BOOLEAN = 6,
        PROPERTY_CONTAINER_TYPE_URL_STRING = 7,
        PROPERTY_CONTAINER_TYPE_COUNT
    };

    struct PropertyContainer
    {
        uint32_t m_Count;
        dmhash_t* m_Ids;
        uint32_t* m_Indexes;
        PropertyContainerType* m_Types;
        dmhash_t* m_HashData;
        float* m_FloatData;
        char* m_URLData;
        char* m_StringData;
        // The actual data for the arrays is stored here, all the data is allocated in one chunk of memory
        // [optional padding to align dmhash_t ID array]
        // dmhash_t [entry_count]               The ID hash of each entry
        // uint32_t [entry_count]               The offset into respective data array depending on type
        // PropertyContainerType [entry_count]  The type of the entry
        // [optional padding to align dmhash_t data]
        // dmhash_t [hash_count]                All the hash values
        // float [float_count]                  All the number, vector3, vector4 and quad values
        // char [32*url_count]                  All the URLs (dmMessage::URL in char[32] form)
        // char [url_string_size + bool_count]  All the string for string based urls and one char per boolean
    };

    struct PropertyContainerBuilder
    {
        PropertyContainerBuilder(PropertyContainer* property_container)
            : m_PropertyContainer(property_container)
            , m_EntryOffset(0)
            , m_FloatOffset(0)
            , m_HashOffset(0)
            , m_StringOffset(0)
            , m_URLOffset(0)
        {}
        PropertyContainer* m_PropertyContainer;
        uint32_t m_EntryOffset;
        uint32_t m_FloatOffset;
        uint32_t m_HashOffset;
        uint32_t m_StringOffset;
        uint32_t m_URLOffset;
    };

    static HPropertyContainer AllocatePropertyContainer(const PropertyContainerParameters& params)
    {
        uint32_t prop_count = params.m_NumberCount + params.m_HashCount + params.m_URLStringCount + params.m_URLCount + params.m_Vector3Count + params.m_Vector4Count + params.m_QuatCount + params.m_BoolCount;

        // The sizes are aligned so the data following after will be properly aligned.
        size_t struct_offset = 0;
        size_t struct_size = sizeof(PropertyContainer);

        size_t ids_offset = DM_ALIGN(struct_offset + struct_size, 8);
        size_t ids_size = sizeof(dmhash_t) * prop_count;

        size_t indexes_offset = DM_ALIGN(ids_offset + ids_size, 4);
        size_t indexes_size = sizeof(uint32_t) * prop_count;

        size_t types_offset = DM_ALIGN(indexes_offset + indexes_size, 4);
        size_t types_size = sizeof(PropertyContainerType) * prop_count;

        size_t hashes_offset = DM_ALIGN(types_offset + types_size, 8);
        size_t hashes_size = sizeof(uint64_t) * params.m_HashCount;

        size_t floats_offset = DM_ALIGN(hashes_offset + hashes_size, 4);
        size_t floats_size = sizeof(float) * (params.m_NumberCount + params.m_Vector3Count * 3 + params.m_Vector4Count * 4 + params.m_QuatCount * 4);

        size_t urls_offset = DM_ALIGN(floats_offset + floats_size, 8);
        size_t urls_size = params.m_URLCount * sizeof(dmMessage::URL);

        size_t strings_offset = urls_offset + urls_size;
        size_t strings_size = params.m_URLStringSize + params.m_BoolCount;

        size_t property_container_size = strings_offset + strings_size;

        void* mem;
        if (dmMemory::RESULT_OK != dmMemory::AlignedMalloc(&mem, 8, property_container_size))
        {
            return 0x0;
        }
        uint8_t* p = (uint8_t*)(mem);

        PropertyContainer* result = (PropertyContainer*)&p[struct_offset];

        result->m_Count = prop_count;
        result->m_Ids = (dmhash_t*)&p[ids_offset];
        result->m_Indexes = (uint32_t*)&p[indexes_offset];
        result->m_Types = (PropertyContainerType*)&p[types_offset];
        result->m_HashData = (dmhash_t*)&p[hashes_offset];
        result->m_FloatData = (float*)&p[floats_offset];
        result->m_URLData = (char*)&p[urls_offset];
        result->m_StringData = (char*)&p[strings_offset];

        return result;
    }

    HPropertyContainerBuilder CreatePropertyContainerBuilder(const PropertyContainerParameters& params)
    {
        PropertyContainer* container = AllocatePropertyContainer(params);
        if (container == 0x0)
        {
            return 0x0;
        }
        HPropertyContainerBuilder builder = new PropertyContainerBuilder(container);
        if (builder == 0x0)
        {
            DestroyPropertyContainer(container);
            return 0x0;
        }
        return builder;
    }


    static uint32_t AllocateEntry(HPropertyContainerBuilder builder, dmhash_t id, PropertyContainerType type)
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset++;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Types[entry_offset] = type;
        return entry_offset;
    }

    void PushFloatType(HPropertyContainerBuilder builder, dmhash_t id, PropertyType type, const float values[])
    {
        uint32_t entry_offset;
        uint32_t count;
        switch (type)
        {
            case PROPERTY_TYPE_NUMBER:
                entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_NUMBER);
                count = 1;
                break;
            case PROPERTY_TYPE_VECTOR3:
                entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_VECTOR3);
                count = 3;
                break;
            case PROPERTY_TYPE_VECTOR4:
                entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_VECTOR4);
                count = 4;
                break;
            case PROPERTY_TYPE_QUAT:
                entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_QUAT);
                count = 4;
                break;
            default:
                assert(false);
                return;
        }
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        for (uint32_t i = 0; i < count; ++i)
        {
            builder->m_PropertyContainer->m_FloatData[float_offset + i] = values[i];
        }
        builder->m_FloatOffset += count;
    }

    void PushBool(HPropertyContainerBuilder builder, dmhash_t id, bool value)
    {
        uint32_t entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_BOOLEAN);
        uint32_t bool_offset = builder->m_StringOffset;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = bool_offset;
        builder->m_PropertyContainer->m_StringData[bool_offset] = value ? 1 : 0;
        builder->m_StringOffset += 1;
    }

    void PushHash(HPropertyContainerBuilder builder, dmhash_t id, dmhash_t value)
    {
        uint32_t entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_HASH);
        uint32_t hash_offset = builder->m_HashOffset;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = hash_offset;
        builder->m_PropertyContainer->m_HashData[hash_offset] = value;
        builder->m_HashOffset += 1;
    }

    void PushURLString(HPropertyContainerBuilder builder, dmhash_t id, const char* value)
    {
        uint32_t entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_URL_STRING);
        uint32_t string_offset = builder->m_StringOffset;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = string_offset;
        size_t string_length = strlen(value) + 1;
        memcpy(&builder->m_PropertyContainer->m_StringData[string_offset], value, string_length);
        builder->m_StringOffset += string_length;
    }

    void PushURL(HPropertyContainerBuilder builder, dmhash_t id, const char value[sizeof(dmMessage::URL)])
    {
        uint32_t entry_offset = AllocateEntry(builder, id, PROPERTY_CONTAINER_TYPE_URL);
        uint32_t url_offset = builder->m_URLOffset;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = url_offset;
        memcpy(&builder->m_PropertyContainer->m_URLData[url_offset], value, sizeof(dmMessage::URL));
        builder->m_URLOffset += sizeof(dmMessage::URL);
    }

    HPropertyContainer CreatePropertyContainer(HPropertyContainerBuilder builder)
    {
        HPropertyContainer result = builder->m_PropertyContainer;
        delete builder;
        return result;
    }

    static uint32_t INVALID_ENTRY_INDEX = 0xffffffffu;

    static uint32_t FindId(HPropertyContainer container, dmhash_t id)
    {
        for (uint32_t i = 0; i < container->m_Count; ++i)
        {
            if (container->m_Ids[i] == id)
            {
                return i;
            }
        }
        return INVALID_ENTRY_INDEX;
    }

    static void CountEntry(PropertyContainerParameters& params, HPropertyContainer container, uint32_t i)
    {
        switch(container->m_Types[i])
        {
            case PROPERTY_CONTAINER_TYPE_NUMBER:
                ++params.m_NumberCount;
                break;
            case PROPERTY_CONTAINER_TYPE_HASH:
                ++params.m_HashCount;
                break;
            case PROPERTY_CONTAINER_TYPE_URL:
                ++params.m_URLCount;
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR3:
                ++params.m_Vector3Count;
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR4:
                ++params.m_Vector4Count;
                break;
            case PROPERTY_CONTAINER_TYPE_QUAT:
                ++params.m_QuatCount;
                break;
            case PROPERTY_CONTAINER_TYPE_BOOLEAN:
                ++params.m_BoolCount;
                break;
            case PROPERTY_CONTAINER_TYPE_URL_STRING:
                params.m_URLStringSize += strlen(&container->m_StringData[container->m_Indexes[i]]) + 1;
                ++params.m_URLStringCount;
                break;
            default:
                assert(false);
                break;
        }
    }

    static void PushEntry(HPropertyContainerBuilder builder, HPropertyContainer container, uint32_t i)
    {
        switch(container->m_Types[i])
        {
            case PROPERTY_CONTAINER_TYPE_NUMBER:
                PushFloatType(builder, container->m_Ids[i], PROPERTY_TYPE_NUMBER, &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_HASH:
                PushHash(builder, container->m_Ids[i], container->m_HashData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_URL:
                PushURL(builder, container->m_Ids[i], &container->m_URLData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR3:
                PushFloatType(builder, container->m_Ids[i],PROPERTY_TYPE_VECTOR3, &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR4:
                PushFloatType(builder, container->m_Ids[i], PROPERTY_TYPE_VECTOR4, &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_QUAT:
                PushFloatType(builder, container->m_Ids[i], PROPERTY_TYPE_QUAT, &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_BOOLEAN:
                PushBool(builder, container->m_Ids[i], container->m_StringData[container->m_Indexes[i]] != 0);
                break;
            case PROPERTY_CONTAINER_TYPE_URL_STRING:
                PushURLString(builder, container->m_Ids[i], &container->m_StringData[container->m_Indexes[i]]);
                break;
            default:
                assert(false);
                break;
        }
    }

    HPropertyContainer MergePropertyContainers(HPropertyContainer container, HPropertyContainer overrides)
    {
        PropertyContainerParameters params;
        for (uint32_t i = 0; i < overrides->m_Count; ++i)
        {
            CountEntry(params, overrides, i);
        }

        for (uint32_t i = 0; i < container->m_Count; ++i)
        {
            if (FindId(overrides, container->m_Ids[i]) != INVALID_ENTRY_INDEX)
            {
                continue;
            }
            CountEntry(params, container, i);
        }

        HPropertyContainerBuilder builder = CreatePropertyContainerBuilder(params);

        for (uint32_t i = 0; i < overrides->m_Count; ++i)
        {
            PushEntry(builder, overrides, i);
        }

        for (uint32_t i = 0; i < container->m_Count; ++i)
        {
            if (FindId(overrides, container->m_Ids[i]) != INVALID_ENTRY_INDEX)
            {
                continue;
            }
            PushEntry(builder, container, i);
        }

        return CreatePropertyContainer(builder);
    }

    static bool ResolveURL(Properties* properties, const char* url, dmMessage::URL* out_url)
    {
        dmMessage::URL default_url;
        properties->m_GetURLCallback((lua_State*)properties->m_ResolvePathUserData, &default_url);
        dmMessage::Result result = dmScript::ResolveURL((lua_State*)properties->m_ResolvePathUserData, url, out_url, &default_url);
        if (result != dmMessage::RESULT_OK)
        {
            return false;
        }
        return true;
    }

    PropertyResult PropertyContainerGetPropertyCallback(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var)
    {
        if (user_data == 0)
        {
            return PROPERTY_RESULT_NOT_FOUND;
        }
        PropertyContainer* property_container = (PropertyContainer*)user_data;
        uint32_t i = FindId(property_container, id);
        if (i == INVALID_ENTRY_INDEX)
        {
            return PROPERTY_RESULT_NOT_FOUND;
        }
        uint32_t index = property_container->m_Indexes[i];
        switch(property_container->m_Types[i])
        {
            case PROPERTY_CONTAINER_TYPE_NUMBER:
                out_var.m_Number = property_container->m_FloatData[index];
                out_var.m_Type = PROPERTY_TYPE_NUMBER;
                break;
            case PROPERTY_CONTAINER_TYPE_HASH:
                out_var.m_Hash = property_container->m_HashData[index];
                out_var.m_Type = PROPERTY_TYPE_HASH;
                break;
            case PROPERTY_CONTAINER_TYPE_URL_STRING:
                if (!ResolveURL(properties, &property_container->m_StringData[index], (dmMessage::URL*) out_var.m_URL))
                {
                    return PROPERTY_RESULT_INVALID_FORMAT;
                }
                out_var.m_Type = PROPERTY_TYPE_URL;
                break;
            case PROPERTY_CONTAINER_TYPE_URL:
                memcpy(&out_var.m_URL, &property_container->m_URLData[index], sizeof(dmMessage::URL));
                out_var.m_Type = PROPERTY_TYPE_URL;
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR3:
                out_var.m_V4[0] = property_container->m_FloatData[index+0];
                out_var.m_V4[1] = property_container->m_FloatData[index+1];
                out_var.m_V4[2] = property_container->m_FloatData[index+2];
                out_var.m_Type = PROPERTY_TYPE_VECTOR3;
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR4:
                out_var.m_V4[0] = property_container->m_FloatData[index+0];
                out_var.m_V4[1] = property_container->m_FloatData[index+1];
                out_var.m_V4[2] = property_container->m_FloatData[index+2];
                out_var.m_V4[3] = property_container->m_FloatData[index+3];
                out_var.m_Type = PROPERTY_TYPE_VECTOR4;
                break;
            case PROPERTY_CONTAINER_TYPE_QUAT:
                out_var.m_V4[0] = property_container->m_FloatData[index+0];
                out_var.m_V4[1] = property_container->m_FloatData[index+1];
                out_var.m_V4[2] = property_container->m_FloatData[index+2];
                out_var.m_V4[3] = property_container->m_FloatData[index+3];
                out_var.m_Type = PROPERTY_TYPE_QUAT;
                break;
            case PROPERTY_CONTAINER_TYPE_BOOLEAN:
                out_var.m_Bool = property_container->m_StringData[index] != 0;
                out_var.m_Type = PROPERTY_TYPE_BOOLEAN;
                break;
            default:
                assert(false);
                break;
        }
        return PROPERTY_RESULT_OK;
    }

    void DestroyPropertyContainer(HPropertyContainer container)
    {
        dmMemory::AlignedFree(container);
    }

    void DestroyPropertyContainerCallback(uintptr_t user_data)
    {
        if (user_data == 0)
        {
            return;
        }
        DestroyPropertyContainer((HPropertyContainer)user_data);
    }


    dmResource::Result LoadPropertyResources(dmResource::HFactory factory, const char** resource_paths, uint32_t resource_path_count, dmArray<void*>& out_resources)
    {
        assert(out_resources.Size() == 0);
        out_resources.SetCapacity(resource_path_count);
        for(uint32_t i = 0; i < resource_path_count; ++i)
        {
            void* resource;
            dmResource::Result res = dmResource::Get(factory, resource_paths[i], &resource);
            if(res != dmResource::RESULT_OK)
            {
                dmLogError("Could not load property resource '%s' (%d)", resource_paths[i], res);
                UnloadPropertyResources(factory, out_resources);
                return res;
            }
            out_resources.Push(resource);
        }
        return dmResource::RESULT_OK;
    }

    void UnloadPropertyResources(dmResource::HFactory factory, dmArray<void*>& resources)
    {
        for(uint32_t i = 0; i < resources.Size(); ++i)
        {
            dmResource::Release(factory, resources[i]);
        }
        resources.SetSize(0);
        resources.SetCapacity(0);
    }
}
