// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <float.h>  // FLT_MAX

#include <dlib/buffer.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <gamesys/mesh_ddf.h>
#include <gamesys/texture_set_ddf.h>
#include <graphics/graphics_ddf.h>
#include <render/font_renderer.h>
#include <resource/resource.h>
#include <resource/resource_util.h>
#include <gameobject/gameobject.h>

#include "script_resource.h"
#include "script_buffer.h"

#include "../gamesys.h"
#include "../resources/res_buffer.h"
#include "../resources/res_texture.h"
#include "../resources/res_textureset.h"

#include <dmsdk/script/script.h>
#include <dmsdk/gamesys/script.h>

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

/*# reference to material resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.material
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Load a material and set it to a sprite:
 *
 * ```lua
 * go.property("my_material", resource.material("/material.material"))
 * function init(self)
 *   go.set("#sprite", "material", self.my_material)
 * end
 * ```
 *
 * Load a material resource and update a named material with the resource:
 *
 * ```lua
 * go.property("my_material", resource.material("/material.material"))
 * function init(self)
 *   go.set("#gui", "materials", self.my_material, {key = "my_material"})
 * end
 * ```
 */

/*# reference to font resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.font
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Load a font and set it to a label:
 *
 * ```lua
 * go.property("my_font", resource.font("/font.font"))
 * function init(self)
 *   go.set("#label", "font", self.my_font)
 * end
 * ```
 *
 * Load a font and set it to a gui:
 *
 * ```lua
 * go.property("my_font", resource.font("/font.font"))
 * function init(self)
 *   go.set("#gui", "fonts", self.my_font, {key = "my_font"})
 * end
 * ```
 */

/*# reference to texture resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.texture
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Load a texture and set it to a model:
 *
 * ```lua
 * go.property("my_texture", resource.texture("/texture.png"))
 * function init(self)
 *   go.set("#model", "texture0", self.my_texture)
 * end
 * ```
 */

/*# reference to atlas resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.atlas
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Load an atlas and set it to a sprite:
 *
 * ```lua
 * go.property("my_atlas", resource.atlas("/atlas.atlas"))
 * function init(self)
 *   go.set("#sprite", "image", self.my_atlas)
 * end
 * ```
 *
 * Load an atlas and set it to a gui:
 *
 * ```lua
 * go.property("my_atlas", resource.atlas("/atlas.atlas"))
 * function init(self)
 *   go.set("#gui", "textures", self.my_atlas, {key = "my_atlas"})
 * end
 * ```
 */

/*# reference to buffer resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.buffer
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Set a unique buffer it to a sprite:
 *
 * ```lua
 * go.property("my_buffer", resource.buffer("/cube.buffer"))
 * function init(self)
 *   go.set("#mesh", "vertices", self.my_buffer)
 * end
 * ```
 */

/*# reference to tile source resource
 *
 * Constructor-like function with two purposes:
 *
 * - Load the specified resource as part of loading the script
 * - Return a hash to the run-time version of the resource
 *
 * [icon:attention] This function can only be called within [ref:go.property] function calls.
 *
 * @name resource.tile_source
 * @param [path] [type:string] optional resource path string to the resource
 * @return path [type:hash] a path hash to the binary version of the resource
 * @examples
 *
 * Load tile source and set it to a tile map:
 *
 * ```lua
 * go.property("my_tile_source", resource.tile_source("/tilesource.tilesource"))
 * function init(self)
 *   go.set("#tilemap", "tile_source", self.my_tile_source)
 * end
 * ```
 */

struct ResourceModule
{
    dmResource::HFactory m_Factory;
    dmGraphics::HContext m_GraphicsContext;
} g_ResourceModule;

static int ReportPathError(lua_State* L, dmResource::Result result, dmhash_t path_hash)
{
    char msg[256];
    const char* format = 0;
    switch(result)
    {
    case dmResource::RESULT_RESOURCE_NOT_FOUND: format = "The resource was not found (%d): %llu, %s"; break;
    case dmResource::RESULT_NOT_SUPPORTED:      format = "The resource type does not support this operation (%d): %llu, %s"; break;
    default:                                    format = "The resource was not updated (%d): %llu, %s"; break;
    }
    dmSnPrintf(msg, sizeof(msg), format, result, (unsigned long long)path_hash, dmHashReverseSafe64(path_hash));
    return luaL_error(L, "%s", msg);
}

static void* CheckResource(lua_State* L, dmResource::HFactory factory, dmhash_t path_hash, const char* resource_ext)
{
    dmResource::SResourceDescriptor* rd = dmResource::FindByHash(factory, path_hash);
    if (!rd) {
        luaL_error(L, "Could not get %s type resource: %s", resource_ext, dmHashReverseSafe64(path_hash));
        return 0;
    }

    dmResource::ResourceType resource_type;
    dmResource::Result r = dmResource::GetType(factory, rd->m_Resource, &resource_type);
    if( r != dmResource::RESULT_OK )
    {
        ReportPathError(L, r, path_hash);
    }

    dmResource::ResourceType expected_resource_type;
    r = dmResource::GetTypeFromExtension(factory, resource_ext, &expected_resource_type);
    if( r != dmResource::RESULT_OK )
    {
        ReportPathError(L, r, path_hash);
    }

    if (resource_type != expected_resource_type) {
        luaL_error(L, "Resource %s is not of type %s.", dmHashReverseSafe64(path_hash), resource_ext);
        return 0;
    }

    return rd->m_Resource;
}

static dmhash_t GetCanonicalPathHash(const char* path)
{
    char canonical_path[dmResource::RESOURCE_PATH_MAX];
    uint32_t path_len  = dmResource::GetCanonicalPath(path, canonical_path);
    return dmHashBuffer64(canonical_path, path_len);
}

static void PreCreateResource(lua_State* L, const char* path_str, const char* path_ext_wanted, dmhash_t* canonical_path_hash_out)
{
    const char* path_ext = dmResource::GetExtFromPath(path_str);

    if (path_ext == 0x0 || dmStrCaseCmp(path_ext, path_ext_wanted) != 0)
    {
        luaL_error(L, "Unable to create resource, path '%s' must have the %s extension", path_str, path_ext_wanted);
    }

    dmhash_t canonical_path_hash = GetCanonicalPathHash(path_str);
    if (dmResource::FindByHash(g_ResourceModule.m_Factory, canonical_path_hash))
    {
        luaL_error(L, "Unable to create resource, a resource is already registered at path '%s'", path_str);
    }

    *canonical_path_hash_out = canonical_path_hash;
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

    uint32_t datasize = 0;
    void* data = 0;
    dmBuffer::GetBytes(buffer->m_Buffer, &data, &datasize);

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, data, datasize);
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
 * @param path [type:string] The path to the resource
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

    dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
    dmScript::PushBuffer(L, luabuf);
    assert(top + 1 == lua_gettop(L));
    return 1;
}


static int DoCheckError(lua_State* L, const char* attr_name, const char* expected_type)
{
    char msg[256];
    dmSnPrintf(msg, sizeof(msg), "Wrong type for table attribute '%s'. Expected %s, got %s", attr_name, expected_type, luaL_typename(L, -1) );
    return luaL_error(L, "%s", msg);
}

////////////////////////////////////

template<typename T>
T CheckValue(lua_State* L, int index, const char* attr_name)
{
    (void)L;
    (void)index;
    (void)attr_name;
    T t = 0;
    return t; // SHouldn't be used
}

template<>
bool CheckValue<bool>(lua_State* L, int index, const char* attr_name)
{
    if (!lua_isboolean(L, index)) {
        return DoCheckError(L, attr_name, "boolean");
    }
    return lua_toboolean(L, index);
}

template<>
float CheckValue<float>(lua_State* L, int index, const char* attr_name)
{
    if (!lua_isnumber(L, index)) {
        return DoCheckError(L, attr_name, "number");
    }
    return lua_tonumber(L, index);
}

template<>
int CheckValue<int>(lua_State* L, int index, const char* attr_name)
{
    if (!lua_isnumber(L, index)) {
        return DoCheckError(L, attr_name, "integer");
    }
    return lua_tointeger(L, index);
}

template<typename T>
T CheckValueDefault(lua_State* L, int index, const char* attr_name, T default_value)
{
    if (lua_isnil(L, -1)) {
        return default_value;
    }
    return CheckValue<T>(L, -1, attr_name);
}

////////////////////////////////////

template<typename T>
T CheckTableValue(lua_State* L, int index, const char* name)
{
    lua_pushstring(L, name);
    lua_gettable(L, index);
    T result = CheckValue<T>(L, -1, name);
    lua_pop(L, 1);
    return result;
}

template<typename T>
T CheckTableValue(lua_State* L, int index, const char* name, T default_value)
{
    lua_pushstring(L, name);
    lua_gettable(L, index);
    T result = CheckValueDefault<T>(L, -1, name, default_value);
    lua_pop(L, 1);
    return result;
}

template<typename T>
T CheckFieldValue(lua_State* L, int index, const char* name)
{
    lua_getfield(L, index, name);
    T result = CheckValue<T>(L, -1, name);
    lua_pop(L, 1);
    return result;
}

template<typename T>
T CheckFieldValue(lua_State* L, int index, const char* name, T default_value)
{
    lua_getfield(L, index, name);
    T result = CheckValueDefault<T>(L, -1, name, default_value);
    lua_pop(L, 1);
    return result;
}

////////////////////////////////////
static bool CheckTableBoolean(lua_State* L, int index, const char* name, bool default_value)
{
    return CheckTableValue<bool>(L, index, name, default_value);
}
static int CheckTableInteger(lua_State* L, int index, const char* name)
{
    return CheckTableValue<int>(L, index, name);
}
static int CheckTableInteger(lua_State* L, int index, const char* name, int default_value)
{
    return CheckTableValue<int>(L, index, name, default_value);
}
static float CheckTableNumber(lua_State* L, int index, const char* name, float default_value)
{
    return CheckTableValue<float>(L, index, name, default_value);
}

static dmGraphics::TextureImage::TextureFormat GraphicsTextureFormatToImageFormat(int textureformat)
{
    #define GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(x) case dmGraphics::x: return dmGraphics::TextureImage::x
    switch(textureformat)
    {
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_LUMINANCE);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_ETC1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ETC2);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_ASTC_4x4);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB_BC1);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_BC3);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R_BC4);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG_BC5);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA_BC7);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB16F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGB32F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA16F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RGBA32F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R16F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG16F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_R32F);
        GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE(TEXTURE_FORMAT_RG32F);
    };
    #undef GRAPHCIS_TO_TEXTURE_IMAGE_ENUM_CASE
    return (dmGraphics::TextureImage::TextureFormat) -1;
}

static dmGraphics::TextureImage::Type GraphicsTextureTypeToImageType(dmGraphics::TextureType texturetype)
{
    switch(texturetype)
    {
        case dmGraphics::TEXTURE_TYPE_2D:       return dmGraphics::TextureImage::TYPE_2D;
        case dmGraphics::TEXTURE_TYPE_2D_ARRAY: return dmGraphics::TextureImage::TYPE_2D_ARRAY;
        case dmGraphics::TEXTURE_TYPE_CUBE_MAP: return dmGraphics::TextureImage::TYPE_CUBEMAP;
        default: assert(0);
    }
    dmLogError("Unsupported texture type (%d)", texturetype);
    return (dmGraphics::TextureImage::Type) -1;
}

