// Copyright 2020-2022 The Defold Foundation
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

#include "gui.h"

#include <string.h>
#include <new>
#include <algorithm>

#include <dlib/dlib.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <dlib/transform.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/trig_lookup.h>

#include <script/script.h>
#include <script/lua_source_ddf.h>

#include <render/display_profiles.h>

#include "gui_private.h"
#include "gui_script.h"

DM_PROPERTY_U32(rmtp_Gui, 0, FrameReset, "");
DM_PROPERTY_U32(rmtp_GuiAnimations, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiActiveAnimations, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiNodes, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiActiveNodes, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiStaticTextures, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiDynamicTextures, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiTextures, 0, FrameReset, "", &rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiParticlefx, 0, FrameReset, "", &rmtp_Gui);

namespace dmGui
{
    using namespace dmVMath;

    /**
     * Default layer id
     */
    const dmhash_t DEFAULT_LAYER = dmHashString64("");

    const dmhash_t DEFAULT_LAYOUT = dmHashString64("");

    const uint16_t INVALID_INDEX = 0xffff;

    const uint64_t INVALID_RENDER_KEY = 0xffffffffffffffff;

    const uint32_t INITIAL_SCENE_COUNT = 32;

    const uint64_t LAYER_RANGE = 4; // 16 layers
    const uint64_t INDEX_RANGE = 13; // 8192 nodes
    const uint64_t CLIPPER_RANGE = 8;

    const uint64_t SUB_INDEX_SHIFT = 0;
    const uint64_t SUB_LAYER_SHIFT = INDEX_RANGE;
    const uint64_t CLIPPER_SHIFT = SUB_LAYER_SHIFT + LAYER_RANGE;
    const uint64_t INDEX_SHIFT = CLIPPER_SHIFT + CLIPPER_RANGE;
    const uint64_t LAYER_SHIFT = INDEX_SHIFT + INDEX_RANGE;


    static inline void UpdateTextureSetAnimData(HScene scene, InternalNode* n);
    static inline Animation* GetComponentAnimation(HScene scene, HNode node, float* value);
    static inline void ResetInternalNode(HScene scene, InternalNode* n);
    static void RemoveFromNodeList(HScene scene, InternalNode* n);

    static const char* SCRIPT_FUNCTION_NAMES[] =
    {
        "init",
        "final",
        "update",
        "on_message",
        "on_input",
        "on_reload"
    };

#define PROP(name, prop)\
    { dmHashString64(#name), prop, 0xff }, \
    { dmHashString64(#name ".x"), prop, 0 }, \
    { dmHashString64(#name ".y"), prop, 1 }, \
    { dmHashString64(#name ".z"), prop, 2 }, \
    { dmHashString64(#name ".w"), prop, 3 },

    struct PropDesc
    {
        dmhash_t m_Hash;
        Property m_Property;
        uint8_t  m_Component;
    };

    PropDesc g_Properties[] = {
            PROP(position, PROPERTY_POSITION )
            PROP(rotation, PROPERTY_ROTATION )
            PROP(scale, PROPERTY_SCALE )
            PROP(color, PROPERTY_COLOR )
            PROP(size, PROPERTY_SIZE )
            PROP(outline, PROPERTY_OUTLINE )
            PROP(shadow, PROPERTY_SHADOW )
            PROP(slice9, PROPERTY_SLICE9 )
            { dmHashString64("inner_radius"), PROPERTY_PIE_PARAMS, 0 },
            { dmHashString64("fill_angle"), PROPERTY_PIE_PARAMS, 1 },
            { dmHashString64("leading"), PROPERTY_TEXT_PARAMS, 0 },
            { dmHashString64("tracking"), PROPERTY_TEXT_PARAMS, 1 },
    };
#undef PROP

    PropDesc g_PropTable[] = {
            { dmHashString64("position"), PROPERTY_POSITION, 0xff },
            { dmHashString64("rotation"), PROPERTY_ROTATION, 0xff },
            { dmHashString64("scale"), PROPERTY_SCALE, 0xff },
            { dmHashString64("color"), PROPERTY_COLOR, 0xff },
            { dmHashString64("size"), PROPERTY_SIZE, 0xff },
            { dmHashString64("outline"), PROPERTY_OUTLINE, 0xff },
            { dmHashString64("shadow"), PROPERTY_SHADOW, 0xff },
            { dmHashString64("slice"), PROPERTY_SLICE9, 0xff },
    };

    static PropDesc* GetPropertyDesc(dmhash_t property_hash)
    {
        int n_props = sizeof(g_Properties) / sizeof(g_Properties[0]);
        for (int i = 0; i < n_props; ++i) {
            PropDesc* pd = &g_Properties[i];
            if (pd->m_Hash == property_hash) {
                return pd;
            }
        }
        return 0;
    }

    TextMetrics::TextMetrics()
    {
        memset(this, 0, sizeof(TextMetrics));
    }

    InputAction::InputAction()
    {
        memset(this, 0, sizeof(InputAction));
    }

    bool IsNodeValid(HScene scene, HNode node)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        if (index >= scene->m_Nodes.Size())
            return false;
        InternalNode* n = &scene->m_Nodes[index];
        return n->m_Version == version && n->m_Index == index;
    }

    InternalNode* GetNode(HScene scene, HNode node)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);
        assert(n->m_Index == index);
        return n;
    }

    HContext NewContext(const NewContextParams* params)
    {
        Context* context = new Context();
        context->m_LuaState = InitializeScript(params->m_ScriptContext);
        context->m_GetURLCallback = params->m_GetURLCallback;
        context->m_GetUserDataCallback = params->m_GetUserDataCallback;
        context->m_ResolvePathCallback = params->m_ResolvePathCallback;
        context->m_GetTextMetricsCallback = params->m_GetTextMetricsCallback;
        context->m_DefaultProjectWidth = params->m_DefaultProjectWidth;
        context->m_DefaultProjectHeight = params->m_DefaultProjectHeight;
        context->m_PhysicalWidth = params->m_PhysicalWidth;
        context->m_PhysicalHeight = params->m_PhysicalHeight;
        context->m_Dpi = params->m_Dpi;
        context->m_Scenes.SetCapacity(INITIAL_SCENE_COUNT);
        context->m_ScratchBoneNodes.SetCapacity(32);

        return context;
    }

    void DeleteContext(HContext context, dmScript::HContext script_context)
    {
        FinalizeScript(context->m_LuaState, script_context);
        delete context;
    }

    void GetPhysicalResolution(HContext context, uint32_t& width, uint32_t& height)
    {
        width = context->m_PhysicalWidth;
        height = context->m_PhysicalHeight;
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        return context->m_Dpi;
    }

    void GetSceneResolution(HScene scene, uint32_t& width, uint32_t& height)
    {
        width = scene->m_Width;
        height = scene->m_Height;
    }

    void SetSceneResolution(HScene scene, uint32_t width, uint32_t height)
    {
        scene->m_Width = width;
        scene->m_Height = height;
        scene->m_ResChanged = 1;
    }

    void GetPhysicalResolution(HScene scene, uint32_t& width, uint32_t& height)
    {
        width = scene->m_Context->m_PhysicalWidth;
        height = scene->m_Context->m_PhysicalHeight;
    }

    uint32_t GetDisplayDpi(HScene scene)
    {
        return scene->m_Context->m_Dpi;
    }

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height)
    {
        context->m_PhysicalWidth = width;
        context->m_PhysicalHeight = height;
        dmArray<HScene>& scenes = context->m_Scenes;
        uint32_t scene_count = scenes.Size();

        for (uint32_t i = 0; i < scene_count; ++i)
        {
            Scene* scene = scenes[i];
            scene->m_ResChanged = 1;
            if(scene->m_OnWindowResizeCallback)
            {
                scene->m_OnWindowResizeCallback(scene, width, height);
            }
        }
    }

    void GetDefaultResolution(HContext context, uint32_t& width, uint32_t& height)
    {
        width = context->m_DefaultProjectWidth;
        height = context->m_DefaultProjectHeight;
    }

    void SetDefaultResolution(HContext context, uint32_t width, uint32_t height)
    {
        context->m_DefaultProjectWidth = width;
        context->m_DefaultProjectHeight = height;
    }

    void* GetDisplayProfiles(HScene scene)
    {
        return scene->m_Context->m_DisplayProfiles;
    }

    AdjustReference GetSceneAdjustReference(HScene scene)
    {
        return scene->m_AdjustReference;
    }

    void SetDisplayProfiles(HContext context, void* display_profiles)
    {
        context->m_DisplayProfiles = display_profiles;
    }

    void SetDefaultFont(HContext context, void* font)
    {
        context->m_DefaultFont = font;
    }

    void SetSceneAdjustReference(HScene scene, AdjustReference adjust_reference)
    {
        scene->m_AdjustReference = adjust_reference;
    }

    void SetDefaultNewSceneParams(NewSceneParams* params)
    {
        memset(params, 0, sizeof(*params));
        // The default max value for a scene is 512 (same as in gui_ddf.proto). Absolute max value is 2^INDEX_RANGE.
        params->m_MaxNodes = 512;
        params->m_MaxAnimations = 128;
        params->m_MaxTextures = 32;
        params->m_MaxFonts = 4;
        params->m_MaxParticlefxs = 128;
        // 16 is a hard cap for max layers since only 4 bits is available in the render key (see LAYER_RANGE above)
        params->m_MaxLayers = 16;
        params->m_AdjustReference = dmGui::ADJUST_REFERENCE_LEGACY;

        params->m_ScriptWorld = 0x0;
    }

    static void ResetScene(HScene scene) {
        memset(scene, 0, sizeof(Scene));
        scene->m_InstanceReference = LUA_NOREF;
        scene->m_DataReference = LUA_NOREF;
        scene->m_ContextTableReference = LUA_NOREF;
    }

    HScene NewScene(HContext context, const NewSceneParams* params)
    {
        lua_State* L = context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = (Scene*)lua_newuserdata(L, sizeof(Scene));
        ResetScene(scene);

        dmArray<HScene>& scenes = context->m_Scenes;
        if (scenes.Full())
        {
            scenes.SetCapacity(scenes.Capacity() + INITIAL_SCENE_COUNT);
        }
        scenes.Push(scene);

        lua_pushvalue(L, -1);
        scene->m_InstanceReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        // Here we create a custom table to hold the references created by this gui scene
        // It is also the "Instance Context Table" used to by META_TABLE_SET_CONTEXT_VALUE
        // and META_TABLE_GET_CONTEXT_VALUE implementaion
        lua_newtable(L);
        scene->m_ContextTableReference = dmScript::Ref(L, LUA_REGISTRYINDEX);

        lua_newtable(L);
        scene->m_DataReference = dmScript::Ref(L, LUA_REGISTRYINDEX);

        scene->m_Context = context;
        scene->m_Script = 0x0;
        scene->m_ParticlefxContext = params->m_ParticlefxContext;
        scene->m_Nodes.SetCapacity(params->m_MaxNodes);
        scene->m_NodePool.SetCapacity(params->m_MaxNodes);
        scene->m_Animations.SetCapacity(params->m_MaxAnimations);
        scene->m_Textures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_DynamicTextures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_Fonts.SetCapacity(params->m_MaxFonts*2, params->m_MaxFonts);
        scene->m_Particlefxs.SetCapacity(params->m_MaxParticlefxs*2, params->m_MaxParticlefxs);
        scene->m_AliveParticlefxs.SetCapacity(params->m_MaxParticlefx);
        scene->m_Layers.SetCapacity(params->m_MaxLayers*2, params->m_MaxLayers);
        scene->m_Layouts.SetCapacity(1);
        scene->m_AdjustReference = params->m_AdjustReference;
        scene->m_DefaultFont = 0;
        scene->m_UserData = params->m_UserData;
        scene->m_RenderHead = INVALID_INDEX;
        scene->m_RenderTail = INVALID_INDEX;
        scene->m_NextVersionNumber = 0;
        scene->m_RenderOrder = 0;
        scene->m_Width = context->m_DefaultProjectWidth;
        scene->m_Height = context->m_DefaultProjectHeight;
        scene->m_FetchTextureSetAnimCallback = params->m_FetchTextureSetAnimCallback;
        scene->m_CreateCustomNodeCallback = params->m_CreateCustomNodeCallback;
        scene->m_DestroyCustomNodeCallback = params->m_DestroyCustomNodeCallback;
        scene->m_CloneCustomNodeCallback = params->m_CloneCustomNodeCallback;
        scene->m_UpdateCustomNodeCallback = params->m_UpdateCustomNodeCallback;
        scene->m_CreateCustomNodeCallbackContext = params->m_CreateCustomNodeCallbackContext;
        scene->m_GetResourceCallback = params->m_GetResourceCallback;
        scene->m_GetResourceCallbackContext = params->m_GetResourceCallbackContext;
        scene->m_OnWindowResizeCallback = params->m_OnWindowResizeCallback;
        scene->m_ScriptWorld = params->m_ScriptWorld;

        scene->m_Layers.Put(DEFAULT_LAYER, scene->m_NextLayerIndex++);

        ClearLayouts(scene);

        luaL_getmetatable(L, GUI_SCRIPT_INSTANCE);
        lua_setmetatable(L, -2);

        dmScript::SetInstance(L);
        dmScript::InitializeInstance(scene->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));

        return scene;
    }

    void DeleteScene(HScene scene)
    {
        lua_State*L = scene->m_Context->m_LuaState;

        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
        dmScript::SetInstance(L);
        dmScript::FinalizeInstance(scene->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* n = &nodes[i];

            if (n->m_Node.m_CustomType != 0)
            {
                scene->m_DestroyCustomNodeCallback(scene->m_CreateCustomNodeCallbackContext, scene, GetNodeHandle(n), n->m_Node.m_CustomType, n->m_Node.m_CustomData);
            }

            if (n->m_Node.m_Text)
                free((void*) n->m_Node.m_Text);
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, scene->m_DataReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);

        dmArray<HScene>& scenes = scene->m_Context->m_Scenes;
        uint32_t scene_count = scenes.Size();
        for (uint32_t i = 0; i < scene_count; ++i)
        {
            if (scenes[i] == scene)
            {
                scenes.EraseSwap(i);
                break;
            }
        }

        scene->~Scene();

        ResetScene(scene);
    }

    void SetSceneUserData(HScene scene, void* userdata)
    {
        scene->m_UserData = userdata;
    }

    void* GetSceneUserData(HScene scene)
    {
        return scene->m_UserData;
    }

    Result AddTexture(HScene scene, dmhash_t texture_name_hash, void* texture, NodeTextureType texture_type, uint32_t original_width, uint32_t original_height)
    {
        if (scene->m_Textures.Full())
            return RESULT_OUT_OF_RESOURCES;

        scene->m_Textures.Put(texture_name_hash, TextureInfo(texture, texture_type, original_width, original_height));
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_TextureHash == texture_name_hash)
            {
                nodes[i].m_Node.m_Texture     = texture;
                nodes[i].m_Node.m_TextureType = texture_type;
            }
        }
        return RESULT_OK;
    }

    void RemoveTexture(HScene scene, dmhash_t texture_name_hash)
    {
        scene->m_Textures.Erase(texture_name_hash);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            Node& node = nodes[i].m_Node;
            if (node.m_TextureHash == texture_name_hash)
            {
                if (node.m_TextureType == NODE_TEXTURE_TYPE_TEXTURE_SET)
                {
                    CancelNodeFlipbookAnim(scene, GetNodeHandle(&nodes[i]));
                }

                node.m_Texture     = 0;
                node.m_TextureType = NODE_TEXTURE_TYPE_NONE;
            }
        }
    }

    void ClearTextures(HScene scene)
    {
        scene->m_Textures.Clear();
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            Node& node = nodes[i].m_Node;
            if (node.m_TextureType == NODE_TEXTURE_TYPE_TEXTURE_SET)
            {
                CancelNodeFlipbookAnim(scene, GetNodeHandle(&nodes[i]));
            }
            node.m_Texture = 0;
            node.m_TextureType = NODE_TEXTURE_TYPE_NONE;
        }
    }

    void* GetTexture(HScene scene, dmhash_t texture_name_hash)
    {
        TextureInfo* textureInfo = scene->m_Textures.Get(texture_name_hash);
        if (!textureInfo)
        {
            return 0;
        }
        return textureInfo->m_TextureSource;
    }

    static bool CopyImageBufferFlipped(uint32_t width, uint32_t height, const uint8_t* buffer, uint32_t buffer_size, dmImage::Type type, uint8_t* out_buffer)
    {
        uint32_t stride = width*sizeof(uint8_t);
        if (type == dmImage::TYPE_RGB) {
            stride *= 3;
        } else if (type == dmImage::TYPE_RGBA) {
            stride *= 4;
        }

        if (stride*height != buffer_size) {
            dmLogError("Invalid data size when flipping image buffer.");
            return false;
        }

        const uint8_t* read_buffer = buffer+buffer_size;
        for (uint32_t y = 0; y < height; ++y)
        {
            read_buffer -= stride;
            memcpy(out_buffer, read_buffer, stride);
            out_buffer += stride;
        }

        return true;
    }

    Result NewDynamicTexture(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, bool flip, const void* buffer, uint32_t buffer_size)
    {
        uint32_t expected_buffer_size = width * height * dmImage::BytesPerPixel(type);
        if (buffer_size != expected_buffer_size) {
            dmLogError("Invalid image buffer size. Expected %d, got %d", expected_buffer_size, buffer_size);
            return RESULT_INVAL_ERROR;
        }

        if (DynamicTexture* t = scene->m_DynamicTextures.Get(texture_hash)) {
            if (t->m_Deleted) {
                t->m_Deleted = 0;
                return RESULT_OK;
            } else {
                return RESULT_TEXTURE_ALREADY_EXISTS;
            }
        }

        if (scene->m_DynamicTextures.Full()) {
            return RESULT_OUT_OF_RESOURCES;
        }

        DynamicTexture t(0);
        t.m_Buffer = malloc(buffer_size);
        if (flip) {
            if (!CopyImageBufferFlipped(width, height, (uint8_t*)buffer, buffer_size, type, (uint8_t*)t.m_Buffer)) {
                free(t.m_Buffer);
                t.m_Buffer = 0;
                return RESULT_DATA_ERROR;
            }
        } else {
            memcpy(t.m_Buffer, buffer, buffer_size);
        }
        t.m_Width = width;
        t.m_Height = height;
        t.m_Type = type;

        scene->m_DynamicTextures.Put(texture_hash, t);

        return RESULT_OK;
    }

Result DeleteDynamicTexture(HScene scene, const dmhash_t texture_hash)
    {
        DynamicTexture* t = scene->m_DynamicTextures.Get(texture_hash);

        if (!t) {
            return RESULT_RESOURCE_NOT_FOUND;
        }
        t->m_Deleted = 1U;

        if (t->m_Buffer) {
            free(t->m_Buffer);
            t->m_Buffer = 0;
        }

        return RESULT_OK;
    }

    Result SetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, bool flip, const void* buffer, uint32_t buffer_size)
    {
        DynamicTexture*t = scene->m_DynamicTextures.Get(texture_hash);

        if (!t) {
            return RESULT_RESOURCE_NOT_FOUND;
        }

        if (t->m_Deleted) {
            dmLogError("Can't set texture data for deleted texture");
            return RESULT_INVAL_ERROR;
        }

        if (t->m_Buffer) {
            free(t->m_Buffer);
            t->m_Buffer = 0;
        }

        t->m_Buffer = malloc(buffer_size);
        if (flip) {
            if (!CopyImageBufferFlipped(width, height, (uint8_t*)buffer, buffer_size, type, (uint8_t*)t->m_Buffer)) {
                free(t->m_Buffer);
                t->m_Buffer = 0;
                return RESULT_DATA_ERROR;
            }
        } else {
            memcpy(t->m_Buffer, buffer, buffer_size);
        }
        t->m_Width = width;
        t->m_Height = height;
        t->m_Type = type;

        return RESULT_OK;
    }

