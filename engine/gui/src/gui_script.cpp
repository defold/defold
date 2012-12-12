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

    static Scene* GetScene(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;
        lua_getglobal(L, "__scene__");
        Scene* scene = (Scene*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return scene;
    }

    static bool LuaIsNode(lua_State *L, int ud)
    {
        int top = lua_gettop(L);
        (void) top;
        void *p = lua_touserdata(L, ud);
        if (p != NULL)
        {
            if (lua_getmetatable(L, ud))
            {
                lua_getfield(L, LUA_REGISTRYINDEX, NODE_PROXY_TYPE_NAME);
                if (lua_rawequal(L, -1, -2))
                {
                    lua_pop(L, 2);
                    assert(top == lua_gettop(L));
                    return true;
                }
            }
        }
        assert(top == lua_gettop(L));
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
        int top = lua_gettop(L);
        (void) top;
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
        assert(top + 1 == lua_gettop(L));
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

    /*# gets the node with the specified id
     *
     * @name gui.get_node
     * @param id id of the node to retrieve (string|hash)
     * @return node instance (node)
     */
    int LuaGetNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

        HNode node = 0;
        if (lua_isstring(L, 1))
        {
            const char* id = luaL_checkstring(L, 1);
            node = GetNodeById(scene, id);
            if (node == 0)
            {
                luaL_error(L, "No such node: %s", id);
            }
        }
        else
        {
            dmhash_t id = dmScript::CheckHash(L, 1);
            node = GetNodeById(scene, id);
            if (node == 0)
            {
                const char* id_string = (const char*)dmHashReverse64(id, 0x0);
                if (id_string != 0x0)
                    luaL_error(L, "No such node: %s", id_string);
                else
                    luaL_error(L, "No such node: %llu", id);
            }
        }

        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# gets the id of the specified node
     *
     * @name gui.get_id
     * @param node node to retrieve the id from (node)
     * @return id of the node (hash)
     */
    int LuaGetId(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);

        dmScript::PushHash(L, n->m_NameHash);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# gets the index of the specified node
     * The index defines the order in which a node appear in a gui scene.
     * Higher index means the node is drawn above lower indexed nodes.
     * @name gui.get_index
     * @param node node to retrieve the id from (node)
     * @return id of the node (hash)
     */
    int LuaGetIndex(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);

        lua_pushnumber(L, n->m_Index);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# deletes a node
     *
     * @name gui.delete_node
     * @param node node to delete (node)
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

        int top = lua_gettop(L);
        (void) top;

        lua_pushlightuserdata(L, (void*) scene);
        lua_setglobal(L, "__scene__");

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

        lua_pushlightuserdata(L, (void*) 0x0);
        lua_setglobal(L, "__scene__");

        assert(top == lua_gettop(L));
    }

    /*# linear interpolation
     *
     * @name gui.EASING_NONE
     * @variable
     */

    /*# in easing interpolation
     *
     * @name gui.EASING_IN
     * @variable
     */

    /*# out easing interpolation
     *
     * @name gui.EASING_OUT
     * @variable
     */

    /*# in and out easing interpolation
     *
     * @name gui.EASING_INOUT
     * @variable
     */

    /*# animates a node property
     * <p>
     * This starts an animation of a node property according to the specified parameters. If the node property is already being
     * animated, that animation will be canceled and replaced by the new one. Note however that several different node properties
     * can be animated simultaneously. Use <code>gui.cancel_animation</code> to stop the animation before it has completed.
     * </p>
     * <p>
     * If a <code>complete_function</code> (lua function) is specified, that function will be called when the animation has completed.
     * By starting a new animation in that function, several animations can be sequenced together. See the examples for more information.
     * </p>
     *
     * @name gui.animate
     * @param node node to animate (node)
     * @param property property to animate (constant)
     * <ul>
     *   <li><code>gui.PROP_POSITION</code></li>
     *   <li><code>gui.PROP_ROTATION</code></li>
     *   <li><code>gui.PROP_SCALE</code></li>
     *   <li><code>gui.PROP_COLOR</code></li>
     *   <li><code>gui.PROP_OUTLINE</code></li>
     *   <li><code>gui.PROP_SHADOW</code></li>
     *   <li><code>gui.PROP_SIZE</code></li>
     * </ul>
     * @param to target property value (vector3|vector4)
     * @param easing easing to use during animation (constant)
     * <ul>
     *   <li><code>gui.EASING_NONE</code></li>
     *   <li><code>gui.EASING_IN</code></li>
     *   <li><code>gui.EASING_OUT</code></li>
     *   <li><code>gui.EASING_INOUT</code></li>
     * </ul>
     * @param duration duration of the animation (number)
     * @param [delay] delay before the animation starts (number)
     * @param [complete_function] function to call when the animation has completed (function)
     * @examples
     * <p>
     * How to start a simple color animation, where the node fades in to white during 0.5 seconds:
     * <pre>
     * gui.set_color(node, vmath.vector4(0, 0, 0, 0)) -- node is fully transparent
     * gui.animate(node, gui.COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUT, 0.5) -- start animation
     * </pre>
     * </p>
     * <p>
     * How to start a sequenced animation where the node fades in to white during 0.5 seconds, stays visible for 2 seconds and then fades out:
     * <pre>
     * local function on_animation_done(self, node)
     *     -- fade out node, but wait 2 seconds before the animation starts
     *     gui.animate(node, gui.COLOR, vmath.vector4(0, 0, 0, 0), gui.EASING_OUT, 0.5, 2.0)
     * end
     *
     * function init(self)
     *     -- fetch the node we want to animate
     *     local my_node = gui.get_node("my_node")
     *     -- node is initially set to fully transparent
     *     gui.set_color(my_node, vmath.vector4(0, 0, 0, 0))
     *     -- animate the node immediately and call on_animation_done when the animation has completed
     *     gui.animate(my_node, gui.COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUT, 0.5, 0.0, on_animation_done)
     * end
     */
    int LuaAnimate(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* node = LuaCheckNode(L, 1, &hnode);
        (void) node;

        int property = (int) luaL_checknumber(L, 2);
        Vector4 to;
        if (dmScript::IsVector3(L, 3))
        {
            Vector4 original = dmGui::GetNodeProperty(scene, hnode, (dmGui::Property)property);
            to = Vector4(*dmScript::CheckVector3(L, 3), original.getW());
        }
        else
        {
            to = *dmScript::CheckVector4(L, 3);
        }
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
        } else if (!lua_isnone(L, 6)) {
            // If argument 6 is specified is has to be a number
            luaL_typerror(L, 6, "number");
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

    /*# cancels an ongoing animation
     * If an animation of the specified node is currently running (started by <code>gui.animate</code>), it will immediately be canceled.
     *
     * @name gui.cancel_animation
     * @param node node that should have its animation canceled (node)
     * @param property property for which the animation should be canceled (constant)
     * <ul>
     *   <li><code>gui.PROP_POSITION</code></li>
     *   <li><code>gui.PROP_ROTATION</code></li>
     *   <li><code>gui.PROP_SCALE</code></li>
     *   <li><code>gui.PROP_COLOR</code></li>
     *   <li><code>gui.PROP_OUTLINE</code></li>
     *   <li><code>gui.PROP_SHADOW</code></li>
     *   <li><code>gui.PROP_SIZE</code></li>
     * </ul>
     */
    int LuaCancelAnimation(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

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

    static int LuaDoNewNode(lua_State* L, Point3 pos, Vector3 size, NodeType node_type, const char* text)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

        HNode node = NewNode(scene, pos, size, node_type);
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

    /*# creates a new box node
     *
     * @name gui.new_box_node
     * @param pos node position (vector3|vector4)
     * @param size node size (vector3)
     * @return new box node (node)
     */
    static int LuaNewBoxNode(lua_State* L)
    {
        Vector3 pos;
        if (dmScript::IsVector4(L, 1))
        {
            Vector4* p4 = dmScript::CheckVector4(L, 1);
            pos = Vector3(p4->getX(), p4->getY(), p4->getZ());
        }
        else
        {
            pos = *dmScript::CheckVector3(L, 1);
        }
        Vector3 size = *dmScript::CheckVector3(L, 2);
        return LuaDoNewNode(L, Point3(pos), size, NODE_TYPE_BOX, 0);
    }

    /*# creates a new text node
     *
     * @name gui.new_text_node
     * @param pos node position (vector3|vector4)
     * @param text node text (string)
     * @return new text node (node)
     */
    static int LuaNewTextNode(lua_State* L)
    {
        Vector3 pos;
        if (dmScript::IsVector4(L, 1))
        {
            Vector4* p4 = dmScript::CheckVector4(L, 1);
            pos = Vector3(p4->getX(), p4->getY(), p4->getZ());
        }
        else
        {
            pos = *dmScript::CheckVector3(L, 1);
        }
        Vector3 size = Vector3(1,1,1);
        const char* text = luaL_checkstring(L, 2);
        return LuaDoNewNode(L, Point3(pos), size, NODE_TYPE_TEXT, text);
    }

    /*# gets the node text
     * This is only useful for text nodes.
     *
     * @name gui.get_text
     * @param node node from which to get the text (node)
     * @return text value (string)
     */
    static int LuaGetText(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushstring(L, n->m_Node.m_Text);
        return 1;
    }

    /*# sets the node text
     * This is only useful for text nodes.
     *
     * @name gui.set_text
     * @param node node to set text for (node)
     * @param text text to set (string)
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

    /*# gets the node blend mode
     * Blend mode defines how the node will be blended with the background.
     *
     * @name gui.get_blend_mode
     * @param node node from which to get the blend mode (node)
     * @return node blend mode (constant)
     * <ul>
     *   <li><code>gui.BLEND_ALPHA</code></li>
     *   <li><code>gui.BLEND_ADD</code></li>
     *   <li><code>gui.BLEND_ADD_ALPHA</code></li>
     *   <li><code>gui.BLEND_MULT</code></li>
     * </ul>
     */
    static int LuaGetBlendMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_BlendMode);
        return 1;
    }

    /*# sets node blend mode
     * Blend mode defines how the node will be blended with the background.
     *
     * @name gui.set_blend_mode
     * @param node node to set blend mode for (node)
     * @param blend_mode blend mode to set (constant)
     * <ul>
     *   <li><code>gui.BLEND_ALPHA</code></li>
     *   <li><code>gui.BLEND_ADD</code></li>
     *   <li><code>gui.BLEND_ADD_ALPHA</code></li>
     *   <li><code>gui.BLEND_MULT</code></li>
     * </ul>
     */
    static int LuaSetBlendMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int blend_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_BlendMode = (BlendMode) blend_mode;
        return 0;
    }

    /*# gets the node texture
     * This is currently only useful for box nodes. The texture must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_texture
     * @param node node to get texture from (node)
     * @return texture id (hash)
     */
    static int LuaGetTexture(lua_State* L)
    {
        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeTextureId(scene, hnode));
        return 1;
    }

    /*# sets the node texture
     * This is currently only useful for box nodes. The texture must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_texture
     * @param node node to set texture for (node)
     * @param texture texture id (string|hash)
     */
    static int LuaSetTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;
        if (lua_isstring(L, 2))
        {
            const char* texture_id = luaL_checkstring(L, 2);

            Result r = SetNodeTexture(scene, hnode, texture_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Texture %s is not specified in scene", texture_id);
            }
        }
        else
        {
            dmhash_t texture_id = dmScript::CheckHash(L, 2);

            Result r = SetNodeTexture(scene, hnode, texture_id);
            if (r != RESULT_OK)
            {
                const char* id_string = (const char*)dmHashReverse64(texture_id, 0x0);
                if (id_string != 0x0)
                    luaL_error(L, "Texture %s is not specified in scene", id_string);
                else
                    luaL_error(L, "Texture %llu is not specified in scene", texture_id);
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the node font
     * This is only useful for text nodes. The font must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_font
     * @param node node from which to get the font (node)
     * @return font id (hash)
     */
    static int LuaGetFont(lua_State* L)
    {
        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeFontId(scene, hnode));
        return 1;
    }

    /*# sets the node font
     * This is only useful for text nodes. The font must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_font
     * @param node node for which to set the font (node)
     * @param font font id (string|hash)
     */
    static int LuaSetFont(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        if (lua_isstring(L, 2))
        {
            const char* font_id = luaL_checkstring(L, 2);

            Result r = SetNodeFont(scene, hnode, font_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Font %s is not specified in scene", font_id);
            }
        }
        else
        {
            dmhash_t font_id = dmScript::CheckHash(L, 2);
            Result r = SetNodeFont(scene, hnode, font_id);
            if (r != RESULT_OK)
            {
                const char* id_string = (const char*)dmHashReverse64(font_id, 0x0);
                if (id_string != 0x0)
                    luaL_error(L, "Font %s is not specified in scene", id_string);
                else
                    luaL_error(L, "Font %llu is not specified in scene", font_id);
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the x-anchor of a node
     * The x-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.get_xanchor
     * @param node node to get x-anchor from (node)
     * @return anchor anchor constant (constant)
     * <ul>
     *   <li><code>gui.ANCHOR_NONE</code></li>
     *   <li><code>gui.ANCHOR_LEFT</code></li>
     *   <li><code>gui.ANCHOR_RIGHT</code></li>
     * </ul>
     */
    static int LuaGetXAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        Scene* scene = GetScene(L);

        lua_pushnumber(L, GetNodeXAnchor(scene, hnode));

        return 1;
    }

    /*# sets the x-anchor of a node
     * The x-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.set_xanchor
     * @param node node to set x-anchor for (node)
     * @param anchor anchor constant (constant)
     * <ul>
     *   <li><code>gui.ANCHOR_NONE</code></li>
     *   <li><code>gui.ANCHOR_LEFT</code></li>
     *   <li><code>gui.ANCHOR_RIGHT</code></li>
     * </ul>
     */
    static int LuaSetXAnchor(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int anchor = luaL_checknumber(L, 2);
        if (anchor != XANCHOR_NONE && anchor != XANCHOR_LEFT && anchor != XANCHOR_RIGHT)
        {
            luaL_error(L, "Invalid x-anchor: %d", anchor);
        }

        Scene* scene = GetScene(L);

        SetNodeXAnchor(scene, hnode, (XAnchor) anchor);

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the y-anchor of a node
     * The y-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.get_yanchor
     * @param node node to get y-anchor from (node)
     * @return anchor anchor constant (constant)
     * <ul>
     *   <li><code>gui.ANCHOR_NONE</code></li>
     *   <li><code>gui.ANCHOR_TOP</code></li>
     *   <li><code>gui.ANCHOR_BOTTOM</code></li>
     * </ul>
     */
    static int LuaGetYAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        Scene* scene = GetScene(L);

        lua_pushnumber(L, GetNodeYAnchor(scene, hnode));

        return 1;
    }

    /*# sets the y-anchor of a node
     * The y-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.set_yanchor
     * @param node node to set y-anchor for (node)
     * @param anchor anchor constant (constant)
     * <ul>
     *   <li><code>gui.ANCHOR_NONE</code></li>
     *   <li><code>gui.ANCHOR_TOP</code></li>
     *   <li><code>gui.ANCHOR_BOTTOM</code></li>
     * </ul>
     */
    static int LuaSetYAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int anchor = luaL_checknumber(L, 2);
        if (anchor != YANCHOR_NONE && anchor != YANCHOR_TOP && anchor != YANCHOR_BOTTOM)
        {
            luaL_error(L, "Invalid y-anchor: %d", anchor);
        }

        Scene* scene = GetScene(L);

        SetNodeYAnchor(scene, hnode, (YAnchor) anchor);

        return 0;
    }

    /*# gets the pivot of a node
     * The pivot specifies how the node is drawn and rotated from its position.
     *
     * @name gui.get_pivot
     * @param node node to get pivot from (node)
     * @return pivot constant (constant)
     * <ul>
     *   <li><code>gui.PIVOT_CENTER</code></lid>
     *   <li><code>gui.PIVOT_N</code></lid>
     *   <li><code>gui.PIVOT_NE</code></lid>
     *   <li><code>gui.PIVOT_E</code></lid>
     *   <li><code>gui.PIVOT_SE</code></lid>
     *   <li><code>gui.PIVOT_S</code></lid>
     *   <li><code>gui.PIVOT_SW</code></lid>
     *   <li><code>gui.PIVOT_W</code></lid>
     *   <li><code>gui.PIVOT_NW</code></lid>
     * </ul>
     */
    static int LuaGetPivot(lua_State* L)
    {
        Scene* scene = GetScene(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodePivot(scene, hnode));

        return 1;
    }

    /*# sets the pivot of a node
     * The pivot specifies how the node is drawn and rotated from its position.
     *
     * @name gui.set_pivot
     * @param node node to set pivot for (node)
     * @param pivot pivot constant (constant)
     * <ul>
     *   <li><code>gui.PIVOT_CENTER</code></lid>
     *   <li><code>gui.PIVOT_N</code></lid>
     *   <li><code>gui.PIVOT_NE</code></lid>
     *   <li><code>gui.PIVOT_E</code></lid>
     *   <li><code>gui.PIVOT_SE</code></lid>
     *   <li><code>gui.PIVOT_S</code></lid>
     *   <li><code>gui.PIVOT_SW</code></lid>
     *   <li><code>gui.PIVOT_W</code></lid>
     *   <li><code>gui.PIVOT_NW</code></lid>
     * </ul>
     */
    static int LuaSetPivot(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int pivot = luaL_checknumber(L, 2);
        if (pivot < PIVOT_CENTER || pivot > PIVOT_NW)
        {
            luaL_error(L, "Invalid pivot: %d", pivot);
        }

        Scene* scene = GetScene(L);

        SetNodePivot(scene, hnode, (Pivot) pivot);

        return 0;
    }

    /*# gets the scene width
     *
     * @name gui.get_width
     * @return scene width (number)
     */
    static int LuaGetWidth(lua_State* L)
    {
        Scene* scene = GetScene(L);

        lua_pushnumber(L, scene->m_Context->m_Width);
        return 1;
    }

    /*# gets the scene height
     *
     * @name gui.get_height
     * @return scene height (number)
     */
    static int LuaGetHeight(lua_State* L)
    {
        Scene* scene = GetScene(L);

        lua_pushnumber(L, scene->m_Context->m_Height);
        return 1;
    }

    /*# determines if the node is pickable by the supplied coordinates
     *
     * @name gui.pick_node
     * @param node node to be tested for picking (node)
     * @param x x-coordinate in screen-space
     * @param y y-coordinate in screen-space
     * @return pick result (boolean)
     */
    static int LuaPickNode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        Scene* scene = GetScene(L);

        lua_pushboolean(L, PickNode(scene, hnode, x, y));
        return 1;
    }

    /*# retrieves if a node is enabled or not
     *
     * Disabled nodes are not rendered and animations acting on them are not evaluated.
     *
     * @name gui.is_enabled
     * @param node node to query (node)
     * @return whether the node is enabled or not (boolean)
     */
    static int LuaIsEnabled(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        Scene* scene = GetScene(L);

        lua_pushboolean(L, dmGui::IsNodeEnabled(scene, hnode));

        return 1;
    }

    /*# enables/disables a node
     *
     * Disabled nodes are not rendered and animations acting on them are not evaluated.
     *
     * @name gui.set_enabled
     * @param node node to be enabled/disabled (node)
     * @param enabled whether the node should be enabled or not (boolean)
     */
    static int LuaSetEnabled(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int enabled = lua_toboolean(L, 2);

        Scene* scene = GetScene(L);

        dmGui::SetNodeEnabled(scene, hnode, enabled != 0);

        return 0;
    }

    /*# gets the node adjust mode
     * Adjust mode defines how the node will adjust itself to a screen resolution which differs from the project settings.
     *
     * @name gui.get_adjust_mode
     * @param node node from which to get the adjust mode (node)
     * @return node adjust mode (constant)
     * <ul>
     *   <li><code>gui.ADJUST_FIT</code></li>
     *   <li><code>gui.ADJUST_ZOOM</code></li>
     *   <li><code>gui.ADJUST_STRETCH</code></li>
     * </ul>
     */
    static int LuaGetAdjustMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_AdjustMode);
        return 1;
    }

    /*# sets node adjust mode
     * Adjust mode defines how the node will adjust itself to a screen resolution which differs from the project settings.
     *
     * @name gui.set_adjust_mode
     * @param node node to set adjust mode for (node)
     * @param adjust_mode adjust mode to set (constant)
     * <ul>
     *   <li><code>gui.ADJUST_FIT</code></li>
     *   <li><code>gui.ADJUST_ZOOM</code></li>
     *   <li><code>gui.ADJUST_STRETCH</code></li>
     * </ul>
     */
    static int LuaSetAdjustMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int adjust_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_AdjustMode = (AdjustMode) adjust_mode;
        return 0;
    }

    /*# gets the node position
     *
     * @name gui.get_position
     * @param node node to get the position from (node)
     * @return node position (vector3)
     */

    /*# sets the node position
     *
     * @name gui.set_position
     * @param node node to set the position for (node)
     * @param position new position (vector3|vector4)
     */

    /*# gets the node rotation
     *
     * @name gui.get_rotation
     * @param node node to get the rotation from (node)
     * @return node rotation (vector3)
     */

    /*# sets the node rotation
     *
     * @name gui.set_rotation
     * @param node node to set the rotation for (node)
     * @param rotation new rotation (vector3|vector4)
     */

    /*# gets the node scale
     *
     * @name gui.get_scale
     * @param node node to get the scale from (node)
     * @return node scale (vector3)
     */

    /*# sets the node scale
     *
     * @name gui.set_scale
     * @param node node to set the scale for (node)
     * @param scale new scale (vector3|vector4)
     */

    /*# gets the node color
     *
     * @name gui.get_color
     * @param node node to get the color from (node)
     * @return node color (vector4)
     */

    /*# sets the node color
     *
     * @name gui.set_color
     * @param node node to set the color for (node)
     * @param color new color (vector3|vector4)
     */

    /*# gets the node outline color
     *
     * @name gui.get_outline
     * @param node node to get the outline color from (node)
     * @return node outline color (vector4)
     */

    /*# sets the node outline color
     *
     * @name gui.set_outline
     * @param node node to set the outline color for (node)
     * @param color new outline color (vector3|vector4)
     */

    /*# gets the node shadow color
     *
     * @name gui.get_shadow
     * @param node node to get the shadow color from (node)
     * @return node shadow color (vector4)
     */

    /*# sets the node shadow color
     *
     * @name gui.set_shadow
     * @param node node to set the shadow color for (node)
     * @param color new shadow color (vector3|vector4)
     */

    /*# gets the node size
     *
     * @name gui.get_size
     * @param node node to get the size from (node)
     * @return node size (vector3)
     */

    /*# sets the node size
     *
     * @name gui.set_size
     * @param node node to set the size for (node)
     * @param size new size (vector3|vector4)
     */