static void MakeTextureImage(uint16_t width, uint16_t height, uint8_t max_mipmaps, uint8_t bitspp,
    dmGraphics::TextureImage::Type type, dmGraphics::TextureImage::TextureFormat format,
    dmGraphics::TextureImage::CompressionType compression_type, dmBuffer::HBuffer texture_buffer,
    dmGraphics::TextureImage* texture_image)
{
    uint32_t* mip_map_sizes              = new uint32_t[max_mipmaps];
    uint32_t* mip_map_offsets            = new uint32_t[max_mipmaps];
    uint32_t* mip_map_offsets_compressed = new uint32_t[1];
    uint8_t layer_count = type == dmGraphics::TextureImage::TYPE_CUBEMAP ? 6 : 1;

    uint32_t data_size = 0;
    uint16_t mm_width  = width;
    uint16_t mm_height = height;
    for (uint32_t i = 0; i < max_mipmaps; ++i)
    {
        mip_map_sizes[i]    = dmMath::Max(mm_width, mm_height);
        mip_map_offsets[i]  = (data_size / 8);
        data_size          += mm_width * mm_height * bitspp * layer_count;
        mm_width           /= 2;
        mm_height          /= 2;
    }
    assert(data_size > 0);

    data_size                *= layer_count;
    uint32_t image_data_size  = data_size / 8; // bits -> bytes for compression formats
    uint8_t* image_data       = 0;

    if (texture_buffer)
    {
        uint8_t* data     = 0;
        uint32_t datasize = 0;
        dmBuffer::GetBytes(texture_buffer, (void**)&data, &datasize);
        image_data      = data;
        image_data_size = datasize;
    }
    else
    {
        image_data = new uint8_t[image_data_size];
    }

    // Note: Right now we only support creating compressed 2D textures with 1 mipmap,
    //       so we only need a pointer here for the data offset.
    mip_map_offsets_compressed[0] = image_data_size;

    dmGraphics::TextureImage::Image* image = new dmGraphics::TextureImage::Image();
    texture_image->m_Alternatives.m_Data   = image;
    texture_image->m_Alternatives.m_Count  = 1;
    texture_image->m_Type                  = type;
    texture_image->m_Count                 = layer_count;

    image->m_Width                = width;
    image->m_Height               = height;
    image->m_OriginalWidth        = width;
    image->m_OriginalHeight       = height;
    image->m_Format               = format;
    image->m_CompressionType      = compression_type;
    image->m_CompressionFlags     = 0;
    image->m_Data.m_Data          = image_data;
    image->m_Data.m_Count         = image_data_size;

    image->m_MipMapOffset.m_Data  = mip_map_offsets;
    image->m_MipMapOffset.m_Count = max_mipmaps;
    image->m_MipMapSize.m_Data    = mip_map_sizes;
    image->m_MipMapSize.m_Count   = max_mipmaps;
    image->m_MipMapSizeCompressed.m_Data  = mip_map_offsets_compressed;
    image->m_MipMapSizeCompressed.m_Count = 1;
}

static void DestroyTextureImage(dmGraphics::TextureImage& texture_image, bool destroy_image_data)
{
    for (int i = 0; i < texture_image.m_Alternatives.m_Count; ++i)
    {
        dmGraphics::TextureImage::Image& image = texture_image.m_Alternatives.m_Data[i];
        delete[] image.m_MipMapOffset.m_Data;
        delete[] image.m_MipMapSize.m_Data;
        delete[] image.m_MipMapSizeCompressed.m_Data;
        if (destroy_image_data)
            delete[] image.m_Data.m_Data;
    }
    delete[] texture_image.m_Alternatives.m_Data;
}

static void CheckTextureResource(lua_State* L, int i, const char* field_name, dmhash_t* texture_path_out, dmGraphics::HTexture* texture_out)
{
    lua_getfield(L, i, field_name);
    dmhash_t path_hash           = dmScript::CheckHashOrString(L, -1);
    TextureResource* texture_res = (TextureResource*) CheckResource(L, g_ResourceModule.m_Factory, path_hash, "texturec");
    *texture_out                 = texture_res->m_Texture;
    *texture_path_out            = path_hash;
    lua_pop(L, 1); // "texture"
}

/*# create a texture
 * Creates a new texture resource that can be used in the same way as any texture created during build time.
 * The path used for creating the texture must be unique, trying to create a resource at a path that is already
 * registered will trigger an error. If the intention is to instead modify an existing texture, use the [ref:resource.set_texture]
 * function. Also note that the path to the new texture resource must have a '.texturec' extension,
 * meaning "/path/my_texture" is not a valid path but "/path/my_texture.texturec" is.
 *
 * @name resource.create_texture
 *
 * @param path [type:string] The path to the resource.
 * @param table [type:table] A table containing info about how to create the texture. Supported entries:
 *
 * `type`
 * : [type:number] The texture type. Supported values:
 *
 * - `resource.TEXTURE_TYPE_2D`
 * - `resource.TEXTURE_TYPE_CUBE_MAP`
 *
 * `width`
 * : [type:number] The width of the texture (in pixels)
 *
 * `height`
 * : [type:number] The width of the texture (in pixels)
 *
 * `format`
 * : [type:number] The texture format, note that some of these formats might not be supported by the running device. Supported values:
 *
 * - `resource.TEXTURE_FORMAT_LUMINANCE`
 * - `resource.TEXTURE_FORMAT_RGB`
 * - `resource.TEXTURE_FORMAT_RGBA`
 * 
 * These constants might not be available on the device:
 * - `resource.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1`
 * - `resource.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1`
 * - `resource.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1`
 * - `resource.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1`
 * - `resource.TEXTURE_FORMAT_RGB_ETC1`
 * - `resource.TEXTURE_FORMAT_RGBA_ETC2`
 * - `resource.TEXTURE_FORMAT_RGBA_ASTC_4x4`
 * - `resource.TEXTURE_FORMAT_RGB_BC1`
 * - `resource.TEXTURE_FORMAT_RGBA_BC3`
 * - `resource.TEXTURE_FORMAT_R_BC4`
 * - `resource.TEXTURE_FORMAT_RG_BC5`
 * - `resource.TEXTURE_FORMAT_RGBA_BC7`
 * - `resource.TEXTURE_FORMAT_RGB16F`
 * - `resource.TEXTURE_FORMAT_RGB32F`
 * - `resource.TEXTURE_FORMAT_RGBA16F`
 * - `resource.TEXTURE_FORMAT_RGBA32F`
 * - `resource.TEXTURE_FORMAT_R16F`
 * - `resource.TEXTURE_FORMAT_RG16F`
 * - `resource.TEXTURE_FORMAT_R32F`
 * - `resource.TEXTURE_FORMAT_RG32F`
 *
 * You can test if the device supports these values by checking if a specific enum is nil or not:
 *
 * ```lua
 * if resource.TEXTURE_FORMAT_RGBA16F ~= nil then
 *     -- it is safe to use this format
 * end
 * ```
 *
 * `max_mipmaps`
 * : [type:number] optional max number of mipmaps. Defaults to zero, i.e no mipmap support
 *
 * `compression_type`
 * : [type:number] optional specify the compression type for the data in the buffer object that holds the texture data. Will only be used when a compressed buffer has been passed into the function.
 * Creating an empty texture with no buffer data is not supported as a core feature. Defaults to resource.COMPRESSION_TYPE_DEFAULT, i.e no compression. Supported values:
 *
 * - `COMPRESSION_TYPE_DEFAULT`
 * - `COMPRESSION_TYPE_BASIS_UASTC`
 *
 * @param buffer [type:buffer] optional buffer of precreated pixel data
 *
 * @return path [type:hash] The path to the resource.
 *
 * @examples
 * How to create an 128x128 RGBA texture resource and assign it to a model
 *
 * ```lua
 * function init(self)
 *     local tparams = {
 *        width          = 128,
 *        height         = 128,
 *        type           = resource.TEXTURE_TYPE_2D,
 *        format         = resource.TEXTURE_FORMAT_RGBA,
 *    }
 *    local my_texture_id = resource.create_texture("/my_custom_texture.texturec", tparams)
 *    go.set("#model", "texture0", my_texture_id)
 * end
 * ```
 *
 * @examples
 * How to create an 128x128 floating point texture (RGBA32F) resource from a buffer object
 *
 * ```lua
 * function init(self)
 *     -- Create a new buffer with 4 components and FLOAT32 type
 *     local tbuffer = buffer.create(128 * 128, { {name=hash("rgba"), type=buffer.VALUE_TYPE_FLOAT32, count=4} } )   
 *     local tstream = buffer.get_stream(tbuffer, hash("rgba"))
 *
 *     -- Fill the buffer stream with some float values
 *     for y=1,128 do
 *         for x=1,128 do
 *             local index = (y-1) * 128 * 4 + (x-1) * 4 + 1
 *             tstream[index + 0] = 999.0
 *             tstream[index + 1] = -1.0
 *             tstream[index + 2] = 0.5
 *             tstream[index + 3] = 1.0
 *         end
 *     end
 *      
 *     -- Create a 2D Texture with a RGBA23F format
 *     local tparams = {
 *        width          = 128,
 *        height         = 128,
 *        type           = resource.TEXTURE_TYPE_2D,
 *        format         = resource.TEXTURE_FORMAT_RGBA32F,
 *    }
 *
 *    -- Note that we pass the buffer as the last argument here!
 *    local my_texture_id = resource.create_texture("/my_custom_texture.texturec", tparams, tbuffer)
 *    
 *    -- assign the texture to a model
 *    go.set("#model", "texture0", my_texture_id)
 * end
 * ```
 */
static int CreateTexture(lua_State* L)
{
    // This function pushes the hash of the resource created
    int top                  = lua_gettop(L);
    const char* path_str     = luaL_checkstring(L, 1);
    const char* texturec_ext = ".texturec";

    dmhash_t canonical_path_hash;
    PreCreateResource(L, path_str, texturec_ext, &canonical_path_hash);

    dmGameObject::HInstance sender_instance = dmScript::CheckGOInstance(L);
    dmGameObject::HCollection collection    = dmGameObject::GetCollection(sender_instance);

    luaL_checktype(L, 2, LUA_TTABLE);
    dmGraphics::TextureType type     = (dmGraphics::TextureType) CheckTableInteger(L, 2, "type");
    dmGraphics::TextureFormat format = (dmGraphics::TextureFormat) CheckTableInteger(L, 2, "format");
    uint32_t width                   = (uint32_t) CheckTableInteger(L, 2, "width");
    uint32_t height                  = (uint32_t) CheckTableInteger(L, 2, "height");
    uint32_t max_mipmaps             = (uint32_t) CheckTableInteger(L, 2, "max_mipmaps", 0);

    // TODO: Texture arrays
    if (!(type == dmGraphics::TEXTURE_TYPE_2D || type == dmGraphics::TEXTURE_TYPE_CUBE_MAP))
    {
        return luaL_error(L, "Unable to create texture, unsupported texture type '%s'.", dmGraphics::GetTextureTypeLiteral(type));
    }

    dmGraphics::TextureImage::CompressionType compression_type = (dmGraphics::TextureImage::CompressionType) CheckTableInteger(L, 2, "compression_type", (int) dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT);

    dmBuffer::HBuffer buffer = 0;
    if (dmScript::IsBuffer(L, 3))
    {
        dmScript::LuaHBuffer* l_buffer = dmScript::CheckBuffer(L, 3);
        buffer                         = dmGameSystem::UnpackLuaBuffer(l_buffer);
    }

    uint8_t max_mipmaps_actual = dmGraphics::GetMipmapCount(dmMath::Max(width, height));
    if (max_mipmaps > max_mipmaps_actual)
    {
        dmLogWarning("Max mipmaps %d requested for texture %s, but max mipmaps supported for size (%d, %d) is %d",
            max_mipmaps, path_str, width, height, max_mipmaps_actual);
        max_mipmaps = max_mipmaps_actual;
    }

    if (buffer == 0 && compression_type != dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT)
    {
        dmLogWarning("Compression type %d requested for textures %s, but no buffer was supplied. This is not supported, COMPRESSION_TYPE_DEFAULT will be used",
            (int) compression_type, path_str);
        compression_type = dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT;
    }

    // Max mipmap count is inclusive, so need at least 1
    max_mipmaps                                        = dmMath::Max((uint32_t) 1, max_mipmaps);
    uint32_t tex_bpp                                   = dmGraphics::GetTextureFormatBitsPerPixel((dmGraphics::TextureFormat) format);
    dmGraphics::TextureImage::Type tex_type            = GraphicsTextureTypeToImageType(type);
    dmGraphics::TextureImage::TextureFormat tex_format = GraphicsTextureFormatToImageFormat(format);
    dmGraphics::TextureImage texture_image             = {};

    if (!dmGraphics::IsTextureFormatSupported(g_ResourceModule.m_GraphicsContext, format))
    {
        return luaL_error(L, "Unable to set texture, unsupported texture format '%s'.", dmGraphics::GetTextureFormatLiteral(format));
    }

    // TODO: To support this, we need to supply separate buffers for each side as an option, or offsets into the buffer where each side is located
    if ((tex_type == dmGraphics::TextureImage::TYPE_CUBEMAP || tex_type == dmGraphics::TextureImage::TYPE_2D_ARRAY) && compression_type != dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT)
    {
        return luaL_error(L, "Compression type %d requested for texture %s with type '%s', but this is currently not supported.",
            (int) compression_type, path_str, dmGraphics::GetTextureTypeLiteral(type));
    }

    MakeTextureImage(width, height, max_mipmaps, tex_bpp, tex_type, tex_format, compression_type, buffer, &texture_image);

    dmArray<uint8_t> ddf_buffer;
    dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(&texture_image, dmGraphics::TextureImage::m_DDFDescriptor, ddf_buffer);
    assert(ddf_result == dmDDF::RESULT_OK);

    void* resource = 0x0;
    dmResource::Result res = dmResource::CreateResource(g_ResourceModule.m_Factory, path_str, ddf_buffer.Begin(), ddf_buffer.Size(), &resource);

    DestroyTextureImage(texture_image, buffer == 0);

    if (res != dmResource::RESULT_OK)
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, res, canonical_path_hash);
    }

    dmGameObject::AddDynamicResourceHash(collection, canonical_path_hash);

    dmScript::PushHash(L, canonical_path_hash);
    assert((top+1) == lua_gettop(L));
    return 1;
}

