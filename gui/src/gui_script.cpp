#include "gui_script.h"

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>

#include <script/script.h>

#include "gui.h"
#include "gui_private.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

namespace dmGui
{
    #define LIB_NAME "gui"
    #define NODE_PROXY_TYPE_NAME "NodeProxy"

    static NodeProxy* NodeProxy_Check(lua_State *L, int index)
    {
        NodeProxy* n;
        luaL_checktype(L, index, LUA_TUSERDATA);
        n = (NodeProxy*)luaL_checkudata(L, index, NODE_PROXY_TYPE_NAME);
        if (n == NULL) luaL_typerror(L, index, NODE_PROXY_TYPE_NAME);
        return n;
    }

    static bool LuaIsNode(lua_State *L, int ud)
    {
        void *p = lua_touserdata(L, ud);
        if (p != NULL)
        {
            if (lua_getmetatable(L, ud))
            {
                lua_getfield(L, LUA_REGISTRYINDEX, NODE_PROXY_TYPE_NAME);
                if (lua_rawequal(L, -1, -2))
                {
                    lua_pop(L, 2);
                    return true;
                }
            }
        }
        return false;
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

    static int NodeProxy_gc (lua_State *L)
    {
        return 0;
    }

    static int NodeProxy_tostring (lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        Vector4 pos = n->m_Node.m_Properties[PROPERTY_POSITION];
        switch (n->m_Node.m_NodeType)
        {
            case NODE_TYPE_BOX:
                lua_pushfstring(L, "box@(%f, %f, %f)", pos.getX(), pos.getY(), pos.getZ());
                break;
            case NODE_TYPE_TEXT:
                lua_pushfstring(L, "%s@(%f, %f, %f)", n->m_Node.m_Text, pos.getX(), pos.getY(), pos.getZ());
                break;
            default:
                lua_pushfstring(L, "unknown@(%f, %f, %f)", pos.getX(), pos.getY(), pos.getZ());
                break;
        }
        return 1;
    }

    static int NodeProxy_index(lua_State *L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        (void)n;

        const char* key = luaL_checkstring(L, 2);
        return luaL_error(L, "Illegal operation, try %s.get_%s(<node>)", LIB_NAME, key);
    }

    static int NodeProxy_newindex(lua_State *L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;
        const char* key = luaL_checkstring(L, 2);

        return luaL_error(L, "Illegal operation, try %s.set_%s(<node>, <value>)", LIB_NAME, key);
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

    /*#
     * Get node from name
     * @name gui.get_node
     * @param name name of the node to get
     * @return node instance
     */
    int LuaGetNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        HNode node = GetNodeById(scene, name);
        if (node == 0)
        {
            luaL_error(L, "No such node: %s", name);
        }

        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*#
     * Delete node
     * @name gui.delete_node
     * @param node node to delete
     */
    int LuaDeleteNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        // Set deferred delete flag
        n->m_Deleted = 1;

        assert(top == lua_gettop(L));

        return 0;
    }

    void LuaAnimationComplete(HScene scene, HNode node, void* userdata1, void* userdata2)
    {
        lua_State* L = scene->m_Context->m_LuaState;

        int ref = (int) userdata1;
        int node_ref = (int) userdata2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_SelfReference);
        lua_rawgeti(L, LUA_REGISTRYINDEX, node_ref);

        int ret = lua_pcall(L, 2, 0, 0);
        if (ret != 0)
        {
            dmLogError("Error running animation callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }

        lua_unref(L, ref);
        lua_unref(L, node_ref);
    }

    /*#
     * No easing interpolation
     * @name gui.EASING_NONE
     * @variable
     */

    /*#
     * In easing interpolation
     * @name gui.EASING_IN
     * @variable
     */

    /*#
     * Out easing interpolation
     * @name gui.EASING_OUT
     * @variable
     */

    /*#
     * In and out easing interpolation
     * @name gui.EASING_INOUT
     * @variable
     */

    /*#
     * Animate node property
     * @name gui.animate
     * @param node node to animate
     * @param property property to animate
     * @param to target property value (vector3 or vector4)
     * @param easing easing. EASING_NONE, EASING_IN, EASING_OUT or EASING_INOUT
     * @param duration animation duration
     */

    /*#
     * Animate node property
     * @name gui.animate
     * @param node node to animate
     * @param property property to animate
     * @param to target property value (vector3 or vector4)
     * @param easing easing. EASING_NONE, EASING_IN, EASING_OUT or EASING_INOUT
     * @param duration animation duration
     * @param delay animation delay.
     */

    /*#
     * Animate node property
     * @name gui.animate
     * @param node node to animate
     * @param property property to animate
     * @param to target property value (vector3 or vector4)
     * @param easing easing. EASING_NONE, EASING_IN, EASING_OUT or EASING_INOUT
     * @param duration animation duration
     * @param delay animation delay.
     * @param complete_function function to call when animation is completed.
     */
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
        Vector4 to;
        if (dmScript::IsVector3(L, 3))
            to = Vector4(*dmScript::CheckVector3(L, 3));
        else
            to = *dmScript::CheckVector4(L, 3);
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

    /*#
     * Cancel animation
     * @name gui.cancel_animation
     * @param node node to cancel property for
     * @param property property to cancel
     */
    int LuaCancelAnimation(lua_State* L)
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

        if (property >= PROPERTY_COUNT)
        {
            luaL_error(L, "Invalid property index: %d", property);
        }

        CancelAnimation(scene, hnode, (Property) property);

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
        GetNode(scene, node)->m_Node.m_Font = scene->m_DefaultFont;
        SetNodeText(scene, node, text);

        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*#
     * Create a new box node
     * @name render.new_box_node
     * @param pos node position
     * @param ext node extent
     * @return new box node
     */
    static int LuaNewBoxNode(lua_State* L)
    {
        Vector3 pos = *dmScript::CheckVector3(L, 1);
        Vector3 ext = *dmScript::CheckVector3(L, 2);
        return LuaDoNewNode(L, Point3(pos), ext, NODE_TYPE_BOX, 0);
    }

    /*#
     * Create a new text node
     * @name render.new_text_node
     * @param pos node position
     * @param ext node extent
     * @param ext text node text
     * @return new text node
     */
    static int LuaNewTextNode(lua_State* L)
    {
        Vector3 pos = *dmScript::CheckVector3(L, 1);
        Vector3 ext = Vector3(1,1,1);
        const char* text = luaL_checkstring(L, 2);
        return LuaDoNewNode(L, Point3(pos), ext, NODE_TYPE_TEXT, text);
    }

    /*#
     * Get node text
     * @name gui.get_text
     * @param node node to get text for
     * @return text value
     */
    static int LuaGetText(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushstring(L, n->m_Node.m_Text);
        return 1;
    }

    /*#
     * Set node text
     * @name gui.set_text
     * @param node node to set text for
     * @param text text to set
     */
    static int LuaSetText(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        const char* text = luaL_checkstring(L, 2);
        if (n->m_Node.m_Text)
            free((void*) n->m_Node.m_Text);
        n->m_Node.m_Text = strdup(text);
        return 0;
    }

    /*#
     * Get node blend mode
     * @name gui.get_blend_mode
     * @return node blend mode
     */
    static int LuaGetBlendMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_BlendMode);
        return 1;
    }