#define LUASET(name, property) \
        int LuaSet##name(lua_State* L)\
        {\
            HNode hnode;\
            InternalNode* n = LuaCheckNode(L, 1, &hnode);\
            Vector4 v;\
            if (dmScript::IsVector3(L, 2))\
            {\
                Scene* scene = GetScene(L);\
                Vector4 original = dmGui::GetNodeProperty(scene, hnode, property);\
                v = Vector4(*dmScript::CheckVector3(L, 2), original.getW());\
            }\
            else\
                v = *dmScript::CheckVector4(L, 2);\
            n->m_Node.m_Properties[property] = v;\
            return 0;\
        }\

#define LUAGETSETV3(name, property) \
    int LuaGet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        const Vector4& v = n->m_Node.m_Properties[property];\
        dmScript::PushVector3(L, Vector3(v.getX(), v.getY(), v.getZ()));\
        return 1;\
    }\
    LUASET(name, property)\

#define LUAGETSETV4(name, property) \
    int LuaGet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNode(L, 1, 0);\
        dmScript::PushVector4(L, n->m_Node.m_Properties[property]);\
        return 1;\
    }\
    LUASET(name, property)\

    LUAGETSETV3(Position, PROPERTY_POSITION)
    LUAGETSETV3(Rotation, PROPERTY_ROTATION)
    LUAGETSETV3(Scale, PROPERTY_SCALE)
    LUAGETSETV4(Color, PROPERTY_COLOR)
    LUAGETSETV4(Outline, PROPERTY_OUTLINE)
    LUAGETSETV4(Shadow, PROPERTY_SHADOW)
    LUAGETSETV3(Size, PROPERTY_SIZE)