/*# release a resource
 * Release a resource.
 *
 * [icon:attention] This is a potentially dangerous operation, releasing resources currently being used can cause unexpected behaviour.
 *
 * @name resource.release
 *
 * @param path [type:hash|string] The path to the resource.
 *
 */
static int ReleaseResource(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    dmResource::SResourceDescriptor* rd = dmResource::FindByHash(g_ResourceModule.m_Factory, path_hash);
    if (!rd)
    {
        return DM_LUA_ERROR("Could not get resource: %s", dmHashReverseSafe64(path_hash));
    }

    dmGameObject::HInstance sender_instance = dmScript::CheckGOInstance(L);
    dmGameObject::HCollection collection    = dmGameObject::GetCollection(sender_instance);

    // This will remove the entry in the collections list of dynamically allocated resource (if it exists),
    // but we do the actual release here since we allow releasing arbitrary resources now
    dmGameObject::RemoveDynamicResourceHash(collection, path_hash);
    dmResource::Release(g_ResourceModule.m_Factory, rd->m_Resource);

    return 0;
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
 * - `resource.TEXTURE_TYPE_CUBE_MAP`
 *
 * `width`
 * : [type:number] The width of the texture (in pixels)
 *
 * `height`
 * : [type:number] The width of the texture (in pixels)
 *
 * `format`
 * : [type:number] The texture format, note that some of these formats are platform specific. Supported values:
 *
 * - `resource.TEXTURE_FORMAT_LUMINANCE`
 * - `resource.TEXTURE_FORMAT_RGB`
 * - `resource.TEXTURE_FORMAT_RGBA`
 *
 * These constants might not be available on the device:
 * - `resource.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1`
 * - `resource.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1`
 * - `resource.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1`
 * - `resource.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1`
 * - `resource.TEXTURE_FORMAT_RGB_ETC1`
 * - `resource.TEXTURE_FORMAT_RGBA_ETC2`
 * - `resource.TEXTURE_FORMAT_RGBA_ASTC_4x4`
 * - `resource.TEXTURE_FORMAT_RGB_BC1`
 * - `resource.TEXTURE_FORMAT_RGBA_BC3`
 * - `resource.TEXTURE_FORMAT_R_BC4`
 * - `resource.TEXTURE_FORMAT_RG_BC5`
 * - `resource.TEXTURE_FORMAT_RGBA_BC7`
 * - `resource.TEXTURE_FORMAT_RGB16F`
 * - `resource.TEXTURE_FORMAT_RGB32F`
 * - `resource.TEXTURE_FORMAT_RGBA16F`
 * - `resource.TEXTURE_FORMAT_RGBA32F`
 * - `resource.TEXTURE_FORMAT_R16F`
 * - `resource.TEXTURE_FORMAT_RG16F`
 * - `resource.TEXTURE_FORMAT_R32F`
 * - `resource.TEXTURE_FORMAT_RG32F`
 *
 * You can test if the device supports these values by checking if a specific enum is nil or not:
 *
 * ```lua
 * if resource.TEXTURE_FORMAT_RGBA16F ~= nil then
 *     -- it is safe to use this format
 * end
 * ```
 *
 * `x`
 * : [type:number] optional x offset of the texture (in pixels)
 *
 * `y`
 * : [type:number] optional y offset of the texture (in pixels)
 *
 * `mipmap`
 * : [type:number] optional mipmap to upload the data to
 *
 * `compression_type`
 * : [type:number] optional specify the compression type for the data in the buffer object that holds the texture data. Defaults to resource.COMPRESSION_TYPE_DEFAULT, i.e no compression. Supported values:
 *
 * - `COMPRESSION_TYPE_DEFAULT`
 * - `COMPRESSION_TYPE_BASIS_UASTC`
 *
 * @param buffer [type:buffer] The buffer of precreated pixel data
 *
 * [icon:attention] To update a cube map texture you need to pass in six times the amount of data via the buffer, since a cube map has six sides!
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
 *
 *   local resource_path = go.get("#model", "texture0")
 *   local args = { width=self.width, height=self.height, type=resource.TEXTURE_TYPE_2D, format=resource.TEXTURE_FORMAT_RGB, num_mip_maps=1 }
 *   resource.set_texture( resource_path, args, self.buffer )
 * end
 * ```
 *
 * @examples
 * How to update a specific region of an atlas by using the x,y values. Assumes the already set atlas is a 128x128 texture.
 *
 * ```lua
 * function init(self)
 *   self.x = 16
 *   self.y = 16
 *   self.height = 128 - self.x * 2
 *   self.width = 128 - self.y * 2
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
 *
 *   local resource_path = go.get("#model", "texture0")
 *   local args = { width=self.width, height=self.height, x=self.x, y=self.y, type=resource.TEXTURE_TYPE_2D, format=resource.TEXTURE_FORMAT_RGB, num_mip_maps=1 }
 *   resource.set_texture( resource_path, args, self.buffer )
 * end
 * ```
 */
static int SetTexture(lua_State* L)
{
    // Note: We only support uploading a single mipmap for a single slice at a time
    const uint32_t NUM_MIP_MAPS = 1;
    const int32_t DEFAULT_INT_NOT_SET = -1;

    int top = lua_gettop(L);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);
    dmGraphics::TextureType type     = (dmGraphics::TextureType) CheckTableInteger(L, 2, "type");
    dmGraphics::TextureFormat format = (dmGraphics::TextureFormat) CheckTableInteger(L, 2, "format");
    uint32_t width                   = (uint32_t) CheckTableInteger(L, 2, "width");
    uint32_t height                  = (uint32_t) CheckTableInteger(L, 2, "height");
    uint32_t mipmap                  = (uint32_t) CheckTableInteger(L, 2, "mipmap", 0);
    int32_t x                        = (int32_t)  CheckTableInteger(L, 2, "x", DEFAULT_INT_NOT_SET);
    int32_t y                        = (int32_t)  CheckTableInteger(L, 2, "y", DEFAULT_INT_NOT_SET);

    if (!dmGraphics::IsTextureFormatSupported(g_ResourceModule.m_GraphicsContext, format))
    {
        return luaL_error(L, "Unable to set texture, unsupported texture format '%s'.", dmGraphics::GetTextureFormatLiteral(format));
    }

    // TODO: Texture arrays
    if (!(type == dmGraphics::TEXTURE_TYPE_2D || type == dmGraphics::TEXTURE_TYPE_CUBE_MAP))
    {
        return luaL_error(L, "Unable to set texture, unsupported texture type '%s'.", dmGraphics::GetTextureTypeLiteral(type));
    }

    dmGraphics::TextureImage::CompressionType compression_type = (dmGraphics::TextureImage::CompressionType) CheckTableInteger(L, 2, "compression_type", (int) dmGraphics::TextureImage::COMPRESSION_TYPE_DEFAULT);

    bool sub_update = x != DEFAULT_INT_NOT_SET || y != DEFAULT_INT_NOT_SET;
    x               = dmMath::Max(0, x);
    y               = dmMath::Max(0, y);

    uint8_t layer_count = type == dmGraphics::TEXTURE_TYPE_CUBE_MAP ? 6 : 1;

    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 3);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

    dmGraphics::TextureImage::Image image  = {};
    dmGraphics::TextureImage texture_image = {};
    texture_image.m_Alternatives.m_Data    = &image;
    texture_image.m_Alternatives.m_Count   = 1;
    texture_image.m_Type                   = GraphicsTextureTypeToImageType(type);
    texture_image.m_Count                  = 1;

    image.m_Width                = width;
    image.m_Height               = height;
    image.m_OriginalWidth        = width;
    image.m_OriginalHeight       = height;
    image.m_Format               = GraphicsTextureFormatToImageFormat(format);
    image.m_CompressionType      = compression_type;
    image.m_CompressionFlags     = 0;
    image.m_Data.m_Data          = data;
    image.m_Data.m_Count         = datasize;

    // Note: When uploading cubemap faces on OpenGL, we expect that the "data size" is **per** slice
    //       and not the entire data size of the buffer. For vulkan we don't look at this value but instead
    //       calculate a slice size. Maybe we should do one or the other..
    uint32_t mip_map_offsets             = 0;
    uint32_t mip_map_sizes               = datasize / layer_count;
    image.m_MipMapOffset.m_Data          = &mip_map_offsets;
    image.m_MipMapOffset.m_Count         = NUM_MIP_MAPS;
    image.m_MipMapSize.m_Data            = &mip_map_sizes;
    image.m_MipMapSize.m_Count           = NUM_MIP_MAPS;
    image.m_MipMapSizeCompressed.m_Data  = &mip_map_sizes;
    image.m_MipMapSizeCompressed.m_Count = NUM_MIP_MAPS;

    ResTextureReCreateParams recreate_params;
    recreate_params.m_TextureImage = &texture_image;

    ResTextureUploadParams& upload_params = recreate_params.m_UploadParams;
    upload_params.m_X                     = x;
    upload_params.m_Y                     = y;
    upload_params.m_MipMap                = mipmap;
    upload_params.m_SubUpdate             = sub_update;
    upload_params.m_UploadSpecificMipmap  = 1;

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, (void*) &recreate_params);

    if( r != dmResource::RESULT_OK )
    {
        assert(top == lua_gettop(L));
        return ReportPathError(L, r, path_hash);
    }

    assert(top == lua_gettop(L));
    return 0;
}

/*# get texture info
 * Gets texture info from a texture resource path or a texture handle
 *
 * @name resource.get_texture_info
 *
 * @param path [type:hash|string|handle] The path to the resource or a texture handle
 * @return table [type:table] A table containing info about the texture:
 *
 * `handle`
 * : [type:handle] the opaque handle to the texture resource
 *
 * `width`
 * : [type:integer] width of the texture
 *
 * `height`
 * : [type:integer] height of the texture
 *
 * `depth`
 * : [type:integer] depth of the texture (i.e 1 for a 2D texture and 6 for a cube map)
 *
 * `mipmaps`
 * : [type:integer] number of mipmaps of the texture
 *
 * `type`
 * : [type:number] The texture type. Supported values:
 *
 * - `resource.TEXTURE_TYPE_2D`
 * - `resource.TEXTURE_TYPE_CUBE_MAP`
 * - `resource.TEXTURE_TYPE_2D_ARRAY`
 *
 * @examples
 * Create a new texture and get the metadata from it
 *
 * ```lua
 * function init(self)
 *     -- create an empty texture
 *     local tparams = {
 *         width          = 128,
 *         height         = 128,
 *         type           = resource.TEXTURE_TYPE_2D,
 *         format         = resource.TEXTURE_FORMAT_RGBA,
 *     }
 *
 *     local my_texture_path = resource.create_texture("/my_texture.texturec", tparams)
 *     local my_texture_info = resource.get_texture_info(my_texture_path)
 *
 *     -- my_texture_info now contains
 *     -- {
 *     --      handle = <the-numeric-handle>,
 *     --      width = 128,
 *     --      height = 128,
 *     --      depth = 1
 *     --      mipmaps = 1,
 *     --      type = resource.TEXTURE_TYPE_2D
 *     -- }
 * end
 * ```
 *
 * @examples
 * Get the meta data from an atlas resource
 *
 * ```lua
 * function init(self)
 *     local my_atlas_info   = resource.get_atlas("/my_atlas.a.texturesetc")
 *     local my_texture_info = resource.get_texture_info(my_atlas_info.texture)
 *
 *     -- my_texture_info now contains the information about the texture that is backing the atlas
 * end
 * ```
 */
static int GetTextureInfo(lua_State* L)
{
    int top = lua_gettop(L);
    dmGraphics::HTexture texture_handle = 0;

    if (lua_isnumber(L, 1))
    {
        texture_handle = lua_tonumber(L, 1);
        if (!dmGraphics::IsAssetHandleValid(g_ResourceModule.m_GraphicsContext, texture_handle))
        {
            return luaL_error(L, "Texture handle is not valid.");
        }
    }
    else
    {
        dmhash_t path_hash           = dmScript::CheckHashOrString(L, 1);
        TextureResource* texture_res = (TextureResource*) CheckResource(L, g_ResourceModule.m_Factory, path_hash, "texturec");
        texture_handle               = texture_res->m_Texture;

        if (!dmGraphics::IsAssetHandleValid(g_ResourceModule.m_GraphicsContext, texture_handle))
        {
            return luaL_error(L, "Texture '%s' is not a valid texture handle.", dmHashReverseSafe64(path_hash));
        }
    }

    uint32_t texture_width               = dmGraphics::GetTextureWidth(texture_handle);
    uint32_t texture_height              = dmGraphics::GetTextureHeight(texture_handle);
    uint32_t texture_depth               = dmGraphics::GetTextureDepth(texture_handle);
    uint32_t texture_mipmaps             = dmGraphics::GetTextureMipmapCount(texture_handle);
    dmGraphics::TextureType texture_type = dmGraphics::GetTextureType(texture_handle);

    lua_newtable(L);

    lua_pushnumber(L, texture_handle);
    lua_setfield(L, -2, "handle");

    lua_pushinteger(L, texture_width);
    lua_setfield(L, -2, "width");

    lua_pushinteger(L, texture_height);
    lua_setfield(L, -2, "height");

    lua_pushinteger(L, texture_depth);
    lua_setfield(L, -2, "depth");

    lua_pushinteger(L, texture_mipmaps);
    lua_setfield(L, -2, "mipmaps");

    lua_pushinteger(L, texture_type);
    lua_setfield(L, -2, "type");

    assert((top + 1) == lua_gettop(L));
    return 1;
}

// Allocates a new array and fills it with data from a lua table at top of stack.
// Only supports number values. Note: Doesn't do any error checking!
template<typename T>
static void MakeNumberArrayFromLuaTable(lua_State* L, const char* field, void** data_out, uint32_t* count_out)
{
    lua_getfield(L, -1, field);
    int num_entries = lua_objlen(L, -1);
    T* data_ptr = new T[num_entries];

    lua_pushnil(L);
    while (lua_next(L, -2))
    {
        int32_t table_index     = lua_tonumber(L, -2);
        data_ptr[table_index-1] = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    *data_out  = data_ptr;
    *count_out = num_entries;
}

static void DestroyTextureSet(dmGameSystemDDF::TextureSet& texture_set)
{
    delete[] texture_set.m_Animations.m_Data;
    delete[] texture_set.m_Geometries.m_Data;
    delete[] texture_set.m_FrameIndices.m_Data;
    delete[] texture_set.m_TexCoords.m_Data;
}

// These lookup functions are needed because the values for the two enums are different,
// so we can't rely on the raw value to convert between them
static dmGameObject::Playback DDFPlaybackToGameObjectPlayback(dmGameSystemDDF::Playback playback)
{
    switch(playback)
    {
        case dmGameSystemDDF::PLAYBACK_NONE          : return dmGameObject::PLAYBACK_NONE;
        case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD  : return dmGameObject::PLAYBACK_ONCE_FORWARD;
        case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD : return dmGameObject::PLAYBACK_ONCE_BACKWARD;
        case dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG : return dmGameObject::PLAYBACK_ONCE_PINGPONG;
        case dmGameSystemDDF::PLAYBACK_LOOP_FORWARD  : return dmGameObject::PLAYBACK_LOOP_FORWARD;
        case dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD : return dmGameObject::PLAYBACK_LOOP_BACKWARD;
        case dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG : return dmGameObject::PLAYBACK_LOOP_PINGPONG;
        default:break;
    }
    assert(0);
    return (dmGameObject::Playback) -1;
}

static dmGameSystemDDF::Playback GameObjectPlaybackToDDFPlayback(dmGameObject::Playback playback)
{
    switch(playback)
    {
        case dmGameObject::PLAYBACK_NONE:          return dmGameSystemDDF::PLAYBACK_NONE;
        case dmGameObject::PLAYBACK_ONCE_FORWARD:  return dmGameSystemDDF::PLAYBACK_ONCE_FORWARD;
        case dmGameObject::PLAYBACK_ONCE_BACKWARD: return dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD;
        case dmGameObject::PLAYBACK_ONCE_PINGPONG: return dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG;
        case dmGameObject::PLAYBACK_LOOP_FORWARD:  return dmGameSystemDDF::PLAYBACK_LOOP_FORWARD;
        case dmGameObject::PLAYBACK_LOOP_BACKWARD: return dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD;
        case dmGameObject::PLAYBACK_LOOP_PINGPONG: return dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG;
        default:break;
    }
    assert(0);
    return (dmGameSystemDDF::Playback) -1;
}

static void CheckAtlasArguments(lua_State* L, uint32_t* num_geometries_out, uint32_t* num_animations_out, uint32_t* num_animation_frames_out)
{
    int top = lua_gettop(L);
    uint32_t num_geometries = 0;
    uint32_t num_animations = 0;
    uint32_t num_animation_frames = 0;

    lua_getfield(L, -1, "geometries");

    if (!lua_isnil(L, -1))
    {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            luaL_checktype(L, -1, LUA_TTABLE);
            int geometry_index = luaL_checkinteger(L, -2);

            #define VALIDATE_GEOMETRY_STREAM(field_name, num_components) \
                { \
                    lua_getfield(L, -1, field_name); \
                    luaL_checktype(L, -1, LUA_TTABLE); \
                    if (lua_objlen(L, -1) % num_components != 0) \
                        luaL_error(L, "Uneven number of entries in %s table for geometry [%d]", field_name, geometry_index); \
                    lua_pushnil(L); \
                    while (lua_next(L, -2)) \
                    { \
                        luaL_checkinteger(L, -1); \
                        luaL_checktype(L, -2, LUA_TNUMBER); \
                        lua_pop(L, 1); \
                    } \
                    lua_pop(L, 1); \
                }

            VALIDATE_GEOMETRY_STREAM("vertices", 2);
            VALIDATE_GEOMETRY_STREAM("uvs",      2);
            VALIDATE_GEOMETRY_STREAM("indices",  3);
            #undef VALIDATE_GEOMETRY_STREAM

            lua_pop(L, 1);

            num_geometries++;

            num_animation_frames++;
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "animations");
    if (!lua_isnil(L, -1))
    {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            luaL_checktype(L, -1, LUA_TTABLE);
            int animation_index = luaL_checkinteger(L, -2);

            // Note: checkstring can change the lua stack, so we use isstring instead
            lua_getfield(L, -1, "id");
            if (!lua_isstring(L, -1))
            {
                luaL_error(L, "Invalid 'id' in animations table at index [%d], either missing or wrong type", num_animations + 1);
            }
            lua_pop(L, 1);

            // Required fields
            CheckFieldValue<int>(L, -1, "width");
            CheckFieldValue<int>(L, -1, "height");
            int frame_start = CheckFieldValue<int>(L, -1, "frame_start");
            int frame_end   = CheckFieldValue<int>(L, -1, "frame_end");

            // Non-required fields
            CheckFieldValue<int>(L,  -1, "playback", 0);
            CheckFieldValue<int>(L,  -1, "fps", 0);
            CheckFieldValue<bool>(L, -1, "flip_vertical", false );
            CheckFieldValue<bool>(L, -1, "flip_horizontal", false );

            // Validate frame indices
            int frame_interval = frame_end - frame_start;
            if (frame_start < 1 || frame_start > (num_geometries+1)) // +1 for lua indexing
            {
                luaL_error(L, "Invalid frame_start in animation [%d], index %d is outside of geometry bounds 0..%d",
                        animation_index, frame_start, num_geometries);
            }

            if (frame_end < 1 || frame_end > (num_geometries+1)) // +1 for lua indexing
            {
                luaL_error(L, "Invalid frame_end in animation [%d], index %d is outside of geometry bounds 0..%d",
                    animation_index, frame_end, num_geometries);
            }

            if (frame_interval <= 0)
            {
                luaL_error(L, "Invalid frame interval in animation [%d], start - end = %d", animation_index, frame_interval);
            }

            lua_pop(L, 1);

            num_animations++;

            num_animation_frames += frame_interval;
        }
    }

    lua_pop(L, 1);

    *num_animations_out       = num_animations;
    *num_geometries_out       = num_geometries;
    *num_animation_frames_out = num_animation_frames;

    if (num_geometries == 0)
    {
        luaL_error(L, "Atlas requires at least one entry in the 'geometries' table");
    }
    if (num_animations == 0)
    {
        luaL_error(L, "Atlas requires at least one entry in the 'animations' table");
    }

    assert(lua_gettop(L) == top);
}

// Creates a texture set from the lua stack, it is expected that the argument
// table is on top of the stack and that all fields have valid data
static void MakeTextureSetFromLua(lua_State* L, dmhash_t texture_path_hash, dmGraphics::HTexture texture, uint32_t num_geometries, uint8_t num_animations, uint32_t num_animation_frames, dmGameSystemDDF::TextureSet* texture_set_ddf)
{
    int top = lua_gettop(L);
    texture_set_ddf->m_Texture     = 0;
    texture_set_ddf->m_TextureHash = texture_path_hash;

    float tex_width            = dmGraphics::GetTextureWidth(texture);
    float tex_height           = dmGraphics::GetTextureHeight(texture);
    uint32_t frame_index_count = 0;

    texture_set_ddf->m_Geometries.m_Data  = new dmGameSystemDDF::SpriteGeometry[num_geometries];
    texture_set_ddf->m_Geometries.m_Count = num_geometries;
    memset(texture_set_ddf->m_Geometries.m_Data, 0, sizeof(dmGameSystemDDF::SpriteGeometry) * num_geometries);

    texture_set_ddf->m_Animations.m_Data  = new dmGameSystemDDF::TextureSetAnimation[num_animations];
    texture_set_ddf->m_Animations.m_Count = num_animations;
    memset(texture_set_ddf->m_Animations.m_Data, 0, sizeof(dmGameSystemDDF::TextureSetAnimation) * num_animations);

    const uint32_t num_tex_coords_per_quad  = 8;
    const uint32_t num_tex_coords_byte_size = num_animation_frames * num_tex_coords_per_quad * sizeof(float);
    texture_set_ddf->m_TexCoords.m_Data     = new uint8_t[num_tex_coords_byte_size];
    texture_set_ddf->m_TexCoords.m_Count    = num_tex_coords_byte_size;
    memset(texture_set_ddf->m_TexCoords.m_Data, 0, num_tex_coords_byte_size);

    // Fill in the UV bounding box of each geometry so we can copy that to the texcoord pointer later
    float* geometry_scratch_ptr    = new float[num_geometries * num_tex_coords_per_quad];
    float* geometry_scratch_cursor = geometry_scratch_ptr;
    float* texcoord_ptr            = (float*) texture_set_ddf->m_TexCoords.m_Data;

    if (num_geometries > 0)
    {
        float inv_tex_width  = 1.0f / tex_width;
        float inv_tex_height = 1.0f / tex_height;

        lua_getfield(L, -1, "geometries");
        for (int i = 0; i < num_geometries; ++i)
        {
            lua_pushnumber(L, i+1);
            lua_gettable(L, -2);

            dmGameSystemDDF::SpriteGeometry& geometry = texture_set_ddf->m_Geometries[i];
            MakeNumberArrayFromLuaTable<float>(L, "vertices", (void**) &geometry.m_Vertices.m_Data, &geometry.m_Vertices.m_Count);
            MakeNumberArrayFromLuaTable<float>(L, "uvs", (void**) &geometry.m_Uvs.m_Data, &geometry.m_Uvs.m_Count);
            MakeNumberArrayFromLuaTable<int>(L, "indices", (void**) &geometry.m_Indices.m_Data, &geometry.m_Indices.m_Count);

            lua_pop(L, 1);

            // Calculate extents so that we can transform to -0.5 .. 0.5 based
            // on the middle of the sprite
            float geo_width = 0.0f;
            float geo_height = 0.0f;
            for (int j = 0; j < geometry.m_Vertices.m_Count; j += 2)
            {
                geo_width  = dmMath::Max(geo_width, geometry.m_Vertices.m_Data[j]);
                geo_height = dmMath::Max(geo_height, geometry.m_Vertices.m_Data[j+1]);
            }

            geometry.m_Width  = geo_width;
            geometry.m_Height = geo_height;

            // Transform from texel to local space for position and uvs
            // Position and texcoords are flipped on y/t axis so that coordinates are
            // 0,0 in the top left corner, which is the same as the pipeline
            for (int j = 0; j < geometry.m_Vertices.m_Count; j += 2)
            {
                geometry.m_Vertices[j]     = geometry.m_Vertices[j] / geo_width - 0.5;
                geometry.m_Vertices[j + 1] = 1.0 - (geometry.m_Vertices[j + 1] / geo_height) - 0.5;
            }


            // For the UVs to work correctly if a component cannot use geometries,
            // we need to caluclate the bounding box of the texture coordinates.
            float min_uv_u = FLT_MAX;
            float min_uv_v = FLT_MAX;
            float max_uv_u = -FLT_MAX;
            float max_uv_v = -FLT_MAX;

            for (int j = 0; j < geometry.m_Uvs.m_Count; j += 2)
            {
                float u = geometry.m_Uvs[j] * inv_tex_width;
                float v = geometry.m_Uvs[j+1] * inv_tex_height;

                min_uv_u = dmMath::Min(u, min_uv_u);
                min_uv_v = dmMath::Min(v, min_uv_v);
                max_uv_u = dmMath::Max(u, max_uv_u);
                max_uv_v = dmMath::Max(v, max_uv_v);

                geometry.m_Uvs[j]     = u;
                geometry.m_Uvs[j + 1] = 1.0 - v;
            }

            // From texture_set_ddf.proto:
            // For unrotated quads, the order is: [(minU,maxV),(minU,minV),(maxU,minV),(maxU,maxV)]
            // Note that we need to invert the V coordinates here to account for the texture coordinate space
            // NOTE: We could perhaps do it in the loop above by swapping the V min and max coordinates
            geometry_scratch_cursor[0] = min_uv_u;
            geometry_scratch_cursor[1] = 1.0 - max_uv_v;

            geometry_scratch_cursor[2] = min_uv_u;
            geometry_scratch_cursor[3] = 1.0 - min_uv_v;

            geometry_scratch_cursor[4] = max_uv_u;
            geometry_scratch_cursor[5] = 1.0 - min_uv_v;

            geometry_scratch_cursor[6] = max_uv_u;
            geometry_scratch_cursor[7] = 1.0 - max_uv_v;

            geometry_scratch_cursor += 8;
            frame_index_count++;
        }
        lua_pop(L, 1); // geometries
    }

    if (num_animations > 0)
    {
        lua_getfield(L, -1, "animations");
        for (int i = 0; i < num_animations; ++i)
        {
            lua_pushnumber(L, i+1);
            lua_gettable(L, -2);

            dmGameSystemDDF::TextureSetAnimation& animation = texture_set_ddf->m_Animations[i];

            // Default values taken from texture_set_ddf->proto
            animation.m_Fps      = 30;
            animation.m_Playback = dmGameSystemDDF::PLAYBACK_ONCE_FORWARD;

            lua_getfield(L, -1, "id");
            animation.m_Id = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "width");
            animation.m_Width = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "height");
            animation.m_Height = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "frame_start");
            int frame_start = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "frame_end");
            int frame_end = lua_tointeger(L, -1);
            lua_pop(L, 1);

            // Get optional arguments
            lua_getfield(L, -1, "playback");
            if (lua_isnumber(L, -1))
            {
                animation.m_Playback = GameObjectPlaybackToDDFPlayback((dmGameObject::Playback) lua_tointeger(L,-1));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "fps");
            if (lua_isnumber(L, -1))
            {
                animation.m_Fps = lua_tointeger(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "flip_vertical");
            if (lua_isboolean(L, -1))
            {
                animation.m_FlipVertical = lua_toboolean(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "flip_horizontal");
            if (lua_isboolean(L, -1))
            {
                animation.m_FlipHorizontal = lua_toboolean(L, -1);
            }
            lua_pop(L, 1);

            lua_pop(L, 1);

            // Correct frame start/end
            animation.m_Start  = frame_start + num_geometries - 1;
            animation.m_End    = frame_end + num_geometries - 1;
            frame_index_count += frame_end - frame_start;
        }
        lua_pop(L, 1); // animations
    }

    texture_set_ddf->m_UseGeometries        = 1;
    texture_set_ddf->m_FrameIndices.m_Data  = new uint32_t[frame_index_count];
    texture_set_ddf->m_FrameIndices.m_Count = frame_index_count;
    memset(texture_set_ddf->m_FrameIndices.m_Data, 0, sizeof(uint32_t) * frame_index_count);

    // We can write all geometry UV bounds
    memcpy(texcoord_ptr, geometry_scratch_ptr, num_geometries * sizeof(float) * 8);
    texcoord_ptr += num_geometries * 8;

    uint32_t frame_index = 0;
    for (int i = 0; i < num_geometries; ++i)
    {
        texture_set_ddf->m_FrameIndices[frame_index++] = i;
    }

    for (int i = 0; i < texture_set_ddf->m_Animations.m_Count; ++i)
    {
        uint32_t frame_start = texture_set_ddf->m_Animations[i].m_Start;
        uint32_t frame_count = texture_set_ddf->m_Animations[i].m_End - frame_start;

        // Values stored in the frame indices table refer to entries in the
        // m_Geometry table of the DDF, so we need to adjust the values so
        // that the start and end values are based from zero because that is how
        // the indirection works when getting animations in e.g comp_sprite
        for (int j = 0; j < frame_count; ++j)
        {
            uint32_t animation_frame_index = frame_start + j - num_geometries;
            float* tc_read_ptr             = &geometry_scratch_ptr[animation_frame_index * 8];
            memcpy(texcoord_ptr, tc_read_ptr, sizeof(float) * 8);

            texcoord_ptr += 8;
            texture_set_ddf->m_FrameIndices[frame_index++] = animation_frame_index;
        }
    }

    delete[] geometry_scratch_ptr;

    assert(top == lua_gettop(L));
}

/*# create an atlas resource
 * This function creates a new atlas resource that can be used in the same way as any atlas created during build time.
 * The path used for creating the atlas must be unique, trying to create a resource at a path that is already
 * registered will trigger an error. If the intention is to instead modify an existing atlas, use the [ref:resource.set_atlas]
 * function. Also note that the path to the new atlas resource must have a '.texturesetc' extension,
 * meaning "/path/my_atlas" is not a valid path but "/path/my_atlas.texturesetc" is.
 *
 * When creating the atlas, at least one geometry and one animation is required, and an error will be
 * raised if these requirements are not met. A reference to the resource will be held by the collection
 * that created the resource and will automatically be released when that collection is destroyed.
 * Note that releasing a resource essentially means decreasing the reference count of that resource,
 * and not necessarily that it will be deleted.
 *
 * @name resource.create_atlas
 *
 * @param path [type:string] The path to the resource.
 * @param table [type:table] A table containing info about how to create the texture. Supported entries:
 *
 * * `texture`
 * : [type:string|hash] the path to the texture resource, e.g "/main/my_texture.texturec"
 *
 * * `animations`
 * : [type:table] a list of the animations in the atlas. Supports the following fields:
 *
 * * `id`
 * : [type:string] the id of the animation, used in e.g sprite.play_animation
 *
 * * `width`
 * : [type:integer] the width of the animation
 *
 * * `height`
 * : [type:integer] the height of the animation
 *
 * * `frame_start`
 * : [type:integer] index to the first geometry of the animation. Indices are lua based and must be in the range of 1 .. <number-of-geometries> in atlas.
 *
 * * `frame_end`
 * : [type:integer] index to the last geometry of the animation (non-inclusive). Indices are lua based and must be in the range of 1 .. <number-of-geometries> in atlas.
 *
 * * `playback`
 * : [type:constant] optional playback mode of the animation, the default value is [ref:go.PLAYBACK_ONCE_FORWARD]
 *
 * * `fps`
 * : [type:integer] optional fps of the animation, the default value is 30
 *
 * * `flip_vertical`
 * : [type:boolean] optional flip the animation vertically, the default value is false
 *
 * * `flip_horizontal`
 * : [type:boolean] optional flip the animation horizontally, the default value is false
 *
 * * `geometries`
 * : [type:table] A list of the geometries that should map to the texture data. Supports the following fields:
 *
 * * `vertices`
 * : [type:table] a list of the vertices in texture space of the geometry in the form {px0, py0, px1, py1, ..., pxn, pyn}
 *
 * * `uvs`
 * : [type:table] a list of the uv coordinates in texture space of the geometry in the form of {u0, v0, u1, v1, ..., un, vn}
 *
 * * `indices`
 * : [type:table] a list of the indices of the geometry in the form {i0, i1, i2, ..., in}. Each tripe in the list represents a triangle.
 *
 * @note The index values are zero based where zero refers to the first entry of the vertex and uv lists
 *
 * @return path [type:hash] Returns the atlas resource path
 *
 * @examples
 * Create a backing texture and an atlas
 *
 * ```lua
 * function init(self)
 *     -- create an empty texture
 *     local tparams = {
 *         width          = 128,
 *         height         = 128,
 *         type           = resource.TEXTURE_TYPE_2D,
 *         format         = resource.TEXTURE_FORMAT_RGBA,
 *     }
 *     local my_texture_id = resource.create_texture("/my_texture.texturec", tparams)
 *
 *     -- optionally use resource.set_texture to upload data to texture
 *
 *     -- create an atlas with one animation and one square geometry
 *     -- note that the function doesn't support hashes for the texture,
 *     -- you need to use a string for the texture path here aswell
 *     local aparams = {
 *         texture = "/my_texture.texturec",
 *         animations = {
 *             {
 *                 id          = "my_animation",
 *                 width       = 128,
 *                 height      = 128,
 *                 frame_start = 1,
 *                 frame_end   = 2,
 *             }
 *         },
 *         geometries = {
 *             {
 *                 vertices  = {
 *                     0,   0,
 *                     0,   128,
 *                     128, 128,
 *                     128, 0
 *                 },
 *                 uvs = {
 *                     0,   0,
 *                     0,   128,
 *                     128, 128,
 *                     128, 0
 *                 },
 *                 indices = {0,1,2,0,2,3}
 *             }
 *         }
 *     }
 *     local my_atlas_id = resource.create_atlas("/my_atlas.texturesetc", aparams)
 *
 *     -- assign the atlas to the 'sprite' component on the same go
 *     go.set("#sprite", "image", my_atlas_id)
 * end
 * ```
 */
static int CreateAtlas(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const char* path_str           = luaL_checkstring(L, 1);
    const char* texturec_ext       = ".texturesetc";
    const char* texture_field_name = "texture";

    dmhash_t canonical_path_hash = 0;
    PreCreateResource(L, path_str, texturec_ext, &canonical_path_hash);

    dmGameSystemDDF::TextureSet texture_set_ddf = {};

    // Validate arguments and get atlas data
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushvalue(L, 2);
        dmGraphics::HTexture texture;
        dmhash_t texture_path;
        CheckTextureResource(L, -1, texture_field_name, &texture_path, &texture);

        uint32_t num_geometries       = 0;
        uint32_t num_animations       = 0;
        uint32_t num_animation_frames = 0;

        // Note: We do a separate pass over the lua state to validate the data in the args table,
        //       this is because we need to allocate dynamic memory and can't use luaL_check** functions
        //       since they longjmp away so we can't release the memory..
        CheckAtlasArguments(L, &num_geometries, &num_animations, &num_animation_frames);

        MakeTextureSetFromLua(L, texture_path, texture, num_geometries, num_animations, num_animation_frames, &texture_set_ddf);

        lua_pop(L, 1); // args table
    }

    dmGameObject::HInstance sender_instance = dmScript::CheckGOInstance(L);
    dmGameObject::HCollection collection    = dmGameObject::GetCollection(sender_instance);

    dmArray<uint8_t> ddf_buffer;
    dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(&texture_set_ddf, dmGameSystemDDF::TextureSet::m_DDFDescriptor, ddf_buffer);
    assert(ddf_result == dmDDF::RESULT_OK);

    void* resource = 0x0;
    dmResource::Result res = dmResource::CreateResource(g_ResourceModule.m_Factory, path_str, ddf_buffer.Begin(), ddf_buffer.Size(), &resource);

    if (res != dmResource::RESULT_OK)
    {
        return ReportPathError(L, res, canonical_path_hash);
    }

    dmGameObject::AddDynamicResourceHash(collection, canonical_path_hash);
    dmScript::PushHash(L, canonical_path_hash);

    return 1;
}

/*# set atlas data
 * Sets the data for a specific atlas resource. Setting new atlas data is specified by passing in
 * a texture path for the backing texture of the atlas, a list of geometries and a list of animations
 * that map to the entries in the geometry list. The geometry entries are represented by three lists:
 * vertices, uvs and indices that together represent triangles that are used in other parts of the
 * engine to produce render objects from.
 *
 * Vertex and uv coordinates for the geometries are expected to be
 * in pixel coordinates where 0,0 is the top left corner of the texture.
 *
 * There is no automatic padding or margin support when setting custom data,
 * which could potentially cause filtering artifacts if used with a material sampler that has linear filtering.
 * If that is an issue, you need to calculate padding and margins manually before passing in the geometry data to
 * this function.
 *
 * @note Custom atlas data is not compatible with slice-9 for sprites
 *
 * @name resource.set_atlas
 *
 * @param path [type:hash|string] The path to the atlas resource
 * @param table [type:table] A table containing info about the atlas. Supported entries:
 *
 * * `texture`
 * : [type:string|hash] the path to the texture resource, e.g "/main/my_texture.texturec"
 *
 * * `animations`
 * : [type:table] a list of the animations in the atlas. Supports the following fields:
 *
 * * `id`
 * : [type:string] the id of the animation, used in e.g sprite.play_animation
 *
 * * `width`
 * : [type:integer] the width of the animation
 *
 * * `height`
 * : [type:integer] the height of the animation
 *
 * * `frame_start`
 * : [type:integer] index to the first geometry of the animation. Indices are lua based and must be in the range of 1 .. <number-of-geometries> in atlas.
 *
 * * `frame_end`
 * : [type:integer] index to the last geometry of the animation (non-inclusive). Indices are lua based and must be in the range of 1 .. <number-of-geometries> in atlas.
 *
 * * `playback`
 * : [type:constant] optional playback mode of the animation, the default value is [ref:go.PLAYBACK_ONCE_FORWARD]
 *
 * * `fps`
 * : [type:integer] optional fps of the animation, the default value is 30
 *
 * * `flip_vertical`
 * : [type:boolean] optional flip the animation vertically, the default value is false
 *
 * * `flip_horizontal`
 * : [type:boolean] optional flip the animation horizontally, the default value is false
 *
 * * `geometries`
 * : [type:table] A list of the geometries that should map to the texture data. Supports the following fields:
 *
 * * `vertices`
 * : [type:table] a list of the vertices in texture space of the geometry in the form {px0, py0, px1, py1, ..., pxn, pyn}
 *
 * * `uvs`
 * : [type:table] a list of the uv coordinates in texture space of the geometry in the form of {u0, v0, u1, v1, ..., un, vn}
 *
 * * `indices`
 * : [type:table] a list of the indices of the geometry in the form {i0, i1, i2, ..., in}. Each tripe in the list represents a triangle.
 *
 * @note The index values are zero based where zero refers to the first entry of the vertex and uv lists
 *
 * @examples
 * Add a new animation to an existing atlas
 *
 * ```lua
 * function init(self)
 *     local data = resource.get_atlas("/main/my_atlas.a.texturesetc")
 *     local my_animation = {
 *         id          = "my_new_animation",
 *         width       = 128,
 *         height      = 128,
 *         frame_start = 1,
 *         frame_end   = 6,
 *         playback    = go.PLAYBACK_LOOP_PINGPONG,
 *         fps         = 8
 *     }
 *     table.insert(data.animations, my_animation)
 *     resource.set_atlas("/main/my_atlas.a.texturesetc", data)
 * end
 * ```
 *
 * @examples
 * Sets atlas data for a 256x256 texture with a single animation being rendered as a quad
 *
 * ```lua
 * function init(self)
 *     local params = {
 *         texture = "/main/my_256x256_texture.texturec",
 *         animations = {
 *             {
 *                 id          = "my_animation",
 *                 width       = 256,
 *                 height      = 256,
 *                 frame_start = 1,
 *                 frame_end   = 2,
 *             }
 *         },
 *         geometries = {
 *             {
 *                 vertices = {
 *                     0,   0,
 *                     0,   256,
 *                     256, 256,
 *                     256, 0
 *                 },
 *                 uvs = {
 *                     0, 0,
 *                     0, 256,
 *                     256, 256,
 *                     256, 0
 *                 },
 *                 indices = { 0,1,2,0,2,3 }
 *             }
 *         }
 *     }
 *     resource.set_atlas("/main/test.a.texturesetc", params)
 * end
 * ```
 */

static int SetAtlas(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    CheckResource(L, g_ResourceModule.m_Factory, path_hash, "texturesetc");

    dmGameSystemDDF::TextureSet texture_set_ddf = {};
    uint32_t num_geometries                     = 0;
    uint32_t num_animations                     = 0;
    uint32_t num_animation_frames               = 0;

    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushvalue(L, 2);

    dmGraphics::HTexture texture;
    dmhash_t texture_path;
    CheckTextureResource(L, -1, "texture", &texture_path, &texture);

    // Note: We do a separate pass over the lua state to validate the data in the args table,
    //       this is because we need to allocate dynamic memory and can't use luaL_check** functions
    //       since they longjmp away so we can't release the memory..
    CheckAtlasArguments(L, &num_geometries, &num_animations, &num_animation_frames);

    MakeTextureSetFromLua(L, texture_path, texture, num_geometries, num_animations, num_animation_frames, &texture_set_ddf);
    lua_pop(L, 1); // args table

    dmArray<uint8_t> ddf_buffer;
    dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(&texture_set_ddf, dmGameSystemDDF::TextureSet::m_DDFDescriptor, ddf_buffer);
    assert(ddf_result == dmDDF::RESULT_OK);

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, ddf_buffer.Begin(), ddf_buffer.Size());

    DestroyTextureSet(texture_set_ddf);

    if(r != dmResource::RESULT_OK)
    {
        return ReportPathError(L, r, path_hash);
    }

    return 0;
}

/*# Get atlas data
 * Returns the atlas data for an atlas
 *
 * @name resource.get_atlas
 *
 * @param path [type:hash|string] The path to the atlas resource
 *
 * @return data [type:table] A table with the following entries:
 *
 * - texture
 * - geometries
 * - animations
 *
 * See [ref:resource.set_atlas] for a detailed description of each field
 *
 */
static int GetAtlas(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    TextureSetResource* texture_set_res      = (TextureSetResource*) CheckResource(L, g_ResourceModule.m_Factory, path_hash, "texturesetc");
    dmGameSystemDDF::TextureSet* texture_set = texture_set_res->m_TextureSet;
    assert(texture_set);

    float tex_width  = (float) dmGraphics::GetTextureWidth(texture_set_res->m_Texture->m_Texture);
    float tex_height = (float) dmGraphics::GetTextureHeight(texture_set_res->m_Texture->m_Texture);

    #define SET_LUA_TABLE_FIELD(set_fn, key, val) \
        set_fn(L, val); \
        lua_setfield(L, -2, key);

    lua_newtable(L);

    if (texture_set->m_TextureHash)
    {
        SET_LUA_TABLE_FIELD(dmScript::PushHash, "texture", texture_set->m_TextureHash);
    }
    else
    {
        SET_LUA_TABLE_FIELD(lua_pushstring, "texture", texture_set->m_Texture);
    }

    lua_pushliteral(L, "animations");
    lua_newtable(L);

    uint32_t num_geometries = texture_set->m_Geometries.m_Count;

    for (int i = 0; i < texture_set->m_Animations.m_Count; ++i)
    {
        dmGameSystemDDF::TextureSetAnimation& anim = texture_set->m_Animations[i];

        // Note:
        // frame_start and frame_end is not necessarily the same thing as an animations m_Start/m_End,
        // since we generate the geometry based on input textures. But now with the
        // resource.set_atlas function we allow creating arbitrary geometry and mapping
        // that to animations and have no concept of input textures.
        // So we need this indirection to be able to support both creating custom animations in runtime
        // and the way we have created the DDF in the build pipeline.
        uint32_t index_start = texture_set->m_FrameIndices[anim.m_Start];
        uint32_t index_end   = index_start + (anim.m_End - anim.m_Start);

        lua_pushinteger(L, (lua_Integer) (i+1));
        lua_newtable(L);

        SET_LUA_TABLE_FIELD(lua_pushstring, "id", anim.m_Id);
        SET_LUA_TABLE_FIELD(lua_pushinteger, "width", anim.m_Width);
        SET_LUA_TABLE_FIELD(lua_pushinteger, "height", anim.m_Height);
        SET_LUA_TABLE_FIELD(lua_pushinteger, "fps", anim.m_Fps);
        SET_LUA_TABLE_FIELD(lua_pushinteger, "playback", (lua_Integer) DDFPlaybackToGameObjectPlayback(anim.m_Playback));
        SET_LUA_TABLE_FIELD(lua_pushinteger, "frame_start", index_start + 1);
        SET_LUA_TABLE_FIELD(lua_pushinteger, "frame_end", index_end + 1);
        SET_LUA_TABLE_FIELD(lua_pushboolean, "flip_horizontal", anim.m_FlipHorizontal);
        SET_LUA_TABLE_FIELD(lua_pushboolean, "flip_vertical", anim.m_FlipVertical);

        lua_rawset(L, -3);
    }

    #undef SET_LUA_TABLE_FIELD

    lua_rawset(L, -3);

    {
        lua_pushliteral(L, "geometries");
        lua_newtable(L);

        for (int i = 0; i < num_geometries; ++i)
        {
            dmGameSystemDDF::SpriteGeometry& geom = texture_set->m_Geometries[i];

            lua_pushinteger(L, (lua_Integer) (i+1));
            lua_newtable(L);

            #define SET_LUA_TABLE_RAW(set_fn, key, val) \
                set_fn(L, val); \
                lua_rawseti(L, -2, key);

            {
                assert(geom.m_Vertices.m_Count % 2 == 0);
                assert(geom.m_Uvs.m_Count      % 2 == 0);
                assert(geom.m_Indices.m_Count  % 3 == 0);

                lua_pushliteral(L, "vertices");
                lua_newtable(L);
                for (int j = 0; j < geom.m_Vertices.m_Count; j += 2)
                {
                    float x = (geom.m_Vertices[j] + 0.5) * geom.m_Width;
                    float y = (0.5 - geom.m_Vertices[j+1]) * geom.m_Height;

                    SET_LUA_TABLE_RAW(lua_pushnumber, j + 1, x);
                    SET_LUA_TABLE_RAW(lua_pushnumber, j + 2, y);
                }
                lua_rawset(L, -3);

                lua_pushliteral(L, "uvs");
                lua_newtable(L);
                for (int j = 0; j < geom.m_Uvs.m_Count; j += 2)
                {
                    float s = geom.m_Uvs[j] * tex_width;
                    float t = (1.0 - geom.m_Uvs[j + 1]) * tex_height;

                    SET_LUA_TABLE_RAW(lua_pushnumber, j + 1, s);
                    SET_LUA_TABLE_RAW(lua_pushnumber, j + 2, t);
                }
                lua_rawset(L, -3);

                lua_pushliteral(L, "indices");
                lua_newtable(L);
                for (int j = 0; j < geom.m_Indices.m_Count; ++j)
                {
                    SET_LUA_TABLE_RAW(lua_pushinteger, j + 1, geom.m_Indices[j]);
                }
                lua_rawset(L, -3);
            }

            #undef SET_LUA_TABLE_RAW

            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }

    return 1;
}

/*# Update internal sound resource
 * Update internal sound resource (wavc/oggc) with new data
 *
 * @name resource.set_sound
 *
 * @param path [type:hash|string] The path to the resource
 * @param buffer [type:string] A lua string containing the binary sound data
 */
static int SetSound(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);

    // get resource path as hash
    const dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);
    // get the sound buffer
    luaL_checktype(L, 2, LUA_TSTRING);
    size_t buffer_size;
    const char* buffer = lua_tolstring(L, 2, &buffer_size);

    dmResource::Result r = dmResource::SetResource(g_ResourceModule.m_Factory, path_hash, (void*) buffer, buffer_size);

    if( r != dmResource::RESULT_OK ) {
        return ReportPathError(L, r, path_hash);
    }

    return 0;
}

/*# create a buffer resource
 * This function creates a new buffer resource that can be used in the same way as any buffer created during build time.
 * The function requires a valid buffer created from either [ref:buffer.create] or another pre-existing buffer resource.
 * By default, the new resource will take ownership of the buffer lua reference, meaning the buffer will not automatically be removed
 * when the lua reference to the buffer is garbage collected. This behaviour can be overruled by specifying 'transfer_ownership = false'
 * in the argument table. If the new buffer resource is created from a buffer object that is created by another resource,
 * the buffer object will be copied and the new resource will effectively own a copy of the buffer instead.
 *
 * Note that the path to the new resource must have the '.bufferc' extension, "/path/my_buffer" is not a valid path but "/path/my_buffer.bufferc" is.
 * The path must also be unique, attempting to create a buffer with the same name as an existing resource will raise an error.
 *
 * @name resource.create_buffer
 *
 * @param path [type:string] The path to the resource.
 * @param table [type:table] A table containing info about how to create the buffer. Supported entries:
 *
 * * `buffer`
 * : [type:buffer] the buffer to bind to this resource
 *
 * * `transfer_ownership`
 * : [type:boolean] optional flag to determine wether or not the resource should take over ownership of the buffer object (default true)
 *
 * @return path [type:hash] Returns the buffer resource path
 *
 * @examples
 * Create a buffer object and bind it to a buffer resource
 *
 * ```lua
 * function init(self)
 *     local size = 1
 *     local positions = {
 *         -- triangle 1
 *          size,  size, 0,
 *         -size, -size, 0,
 *          size, -size, 0,
 *         -- triangle 2
 *          size, size,  0,
 *         -size,  size, 0,
 *         -size, -size, 0,
 *     }
 *
 *     local buffer_handle = buffer.create(#positions, {
 *         {
 *             name  = hash("position"),
 *             type  = buffer.VALUE_TYPE_FLOAT32,
 *             count = 3
 *         }
 *     })
 *
 *     local stream = buffer.get_stream(buffer_handle, hash("position"))
 *
 *     -- transfer vertex data to buffer
 *     for k=1,#positions do
 *         stream[k] = positions[k]
 *     end
 *
 *     local my_buffer = resource.create_buffer("/my_buffer.bufferc", { buffer = buffer_handle })
 *     go.set("/go#mesh", "vertices", my_buffer)
 * end
 * ```
 *
 * @examples
 * Create a buffer resource from existing resource
 *
 * ```lua
 * function init(self)
 *     local res = resource.get_buffer("/my_buffer_path.bufferc")
 *     -- create a cloned buffer resource from another resource buffer
 *     local buf = reource.create_buffer("/my_cloned_buffer.bufferc", { buffer = res })
 *     -- assign cloned buffer to a mesh component
 *     go.set("/go#mesh", "vertices", buf)
 * end
 * ```
 */
static int CreateBuffer(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const char* path_str     = luaL_checkstring(L, 1);
    const char* resource_ext = ".bufferc";

    dmhash_t canonical_path_hash = 0;
    PreCreateResource(L, path_str, resource_ext, &canonical_path_hash);

    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushvalue(L, 2);

    lua_getfield(L, -1, "buffer");
    dmScript::LuaHBuffer* lua_buffer = dmScript::CheckBuffer(L, -1);
    lua_pop(L, 1); // "buffer"

    bool transfer_ownership = CheckFieldValue<bool>(L, -1, "transfer_ownership", true);

    lua_pop(L, 1); // args table

    dmGameObject::HInstance sender_instance = dmScript::CheckGOInstance(L);
    dmGameObject::HCollection collection    = dmGameObject::GetCollection(sender_instance);

    // JG: We have to do this awkwardness because the create functions for the resource
    //     dmDDF::LoadMessage won't accept an empty dmBufferDDF::BufferDesc structure
    //     and the buffer res won't accept a bufferdesc with a stream that doesn't have data
    int dummy_value_ptr = 0;
    dmBufferDDF::StreamDesc buffer_stream_dummy_ddf = {};
    buffer_stream_dummy_ddf.m_ValueType  = dmBufferDDF::VALUE_TYPE_INT8;
    buffer_stream_dummy_ddf.m_ValueCount = 1;
    buffer_stream_dummy_ddf.m_I.m_Data   = &dummy_value_ptr;
    buffer_stream_dummy_ddf.m_I.m_Count  = 1;

    dmBufferDDF::BufferDesc buffer_desc_dummy_ddf = {};
    buffer_desc_dummy_ddf.m_Streams.m_Data = &buffer_stream_dummy_ddf;
    buffer_desc_dummy_ddf.m_Streams.m_Count = 1;

    dmArray<uint8_t> ddf_buffer;
    dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(&buffer_desc_dummy_ddf, dmBufferDDF::BufferDesc::m_DDFDescriptor, ddf_buffer);
    assert(ddf_result == dmDDF::RESULT_OK);

    BufferResource* resource = 0x0;
    dmResource::Result resource_res = dmResource::CreateResource(g_ResourceModule.m_Factory, path_str, ddf_buffer.Begin(), ddf_buffer.Size(), (void**) &resource);

    if (resource_res != dmResource::RESULT_OK)
    {
        return ReportPathError(L, resource_res, canonical_path_hash);
    }

    dmBuffer::HBuffer buffer = dmGameSystem::UnpackLuaBuffer(lua_buffer);

    // Regardless of transferring ownership, if the lua buffer is referring to a resource,
    // we nee to copy that buffer into this new resource
    if (lua_buffer->m_Owner == dmScript::OWNER_RES)
    {
        dmBuffer::HBuffer dst_buffer;
        dmBuffer::Result br = dmBuffer::Clone(buffer, &dst_buffer);
        if (br != dmBuffer::RESULT_OK)
        {
            dmhash_t res_path_other;
            dmResource::GetPath(g_ResourceModule.m_Factory, lua_buffer->m_BufferRes, &res_path_other);
            return luaL_error(L, "Unable to create buffer resource '%s' from buffer resource '%s': %s (%d).",
                dmHashReverseSafe64(canonical_path_hash), dmHashReverseSafe64(res_path_other),
                dmBuffer::GetResultString(br), br);
        }

        buffer = dst_buffer;
    }

    resource->m_BufferDDF = 0;
    resource->m_Buffer    = buffer;
    resource->m_Stride    = dmBuffer::GetStructSize(buffer);

    dmBuffer::GetCount(buffer, &resource->m_ElementCount);
    dmBuffer::GetContentVersion(buffer, &resource->m_Version);

    if (transfer_ownership)
    {
        // IncRef if this is a lua buffer or a buffer from another resource,
        // otherwise the GC will potentially destroy the resource prematurely
        if (lua_buffer->m_Owner == dmScript::OWNER_LUA)
        {
            dmResource::IncRef(g_ResourceModule.m_Factory, resource);
        }
        else if (lua_buffer->m_Owner == dmScript::OWNER_RES)
        {
            // We are transferring the ownership of the resource in the lua buffer
            // from one resource to another, so we need to decref the old resource
            // and incref the new one by the same logic as with the lua case
            dmResource::Release(g_ResourceModule.m_Factory, lua_buffer->m_BufferRes);
            dmResource::IncRef(g_ResourceModule.m_Factory, resource);
        }

        lua_buffer->m_Owner     = dmScript::OWNER_RES;
        lua_buffer->m_BufferRes = resource;
    }

    dmGameObject::AddDynamicResourceHash(collection, canonical_path_hash);

    dmScript::PushHash(L, canonical_path_hash);

    return 1;
}

/*# get resource buffer
 * gets the buffer from a resource
 *
 * @name resource.get_buffer
 *
 * @param path [type:hash|string] The path to the resource
 * @return buffer [type:buffer] The resource buffer
 *
 * @examples
 * How to get the data from a buffer
 *
 * ```lua
 * function init(self)
 *
 *     local res_path = go.get("#mesh", "vertices")
 *     local buf = resource.get_buffer(res_path)
 *     local stream_positions = buffer.get_stream(buf, "position")
 *
 *     for i=1,#stream_positions do
 *         print(i, stream_positions[i])
 *     end
 * end
 * ```
 */
static int GetBuffer(lua_State* L)
{
    int top = lua_gettop(L);
    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    void* resource = CheckResource(L, g_ResourceModule.m_Factory, path_hash, "bufferc");

    dmGameSystem::BufferResource* buffer_resource = (dmGameSystem::BufferResource*)resource;

    if (!dmBuffer::IsBufferValid(buffer_resource->m_Buffer))
    {
        return luaL_error(L, "The buffer handle is invalid");
    }

    dmResource::IncRef(g_ResourceModule.m_Factory, buffer_resource);
    dmScript::LuaHBuffer luabuf((void*)buffer_resource);
    PushBuffer(L, luabuf);

    assert(top + 1 == lua_gettop(L));
    return 1;
}

/*# set resource buffer
 * Sets the buffer of a resource. By default, setting the resource buffer will either copy the data from the incoming buffer object
 * to the buffer stored in the destination resource, or make a new buffer object if the sizes between the source buffer and the destination buffer
 * stored in the resource differs. In some cases, e.g performance reasons, it might be beneficial to just set the buffer object on the resource without copying or cloning.
 * To achieve this, set the `transfer_ownership` flag to true in the argument table. Transferring ownership from a lua buffer to a resource with this function
 * works exactly the same as [ref:resource.create_buffer]: the destination resource will take ownership of the buffer held by the lua reference, i.e the buffer will not automatically be removed
 * when the lua reference to the buffer is garbage collected.
 * 
 * Note: When setting a buffer with `transfer_ownership = true`, the currently bound buffer in the resource will be destroyed.
 *
 * @name resource.set_buffer
 *
 * @param path [type:hash|string] The path to the resource
 * @param buffer [type:buffer] The resource buffer
 * @param table [type:table] A table containing info about how to set the buffer. Supported entries:
 *
 * * `transfer_ownership`
 * : [type:boolean] optional flag to determine wether or not the resource should take over ownership of the buffer object (default false)
 *
 * @examples
 * How to set the data from a buffer
 *
 * ```lua
 * local function fill_stream(stream, verts)
 *     for key, value in ipairs(verts) do
 *         stream[key] = verts[key]
 *     end
 * end
 *
 * function init(self)
 *
 *     local res_path = go.get("#mesh", "vertices")
 *
 *     local positions = {
 *          1, -1, 0,
 *          1,  1, 0,
 *          -1, -1, 0
 *     }
 *
 *     local num_verts = #positions / 3
 *
 *     -- create a new buffer
 *     local buf = buffer.create(num_verts, {
 *         { name = hash("position"), type=buffer.VALUE_TYPE_FLOAT32, count = 3 }
 *     })
 *
 *     local buf = resource.get_buffer(res_path)
 *     local stream_positions = buffer.get_stream(buf, "position")
 *
 *     fill_stream(stream_positions, positions)
 *
 *     resource.set_buffer(res_path, buf)
 * end
 * ```
 */
static int SetBuffer(lua_State* L)
{
    int top = lua_gettop(L);
    dmhash_t path_hash           = dmScript::CheckHashOrString(L, 1);
    dmScript::LuaHBuffer* luabuf = dmScript::CheckBuffer(L, 2);
    bool transfer_ownership      = false;

    if (lua_istable(L, 3))
    {
        lua_pushvalue(L, 3);
        transfer_ownership = CheckFieldValue<bool>(L, -1, "transfer_ownership", false);
        lua_pop(L, 1); // args table
    }

    dmBuffer::HBuffer src_buffer                  = dmGameSystem::UnpackLuaBuffer(luabuf);
    void* resource                                = CheckResource(L, g_ResourceModule.m_Factory, path_hash, "bufferc");
    dmGameSystem::BufferResource* buffer_resource = (dmGameSystem::BufferResource*)resource;
    dmBuffer::HBuffer dst_buffer                  = buffer_resource->m_Buffer;

    if (transfer_ownership)
    {
        if (src_buffer != dst_buffer)
        {
            uint32_t src_count = 0;
            dmBuffer::Result br = dmBuffer::GetCount(src_buffer, &src_count);
            if (br != dmBuffer::RESULT_OK)
            {
                return luaL_error(L, "Unable to get buffer size for source buffer: %s (%d).", dmBuffer::GetResultString(br), br);
            }

            dmBuffer::Destroy(buffer_resource->m_Buffer);
            buffer_resource->m_Buffer       = src_buffer;
            buffer_resource->m_ElementCount = src_count;
            buffer_resource->m_Stride       = dmBuffer::GetStructSize(src_buffer);

            if (luabuf->m_Owner == dmScript::OWNER_RES)
            {
                // We are transferring the ownership of the resource in the lua buffer
                // to the destination resource, so we decref the source resource.
                // We don't need to incref the destination resource in this case,
                // because we are simply updating the content and not the resoure itself
                dmResource::Release(g_ResourceModule.m_Factory, luabuf->m_BufferRes);
            }
        }

        luabuf->m_Owner     = dmScript::OWNER_RES;
        luabuf->m_BufferRes = resource;
    }
    else
    {
        // Make sure the destination buffer has enough size (otherwise, resize it).
        // TODO: Check if incoming buffer size is smaller than current size -> don't allocate new dmbuffer,
        //       but copy smaller data and change "size".
        uint32_t dst_count = 0;
        dmBuffer::Result br = dmBuffer::GetCount(dst_buffer, &dst_count);
        if (br != dmBuffer::RESULT_OK)
        {
            return luaL_error(L, "Unable to get buffer size for %s: %s (%d).", dmHashReverseSafe64(path_hash), dmBuffer::GetResultString(br), br);
        }

        uint32_t src_count = 0;
        br = dmBuffer::GetCount(src_buffer, &src_count);
        if (br != dmBuffer::RESULT_OK)
        {
            return luaL_error(L, "Unable to get buffer size for source buffer: %s (%d).", dmBuffer::GetResultString(br), br);
        }

        if (dst_count != src_count)
        {
            dmBuffer::HBuffer dst_buffer;
            br = dmBuffer::Clone(src_buffer, &dst_buffer);
            if (br != dmBuffer::RESULT_OK)
            {
                return luaL_error(L, "Unable to create cloned buffer: %s (%d)", dmBuffer::GetResultString(br), br);
            }

            dmBuffer::Destroy(buffer_resource->m_Buffer);
            buffer_resource->m_Buffer       = dst_buffer;
            buffer_resource->m_ElementCount = src_count;
        }
        else
        {
            br = dmBuffer::Copy(dst_buffer, src_buffer);
            if (br != dmBuffer::RESULT_OK)
            {
                return luaL_error(L, "Could not copy data from buffer: %s (%d).", dmBuffer::GetResultString(br), br);
            }
        }
    }

    // Update the content version
    dmBuffer::UpdateContentVersion(dst_buffer);
    dmBuffer::GetContentVersion(buffer_resource->m_Buffer, &buffer_resource->m_Version);
    buffer_resource->m_NameHash = path_hash;

    assert(top == lua_gettop(L));
    return 0;
}

static void PushTextMetricsTable(lua_State* L, const dmRender::TextMetrics* metrics)
{
    lua_createtable(L, 0, 4);
    lua_pushliteral(L, "width");
    lua_pushnumber(L, metrics->m_Width);
    lua_rawset(L, -3);
    lua_pushliteral(L, "height");
    lua_pushnumber(L, metrics->m_Height);
    lua_rawset(L, -3);
    lua_pushliteral(L, "max_ascent");
    lua_pushnumber(L, metrics->m_MaxAscent);
    lua_rawset(L, -3);
    lua_pushliteral(L, "max_descent");
    lua_pushnumber(L, metrics->m_MaxDescent);
    lua_rawset(L, -3);
}

/*#  gets the text metrics for a font
 *
 * Gets the text metrics from a font
 *
 * @name resource.get_text_metrics
 * @param url [type:hash] the font to get the (unscaled) metrics from
 * @param text [type:string] text to measure
 * @param [options] [type:table] A table containing parameters for the text. Supported entries:
 *
 * `width`
 * : [type:integer] The width of the text field. Not used if `line_break` is false.
 *
 * `leading`
 * : [type:number] The leading (default 1.0)
 *
 * `tracking`
 * : [type:number] The tracking (default 0.0)
 *
 * `line_break`
 * : [type:boolean] If the calculation should consider line breaks (default false)
 *
 * @return metrics [type:table] a table with the following fields:
 *
 * - width
 * - height
 * - max_ascent
 * - max_descent
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *     local font = go.get("#label", "font")
 *     local metrics = resource.get_text_metrics(font, "The quick brown fox\n jumps over the lazy dog")
 *     pprint(metrics)
 * end
 * ```
 */
