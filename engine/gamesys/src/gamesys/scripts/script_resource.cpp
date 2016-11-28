#include "script_resource.h"
#include <dlib/buffer.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <resource/resource.h>
#include <script/script.h>
#include "../gamesys.h"

namespace dmGameSystem
{

/*# Resource API documentation
 *
 * Functions and constants to access the resources
 *
 * @name Resource
 * @namespace resource
 */



struct ResourceModule
{
    dmResource::HFactory m_Factory;
} g_ResourceModule;


static void ReportPathError(lua_State* L, dmResource::Result result, dmhash_t path_hash)
{
    char msg[256];
    const char* reverse = (const char*) dmHashReverse64(path_hash, 0);
    const char* format = 0;
    switch(result)
    {
    case dmResource::RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found: %s"; break;
    case dmResource::RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation: %s"; break;
    default:                                    format = "The resource was not updated: %s"; break;
    }
    DM_SNPRINTF(msg, sizeof(msg), format, reverse);
    luaL_error(L, msg);
}

/*# Set a resource
 * Sets the resource data for a specific resource
 *
 * @name resource.set
 *
 * @param path (hash|string) The path to the resource
 * @param buffer (buffer) The buffer of precreated data, suitable for the intended resource type
 *
 * @examples
 * <pre>
 * function update(self)
 *     -- copy the data from the texture of sprite1 to sprite2
 *     local buffer = resource.load(go.get("#sprite1", "texture0"))
 *     resource.set( go.get("#sprite2", "texture0"), buffer )
 * end
 * </pre>
 */
static int Set(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    dmBuffer::HBuffer* buffer = dmScript::CheckBuffer(L, 2);

    dmResource::Result r = dmResource::Set(g_ResourceModule.m_Factory, path_hash, *buffer);
    if( r != dmResource::RESULT_OK )
    {
        ReportPathError(L, r, path_hash);
    }
    return 0;
}

// Resource specific hooks
static int CreateTexture(lua_State* L)
{
    //uint64_t path_hash, struct dmBuffer::Buffer**
    dmLogWarning("resource.create_texture is not implemented yet!");

    return 0;
}

/*# Load a resource
 * Loads the resource data for a specific resource.
 *
 * @name resource.load
 *
 * @param path (hash|string) The path to the resource
 * @return (buffer) Returns the buffer stored on disc
 *
 * @examples
 * <pre>
 * function update(self)
 *     -- copy the data from the texture of sprite1 to sprite2
 *     local buffer = resource.load(go.get("#sprite1", "texture0"))
 *     resource.set( go.get("#sprite2", "texture0"), buffer )
 * end
 * </pre>
 *
 * In order for the engine to include custom resources in the build process, you need
 * to specify them in the "game.project" settings file:
 * <pre>
 * [project]
 * title = My project
 * version = 0.1
 * custom_resources = main/data/,assets/level_data.json
 * </pre>
 */
static int Load(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    void* resource = 0;
    uint32_t resourcesize = 0;
    dmResource::Result r = dmResource::GetRaw(g_ResourceModule.m_Factory, name, &resource, &resourcesize);

    if( r != dmResource::RESULT_OK )
    {
        ReportPathError(L, r, dmHashString64(name));
    }

    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    dmBuffer::HBuffer buffer = 0;
    dmBuffer::Allocate(resourcesize, streams_decl, 1, &buffer);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer, (void**)&data, &datasize);

    memcpy(data, resource, resourcesize);

    dmScript::PushBuffer(L, buffer);
    return 1;
}



static const luaL_reg Module_methods[] =
{
    {"set", Set},
    {"load", Load},
    {"create_texture", CreateTexture},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "resource", Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

void ScriptResourceRegister(const ScriptLibContext& context)
{
    LuaInit(context.m_LuaState);
    g_ResourceModule.m_Factory = context.m_Factory;
}

void ScriptResourceFinalize(const ScriptLibContext& context)
{
}

}
