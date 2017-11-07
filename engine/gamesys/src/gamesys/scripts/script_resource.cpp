#include "script_resource.h"
#include <dlib/buffer.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <resource/resource.h>
#include <graphics/graphics_ddf.h>
#include <script/script.h>
#include "../gamesys.h"
#include "script_resource_liveupdate.h"


namespace dmGameSystem
{

/*# Resource API documentation
 *
 * Functions and constants to access resources.
 *
 * @document
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
    const char* format = 0;
    switch(result)
    {
    case dmResource::RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found: %llu, %s"; break;
    case dmResource::RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation: %llu, %s"; break;
    default:                                    format = "The resource was not updated: %llu, %s"; break;
    }
    DM_SNPRINTF(msg, sizeof(msg), format, (unsigned long long)path_hash, dmHashReverseSafe64(path_hash));
    return luaL_error(L, "%s", msg);
}

/*# Set a resource
 * Sets the resource data for a specific resource
 *
 * @name resource.set
 *
 * @param path [type:string|hash] The path to the resource
 * @param buffer [type:buffer] The buffer of precreated data, suitable for the intended resource type
 *
 * @examples
 *
 * Assuming the folder "/res" is added to the project custom resources:
 *
 * ```lua
 * -- load a texture resource and set it on a sprite
 * local buffer = resource.load("/res/new.texturec")
 * resource.set(go.get("#sprite", "texture0"), buffer)
 * ```
 */
static int Set(lua_State* L)
{
    int top = lua_gettop(L);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 2);

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, buffer->m_Buffer);
    if( r != dmResource::RESULT_OK )
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, r, path_hash);
    }
    assert(top == lua_gettop(L));
    return 0;
}

/*# load a resource
 * Loads the resource data for a specific resource.
 *
 * @name resource.load
 *
 * @param path [type:string|hash] The path to the resource
 * @return buffer [type:buffer] Returns the buffer stored on disc
 *
 * @examples
 *
 * ```lua
 * -- read custom resource data into buffer
 * local buffer = resource.load("/resources/datafile")
 * ```
 *
 * In order for the engine to include custom resources in the build process, you need
 * to specify them in the "game.project" settings file:
 *
 * ```ini
 * [project]
 * title = My project
 * version = 0.1
 * custom_resources = resources/,assets/level_data.json
 * ```
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
    dmBuffer::Create(resourcesize, streams_decl, 1, &buffer);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer, (void**)&data, &datasize);

    memcpy(data, resource, resourcesize);

    dmScript::LuaHBuffer luabuf = {buffer, true};
    dmScript::PushBuffer(L, luabuf);
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
        return luaL_error(L, "%s", msg);
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


/*# set a texture
 * Sets the pixel data for a specific texture.
 *
 * @name resource.set_texture
 *
 * @param path [type:hash|string] The path to the resource
 * @param table [type:table] A table containing info about the texture. Supported entries:
 *
 * `type`
 * : [type:number] The texture type. Supported values:
 *
 * - `resource.TEXTURE_TYPE_2D`
 *
 * `width`
 * : [type:number] The width of the texture (in pixels)
 *
 * `height`
 * : [type:number] The width of the texture (in pixels)
 *
 * `format`
 * : [type:number] The texture format. Supported values:
 *
 * - `resource.TEXTURE_FORMAT_LUMINANCE`
 * - `resource.TEXTURE_FORMAT_RGB`
 * - `resource.TEXTURE_FORMAT_RGBA`
 *
 * @param buffer [type:buffer] The buffer of precreated pixel data
 *
 * [icon:attention] Currently, only 1 mipmap is generated.
 *
 * @examples
 * How to set all pixels of an atlas
 *
 * ```lua
 * function init(self)
 *   self.height = 128
 *   self.width = 128
 *   self.buffer = buffer.create(self.width * self.height, { {name=hash("rgb"), type=buffer.VALUE_TYPE_UINT8, count=3} } )
 *   self.stream = buffer.get_stream(self.buffer, hash("rgb"))
 *   
 *   for y=1,self.height do
 *       for x=1,self.width do
 *           local index = (y-1) * self.width * 3 + (x-1) * 3 + 1
 *           self.stream[index + 0] = 0xff
 *           self.stream[index + 1] = 0x80
 *           self.stream[index + 2] = 0x10
 *       end
 *   end

 *   local resource_path = go.get("#sprite", "texture0")
 *   local header = { width=self.width, height=self.height, type=resource.TEXTURE_TYPE_2D, format=resource.TEXTURE_FORMAT_RGB, num_mip_maps=1 }
 *   resource.set_texture( resource_path, header, self.buffer )
 * end
 * ```
 */
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

    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 3);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

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

    // LiveUpdate functionality in resource namespace
    {"get_current_manifest", dmLiveUpdate::Resource_GetCurrentManifest},
    {"create_manifest", dmLiveUpdate::Resource_CreateManifest},
    {"destroy_manifest", dmLiveUpdate::Resource_DestroyManifest},
    {"store_resource", dmLiveUpdate::Resource_StoreResource},
    {"store_manifest", dmLiveUpdate::Resource_StoreManifest},

    {0, 0}
};

/*# 2D texture type
 *
 * @name resource.TEXTURE_TYPE_2D
 * @variable
 */

/*# luminance type texture format
 *
 * @name resource.TEXTURE_FORMAT_LUMINANCE
 * @variable
 */

/*# RGB type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB
 * @variable
 */

/*# RGBA type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA
 * @variable
 */

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