static int GetTextMetrics(lua_State* L)
{
    int top = lua_gettop(L);

    dmhash_t path_hash = dmScript::CheckHashOrString(L, 1);

    size_t len = 0;
    const char* text = luaL_checklstring(L, 2, &len);

    dmRender::HFontMap font_map = (dmRender::HFontMap)CheckResource(L, g_ResourceModule.m_Factory, path_hash, "fontc");;

    bool line_break = false;
    float leading = 1.0f;
    float tracking = 0.0f;
    float width = 100000.0f;
    if (top >= 3)
    {
        int table_index = 3;
        luaL_checktype(L, table_index, LUA_TTABLE); // Make sure it's actually a table
        width = (float)CheckTableNumber(L, table_index, "width", width);
        leading = (float)CheckTableNumber(L, table_index, "leading", leading);
        tracking = (float)CheckTableNumber(L, table_index, "tracking", tracking);
        line_break = CheckTableBoolean(L, table_index, "line_break", line_break);
    }

    dmRender::TextMetrics metrics;
    dmRender::GetTextMetrics(font_map, text, width, line_break, leading, tracking, &metrics);
    PushTextMetricsTable(L, &metrics);
    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"set", Set},
    {"load", Load},
    {"create_atlas", CreateAtlas},
    {"create_buffer", CreateBuffer},
    {"create_texture", CreateTexture},
    {"release", ReleaseResource},
    {"set_atlas", SetAtlas},
    {"get_atlas", GetAtlas},
    {"set_texture", SetTexture},
    {"get_texture_info", GetTextureInfo},
    {"set_sound", SetSound},
    {"get_buffer", GetBuffer},
    {"set_buffer", SetBuffer},
    {"get_text_metrics", GetTextMetrics},
    {0, 0}
};

