#include "gui.h"

#include <string.h>
#include <new>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/profile.h>

#include <script/script.h>

#include "gui_private.h"
#include "gui_script.h"

namespace dmGui
{
    const uint16_t INVALID_INDEX = 0xffff;

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

    Script::Script()
    : m_Context(0x0)
    {
        for (int i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
            m_FunctionReferences[i] = LUA_NOREF;
    }

    TextMetrics::TextMetrics()
    {
        memset(this, 0, sizeof(TextMetrics));
    }

    InputAction::InputAction()
    {
        memset(this, 0, sizeof(InputAction));
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
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        context->m_PhysicalWidth = params->m_PhysicalWidth;
        context->m_PhysicalHeight = params->m_PhysicalHeight;
        context->m_HidContext = params->m_HidContext;

        return context;
    }

    void DeleteContext(HContext context, dmScript::HContext script_context)
    {
        FinalizeScript(context->m_LuaState, script_context);
        delete context;
    }

    void SetResolution(HContext context, uint32_t width, uint32_t height)
    {
        context->m_Width = width;
        context->m_Height = height;
    }

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height)
    {
        context->m_PhysicalWidth = width;
        context->m_PhysicalHeight = height;
    }

    void SetDefaultNewSceneParams(NewSceneParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_MaxNodes = 128;
        params->m_MaxAnimations = 128;
        params->m_MaxTextures = 32;
        params->m_MaxFonts = 4;
    }

    Scene::Scene()
    {
        memset(this, 0, sizeof(Scene));
    }

    HScene NewScene(HContext context, const NewSceneParams* params)
    {
        lua_State* L = context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = new (lua_newuserdata(L, sizeof(Scene))) Scene();

        lua_pushvalue(L, -1);
        scene->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        scene->m_DataReference = luaL_ref(L, LUA_REGISTRYINDEX);

        scene->m_Context = context;
        scene->m_Script = 0x0;
        scene->m_Nodes.SetCapacity(params->m_MaxNodes);
        scene->m_Nodes.SetSize(params->m_MaxNodes);
        scene->m_NodePool.SetCapacity(params->m_MaxNodes);
        scene->m_Animations.SetCapacity(params->m_MaxAnimations);
        scene->m_Textures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_DynamicTextures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_Fonts.SetCapacity(params->m_MaxFonts*2, params->m_MaxFonts);
        scene->m_DefaultFont = 0;
        scene->m_UserData = params->m_UserData;
        scene->m_RenderHead = INVALID_INDEX;
        scene->m_RenderTail = INVALID_INDEX;
        scene->m_NextVersionNumber = 0;

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            memset(n, 0, sizeof(*n));
            n->m_Index = INVALID_INDEX;
        }

        luaL_getmetatable(L, GUI_SCRIPT_INSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return scene;
    }

    void DeleteScene(HScene scene)
    {
        lua_State*L = scene->m_Context->m_LuaState;

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            if (n->m_Node.m_Text)
                free((void*) n->m_Node.m_Text);
        }

        luaL_unref(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, scene->m_DataReference);

        scene->~Scene();

