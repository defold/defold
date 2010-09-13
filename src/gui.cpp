#include "gui.h"

#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

namespace dmGui
{
    struct Gui
    {
        lua_State* m_LuaState;
    };

    struct InternalNode
    {
        Node     m_Node;
        uint64_t m_NameHash;
        uint16_t m_Version;
        uint16_t m_Index;
    };

    struct Animation
    {
        HNode    m_Node;
        Vector4* m_Value;
        Vector4  m_From;
        Vector4  m_To;
        float    m_Delay;
        float    m_Elapsed;
        float    m_Duration;
        float    m_BezierControlPoints[4];
        AnimationComplete m_AnimationComplete;
        void*    m_Userdata1;
        void*    m_Userdata2;
        uint16_t m_FirstUpdate : 1;
        uint16_t m_AnimationCompleteCalled : 1;
    };

    struct Scene
    {
        int                   m_InitFunctionReference;
        int                   m_UpdateFunctionReference;
        int                   m_OnInputFunctionReference;
        int                   m_SelfReference;
        uint16_t              m_RunInit : 1;
        Gui*                  m_Gui;
        dmIndexPool16         m_NodePool;
        dmArray<InternalNode> m_Nodes;
        dmArray<Animation>    m_Animations;
        dmHashTable64<void*>  m_Textures;
        dmHashTable64<void*>  m_Fonts;
        uint16_t              m_NextVersionNumber;
    };

    struct NodeProxy
    {
        HScene m_Scene;
        HNode  m_Node;
    };

