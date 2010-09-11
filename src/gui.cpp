#include "gui.h"

#include <string.h>
#include <dlib/array.h>
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
        Vector4* m_Value;
        Vector4  m_From;
        Vector4  m_To;
        float    m_Delay;
        float    m_Elapsed;
        float    m_Duration;
        float    m_BezierControlPoints[4];
        uint16_t m_FirstUpdate : 1;
    };

    struct Scene
    {
        int                   m_InitFunctionReference;
        int                   m_UpdateFunctionReference;
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

    static InternalNode* LuaCheckNode(lua_State*L, int index, HNode* hnode)
    {
        NodeProxy* np = NodeProxy_Check(L, 1);
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
        Vector3 from = CheckVector3(L, 3);
        Vector3 to = CheckVector3(L, 4);
        int easing = (int) luaL_checknumber(L, 5);
        lua_Number duration = luaL_checknumber(L, 6);
        float delay = 0.0f;
        if (lua_isnumber(L, 7))
            delay = (float) lua_tonumber(L, 7);

        if (property >= PROPERTY_COUNT)
        {
            luaL_error(L, "Invalid property index: %d", property);
        }

        if (easing >= EASING_COUNT)
        {
            luaL_error(L, "Invalid easing: %d", easing);
        }

        AnimateNode(scene, hnode, (Property) property, Vector4(from), Vector4(to), (Easing) easing, (float) duration, delay);

        assert(top== lua_gettop(L));
        return 0;
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
        Vector3 pos = CheckVector3(L, 2);\
        n->m_Node.m_Properties[property] = Vector4(pos);\
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
#if 0
        NodeProxy* np = NodeProxy_Check(L, 1);
        assert(np);
        Node* n = &np->m_Scene->m_Nodes[np->m_NodeIndex];
        n->m_Enabled = 0;
        if (n->m_Text)
            free((void*) n->m_Text);
        n->m_Text = 0;
        np->m_Scene->m_NodeIndexPool.Push(np->m_NodeIndex);

        np->m_Scene->m_RefCount--;
        if (np->m_Scene->m_RefCount == 0)
        {
            delete np->m_Scene;
        }
#endif

        return 0;
    }

    static int NodeProxy_tostring (lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        Vector4 pos = n->m_Node.m_Properties[PROPERTY_POSITION];
        lua_pushfstring(L, "%s@(%f, %f, %f)", n->m_Node.m_Text, pos.getX(), pos.getY(), pos.getZ());
        return 1;
    }

    static int PropertyNameToIndex(const char* name)
    {
        if (strcmp(name, "Position") == 0)
            return (int) PROPERTY_POSITION;
        else if (strcmp(name, "Rotation") == 0)
            return (int) PROPERTY_ROTATION;
        else if (strcmp(name, "Scale") == 0)
            return (int) PROPERTY_SCALE;
        else if (strcmp(name, "Color") == 0)
            return (int) PROPERTY_COLOR;
        else if (strcmp(name, "Extents") == 0)
            return (int) PROPERTY_EXTENTS;
        else
            return -1;
    }

    static int NodeProxy_index(lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "Text") == 0)
        {
            lua_pushstring(L, n->m_Node.m_Text);
        }
        else
        {
            luaL_error(L, "Unknown property: '%s'", key);
        }
        return 1;
    }

    static int NodeProxy_newindex(lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        const char* key = luaL_checkstring(L, 2);

        if (strcmp(key, "Text") == 0)
        {
            const char* text = luaL_checkstring(L, 3);
            if (n->m_Node.m_Text)
                free((void*) n->m_Node.m_Text);
            n->m_Node.m_Text = strdup(text);
        }
        else
        {
            luaL_error(L, "Unknown property: '%s'", key);
        }

        return 0;
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

        lua_pushliteral(L, "GetNode");
        lua_pushcfunction(L, LuaGetNode);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "Animate");
        lua_pushcfunction(L, LuaAnimate);
        lua_rawset(L, LUA_GLOBALSINDEX);

#define REGGETSET(name) \
        lua_pushliteral(L, "Get"#name);\
        lua_pushcfunction(L, LuaGet##name);\
        lua_rawset(L, LUA_GLOBALSINDEX);\
\
        lua_pushliteral(L, "Set"#name);\
        lua_pushcfunction(L, LuaSet##name);\
        lua_rawset(L, LUA_GLOBALSINDEX);\
        \

        REGGETSET(Position)
        REGGETSET(Rotation)
        REGGETSET(Scale)
        REGGETSET(Color)
        REGGETSET(Extents)

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

        luaL_unref(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
        delete scene;
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

    void UpdateAnimations(HScene scene, float dt)
    {
        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n = animations->Size();
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
                          3 * (1-t) * (1-t)* t * anim->m_BezierControlPoints[1] +
                          3 * (1-t) * t * t * anim->m_BezierControlPoints[2] +
                          t * t * t * anim->m_BezierControlPoints[3];

                *anim->m_Value = anim->m_From * (1-x) + anim->m_To * x;
            }
            else
            {
                anim->m_Delay -= dt;
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

        lua_getglobal(L, "Init");
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

        lua_getglobal(L, "Update");
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No update function found in script");
            lua_pop(L, 1);
            res = RESULT_MISSING_UPDATE_FUNCTION_ERROR;
            goto bail;
        }

        if (scene->m_UpdateFunctionReference != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, scene->m_UpdateFunctionReference);
        scene->m_UpdateFunctionReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_pushnil(L);
        lua_setglobal(L, "Init");
        lua_pushnil(L);
        lua_setglobal(L, "Update");

bail:
        assert(top == lua_gettop(L));
        return res;
    }

    HNode NewNode(HScene scene, const Point3& position, const Vector3& extents)
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
            node->m_Node.m_Properties[PROPERTY_COLOR] = Vector4(1,1,1,0);
            node->m_Node.m_Properties[PROPERTY_EXTENTS] = Vector4(extents, 0);
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

    void AnimateNode(HScene scene, HNode node, Property property, const Vector4& from, const Vector4& to, Easing easing, float duration, float delay)
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

            default:
                assert(0);
        }

        // Remove old animation for the same property
        for (uint32_t i = 0; i < scene->m_Animations.Size(); ++i)
        {
            const Animation* anim = &scene->m_Animations[i];
            if (animation.m_Value == anim->m_Value)
            {
                scene->m_Animations.EraseSwap(i);
                break;
            }
        }

        if (scene->m_Animations.Full())
        {
            dmLogWarning("Out of animation resources (%d)", scene->m_Animations.Size());
            return;
        }

        animation.m_From = from;
        animation.m_To = to;
        animation.m_Delay = delay;
        animation.m_Elapsed = 0.0f;
        animation.m_Duration = duration;
        animation.m_FirstUpdate = 1;

        switch (easing)
        {
            case EASING_NONE:
                animation.m_BezierControlPoints[0] = 0.0f;
                animation.m_BezierControlPoints[1] = 0.25f;
                animation.m_BezierControlPoints[2] = 0.75f;
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

        scene->m_Animations.Push(animation);
    }
}  // namespace dmGui