    /*#
     * Set node blend mode
     * @name gui.set_blend_mode
     * @param node node to set blend mode for
     * @param blend_mode blend mode to set. Valid blend modes: BLEND_MODE_ALPHA, BLEND_MODE_ADD, BLEND_MODE_ADD_ALPHA and BLEND_MODE_MULT
     */
    static int LuaSetBlendMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int blend_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_BlendMode = (BlendMode) blend_mode;
        return 0;
    }

    /*#
     * Set node texture
     * @name gui.set_texture
     * @param node node to set texture for
     * @param texture texture name
     */
    static int LuaSetTexture(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;
        const char* texture_name = luaL_checkstring(L, 2);

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        Result r = SetNodeTexture(scene, hnode, texture_name);
        if (r != RESULT_OK)
        {
            luaL_error(L, "Texture %s is not specified in scene", texture_name);
        }
        return 0;
    }

    /*#
     * Set node font
     * @name gui.set_font
     * @param node node to set font for
     * @param font font name
     */
    static int LuaSetFont(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;
        const char* font_name = luaL_checkstring(L, 2);

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        Result r = SetNodeFont(scene, hnode, font_name);
        if (r != RESULT_OK)
        {
            luaL_error(L, "Font %s is not specified in scene", font_name);
        }
        return 0;
    }

    /*#
     * Set x-anchor for node
     * @name gui.set_xanchor
     * @param node node to set x-anchor for
     * @param anchor anchor value, eg gui.LEFT
     */
    static int LuaSetXAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int anchor = luaL_checknumber(L, 2);
        if (anchor != XANCHOR_LEFT && anchor != XANCHOR_RIGHT)
        {
            luaL_error(L, "Invalid x-anchor: %d", anchor);
        }

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        SetNodeXAnchor(scene, hnode, (XAnchor) anchor);

        return 0;
    }

    /*#
     * Set y-anchor for node
     * @name gui.set_yanchor
     * @param node node to set y-anchor for
     * @param anchor anchor value, eg gui.TOP
     */
    static int LuaSetYAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int anchor = luaL_checknumber(L, 2);
        if (anchor != YANCHOR_TOP && anchor != YANCHOR_BOTTOM)
        {
            luaL_error(L, "Invalid y-anchor: %d", anchor);
        }

        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        SetNodeYAnchor(scene, hnode, (YAnchor) anchor);

        return 0;
    }

    /*#
     * Retrieve scene width
     * @name gui.get_width
     */
    static int LuaGetWidth(lua_State* L)
    {
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*)lua_touserdata(L, -1);
        lua_pushnumber(L, scene->m_ReferenceWidth);
        return 1;
    }

    /*#
     * Retrieve scene height
     * @name gui.get_height
     */
    static int LuaGetHeight(lua_State* L)
    {
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*)lua_touserdata(L, -1);
        lua_pushnumber(L, scene->m_ReferenceHeight);
        return 1;
    }

    /*#
     * Get node position
     * @name gui.get_position
     * @param node node to get position for
     * @return node position
     */

    /*#
     * Set node position
     * @name gui.set_position
     * @param node node to set position for
     * @param position new position
     */

    /*#
     * Get node rotation
     * @name gui.get_rotation
     * @param node node to get rotation for
     * @return node rotation
     */

    /*#
     * Set node rotation
     * @name gui.set_rotation
     * @param node node to set rotation for
     * @param rotation new rotation
     */

    /*#
     * Get node scale
     * @name gui.get_scale
     * @param node node to get scale for
     * @return node scale
     */

    /*#
     * Set node scale
     * @name gui.set_scale
     * @param node node to set scale for
     * @param scale new scale
     */

    /*#
     * Get node color
     * @name gui.get_color
     * @param node node to get color for
     * @return node color
     */

    /*#
     * Set node color
     * @name gui.set_color
     * @param node node to set color for
     * @param color new color
     */

    /*#
     * Get node extents
     * @name gui.get_extents
     * @param node node to get extents for
     * @return node extents
     */

    /*#
     * Set node extents
     * @name gui.set_extents
     * @param node node to set extents for
     * @param extents new extents
     */