    static InternalNode* GetNode(HScene scene, HNode node)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);
        assert(n->m_Index == index);
        return n;
    }

    static bool IsValidNode(HScene scene, HNode node)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        if (index < scene->m_Nodes.Size())
        {
            InternalNode* n = &scene->m_Nodes[index];
            return n->m_Version == version && n->m_Index == index;
        }
        else
        {
            return false;
        }
    }

    // Lua API
    #define NODEPROXY "NodeProxy"

    Vector4 CheckVector4(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "X");
        lua_pop(L, 1);
        lua_rawgeti(L, index, 1);
        lua_rawgeti(L, index, 2);
        lua_rawgeti(L, index, 3);
        lua_rawgeti(L, index, 4);

        float x = luaL_checknumber(L, -4);
        float y = luaL_checknumber(L, -3);
        float z = luaL_checknumber(L, -2);
        float w = 0.0f;
        if (lua_isnumber(L, -1))
            w = lua_tonumber(L, -1);

        lua_pop(L, 4);

        return Vector4(x, y, z, w);
    }

    Vector3 CheckVector3(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "X");
        lua_pop(L, 1);
        lua_rawgeti(L, index, 1);
        lua_rawgeti(L, index, 2);
        lua_rawgeti(L, index, 3);

        float x = luaL_checknumber(L, -3);
        float y = luaL_checknumber(L, -2);
        float z = luaL_checknumber(L, -1);
        lua_pop(L, 3);

        return Vector3(x, y, z);
    }

    void PushVector3(lua_State* L, const Vector4& vec)
    {
        lua_newtable(L);

        lua_pushnumber(L, vec.getX());
        lua_rawseti(L, -2, 1);

        lua_pushnumber(L, vec.getY());
        lua_rawseti(L, -2, 2);

        lua_pushnumber(L, vec.getZ());
        lua_rawseti(L, -2, 3);
    }

    void PushVector4(lua_State* L, const Vector4& vec)
    {
        lua_newtable(L);

        lua_pushnumber(L, vec.getX());
        lua_rawseti(L, -2, 1);

        lua_pushnumber(L, vec.getY());
        lua_rawseti(L, -2, 2);

        lua_pushnumber(L, vec.getZ());
        lua_rawseti(L, -2, 3);

        lua_pushnumber(L, vec.getW());
        lua_rawseti(L, -2, 4);
    }

    static NodeProxy* NodeProxy_Check(lua_State *L, int index)
    {
        NodeProxy* n;
        luaL_checktype(L, index, LUA_TUSERDATA);
        n = (NodeProxy*)luaL_checkudata(L, index, NODEPROXY);
        if (n == NULL) luaL_typerror(L, index, NODEPROXY);
        return n;
    }

    static bool LuaIsNode(lua_State *L, int ud)
    {
        void *p = lua_touserdata(L, ud);
        if (p != NULL)
        {
            if (lua_getmetatable(L, ud))
            {
                lua_getfield(L, LUA_REGISTRYINDEX, NODEPROXY);
                if (lua_rawequal(L, -1, -2))
                {
                    lua_pop(L, 2);
                    return true;
                }
            }
        }
        return false;
    }

    static InternalNode* LuaCheckNode(lua_State*L, int index, HNode* hnode)
    {
        NodeProxy* np = NodeProxy_Check(L, index);
        if (IsValidNode(np->m_Scene, np->m_Node))
        {
            InternalNode*n = GetNode(np->m_Scene, np->m_Node);
            if (hnode)
                *hnode = np->m_Node;
            return n;
        }
        else
        {
            luaL_error(L, "Deleted node");
        }

        return 0; // Never reaached
    }

    int LuaGetNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        HNode node = GetNodeByName(scene, name);
        if (node == 0)
        {
            luaL_error(L, "No such node: %s", name);
        }

        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODEPROXY);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    void LuaAnimationComplete(HScene scene, HNode node, void* userdata1, void* userdata2)
    {
        lua_State* L = scene->m_Gui->m_LuaState;

        int ref = (int) userdata1;
        int node_ref = (int) userdata2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, node_ref);

        int ret = lua_pcall(L, 1, 0, 0);
        if (ret != 0)
        {
            dmLogError("Error running animation callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }

        lua_unref(L, ref);
        lua_unref(L, node_ref);
    }

    int LuaAnimate(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        HNode hnode;
        InternalNode* node = LuaCheckNode(L, 1, &hnode);
        (void) node;

        int property = (int) luaL_checknumber(L, 2);
        Vector4 to = CheckVector4(L, 3);
        int easing = (int) luaL_checknumber(L, 4);
        lua_Number duration = luaL_checknumber(L, 5);
        float delay = 0.0f;
        int node_ref = LUA_NOREF;
        int animation_complete_ref = LUA_NOREF;
        if (lua_isnumber(L, 6))
        {
            delay = (float) lua_tonumber(L, 6);
            if (lua_isfunction(L, 7))
            {
                lua_pushvalue(L, 7);
                animation_complete_ref = luaL_ref(L, LUA_REGISTRYINDEX);
                lua_pushvalue(L, 1);
                node_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            }
        }

        if (property >= PROPERTY_COUNT)
        {
            luaL_error(L, "Invalid property index: %d", property);
        }

        if (easing >= EASING_COUNT)
        {
            luaL_error(L, "Invalid easing: %d", easing);
        }

        if (animation_complete_ref == LUA_NOREF)
            AnimateNode(scene, hnode, (Property) property, to, (Easing) easing, (float) duration, delay, 0, 0, 0);
        else
        {
            AnimateNode(scene, hnode, (Property) property, to, (Easing) easing, (float) duration, delay, &LuaAnimationComplete, (void*) animation_complete_ref, (void*) node_ref);
        }

        assert(top== lua_gettop(L));
        return 0;
    }

    static int LuaDoNewNode(lua_State* L, Point3 pos, Vector3 ext, NodeType node_type, const char* text)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        HNode node = NewNode(scene, pos, ext, node_type);
        if (!node)
        {
            luaL_error(L, "Out of nodes (max %d)", scene->m_Nodes.Capacity());
        }
        SetNodeText(scene, node, text);

        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODEPROXY);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    static int LuaHash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        char buf[16+1];
        DM_SNPRINTF(buf, sizeof(buf), "%llX", dmHashString64(str));
        lua_pushstring(L, buf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int LuaNewBoxNode(lua_State* L)
    {
        Vector3 pos = CheckVector3(L, 1);
        Vector3 ext = CheckVector3(L, 2);
        return LuaDoNewNode(L, Point3(pos), ext, NODE_TYPE_BOX, 0);
    }

    static int LuaNewTextNode(lua_State* L)
    {
        Vector3 pos = CheckVector3(L, 1);
        Vector3 ext = Vector3(1,1,1);
        const char* text = luaL_checkstring(L, 2);
        return LuaDoNewNode(L, Point3(pos), ext, NODE_TYPE_TEXT, text);
    }

#define LUAGETSET(name, property) \
    int LuaGet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        PushVector3(L, n->m_Node.m_Properties[property]);\
        return 1;\
    }\
