#include "gui.h"

#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>

#include <script/script.h>

#include "gui_private.h"
#include "gui_script.h"

namespace dmGui
{
    static const char* SCRIPT_FUNCTION_NAMES[] =
    {
        "init",
        "final",
        "update",
        "on_message",
        "on_input",
        "on_reload"
    };

    Script::Script()
    : m_Context(0x0)
    {
        for (int i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
            m_FunctionReferences[i] = LUA_NOREF;
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

        return context;
    }

    void DeleteContext(HContext context)
    {
        FinalizeScript(context->m_LuaState);
        delete context;
    }

    void SetDefaultNewSceneParams(NewSceneParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_MaxNodes = 128;
        params->m_MaxAnimations = 128;
        params->m_MaxTextures = 32;
        params->m_MaxFonts = 4;
    }

    HScene NewScene(HContext context, const NewSceneParams* params)
    {
        Scene* scene = new Scene();
        scene->m_SelfReference = LUA_NOREF;
        scene->m_Context = context;
        scene->m_Script = 0x0;
        scene->m_Nodes.SetCapacity(params->m_MaxNodes);
        scene->m_Nodes.SetSize(params->m_MaxNodes);
        scene->m_NodePool.SetCapacity(params->m_MaxNodes);
        scene->m_Animations.SetCapacity(params->m_MaxAnimations);
        scene->m_Textures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_Fonts.SetCapacity(params->m_MaxFonts*2, params->m_MaxFonts);
        scene->m_DefaultFont = 0;
        scene->m_UserData = params->m_UserData;

        scene->m_NextVersionNumber = 0;

        scene->m_ReferenceWidth = 640;
        scene->m_ReferenceHeight = 480;
        scene->m_PhysicalWidth = scene->m_ReferenceWidth;
        scene->m_PhysicalHeight = scene->m_ReferenceHeight;

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            memset(n, 0, sizeof(*n));
            n->m_Index = 0xffff;
        }

        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;
        lua_newtable(L);
        scene->m_SelfReference = luaL_ref(L, LUA_REGISTRYINDEX);
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

        luaL_unref(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
        delete scene;
    }

    void SetReferenceResolution(HScene scene, uint32_t width, uint32_t height)
    {
        scene->m_ReferenceWidth = width;
        scene->m_ReferenceHeight = height;
    }

    void SetPhysicalResolution(HScene scene, uint32_t width, uint32_t height)
    {
        scene->m_PhysicalWidth = width;
        scene->m_PhysicalHeight = height;
    }

    void SetSceneUserData(HScene scene, void* user_data)
    {
        scene->m_UserData = user_data;
    }

    void* GetSceneUserData(HScene scene)
    {
        return scene->m_UserData;
    }

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count)
    {
        lua_State* L = scene->m_Context->m_LuaState;

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            const InputAction* ia = &input_actions[i];

            if (scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONINPUT] != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONINPUT]);
                assert(lua_isfunction(L, -1));
                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);

                dmScript::PushHash(L, ia->m_ActionId);

                lua_newtable(L);

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

                int ret = lua_pcall(L, 3, 0, 0);

                if (ret != 0)
                {
                    dmLogError("Error running script: %s", lua_tostring(L,-1));
                    lua_pop(L, 1);
                    return RESULT_SCRIPT_ERROR;
                }
            }

        }

        return RESULT_OK;
    }

    Result DispatchMessage(HScene scene, dmMessage::Message* message)
    {
        lua_State*L = scene->m_Context->m_LuaState;

        if (scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE] != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE]);
            assert(lua_isfunction(L, -1));
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);

            dmScript::PushHash(L, message->m_Id);

            if (message->m_Descriptor)
            {
                dmScript::PushDDF(L, (dmDDF::Descriptor*)message->m_Descriptor, (const char*) message->m_Data);
            }
            else
            {
                dmScript::PushTable(L, (const char*) message->m_Data);
            }

            dmScript::PushURL(L, message->m_Sender);

            int ret = lua_pcall(L, 4, 0, 0);

            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                return RESULT_SCRIPT_ERROR;
            }
        }

        return RESULT_OK;
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

    void RenderScene(HScene scene, RenderNode render_node, void* context)
    {
        // NOTE: Currently width is the reference for scale factor
        float scale_factor = (float) scene->m_PhysicalWidth / (float) scene->m_ReferenceWidth;

        Vector4 scale(scale_factor, scale_factor, 1, 1);

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            if (n->m_Index != 0xffff)
            {
                Vector4 saved_position = n->m_Node.m_Properties[PROPERTY_POSITION];
                Vector4 scaled_position = mulPerElem(saved_position, scale);

                if (n->m_Node.m_XAnchor == XANCHOR_RIGHT)
                {
                    float distance = (scene->m_ReferenceWidth - saved_position.getX()) * scale_factor;
                    scaled_position.setX(scene->m_PhysicalWidth - distance);
                }

                if (n->m_Node.m_YAnchor == YANCHOR_BOTTOM)
                {
                    float distance = (scene->m_ReferenceHeight - saved_position.getY()) * scale_factor;
                    scaled_position.setY(scene->m_PhysicalHeight - distance);
                }

                n->m_Node.m_Properties[PROPERTY_POSITION] = scaled_position;
                render_node(scene, &n->m_Node, 1, context);
                n->m_Node.m_Properties[PROPERTY_POSITION] = saved_position;
            }
        }
    }

    void UpdateAnimations(HScene scene, float dt)
    {
        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n = animations->Size();

        for (uint32_t i = 0; i < n; ++i)
        {
            Animation* anim = &(*animations)[i];

            if (anim->m_Elapsed >= anim->m_Duration)
            {
                continue;
            }

            if (anim->m_Delay > 0)
            {
                anim->m_Delay -= dt;
            }

            if (anim->m_Delay <= 0)
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
                float t = anim->m_Elapsed / anim->m_Duration;

                if (t > 1)
                    t = 1;

                float x = (1-t) * (1-t) * (1-t) * anim->m_BezierControlPoints[0] +
                          3 * (1-t) * (1-t) * t * anim->m_BezierControlPoints[1] +
                          3 * (1-t) * t * t * anim->m_BezierControlPoints[2] +
                          t * t * t * anim->m_BezierControlPoints[3];

                *anim->m_Value = anim->m_From * (1-x) + anim->m_To * x;

                if (anim->m_Elapsed + dt >= anim->m_Duration)
                {
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

        n = animations->Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Animation* anim = &(*animations)[i];

            if (anim->m_Elapsed >= anim->m_Duration)
            {
                animations->EraseSwap(i);
                i--;
                n--;
                continue;
            }
        }
    }

    Result InitScene(HScene scene)
    {
        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result result = RESULT_OK;
        if (scene->m_Script == 0x0)
            return result;

        lua_getglobal(L, "gui");
        lua_pushnumber(L, (lua_Number) scene->m_ReferenceWidth);
        lua_setfield(L, -2, "width");
        lua_pushnumber(L, (lua_Number) scene->m_ReferenceHeight);
        lua_setfield(L, -2, "height");
        lua_pop(L, 1);

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

        if (scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_INIT] != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_INIT]);
            assert(lua_isfunction(L, -1));

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
            int ret = lua_pcall(L, 1, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = RESULT_SCRIPT_ERROR;
            }
        }
        assert(top == lua_gettop(L));
        return result;
    }

    Result FinalScene(HScene scene)
    {
        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result result = RESULT_OK;
        if (scene->m_Script == 0x0)
            return result;

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

        if (scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_INIT] != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_INIT]);
            assert(lua_isfunction(L, -1));

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
            int ret = lua_pcall(L, 1, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = RESULT_SCRIPT_ERROR;
            }
        }
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
        }
        assert(top == lua_gettop(L));
        return result;
    }

    Result UpdateScene(HScene scene, float dt)
    {
        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result result = RESULT_OK;
        UpdateAnimations(scene, dt);
        if (scene->m_Script == 0x0)
            return result;

        lua_getglobal(L, "gui");
        lua_pushnumber(L, (lua_Number) scene->m_ReferenceWidth);
        lua_setfield(L, -2, "width");
        lua_pushnumber(L, (lua_Number) scene->m_ReferenceHeight);
        lua_setfield(L, -2, "height");
        lua_pop(L, 1);

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

        if (scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_UPDATE] != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_UPDATE]);

            assert(lua_isfunction(L, -1));

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
            lua_pushnumber(L, (lua_Number) dt);
            int ret = lua_pcall(L, 2, 0, 0);

            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = RESULT_SCRIPT_ERROR;
            }
        }

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
        }

        assert(top == lua_gettop(L));
        return result;
    }

    Result SetSceneScript(HScene scene, HScript script)
    {
        scene->m_Script = script;
        return RESULT_OK;
    }

    HNode NewNode(HScene scene, const Point3& position, const Vector3& extents, NodeType node_type)
    {
        if (scene->m_NodePool.Remaining() == 0)
        {
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
            node->m_Node.m_Properties[PROPERTY_EXTENTS] = Vector4(extents, 0);
            node->m_Node.m_NodeType = (uint32_t) node_type;
            node->m_Node.m_TextureHash = 0;
            node->m_Node.m_Texture = 0;
            node->m_Node.m_FontHash = 0;
            node->m_Node.m_Font = 0;
            node->m_Version = version;
            node->m_Index = index;
            scene->m_NextVersionNumber = (version + 1) % ((1 << 16) - 1);

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

        uint32_t n = scene->m_Nodes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            InternalNode* node = &scene->m_Nodes[i];
            if (node->m_NameHash == name_hash)
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

        scene->m_NodePool.Push(n->m_Index);
        n->m_Index = 0xffff;
        n->m_NameHash = 0;
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

    Vector4 GetNodeProperty(HScene scene, HNode node, Property property)
    {
        assert(property < PROPERTY_COUNT);
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Properties[property];
    }

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value)
    {
        assert(property < PROPERTY_COUNT);
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_Properties[property] = value;
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

    Result SetNodeTexture(HScene scene, HNode node, const char* texture_name)
    {
        uint64_t texture_hash = dmHashString64(texture_name);
        void** texture = scene->m_Textures.Get(texture_hash);
        if (texture)
        {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_TextureHash = texture_hash;
            n->m_Node.m_Texture = *texture;
            return RESULT_OK;
        }
        else
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
    }

    Result SetNodeFont(HScene scene, HNode node, const char* font_name)
    {
        uint64_t font_hash = dmHashString64(font_name);
        void** font = scene->m_Fonts.Get(font_hash);
        if (font)
        {
            InternalNode* n = GetNode(scene, node);
            n->m_Node.m_FontHash = font_hash;
            n->m_Node.m_Font = *font;
            return RESULT_OK;
        }
        else
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
    }

    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_BlendMode = (uint32_t) blend_mode;
    }

    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_XAnchor = (uint32_t) x_anchor;
    }

    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Node.m_YAnchor = (uint32_t) y_anchor;
    }

    void AnimateNode(HScene scene,
                     HNode node,
                     Property property,
                     const Vector4& to,
                     Easing easing,
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

        switch (property)
        {
            case PROPERTY_POSITION:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_POSITION];
                break;

            case PROPERTY_ROTATION:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_ROTATION];
                break;

            case PROPERTY_SCALE:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_SCALE];
                break;

            case PROPERTY_COLOR:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_COLOR];
                break;

            case PROPERTY_EXTENTS:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_EXTENTS];
                break;

            default:
                assert(0);
        }

        uint32_t animation_index = 0xffffffff;

        // Remove old animation for the same property
        for (uint32_t i = 0; i < scene->m_Animations.Size(); ++i)
        {
            const Animation* anim = &scene->m_Animations[i];
            if (animation.m_Value == anim->m_Value)
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
        animation.m_To = to;
        animation.m_Delay = delay;
        animation.m_Elapsed = 0.0f;
        animation.m_Duration = duration;
        animation.m_AnimationComplete = animation_complete;
        animation.m_Userdata1 = userdata1;
        animation.m_Userdata2 = userdata2;
        animation.m_FirstUpdate = 1;
        animation.m_AnimationCompleteCalled = 0;

        switch (easing)
        {
            case EASING_NONE:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 0.333333333f;
                animation.m_BezierControlPoints[2] = 0.666666667f;
                animation.m_BezierControlPoints[3] = 1.0f;
                break;

            case EASING_OUT:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 1.0f;
                animation.m_BezierControlPoints[2] = 1.0f;
                animation.m_BezierControlPoints[3] = 1.0f;
                break;

            case EASING_IN:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 0.0f;
                animation.m_BezierControlPoints[2] = 0.0f;
                animation.m_BezierControlPoints[3] = 1.0f;
                break;

            case EASING_INOUT:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 0.0f;
                animation.m_BezierControlPoints[2] = 1.0f;
                animation.m_BezierControlPoints[3] = 1.0f;
                break;

            default:
                assert(0);
        }

        scene->m_Animations[animation_index] = animation;
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
}  // namespace dmGui
