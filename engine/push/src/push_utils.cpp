#include "push_utils.h"

// NOTE: Copy-paste from script_json
int JsonToLua(lua_State*L, dmJson::Document* doc, int index)
{
    const dmJson::Node& n = doc->m_Nodes[index];
    const char* json = doc->m_Json;
    int l = n.m_End - n.m_Start;
    switch (n.m_Type)
    {
    case dmJson::TYPE_PRIMITIVE:
        if (l == 4 && memcmp(json + n.m_Start, "null", 4) == 0) {
            lua_pushnil(L);
        } else if (l == 4 && memcmp(json + n.m_Start, "true", 4) == 0) {
            lua_pushboolean(L, 1);
        } else if (l == 5 && memcmp(json + n.m_Start, "false", 5) == 0) {
            lua_pushboolean(L, 0);
        } else {
            double val = atof(json + n.m_Start);
            lua_pushnumber(L, val);
        }
        return index + 1;

    case dmJson::TYPE_STRING:
        lua_pushlstring(L, json + n.m_Start, l);
        return index + 1;

    case dmJson::TYPE_ARRAY:
        lua_createtable(L, n.m_Size, 0);
        ++index;
        for (int i = 0; i < n.m_Size; ++i) {
            index = JsonToLua(L, doc, index);
            lua_rawseti(L, -2, i+1);
        }
        return index;

    case dmJson::TYPE_OBJECT:
        lua_createtable(L, 0, n.m_Size);
        ++index;
        for (int i = 0; i < n.m_Size; i += 2) {
            index = JsonToLua(L, doc, index);
            index = JsonToLua(L, doc, index);
            lua_rawset(L, -3);
        }

        return index;
    }

    assert(false && "not reached");
    return index;
}