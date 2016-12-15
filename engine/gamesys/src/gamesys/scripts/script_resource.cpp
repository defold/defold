#include "script_resource.h"
#include <dlib/buffer.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <resource/resource.h>
#include <graphics/graphics_ddf.h>
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


static int ReportPathError(lua_State* L, dmResource::Result result, dmhash_t path_hash)
{
    char msg[256];
    const char* reverse = (const char*) dmHashReverse64(path_hash, 0);
    const char* format = 0;
    switch(result)
    {
    case dmResource::RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found: %llu, %s"; break;
    case dmResource::RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation: %llu, %s"; break;
    default:                                    format = "The resource was not updated: %llu, %s"; break;
    }
    DM_SNPRINTF(msg, sizeof(msg), format, path_hash, reverse != 0 ? reverse : "<no hash available>");
    return luaL_error(L, msg);
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
    int top = lua_gettop(L);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    dmBuffer::HBuffer* buffer = dmScript::CheckBuffer(L, 2);

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, *buffer);
    if( r != dmResource::RESULT_OK )
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, r, path_hash);
    }
    assert(top == lua_gettop(L));
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
    int top = lua_gettop(L);
    const char* name = luaL_checkstring(L, 1);

    void* resource = 0;
    uint32_t resourcesize = 0;
    dmResource::Result r = dmResource::GetRaw(g_ResourceModule.m_Factory, name, &resource, &resourcesize);

    if( r != dmResource::RESULT_OK )
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, r, dmHashString64(name));
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
    assert(top + 1 == lua_gettop(L));
    return 1;
}

static int CheckTableNumber(lua_State* L, int index, const char* name)
{
    int result = -1;
    lua_pushstring(L, name);
    lua_gettable(L, index);
    if (lua_isnumber(L, -1)) {
        result = lua_tointeger(L, -1);
    } else {
        char msg[256];
        DM_SNPRINTF(msg, sizeof(msg), "Wrong type for table attribute '%s'. Expected number, got %s", name, luaL_typename(L, -1) );
        return luaL_error(L, msg);
    }
    lua_pop(L, 1);
    return result;
}

static int GraphicsTextureFormatToImageFormat(int textureformat)
{
    switch(textureformat)
    {
        case dmGraphics::TEXTURE_FORMAT_LUMINANCE:          return dmGraphics::TextureImage::TEXTURE_FORMAT_LUMINANCE;
        case dmGraphics::TEXTURE_FORMAT_RGB:                return dmGraphics::TextureImage::TEXTURE_FORMAT_RGB;
        case dmGraphics::TEXTURE_FORMAT_RGBA:               return dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA;
        case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:   return dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
        case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   return dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
        case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:  return dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
        case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  return dmGraphics::TextureImage::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        case dmGraphics::TEXTURE_FORMAT_RGB_ETC1:           return dmGraphics::TextureImage::TEXTURE_FORMAT_RGB_ETC1;
    };
    assert(false);
    return -1;
}

static int GraphicsTextureTypeToImageType(int texturetype)
{
    switch(texturetype)
    {
        case dmGraphics::TEXTURE_TYPE_2D:          return dmGraphics::TextureImage::TYPE_2D;
        case dmGraphics::TEXTURE_TYPE_CUBE_MAP:    return dmGraphics::TextureImage::TYPE_CUBEMAP;
    };
    assert(false);
    return -1;
}

static int SetTexture(lua_State* L)
{
    int top = lua_gettop(L);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t type = (uint32_t)CheckTableNumber(L, 2, "type");
    uint32_t width = (uint32_t)CheckTableNumber(L, 2, "width");
    uint32_t height = (uint32_t)CheckTableNumber(L, 2, "height");
    uint32_t format = (uint32_t)CheckTableNumber(L, 2, "format");

    uint32_t num_mip_maps = 1;

    dmBuffer::HBuffer* buffer = dmScript::CheckBuffer(L, 3);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(*buffer, (void**)&data, &datasize);

    dmGraphics::TextureImage* texture_image = new dmGraphics::TextureImage;
    texture_image->m_Alternatives.m_Data = new dmGraphics::TextureImage::Image[1];
    texture_image->m_Alternatives.m_Count = 1;
    texture_image->m_Type = (dmGraphics::TextureImage::Type)GraphicsTextureTypeToImageType(type);
    
    for (uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
    {
        dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
        image->m_Width = width;
        image->m_Height = height;
        image->m_OriginalWidth = width;
        image->m_OriginalHeight = height;
        image->m_Format = (dmGraphics::TextureImage::TextureFormat)GraphicsTextureFormatToImageFormat(format);
        image->m_CompressionType = dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT;
        image->m_CompressionFlags = 0;
        image->m_Data.m_Data = data;
        image->m_Data.m_Count = datasize;
        image->m_MipMapOffset.m_Data = new uint32_t[num_mip_maps];
        image->m_MipMapOffset.m_Count = num_mip_maps;

        image->m_MipMapSize.m_Data = new uint32_t[num_mip_maps];
        image->m_MipMapSize.m_Count = num_mip_maps;

        for( uint32_t mip = 0; mip < num_mip_maps; ++mip )
        {
            image->m_MipMapOffset[mip] = 0; // TODO: Fix mip offsets
            image->m_MipMapSize[mip] = datasize;
        }
    }

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, (void*)texture_image);

    // cleanup memory

    for (uint32_t i = 0; i < texture_image->m_Alternatives.m_Count; ++i)
    {
        dmGraphics::TextureImage::Image* image = &texture_image->m_Alternatives[i];
        delete[] image->m_MipMapSize.m_Data;
        delete[] image->m_MipMapOffset.m_Data;
    }
    delete[] texture_image->m_Alternatives.m_Data;
    delete texture_image;

    if( r != dmResource::RESULT_OK )
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, r, path_hash);
    }

    assert(top == lua_gettop(L));
    return 0;
}


static const luaL_reg Module_methods[] =
{
    {"set", Set},
    {"load", Load},
    {"set_texture", SetTexture},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "resource", Module_methods);

#define SETGRAPHICSCONSTANT(name) \
    lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
    lua_setfield(L, -2, #name); \

    SETGRAPHICSCONSTANT(TEXTURE_TYPE_2D);
    SETGRAPHICSCONSTANT(TEXTURE_TYPE_CUBE_MAP);

    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_LUMINANCE);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGB);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_DEPTH);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_STENCIL);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGB_ETC1);
    /* DEF-994 We don't support these internally yet
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGB_DXT1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA_DXT1);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA_DXT3);
    SETGRAPHICSCONSTANT(TEXTURE_FORMAT_RGBA_DXT5);
    */

#undef SETGRAPHICSCONSTANT

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

} // namespace dmGameSystem
