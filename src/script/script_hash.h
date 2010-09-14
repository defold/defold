#ifndef SCRIPT_HASH_H
#define SCRIPT_HASH_H

#include <stdint.h>

#include <dlib/hash.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    typedef uint32_t Hash;

    void RegisterHashLib(lua_State* L);

    void PushHash(lua_State* L, const char* s);
    Hash CheckHash(lua_State* L, int index);
}

#endif // SCRIPT_HASH_H