        memset(scene, 0, sizeof(Scene));
    }

    void SetSceneUserData(HScene scene, void* user_data)
    {
        scene->m_UserData = user_data;
    }

    void* GetSceneUserData(HScene scene)
    {
        return scene->m_UserData;
    }

    Result AddTexture(HScene scene, const char* texture_name, void* texture)
    {
        if (scene->m_Textures.Full())
            return RESULT_OUT_OF_RESOURCES;

        uint64_t texture_hash = dmHashString64(texture_name);
        scene->m_Textures.Put(texture_hash, texture);
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            if (scene->m_Nodes[i].m_Node.m_TextureHash == texture_hash)
                scene->m_Nodes[i].m_Node.m_Texture = texture;
        }
        return RESULT_OK;
    }

    void RemoveTexture(HScene scene, const char* texture_name)
    {
        uint64_t texture_hash = dmHashString64(texture_name);
        scene->m_Textures.Erase(texture_hash);
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            if (scene->m_Nodes[i].m_Node.m_TextureHash == texture_hash)
                scene->m_Nodes[i].m_Node.m_Texture = 0;
        }
    }

    void ClearTextures(HScene scene)
    {
        scene->m_Textures.Clear();
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            scene->m_Nodes[i].m_Node.m_Texture = 0;
        }
    }

    Result NewDynamicTexture(HScene scene, const char* texture_name, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, uint32_t buffer_size)
    {
        dmhash_t texture_hash = dmHashString64(texture_name);
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
        memcpy(t.m_Buffer, buffer, buffer_size);
        t.m_Width = width;
        t.m_Height = height;
        t.m_Type = type;

        scene->m_DynamicTextures.Put(texture_hash, t);

        return RESULT_OK;
    }

    Result DeleteDynamicTexture(HScene scene, const char* texture_name)
    {
        dmhash_t texture_hash = dmHashString64(texture_name);
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

    Result SetDynamicTextureData(HScene scene, const char* texture_name, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, uint32_t buffer_size)
    {
        dmhash_t texture_hash = dmHashString64(texture_name);
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
        memcpy(t->m_Buffer, buffer, buffer_size);
        t->m_Width = width;
        t->m_Height = height;
        t->m_Type = type;

        return RESULT_OK;
    }

    Result AddFont(HScene scene, const char* font_name, void* font)
    {
        if (scene->m_Fonts.Full())
            return RESULT_OUT_OF_RESOURCES;

        if (!scene->m_DefaultFont)
            scene->m_DefaultFont = font;

        uint64_t font_hash = dmHashString64(font_name);
        scene->m_Fonts.Put(font_hash, font);
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            if (scene->m_Nodes[i].m_Node.m_FontHash == font_hash)
                scene->m_Nodes[i].m_Node.m_Font = font;
        }
        return RESULT_OK;
    }

    void RemoveFont(HScene scene, const char* font_name)
    {
        uint64_t font_hash = dmHashString64(font_name);
        scene->m_Fonts.Erase(font_hash);
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            if (scene->m_Nodes[i].m_Node.m_FontHash == font_hash)
                scene->m_Nodes[i].m_Node.m_Font = 0;
        }
    }

    void ClearFonts(HScene scene)
    {
        scene->m_Fonts.Clear();
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            scene->m_Nodes[i].m_Node.m_Font = 0;
        }
    }

    HNode GetNodeHandle(InternalNode* node)
    {
        return ((uint32_t) node->m_Version) << 16 | node->m_Index;
    }

    Vector4 CalculateReferenceScale(HContext context)
    {
        float scale_x = (float) context->m_PhysicalWidth / (float) context->m_Width;
        float scale_y = (float) context->m_PhysicalHeight / (float) context->m_Height;
        return Vector4(scale_x, scale_y, 1, 1);
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
            params->m_Params->m_DeleteTexture(scene, texture->m_Handle, context);
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
            dmArray<InternalNode>& nodes = scene->m_Nodes;
            uint32_t n = nodes.Size();
            for (uint32_t j = 0; j < n; ++j) {
                Node& node = nodes[j].m_Node;
                if (DynamicTexture* texture = scene->m_DynamicTextures.Get(node.m_TextureHash)) {
                    node.m_Texture = texture->m_Handle;
                }
            }
        }
    }

    static void DeferredDeleteDynamicTextures(HScene scene, const RenderSceneParams& params, void* context)
    {
        for (uint32_t i = 0; i < scene->m_DeletedDynamicTextures.Size(); ++i) {
            dmhash_t texture_hash = scene->m_DeletedDynamicTextures[i];
            scene->m_DynamicTextures.Erase(texture_hash);

            dmArray<InternalNode>& nodes = scene->m_Nodes;
            uint32_t n = nodes.Size();
            for (uint32_t j = 0; j < n; ++j) {
                Node& node = nodes[j].m_Node;
                if (node.m_TextureHash == texture_hash) {
                    node.m_Texture = 0;
                    // Do not break here. Texture may be used multiple times.
                }
            }
        }
    }

    void RenderScene(HScene scene, const RenderSceneParams& params, void* context)
    {
        Context* c = scene->m_Context;

        UpdateDynamicTextures(scene, params, context);
        DeferredDeleteDynamicTextures(scene, params, context);

        Vector4 scale = CalculateReferenceScale(c);
        c->m_RenderNodes.SetSize(0);
        c->m_RenderTransforms.SetSize(0);
        uint32_t capacity = scene->m_NodePool.Size();
        if (capacity > c->m_RenderNodes.Capacity())
        {
            c->m_RenderNodes.SetCapacity(capacity);
            c->m_RenderTransforms.SetCapacity(capacity);
        }
        Matrix4 node_transform;
        uint32_t index = scene->m_RenderHead;
        while (index != INVALID_INDEX)
        {
            InternalNode* n = &scene->m_Nodes[index];
            if (n->m_Node.m_Enabled)
            {
                CalculateNodeTransform(scene, n->m_Node, scale, false, &node_transform);
                HNode node = GetNodeHandle(n);
                c->m_RenderNodes.Push(node);
                c->m_RenderTransforms.Push(node_transform);
            }
            index = n->m_NextIndex;
        }
        params.m_RenderNodes(scene, c->m_RenderNodes.Begin(), c->m_RenderTransforms.Begin(), c->m_RenderNodes.Size(), context);
    }

    void RenderScene(HScene scene, RenderNodes render_nodes, void* context)
    {
        RenderSceneParams p;
        p.m_RenderNodes = render_nodes;
        RenderScene(scene, p, context);
    }

    void UpdateAnimations(HScene scene, float dt)
    {
        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n = animations->Size();

        uint32_t active_animations = 0;

        for (uint32_t i = 0; i < n; ++i)
        {
            Animation* anim = &(*animations)[i];

            if ((!anim->m_Enabled) || anim->m_Elapsed >= anim->m_Duration || anim->m_Cancelled)
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
                }

                // NOTE: We add dt to elapsed before we calculate t.
                // Example: 60 updates with dt=1/60.0 should result in a complete animation
                anim->m_Elapsed += dt;
                // Clamp elapsed to duration if we are closer than half a time step
                anim->m_Elapsed = dmMath::Select(anim->m_Elapsed + dt * 0.5f - anim->m_Duration, anim->m_Duration, anim->m_Elapsed);
                // Calculate normalized time if elapsed has not yet reached duration, otherwise it's set to 1 (animation complete)
                float t = dmMath::Select(anim->m_Duration - anim->m_Elapsed, anim->m_Elapsed / anim->m_Duration, 1.0f);
                float t2 = t;
                if (anim->m_Playback == PLAYBACK_ONCE_BACKWARD || anim->m_Playback == PLAYBACK_LOOP_BACKWARD || anim->m_Backwards) {
                    t2 = 1.0f - t;
                }

                float x = dmEasing::GetValue(anim->m_Easing, t2);

                *anim->m_Value = anim->m_From * (1-x) + anim->m_To * x;

                // Animation complete, see above
                if (t >= 1.0f)
                {
                    bool looping = anim->m_Playback == PLAYBACK_LOOP_FORWARD || anim->m_Playback == PLAYBACK_LOOP_BACKWARD || anim->m_Playback == PLAYBACK_LOOP_PINGPONG;
                    if (looping) {
                        anim->m_Elapsed = anim->m_Elapsed - anim->m_Duration;
                        if (anim->m_Playback == PLAYBACK_LOOP_PINGPONG) {
                            anim->m_Backwards ^= 1;
                        }
                    } else {
                        if (!anim->m_AnimationCompleteCalled && anim->m_AnimationComplete)
                        {
                            // NOTE: Very important to set m_AnimationCompleteCalled to 1
                            // before invoking the call-back. The call-back could potentially
                            // start a new animation that could reuse the same animation slot.
                            anim->m_AnimationCompleteCalled = 1;
                            anim->m_AnimationComplete(scene, anim->m_Node, anim->m_Userdata1, anim->m_Userdata2);
                        }
                    }
                }
            }
            else
            {
                anim->m_Delay -= dt;
            }
        }

        n = animations->Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Animation* anim = &(*animations)[i];

            if (anim->m_Elapsed >= anim->m_Duration || anim->m_Cancelled)
            {
                animations->EraseSwap(i);
                i--;
                n--;
                continue;
            }
        }

        DM_COUNTER("Gui.Animations", n);
        DM_COUNTER("Gui.ActiveAnimations", active_animations);
    }

    struct InputArgs
    {
        const InputAction* m_Action;
        bool m_Consumed;
    };

    Result RunScript(HScene scene, ScriptFunction script_function, int custom_ref, void* args)
    {
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

            lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
            assert(lua_isfunction(L, -1));
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);

            uint32_t arg_count = 1;
            uint32_t ret_count = 0;

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
                        dmScript::PushDDF(L, (dmDDF::Descriptor*)message->m_Descriptor, (const char*) message->m_Data);
                    }
                    else if (message->m_DataSize > 0)
                    {
                        dmScript::PushTable(L, (const char*) message->m_Data);
                    }
                    else
                    {
                        lua_newtable(L);
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

                            lua_pushliteral(L, "dx");
                            lua_pushinteger(L, (lua_Integer) t.m_DX);
                            lua_settable(L, -3);

                            lua_pushliteral(L, "dy");
                            lua_pushinteger(L, (lua_Integer) t.m_DY);
                            lua_settable(L, -3);

                            lua_settable(L, -3);
                        }
                        lua_settable(L, -3);
                    }

                    if (ia->m_TextCount > 0)
                    {
                        lua_pushliteral(L, "text");
                        lua_pushlstring(L, ia->m_Text, ia->m_TextCount);
                        lua_settable(L, -3);
                    }

                    arg_count += 2;
                }
                break;
            default:
                break;
            }

            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);

            Result result = RESULT_OK;
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                assert(top == lua_gettop(L));
                result = RESULT_SCRIPT_ERROR;
            }
            else
            {
                switch (script_function)
                {
                case SCRIPT_FUNCTION_ONINPUT:
                    {
                        InputArgs* input_args = (InputArgs*)args;
                        int ret_count = lua_gettop(L) - top;
                        if (ret_count == 1 && lua_isboolean(L, -1))
                        {
                            input_args->m_Consumed = lua_toboolean(L, -1);
                        }
                        else if (ret_count != 0)
                        {
                            dmLogError("The function %s must either return true/false, or no value at all.", SCRIPT_FUNCTION_NAMES[script_function]);
                            result = RESULT_SCRIPT_ERROR;
                        }
                    }
                    break;
                default:
                    if (lua_gettop(L) - top != (int32_t)ret_count)
                    {
                        dmLogError("The function %s must have exactly %d return values.", SCRIPT_FUNCTION_NAMES[script_function], ret_count);
                        result = RESULT_SCRIPT_ERROR;
                    }
                    break;
                }
            }
            lua_pushnil(L);
            dmScript::SetInstance(L);
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
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &scene->m_Nodes[i];
            if (node->m_Deleted)
            {
                HNode hnode = GetNodeHandle(node);
                DeleteNode(scene, hnode);
                node->m_Deleted = 0; // Make sure to clear deferred delete flag
            }
        }

        return result;
    }

    Result UpdateScene(HScene scene, float dt)
    {
        Result result = RunScript(scene, SCRIPT_FUNCTION_UPDATE, LUA_NOREF, (void*)&dt);

        UpdateAnimations(scene, dt);

        uint32_t total_nodes = 0;
        uint32_t active_nodes = 0;
        // Deferred deletion of nodes
        uint32_t n = scene->m_Nodes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &scene->m_Nodes[i];
            if (node->m_Deleted)
            {
                uint16_t index = node->m_Index;
                uint16_t version = node->m_Version;
                HNode hnode = ((uint32_t) version) << 16 | index;
                DeleteNode(scene, hnode);
                node->m_Deleted = 0; // Make sure to clear deferred delete flag
            }
            else if (node->m_Index != INVALID_INDEX)
            {
                ++total_nodes;
                if (node->m_Node.m_Enabled)
                    ++active_nodes;
            }
        }

        DM_COUNTER("Gui.Nodes", total_nodes);
        DM_COUNTER("Gui.ActiveNodes", active_nodes);
        DM_COUNTER("Gui.StaticTextures", scene->m_Textures.Size());
        DM_COUNTER("Gui.DynamicTextures", scene->m_DynamicTextures.Size());
        DM_COUNTER("Gui.Textures", scene->m_Textures.Size() + scene->m_DynamicTextures.Size());

        return result;
    }

    Result DispatchMessage(HScene scene, dmMessage::Message* message)
    {
        int custom_ref = LUA_NOREF;
        bool is_callback = false;
        if (message->m_Receiver.m_Function) {
            // NOTE: By convention m_Function is the ref + 2, see message.h in dlib
            custom_ref = message->m_Receiver.m_Function - 2;
            is_callback = true;
        }

        Result r = RunScript(scene, SCRIPT_FUNCTION_ONMESSAGE, custom_ref, (void*)message);

        if (is_callback) {
            lua_State* L = scene->m_Context->m_LuaState;
            luaL_unref(L, LUA_REGISTRYINDEX, custom_ref);
        }
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

    HNode NewNode(HScene scene, const Point3& position, const Vector3& size, NodeType node_type)
    {
        if (scene->m_NodePool.Remaining() == 0)
        {
            dmLogError("Could not create the node since the buffer is full (%d).", scene->m_NodePool.Capacity());
            return 0;
        }
        else
        {
            uint16_t index = scene->m_NodePool.Pop();
            uint16_t version = scene->m_NextVersionNumber;
            if (version == 0)
            {
                // We can't use zero in order to avoid a handle == 0
                ++version;
            }
            HNode hnode = ((uint32_t) version) << 16 | index;
            InternalNode* node = &scene->m_Nodes[index];
            node->m_Node.m_Properties[PROPERTY_POSITION] = Vector4(Vector3(position), 1);
            node->m_Node.m_Properties[PROPERTY_ROTATION] = Vector4(0);
            node->m_Node.m_Properties[PROPERTY_SCALE] = Vector4(1,1,1,0);
            node->m_Node.m_Properties[PROPERTY_COLOR] = Vector4(1,1,1,1);
            node->m_Node.m_Properties[PROPERTY_OUTLINE] = Vector4(0,0,0,1);
            node->m_Node.m_Properties[PROPERTY_SHADOW] = Vector4(0,0,0,1);
            node->m_Node.m_Properties[PROPERTY_SIZE] = Vector4(size, 0);
            node->m_Node.m_BlendMode = 0;
            node->m_Node.m_NodeType = (uint32_t) node_type;
            node->m_Node.m_XAnchor = 0;
            node->m_Node.m_YAnchor = 0;
            node->m_Node.m_Pivot = 0;
            node->m_Node.m_AdjustMode = 0;
            node->m_Node.m_LineBreak = 0;
            node->m_Node.m_Enabled = 1;

            node->m_Node.m_HasResetPoint = false;
            node->m_Node.m_TextureHash = 0;
            node->m_Node.m_Texture = 0;
            node->m_Node.m_FontHash = 0;
            node->m_Node.m_Font = 0;
            node->m_Version = version;
            node->m_Index = index;
            node->m_PrevIndex = INVALID_INDEX;
            node->m_NextIndex = INVALID_INDEX;
            scene->m_NextVersionNumber = (version + 1) % ((1 << 16) - 1);
            MoveNodeAbove(scene, hnode, INVALID_HANDLE);

            return hnode;
        }
    }

    void SetNodeId(HScene scene, HNode node, const char* id)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_NameHash = dmHashString64(id);
    }

    HNode GetNodeById(HScene scene, const char* id)
    {
        dmhash_t name_hash = dmHashString64(id);
        return GetNodeById(scene, name_hash);
    }

    HNode GetNodeById(HScene scene, dmhash_t id)
    {
        uint32_t n = scene->m_Nodes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &scene->m_Nodes[i];
            if (node->m_NameHash == id)
            {
                return ((uint32_t) node->m_Version) << 16 | node->m_Index;
            }
        }
        return 0;
    }

    uint32_t GetNodeCount(HScene scene)
    {
        return scene->m_NodePool.Size();
    }

    static void RemoveFromRenderList(HScene scene, InternalNode* n)
    {
        // Remove from list
        if (n->m_PrevIndex != INVALID_INDEX)
            scene->m_Nodes[n->m_PrevIndex].m_NextIndex = n->m_NextIndex;
        if (n->m_NextIndex != INVALID_INDEX)
            scene->m_Nodes[n->m_NextIndex].m_PrevIndex = n->m_PrevIndex;
        if (scene->m_RenderHead == n->m_Index)
            scene->m_RenderHead = n->m_NextIndex;
        if (scene->m_RenderTail == n->m_Index)
            scene->m_RenderTail = n->m_PrevIndex;
    }

    void DeleteNode(HScene scene, HNode node)
    {
        InternalNode*n = GetNode(scene, node);

        dmArray<Animation> *animations = &scene->m_Animations;
        uint32_t n_anims = animations->Size();
        for (uint32_t i = 0; i < n_anims; ++i)
        {
            Animation* anim = &(*animations)[i];

            if (anim->m_Node == node)
            {
                animations->EraseSwap(i);
                i--;
                n_anims--;
                continue;
            }
        }
        RemoveFromRenderList(scene, n);
        scene->m_NodePool.Push(n->m_Index);
        if (n->m_Node.m_Text)
            free((void*)n->m_Node.m_Text);
        memset(n, 0, sizeof(InternalNode));
        n->m_Index = INVALID_INDEX;
    }

    void ClearNodes(HScene scene)
    {
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            memset(n, 0, sizeof(*n));
            n->m_Index = INVALID_INDEX;
        }
        scene->m_RenderHead = INVALID_INDEX;
        scene->m_RenderTail = INVALID_INDEX;
        scene->m_NodePool.Clear();
        scene->m_Animations.SetSize(0);
    }

    void ResetNodes(HScene scene)
    {
        uint32_t n_nodes = scene->m_Nodes.Size();
        for (uint32_t i = 0; i < n_nodes; ++i) {
            Node* n = &scene->m_Nodes[i].m_Node;
            if (n->m_HasResetPoint) {
                memcpy(n->m_Properties, n->m_ResetPointProperties, sizeof(n->m_Properties));
                n->m_State = n->m_ResetPointState;
            }
        }
        scene->m_Animations.SetSize(0);
    }

    NodeType GetNodeType(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return (NodeType)n->m_Node.m_NodeType;
    }

    Point3 GetNodePosition(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return Point3(n->m_Node.m_Properties[PROPERTY_POSITION].getXYZ());
    }

    void SetNodePosition(HScene scene, HNode node, const Point3& position)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[PROPERTY_POSITION] = Vector4(position);
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
        dmLogError("Property %s not found", (const char*) dmHashReverse64(property, 0));
        return Vector4(0, 0, 0, 0);
    }

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value)
    {
        assert(property < PROPERTY_COUNT);
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[property] = value;
    }

    void SetNodeResetPoint(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        memcpy(n->m_Node.m_ResetPointProperties, n->m_Node.m_Properties, sizeof(n->m_Node.m_Properties));
        n->m_Node.m_ResetPointState = n->m_Node.m_State;
        n->m_Node.m_HasResetPoint = true;
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

    void* GetNodeTexture(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Texture;
    }

    dmhash_t GetNodeTextureId(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_TextureHash;
    }

    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id)
    {
        if (void** texture = scene->m_Textures.Get(texture_id)) {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_TextureHash = texture_id;
            n->m_Node.m_Texture = *texture;
            return RESULT_OK;
        } else if (DynamicTexture* texture = scene->m_DynamicTextures.Get(texture_id)) {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_TextureHash = texture_id;
            n->m_Node.m_Texture = texture->m_Handle;
            return RESULT_OK;
        } else {
            return RESULT_RESOURCE_NOT_FOUND;
        }
    }

    Result SetNodeTexture(HScene scene, HNode node, const char* texture_id)
    {
        return SetNodeTexture(scene, node, dmHashString64(texture_id));
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

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_AdjustMode = (uint32_t) adjust_mode;
    }

    static void AnimateComponent(HScene scene,
                                 HNode node,
                                 float* value,
                                 float to,
                                 dmEasing::Type easing,
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

        Animation animation;
        uint32_t animation_index = 0xffffffff;

        // Remove old animation for the same property
        for (uint32_t i = 0; i < scene->m_Animations.Size(); ++i)
        {
            const Animation* anim = &scene->m_Animations[i];
            if (value == anim->m_Value)
            {
                //scene->m_Animations.EraseSwap(i);
                animation_index = i;
                break;
            }
        }

        if (animation_index == 0xffffffff)
        {
            if (scene->m_Animations.Full())
            {
                dmLogWarning("Out of animation resources (%d)", scene->m_Animations.Size());
                return;
            }
            animation_index = scene->m_Animations.Size();
            scene->m_Animations.SetSize(animation_index+1);
        }

        animation.m_Node = node;
        animation.m_Value = value;
        animation.m_To = to;
        animation.m_Delay = delay;
        animation.m_Elapsed = 0.0f;
        animation.m_Duration = duration;
        animation.m_Easing = easing;
        animation.m_Playback = playback;
        animation.m_AnimationComplete = animation_complete;
        animation.m_Userdata1 = userdata1;
        animation.m_Userdata2 = userdata2;
        animation.m_FirstUpdate = 1;
        animation.m_AnimationCompleteCalled = 0;
        animation.m_Cancelled = 0;
        animation.m_Enabled = n->m_Node.m_Enabled;
        animation.m_Backwards = 0;

        scene->m_Animations[animation_index] = animation;
    }

    void AnimateNodeHash(HScene scene,
                         HNode node,
                         dmhash_t property,
                         const Vector4& to,
                         dmEasing::Type easing,
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
        if (pd) {
            Vector4* base_value = &n->m_Node.m_Properties[pd->m_Property];

            if (pd->m_Component == 0xff) {
                for (int j = 0; j < 4; ++j) {
                    AnimateComponent(scene, node, ((float*) base_value) + j, to.getElem(j), easing, playback, duration, delay, animation_complete, userdata1, userdata2);
                    // Only run callback for the first component
                    animation_complete = 0;
                    userdata1 = 0;
                    userdata2 = 0;
                }
            } else {
                AnimateComponent(scene, node, ((float*) base_value) + pd->m_Component, to.getElem(pd->m_Component), easing, playback, duration, delay, animation_complete, userdata1, userdata2);
            }
        } else {
            dmLogError("property '%s' not found", (const char*) dmHashReverse64(property, 0));
        }
    }

    void AnimateNode(HScene scene,
                     HNode node,
                     Property property,
                     const Vector4& to,
                     dmEasing::Type easing,
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

    void CancelAnimation(HScene scene, HNode node, Property property)
    {
        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n_animations = animations->Size();

        InternalNode* n = GetNode(scene, node);

        for (uint32_t i = 0; i < n_animations; ++i)
        {
            Animation* anim = &(*animations)[i];
            float* value = (float*) &n->m_Node.m_Properties[property];
            for (int j = 0; j < 4; ++j) {
                if (anim->m_Node == node && anim->m_Value == (value + j))
                {
                    anim->m_Cancelled = 1;
                    return;
                }
            }
        }
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
                if (pd->m_Component != 0xff) {
                    from = pd->m_Component;
                    to = pd->m_Component + 1;
                }

                float* value = (float*) &n->m_Node.m_Properties[pd->m_Property];
                for (int j = from; j < to; ++j) {
                    if (anim->m_Node == node && anim->m_Value == (value + j))
                    {
                        anim->m_Cancelled = 1;
                        return;
                    }
                }
            }
        } else {
            dmLogError("property '%s' not found", (const char*) dmHashReverse64(property_hash, 0));
        }
    }

    bool PickNode(HScene scene, HNode node, float x, float y)
    {
        Vector4 scale = CalculateReferenceScale(scene->m_Context);
        Matrix4 transform;
        const Node& n = GetNode(scene, node)->m_Node;
        CalculateNodeTransform(scene, n, scale, true, &transform);
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

    bool IsNodeEnabled(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Enabled;
    }

    void SetNodeEnabled(HScene scene, HNode node, bool enabled)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Enabled = enabled;
        dmArray<Animation>& anims = scene->m_Animations;
        uint32_t anim_count = anims.Size();
        for (uint32_t i = 0; i < anim_count; ++i)
        {
            Animation& anim = anims[i];
            if (anim.m_Node == node)
            {
                anim.m_Enabled = enabled;
            }
        }
    }

    void MoveNodeBelow(HScene scene, HNode node, HNode reference)
    {
        if (node != INVALID_HANDLE && node != reference)
        {
            InternalNode* n = GetNode(scene, node);
            RemoveFromRenderList(scene, n);
            uint16_t ref_index = reference & 0xffff;
            if (reference == INVALID_HANDLE || scene->m_RenderHead == ref_index)
            {
                // Insert at head
                ref_index = scene->m_RenderHead;
                if (ref_index != INVALID_INDEX)
                    scene->m_Nodes[ref_index].m_PrevIndex = n->m_Index;
                n->m_NextIndex = ref_index;
                n->m_PrevIndex = INVALID_INDEX;
                scene->m_RenderHead = n->m_Index;
                if (scene->m_RenderTail == INVALID_INDEX)
                    scene->m_RenderTail = n->m_Index;
            }
            else
            {
                // Insert in middle
                InternalNode* ref = &scene->m_Nodes[ref_index];
                scene->m_Nodes[ref->m_PrevIndex].m_NextIndex = n->m_Index;
                n->m_PrevIndex = ref->m_PrevIndex;
                ref->m_PrevIndex = n->m_Index;
                n->m_NextIndex = ref_index;
            }
        }
    }

    void MoveNodeAbove(HScene scene, HNode node, HNode reference)
    {
        if (node != INVALID_HANDLE && node != reference)
        {
            InternalNode* n = GetNode(scene, node);
            RemoveFromRenderList(scene, n);
            uint16_t ref_index = reference & 0xffff;
            if (reference == INVALID_HANDLE || scene->m_RenderTail == ref_index)
            {
                // Insert at tail
                ref_index = scene->m_RenderTail;
                if (ref_index != INVALID_INDEX)
                    scene->m_Nodes[ref_index].m_NextIndex = n->m_Index;
                n->m_PrevIndex = ref_index;
                n->m_NextIndex = INVALID_INDEX;
                scene->m_RenderTail = n->m_Index;
                if (scene->m_RenderHead == INVALID_INDEX)
                    scene->m_RenderHead = n->m_Index;
            }
            else
            {
                // Insert in middle
                InternalNode* ref = &scene->m_Nodes[ref_index];
                scene->m_Nodes[ref->m_NextIndex].m_PrevIndex = n->m_Index;
                n->m_NextIndex = ref->m_NextIndex;
                ref->m_NextIndex = n->m_Index;
                n->m_PrevIndex = ref_index;
            }
        }
    }

    static Quat EulerToQuat(Vector3 euler_radians)
    {
        // http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
        Vector3 half_euler = euler_radians * 0.5f;
        float cx = (float)cos(half_euler.getX());
        float sx = (float)sin(half_euler.getX());
        float cy = (float)cos(half_euler.getY());
        float sy = (float)sin(half_euler.getY());
        float cz = (float)cos(half_euler.getZ());
        float sz = (float)sin(half_euler.getZ());
        return Quat(sx*cy*cz - cx*sy*sz,
                cx*sy*cz + sx*cy*sz,
                cx*cy*sz - sx*sy*cz,
                cx*cy*cz + sx*sy*sz);
    }

    void CalculateNodeTransform(HScene scene, const Node& node, const Vector4& reference_scale, bool boundary, Matrix4* out_transform)
    {
        Vector4 position = node.m_Properties[dmGui::PROPERTY_POSITION];
        const Vector4& rotation = node.m_Properties[dmGui::PROPERTY_ROTATION];
        const Vector4& size = node.m_Properties[dmGui::PROPERTY_SIZE];
        const Vector4& scale = node.m_Properties[dmGui::PROPERTY_SCALE];
        const dmGui::Pivot pivot = (dmGui::Pivot) node.m_Pivot;

        HContext context = scene->m_Context;

        // Apply ref-scaling to scale uniformly, select the smallest scale component to make sure everything fits
        Vector3 adjust_scale = reference_scale.getXYZ();
        if (node.m_AdjustMode == dmGui::ADJUST_MODE_FIT)
        {
            float uniform = dmMath::Min(reference_scale.getX(), reference_scale.getY());
            adjust_scale = Vector3(uniform, uniform, 1.0f);
        }
        else if (node.m_AdjustMode == dmGui::ADJUST_MODE_ZOOM)
        {
            float uniform = dmMath::Max(reference_scale.getX(), reference_scale.getY());
            adjust_scale = Vector3(uniform, uniform, 1.0f);
        }
        Vector3 screen_dims = Vector3(context->m_Width, context->m_Height, 0.0f);
        Vector3 adjusted_dims = mulPerElem(screen_dims, adjust_scale);
        Vector3 offset = (Vector3(context->m_PhysicalWidth, context->m_PhysicalHeight, 0.0f) - adjusted_dims) * 0.5f;
        // Apply anchoring
        Vector4 scaled_position = mulPerElem(position, Vector4(adjust_scale, 1.0f));
        if (node.m_XAnchor == XANCHOR_LEFT)
        {
            offset.setX(0.0f);
            scaled_position.setX(position.getX() * reference_scale.getX());
        }
        else if (node.m_XAnchor == XANCHOR_RIGHT)
        {
            offset.setX(0.0f);
            float distance = (context->m_Width - position.getX()) * reference_scale.getX();
            scaled_position.setX(context->m_PhysicalWidth - distance);
        }
        if (node.m_YAnchor == YANCHOR_TOP)
        {
            offset.setY(0.0f);
            float distance = (context->m_Height - position.getY()) * reference_scale.getY();
            scaled_position.setY(context->m_PhysicalHeight - distance);
        }
        else if (node.m_YAnchor == YANCHOR_BOTTOM)
        {
            offset.setY(0.0f);
            scaled_position.setY(position.getY() * reference_scale.getY());
        }
        position = scaled_position;

        float width = size.getX();
        float height = size.getY();

        Vector4 delta_pivot = Vector4(0.0f, 0.0f, 0.0f, 0.0f);

        switch (pivot)
        {
            case dmGui::PIVOT_CENTER:
            case dmGui::PIVOT_S:
            case dmGui::PIVOT_N:
                delta_pivot.setX(width * 0.5f);
                break;

            case dmGui::PIVOT_NE:
            case dmGui::PIVOT_E:
            case dmGui::PIVOT_SE:
                delta_pivot.setX(width);
                break;

            case dmGui::PIVOT_SW:
            case dmGui::PIVOT_W:
            case dmGui::PIVOT_NW:
                break;
        }
        switch (pivot) {
            case dmGui::PIVOT_CENTER:
            case dmGui::PIVOT_E:
            case dmGui::PIVOT_W:
                delta_pivot.setY(height * 0.5f);
                break;

            case dmGui::PIVOT_N:
            case dmGui::PIVOT_NE:
            case dmGui::PIVOT_NW:
                delta_pivot.setY(height);
                break;

            case dmGui::PIVOT_S:
            case dmGui::PIVOT_SW:
            case dmGui::PIVOT_SE:
                break;
        }

        const float deg_to_rad = 3.1415926f / 180.0f;
        Quat r = EulerToQuat(rotation.getXYZ() * deg_to_rad);
        Vector3 s = mulPerElem(scale.getXYZ(), adjust_scale);
        Vector3 t = -delta_pivot.getXYZ();
        t = offset + position.getXYZ() + rotate(r, mulPerElem(s, t));
        *out_transform = Matrix4::rotation(r) * Matrix4::scale(s);
        out_transform->setTranslation(t);
        bool render_text = node.m_NodeType == NODE_TYPE_TEXT && !boundary;
        if (!render_text)
        {
            *out_transform *= Matrix4::scale(Vector3(width, height, 1));
        }
    }

    HScript NewScript(HContext context)
    {
        Script* script = new Script();
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;
        script->m_Context = context;
        return script;
    }

    void DeleteScript(HScript script)
    {
        delete script;
    }

    Result SetScript(HScript script, const char* source, uint32_t source_length, const char* filename)
    {
        lua_State* L = script->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result res = RESULT_OK;

        int ret = luaL_loadbuffer(L, source, source_length, filename);

        if (ret != 0)
        {
            dmLogError("Error compiling script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            res = RESULT_SYNTAX_ERROR;
            goto bail;
        }

        ret = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            res = RESULT_SCRIPT_ERROR;
            goto bail;
        }

        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (script->m_FunctionReferences[i] != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
                script->m_FunctionReferences[i] = LUA_NOREF;
            }

            lua_getglobal(L, SCRIPT_FUNCTION_NAMES[i]);
            if (lua_type(L, -1) == LUA_TFUNCTION)
            {
                script->m_FunctionReferences[i] = luaL_ref(L, LUA_REGISTRYINDEX);
            }
            else
            {
                if (lua_isnil(L, -1) == 0)
                    dmLogWarning("'%s' is not a function (%s)", SCRIPT_FUNCTION_NAMES[i], filename);
                lua_pop(L, 1);
            }

            lua_pushnil(L);
            lua_setglobal(L, SCRIPT_FUNCTION_NAMES[i]);
        }

bail:
        assert(top == lua_gettop(L));
        return res;
    }

    lua_State* GetLuaState(HContext context)
    {
        return context->m_LuaState;
    }

}  // namespace dmGui