/*# 2D texture type
 *
 * @name resource.TEXTURE_TYPE_2D
 * @variable
 */

/*# Cube map texture type
 *
 * @name resource.TEXTURE_TYPE_CUBE_MAP
 * @variable
 */

/*# 2D Array texture type
 *
 * @name resource.TEXTURE_TYPE_2D_ARRAY
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

/*# RGB_PVRTC_2BPPV1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
 * @variable
 */

/*# RGB_PVRTC_4BPPV1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
 * @variable
 */

/*# RGBA_PVRTC_2BPPV1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
 * @variable
 */

/*# RGBA_PVRTC_4BPPV1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
 * @variable
 */

/*# RGB_ETC1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB_ETC1
 * @variable
 */

/*# RGBA_ETC2 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_ETC2
 * @variable
 */

/*# RGBA_ASTC_4x4 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_ASTC_4x4
 * @variable
 */

/*# RGB_BC1 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB_BC1
 * @variable
 */

/*# RGBA_BC3 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_BC3
 * @variable
 */

/*# R_BC4 type texture format
 *
 * @name resource.TEXTURE_FORMAT_R_BC4
 * @variable
 */

/*# RG_BC5 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RG_BC5
 * @variable
 */

/*# RGBA_BC7 type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA_BC7
 * @variable
 */