#undef LUAGETSET

    void SetDefaultNewContextParams(NewContextParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_Width = 640;
        params->m_Height = 960;
        params->m_PhysicalWidth = 640;
        params->m_PhysicalHeight = 960;
    }

#define REGGETSET(name, luaname) \
        {"get_"#luaname, LuaGet##name},\
        {"set_"#luaname, LuaSet##name},\

    static const luaL_reg Gui_methods[] =
    {
        {"get_node",        LuaGetNode},
        {"get_id",          LuaGetId},
        {"get_index",       LuaGetIndex},
        {"delete_node",     LuaDeleteNode},
        {"animate",         LuaAnimate},
        {"cancel_animation",LuaCancelAnimation},
        {"new_box_node",    LuaNewBoxNode},
        {"new_text_node",   LuaNewTextNode},
        {"get_text",        LuaGetText},
        {"set_text",        LuaSetText},
        {"get_blend_mode",  LuaGetBlendMode},
        {"set_blend_mode",  LuaSetBlendMode},
        {"get_texture",     LuaGetTexture},
        {"set_texture",     LuaSetTexture},
        {"get_font",        LuaGetFont},
        {"set_font",        LuaSetFont},
        {"get_xanchor",     LuaGetXAnchor},
        {"set_xanchor",     LuaSetXAnchor},
        {"get_yanchor",     LuaGetYAnchor},
        {"set_yanchor",     LuaSetYAnchor},
        {"get_pivot",       LuaGetPivot},
        {"set_pivot",       LuaSetPivot},
        {"get_width",       LuaGetWidth},
        {"get_height",      LuaGetHeight},
        {"pick_node",       LuaPickNode},
        {"is_enabled",      LuaIsEnabled},
        {"set_enabled",     LuaSetEnabled},
        {"get_adjust_mode", LuaGetAdjustMode},
        {"set_adjust_mode", LuaSetAdjustMode},
        REGGETSET(Position, position)
        REGGETSET(Rotation, rotation)
        REGGETSET(Scale, scale)
        REGGETSET(Color, color)
        REGGETSET(Outline, outline)
        REGGETSET(Shadow, shadow)
        REGGETSET(Size, size)
        {0, 0}
    };

#undef REGGETSET

    dmhash_t ScriptResolvePathCallback(lua_State* L, const char* path, uint32_t path_size)
    {
        Scene* scene = GetScene(L);

        return scene->m_Context->m_ResolvePathCallback(scene, path, path_size);
    }

    void ScriptGetURLCallback(lua_State* L, dmMessage::URL* url)
    {
        Scene* scene = GetScene(L);
        if (scene == 0x0)
        {
            luaL_error(L, "You can only create URLs inside inside the callback functions (init, update, etc).");
        }

        scene->m_Context->m_GetURLCallback(scene, url);
    }

    uintptr_t ScriptGetUserDataCallback(lua_State* L)
    {
        Scene* scene = GetScene(L);

        return scene->m_Context->m_GetUserDataCallback(scene);
    }

    /*# position property
     *
     * @name gui.PROP_POSITION
     * @variable
     */

    /*# rotation property
     *
     * @name gui.PROP_ROTATION
     * @variable
     */

    /*# scale property
     *
     * @name gui.PROP_SCALE
     * @variable
     */

    /*# color property
     *
     * @name gui.PROP_COLOR
     * @variable
     */

    /*# outline color property
     *
     * @name gui.PROP_OUTLINE
     * @variable
     */

    /*# shadow color property
     *
     * @name gui.PROP_SHADOW
     * @variable
     */

    /*# size property
     *
     * @name gui.PROP_SIZE
     * @variable
     */

    /*# alpha blending
     *
     * @name gui.BLEND_ALPHA
     * @variable
     */

    /*# additive blending
     *
     * @name gui.BLEND_ADD
     * @variable
     */

    /*# additive alpha blending
     *
     * @name gui.BLEND_ADD_ALPHA
     * @variable
     */

    /*# multiply blending
     *
     * @name gui.BLEND_MULT
     * @variable
     */

    /*# left x-anchor
     *
     * @name gui.ANCHOR_LEFT
     * @variable
     */

    /*# right x-anchor
     *
     * @name gui.ANCHOR_RIGHT
     * @variable
     */

    /*# top y-anchor
     *
     * @name gui.ANCHOR_TOP
     * @variable
     */

    /*# bottom y-anchor
     *
     * @name gui.ANCHOR_BOTTOM
     * @variable
     */

    /*# center pivor
     *
     * @name gui.PIVOT_CENTER
     * @variable
     */
    /*# north pivot
     *
     * @name gui.PIVOT_N
     * @variable
     */
    /*# north-east pivot
     *
     * @name gui.PIVOT_NE
     * @variable
     */
    /*# east pivot
     *
     * @name gui.PIVOT_E
     * @variable
     */
    /*# south-east pivot
     *
     * @name gui.PIVOT_SE
     * @variable
     */
    /*# south pivot
     *
     * @name gui.PIVOT_S
     * @variable
     */
    /*# south-west pivot
     *
     * @name gui.PIVOT_SW
     * @variable
     */
    /*# west pivot
     *
     * @name gui.PIVOT_W
     * @variable
     */
    /*# north-west pivot
     *
     * @name gui.PIVOT_NW
     * @variable
     */

    /*# fit adjust mode
     * Adjust mode is used when the screen resolution differs from the project settings.
     * The fit mode ensures that the entire node is visible in the adjusted gui scene.
     * @name gui.ADJUST_FIT
     * @variable
     */

    /*# zoom adjust mode
     * Adjust mode is used when the screen resolution differs from the project settings.
     * The zoom mode ensures that the node fills its entire area and might make the node exceed it.
     * @name gui.ADJUST_ZOOM
     * @variable
     */

    /*# stretch adjust mode
     * Adjust mode is used when the screen resolution differs from the project settings.
     * The stretch mode ensures that the node is displayed as is in the adjusted gui scene, which might scale it non-uniformally.
     * @name gui.ADJUST_STRETCH
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
        lua_setfield(L, -2, "PROP_"#name);\

        SETPROP(POSITION)
        SETPROP(ROTATION)
        SETPROP(SCALE)
        SETPROP(COLOR)
        SETPROP(OUTLINE)
        SETPROP(SHADOW)
        SETPROP(SIZE)

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
        lua_setfield(L, -2, "BLEND_"#name);\

        SETBLEND(ALPHA)
        SETBLEND(ADD)
        SETBLEND(ADD_ALPHA)
        SETBLEND(MULT)

#undef SETBLEND

        // Assert that the assumption of 0 below holds
        assert(XANCHOR_NONE == 0);
        assert(YANCHOR_NONE == 0);

        lua_pushnumber(L, 0);
        lua_setfield(L, -2, "ANCHOR_NONE");
        lua_pushnumber(L, (lua_Number) XANCHOR_LEFT);
        lua_setfield(L, -2, "ANCHOR_LEFT");
        lua_pushnumber(L, (lua_Number) XANCHOR_RIGHT);
        lua_setfield(L, -2, "ANCHOR_RIGHT");
        lua_pushnumber(L, (lua_Number) YANCHOR_TOP);
        lua_setfield(L, -2, "ANCHOR_TOP");
        lua_pushnumber(L, (lua_Number) YANCHOR_BOTTOM);
        lua_setfield(L, -2, "ANCHOR_BOTTOM");

#define SETPIVOT(name) \
        lua_pushnumber(L, (lua_Number) PIVOT_##name); \
        lua_setfield(L, -2, "PIVOT_"#name);\

        SETPIVOT(CENTER)
        SETPIVOT(N)
        SETPIVOT(NE)
        SETPIVOT(E)
        SETPIVOT(SE)
        SETPIVOT(S)
        SETPIVOT(SW)
        SETPIVOT(W)
        SETPIVOT(NW)

#undef SETPIVOT

#define SETADJUST(name) \
        lua_pushnumber(L, (lua_Number) ADJUST_MODE_##name); \
        lua_setfield(L, -2, "ADJUST_"#name);\

        SETADJUST(FIT)
        SETADJUST(ZOOM)
        SETADJUST(STRETCH)

#undef SETADJUST

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
        lua_close(L);
    }

    // Documentation for the scripts

    /*# called when a gui component is initialized
     * This is a callback-function, which is called by the engine when a gui component is initialized. It can be used
     * to set the initial state of the script and gui scene.
     *
     * @name init
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <pre>
     * function init(self)
     *     -- set up useful data
     *     self.my_value = 1
     * end
     * </pre>
     */

    /*# called when a gui component is finalized
     * This is a callback-function, which is called by the engine when a gui component is finalized (destroyed). It can
     * be used to e.g. take some last action, report the finalization to other game object instances
     * or release user input focus (see <code>release_input_focus</code>). There is no use in starting any animations or similar
     * from this function since the gui component is about to be destroyed.
     *
     * @name final
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <pre>
     * function final(self)
     *     -- report finalization
     *     msg.post("my_friend_instance", "im_dead", {my_stats = self.some_value})
     * end
     * </pre>
     */

    /*# called every frame to update the gui component
     * This is a callback-function, which is called by the engine every frame to update the state of a gui component.
     * It can be used to perform any kind of gui related tasks, e.g. animating nodes.
     *
     * @name update
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param dt the time-step of the frame update
     * @examples
     * <p>
     * This example demonstrates how to update a text node that displays game score in a counting fashion.
     * It is assumed that the gui component receives messages from the game when a new score is to be shown.
     * </p>
     * <pre>
     * function init(self)
     *     -- fetch the score text node for later use (assumes it is called "score")
     *     self.score_node = gui.get_node("score")
     *     -- keep track of the current score counted up so far
     *     self.current_score = 0
     *     -- keep track of the target score we should count up to
     *     self.current_score = 0
     *     -- how fast we will update the score, in score/second
     *     self.score_update_speed = 1
     * end
     *
     * function update(self, dt)
     *     -- check if target score is more than current score
     *     if self.current_score < self.target_score
     *         -- increment current score according to the speed
     *         self.current_score = self.current_score + dt * self.score_update_speed
     *         -- check if we went past the target score, clamp current score in that case
     *         if self.current_score > self.target_score then
     *             self.current_score = self.target_score
     *         end
     *         -- update the score text node
     *         gui.set_text(self.score_node, "" .. math.floor(self.current_score))
     *     end
     * end
     *
     * function on_message(self, message_id, message, sender)
     *     -- check the message
     *     if message_id == hash("set_score") then
     *         self.target_score = message.score
     *     end
     * end
     * </pre>
     */

    /*# called when a message has been sent to the gui component
     * <p>
     * This is a callback-function, which is called by the engine whenever a message has been sent to the gui component.
     * It can be used to take action on the message, e.g. update the gui or send a response back to the sender of the message.
     * </p>
     * <p>
     * The <code>message</code> parameter is a table containing the message data. If the message is sent from the engine, the
     * documentation of the message specifies which data is supplied.
     * </p>
     * <p>See the <code>update</code> function for examples on how to use this callback-function.</p>
     *
     * @name on_message
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param message_id id of the received message (hash)
     * @param message a table containing the message data (table)
     */

    /*# called when user input is received
     * <p>
     * This is a callback-function, which is called by the engine when user input is sent to the instance of the gui component.
     * It can be used to take action on the input, e.g. modify the gui according to the input.
     * </p>
     * <p>
     * For an instance to obtain user input, it must first acquire input focuse through the message <code>acquire_input_focus</code>.
     * See the documentation of that message for more information.
     * </p>
     * <p>
     * The <code>action</code> parameter is a table containing data about the input mapped to the <code>action_id</code>.
     * For mapped actions it specifies the value of the input and if it was just pressed or released.
     * Actions are mapped to input in an input_binding-file.
     * </p>
     * <p>
     * Mouse movement is specifically handled and uses <code>nil</code> as its <code>action_id</code>.
     * The <code>action</code> only contains positional parameters in this case, such as x and y of the pointer.
     * </p>
     * <p>
     * Here is a brief description of the available table fields:
     * </p>
     * <table>
     *   <th>Field</th>
     *   <th>Description</th>
     *   <tr><td><code>value</code></td><td>The amount of input given by the user. This is usually 1 for buttons and 0-1 for analogue inputs. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>pressed</code></td><td>If the input was pressed this frame, 0 for false and 1 for true. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>released</code></td><td>If the input was released this frame, 0 for false and 1 for true. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>repeated</code></td><td>If the input was repeated this frame, 0 for false and 1 for true. This is similar to how a key on a keyboard is repeated when you hold it down. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>x</code></td><td>The x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>y</code></td><td>The y value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dx</code></td><td>The change in x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dy</code></td><td>The change in y value of a pointer device, if present.</td></tr>
     * </table>
     *
     * @name on_input
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param action_id id of the received input action, as mapped in the input_binding-file (hash)
     * @param action a table containing the input data, see above for a description (table)
     * @examples
     * <pre>
     * function on_input(self, action_id, action)
     *     -- check for input
     *     if action_id == hash("my_action") then
     *         -- take appropritate action
     *         self.my_value = action.value
     *     end
     * end
     * </pre>
     */

    /*# called when the gui script is reloaded
     * <p>
     * This is a callback-function, which is called by the engine when the gui script is reloaded, e.g. from the editor.
     * It can be used for live development, e.g. to tweak constants or set up the state properly for the script.
     * </p>
     *
     * @name on_reload
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <pre>
     * function on_reload(self)
     *     -- restore some color (or similar)
     *     gui.set_color(gui.get_node("my_node"), self.my_original_color)
     * end
     * </pre>
     */
}
