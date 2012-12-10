#include "gui.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
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

        return context;
    }

    void DeleteContext(HContext context)
    {
        FinalizeScript(context->m_LuaState);
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

    void RenderScene(HScene scene, RenderNodes render_nodes, void* context)
    {
        Vector4 scale = CalculateReferenceScale(scene->m_Context);
        Matrix4 node_transform;
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            if (n->m_Index != 0xffff && n->m_Enabled)
            {
                CalculateNodeTransform(scene, n->m_Node, scale, false, &node_transform);
                HNode node = GetNodeHandle(n);
                render_nodes(scene, &node, &node_transform, 1, context);
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

            if ((!anim->m_Enabled) || anim->m_Elapsed >= anim->m_Duration || anim->m_Cancelled)
            {
                continue;
            }

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

                float x = (1-t) * (1-t) * (1-t) * anim->m_BezierControlPoints[0] +
                          3 * (1-t) * (1-t) * t * anim->m_BezierControlPoints[1] +
                          3 * (1-t) * t * t * anim->m_BezierControlPoints[2] +
                          t * t * t * anim->m_BezierControlPoints[3];

                *anim->m_Value = anim->m_From * (1-x) + anim->m_To * x;

                // Animation complete, see above
                if (t >= 1.0f)
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
    }

    struct InputArgs
    {
        const InputAction* m_Action;
        bool m_Consumed;
    };

    Result RunScript(HScene scene, ScriptFunction script_function, void* args)
    {
        if (scene->m_Script == 0x0)
            return RESULT_OK;

        lua_State* L = scene->m_Context->m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        if (scene->m_Script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_pushlightuserdata(L, (void*) scene);
            lua_setglobal(L, "__scene__");

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_Script->m_FunctionReferences[script_function]);
            assert(lua_isfunction(L, -1));
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);

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
            lua_pushlightuserdata(L, (void*) 0x0);
            lua_setglobal(L, "__scene__");
            return result;
        }
        assert(top == lua_gettop(L));
        return RESULT_OK;
    }

    Result InitScene(HScene scene)
    {
        return RunScript(scene, SCRIPT_FUNCTION_INIT, 0x0);
    }

    Result FinalScene(HScene scene)
    {
        Result result = RunScript(scene, SCRIPT_FUNCTION_FINAL, 0x0);

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
        Result result = RunScript(scene, SCRIPT_FUNCTION_UPDATE, (void*)&dt);

        UpdateAnimations(scene, dt);

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

        return result;
    }

    Result DispatchMessage(HScene scene, dmMessage::Message* message)
    {
        return RunScript(scene, SCRIPT_FUNCTION_ONMESSAGE, (void*)message);
    }

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count, bool* input_consumed)
    {
        InputArgs args;
        args.m_Consumed = false;
        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            args.m_Action = &input_actions[i];
            Result result = RunScript(scene, SCRIPT_FUNCTION_ONINPUT, (void*)&args);
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
        return RunScript(scene, SCRIPT_FUNCTION_ONRELOAD, 0x0);
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
            node->m_Node.m_NodeType = (uint32_t) node_type;
            node->m_Node.m_TextureHash = 0;
            node->m_Node.m_Texture = 0;
            node->m_Node.m_FontHash = 0;
            node->m_Node.m_Font = 0;
            node->m_Version = version;
            node->m_Index = index;
            node->m_Enabled = 1;
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

    void ClearNodes(HScene scene)
    {
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            memset(n, 0, sizeof(*n));
            n->m_Index = 0xffff;
        }
        scene->m_NodePool.Clear();
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

    void* GetNodeTexture(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Texture;
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

    void* GetNodeFont(HScene scene, HNode node)
    {
        InternalNode* n = GetNode(scene, node);
        return n->m_Node.m_Font;
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

            case PROPERTY_OUTLINE:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_OUTLINE];
                break;

            case PROPERTY_SHADOW:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_SHADOW];
                break;

            case PROPERTY_SIZE:
                animation.m_Value = &n->m_Node.m_Properties[PROPERTY_SIZE];
                break;

            default:
                assert(0);
                break;
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
        animation.m_Cancelled = 0;
        animation.m_Enabled = n->m_Enabled;

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
                break;
        }

        scene->m_Animations[animation_index] = animation;
    }

    void CancelAnimation(HScene scene, HNode node, Property property)
    {
        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n_animations = animations->Size();

        InternalNode* n = GetNode(scene, node);

        for (uint32_t i = 0; i < n_animations; ++i)
        {
            Animation* anim = &(*animations)[i];
            Vector4* value = &n->m_Node.m_Properties[property];
            if (anim->m_Node == node && anim->m_Value == value)
            {
                anim->m_Cancelled = 1;
                return;
            }
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

    void SetNodeEnabled(HScene scene, HNode node, bool enabled)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_Enabled = enabled;
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
            scaled_position.setX(position.getX() * adjust_scale.getX());
        }
        else if (node.m_XAnchor == XANCHOR_RIGHT)
        {
            offset.setX(0.0f);
            float distance = (context->m_Width - position.getX()) * adjust_scale.getX();
            scaled_position.setX(context->m_PhysicalWidth - distance);
        }
        if (node.m_YAnchor == YANCHOR_TOP)
        {
            offset.setY(0.0f);
            float distance = (context->m_Height - position.getY()) * adjust_scale.getY();
            scaled_position.setY(context->m_PhysicalHeight - distance);
        }
        else if (node.m_YAnchor == YANCHOR_BOTTOM)
        {
            offset.setY(0.0f);
            scaled_position.setY(position.getY() * adjust_scale.getY());
        }
        position = scaled_position;

        float width = size.getX();
        float height = size.getY();
        TextMetrics metrics;
        if (node.m_NodeType == dmGui::NODE_TYPE_TEXT)
        {
            if (scene->m_Context->m_GetTextMetricsCallback != 0x0)
            {
                scene->m_Context->m_GetTextMetricsCallback(node.m_Font, node.m_Text, &metrics);
            }
            width = metrics.m_Width;
            height = metrics.m_MaxAscent + metrics.m_MaxDescent;
        }

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
        bool render_text = node.m_NodeType == NODE_TYPE_TEXT && !boundary;
        switch (pivot) {
            case dmGui::PIVOT_CENTER:
            case dmGui::PIVOT_E:
            case dmGui::PIVOT_W:
                if (render_text)
                    delta_pivot.setY(-metrics.m_MaxDescent * 0.5f + metrics.m_MaxAscent * 0.5f);
                else
                    delta_pivot.setY(height * 0.5f);
                break;

            case dmGui::PIVOT_N:
            case dmGui::PIVOT_NE:
            case dmGui::PIVOT_NW:
                if (render_text)
                    delta_pivot.setY(metrics.m_MaxAscent);
                else
                    delta_pivot.setY(height);
                break;

            case dmGui::PIVOT_S:
            case dmGui::PIVOT_SW:
            case dmGui::PIVOT_SE:
                if (render_text)
                    delta_pivot.setY(-metrics.m_MaxDescent);
                break;
        }

        const float deg_to_rad = 3.1415926f / 180.0f;
        Quat r = EulerToQuat(rotation.getXYZ() * deg_to_rad);
        Vector3 s = mulPerElem(scale.getXYZ(), adjust_scale);
        Vector3 t = -delta_pivot.getXYZ();
        t = offset + position.getXYZ() + rotate(r, mulPerElem(s, t));
        *out_transform = Matrix4::rotation(r) * Matrix4::scale(s);
        out_transform->setTranslation(t);
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