#define LUAGETSET(name, property) \
    int LuaGet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        dmScript::PushVector4(L, n->m_Node.m_Properties[property]);\
        return 1;\
    }\
\
    int LuaSet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        Vector4 pos;\
        if (dmScript::IsVector3(L, 2))\
            pos = Vector4(*dmScript::CheckVector3(L, 2));\
        else\
            pos = *dmScript::CheckVector4(L, 2);\
        n->m_Node.m_Properties[property] = pos;\
        return 0;\
    }\

    LUAGETSET(Position, PROPERTY_POSITION)
    LUAGETSET(Rotation, PROPERTY_ROTATION)
    LUAGETSET(Scale, PROPERTY_SCALE)
    LUAGETSET(Color, PROPERTY_COLOR)
    LUAGETSET(Extents, PROPERTY_EXTENTS)

#undef LUAGETSET

    void SetDefaultNewContextParams(NewContextParams* params)
    {
        memset(params, 0, sizeof(*params));
    }

#define REGGETSET(name, luaname) \
        {"get_"#luaname, LuaGet##name},\
        {"set_"#luaname, LuaSet##name},\

    static const luaL_reg Gui_methods[] =
    {
        {"get_node",        LuaGetNode},
        {"delete_node",     LuaDeleteNode},
        {"animate",         LuaAnimate},
        {"cancel_animation",LuaCancelAnimation},
        {"new_box_node",    LuaNewBoxNode},
        {"new_text_node",   LuaNewTextNode},
        {"get_text",        LuaGetText},
        {"set_text",        LuaSetText},
        {"get_blend_mode",  LuaGetBlendMode},
        {"set_blend_mode",  LuaSetBlendMode},
        {"set_texture",     LuaSetTexture},
        {"set_font",        LuaSetFont},
        {"set_xanchor",     LuaSetXAnchor},
        {"set_yanchor",     LuaSetYAnchor},
        {"get_width",       LuaGetWidth},
        {"get_height",      LuaGetHeight},
        REGGETSET(Position, position)
        REGGETSET(Rotation, rotation)
        REGGETSET(Scale, scale)
        REGGETSET(Color, color)
        REGGETSET(Extents, extents)
        {0, 0}
    };

#undef REGGETSET

    dmhash_t ScriptResolvePathCallback(lua_State* L, const char* path, uint32_t path_size)
    {
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        return scene->m_Context->m_ResolvePathCallback(scene, path, path_size);
    }

    void ScriptGetURLCallback(lua_State* L, dmMessage::URL* url)
    {
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        scene->m_Context->m_GetURLCallback(scene, url);
    }

    uintptr_t ScriptGetUserDataCallback(lua_State* L)
    {
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        return scene->m_Context->m_GetUserDataCallback(scene);
    }

    /*#
     * Position property
     * @name gui.POSITION
     * @variable
     */

    /*#
     * Rotation property
     * @name gui.ROTATION
     * @variable
     */

    /*#
     * Scale property
     * @name gui.SCALE
     * @variable
     */

    /*#
     * Color property
     * @name gui.COLOR
     * @variable
     */

    /*#
     * Extents property
     * @name gui.EXTENTS
     * @variable
     */

    lua_State* InitializeScript(dmScript::HContext script_context)
    {
        lua_State* L = lua_open();

        int top = lua_gettop(L);
        (void)top;

        dmScript::ScriptParams params;
        params.m_Context = script_context;
        params.m_GetURLCallback = ScriptGetURLCallback;
        params.m_GetUserDataCallback = ScriptGetUserDataCallback;
        params.m_ResolvePathCallback = ScriptResolvePathCallback;
        dmScript::Initialize(L, params);

        luaL_register(L, NODE_PROXY_TYPE_NAME, NodeProxy_methods);   // create methods table, add it to the globals
        lua_pop(L, 1);

        luaL_newmetatable(L, NODE_PROXY_TYPE_NAME);                         // create metatable for Image, add it to the Lua registry
        luaL_register(L, 0, NodeProxy_meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, -3);                       // dup methods table
        lua_rawset(L, -3);                          // hide metatable: metatable.__metatable = methods
        lua_pop(L, 1);                              // drop metatable

        luaL_register(L, LIB_NAME, Gui_methods);

#define SETPROP(name) \
        lua_pushnumber(L, (lua_Number) PROPERTY_##name); \
        lua_setfield(L, -2, #name);\

        SETPROP(POSITION)
        SETPROP(ROTATION)
        SETPROP(SCALE)
        SETPROP(COLOR)
        SETPROP(EXTENTS)

#undef SETPROP

#define SETEASING(name) \
        lua_pushnumber(L, (lua_Number) EASING_##name); \
        lua_setfield(L, -2, "EASING_"#name);\

        SETEASING(NONE)
        SETEASING(IN)
        SETEASING(OUT)
        SETEASING(INOUT)

#undef SETEASING

#define SETBLEND(name) \
        lua_pushnumber(L, (lua_Number) BLEND_MODE_##name); \
        lua_setfield(L, -2, "BLEND_MODE_"#name);\

        SETBLEND(ALPHA)
        SETBLEND(ADD)
        SETBLEND(ADD_ALPHA)
        SETBLEND(MULT)

#undef SETBLEND

        lua_pushnumber(L, (lua_Number) XANCHOR_LEFT);
        lua_setfield(L, -2, "LEFT");
        lua_pushnumber(L, (lua_Number) XANCHOR_RIGHT);
        lua_setfield(L, -2, "RIGHT");
        lua_pushnumber(L, (lua_Number) YANCHOR_TOP);
        lua_setfield(L, -2, "TOP");
        lua_pushnumber(L, (lua_Number) YANCHOR_BOTTOM);
        lua_setfield(L, -2, "BOTTOM");

        lua_pop(L, 1);

        assert(lua_gettop(L) == top);

        top = lua_gettop(L);
        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        //luaopen_debug(L);

        // Pop all stack values generated from luaopen_*
        lua_pop(L, lua_gettop(L));

        assert(lua_gettop(L) == top);

        return L;
    }

    void FinalizeScript(lua_State* L)
    {
        dmScript::Finalize(L);
        lua_close(L);
    }
}
