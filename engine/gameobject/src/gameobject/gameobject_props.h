#ifndef GAMEOBJECT_PROPS_H
#define GAMEOBJECT_PROPS_H

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/message.h>
#include <ddf/ddf.h>
#include <script/script.h>

#include "gameobject.h"

#include "../proto/gameobject/gameobject_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmGameObject
{

    enum PropertyLayer
    {
        PROPERTY_LAYER_INSTANCE = 0,
        PROPERTY_LAYER_PROTOTYPE = 1,
        PROPERTY_LAYER_DEFAULT = 2,
        MAX_PROPERTY_LAYER_COUNT
    };

    struct Properties
    {
        Properties();

        PropertySet m_Set[MAX_PROPERTY_LAYER_COUNT];
        dmScript::ResolvePathCallback m_ResolvePathCallback;
        uintptr_t m_ResolvePathUserData;
        dmScript::GetURLCallback m_GetURLCallback;
    };

    struct NewPropertiesParams
    {
        NewPropertiesParams();

        dmScript::ResolvePathCallback m_ResolvePathCallback;
        uintptr_t m_ResolvePathUserData;
        dmScript::GetURLCallback m_GetURLCallback;
    };

    HProperties NewProperties(const NewPropertiesParams& params);
    void DeleteProperties(HProperties properties);

    void SetPropertySet(HProperties properties, PropertyLayer layer, const PropertySet& set);

    PropertyResult GetProperty(const HProperties properties, dmhash_t id, PropertyVar& var);

    typedef struct PropertyContainer* HPropertyContainer;
    typedef struct PropertyContainerBuilder* HPropertyContainerBuilder;

    struct PropertyContainerParameters
    {
        PropertyContainerParameters()
            : m_NumberCount(0)
            , m_HashCount(0)
            , m_URLStringCount(0)
            , m_URLStringSize(0)
            , m_URLCount(0)
            , m_Vector3Count(0)
            , m_Vector4Count(0)
            , m_QuatCount(0)
            , m_BoolCount(0)
        { }
        uint32_t m_NumberCount;
        uint32_t m_HashCount;
        uint32_t m_URLStringCount;
        uint32_t m_URLStringSize;
        uint32_t m_URLCount;
        uint32_t m_Vector3Count;
        uint32_t m_Vector4Count;
        uint32_t m_QuatCount;
        uint32_t m_BoolCount;
    };

    HPropertyContainerBuilder CreatePropertyContainerBuilder(const PropertyContainerParameters& params);
    void PushNumber(HPropertyContainerBuilder builder, dmhash_t id, float value);
    void PushVector3(HPropertyContainerBuilder builder, dmhash_t id, const float values[3]);
    void PushVector4(HPropertyContainerBuilder builder, dmhash_t id, const float values[4]);
    void PushQuat(HPropertyContainerBuilder builder, dmhash_t id, const float values[4]);
    void PushBool(HPropertyContainerBuilder builder, dmhash_t id, float value);
    void PushHash(HPropertyContainerBuilder builder, dmhash_t id, dmhash_t value);
    void PushURLString(HPropertyContainerBuilder builder, dmhash_t id, const char* value);
    void PushURL(HPropertyContainerBuilder builder, dmhash_t id, const char value[sizeof(dmMessage::URL)]);
    HPropertyContainer CreatePropertyContainer(HPropertyContainerBuilder builder);

    HPropertyContainer MergePropertyContainers(HPropertyContainer container, HPropertyContainer overrides);

    PropertyResult PropertyContainerGetPropertyCallback(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
    void DestroyPropertyContainer(HPropertyContainer container);
    void DestroyPropertyContainerCallback(uintptr_t user_data);
}

#endif // GAMEOBJECT_PROPS_H