    Result GetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t* out_width, uint32_t* out_height, dmImage::Type* out_type, const void** out_buffer)
    {
        DynamicTexture*t = scene->m_DynamicTextures.Get(texture_hash);

        if (!t) {
            return RESULT_RESOURCE_NOT_FOUND;
        }

        if (t->m_Deleted) {
            dmLogError("Can't get texture data for deleted texture");
            return RESULT_INVAL_ERROR;
        }

        if (!t->m_Buffer) {
            dmLogError("No texture data available for dynamic texture");
            return RESULT_DATA_ERROR;
        }

        *out_width = t->m_Width;
        *out_height = t->m_Height;
        *out_type = t->m_Type;
        *out_buffer = t->m_Buffer;

        return RESULT_OK;
    }

    static void AddResourcePath(HScene scene, void* resource, dmhash_t path_hash)
    {
        if (scene->m_ResourceToPath.Full())
        {
            int newcapacity = scene->m_ResourceToPath.Capacity() + 8;
            int tablesize = (newcapacity*2)/3;
            scene->m_ResourceToPath.SetCapacity(tablesize, newcapacity);
        }
        scene->m_ResourceToPath.Put((uintptr_t)resource, path_hash);
    }

    static void RemoveResourcePath(HScene scene, void* resource)
    {
        dmhash_t* path_hash = scene->m_ResourceToPath.Get((uintptr_t)resource);
        if (path_hash)
        {
            scene->m_ResourceToPath.Erase((uintptr_t)resource);
        }
    }

    static dmhash_t GetResourcePath(HScene scene, void* resource)
    {
        dmhash_t* path_hash = scene->m_ResourceToPath.Get((uintptr_t)resource);
        if (path_hash)
        {
            return *path_hash;
        }
        return 0;
    }

    Result AddFont(HScene scene, dmhash_t font_name_hash, void* font, dmhash_t path_hash)
    {
        if (scene->m_Fonts.Full())
            return RESULT_OUT_OF_RESOURCES;

        if (!scene->m_DefaultFont)
            scene->m_DefaultFont = font;

        AddResourcePath(scene, font, path_hash);
        scene->m_Fonts.Put(font_name_hash, font);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_FontHash == font_name_hash)
                nodes[i].m_Node.m_Font = font;
        }
        return RESULT_OK;
    }

    void RemoveFont(HScene scene, dmhash_t font_name_hash)
    {
        void** font = scene->m_Fonts.Get(font_name_hash);
        if (font)
        {
            RemoveResourcePath(scene, *font);
        }
        scene->m_Fonts.Erase(font_name_hash);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_FontHash == font_name_hash)
                nodes[i].m_Node.m_Font = 0;
        }
    }

    dmhash_t GetFontPath(HScene scene, dmhash_t font_hash)
    {
        void** font = scene->m_Fonts.Get(font_hash);
        if (!font)
        {
            return 0;
        }
        return GetResourcePath(scene, *font);
    }

    void* GetFont(HScene scene, dmhash_t font_hash)
    {
        void** font = scene->m_Fonts.Get(font_hash);
        if (!font)
        {
            return 0;
        }
        return *font;
    }

    Result AddParticlefx(HScene scene, const char* particlefx_name, void* particlefx_prototype)
    {
        if (scene->m_Particlefxs.Full())
            return RESULT_OUT_OF_RESOURCES;
        uint64_t name_hash = dmHashString64(particlefx_name);
        scene->m_Particlefxs.Put(name_hash, (dmParticle::HPrototype)particlefx_prototype);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_ParticlefxHash == name_hash)
            {
                nodes[i].m_Node.m_ParticlefxPrototype = particlefx_prototype;
            }
        }
        return RESULT_OK;
    }

    void RemoveParticlefx(HScene scene, const char* particlefx_name)
    {
        uint64_t name_hash = dmHashString64(particlefx_name);
        scene->m_Particlefxs.Erase(name_hash);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_ParticlefxHash == name_hash)
            {
                nodes[i].m_Node.m_ParticlefxPrototype = 0;
            }
        }
    }

    void ClearFonts(HScene scene)
    {
        scene->m_Fonts.Clear();
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            RemoveResourcePath(scene, nodes[i].m_Node.m_Font);
            nodes[i].m_Node.m_Font = 0;
        }
    }

    Result AddLayer(HScene scene, const char* layer_name)
    {
        if (scene->m_Layers.Full())
        {
            dmLogError("Max number of layers exhausted (max %d total)", scene->m_Layers.Capacity());
            return RESULT_OUT_OF_RESOURCES;
        }

        uint64_t layer_hash = dmHashString64(layer_name);
        uint16_t index = scene->m_NextLayerIndex++;
        scene->m_Layers.Put(layer_hash, index);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (nodes[i].m_Node.m_LayerHash == layer_hash)
                nodes[i].m_Node.m_LayerIndex = index;
        }
        return RESULT_OK;
    }

    void AllocateLayouts(HScene scene, size_t node_count, size_t layouts_count)
    {
        layouts_count++;
        size_t capacity = dmMath::Max((uint32_t) layouts_count, scene->m_Layouts.Capacity());
        scene->m_Layouts.SetCapacity(capacity);
        scene->m_LayoutsNodeDescs.SetCapacity(layouts_count*node_count);
        scene->m_LayoutsNodeDescs.SetSize(0);
    }

    void ClearLayouts(HScene scene)
    {
        scene->m_LayoutId = DEFAULT_LAYOUT;
        scene->m_Layouts.SetSize(0);
        scene->m_Layouts.Push(DEFAULT_LAYOUT);
        scene->m_LayoutsNodeDescs.SetCapacity(0);
    }

    Result AddLayout(HScene scene, const char* layout_id)
    {
        if (scene->m_Layouts.Full())
        {
            dmLogError("Could not add layout to scene since the buffer is full (%d).", scene->m_Layouts.Capacity());
            return RESULT_OUT_OF_RESOURCES;
        }
        uint64_t layout_hash = dmHashString64(layout_id);
        scene->m_Layouts.Push(layout_hash);
        return RESULT_OK;
    }

    dmhash_t GetLayout(const HScene scene)
    {
        return scene->m_LayoutId;
    }

    uint16_t GetLayoutCount(const HScene scene)
    {
        return (uint16_t)scene->m_Layouts.Size();
    }

    Result GetLayoutId(const HScene scene, uint16_t layout_index, dmhash_t& layout_id_out)
    {
        if(layout_index >= (uint16_t)scene->m_Layouts.Size())
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
        layout_id_out = scene->m_Layouts[layout_index];
        return RESULT_OK;
    }

    uint16_t GetLayoutIndex(const HScene scene, dmhash_t layout_id)
    {
        uint32_t i;
        for(i = 0; i < scene->m_Layouts.Size(); ++i) {
            if(layout_id == scene->m_Layouts[i])
                break;
        }
        if(i == scene->m_Layouts.Size())
        {
            dmLogError("Could not get index for layout '%s'", dmHashReverseSafe64(layout_id));
            return 0;
        }
        return i;
    }

    Result SetNodeLayoutDesc(const HScene scene, HNode node, const void *desc, uint16_t layout_index_start, uint16_t layout_index_end)
    {
        InternalNode* n = GetNode(scene, node);
        void **table = n->m_Node.m_NodeDescTable;
        if(table == 0)
        {
            if(scene->m_LayoutsNodeDescs.Full())
                return RESULT_OUT_OF_RESOURCES;
            size_t table_index = scene->m_LayoutsNodeDescs.Size();
            scene->m_LayoutsNodeDescs.SetSize(table_index + scene->m_Layouts.Size());
            n->m_Node.m_NodeDescTable = table = &scene->m_LayoutsNodeDescs[table_index];
        }
        assert(layout_index_end < scene->m_Layouts.Size());
        for(uint16_t i = layout_index_start; i <= layout_index_end; ++i)
            table[i] = (void*) desc;
        return RESULT_OK;
    }

    Result SetLayout(const HScene scene, dmhash_t layout_id, SetNodeCallback set_node_callback)
    {
        scene->m_LayoutId = layout_id;
        uint16_t index = GetLayoutIndex(scene, layout_id);
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode *n = &nodes[i];
            if(!n->m_Node.m_NodeDescTable)
                continue;
            set_node_callback(scene, GetNodeHandle(n), n->m_Node.m_NodeDescTable[index]);
            n->m_Node.m_DirtyLocal = 1;
        }
        return RESULT_OK;
    }

    HNode GetNodeHandle(InternalNode* node)
    {
        return ((uint32_t) node->m_Version) << 16 | node->m_Index;
    }

    TextureSetAnimDesc* GetNodeTextureSet(HScene scene, HNode node)
    {
        InternalNode* in = GetNode(scene, node);
        Node& n = in->m_Node;
        if(n.m_TextureType != NODE_TEXTURE_TYPE_TEXTURE_SET || n.m_TextureSetAnimDesc.m_TexCoords == 0x0)
            return 0;
        return &n.m_TextureSetAnimDesc;
    }

    static inline int32_t GetNodeAnimationFrameInternal(InternalNode* in)
    {
        Node& n = in->m_Node;
        if(n.m_TextureType != NODE_TEXTURE_TYPE_TEXTURE_SET || n.m_TextureSetAnimDesc.m_TexCoords == 0x0)
            return -1;
        TextureSetAnimDesc* anim_desc = &n.m_TextureSetAnimDesc;
        int32_t anim_frames = anim_desc->m_State.m_End - anim_desc->m_State.m_Start;
        int32_t anim_frame = (int32_t) (n.m_FlipbookAnimPosition * (float)anim_frames);
        return anim_desc->m_State.m_Start + dmMath::Clamp(anim_frame, 0, anim_frames-1);
    }

    int32_t GetNodeAnimationFrame(HScene scene, HNode node)
    {
        return GetNodeAnimationFrameInternal(GetNode(scene, node));
    }

    int32_t GetNodeAnimationFrameCount(HScene scene, HNode node)
    {
        TextureSetAnimDesc* anim_desc = GetNodeTextureSet(scene, node);
        if (anim_desc == 0)
        {
            return -1;
        }
        return anim_desc->m_State.m_End - anim_desc->m_State.m_Start;
    }


    static inline const float* GetNodeFlipbookAnimUVInternal(InternalNode* in)
    {
        int32_t anim_frame = GetNodeAnimationFrameInternal(in);
        if (anim_frame < 0)
            return 0;

        Node& n = in->m_Node;
        TextureSetAnimDesc* anim_desc = &n.m_TextureSetAnimDesc;
        // Each frame is a quad, with 2 floats for each coord (== 8 floats)
        return anim_desc->m_TexCoords + anim_frame * 8;
    }

    const float* GetNodeFlipbookAnimUV(HScene scene, HNode node)
    {
        return GetNodeFlipbookAnimUVInternal(GetNode(scene, node));
    }

    Vector4 CalculateReferenceScale(HScene scene, InternalNode* node)
    {
        float scale_x = 1.0f;
        float scale_y = 1.0f;

        if (scene->m_AdjustReference == ADJUST_REFERENCE_LEGACY || node == 0x0 || node->m_ParentIndex == INVALID_INDEX) {
            scale_x = (float) scene->m_Context->m_PhysicalWidth / (float) scene->m_Width;
            scale_y = (float) scene->m_Context->m_PhysicalHeight / (float) scene->m_Height;
        } else {
            Vector4 adjust_scale = scene->m_Nodes[node->m_ParentIndex].m_Node.m_LocalAdjustScale;
            scale_x = adjust_scale.getX();
            scale_y = adjust_scale.getY();
        }
        return Vector4(scale_x, scale_y, 1.0f, 1.0f);
    }

    inline void CalculateNodeSize(InternalNode* in)
    {
        Node& n = in->m_Node;
        //TODO: Custom/ParticleFX shouldn't be singled out. It's the others that should be included
        if(n.m_SizeMode == SIZE_MODE_MANUAL || n.m_NodeType == NODE_TYPE_CUSTOM ||
           n.m_NodeType == NODE_TYPE_PARTICLEFX || n.m_TextureType != NODE_TEXTURE_TYPE_TEXTURE_SET ||
           n.m_TextureSetAnimDesc.m_TexCoords == 0x0)
        {
            return;
        }

        const float* tc = GetNodeFlipbookAnimUVInternal(in);
        TextureSetAnimDesc* anim_desc = &n.m_TextureSetAnimDesc;

        float w,h;
        if(tc[0] != tc[2] && tc[3] != tc[5])
        {
            // uv-rotated
            w = tc[2]-tc[0];
            h = tc[1]-tc[5];
            n.m_Properties[PROPERTY_SIZE][0] = h * (float)anim_desc->m_State.m_OriginalTextureHeight;
            n.m_Properties[PROPERTY_SIZE][1] = w * (float)anim_desc->m_State.m_OriginalTextureWidth;
        }
        else
        {
            w = tc[4]-tc[0];
            h = tc[3]-tc[1];
            n.m_Properties[PROPERTY_SIZE][0] = w * (float)anim_desc->m_State.m_OriginalTextureWidth;
            n.m_Properties[PROPERTY_SIZE][1] = h * (float)anim_desc->m_State.m_OriginalTextureHeight;
        }
    }

    struct UpdateDynamicTexturesParams
    {
        UpdateDynamicTexturesParams()
        {
            memset(this, 0, sizeof(*this));
        }
        HScene m_Scene;
        void*  m_Context;
        const RenderSceneParams* m_Params;
        int    m_NewCount;
    };

    static void UpdateDynamicTextures(UpdateDynamicTexturesParams* params, const dmhash_t* key, DynamicTexture* texture)
    {
        dmGui::Scene* const scene = params->m_Scene;
        void* const context = params->m_Context;

        if (texture->m_Deleted) {
            if (texture->m_Handle) {
                // handle might be null if the texture is created/destroyed in the same frame
                params->m_Params->m_DeleteTexture(scene, texture->m_Handle, context);
            }
            if (scene->m_DeletedDynamicTextures.Full()) {
                scene->m_DeletedDynamicTextures.OffsetCapacity(16);
            }
            scene->m_DeletedDynamicTextures.Push(*key);
        } else {
            if (!texture->m_Handle && texture->m_Buffer) {
                texture->m_Handle = params->m_Params->m_NewTexture(scene, texture->m_Width, texture->m_Height, texture->m_Type, texture->m_Buffer, context);
                params->m_NewCount++;
                free(texture->m_Buffer);
                texture->m_Buffer = 0;
            } else if (texture->m_Handle && texture->m_Buffer) {
                params->m_Params->m_SetTextureData(scene, texture->m_Handle, texture->m_Width, texture->m_Height, texture->m_Type, texture->m_Buffer, context);
                free(texture->m_Buffer);
                texture->m_Buffer = 0;
            }
        }
    }

    static void UpdateDynamicTextures(HScene scene, const RenderSceneParams& params, void* context)
    {
        UpdateDynamicTexturesParams p;
        p.m_Scene = scene;
        p.m_Context = context;
        p.m_Params = &params;
        scene->m_DeletedDynamicTextures.SetSize(0);
        scene->m_DynamicTextures.Iterate(UpdateDynamicTextures, &p);

        if (p.m_NewCount > 0) {
            uint32_t n = scene->m_Nodes.Size();
            InternalNode* nodes = scene->m_Nodes.Begin();
            for (uint32_t j = 0; j < n; ++j) {
                Node& node = nodes[j].m_Node;
                if (DynamicTexture* texture = scene->m_DynamicTextures.Get(node.m_TextureHash)) {
                    node.m_Texture = texture->m_Handle;
                    node.m_TextureType = NODE_TEXTURE_TYPE_DYNAMIC;
                }
            }
        }
    }

    static void DeferredDeleteDynamicTextures(HScene scene, const RenderSceneParams& params, void* context)
    {
        for (uint32_t i = 0; i < scene->m_DeletedDynamicTextures.Size(); ++i) {
            dmhash_t texture_hash = scene->m_DeletedDynamicTextures[i];
            scene->m_DynamicTextures.Erase(texture_hash);

            uint32_t n = scene->m_Nodes.Size();
            InternalNode* nodes = scene->m_Nodes.Begin();
            for (uint32_t j = 0; j < n; ++j) {
                Node& node = nodes[j].m_Node;
                if (node.m_TextureHash == texture_hash) {
                    node.m_Texture = 0;
                    node.m_TextureType = NODE_TEXTURE_TYPE_NONE;
                    // Do not break here. Texture may be used multiple times.
                }
            }
        }
    }

    static uint16_t GetLayerIndex(HScene scene, InternalNode* node)
    {
        if (node->m_Node.m_LayerHash == DEFAULT_LAYER && node->m_ParentIndex != INVALID_INDEX)
        {
            return GetLayerIndex(scene, &scene->m_Nodes[node->m_ParentIndex & 0xffff]);
        } else {
            return node->m_Node.m_LayerIndex;
        }
    }

    struct RenderEntrySortPred
    {
        bool operator ()(const RenderEntry& a, const RenderEntry& b) const
        {
            return a.m_RenderKey < b.m_RenderKey;
        }
    };

    struct RenderEntrySortDisabledPred
    {
        bool operator ()(const RenderEntry& a, const RenderEntry& b) const
        {
            return a.m_RenderKey < b.m_RenderKey;
        }
    };

    struct ScopeContext {
        ScopeContext() {
            memset(this, 0, sizeof(*this));
            m_NonInvClipperHead = INVALID_INDEX;
            m_NonInvClipperTail = INVALID_INDEX;
        }
        uint16_t m_NonInvClipperHead;
        uint16_t m_NonInvClipperTail;
        uint16_t m_BitFieldOffset;
        uint16_t m_ClipperCount;
        uint16_t m_InvClipperCount;
    };

    static uint16_t CalcBitRange(uint16_t val) {
        uint16_t bit_range = 0;
        while (val != 0) {
            bit_range++;
            val >>= 1;
        }
        return bit_range;
    }

    static uint16_t CalcMask(uint16_t bits) {
        return (1 << bits) - 1;
    }

    static uint64_t CalcRenderKey(uint64_t layer, uint64_t index, uint64_t inv_clipper_id, uint64_t sub_layer, uint64_t sub_index) {
        return (layer << LAYER_SHIFT)
                | (index << INDEX_SHIFT)
                | (inv_clipper_id << CLIPPER_SHIFT)
                | (sub_layer << SUB_LAYER_SHIFT)
                | (sub_index << SUB_INDEX_SHIFT);
    }

    static void UpdateScope(InternalNode* node, StencilScope& scope, StencilScope& child_scope, const StencilScope* parent_scope, uint16_t index, uint16_t non_inv_clipper_count, uint16_t inv_clipper_count, uint16_t bit_field_offset) {
        int bit_range = CalcBitRange(non_inv_clipper_count);
        // state used for drawing the clipper
        scope.m_WriteMask = 0xff;
        scope.m_TestMask = 0;
        if (parent_scope != 0x0) {
            scope.m_TestMask = parent_scope->m_TestMask;
        }
        bool inverted = node->m_Node.m_ClippingInverted;
        if (!inverted) {
            scope.m_RefVal = (index + 1) << bit_field_offset;
            if (parent_scope != 0x0) {
                scope.m_RefVal |= parent_scope->m_RefVal;
            }
        } else {
            scope.m_RefVal = 1 << (7 - index);
            if (parent_scope != 0x0) {
                scope.m_RefVal |= (CalcMask(bit_field_offset) & parent_scope->m_RefVal);
            }
        }
        if (inverted && node->m_Node.m_ClippingVisible) {
            scope.m_ColorMask = 0xf;
        } else {
            scope.m_ColorMask = 0;
        }
        // state used for drawing any sub non-clippers
        child_scope.m_WriteMask = 0;
        if (!inverted) {
            child_scope.m_RefVal = scope.m_RefVal;
            child_scope.m_TestMask = (CalcMask(bit_range) << bit_field_offset) | scope.m_TestMask;
        } else {
            child_scope.m_RefVal = 0;
            child_scope.m_TestMask = scope.m_RefVal;
            if (parent_scope != 0x0) {
                child_scope.m_RefVal |= parent_scope->m_RefVal;
                child_scope.m_TestMask |= parent_scope->m_TestMask;
            }
        }
        child_scope.m_ColorMask = 0xf;
        // Check for overflow
        int inverted_count = 0;
        if (inverted) {
            inverted_count = index + 1;
        } else {
            inverted_count = inv_clipper_count;
        }
        int bit_count = inverted_count + bit_field_offset + bit_range;
        if (bit_count > 8) {
            dmLogWarning("Stencil buffer exceeded, clipping will not work as expected.");
        }
    }

    static void CollectInvClippers(HScene scene, uint16_t start_index, dmArray<InternalClippingNode>& clippers, ScopeContext& scope_context, uint16_t parent_index) {
        uint32_t index = start_index;
        InternalClippingNode* parent = 0x0;
        if (parent_index != INVALID_INDEX) {
            parent = &clippers[parent_index];
        }
        while (index != INVALID_INDEX)
        {
            InternalNode* n = &scene->m_Nodes[index];
            if (n->m_Node.m_Enabled)
            {
                switch (n->m_Node.m_ClippingMode) {
                case CLIPPING_MODE_STENCIL:
                    {
                        uint32_t clipper_index = clippers.Size();
                        clippers.SetSize(clipper_index + 1);
                        InternalClippingNode& clipper = clippers.Back();
                        clipper.m_NodeIndex = index;
                        clipper.m_ParentIndex = parent_index;
                        clipper.m_NextNonInvIndex = INVALID_INDEX;
                        clipper.m_VisibleRenderKey = ~0ULL;
                        n->m_ClipperIndex = clipper_index;
                        if (n->m_Node.m_ClippingInverted) {
                            StencilScope* parent_scope = 0x0;
                            if (parent != 0x0) {
                                parent_scope = &parent->m_ChildScope;
                            }
                            UpdateScope(n, clipper.m_Scope, clipper.m_ChildScope, parent_scope, scope_context.m_InvClipperCount, 0, 0, scope_context.m_BitFieldOffset);
                            ++scope_context.m_InvClipperCount;
                            CollectInvClippers(scene, n->m_ChildHead, clippers, scope_context, clipper_index);
                        } else {
                            // append to linked list
                            uint16_t* pointer = &scope_context.m_NonInvClipperHead;
                            if (*pointer != INVALID_INDEX) {
                                pointer = &clippers[scope_context.m_NonInvClipperTail].m_NextNonInvIndex;
                            }
                            *pointer = clipper_index;
                            scope_context.m_NonInvClipperTail = clipper_index;
                            ++scope_context.m_ClipperCount;
                        }
                    }
                    break;
                case CLIPPING_MODE_NONE:
                    n->m_ClipperIndex = parent_index;
                    CollectInvClippers(scene, n->m_ChildHead, clippers, scope_context, parent_index);
                    break;
                }
            }
            index = n->m_NextIndex;
        }
    }

    static void CollectClippers(HScene scene, uint16_t start_index, uint16_t bit_field_offset, uint16_t inv_clipper_count, dmArray<InternalClippingNode>& clippers, uint16_t parent_index)
    {
        ScopeContext context;
        context.m_BitFieldOffset = bit_field_offset;
        context.m_InvClipperCount = inv_clipper_count;
        CollectInvClippers(scene, start_index, clippers, context, parent_index);
        uint16_t non_inv_clipper_index = context.m_NonInvClipperHead;
        uint16_t index = 0;
        while (non_inv_clipper_index != INVALID_INDEX) {
            InternalClippingNode* non_inv_clipper = &clippers[non_inv_clipper_index];
            StencilScope* parent_scope = 0x0;
            if (non_inv_clipper->m_ParentIndex != INVALID_INDEX) {
                parent_scope = &clippers[non_inv_clipper->m_ParentIndex].m_ChildScope;
            }
            InternalNode* node = &scene->m_Nodes[non_inv_clipper->m_NodeIndex];
            UpdateScope(node, non_inv_clipper->m_Scope, non_inv_clipper->m_ChildScope, parent_scope, index, context.m_ClipperCount, context.m_InvClipperCount, bit_field_offset);
            uint16_t bit_range = CalcBitRange(context.m_ClipperCount);
            CollectClippers(scene, node->m_ChildHead, context.m_BitFieldOffset + bit_range, context.m_InvClipperCount, clippers, non_inv_clipper_index);
            non_inv_clipper_index = non_inv_clipper->m_NextNonInvIndex;
            ++index;
        }
    }

    static void Increment(Scope* scope) {
        scope->m_Index = dmMath::Min(255, scope->m_Index + 1);
    }

    static uint64_t CalcRenderKey(Scope* scope, uint16_t layer, uint16_t index) {
        if (scope != 0x0) {
            return CalcRenderKey(scope->m_RootLayer, scope->m_RootIndex, scope->m_Index, layer, index);
        } else {
            return CalcRenderKey(layer, index, 0, 0, 0);
        }
    }

    static uint16_t CollectRenderEntries(HScene scene, uint16_t start_index, uint16_t order, Scope* scope, dmArray<InternalClippingNode>& clippers, dmArray<RenderEntry>& render_entries) {
        uint16_t index = start_index;
        while (index != INVALID_INDEX) {
            InternalNode* n = &scene->m_Nodes[index];
            if (n->m_Node.m_Enabled) {
                HNode node = GetNodeHandle(n);
                uint16_t layer = GetLayerIndex(scene, n);
                if (n->m_ClipperIndex != INVALID_INDEX) {
                    InternalClippingNode& clipper = clippers[n->m_ClipperIndex];
                    if (clipper.m_NodeIndex == index) {
                        bool root_clipper = scope == 0x0;
                        Scope tmp_scope(0, order);
                        Scope* current_scope = scope;
                        if (current_scope == 0x0) {
                            current_scope = &tmp_scope;
                            ++order;
                        } else {
                            Increment(current_scope);
                        }
                        uint64_t clipping_key = CalcRenderKey(current_scope, 0, 0);
                        uint64_t render_key = CalcRenderKey(current_scope, layer, 1);
                        CollectRenderEntries(scene, n->m_ChildHead, 2, current_scope, clippers, render_entries);
                        if (layer > 0) {
                            render_key = CalcRenderKey(current_scope, layer, 1);
                        }
                        clipper.m_VisibleRenderKey = render_key;
                        RenderEntry entry;
                        entry.m_Node = node;
                        entry.m_RenderKey = clipping_key;
                        if (render_entries.Full())
                        {
                            render_entries.OffsetCapacity(16U);
                        }
                        render_entries.Push(entry);
                        if (n->m_Node.m_ClippingVisible) {
                            entry.m_RenderKey = render_key;
                            if (render_entries.Full())
                            {
                                render_entries.OffsetCapacity(16U);
                            }
                            render_entries.Push(entry);
                        }
                        if (!root_clipper) {
                            Increment(current_scope);
                        }
                        index = n->m_NextIndex;
                        continue;
                    }
                }

                if (n->m_Node.m_NodeType == NODE_TYPE_PARTICLEFX)
                {
                    HNode hnode = GetNodeHandle(n);
                    uint32_t alive_count = scene->m_AliveParticlefxs.Size();
                    for (uint32_t i = 0; i < alive_count; ++i)
                    {
                        ParticlefxComponent* comp = &scene->m_AliveParticlefxs[i];
                        if (hnode == comp->m_Node)
                        {
                            uint32_t emitter_count = dmParticle::GetInstanceEmitterCount(scene->m_ParticlefxContext, comp->m_Instance);

                            // Create a render entry for each emitter
                            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
                            {
                                dmParticle::EmitterRenderData* emitter_render_data;
                                dmParticle::GetEmitterRenderData(scene->m_ParticlefxContext, comp->m_Instance, emitter_i, &emitter_render_data);

                                if (emitter_render_data == 0x0)
                                    continue;

                                RenderEntry emitter_render_entry;
                                emitter_render_entry.m_Node = node;
                                emitter_render_entry.m_RenderKey = CalcRenderKey(scope, layer, order++);
                                emitter_render_entry.m_RenderData = emitter_render_data;

                                if (render_entries.Full())
                                {
                                    render_entries.OffsetCapacity(16U);
                                }
                                render_entries.Push(emitter_render_entry);
                            }
                        }
                    }
                }
                else
                {
                    RenderEntry entry;
                    entry.m_Node = node;
                    entry.m_RenderKey = CalcRenderKey(scope, layer, order++);
                    if (render_entries.Full())
                    {
                        render_entries.OffsetCapacity(16U);
                    }
                    render_entries.Push(entry);
                }

                order = CollectRenderEntries(scene, n->m_ChildHead, order, scope, clippers, render_entries);
            }
            index = n->m_NextIndex;
        }
        return order;
    }

    static void CollectNodes(HScene scene, dmArray<InternalClippingNode>& clippers, dmArray<RenderEntry>& render_entries)
    {
        CollectClippers(scene, scene->m_RenderHead, 0, 0, clippers, INVALID_INDEX);
        CollectRenderEntries(scene, scene->m_RenderHead, 0, 0x0, clippers, render_entries);
    }

    static inline bool IsVisible(InternalNode* n, float opacity)
    {
        bool use_clipping = n->m_ClipperIndex != INVALID_INDEX;

        return n->m_Node.m_IsVisible && (opacity != 0.0f || use_clipping);
    }

    void RenderScene(HScene scene, const RenderSceneParams& params, void* context)
    {
        Context* c = scene->m_Context;

        UpdateDynamicTextures(scene, params, context);
        DeferredDeleteDynamicTextures(scene, params, context);

        c->m_RenderNodes.SetSize(0);
        c->m_RenderTransforms.SetSize(0);
        c->m_RenderOpacities.SetSize(0);
        c->m_StencilClippingNodes.SetSize(0);
        c->m_StencilScopes.SetSize(0);
        c->m_StencilScopeIndices.SetSize(0);
        uint32_t capacity = scene->m_NodePool.Size() * 2;
        if (capacity > c->m_RenderNodes.Capacity())
        {
            c->m_RenderNodes.SetCapacity(capacity);
            c->m_RenderTransforms.SetCapacity(capacity);
            c->m_RenderOpacities.SetCapacity(capacity);
            c->m_SceneTraversalCache.m_Data.SetCapacity(capacity);
            c->m_SceneTraversalCache.m_Data.SetSize(capacity);
            c->m_StencilClippingNodes.SetCapacity(capacity);
            c->m_StencilScopes.SetCapacity(capacity);
            c->m_StencilScopeIndices.SetCapacity(capacity);
        }

        c->m_SceneTraversalCache.m_NodeIndex = 0;
        if(++c->m_SceneTraversalCache.m_Version == INVALID_INDEX)
        {
            c->m_SceneTraversalCache.m_Version = 0;
        }

        CollectNodes(scene, c->m_StencilClippingNodes, c->m_RenderNodes);
        uint32_t node_count = c->m_RenderNodes.Size();
        std::sort(c->m_RenderNodes.Begin(), c->m_RenderNodes.End(), RenderEntrySortPred());
        Matrix4 transform;

        if (c->m_RenderNodes.Capacity() > c->m_RenderTransforms.Capacity())
        {
            uint32_t new_capacity = c->m_RenderNodes.Capacity();
            c->m_RenderTransforms.SetCapacity(new_capacity);
            c->m_RenderOpacities.SetCapacity(new_capacity);
            c->m_SceneTraversalCache.m_Data.SetCapacity(new_capacity);
            c->m_SceneTraversalCache.m_Data.SetSize(new_capacity);
            c->m_StencilClippingNodes.SetCapacity(new_capacity);
            c->m_StencilScopes.SetCapacity(new_capacity);
            c->m_StencilScopeIndices.SetCapacity(new_capacity);
        }

        uint32_t num_pruned = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            RenderEntry& entry = c->m_RenderNodes[i];
            uint16_t index = entry.m_Node & 0xffff;
            InternalNode* n = &scene->m_Nodes[index];
            float opacity = 1.0f;

            // Ideally, we'd like to be able to do this in an Update step,
            // but since the gui script is updated _after_ the gui component, we need to accomodate
            // for any late scripting changes
            // Note: We need this update step for bones as well, since they update their transform
            CalculateNodeSize(n);
            CalculateNodeTransformAndAlphaCached(scene, n, CalculateNodeTransformFlags(CALCULATE_NODE_INCLUDE_SIZE | CALCULATE_NODE_RESET_PIVOT), transform, opacity);

            // Ideally, we'd like to have this update step in the Update function (I'm not even sure why it isn't tbh)
            // But for now, let's prune the list here
            if (!IsVisible(n, opacity) || n->m_Node.m_IsBone)
            {
                entry.m_Node = INVALID_HANDLE;
                entry.m_RenderKey = INVALID_RENDER_KEY;
                ++num_pruned;
                continue;
            }

            c->m_RenderTransforms.Push(transform);
            c->m_RenderOpacities.Push(opacity);
            if (n->m_ClipperIndex != INVALID_INDEX) {
                InternalClippingNode* clipper = &c->m_StencilClippingNodes[n->m_ClipperIndex];
                if (clipper->m_NodeIndex == index) {
                    if (clipper->m_VisibleRenderKey == entry.m_RenderKey) {
                        StencilScope* scope = 0x0;
                        if (clipper->m_ParentIndex != INVALID_INDEX) {
                            scope = &c->m_StencilClippingNodes[clipper->m_ParentIndex].m_ChildScope;
                        }
                        c->m_StencilScopes.Push(scope);
                    } else {
                        c->m_StencilScopes.Push(&clipper->m_Scope);
                    }
                } else {
                    c->m_StencilScopes.Push(&clipper->m_ChildScope);
                }
            } else {
                c->m_StencilScopes.Push(0x0);
            }
        }

        if (num_pruned)
        {
            std::sort(c->m_RenderNodes.Begin(), c->m_RenderNodes.End(), RenderEntrySortDisabledPred());
            c->m_RenderNodes.SetSize(c->m_RenderNodes.Size() - num_pruned);
        }

        scene->m_ResChanged = 0;
        params.m_RenderNodes(scene, c->m_RenderNodes.Begin(), c->m_RenderTransforms.Begin(), c->m_RenderOpacities.Begin(), (const StencilScope**)c->m_StencilScopes.Begin(), c->m_RenderNodes.Size(), context);
    }

    static bool IsNodeEnabledRecursive(HScene scene, uint16_t node_index)
    {
        InternalNode* node = &scene->m_Nodes[node_index];
        if (node->m_Node.m_Enabled && node->m_ParentIndex != INVALID_INDEX)
        {
            return IsNodeEnabledRecursive(scene, node->m_ParentIndex);
        }
        else
        {
            return node->m_Node.m_Enabled;
        }
    }

    #define OLD_VERSION false

    static bool AnimCompare(const Animation& lhs, const float* value)
    {
        return lhs.m_Value < value;
    }

    static inline void RemoveAnimation(dmArray<Animation>& animations, uint32_t i)
    {
        Animation* current = &animations[i];
        Animation* end = animations.End();
        // Move all one step to the "left"
        memmove(current, current+1, sizeof(Animation) * (end-current-1));
        animations.SetSize(animations.Size()-1);
    }

    static inline uint32_t FindAnimation(dmArray<Animation>& animations, float* value)
    {
        Animation* begin = animations.Begin();
        Animation* end = animations.End();
        Animation* anim = std::lower_bound(begin, end, value, AnimCompare);

        if (anim != end && anim->m_Value == value)
        {
            return (uint32_t)(anim - begin);
        }
        return 0xffffffff;
    }

    // This function assumes the size of the array has already grown 1 element
    static inline uint32_t InsertAnimation(dmArray<Animation>& animations, Animation* anim)
    {
        Animation* begin = animations.Begin();
        // We use the fact that the actual end pointer is actually valid here (it's just uninitialized)
        Animation* last = animations.End()-1;
        Animation* current = std::lower_bound(begin, last, anim->m_Value, AnimCompare);
        if (current != last && current->m_Value != anim->m_Value) // anim.m_Value >= value
        {
            // Move all one step to the "right"
            memmove(current+1, current, sizeof(Animation) * (last-current));
        }
        *current = *anim;
        return (uint32_t)(current - begin);
    }

    static inline void CompleteAnimation(HScene scene, Animation* anim, bool finished)
    {
        if (!anim->m_AnimationCompleteCalled)
        {
            // NOTE: Very important to set m_AnimationCompleteCalled to 1
            // before invoking the call-back. The call-back could potentially
            // start a new animation that could reuse the same animation slot.
            anim->m_AnimationCompleteCalled = 1;

            // NOTE: Very important to invoke the easing release callback for
            // the before calling animation complete. The animation
            // complete callback could start a new animation with an easing
            // curve that would be immediately released.
            if (anim->m_Easing.release_callback)
            {
                anim->m_Easing.release_callback(&anim->m_Easing);
            }

            if (anim->m_AnimationComplete)
            {
                anim->m_AnimationComplete(scene, anim->m_Node, finished, anim->m_Userdata1, anim->m_Userdata2);
            }
        }
    }

    void UpdateAnimations(HScene scene, float dt)
    {
        dmArray<Animation>* animations = &scene->m_Animations;

        uint32_t active_animations = 0;

        for (uint32_t i = 0; i < animations->Size(); ++i)
        {
            Animation* anim = &(*animations)[i];

            dmGui::Playback playback = anim->m_Playback;
            bool looping = playback == PLAYBACK_LOOP_FORWARD || playback == PLAYBACK_LOOP_BACKWARD || playback == PLAYBACK_LOOP_PINGPONG;

            if (anim->m_Elapsed > anim->m_Duration
                || anim->m_Cancelled
                || (!looping && anim->m_Elapsed == anim->m_Duration && anim->m_Duration != 0))
            {
                continue;
            }
            if (!IsNodeEnabledRecursive(scene, anim->m_Node & 0xffff))
            {
                continue;
            }
            ++active_animations;

            if (anim->m_Delay < dt)
            {
                if (anim->m_FirstUpdate)
                {
                    anim->m_From = *anim->m_Value;
                    anim->m_FirstUpdate = 0;
                    // Compensate Elapsed with Delay underflow
                    anim->m_Elapsed = -anim->m_Delay;
                    anim->m_Delay = 0;
                }

                // NOTE: We add dt to elapsed before we calculate t.
                // Example: 60 updates with dt=1/60.0 should result in a complete animation
                anim->m_Elapsed += dt*anim->m_PlaybackRate;

                // Clamp elapsed to duration if we are closer than half a time step
                anim->m_Elapsed = dmMath::Select(anim->m_Elapsed + dt * anim->m_PlaybackRate * 0.5f - anim->m_Duration, anim->m_Duration, anim->m_Elapsed);
                // Calculate normalized time if elapsed has not yet reached duration, otherwise it's set to 1 (animation complete)
                float t = 1.0f;
                if (anim->m_Duration != 0)
                {
                    t = dmMath::Select(anim->m_Duration - anim->m_Elapsed, anim->m_Elapsed / anim->m_Duration, 1.0f);
                }
                float t2 = t;
                if (playback == PLAYBACK_ONCE_BACKWARD || playback == PLAYBACK_LOOP_BACKWARD || anim->m_Backwards) {
                    t2 = 1.0f - t;
                }
                if (playback == PLAYBACK_ONCE_PINGPONG || playback == PLAYBACK_LOOP_PINGPONG) {
                    t2 *= 2.0f;
                    if (t2 > 1.0f) {
                        t2 = 2.0f - t2;
                    }
                }

                float x = dmEasing::GetValue(anim->m_Easing, t2);

                *anim->m_Value = anim->m_From + (anim->m_To - anim->m_From) * x;
                // Flag local transform as dirty for the node
                scene->m_Nodes[anim->m_Node & 0xffff].m_Node.m_DirtyLocal = 1;

                // Animation complete, see above
                if (t >= 1.0f)
                {
                    if (looping) {
                        anim->m_Elapsed = anim->m_Elapsed - anim->m_Duration;
                        if (playback == PLAYBACK_LOOP_PINGPONG) {
                            anim->m_Backwards ^= 1;
                        }
                    } else {
                        CompleteAnimation(scene, anim, true);
                    }
                }
            }
            else
            {
                anim->m_Delay -= dt;
            }
        }

        uint32_t n = animations->Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Animation* anim = &(*animations)[i];
            if ((anim->m_Elapsed >= anim->m_Duration && anim->m_Delay == 0) || anim->m_Cancelled)
            {
                // If we have cancelled an animation, its callback won't be called which means
                // we potentially get dangling lua refs in the script system
                if (!anim->m_AnimationCompleteCalled && anim->m_AnimationComplete)
                {
                    anim->m_AnimationCompleteCalled = 1;
                    anim->m_AnimationComplete(scene, anim->m_Node, !anim->m_Cancelled, anim->m_Userdata1, anim->m_Userdata2);
                }

                RemoveAnimation(*animations, i);
                i--;
                n--;
                continue;
            }
        }

        DM_PROPERTY_ADD_U32(rmtp_GuiAnimations, n);
        DM_PROPERTY_ADD_U32(rmtp_GuiActiveAnimations, active_animations);
    }

    struct InputArgs
    {
        const InputAction* m_Action;
        bool m_Consumed;
    };

    Result RunScript(HScene scene, ScriptFunction script_function, int custom_ref, void* args)
    {
        DM_PROFILE(__FUNCTION__);

        if (scene->m_Script == 0x0)
            return RESULT_OK;

        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        int lua_ref = scene->m_Script->m_FunctionReferences[script_function];
        if (custom_ref != LUA_NOREF) {
            lua_ref = custom_ref;
        }

        if (lua_ref != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
            dmScript::SetInstance(L);

            if (custom_ref != LUA_NOREF) {
                dmScript::ResolveInInstance(L, custom_ref);
                if (!lua_isfunction(L, -1))
                {
                    // If the script instance is dead we just ignore the callback
                    lua_pop(L, 1);
                    lua_pushnil(L);
                    dmScript::SetInstance(L);
                    dmLogWarning("Failed to call message response callback function, has it been deleted?");
                    return RESULT_OK;
                }
                dmScript::UnrefInInstance(L, custom_ref);
            }
            else
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
            }

            assert(lua_isfunction(L, -1));

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);

            uint32_t arg_count = 1;
            uint32_t ret_count = 0;

            const char* message_name = 0;
            switch (script_function)
            {
            case SCRIPT_FUNCTION_UPDATE:
                {
                    float* dt = (float*)args;
                    lua_pushnumber(L, (lua_Number) *dt);
                    arg_count += 1;
                }
                break;
            case SCRIPT_FUNCTION_ONMESSAGE:
                {
                    dmMessage::Message* message = (dmMessage::Message*)args;
                    dmScript::PushHash(L, message->m_Id);

                    if (message->m_Descriptor)
                    {
                        message_name = ((const dmDDF::Descriptor*)message->m_Descriptor)->m_Name;
                        dmScript::PushDDF(L, (dmDDF::Descriptor*)message->m_Descriptor, (const char*) message->m_Data, true);
                    }
                    else
                    {
                        if (dmProfile::IsInitialized())
                        {
                            // Try to find the message name via id and reverse hash
                            message_name = (const char*)dmHashReverse64(message->m_Id, 0);
                        }
                        if (message->m_DataSize > 0)
                        {
                            dmScript::PushTable(L, (const char*) message->m_Data, message->m_DataSize);
                        }
                        else
                        {
                            lua_newtable(L);
                        }
                    }

                    dmScript::PushURL(L, message->m_Sender);
                    arg_count += 3;
                }
                break;
            case SCRIPT_FUNCTION_ONINPUT:
                {
                    InputArgs* input_args = (InputArgs*)args;
                    const InputAction* ia = input_args->m_Action;
                    // 0 is reserved for mouse movement
                    if (ia->m_ActionId != 0)
                    {
                        dmScript::PushHash(L, ia->m_ActionId);
                    }
                    else
                    {
                        lua_pushnil(L);
                    }

                    lua_newtable(L);

                    if (ia->m_IsGamepad) {
                        lua_pushliteral(L, "gamepad");
                        lua_pushnumber(L, ia->m_GamepadIndex);
                        lua_settable(L, -3);
                    }

                    if (ia->m_GamepadConnected)
                    {
                        lua_pushlstring(L, ia->m_Text, ia->m_TextCount);
                        lua_setfield(L, -2, "gamepad_name");
                    }

                    if (ia->m_HasGamepadPacket)
                    {
                        dmHID::GamepadPacket gamepadPacket = ia->m_GamepadPacket;
                        lua_pushliteral(L, "gamepad_axis");
                        lua_createtable(L, dmHID::MAX_GAMEPAD_AXIS_COUNT, 0);
                        for (int i = 0; i < dmHID::MAX_GAMEPAD_AXIS_COUNT; ++i)
                        {
                            lua_pushinteger(L, (lua_Integer) (i+1));
                            lua_pushnumber(L, gamepadPacket.m_Axis[i]);
                            lua_settable(L, -3);
                        }
                        lua_settable(L, -3);

                        lua_pushliteral(L, "gamepad_buttons");
                        lua_createtable(L, dmHID::MAX_GAMEPAD_AXIS_COUNT, 0);
                        for (int i = 0; i < dmHID::MAX_GAMEPAD_AXIS_COUNT; ++i)
                        {
                            lua_pushinteger(L, (lua_Integer) (i+1));
                            lua_pushnumber(L, dmHID::GetGamepadButton(&gamepadPacket, i));
                            lua_settable(L, -3);
                        }
                        lua_settable(L, -3);

                        lua_pushliteral(L, "gamepad_hats");
                        lua_createtable(L, dmHID::MAX_GAMEPAD_HAT_COUNT, 0);
                        for (int i = 0; i < dmHID::MAX_GAMEPAD_HAT_COUNT; ++i)
                        {
                            lua_pushinteger(L, (lua_Integer) (i+1));
                            uint8_t hat_value;
                            if (dmHID::GetGamepadHat(&gamepadPacket, i, &hat_value))
                            {
                                lua_pushnumber(L, hat_value);
                            }
                            else
                            {
                                lua_pushnumber(L, 0);
                            }
                            lua_settable(L, -3);
                        }
                        lua_settable(L, -3);
                    }

                    if (ia->m_ActionId != 0)
                    {
                        lua_pushstring(L, "value");
                        lua_pushnumber(L, ia->m_Value);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "pressed");
                        lua_pushboolean(L, ia->m_Pressed);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "released");
                        lua_pushboolean(L, ia->m_Released);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "repeated");
                        lua_pushboolean(L, ia->m_Repeated);
                        lua_rawset(L, -3);
                    }

                    if (ia->m_PositionSet)
                    {
                        lua_pushstring(L, "x");
                        lua_pushnumber(L, ia->m_X);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "y");
                        lua_pushnumber(L, ia->m_Y);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "dx");
                        lua_pushnumber(L, ia->m_DX);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "dy");
                        lua_pushnumber(L, ia->m_DY);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "screen_x");
                        lua_pushnumber(L, ia->m_ScreenX);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "screen_y");
                        lua_pushnumber(L, ia->m_ScreenY);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "screen_dx");
                        lua_pushnumber(L, ia->m_ScreenDX);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "screen_dy");
                        lua_pushnumber(L, ia->m_ScreenDY);
                        lua_rawset(L, -3);
                    }

                    if (ia->m_AccelerationSet)
                    {
                        lua_pushstring(L, "acc_x");
                        lua_pushnumber(L, ia->m_AccX);
                        lua_rawset(L,-3);

                        lua_pushstring(L, "acc_y");
                        lua_pushnumber(L, ia->m_AccY);
                        lua_rawset(L,-3);

                        lua_pushstring(L, "acc_z");
                        lua_pushnumber(L, ia->m_AccZ);
                        lua_rawset(L,-3);
                    }

                    if (ia->m_TouchCount > 0)
                    {
                        int tc = ia->m_TouchCount;
                        lua_pushliteral(L, "touch");
                        lua_createtable(L, tc, 0);
                        for (int i = 0; i < tc; ++i)
                        {
                            const dmHID::Touch& t = ia->m_Touch[i];

                            lua_pushinteger(L, (lua_Integer) (i+1));
                            lua_createtable(L, 0, 6);

                            lua_pushliteral(L, "id");
                            lua_pushinteger(L, (lua_Integer) t.m_Id);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "tap_count");
                            lua_pushinteger(L, (lua_Integer) t.m_TapCount);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "pressed");
                            lua_pushboolean(L, t.m_Phase == dmHID::PHASE_BEGAN);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "released");
                            lua_pushboolean(L, t.m_Phase == dmHID::PHASE_ENDED || t.m_Phase == dmHID::PHASE_CANCELLED);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "x");
                            lua_pushinteger(L, (lua_Integer) t.m_X);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "y");
                            lua_pushinteger(L, (lua_Integer) t.m_Y);
                            lua_settable(L, -3);

                            lua_pushstring(L, "screen_x");
                            lua_pushnumber(L, (lua_Integer) t.m_ScreenX);
                            lua_rawset(L, -3);

                            lua_pushstring(L, "screen_y");
                            lua_pushnumber(L, (lua_Integer) t.m_ScreenY);
                            lua_rawset(L, -3);

                            lua_pushliteral(L, "dx");
                            lua_pushinteger(L, (lua_Integer) t.m_DX);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "dy");
                            lua_pushinteger(L, (lua_Integer) t.m_DY);
                            lua_settable(L, -3);

                            lua_pushstring(L, "screen_dx");
                            lua_pushnumber(L, (lua_Integer) t.m_ScreenDX);
                            lua_rawset(L, -3);

                            lua_pushstring(L, "screen_dy");
                            lua_pushnumber(L, (lua_Integer) t.m_ScreenDY);
                            lua_rawset(L, -3);

                            lua_settable(L, -3);
                        }
                        lua_settable(L, -3);
                    }

                    if (ia->m_HasText)
                    {
                        lua_pushliteral(L, "text");
                        if (ia->m_TextCount == 0) {
                            lua_pushstring(L, "");
                        } else {
                            lua_pushlstring(L, ia->m_Text, ia->m_TextCount);
                        }

                        lua_settable(L, -3);
                    }

                    arg_count += 2;
                }
                break;
            default:
                break;
            }

            Result result = RESULT_OK;

            {
                char buffer[128];
                const char* profiler_string = dmScript::GetProfilerString(L, custom_ref != LUA_NOREF ? -5 : 0, scene->m_Script->m_SourceFileName, SCRIPT_FUNCTION_NAMES[script_function], message_name, buffer, sizeof(buffer));
                DM_PROFILE_DYN(profiler_string, 0);

                if (dmScript::PCall(L, arg_count, LUA_MULTRET) != 0)
                {
                    assert(top == lua_gettop(L));
                    result = RESULT_SCRIPT_ERROR;
                }
            }

            if (result == RESULT_OK)
            {
                switch (script_function)
                {
                case SCRIPT_FUNCTION_ONINPUT:
                    {
                        InputArgs* input_args = (InputArgs*)args;
                        int ret_count = lua_gettop(L) - top;
                        if (ret_count == 1 && lua_isboolean(L, -1))
                        {
                            input_args->m_Consumed = (bool) lua_toboolean(L, -1);
                            lua_pop(L, 1);
                        }
                        else if (ret_count != 0)
                        {
                            dmLogError("The function %s must either return true/false, or no value at all.", SCRIPT_FUNCTION_NAMES[script_function]);
                            result = RESULT_SCRIPT_ERROR;
                            lua_settop(L, top);
                        }
                    }
                    break;
                default:
                    if (lua_gettop(L) - top != (int32_t)ret_count)
                    {
                        dmLogError("The function %s must have exactly %d return values.", SCRIPT_FUNCTION_NAMES[script_function], ret_count);
                        result = RESULT_SCRIPT_ERROR;
                        lua_settop(L, top);
                    }
                    break;
                }
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);
            assert(top == lua_gettop(L));
            return result;
        }
        assert(top == lua_gettop(L));
        return RESULT_OK;
    }

    Result InitScene(HScene scene)
    {
        return RunScript(scene, SCRIPT_FUNCTION_INIT, LUA_NOREF, 0x0);
    }

    Result FinalScene(HScene scene)
    {
        Result result = RunScript(scene, SCRIPT_FUNCTION_FINAL, LUA_NOREF, 0x0);

        // Deferred deletion of nodes
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &nodes[i];

            if (node->m_Deleted)
            {
                HNode hnode = GetNodeHandle(node);
                DeleteNode(scene, hnode, true);
                node->m_Deleted = 0; // Make sure to clear deferred delete flag
                n = scene->m_Nodes.Size();
            }
        }

        // Destroy all living particlefx instances
        uint32_t count = scene->m_AliveParticlefxs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticlefxComponent* c = &scene->m_AliveParticlefxs[i];
            dmParticle::DestroyInstance(scene->m_ParticlefxContext, c->m_Instance);
        }
        scene->m_AliveParticlefxs.SetSize(0);

        ClearLayouts(scene);
        return result;
    }

    Result UpdateScene(HScene scene, float dt)
    {
        Result result = RunScript(scene, SCRIPT_FUNCTION_UPDATE, LUA_NOREF, (void*)&dt);

        uint32_t node_count = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();

        if (dLib::IsDebugMode())
        {
            for (uint32_t i = 0; i < node_count; ++i)
            {
                InternalNode* node = &nodes[i];
                if (!node->m_Deleted)
                {
                    UpdateTextureSetAnimData(scene, node);
                }
            }
        }

        UpdateAnimations(scene, dt);

        uint32_t total_nodes = 0;
        uint32_t active_nodes = 0;
        node_count = scene->m_Nodes.Size();
        nodes      = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < node_count; ++i)
        {
            InternalNode* node = &nodes[i];

            // Deferred deletion of nodes
            if (node->m_Deleted)
            {
                DeleteNode(scene, GetNodeHandle(node), false);
                node->m_Deleted = 0; // Make sure to clear deferred delete flag
                node_count = scene->m_Nodes.Size();
            }
            else if (node->m_Index != INVALID_INDEX)
            {
                ++total_nodes;
                if (node->m_Node.m_Enabled)
                    ++active_nodes;

                if (node->m_Node.m_CustomType != 0)
                {
                    scene->m_UpdateCustomNodeCallback(scene->m_CreateCustomNodeCallbackContext, scene, GetNodeHandle(node),
                                                        node->m_Node.m_CustomType, node->m_Node.m_CustomData, dt);
                }
            }
        }

        // Prune sleeping pfx instances
        uint32_t count = scene->m_AliveParticlefxs.Size();
        uint32_t i = 0;
        while (i < count)
        {
            ParticlefxComponent* c = &scene->m_AliveParticlefxs[i];
            if (dmParticle::IsSleeping(scene->m_ParticlefxContext, c->m_Instance))
            {
                if (c->m_Node != INVALID_HANDLE)
                {
                    InternalNode* n = GetNode(scene, c->m_Node);

                    if (n->m_Node.m_ParticleInstance == c->m_Instance)
                    {
                        n->m_Node.m_ParticleInstance = dmParticle::INVALID_INSTANCE;
                    }

                    if (n->m_Node.m_HasHeadlessPfx)
                    {
                        // Mark all ParticlefxComponents that reference this node
                        // so we don't try to delete the same internal node more than once
                        HNode hnode = c->m_Node;
                        for (uint32_t i_inner = 0; i_inner < count; ++i_inner)
                        {
                            ParticlefxComponent* c_inner = &scene->m_AliveParticlefxs[i_inner];
                            if (c_inner->m_Node == hnode)
                            {
                                c_inner->m_Node = INVALID_HANDLE; // makes the callbacks return nil to the user
                            }
                        }

                        ResetInternalNode(scene, n);
                    }
                }

                dmParticle::DestroyInstance(scene->m_ParticlefxContext, c->m_Instance);
                scene->m_AliveParticlefxs.EraseSwap(i);
                --count;
            }
            else
            {
                ++i;
            }
        }

        DM_PROPERTY_ADD_U32(rmtp_GuiNodes, total_nodes);
        DM_PROPERTY_ADD_U32(rmtp_GuiActiveNodes, active_nodes);
        DM_PROPERTY_ADD_U32(rmtp_GuiStaticTextures, scene->m_Textures.Size());
        DM_PROPERTY_ADD_U32(rmtp_GuiDynamicTextures, scene->m_DynamicTextures.Size());
        DM_PROPERTY_ADD_U32(rmtp_GuiTextures, scene->m_Textures.Size() + scene->m_DynamicTextures.Size());
        DM_PROPERTY_ADD_U32(rmtp_GuiParticlefx, scene->m_AliveParticlefxs.Size());

        return result;
    }

    HNode GetFirstChildNode(HScene scene, HNode node)
    {
        assert(scene != 0);
        if (!node)
        {
            uint32_t node_count = scene->m_Nodes.Size();
            InternalNode* nodes = scene->m_Nodes.Begin();
            for (uint32_t i = 0; i < node_count; ++i)
            {
                InternalNode* node = &nodes[i];
                if (!node->m_Deleted && node->m_Index != INVALID_INDEX && node->m_ParentIndex == INVALID_INDEX)
                {
                    return GetNodeHandle(node);
                }
            }
            return INVALID_HANDLE;
        }

        InternalNode* n = GetNode(scene, node);
        uint16_t child_index = n->m_ChildHead;
        while (child_index != INVALID_INDEX)
        {
            InternalNode* child = &scene->m_Nodes[child_index & 0xffff];
            child_index = child->m_NextIndex;
            if (!child->m_Deleted && child->m_Index != INVALID_INDEX)
                return GetNodeHandle(child);
        }
        return INVALID_HANDLE;
    }

    HNode GetNextNode(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        uint16_t next_index = n->m_NextIndex;
        while (next_index != INVALID_INDEX)
        {
            InternalNode* next = &scene->m_Nodes[next_index & 0xffff];
            next_index = next->m_NextIndex;

            if (!next->m_Deleted && next->m_Index != INVALID_INDEX)
                return GetNodeHandle(next);
        }
        return INVALID_HANDLE;
    }

    Result DispatchMessage(HScene scene, dmMessage::Message* message)
    {
        int custom_ref = LUA_NOREF;
        if (message->m_UserData2) {
            // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, see message.h in dlib
            custom_ref = message->m_UserData2 + LUA_NOREF;
            // RunScript method will DeRef the custom_ref if it is not LUA_NOREF
        }

        Result r = RunScript(scene, SCRIPT_FUNCTION_ONMESSAGE, custom_ref, (void*)message);

        return r;
    }

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count, bool* input_consumed)
    {
        InputArgs args;
        args.m_Consumed = false;
        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            args.m_Action = &input_actions[i];
            Result result = RunScript(scene, SCRIPT_FUNCTION_ONINPUT, LUA_NOREF, (void*)&args);
            if (result != RESULT_OK)
            {
                return result;
            }
            else
            {
                input_consumed[i] = args.m_Consumed;
            }
        }

        return RESULT_OK;
    }

    Result ReloadScene(HScene scene)
    {
        return RunScript(scene, SCRIPT_FUNCTION_ONRELOAD, LUA_NOREF, 0x0);
    }

    Result SetSceneScript(HScene scene, HScript script)
    {
        scene->m_Script = script;
        return RESULT_OK;
    }

    HScript GetSceneScript(HScene scene)
    {
        return scene->m_Script;
    }

    static uint32_t AllocateNode(HScene scene)
    {
        if (scene->m_NodePool.Remaining() == 0)
        {
            return scene->m_NodePool.Capacity();
        }
        uint16_t index = scene->m_NodePool.Pop();
        if (index >= scene->m_Nodes.Size())
        {
            scene->m_Nodes.SetSize(index + 1);
        }
        return index;
    }

    HNode NewNode(HScene scene, const Point3& position, const Vector3& size, NodeType node_type, uint32_t custom_type)
    {
        uint16_t index = AllocateNode(scene);
        if (index == scene->m_NodePool.Capacity())
        {
            dmLogError("Could not create the node since the buffer is full (%d).", scene->m_NodePool.Capacity());
            return 0;
        }
        uint16_t version = scene->m_NextVersionNumber;
        if (version == 0)
        {
            // We can't use zero in order to avoid a handle == 0
            ++version;
        }

        InternalNode* node = &scene->m_Nodes[index];
        memset(node, 0, sizeof(InternalNode));
        node->m_Node.m_Properties[PROPERTY_POSITION] = Vector4(Vector3(position), 1);
        node->m_Node.m_Properties[PROPERTY_ROTATION] = Vector4(0);
        node->m_Node.m_Properties[PROPERTY_SCALE] = Vector4(1,1,1,0);
        node->m_Node.m_Properties[PROPERTY_COLOR] = Vector4(1,1,1,1);
        node->m_Node.m_Properties[PROPERTY_OUTLINE] = Vector4(0,0,0,1);
        node->m_Node.m_Properties[PROPERTY_SHADOW] = Vector4(0,0,0,1);
        node->m_Node.m_Properties[PROPERTY_SIZE] = Vector4(size, 0);
        node->m_Node.m_Properties[PROPERTY_SLICE9] = Vector4(0,0,0,0);
        node->m_Node.m_Properties[PROPERTY_PIE_PARAMS] = Vector4(0,360,0,0);
        node->m_Node.m_Properties[PROPERTY_TEXT_PARAMS] = Vector4(1, 0, 0, 0);
        node->m_Node.m_LocalTransform = Matrix4::identity();
        node->m_Node.m_LocalAdjustScale = Vector4(1.0, 1.0, 1.0, 1.0);
        node->m_Node.m_PerimeterVertices = 32;
        node->m_Node.m_OuterBounds = PIEBOUNDS_ELLIPSE;
        node->m_Node.m_BlendMode = 0;
        node->m_Node.m_NodeType = (uint32_t) node_type;
        node->m_Node.m_CustomType = custom_type;
        node->m_Node.m_XAnchor = 0;
        node->m_Node.m_YAnchor = 0;
        node->m_Node.m_Pivot = 0;
        node->m_Node.m_AdjustMode = 0;
        node->m_Node.m_SizeMode = SIZE_MODE_MANUAL;
        node->m_Node.m_LineBreak = 0;
        node->m_Node.m_Enabled = 1;
        node->m_Node.m_IsVisible = 1;
        node->m_Node.m_DirtyLocal = 1;
        node->m_Node.m_InheritAlpha = 0;
        node->m_Node.m_ClippingMode = CLIPPING_MODE_NONE;
        node->m_Node.m_ClippingVisible = true;
        node->m_Node.m_ClippingInverted = false;

        node->m_Node.m_HasResetPoint = 0;
        node->m_Node.m_TextureHash = 0;
        node->m_Node.m_Texture = 0;
        node->m_Node.m_TextureType = NODE_TEXTURE_TYPE_NONE;
        node->m_Node.m_TextureSetAnimDesc.Init();
        node->m_Node.m_FlipbookAnimHash = 0;
        node->m_Node.m_FlipbookAnimPosition = 0.0f;
        node->m_Node.m_FontHash = 0;
        node->m_Node.m_Font = 0;
        node->m_Node.m_HasHeadlessPfx = 0;
        node->m_Node.m_LayerHash = DEFAULT_LAYER;
        node->m_Node.m_LayerIndex = 0;
        node->m_Node.m_NodeDescTable = 0;
        node->m_Version = version;
        node->m_Index = index;
        node->m_PrevIndex = INVALID_INDEX;
        node->m_NextIndex = INVALID_INDEX;
        node->m_ParentIndex = INVALID_INDEX;
        node->m_ChildHead = INVALID_INDEX;
        node->m_ChildTail = INVALID_INDEX;
        node->m_SceneTraversalCacheVersion = INVALID_INDEX;
        node->m_ClipperIndex = INVALID_INDEX;
        scene->m_NextVersionNumber = (version + 1) % ((1 << 16) - 1);

        HNode hnode = GetNodeHandle(node);

        if (custom_type != 0)
        {
            void* custom_node_data = scene->m_CreateCustomNodeCallback(scene->m_CreateCustomNodeCallbackContext, scene, hnode, custom_type);
            node->m_Node.m_CustomData = custom_node_data;
        }

        MoveNodeAbove(scene, hnode, INVALID_HANDLE);
        return hnode;
    }

    void SetNodeId(HScene scene, HNode node, dmhash_t id)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_NameHash = id;
    }

    void SetNodeId(HScene scene, HNode node, const char* id)
    {
        SetNodeId(scene, node, dmHashString64(id));
    }

    dmhash_t GetNodeId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_NameHash;
    }

    HNode GetNodeById(HScene scene, const char* id)
    {
        dmhash_t name_hash = dmHashString64(id);
        return GetNodeById(scene, name_hash);
    }

    HNode GetNodeById(HScene scene, dmhash_t id)
    {
        uint32_t n = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        HNode foundNode = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &nodes[i];
            if (node->m_NameHash == id)
            {
                foundNode = GetNodeHandle(node);
                // https://github.com/defold/defold/issues/3481
                // The node may have been deleted and another node given the
                // same id in the same frame. If we find a node with the correct
                // id but flagged for deferred delete we keep looking for
                // another one
                if (!node->m_Deleted)
                {
                    break;
                }
            }
        }
        return foundNode;
    }

    uint32_t GetNodeCount(HScene scene)
    {
        return scene->m_NodePool.Size();
    }

    uint32_t GetParticlefxCount(HScene scene)
    {
        return scene->m_AliveParticlefxs.Size();
    }

    static void GetNodeList(HScene scene, InternalNode* n, uint16_t** out_head, uint16_t** out_tail)
    {
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            *out_head = &parent->m_ChildHead;
            *out_tail = &parent->m_ChildTail;
        }
        else
        {
            *out_head = &scene->m_RenderHead;
            *out_tail = &scene->m_RenderTail;
        }
    }

    static void AddToNodeList(HScene scene, InternalNode* n, InternalNode* parent_n, InternalNode* prev_n)
    {
        uint16_t* head = &scene->m_RenderHead, * tail = &scene->m_RenderTail;
        uint16_t parent_index = INVALID_INDEX;
        if (parent_n != 0x0)
        {
            parent_index = parent_n->m_Index;
            head = &parent_n->m_ChildHead;
            tail = &parent_n->m_ChildTail;
        }
        n->m_ParentIndex = parent_index;
        if (prev_n != 0x0)
        {
            if (*tail == prev_n->m_Index)
            {
                *tail = n->m_Index;
                n->m_NextIndex = INVALID_INDEX;
            }
            else if (prev_n->m_NextIndex != INVALID_INDEX)
            {
                InternalNode* next_n = &scene->m_Nodes[prev_n->m_NextIndex];
                next_n->m_PrevIndex = n->m_Index;
                n->m_NextIndex = prev_n->m_NextIndex;
            }
            prev_n->m_NextIndex = n->m_Index;
            n->m_PrevIndex = prev_n->m_Index;
        }
        else
        {
            n->m_PrevIndex = INVALID_INDEX;
            n->m_NextIndex = *head;
            if (*head != INVALID_INDEX)
            {
                InternalNode* next_n = &scene->m_Nodes[*head];
                next_n->m_PrevIndex = n->m_Index;
            }
            *head = n->m_Index;
            if (*tail == INVALID_INDEX)
            {
                *tail = n->m_Index;
            }
        }
    }

    static void RemoveFromNodeList(HScene scene, InternalNode* n)
    {
        // Remove from list
        if (n->m_PrevIndex != INVALID_INDEX)
            scene->m_Nodes[n->m_PrevIndex].m_NextIndex = n->m_NextIndex;
        if (n->m_NextIndex != INVALID_INDEX)
            scene->m_Nodes[n->m_NextIndex].m_PrevIndex = n->m_PrevIndex;
        uint16_t* head_ptr = 0x0, * tail_ptr = 0x0;
        GetNodeList(scene, n, &head_ptr, &tail_ptr);

        if (*head_ptr == n->m_Index)
            *head_ptr = n->m_NextIndex;
        if (*tail_ptr == n->m_Index)
            *tail_ptr = n->m_PrevIndex;
    }

    static inline void ResetInternalNode(HScene scene, InternalNode* n)
    {
        RemoveFromNodeList(scene, n);
        uint16_t node_index = n->m_Index;
        scene->m_NodePool.Push(node_index);
        if (node_index + 1 == scene->m_Nodes.Size())
        {
            scene->m_Nodes.SetSize(node_index);
        }
        if (n->m_Node.m_Text)
            free((void*)n->m_Node.m_Text);
        memset(n, 0, sizeof(InternalNode));
        n->m_Index = INVALID_INDEX;
    }

    void DeleteNode(HScene scene, HNode node, bool delete_headless_pfx)
    {
        InternalNode* n = GetNode(scene, node);

        if (n->m_Node.m_CustomType != 0)
        {
            scene->m_DestroyCustomNodeCallback(scene->m_CreateCustomNodeCallbackContext, scene, node, n->m_Node.m_CustomType, n->m_Node.m_CustomData);
        }

        // Stop (or destroy) any living particle instances started on this node
        uint32_t count = scene->m_AliveParticlefxs.Size();
        uint32_t i = 0;
        if (n->m_Node.m_NodeType == NODE_TYPE_PARTICLEFX)
        {
            while (i < count)
            {
                ParticlefxComponent* c = &scene->m_AliveParticlefxs[i];
                if (node == c->m_Node)
                {
                    if (delete_headless_pfx)
                    {
                        InternalNode* comp_n = GetNode(scene, c->m_Node);
                        dmParticle::DestroyInstance(scene->m_ParticlefxContext, comp_n->m_Node.m_ParticleInstance);
                        n->m_Node.m_ParticleInstance = dmParticle::INVALID_INSTANCE;
                        scene->m_AliveParticlefxs.EraseSwap(i);
                        --count;
                    }
                    else
                    {
                        dmParticle::StopInstance(scene->m_ParticlefxContext, c->m_Instance, false);
                        n->m_Node.m_HasHeadlessPfx = 1;
                        ++i;
                    }
                }
                else
                {
                    ++i;
                }
            }
        }

        // Delete children first
        uint16_t child_index = n->m_ChildHead;
        while (child_index != INVALID_INDEX)
        {
            InternalNode* child = &scene->m_Nodes[child_index & 0xffff];
            child_index = child->m_NextIndex;
            DeleteNode(scene, GetNodeHandle(child), delete_headless_pfx);
        }

        dmArray<Animation> *animations = &scene->m_Animations;
        uint32_t n_anims = animations->Size();
        for (uint32_t i = 0; i < n_anims; ++i)
        {
            Animation* anim = &(*animations)[i];

            if (anim->m_Node == node)
            {
                CompleteAnimation(scene, anim, false);
                RemoveAnimation(*animations, i);
                i--;
                n_anims--;
                continue;
            }
        }

        if (!delete_headless_pfx && n->m_Node.m_HasHeadlessPfx)
        {
            RemoveFromNodeList(scene, n);
            n->m_ParentIndex = INVALID_INDEX;
            n->m_PrevIndex = INVALID_INDEX;
            n->m_NextIndex = INVALID_INDEX;
            return;
        }

        ResetInternalNode(scene, n);
    }


    void DeleteNode(HScene scene, HNode node)
    {
        DeleteNode(scene, node, false);
    }

    void ClearNodes(HScene scene)
    {
        scene->m_Nodes.SetSize(0);
        scene->m_RenderHead = INVALID_INDEX;
        scene->m_RenderTail = INVALID_INDEX;
        scene->m_NodePool.Clear();
        scene->m_Animations.SetSize(0);
    }

    static Vector4 ApplyAdjustOnReferenceScale(const Vector4& reference_scale, uint32_t adjust_mode)
    {
        Vector4 parent_adjust_scale = reference_scale;
        if (adjust_mode == dmGui::ADJUST_MODE_FIT)
        {
            float uniform = dmMath::Min(reference_scale.getX(), reference_scale.getY());
            parent_adjust_scale.setX(uniform);
            parent_adjust_scale.setY(uniform);
        }
        else if (adjust_mode == dmGui::ADJUST_MODE_ZOOM)
        {
            float uniform = dmMath::Max(reference_scale.getX(), reference_scale.getY());
            parent_adjust_scale.setX(uniform);
            parent_adjust_scale.setY(uniform);
        }
        parent_adjust_scale.setZ(1.0f);
        parent_adjust_scale.setW(1.0f);
        return parent_adjust_scale;
    }

    static void AdjustPosScale(HScene scene, InternalNode* n, const Vector4& reference_scale, Vector4& position, Vector4& scale)
    {
        if (scene->m_AdjustReference == ADJUST_REFERENCE_LEGACY && n->m_ParentIndex != INVALID_INDEX)
        {
            return;
        }

        Node& node = n->m_Node;
        // Apply ref-scaling to scale uniformly, select the smallest scale component to make sure everything fits
        Vector4 adjust_scale = ApplyAdjustOnReferenceScale(reference_scale, node.m_AdjustMode);

        Context* context = scene->m_Context;
        Vector4 parent_dims;

        if (scene->m_AdjustReference == ADJUST_REFERENCE_LEGACY || n->m_ParentIndex == INVALID_INDEX) {
            parent_dims = Vector4((float) scene->m_Width, (float) scene->m_Height, 0.0f, 1.0f);
        } else {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            parent_dims = Vector4(parent->m_Node.m_Properties[dmGui::PROPERTY_SIZE].getX(), parent->m_Node.m_Properties[dmGui::PROPERTY_SIZE].getY(), 0.0f, 1.0f);
        }

        Vector4 offset = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        Vector4 adjusted_dims = mulPerElem(parent_dims, adjust_scale);
        Vector4 ref_size;
        if (scene->m_AdjustReference == ADJUST_REFERENCE_LEGACY || n->m_ParentIndex == INVALID_INDEX) {
            ref_size = Vector4((float) context->m_PhysicalWidth, (float) context->m_PhysicalHeight, 0.0f, 1.0f);

            // need to calculate offset for root nodes, since (0,0) is in middle of scene
            offset = (ref_size - adjusted_dims) * 0.5f;
        } else {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            ref_size = Vector4(parent->m_Node.m_Properties[dmGui::PROPERTY_SIZE].getX() * reference_scale.getX(), parent->m_Node.m_Properties[dmGui::PROPERTY_SIZE].getY() * reference_scale.getY(), 0.0f, 1.0f);
        }

        // Apply anchoring
        Vector4 scaled_position = mulPerElem(position, adjust_scale);
        if (node.m_XAnchor == XANCHOR_LEFT || node.m_XAnchor == XANCHOR_RIGHT)
        {
            offset.setX(0.0f);
            scaled_position.setX(position.getX() * reference_scale.getX());
        }

        if (node.m_YAnchor == YANCHOR_TOP || node.m_YAnchor == YANCHOR_BOTTOM)
        {
            offset.setY(0.0f);
            scaled_position.setY(position.getY() * reference_scale.getY());
        }

        position = scaled_position + offset;
        scale = mulPerElem(adjust_scale, scale);
    }

    void UpdateLocalTransform(HScene scene, InternalNode* n)
    {
        Node& node = n->m_Node;

        Vector4 position = node.m_Properties[dmGui::PROPERTY_POSITION];
        Vector4 prop_scale = node.m_Properties[dmGui::PROPERTY_SCALE];
        Vector4 reference_scale = Vector4(1.0, 1.0, 1.0, 1.0);
        node.m_LocalAdjustScale = reference_scale;
        if (scene->m_AdjustReference != ADJUST_REFERENCE_DISABLED) {
            reference_scale = CalculateReferenceScale(scene, n);
            AdjustPosScale(scene, n, reference_scale, position, node.m_LocalAdjustScale);
        }
        const Vector3& rotation = node.m_Properties[dmGui::PROPERTY_ROTATION].getXYZ();
        Quat r = dmVMath::EulerToQuat(rotation);
        r = normalize(r);

        node.m_LocalTransform.setUpper3x3(Matrix3::rotation(r) * Matrix3::scale( mulPerElem(node.m_LocalAdjustScale, prop_scale).getXYZ() ));
        node.m_LocalTransform.setTranslation(position.getXYZ());

        if (scene->m_AdjustReference == ADJUST_REFERENCE_PARENT && n->m_ParentIndex != INVALID_INDEX)
        {
            // undo parent scale (if node has parent)
            Vector3 inv_ref_scale = Vector3(1.0f / reference_scale.getX(), 1.0f / reference_scale.getY(), 1.0f / reference_scale.getZ());
            node.m_LocalTransform = Matrix4::scale( inv_ref_scale ) * node.m_LocalTransform;
        }

        node.m_DirtyLocal = 0;
    }

    void ResetNodes(HScene scene)
    {
        uint32_t n_nodes = scene->m_Nodes.Size();
        InternalNode* nodes = scene->m_Nodes.Begin();
        for (uint32_t i = 0; i < n_nodes; ++i) {
            InternalNode* node = &nodes[i];
            Node* n = &node->m_Node;
            if (n->m_HasResetPoint) {
                memcpy(n->m_Properties, n->m_ResetPointProperties, sizeof(n->m_Properties));
                n->m_DirtyLocal = 1;
                n->m_State = n->m_ResetPointState;
            }
        }
        scene->m_Animations.SetSize(0);
    }

    uint16_t GetRenderOrder(HScene scene)
    {
        return scene->m_RenderOrder;
    }

    NodeType GetNodeType(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (NodeType)n->m_Node.m_NodeType;
    }

    uint32_t GetNodeCustomType(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_CustomType;
    }

    void* GetNodeCustomData(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_CustomData;
    }

    Point3 GetNodePosition(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return Point3(n->m_Node.m_Properties[PROPERTY_POSITION].getXYZ());
    }

    Point3 GetNodeSize(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return Point3(n->m_Node.m_Properties[PROPERTY_SIZE].getXYZ());
    }

    Matrix4 GetNodeWorldTransform(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        Matrix4 world;
        CalculateNodeTransform(scene, n, CalculateNodeTransformFlags(), world);
        return world;
    }

    Vector4 GetNodeSlice9(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_SLICE9];
    }

    void SetNodePosition(HScene scene, HNode node, const Point3& position)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_POSITION] = Vector4(position);
        n->m_Node.m_DirtyLocal = 1;
    }

    bool HasPropertyHash(HScene scene, HNode node, dmhash_t property)
    {
        PropDesc* pd = GetPropertyDesc(property);
        return pd != 0;
    }

    Vector4 GetNodeProperty(HScene scene, HNode node, Property property)
    {
        assert(property < PROPERTY_COUNT);
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[property];
    }

    Vector4 GetNodePropertyHash(HScene scene, HNode node, dmhash_t property)
    {
        InternalNode* n = GetNode(scene, node);
        PropDesc* pd = GetPropertyDesc(property);
        if (pd) {
            Vector4* base_value = &n->m_Node.m_Properties[pd->m_Property];

            if (pd->m_Component == 0xff) {
                return *base_value;
            } else {
                return Vector4(base_value->getElem(pd->m_Component));
            }
        }
        dmLogError("Property '%s' not found", dmHashReverseSafe64(property));
        return Vector4(0, 0, 0, 0);
    }

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value)
    {
        assert(property < PROPERTY_COUNT);
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[property] = value;
        n->m_Node.m_DirtyLocal = 1;
    }

    void SetNodeResetPoint(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        memcpy(n->m_Node.m_ResetPointProperties, n->m_Node.m_Properties, sizeof(n->m_Node.m_Properties));
        n->m_Node.m_ResetPointState = n->m_Node.m_State;
        n->m_Node.m_HasResetPoint = 1;
    }

    const char* GetNodeText(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Text;
    }

    void SetNodeText(HScene scene, HNode node, const char* text)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_Text)
            free((void*) n->m_Node.m_Text);

        if (text)
            n->m_Node.m_Text = strdup(text);
        else
            n->m_Node.m_Text = 0;
    }

    void SetNodeLineBreak(HScene scene, HNode node, bool line_break)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_LineBreak = line_break;
    }

    bool GetNodeLineBreak(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_LineBreak;
    }

    void SetNodeTextLeading(HScene scene, HNode node, float leading)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_TEXT_PARAMS].setX(leading);
    }
    float GetNodeTextLeading(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_TEXT_PARAMS].getX();
    }

    void SetNodeTextTracking(HScene scene, HNode node, float tracking)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_TEXT_PARAMS].setY(tracking);
    }
    float GetNodeTextTracking(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_TEXT_PARAMS].getY();
    }

    void* GetNodeTexture(HScene scene, HNode node, NodeTextureType* textureTypeOut)
    {
        InternalNode* n = GetNode(scene, node);
        *textureTypeOut = n->m_Node.m_TextureType;
        return n->m_Node.m_Texture;
    }

    dmhash_t GetNodeTextureId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_TextureHash;
    }

    dmhash_t GetNodeFlipbookAnimId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_TextureType == NODE_TEXTURE_TYPE_TEXTURE_SET ? n->m_Node.m_FlipbookAnimHash : 0x0;
    }

    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_TextureType == NODE_TEXTURE_TYPE_TEXTURE_SET)
            CancelNodeFlipbookAnim(scene, node);
        if (TextureInfo* texture_info = scene->m_Textures.Get(texture_id)) {
            n->m_Node.m_TextureHash = texture_id;
            n->m_Node.m_Texture = texture_info->m_TextureSource;
            n->m_Node.m_TextureType = texture_info->m_TextureSourceType;

            if((n->m_Node.m_SizeMode != SIZE_MODE_MANUAL) &&
                (n->m_Node.m_NodeType != NODE_TYPE_CUSTOM) &&
                (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) && (texture_info->m_TextureSource))
            {
                n->m_Node.m_Properties[PROPERTY_SIZE][0] = texture_info->m_OriginalWidth;
                n->m_Node.m_Properties[PROPERTY_SIZE][1] = texture_info->m_OriginalHeight;
            }
            return RESULT_OK;
        } else if (DynamicTexture* texture = scene->m_DynamicTextures.Get(texture_id)) {
            n->m_Node.m_TextureHash = texture_id;
            n->m_Node.m_Texture = texture->m_Handle;
            n->m_Node.m_TextureType = NODE_TEXTURE_TYPE_DYNAMIC;
            if((n->m_Node.m_SizeMode != SIZE_MODE_MANUAL) &&
                (n->m_Node.m_NodeType != NODE_TYPE_CUSTOM) &&
                (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX))
            {
                n->m_Node.m_Properties[PROPERTY_SIZE][0] = texture->m_Width;
                n->m_Node.m_Properties[PROPERTY_SIZE][1] = texture->m_Height;
            }
            return RESULT_OK;
        }
        n->m_Node.m_Texture = 0;
        n->m_Node.m_TextureType = NODE_TEXTURE_TYPE_NONE;
        return RESULT_RESOURCE_NOT_FOUND;
    }

    Result SetNodeTexture(HScene scene, HNode node, const char* texture_id)
    {
        return SetNodeTexture(scene, node, dmHashString64(texture_id));
    }

    Result SetNodeTexture(HScene scene, HNode node, NodeTextureType type, void* texture)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_TextureHash = (uintptr_t)texture;
        n->m_Node.m_TextureType = type;
        n->m_Node.m_Texture = texture;
        return RESULT_OK;
    }

    Result SetNodeParticlefx(HScene scene, HNode node, dmhash_t particlefx_id)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        if (scene->m_Particlefxs.Get(particlefx_id) == 0) {
            return RESULT_RESOURCE_NOT_FOUND;
        }

        n->m_Node.m_ParticlefxHash = particlefx_id;
        return RESULT_OK;
    }

    Result GetNodeParticlefx(HScene scene, HNode node, dmhash_t& particlefx_id)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        particlefx_id = n->m_Node.m_ParticlefxHash;
        return RESULT_OK;
    }

    Result SetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id, Vector4& value)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        dmParticle::HParticleContext context = scene->m_ParticlefxContext;
        // Set render constant on all particle instances spawned by this node
        uint32_t count = scene->m_AliveParticlefxs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticlefxComponent* c = &scene->m_AliveParticlefxs[i];
            InternalNode* comp_n = GetNode(scene, c->m_Node);
            if (comp_n->m_Index == n->m_Index && comp_n->m_Version == n->m_Version)
            {
                dmParticle::SetRenderConstant(context, c->m_Instance, emitter_id, constant_id, value);
            }
        }

        return RESULT_OK;
    }

    Result ResetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        dmParticle::HParticleContext context = scene->m_ParticlefxContext;
        // Reset render constant on all particle instances spawned by this node
        uint32_t count = scene->m_AliveParticlefxs.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticlefxComponent* c = &scene->m_AliveParticlefxs[i];
            InternalNode* comp_n = GetNode(scene, c->m_Node);
            if (comp_n->m_Index == n->m_Index && comp_n->m_Version == n->m_Version)
            {
                dmParticle::ResetRenderConstant(context, c->m_Instance, emitter_id, constant_id);
            }
        }

        return RESULT_OK;
    }

    void* GetNodeFont(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Font;
    }

    dmhash_t GetNodeFontId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_FontHash;
    }

    Result SetNodeFont(HScene scene, HNode node, dmhash_t font_id)
    {
        void** font = scene->m_Fonts.Get(font_id);
        if (font)
        {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_FontHash = font_id;
            n->m_Node.m_Font = *font;
            return RESULT_OK;
        }
        else
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
    }

    Result SetNodeFont(HScene scene, HNode node, const char* font_id)
    {
        return SetNodeFont(scene, node, dmHashString64(font_id));
    }

    dmhash_t GetNodeLayerId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_LayerHash;
    }

    Result SetNodeLayer(HScene scene, HNode node, dmhash_t layer_id)
    {
        uint16_t* layer_index = scene->m_Layers.Get(layer_id);
        if (layer_index)
        {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_LayerHash = layer_id;
            n->m_Node.m_LayerIndex = *layer_index;
            return RESULT_OK;
        }
        else
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
    }

    Result SetNodeLayer(HScene scene, HNode node, const char* layer_id)
    {
        return SetNodeLayer(scene, node, dmHashString64(layer_id));
    }

    bool GetNodeInheritAlpha(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_InheritAlpha != 0;
    }

    void SetNodeInheritAlpha(HScene scene, HNode node, bool inherit_alpha)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_InheritAlpha = inherit_alpha;
    }

    void SetNodeAlpha(HScene scene, HNode node, float alpha)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_COLOR].setW(alpha);
    }

    float GetNodeAlpha(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_COLOR].getW();
    }

    float GetNodeFlipbookCursor(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        float t = n->m_Node.m_FlipbookAnimPosition;

        return t;
    }

    void SetNodeFlipbookCursor(HScene scene, HNode node, float cursor)
    {
        InternalNode* n = GetNode(scene, node);

        cursor = dmMath::Clamp(cursor, 0.0f, 1.0f);
        n->m_Node.m_FlipbookAnimPosition = cursor;
        if (n->m_Node.m_FlipbookAnimHash) {
            Animation* anim = GetComponentAnimation(scene, node, &n->m_Node.m_FlipbookAnimPosition);
            if (anim) {

                if (anim->m_Playback == PLAYBACK_ONCE_BACKWARD || anim->m_Playback == PLAYBACK_LOOP_BACKWARD)
                {
                    cursor = 1.0f - cursor;
                } else if (anim->m_Playback == PLAYBACK_ONCE_PINGPONG || anim->m_Playback == PLAYBACK_LOOP_PINGPONG) {
                    cursor /= 2.0f;
                }

                anim->m_Elapsed = cursor * anim->m_Duration;
            }
        }
    }

    float GetNodeFlipbookPlaybackRate(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);

        if (n->m_Node.m_FlipbookAnimHash) {
            Animation* anim = GetComponentAnimation(scene, node, &n->m_Node.m_FlipbookAnimPosition);
            if (anim) {
                return anim->m_PlaybackRate;
            }
        }

        return 0.0f;
    }

    void SetNodeFlipbookPlaybackRate(HScene scene, HNode node, float playback_rate)
    {
        InternalNode* n = GetNode(scene, node);

        if (n->m_Node.m_FlipbookAnimHash) {
            Animation* anim = GetComponentAnimation(scene, node, &n->m_Node.m_FlipbookAnimPosition);
            if (anim) {
                anim->m_PlaybackRate = playback_rate;
            }
        }
    }

    Result PlayNodeParticlefx(HScene scene, HNode node, dmParticle::EmitterStateChangedData* callbackdata)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        dmhash_t particlefx_id = n->m_Node.m_ParticlefxHash;

        if (particlefx_id == 0)
        {
            dmLogError("Particle FX node does not have a particle fx set");
            return RESULT_RESOURCE_NOT_FOUND;
        }

        if (scene->m_AliveParticlefxs.Full())
        {
            dmLogError("Particle FX gui component buffer is full (%d), component disregarded. Increase 'gui.max_particlefx_count' as needed", scene->m_AliveParticlefxs.Capacity());
            return RESULT_OUT_OF_RESOURCES;
        }

        dmParticle::HPrototype particlefx_prototype = *(scene->m_Particlefxs.Get(particlefx_id));
        dmParticle::HInstance inst = dmParticle::CreateInstance(scene->m_ParticlefxContext, particlefx_prototype, callbackdata);

        if (n->m_Node.m_AdjustMode == ADJUST_MODE_STRETCH)
        {
            n->m_Node.m_AdjustMode = ADJUST_MODE_FIT;
            dmLogOnceWarning("Adjust mode \"Stretch\" is not supported by particlefx nodes, falling back to \"Fit\" instead (node '%s').", dmHashReverseSafe64(n->m_NameHash));
        }

        // Set initial transform
        Matrix4 trans;
        CalculateNodeTransform(scene, n, (CalculateNodeTransformFlags)(CALCULATE_NODE_INCLUDE_SIZE), trans);
        dmTransform::Transform transform = dmTransform::ToTransform(trans);
        float scale = transform.GetScalePtr()[0];
        dmParticle::SetPosition(scene->m_ParticlefxContext, inst, Point3(transform.GetTranslation()));
        dmParticle::SetRotation(scene->m_ParticlefxContext, inst, transform.GetRotation());
        dmParticle::SetScale(scene->m_ParticlefxContext, inst, scale);

        uint32_t count = scene->m_AliveParticlefxs.Size();
        scene->m_AliveParticlefxs.SetSize(count + 1);
        ParticlefxComponent* component = &scene->m_AliveParticlefxs[count];
        component->m_Prototype = particlefx_prototype;
        component->m_Instance = inst;
        component->m_Node = node;

        n->m_Node.m_ParticlefxPrototype = particlefx_prototype;
        n->m_Node.m_ParticleInstance = inst;

        dmParticle::StartInstance(scene->m_ParticlefxContext, inst);

        return RESULT_OK;
    }

    Result StopNodeParticlefx(HScene scene, HNode node, bool clear_particles)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX) {
            return RESULT_WRONG_TYPE;
        }

        uint32_t alive_count = scene->m_AliveParticlefxs.Size();
        for (uint32_t i = 0; i < alive_count; ++i)
        {
            ParticlefxComponent* component = &scene->m_AliveParticlefxs[i];
            if (component->m_Node == node)
            {
                dmParticle::StopInstance(scene->m_ParticlefxContext, component->m_Instance, clear_particles);
            }
        }

        return RESULT_OK;
    }

    void SetNodeClippingMode(HScene scene, HNode node, ClippingMode mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_ClippingMode = mode;
    }

    ClippingMode GetNodeClippingMode(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (ClippingMode) n->m_Node.m_ClippingMode;
    }

    void SetNodeClippingVisible(HScene scene, HNode node, bool visible)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_ClippingVisible = (uint32_t) visible;
    }

    bool GetNodeClippingVisible(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_ClippingVisible;
    }

    void SetNodeClippingInverted(HScene scene, HNode node, bool inverted)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_ClippingInverted = (uint32_t) inverted;
    }

    bool GetNodeClippingInverted(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_ClippingInverted;
    }

    Result GetTextMetrics(HScene scene, const char* text, const char* font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        return GetTextMetrics(scene, text, dmHashString64(font_id), width, line_break, leading, tracking, metrics);
    }

    Result GetTextMetrics(HScene scene, const char* text, dmhash_t font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        memset(metrics, 0, sizeof(*metrics));
        void** font = scene->m_Fonts.Get(font_id);
        if (!font) {
            return RESULT_RESOURCE_NOT_FOUND;
        }

        scene->m_Context->m_GetTextMetricsCallback(*font, text, width, line_break, leading, tracking, metrics);
        return RESULT_OK;
    }

    BlendMode GetNodeBlendMode(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (BlendMode)n->m_Node.m_BlendMode;
    }

    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_BlendMode = (uint32_t) blend_mode;
    }

    XAnchor GetNodeXAnchor(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (XAnchor)n->m_Node.m_XAnchor;
    }

    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_XAnchor = (uint32_t) x_anchor;
    }

    YAnchor GetNodeYAnchor(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (YAnchor)n->m_Node.m_YAnchor;
    }

    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_YAnchor = (uint32_t) y_anchor;
    }


    void SetNodeOuterBounds(HScene scene, HNode node, PieBounds bounds)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_OuterBounds = bounds;
    }

    void SetNodePerimeterVertices(HScene scene, HNode node, uint32_t vertices)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_PerimeterVertices = vertices;
    }

    void SetNodeInnerRadius(HScene scene, HNode node, float radius)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_PIE_PARAMS].setX(radius);
    }

    void SetNodePieFillAngle(HScene scene, HNode node, float fill_angle)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_PIE_PARAMS].setY(fill_angle);
    }

    PieBounds GetNodeOuterBounds(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_OuterBounds;
    }

    uint32_t GetNodePerimeterVertices(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_PerimeterVertices;
    }

    float GetNodeInnerRadius(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_PIE_PARAMS].getX();
    }

    float GetNodePieFillAngle(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[PROPERTY_PIE_PARAMS].getY();
    }

    Pivot GetNodePivot(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (Pivot)n->m_Node.m_Pivot;
    }

    void SetNodePivot(HScene scene, HNode node, Pivot pivot)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Pivot = (uint32_t) pivot;
    }

    bool GetNodeIsBone(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_IsBone;
    }

    void SetNodeIsBone(HScene scene, HNode node, bool is_bone)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_IsBone = is_bone;
    }

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_AdjustMode = (uint32_t) adjust_mode;
    }

    void SetNodeSizeMode(HScene scene, HNode node, SizeMode size_mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_SizeMode = (uint32_t) size_mode;
        if((n->m_Node.m_SizeMode != SIZE_MODE_MANUAL) && (n->m_Node.m_NodeType != NODE_TYPE_CUSTOM) && (n->m_Node.m_NodeType != NODE_TYPE_PARTICLEFX))
        {
            if (TextureInfo* texture_info = scene->m_Textures.Get(n->m_Node.m_TextureHash))
            {
                if(texture_info->m_TextureSource)
                {
                    n->m_Node.m_Properties[PROPERTY_SIZE][0] = texture_info->m_OriginalWidth;
                    n->m_Node.m_Properties[PROPERTY_SIZE][1] = texture_info->m_OriginalHeight;
                }
            }
            else if (DynamicTexture* texture = scene->m_DynamicTextures.Get(n->m_Node.m_TextureHash))
            {
                n->m_Node.m_Properties[PROPERTY_SIZE][0] = texture->m_Width;
                n->m_Node.m_Properties[PROPERTY_SIZE][1] = texture->m_Height;
            }
        }
    }

    SizeMode GetNodeSizeMode(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (SizeMode) n->m_Node.m_SizeMode;
    }

    static Animation* AnimateComponent(HScene scene,
                                 HNode node,
                                 float* value,
                                 float to,
                                 dmEasing::Curve easing,
                                 Playback playback,
                                 float duration,
                                 float delay,
                                 float playback_rate,
                                 AnimationComplete animation_complete,
                                 void* userdata1,
                                 void* userdata2)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);

        Animation animation;
        uint32_t animation_index = FindAnimation(scene->m_Animations, value);

        if (animation_index == 0xffffffff)
        {
            if (scene->m_Animations.Full())
            {
                dmLogWarning("Out of animation resources (%d)", scene->m_Animations.Size());
                return NULL;
            }
            animation_index = scene->m_Animations.Size();
            scene->m_Animations.SetSize(animation_index+1);
        }
        else
        {
            // Make sure to invoke the callback when we are re-using the
            // animation index so that we can clean up dangling ref's in
            // the gui_script module.
            Animation* anim = &scene->m_Animations[animation_index];
            if (anim->m_AnimationComplete && !anim->m_AnimationCompleteCalled)
            {
                anim->m_AnimationComplete(scene, anim->m_Node, false, anim->m_Userdata1, anim->m_Userdata2);
            }
        }

        animation.m_Node = node;
        animation.m_Value = value;
        animation.m_To = to;
        animation.m_Delay = delay < 0.0f ? 0.0f : delay;
        animation.m_Elapsed = 0.0f;
        animation.m_Duration = duration < 0.0f ? 0.0f : duration;
        animation.m_PlaybackRate = playback_rate;
        animation.m_Easing = easing;
        animation.m_Playback = playback;
        animation.m_AnimationComplete = animation_complete;
        animation.m_Userdata1 = userdata1;
        animation.m_Userdata2 = userdata2;
        animation.m_FirstUpdate = 1;
        animation.m_AnimationCompleteCalled = 0;
        animation.m_Cancelled = 0;
        animation.m_Backwards = 0;

        animation_index = InsertAnimation(scene->m_Animations, &animation);
        return &scene->m_Animations[animation_index];
    }

    void AnimateNodeHash(HScene scene,
                         HNode node,
                         dmhash_t property,
                         const Vector4& to,
                         dmEasing::Curve easing,
                         Playback playback,
                         float duration,
                         float delay,
                         AnimationComplete animation_complete,
                         void* userdata1,
                         void* userdata2)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);

        PropDesc* pd = GetPropertyDesc(property);
        if (pd)
        {
            float* base_value = (float*)&n->m_Node.m_Properties[pd->m_Property];

            if (pd->m_Component == 0xff)
            {
                dmEasing::Curve no_callback_easing = easing;
                no_callback_easing.release_callback = 0;
                for (int j = 0; j < 3; ++j)
                {
                    AnimateComponent(scene, node, &base_value[j], to.getElem(j), no_callback_easing, playback, duration, delay, 1.0f, 0, 0, 0);
                }

                // Only run callback for the lastcomponent
                AnimateComponent(scene, node, &base_value[3], to.getElem(3), easing, playback, duration, delay, 1.0f, animation_complete, userdata1, userdata2);
            }
            else
            {
                AnimateComponent(scene, node, &base_value[pd->m_Component], to.getElem(pd->m_Component), easing, playback, duration, delay, 1.0f, animation_complete, userdata1, userdata2);
            }
        }
        else
        {
            dmLogError("property '%s' not found", dmHashReverseSafe64(property));
        }
    }

    void AnimateNode(HScene scene,
                     HNode node,
                     Property property,
                     const Vector4& to,
                     dmEasing::Curve easing,
                     Playback playback,
                     float duration,
                     float delay,
                     AnimationComplete animation_complete,
                     void* userdata1,
                     void* userdata2)
    {
        dmhash_t prop_hash = g_PropTable[property].m_Hash;
        AnimateNodeHash(scene, node, prop_hash, to, easing, playback, duration, delay, animation_complete, userdata1, userdata2);
    }

    dmhash_t GetPropertyHash(Property property)
    {
        dmhash_t hash = 0;
        if (PROPERTY_SHADOW >= property) {
            hash = g_PropTable[property].m_Hash;
        }
        return hash;
    }

    void CancelAnimationHash(HScene scene, HNode node, dmhash_t property_hash)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);

        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n_animations = animations->Size();

        PropDesc* pd = GetPropertyDesc(property_hash);
        if (pd) {
            for (uint32_t i = 0; i < n_animations; ++i)
            {
                Animation* anim = &(*animations)[i];

                int from = 0;
                int to = 4; // NOTE: Exclusive range
                int expect = 4;
                if (pd->m_Component != 0xff) {
                    from = pd->m_Component;
                    to = pd->m_Component + 1;
                    expect = 1;
                }

                float* value = (float*) &n->m_Node.m_Properties[pd->m_Property];
                int count = 0;
                for (int j = from; j < to; ++j) {
                    if (anim->m_Node == node && anim->m_Value == (value + j))
                    {
                        anim->m_Cancelled = 1;
                        ++count;
                        if (count == expect) {
                            return;
                        }
                    }
                }
            }
        } else {
            dmLogError("property '%s' not found", dmHashReverseSafe64(property_hash));
        }
    }


    inline Animation* GetComponentAnimation(HScene scene, HNode node, float* value)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);

        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n_animations = animations->Size();
        for (uint32_t i = 0; i < n_animations; ++i)
        {
            Animation* anim = &(*animations)[i];
            if (anim->m_Node == node && anim->m_Value == value)
                return anim;
        }
        return 0;
    }

    static void CancelAnimationComponent(HScene scene, HNode node, float* value)
    {
        Animation* anim = GetComponentAnimation(scene, node, value);
        if(anim == 0x0)
            return;
        anim->m_Cancelled = 1;
    }

    static inline void AnimateTextureSetAnim(HScene scene, HNode node, float offset, float playback_rate, AnimationComplete anim_complete_callback, void* callback_userdata1, void* callback_userdata2)
    {
        InternalNode* n = GetNode(scene, node);
        TextureSetAnimDesc& anim_desc = n->m_Node.m_TextureSetAnimDesc;
        uint64_t anim_frames = (anim_desc.m_State.m_End - anim_desc.m_State.m_Start);
        dmGui::Playback playback = (dmGui::Playback)anim_desc.m_State.m_Playback;
        bool pingpong = playback == dmGui::PLAYBACK_ONCE_PINGPONG || playback == dmGui::PLAYBACK_LOOP_PINGPONG;

        // Ping pong for flipbook animations should result in double the
        // animation duration.
        if (pingpong)
            anim_frames = anim_frames * 2;

        // Convert offset into elapsed time, needed for GUI animation system.
        offset = dmMath::Clamp(offset, 0.0f, 1.0f);
        float elapsed = offset;
        float duration = (float) anim_frames / (float) anim_desc.m_State.m_FPS;
        if (pingpong) {
            elapsed /= 2.0f;
        }
        elapsed = elapsed * duration;

        Animation* anim = AnimateComponent(
                scene,
                node,
                &n->m_Node.m_FlipbookAnimPosition,
                1.0f,
                dmEasing::Curve(dmEasing::TYPE_LINEAR),
                playback,
                duration,
                0.0f,
                playback_rate,
                anim_complete_callback,
                callback_userdata1,
                callback_userdata2);

        if (!anim) {
            return;
        }

        // We force some of the animation properties here to simulate
        // elapsed time for flipbook animations that has offset.
        anim->m_From = 0.0f,
        anim->m_FirstUpdate = 0.0f;
        anim->m_Elapsed = elapsed;
        n->m_Node.m_FlipbookAnimPosition = offset;
    }

    static inline FetchTextureSetAnimResult FetchTextureSetAnim(HScene scene, InternalNode* n, dmhash_t anim)
    {
        FetchTextureSetAnimCallback fetch_anim_callback = scene->m_FetchTextureSetAnimCallback;
        if(fetch_anim_callback == 0x0)
        {
            dmLogError("PlayNodeFlipbookAnim called with node in scene with no FetchTextureSetAnimCallback set.");
            return FETCH_ANIMATION_CALLBACK_ERROR;
        }
        return fetch_anim_callback(n->m_Node.m_Texture, anim, &n->m_Node.m_TextureSetAnimDesc);
    }

    static inline void UpdateTextureSetAnimData(HScene scene, InternalNode* n)
    {
        // if we got a textureset (i.e. texture animation), we want to update the animation in case it is reloaded
        uint64_t anim_hash = n->m_Node.m_FlipbookAnimHash;
        if(n->m_Node.m_TextureType != NODE_TEXTURE_TYPE_TEXTURE_SET || anim_hash == 0)
            return;

        // update animationdata, compare state to current and early bail if equal
        TextureSetAnimDesc& anim_desc = n->m_Node.m_TextureSetAnimDesc;
        const TextureSetAnimDesc::State state_previous = anim_desc.m_State;
        if(FetchTextureSetAnim(scene, n, anim_hash)!=FETCH_ANIMATION_OK)
        {
            // general error in retreiving animation. This could be it being deleted or otherwise changed erraneously
            anim_desc.Init();
            CancelAnimationComponent(scene, GetNodeHandle(n), &n->m_Node.m_FlipbookAnimPosition);
            dmLogWarning("Failed to update animation '%s'.", dmHashReverseSafe64(anim_hash));
            return;
        }

        if (anim_desc.m_State.IsEqual(state_previous))
            return;

        n->m_Node.m_FlipbookAnimPosition = 0.0f;
        HNode node = GetNodeHandle(n);
        if(anim_desc.m_State.m_Playback == PLAYBACK_NONE)
        {
            CancelAnimationComponent(scene, node, &n->m_Node.m_FlipbookAnimPosition);
            return;
        }

        Animation* anim = GetComponentAnimation(scene, node, &n->m_Node.m_FlipbookAnimPosition);
        if(anim && (anim->m_Cancelled == 0))
            AnimateTextureSetAnim(scene, node, 0.0f, 1.0f, anim->m_AnimationComplete, anim->m_Userdata1, anim->m_Userdata2);
        else
            AnimateTextureSetAnim(scene, node, 0.0f, 1.0f, 0, 0, 0);
    }

    Result PlayNodeFlipbookAnim(HScene scene, HNode node, dmhash_t anim, float offset, float playback_rate, AnimationComplete anim_complete_callback, void* callback_userdata1, void* callback_userdata2)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_FlipbookAnimPosition = 0.0f;
        n->m_Node.m_FlipbookAnimHash = 0x0;

        if(anim == 0x0)
        {
            dmLogError("PlayNodeFlipbookAnim called with invalid anim name.");
            return RESULT_INVAL_ERROR;
        }
        if(n->m_Node.m_TextureType != NODE_TEXTURE_TYPE_TEXTURE_SET)
        {
            dmLogError("PlayNodeFlipbookAnim called with node not containing animation.");
            return RESULT_INVAL_ERROR;
        }

        n->m_Node.m_FlipbookAnimHash = anim;
        FetchTextureSetAnimResult result = FetchTextureSetAnim(scene, n, anim);
        if(result != FETCH_ANIMATION_OK)
        {
            CancelAnimationComponent(scene, node, &n->m_Node.m_FlipbookAnimPosition);
            n->m_Node.m_FlipbookAnimHash = 0;
            n->m_Node.m_TextureSetAnimDesc.Init();
            if(result == FETCH_ANIMATION_NOT_FOUND)
            {
                dmLogWarning("The animation '%s' could not be found.", dmHashReverseSafe64(anim));
            }
            else
            {
                dmLogWarning("Error playing animation '%s' (result %d).", dmHashReverseSafe64(anim), (int32_t) result);
            }
            return RESULT_RESOURCE_NOT_FOUND;
        }

        if(n->m_Node.m_TextureSetAnimDesc.m_State.m_Playback == PLAYBACK_NONE)
            CancelAnimationComponent(scene, node, &n->m_Node.m_FlipbookAnimPosition);
        else
            AnimateTextureSetAnim(scene, node, offset, playback_rate, anim_complete_callback, callback_userdata1, callback_userdata2);
        CalculateNodeSize(n);
        return RESULT_OK;
    }

    Result PlayNodeFlipbookAnim(HScene scene, HNode node, const char* anim, float offset, float playback_rate, AnimationComplete anim_complete_callback, void* callback_userdata1, void* callback_userdata2)
    {
        return PlayNodeFlipbookAnim(scene, node, dmHashString64(anim), offset, playback_rate, anim_complete_callback, callback_userdata1, callback_userdata2);
    }

    void CancelNodeFlipbookAnim(HScene scene, HNode node, bool keep_anim_hash)
    {
        InternalNode* n = GetNode(scene, node);
        CancelAnimationComponent(scene, node, &n->m_Node.m_FlipbookAnimPosition);
        if (keep_anim_hash)
            return;
        n->m_Node.m_FlipbookAnimHash = 0;
    }

    void CancelNodeFlipbookAnim(HScene scene, HNode node)
    {
        CancelNodeFlipbookAnim(scene, node, false);
    }

    void GetNodeFlipbookAnimUVFlip(HScene scene, HNode node, bool& flip_horizontal, bool& flip_vertical)
    {
        InternalNode* n = GetNode(scene, node);
        flip_horizontal = n->m_Node.m_TextureSetAnimDesc.m_FlipHorizontal;
        flip_vertical = n->m_Node.m_TextureSetAnimDesc.m_FlipVertical;
    }

    bool PickNode(HScene scene, HNode node, float x, float y)
    {
        Vector4 scale((float) scene->m_Context->m_PhysicalWidth / (float) scene->m_Context->m_DefaultProjectWidth,
                (float) scene->m_Context->m_PhysicalHeight / (float) scene->m_Context->m_DefaultProjectHeight, 1, 1);
        Matrix4 transform;
        InternalNode* n = GetNode(scene, node);
        CalculateNodeTransform(scene, n, CalculateNodeTransformFlags(CALCULATE_NODE_BOUNDARY | CALCULATE_NODE_INCLUDE_SIZE | CALCULATE_NODE_RESET_PIVOT), transform);
        // DEF-3066 set Z scale to 1.0 to get a sound inverse node transform for picking
        transform.setElem(2, 2, 1.0f);
        transform = inverse(transform);
        Vector4 screen_pos(x * scale.getX(), y * scale.getY(), 0.0f, 1.0f);
        Vector4 node_pos = transform * screen_pos;
        const float EPSILON = 0.0001f;
        // check if we need to project the local position to the node plane
        if (dmMath::Abs(node_pos.getZ()) > EPSILON)
        {
            Vector4 ray_dir = transform.getCol2();
            // falsify if node is almost orthogonal to the screen plane, impossible to pick
            if (dmMath::Abs(ray_dir.getZ()) < 0.0001f)
            {
                return false;
            }
            node_pos -= ray_dir * (node_pos.getZ() / ray_dir.getZ());
        }
        return node_pos.getX() >= 0.0f
                && node_pos.getX() <= 1.0f
                && node_pos.getY() >= 0.0f
                && node_pos.getY() <= 1.0f;
    }

    bool IsNodeEnabled(HScene scene, HNode node, bool recursive)
    {
        InternalNode* n = GetNode(scene, node);
        if (recursive)
        {
            return IsNodeEnabledRecursive(scene, n->m_Index);
        }
        return n->m_Node.m_Enabled;
    }

    static inline void SetDirtyLocalRecursive(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_DirtyLocal = 1;
        uint16_t index = n->m_ChildHead;
        while (index != INVALID_INDEX)
        {
            InternalNode* n = &scene->m_Nodes[index];
            n->m_Node.m_DirtyLocal = 1;
            if(n->m_ChildHead != INVALID_INDEX)
            {
                SetDirtyLocalRecursive(scene, GetNodeHandle(n));
            }
            index = n->m_NextIndex;
        }
    }

    void SetNodeEnabled(HScene scene, HNode node, bool enabled)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Enabled = enabled;
        if(enabled)
        {
            SetDirtyLocalRecursive(scene, node);
        }
    }

    bool GetNodeVisible(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_IsVisible;
    }

    void SetNodeVisible(HScene scene, HNode node, bool visible)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_IsVisible = visible;
    }

    void MoveNodeBelow(HScene scene, HNode node, HNode reference)
    {
        if (node != INVALID_HANDLE && node != reference)
        {
            InternalNode* n = GetNode(scene, node);
            RemoveFromNodeList(scene, n);
            InternalNode* parent = 0x0;
            InternalNode* prev = 0x0;
            if (reference != INVALID_HANDLE)
            {
                uint16_t ref_index = reference & 0xffff;
                InternalNode* ref = &scene->m_Nodes[ref_index];
                // The reference is actually the next node, find the previous
                if (ref->m_PrevIndex != INVALID_INDEX)
                {
                    prev = &scene->m_Nodes[ref->m_PrevIndex];
                }
                if (ref->m_ParentIndex != INVALID_INDEX)
                {
                    parent = &scene->m_Nodes[ref->m_ParentIndex];
                }
            }
            AddToNodeList(scene, n, parent, prev);
        }
    }

    void MoveNodeAbove(HScene scene, HNode node, HNode reference)
    {
        if (node != INVALID_HANDLE && node != reference)
        {
            InternalNode* n = GetNode(scene, node);
            RemoveFromNodeList(scene, n);
            InternalNode* parent = 0x0;
            InternalNode* prev = 0x0;
            if (reference != INVALID_HANDLE)
            {
                uint16_t ref_index = reference & 0xffff;
                prev = &scene->m_Nodes[ref_index];
                if (prev->m_ParentIndex != INVALID_INDEX)
                {
                    parent = &scene->m_Nodes[prev->m_ParentIndex];
                }
            }
            else
            {
                // Find the previous node of the root list
                uint16_t prev_index = scene->m_RenderTail;
                if (prev_index != INVALID_INDEX)
                {
                    prev = &scene->m_Nodes[prev_index];
                }
            }
            AddToNodeList(scene, n, parent, prev);
        }
    }

    HNode GetNodeParent(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        if (n->m_ParentIndex == INVALID_INDEX)
        {
            return dmGui::INVALID_HANDLE;
        }
        InternalNode* parent_n = &scene->m_Nodes[n->m_ParentIndex];
        return GetNodeHandle(parent_n);
    }

    static Vector3 ScreenToLocalPosition(HScene scene, InternalNode* node, InternalNode* parent_node, dmVMath::Vector3 screen_position)
    {
        Matrix4 parent_m;

        Vector4 reference_scale;
        Vector4 adjust_scale;
        Vector4 offset(0.0f);

        // Calculate the new parents scene transform
        // We also need to calculate values relative to adjustments and offset
        // corresponding to the values that would be used in AdjustPosScale (see reasoning below).
        if (parent_node != 0x0)
        {
            CalculateNodeTransform(scene, parent_node, CalculateNodeTransformFlags(), parent_m);
            reference_scale = parent_node->m_Node.m_LocalAdjustScale;
            adjust_scale = ApplyAdjustOnReferenceScale(reference_scale, node->m_Node.m_AdjustMode);
        } else {
            reference_scale = CalculateReferenceScale(scene, 0x0);
            adjust_scale = ApplyAdjustOnReferenceScale(reference_scale, node->m_Node.m_AdjustMode);
            parent_m = Matrix4::scale(adjust_scale.getXYZ());

            Vector4 parent_dims = Vector4((float) scene->m_Width, (float) scene->m_Height, 0.0f, 1.0f);
            Vector4 adjusted_dims = mulPerElem(parent_dims, adjust_scale);
            Vector4 ref_size = Vector4((float) scene->m_Context->m_PhysicalWidth, (float) scene->m_Context->m_PhysicalHeight, 0.0f, 1.0f);
            offset = (ref_size - adjusted_dims) * 0.5f;
        }

        // We calculate a new position that will be the relative position once
        // the node has been childed to the new parent.
        Vector3 position = screen_position - parent_m.getCol3().getXYZ();

        // We need to perform the inverse of what AdjustPosScale will do to counteract when
        // it will be applied during next call to CalculateNodeTransform.
        // See AdjustPosScale for comparison on the steps being performed/inversed.
        if (node->m_Node.m_XAnchor == XANCHOR_LEFT || node->m_Node.m_XAnchor == XANCHOR_RIGHT) {
            offset.setX(0.0f);
        }
        if (node->m_Node.m_YAnchor == YANCHOR_TOP || node->m_Node.m_YAnchor == YANCHOR_BOTTOM) {
            offset.setY(0.0f);
        }

        Vector3 scaled_position = position - offset.getXYZ();
        position = mulPerElem(recipPerElem(adjust_scale.getXYZ()), scaled_position);

        if (node->m_Node.m_XAnchor == dmGui::XANCHOR_LEFT || node->m_Node.m_XAnchor == dmGui::XANCHOR_RIGHT) {
            position.setX(scaled_position.getX() / reference_scale.getX());
        }

        if (node->m_Node.m_YAnchor == dmGui::YANCHOR_BOTTOM || node->m_Node.m_YAnchor == dmGui::YANCHOR_TOP) {
            position.setY(scaled_position.getY() / reference_scale.getY());
        }
        return position;
    }

    Point3 ScreenToLocalPosition(HScene scene, HNode node, const Point3& screen_position)
    {
        InternalNode* n = GetNode(scene, node);
        InternalNode* parent_node = 0x0;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            parent_node = &scene->m_Nodes[n->m_ParentIndex];
        }
        Vector3 local_position = ScreenToLocalPosition(scene, n, parent_node, Vector3(screen_position));
        return Point3(local_position);
    }

    static void SetScreenPosition(HScene scene, InternalNode* node, InternalNode* parent_node, dmVMath::Vector3 screen_position)
    {
        Vector3 local_position = ScreenToLocalPosition(scene, node, parent_node, screen_position);
        node->m_Node.m_Properties[dmGui::PROPERTY_POSITION] = Vector4(local_position, 1.0f);
        node->m_Node.m_DirtyLocal = 1;
    }

    void SetScreenPosition(HScene scene, HNode node, const Point3& screen_position)
    {
        InternalNode* n = GetNode(scene, node);
        InternalNode* parent_node = 0x0;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            parent_node = &scene->m_Nodes[n->m_ParentIndex];
        }
        SetScreenPosition(scene, n, parent_node, Vector3(screen_position));
    }

    Result SetNodeParent(HScene scene, HNode node, HNode parent, bool keep_scene_transform)
    {
        if (node == parent)
            return RESULT_INF_RECURSION;
        InternalNode* n = GetNode(scene, node);
        uint16_t parent_index = INVALID_INDEX;
        InternalNode* parent_node = 0x0;
        if (parent != INVALID_HANDLE)
        {
            parent_node = GetNode(scene, parent);
            // Check for infinite recursion
            uint16_t ancestor_index = parent_node->m_ParentIndex;
            while (ancestor_index != INVALID_INDEX)
            {
                if (n->m_Index == ancestor_index)
                    return RESULT_INF_RECURSION;
                ancestor_index = scene->m_Nodes[ancestor_index].m_ParentIndex;
            }
            parent_index = parent_node->m_Index;
        }
        if (parent_index != n->m_ParentIndex)
        {
            if (keep_scene_transform) {

                Matrix4 node_m;
                // Calculate the nodes current scene transform
                CalculateNodeTransform(scene, n, CalculateNodeTransformFlags(), node_m);
                Vector3 node_pos = node_m.getCol3().getXYZ();

                SetScreenPosition(scene, n, parent_node, node_pos);
            }

            RemoveFromNodeList(scene, n);
            InternalNode* prev = 0x0;
            uint16_t prev_index = scene->m_RenderTail;
            if (parent_index != INVALID_INDEX)
            {
                prev_index = parent_node->m_ChildTail;
            }
            if (prev_index != INVALID_INDEX)
            {
                prev = &scene->m_Nodes[prev_index];
            }
            AddToNodeList(scene, n, parent_node, prev);
        }
        return RESULT_OK;
    }

    Result CloneNode(HScene scene, HNode node, HNode* out_node)
    {
        uint16_t index = AllocateNode(scene);
        if (index == scene->m_NodePool.Capacity())
        {
            dmLogError("Could not create the node since the buffer is full (%d).", scene->m_NodePool.Capacity());
            return RESULT_OUT_OF_RESOURCES;
        }
        uint16_t version = scene->m_NextVersionNumber;
        if (version == 0)
        {
            // We can't use zero in order to avoid a handle == 0
            ++version;
        }
        *out_node = ((uint32_t) version) << 16 | index;
        InternalNode* out_n = &scene->m_Nodes[index];
        memset(out_n, 0, sizeof(InternalNode));

        InternalNode* n = GetNode(scene, node);
        out_n->m_Node = n->m_Node;
        if (n->m_Node.m_Text != 0x0)
            out_n->m_Node.m_Text = strdup(n->m_Node.m_Text);
        out_n->m_Version = version;
        out_n->m_Index = index;
        out_n->m_SceneTraversalCacheVersion = INVALID_INDEX;
        out_n->m_PrevIndex = INVALID_INDEX;
        out_n->m_NextIndex = INVALID_INDEX;
        out_n->m_ParentIndex = INVALID_INDEX;
        out_n->m_ChildHead = INVALID_INDEX;
        out_n->m_ChildTail = INVALID_INDEX;
        scene->m_NextVersionNumber = (version + 1) % ((1 << 16) - 1);

        if (n->m_Node.m_CustomType != 0)
        {
            void* src_custom_data = n->m_Node.m_CustomData;
            out_n->m_Node.m_CustomData = scene->m_CloneCustomNodeCallback(scene->m_CreateCustomNodeCallbackContext, scene, *out_node, n->m_Node.m_CustomType, src_custom_data);
            out_n->m_Node.m_CustomType = n->m_Node.m_CustomType;
        }

        if (n->m_Node.m_FlipbookAnimHash != 0)
        {
            float playback_rate = GetNodeFlipbookPlaybackRate(scene, node);
            float cursor = GetNodeFlipbookCursor(scene, node);
            PlayNodeFlipbookAnim(scene, *out_node, n->m_Node.m_FlipbookAnimHash, cursor, playback_rate, 0, 0, 0);
        }

        if (n->m_Node.m_ParticleInstance != 0x0)
        {
            out_n->m_Node.m_ParticleInstance = dmParticle::INVALID_INSTANCE;
            out_n->m_Node.m_ParticlefxHash = n->m_Node.m_ParticlefxHash;
        }
        // Add to the top of the scene
        MoveNodeAbove(scene, *out_node, INVALID_HANDLE);

        return RESULT_OK;
    }

    inline void CalculateParentNodeTransform(HScene scene, InternalNode* n, Matrix4& out_transform)
    {
        Matrix4 parent_trans;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            CalculateParentNodeTransform(scene, parent, parent_trans);
        }

        const Node& node = n->m_Node;
        if (node.m_DirtyLocal || (scene->m_ResChanged && scene->m_AdjustReference != ADJUST_REFERENCE_DISABLED))
        {
            UpdateLocalTransform(scene, n);
        }
        out_transform = node.m_LocalTransform;

        if (n->m_ParentIndex != INVALID_INDEX)
        {
            out_transform = parent_trans * out_transform;
        }
    }

    void CalculateNodeTransform(HScene scene, InternalNode* n, const CalculateNodeTransformFlags flags, Matrix4& out_transform)
    {
        Matrix4 parent_trans;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            CalculateParentNodeTransform(scene, parent, parent_trans);
        }

        const Node& node = n->m_Node;
        if (node.m_DirtyLocal || (scene->m_ResChanged && scene->m_AdjustReference != ADJUST_REFERENCE_DISABLED))
        {
            UpdateLocalTransform(scene, n);
        }
        out_transform = node.m_LocalTransform;
        CalculateNodeExtents(node, flags, out_transform);

        if (n->m_ParentIndex != INVALID_INDEX)
        {
            out_transform = parent_trans * out_transform;
        }
    }

    void* GetResource(dmGui::HScene scene, dmhash_t resource_id, dmhash_t suffix_with_dot)
    {
        // First, ask the comp_gui.cpp for the resource (it might be overridden!)
        if (scene->m_GetResourceCallback)
            return scene->m_GetResourceCallback(scene->m_GetResourceCallbackContext, scene, resource_id, suffix_with_dot);

        // Check the internal resources (if we ever need to)
        return 0;
    }

    static void ResetScript(HScript script) {
        memset(script, 0, sizeof(Script));
        for (int i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i) {
            script->m_FunctionReferences[i] = LUA_NOREF;
        }
        script->m_InstanceReference = LUA_NOREF;
    }

    HScript NewScript(HContext context)
    {
        lua_State* L = context->m_LuaState;
        Script* script = (Script*)lua_newuserdata(L, sizeof(Script));
        ResetScript(script);
        script->m_Context = context;
        script->m_SourceFileName = 0;

        luaL_getmetatable(L, GUI_SCRIPT);
        lua_setmetatable(L, -2);

        script->m_InstanceReference = dmScript::Ref(L, LUA_REGISTRYINDEX);

        return script;
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = script->m_Context->m_LuaState;
        for (int i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i) {
            if (script->m_FunctionReferences[i] != LUA_NOREF) {
                dmScript::Unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
            }
        }
        dmScript::Unref(L, LUA_REGISTRYINDEX, script->m_InstanceReference);
        free((void*)script->m_SourceFileName);
        script->~Script();
        ResetScript(script);
    }

    Result SetScript(HScript script, dmLuaDDF::LuaSource *source)
    {
        lua_State* L = script->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result res = RESULT_OK;

        int ret = dmScript::LuaLoad(L, source);
        if (ret != 0)
        {
            dmLogError("Error compiling script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            res = RESULT_SYNTAX_ERROR;
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_InstanceReference);
        dmScript::SetInstance(L);

        ret = dmScript::PCall(L, 0, 0);

        lua_pushnil(L);
        dmScript::SetInstance(L);

        if (ret != 0)
        {
            res = RESULT_SCRIPT_ERROR;
            goto bail;
        }

        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (script->m_FunctionReferences[i] != LUA_NOREF)
            {
                dmScript::Unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
                script->m_FunctionReferences[i] = LUA_NOREF;
            }

            lua_getglobal(L, SCRIPT_FUNCTION_NAMES[i]);
            if (lua_type(L, -1) == LUA_TFUNCTION)
            {
                script->m_FunctionReferences[i] = dmScript::Ref(L, LUA_REGISTRYINDEX);
            }
            else
            {
                if (lua_isnil(L, -1) == 0)
                    dmLogWarning("'%s' is not a function (%s)", SCRIPT_FUNCTION_NAMES[i], source->m_Filename);
                lua_pop(L, 1);
            }

            lua_pushnil(L);
            lua_setglobal(L, SCRIPT_FUNCTION_NAMES[i]);
        }
        script->m_SourceFileName = strdup(source->m_Filename);
bail:
        assert(top == lua_gettop(L));
        return res;
    }

    lua_State* GetLuaState(HContext context)
    {
        return context->m_LuaState;
    }

    int GetContextTableRef(HScene scene)
    {
        return scene->m_ContextTableReference;
    }

}  // namespace dmGui
