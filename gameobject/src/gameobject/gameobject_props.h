#ifndef GAMEOBJECT_PROPS_H
#define GAMEOBJECT_PROPS_H

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/message.h>
#include <ddf/ddf.h>

#include "gameobject.h"

#include "../proto/gameobject_ddf.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmGameObject
{
    struct PropertyDef
    {
        const char* m_Name;
        dmhash_t m_Id;
        dmGameObjectDDF::PropertyType m_Type;
        union
        {
            double m_Number;
            dmhash_t m_Hash;
            dmMessage::URL m_URL;
        };
    };

    struct Properties
    {
        Properties();

        uint8_t* m_Buffer;
        uint32_t m_BufferSize;
        Properties* m_Next;
    };

    HProperties NewProperties();
    void DeleteProperties(HProperties properties);

    bool GetProperty(HProperties properties, dmhash_t id, double& value);
    bool GetProperty(HProperties properties, dmhash_t id, dmhash_t& value);
    bool GetProperty(HProperties properties, dmhash_t id, dmMessage::URL& value);

    void SetProperties(HProperties properties, uint8_t* buffer, uint32_t buffer_size);
    uint32_t SerializeProperties(const dmGameObjectDDF::PropertyDesc* properties, uint32_t count, uint8_t* out_buffer, uint32_t out_buffer_size);
    uint32_t SerializeProperties(const dmArray<PropertyDef>& properties, uint8_t* out_buffer, uint32_t out_buffer_size);

    void AppendProperties(HProperties properties, HProperties next);

    void PropertiesToLuaTable(const dmArray<PropertyDef>& property_defs, const Properties* properties, lua_State* L, int index);
    void ClearPropertiesFromLuaTable(const dmArray<PropertyDef>& property_defs, const Properties* properties, lua_State* L, int index);
}

#endif // GAMEOBJECT_PROPS_H