\
    int LuaSet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        Vector4 pos = CheckVector4(L, 2);\
        n->m_Node.m_Properties[property] = pos;\
        return 0;\
    }\

    LUAGETSET(Position, PROPERTY_POSITION)
    LUAGETSET(Rotation, PROPERTY_ROTATION)
    LUAGETSET(Scale, PROPERTY_SCALE)
    LUAGETSET(Color, PROPERTY_COLOR)
    LUAGETSET(Extents, PROPERTY_EXTENTS)

#undef LUAGETSET

    static int NodeProxy_gc (lua_State *L)
    {
        return 0;
    }

    static int NodeProxy_tostring (lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        Vector4 pos = n->m_Node.m_Properties[PROPERTY_POSITION];
        lua_pushfstring(L, "%s@(%f, %f, %f)", n->m_Node.m_Text, pos.getX(), pos.getY(), pos.getZ());
        return 1;
    }

    static int NodeProxy_index(lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "text") == 0)
        {
            lua_pushstring(L, n->m_Node.m_Text);
        }
        else if (strcmp(key, "blend_mode") == 0)
        {
            lua_pushnumber(L, (lua_Number) n->m_Node.m_BlendMode);
        }
        else
        {
            luaL_error(L, "Unknown property: '%s'", key);
        }
        return 1;
    }

    static int NodeProxy_newindex(lua_State *L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        const char* key = luaL_checkstring(L, 2);

        if (strcmp(key, "text") == 0)
        {
            const char* text = luaL_checkstring(L, 3);
            if (n->m_Node.m_Text)
                free((void*) n->m_Node.m_Text);
            n->m_Node.m_Text = strdup(text);
        }
        else if (strcmp(key, "blend_mode") == 0)
        {
            int blend_mode = (int) luaL_checknumber(L, 3);
            n->m_Node.m_BlendMode = (BlendMode) blend_mode;
        }
        else if (strcmp(key, "texture") == 0)
        {
            lua_getglobal(L, "__scene__");
            Scene* scene = (Scene*) lua_touserdata(L, -1);
            lua_pop(L, 1);

            const char* texture_name = luaL_checkstring(L, 3);
            Result r = SetNodeTexture(scene, hnode, texture_name);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Texture %s is not specified in scene", texture_name);
            }
        }
        else if (strcmp(key, "font") == 0)
        {
            lua_getglobal(L, "__scene__");
            Scene* scene = (Scene*) lua_touserdata(L, -1);
            lua_pop(L, 1);

            const char* font_name = luaL_checkstring(L, 3);
            Result r = SetNodeFont(scene, hnode, font_name);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Font %s is not specified in scene", font_name);
            }
        }
        else
        {
            luaL_error(L, "Unknown property: '%s'", key);
        }

        return 0;
    }

    static int NodeProxy_eq(lua_State *L)
    {
        if (!LuaIsNode(L, 1))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        if (!LuaIsNode(L, 2))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        HNode hn1, hn2;
        InternalNode* n1 = LuaCheckNode(L, 1, &hn1);
        InternalNode* n2 = LuaCheckNode(L, 2, &hn2);
        (void) n1;
        (void) n2;

        lua_pushboolean(L, (int) (hn1 == hn2));
        return 1;
    }

    static const luaL_reg NodeProxy_methods[] =
    {
        {0, 0}
    };

    static const luaL_reg NodeProxy_meta[] =
    {
        {"__gc",       NodeProxy_gc},
        {"__tostring", NodeProxy_tostring},
        {"__index",    NodeProxy_index},
        {"__newindex", NodeProxy_newindex},
        {"__eq",       NodeProxy_eq},
        {0, 0}
    };

    HGui New()
    {
        Gui* gui = new Gui();
        gui->m_LuaState = lua_open();
        lua_State *L = gui->m_LuaState;

        luaL_openlib(L, NODEPROXY, NodeProxy_methods, 0);   // create methods table, add it to the globals
        luaL_newmetatable(L, NODEPROXY);                         // create metatable for Image, add it to the Lua registry
        luaL_openlib(L, 0, NodeProxy_meta, 0);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, -3);                       // dup methods table
        lua_rawset(L, -3);                          // hide metatable: metatable.__metatable = methods
        lua_pop(L, 1);                              // drop metatable

        lua_pushliteral(L, "get_node");
        lua_pushcfunction(L, LuaGetNode);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "animate");
        lua_pushcfunction(L, LuaAnimate);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "hash");
        lua_pushcfunction(L, LuaHash);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "new_box_node");
        lua_pushcfunction(L, LuaNewBoxNode);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "new_text_node");
        lua_pushcfunction(L, LuaNewTextNode);
        lua_rawset(L, LUA_GLOBALSINDEX);

