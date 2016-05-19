#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "components/comp_collision_object.h"

#include "script_mesh.h"
#include "../resources/res_mesh.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    struct MeshScriptContext
    {
    	dmResource::HFactory m_Factory;
    };


#define MESH_CONTEXT_NAME "__mesh_context"

    int Mesh_Set(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* mesh_name = luaL_checkstring(L, 1);
        size_t buffer_size;
        luaL_checktype(L, 2, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 2, &buffer_size);

        lua_getglobal(L, MESH_CONTEXT_NAME);
        MeshScriptContext* context = (MeshScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        Mesh* mesh = 0;
        dmResource::Result r = dmResource::Get(context->m_Factory, mesh_name, (void**) &mesh);
        if (r != dmResource::RESULT_OK) {
        	luaL_error(L, "Failed to find mesh: %s", mesh_name);
        }

        dmGraphics::SetVertexBufferData(mesh->m_VertexBuffer, buffer_size, buffer, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        mesh->m_VertexCount = buffer_size / sizeof(MeshVertex);

        dmResource::Release(context->m_Factory, (void*) mesh);

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg MESH_FUNCTIONS[] =
    {
        {"set", Mesh_Set},
        {0, 0}
    };

    void ScriptMeshRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "mesh", MESH_FUNCTIONS);
        lua_pop(L, 1);

        bool result = true;

        MeshScriptContext* mesh_context = new MeshScriptContext();
        mesh_context->m_Factory = context.m_Factory;
        lua_pushlightuserdata(L, mesh_context);
        lua_setglobal(L, MESH_CONTEXT_NAME);
    }

    void ScriptMeshFinalize(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        if (L != 0x0)
        {
            int top = lua_gettop(L);
            (void)top;

            lua_getglobal(L, MESH_CONTEXT_NAME);
            MeshScriptContext* mesh_context = (MeshScriptContext*)lua_touserdata(L, -1);
            lua_pop(L, 1);
            if (mesh_context != 0x0)
            {
                delete mesh_context;
            }

            assert(top == lua_gettop(L));
        }
    }

}

#undef MESH_CONTEXT_NAME
#undef COLLISION_OBJECT_EXT
