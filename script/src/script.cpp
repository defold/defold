#include "script.h"

#include <dlib/hashtable.h>
#include <dlib/log.h>

#include "script_private.h"
#include "script_hash.h"
#include "script_msg.h"
#include "script_vmath.h"
#include "script_sys.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    ScriptParams::ScriptParams()
    : m_SetURLsCallback(0x0)
    {

    }

    void Initialize(lua_State* L, const ScriptParams& params)
    {
        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);

        lua_pushlightuserdata(L, (void*)params.m_SetURLsCallback);
        lua_setglobal(L, SCRIPT_GET_URLS_CALLBACK);

        dmHashTable64<const dmDDF::Descriptor*>* descriptors = new dmHashTable64<const dmDDF::Descriptor*>();
        lua_pushlightuserdata(L, (void*)descriptors);
        descriptors->SetCapacity(17, 128);
        lua_setglobal(L, SCRIPT_DDF_TYPES);
    }

    void Finalize(lua_State* L)
    {
        lua_getglobal(L, SCRIPT_DDF_TYPES);
        delete (dmHashTable64<const dmDDF::Descriptor*>*)lua_touserdata(L, -1);
        lua_pop(L, 1);
    }

    bool RegisterDDFType(lua_State* L, const dmDDF::Descriptor* descriptor)
    {
        lua_getglobal(L, SCRIPT_DDF_TYPES);
        dmHashTable64<const dmDDF::Descriptor*>* descriptors = (dmHashTable64<const dmDDF::Descriptor*>*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (descriptors->Full())
        {
            dmLogError("Unable to register ddf type. Out of resources.");
            return false;
        }
        else
        {
            descriptors->Put(dmHashString64(descriptor->m_Name), descriptor);
            return true;
        }
    }
}