#define REGGETSET(name, luaname) \
        lua_pushliteral(L, "get_"#luaname);\
        lua_pushcfunction(L, LuaGet##name);\
        lua_rawset(L, LUA_GLOBALSINDEX);\
\
        lua_pushliteral(L, "set_"#luaname);\
        lua_pushcfunction(L, LuaSet##name);\
        lua_rawset(L, LUA_GLOBALSINDEX);\
        \

        REGGETSET(Position, position)
        REGGETSET(Rotation, rotation)
        REGGETSET(Scale, scale)
        REGGETSET(Color, color)
        REGGETSET(Extents, extents)

#undef REGGETSET

#define SETPROP(name) \
        lua_pushnumber(L, (lua_Number) PROPERTY_##name); \
        lua_setglobal(L, #name);\

        SETPROP(POSITION)
        SETPROP(ROTATION)
        SETPROP(SCALE)
        SETPROP(COLOR)
        SETPROP(EXTENTS)

#undef SETPROP

#define SETEASING(name) \
        lua_pushnumber(L, (lua_Number) EASING_##name); \
        lua_setglobal(L, "EASING_"#name);\

        SETEASING(NONE)
        SETEASING(IN)
        SETEASING(OUT)
        SETEASING(INOUT)

#undef SETEASING

#define SETBLEND(name) \
        lua_pushnumber(L, (lua_Number) BLEND_MODE_##name); \
        lua_setglobal(L, "BLEND_MODE_"#name);\

        SETBLEND(ALPHA)
        SETBLEND(ADD)
        SETBLEND(ADD_ALPHA)
        SETBLEND(MULT)

#undef SETBLEND

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        //luaopen_debug(L);

        return gui;
    }

    void Delete(HGui gui)
    {
        lua_close(gui->m_LuaState);
        delete gui;
    }

    void SetDefaultNewSceneParams(NewSceneParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_MaxNodes = 128;
        params->m_MaxAnimations = 128;
        params->m_MaxTextures = 32;
        params->m_MaxFonts = 4;
    }

    HScene NewScene(HGui gui, const NewSceneParams* params)
    {
        Scene* scene = new Scene();
        scene->m_InitFunctionReference = LUA_NOREF;
        scene->m_UpdateFunctionReference = LUA_NOREF;
        scene->m_OnInputFunctionReference = LUA_NOREF;
        scene->m_SelfReference = LUA_NOREF;
        scene->m_RunInit = 0;
        scene->m_Gui = gui;
        scene->m_Nodes.SetCapacity(params->m_MaxNodes);
        scene->m_Nodes.SetSize(params->m_MaxNodes);
        scene->m_NodePool.SetCapacity(params->m_MaxNodes);
        scene->m_Animations.SetCapacity(params->m_MaxAnimations);
        scene->m_Textures.SetCapacity(params->m_MaxTextures*2, params->m_MaxTextures);
        scene->m_Fonts.SetCapacity(params->m_MaxFonts*2, params->m_MaxFonts);

        scene->m_NextVersionNumber = 0;

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            memset(n, 0, sizeof(*n));
            n->m_Index = 0xffff;
        }

        lua_State*L = scene->m_Gui->m_LuaState;
        int top = lua_gettop(L);
        (void) top;
        lua_newtable(L);
        scene->m_SelfReference = luaL_ref(L, LUA_REGISTRYINDEX);
        assert(top == lua_gettop(L));

        return scene;
    }

    void DeleteScene(HScene scene)
    {
        lua_State*L = scene->m_Gui->m_LuaState;

        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            if (n->m_Node.m_Text)
                free((void*) n->m_Node.m_Text);
        }

        if (scene->m_InitFunctionReference != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, scene->m_InitFunctionReference);

        if (scene->m_UpdateFunctionReference != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, scene->m_UpdateFunctionReference);

        if (scene->m_OnInputFunctionReference != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, scene->m_OnInputFunctionReference);

        luaL_unref(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
        delete scene;
    }

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count)
    {
        lua_State*L = scene->m_Gui->m_LuaState;

        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            const InputAction* ia = &input_actions[i];

            if (scene->m_OnInputFunctionReference != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_OnInputFunctionReference);
                assert(lua_isfunction(L, -1));
                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);

                lua_newtable(L);
                lua_pushstring(L, "action_id");
                char tmp[16+1];
                DM_SNPRINTF(tmp, sizeof(tmp), "%llX", ia->m_ActionId);
                lua_pushstring(L, tmp);
                lua_rawset(L, -3);

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

                int ret = lua_pcall(L, 2, 0, 0);

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

    Result AddTexture(HScene scene, const char* texture_name, void* texture)
    {
        if (scene->m_Textures.Full())
            return RESULT_OUT_OF_RESOURCES;

        scene->m_Textures.Put(dmHashString64(texture_name), texture);
        return RESULT_OK;
    }

    Result AddFont(HScene scene, const char* font_name, void* font)
    {
        if (scene->m_Fonts.Full())
            return RESULT_OUT_OF_RESOURCES;

        scene->m_Fonts.Put(dmHashString64(font_name), font);
        return RESULT_OK;
    }

    void RenderScene(HScene scene, RenderNode render_node, void* context)
    {
        for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        {
            InternalNode* n = &scene->m_Nodes[i];
            render_node(scene, &n->m_Node, 1, context);
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

            if (anim->m_Delay <= 0)
            {
                if (anim->m_FirstUpdate)
                {
                    anim->m_From = *anim->m_Value;
                    anim->m_FirstUpdate = 0;
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
                //if (t >= 1)
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

            if (anim->m_Elapsed >= anim->m_Duration)
            {
                animations->EraseSwap(i);
                i--;
                n--;
                continue;
            }
        }
    }

    Result UpdateScene(HScene scene, float dt)
    {
        lua_State*L = scene->m_Gui->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result result = RESULT_OK;
        UpdateAnimations(scene, dt);

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

        if (scene->m_RunInit)
        {
            scene->m_RunInit = 0;

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InitFunctionReference);
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

        if (result == RESULT_OK)
        {
            if (scene->m_UpdateFunctionReference != LUA_NOREF)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_UpdateFunctionReference);

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
        }

        assert(top == lua_gettop(L));
        return result;
    }

    Result SetSceneScript(HScene scene, const char* script, uint32_t script_length)
    {
        lua_State*L = scene->m_Gui->m_LuaState;
        int top = lua_gettop(L);
        (void) top;

        Result res = RESULT_OK;
        int ret = luaL_loadbuffer(L, script, script_length, "script");

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

        lua_getglobal(L, "init");
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
        }
        else
        {
            scene->m_RunInit = 1;
            if (scene->m_InitFunctionReference != LUA_NOREF)
                luaL_unref(L, LUA_REGISTRYINDEX, scene->m_InitFunctionReference);
            scene->m_InitFunctionReference = luaL_ref( L, LUA_REGISTRYINDEX );
        }

        lua_getglobal(L, "update");
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
        }
        else
        {
            if (scene->m_UpdateFunctionReference != LUA_NOREF)
                luaL_unref(L, LUA_REGISTRYINDEX, scene->m_UpdateFunctionReference);
            scene->m_UpdateFunctionReference = luaL_ref( L, LUA_REGISTRYINDEX );
        }

        lua_getglobal(L, "on_input");
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
        }
        else
        {
            if (scene->m_OnInputFunctionReference != LUA_NOREF)
                luaL_unref(L, LUA_REGISTRYINDEX, scene->m_OnInputFunctionReference);
            scene->m_OnInputFunctionReference = luaL_ref( L, LUA_REGISTRYINDEX );
        }

        lua_pushnil(L);
        lua_setglobal(L, "init");
        lua_pushnil(L);
        lua_setglobal(L, "update");

bail:
        assert(top == lua_gettop(L));
        return res;
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
            node->m_Version = version;
            node->m_Index = index;
            scene->m_NextVersionNumber = (version + 1) % (1 << 16 - 1);

            return hnode;
        }
    }

    void SetNodeName(HScene scene, HNode node, const char* name)
    {
        InternalNode* n = GetNode(scene, node);
        n->m_NameHash = dmHashString64(name);
    }

    HNode GetNodeByName(HScene scene, const char* name)
    {
        uint64_t name_hash = dmHashString64(name);

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

            case EASING_IN:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 1.0f;
                animation.m_BezierControlPoints[2] = 1.0f;
                animation.m_BezierControlPoints[3] = 1.0f;
                break;

            case EASING_OUT:
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
}  // namespace dmGui
