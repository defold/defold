#include "gameobject_props.h"

#include <string.h>
#include <stdlib.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
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

        size_t struct_size = sizeof(PropertyContainer);
        size_t ids_size = DM_ALIGN(sizeof(dmhash_t) * prop_count, 4);
        size_t indexes_size = DM_ALIGN(sizeof(uint32_t) * prop_count, 4);
        size_t types_size = DM_ALIGN(sizeof(PropertyContainerType) * prop_count, 4);
        size_t hashes_size = DM_ALIGN(sizeof(uint64_t) * params.m_HashCount, 4);
        size_t floats_size = DM_ALIGN(sizeof(float) * (params.m_NumberCount + params.m_Vector3Count * 3 + params.m_Vector4Count * 4 + params.m_QuatCount * 4 + params.m_BoolCount), 4);
        size_t url_size = DM_ALIGN(params.m_URLCount * sizeof(dmMessage::URL), 1);
        size_t strings_size = params.m_URLStringSize;

        size_t property_container_size = struct_size + 
            ids_size +
            indexes_size +
            types_size +
            hashes_size +
            floats_size +
            url_size +
            strings_size;

        void* mem = malloc(property_container_size);
        if (mem == 0x0)
        {
            return 0x0;
        }
        uint8_t* p = (uint8_t*)(mem);

        PropertyContainer* result = (PropertyContainer*)mem;

        result->m_Count = prop_count;
        p += struct_size;

        result->m_Ids = (dmhash_t*)(p);
        p += ids_size;

        result->m_Indexes = (uint32_t*)(p);
        p += indexes_size;

        result->m_Types = (PropertyContainerType*)(p);
        p += types_size;

        result->m_HashData = (dmhash_t*)(p);
        p += hashes_size;

        result->m_URLData = (char*)(p);
        p += url_size;

        result->m_FloatData = (float*)(p);
        p += floats_size;

        result->m_StringData = (char*)(p);
        p += strings_size;

        assert(p == &((uint8_t*)(mem))[property_container_size]);

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
            free(container);
            return 0x0;
        }
        return builder;
    }

    void PushNumber(HPropertyContainerBuilder builder, dmhash_t id, float value)
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_NUMBER;
        builder->m_PropertyContainer->m_FloatData[float_offset] = value;
        builder->m_FloatOffset += 1;
        ++builder->m_EntryOffset;
    }

    void PushVector3(HPropertyContainerBuilder builder, dmhash_t id, const float values[3])
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_VECTOR3;
        builder->m_PropertyContainer->m_FloatData[float_offset + 0] = values[0];
        builder->m_PropertyContainer->m_FloatData[float_offset + 1] = values[1];
        builder->m_PropertyContainer->m_FloatData[float_offset + 2] = values[2];
        builder->m_FloatOffset += 3;
        ++builder->m_EntryOffset;
    }

    void PushVector4(HPropertyContainerBuilder builder, dmhash_t id, const float values[4])
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_VECTOR4;
        builder->m_PropertyContainer->m_FloatData[float_offset + 0] = values[0];
        builder->m_PropertyContainer->m_FloatData[float_offset + 1] = values[1];
        builder->m_PropertyContainer->m_FloatData[float_offset + 2] = values[2];
        builder->m_PropertyContainer->m_FloatData[float_offset + 3] = values[3];
        builder->m_FloatOffset += 4;
        ++builder->m_EntryOffset;
    }

    void PushQuat(HPropertyContainerBuilder builder, dmhash_t id, const float values[4])
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_QUAT;
        builder->m_PropertyContainer->m_FloatData[float_offset + 0] = values[0];
        builder->m_PropertyContainer->m_FloatData[float_offset + 1] = values[1];
        builder->m_PropertyContainer->m_FloatData[float_offset + 2] = values[2];
        builder->m_PropertyContainer->m_FloatData[float_offset + 3] = values[3];
        builder->m_FloatOffset += 4;
        ++builder->m_EntryOffset;
    }

    void PushBool(HPropertyContainerBuilder builder, dmhash_t id, float value)
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t float_offset = builder->m_FloatOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = float_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_BOOLEAN;
        builder->m_PropertyContainer->m_FloatData[float_offset] = value;
        builder->m_FloatOffset += 1;
        ++builder->m_EntryOffset;
    }

    void PushHash(HPropertyContainerBuilder builder, dmhash_t id, dmhash_t value)
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t hash_offset = builder->m_HashOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = hash_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_HASH;
        builder->m_PropertyContainer->m_HashData[hash_offset] = value;
        builder->m_HashOffset += 1;
        ++builder->m_EntryOffset;
    }

    void PushURLString(HPropertyContainerBuilder builder, dmhash_t id, const char* value)
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t string_offset = builder->m_StringOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = string_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_URL_STRING;
        size_t string_length = strlen(value) + 1;
        memcpy(&builder->m_PropertyContainer->m_StringData[string_offset], value, string_length);
        builder->m_StringOffset += string_length;
        ++builder->m_EntryOffset;
    }

    void PushURL(HPropertyContainerBuilder builder, dmhash_t id, const char value[sizeof(dmMessage::URL)])
    {
        assert(builder->m_EntryOffset < builder->m_PropertyContainer->m_Count);
        uint32_t entry_offset = builder->m_EntryOffset;
        uint32_t url_offset = builder->m_URLOffset;
        builder->m_PropertyContainer->m_Ids[entry_offset] = id;
        builder->m_PropertyContainer->m_Indexes[entry_offset] = url_offset;
        builder->m_PropertyContainer->m_Types[entry_offset] = PROPERTY_CONTAINER_TYPE_URL;
        memcpy(&builder->m_PropertyContainer->m_URLData[url_offset], value, sizeof(dmMessage::URL));
        builder->m_URLOffset += sizeof(dmMessage::URL);
        ++builder->m_EntryOffset;
    }

    HPropertyContainer CreatePropertyContainer(HPropertyContainerBuilder builder)
    {
        HPropertyContainer result = builder->m_PropertyContainer;
        delete builder;
        return result;
    }

    static bool HasId(HPropertyContainer container, dmhash_t id)
    {
        for (uint32_t i = 0; i < container->m_Count; ++i)
        {
            if (container->m_Ids[i] == id)
            {
                return true;
            }
        }
        return false;
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
                PushNumber(builder, container->m_Ids[i], container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_HASH:
                PushHash(builder, container->m_Ids[i], container->m_HashData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_URL:
                PushURL(builder, container->m_Ids[i], &container->m_URLData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR3:
                PushVector3(builder, container->m_Ids[i], &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_VECTOR4:
                PushVector4(builder, container->m_Ids[i], &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_QUAT:
                PushQuat(builder, container->m_Ids[i], &container->m_FloatData[container->m_Indexes[i]]);
                break;
            case PROPERTY_CONTAINER_TYPE_BOOLEAN:
                PushBool(builder, container->m_Ids[i], container->m_FloatData[container->m_Indexes[i]]);
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
            if (HasId(overrides, container->m_Ids[i]))
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
            if (HasId(overrides, container->m_Ids[i]))
            {
                continue;
            }
            PushEntry(builder, container, i);
        }

        DestroyPropertyContainer(container);
        DestroyPropertyContainer(overrides);

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
        for (uint32_t i = 0; i < property_container->m_Count; ++i)
        {
            if (property_container->m_Ids[i] == id)
            {
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
                        out_var.m_Bool = property_container->m_FloatData[index] != 0.f;
                        out_var.m_Type = PROPERTY_TYPE_BOOLEAN;
                        break;
                    default:
                        assert(false);
                        break;
                }
                return PROPERTY_RESULT_OK;
            }
        }
        return PROPERTY_RESULT_NOT_FOUND;
    }

    void DestroyPropertyContainer(HPropertyContainer container)
    {
        free(container);
    }

    void DestroyPropertyContainerCallback(uintptr_t user_data)
    {
        if (user_data == 0)
        {
            return;
        }
        DestroyPropertyContainer((HPropertyContainer)user_data);
    }

}