/*# RGB16F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB16F
 * @variable
 */

/*# RGB32F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGB32F
 * @variable
 */

/*# RGBA16F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA16F
 * @variable
 */

/*# RGBA32F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RGBA32F
 * @variable
 */

/*# R16F type texture format
 *
 * @name resource.TEXTURE_FORMAT_R16F
 * @variable
 */

/*# RG16F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RG16F
 * @variable
 */

/*# R32F type texture format
 *
 * @name resource.TEXTURE_FORMAT_R32F
 * @variable
 */

/*# RG32F type texture format
 *
 * @name resource.TEXTURE_FORMAT_RG32F
 * @variable
 */

/*# COMPRESSION_TYPE_DEFAULT compression type
 *
 * @name resource.COMPRESSION_TYPE_DEFAULT
 * @variable
 */

/*# BASIS_UASTC compression type
 *
 * @name resource.COMPRESSION_TYPE_BASIS_UASTC
 * @variable
 */

static void LuaInit(lua_State* L, dmGraphics::HContext graphics_context)
{
    int top = lua_gettop(L);
    luaL_register(L, "resource", Module_methods);

#define SETGRAPHICS_ENUM(name) \
    lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
    lua_setfield(L, -2, #name);

    SETGRAPHICS_ENUM(TEXTURE_TYPE_2D);
    SETGRAPHICS_ENUM(TEXTURE_TYPE_CUBE_MAP);
    SETGRAPHICS_ENUM(TEXTURE_TYPE_2D_ARRAY);
#undef SETGRAPHICS_ENUM

#define SETTEXTUREFORMAT_IF_SUPPORTED(name) \
    if (graphics_context != 0 && dmGraphics::IsTextureFormatSupported(graphics_context, dmGraphics::name)) \
    { \
        lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
        lua_setfield(L, -2, #name); \
    }

    // JG: Perhaps these should be in a 'graphics' namespace shared with the render script,
    //     feels a bit strange to have different modules that expose the same enums but with different names.
    //     In the render scripts we call them "FORMAT_RGB16F" but here it's "TEXTURE_FORMAT_RGB16F", and we only expose
    //     a couple of select formats in render scripts. Would be nice to have a single point of exposure for these things
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_LUMINANCE);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_DEPTH);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_STENCIL);

    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_ETC1);

    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_ETC2);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_ASTC_4x4);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_BC1);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_BC3);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R_BC4);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG_BC5);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_BC7);

    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB16F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB32F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA16F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA32F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R16F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG16F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R32F);
    SETTEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG32F);

#undef SETTEXTUREFORMAT_IF_SUPPORTED


#define SETCOMPRESSIONTYPE(name) \
    lua_pushnumber(L, (lua_Number) dmGraphics::TextureImage:: name); \
    lua_setfield(L, -2, #name);

    SETCOMPRESSIONTYPE(COMPRESSION_TYPE_DEFAULT);
    SETCOMPRESSIONTYPE(COMPRESSION_TYPE_BASIS_UASTC);

#undef SETCOMPRESSIONTYPE

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

void ScriptResourceRegister(const ScriptLibContext& context)
{
    LuaInit(context.m_LuaState, context.m_GraphicsContext);
    g_ResourceModule.m_Factory         = context.m_Factory;
    g_ResourceModule.m_GraphicsContext = context.m_GraphicsContext;
}

void ScriptResourceFinalize(const ScriptLibContext& context)
{
}

} // namespace dmGameSystem
