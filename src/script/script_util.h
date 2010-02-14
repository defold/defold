#ifndef LUASUPPORT_H
#define LUASUPPORT_H

#include <stdint.h>
#include <ddf/ddf.h>
extern "C"
{
#include "../lua/lua.h"
}

namespace dmScriptUtil
{
    /**
     * Load DDF structure from Lua stack
     * @param L Lua state
     * @param descriptor DDF descriptor
     * @param buffer Buffer
     * @param buffer_size Buffer size
     */
    void LuaTableToDDF(lua_State* L, const dmDDF::Descriptor* descriptor, char* buffer, uint32_t buffer_size);

    /**
     * Push DDF value to Lua stack
     * @param L Lua state
     * @param field_desc Field descriptor
     * @param data DDF dat
     */
    void DDFToLuaValue(lua_State* L, const dmDDF::FieldDescriptor* field_desc, const char* data);

    /**
     * Push DDF message to Lua stack
     * @param L Lua state
     * @param descriptor Field descriptor
     * @param data DDF data
     */
    void DDFToLuaTable(lua_State*L, const dmDDF::Descriptor* descriptor, const char* data);

}

#endif
