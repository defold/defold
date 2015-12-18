#include "gui_script.h"

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/math.h>

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

    static int GuiScriptGetURL(lua_State* L)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(url);
        dmScript::PushURL(L, url);
        return 1;
    }

    static int GuiScriptResolvePath(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 2);
        dmScript::PushHash(L, dmHashString64(path));
        return 1;
    }

    static int GuiScriptIsValid(lua_State* L)
    {
        Script* script = (Script*)lua_touserdata(L, 1);
        lua_pushboolean(L, script != 0x0 && script->m_Context != 0x0);
        return 1;
    }

    static const luaL_reg GuiScript_methods[] =
    {
        {0,0}
    };

    static const luaL_reg GuiScript_meta[] =
    {
        {dmScript::META_TABLE_GET_URL,      GuiScriptGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH, GuiScriptResolvePath},
        {dmScript::META_TABLE_IS_VALID,     GuiScriptIsValid},
        {0, 0}
    };

    static Scene* GetScene(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;
        dmScript::GetInstance(L);
        Scene* scene = 0x0;
        if (dmScript::IsUserType(L, -1, GUI_SCRIPT_INSTANCE)) {
            scene = (Scene*)lua_touserdata(L, -1);
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return scene;
    }

    static Scene* GuiScriptInstance_Check(lua_State *L, int index)
    {
        return (Scene*)dmScript::CheckUserType(L, index, GUI_SCRIPT_INSTANCE);
    }

    static Scene* GuiScriptInstance_Check(lua_State *L)
    {
        dmScript::GetInstance(L);
        Scene* scene = GuiScriptInstance_Check(L, -1);
        lua_pop(L, 1);
        return scene;
    }

    static int GuiScriptInstance_gc (lua_State *L)
    {
        Scene* i = GuiScriptInstance_Check(L, 1);
        memset(i, 0, sizeof(*i));
        (void) i;
        assert(i);
        return 0;
    }

    static int GuiScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "GuiScript: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int GuiScriptInstance_index(lua_State *L)
    {
        Scene* i = GuiScriptInstance_Check(L, 1);
        assert(i);

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_DataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int GuiScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);

        Scene* i = GuiScriptInstance_Check(L, 1);
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_DataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static int GuiScriptInstanceGetURL(lua_State* L)
    {
        Scene* scene = (Scene*)lua_touserdata(L, 1);
        dmMessage::URL url;
        scene->m_Context->m_GetURLCallback(scene, &url);
        dmScript::PushURL(L, url);
        return 1;
    }

    static int GuiScriptInstanceResolvePath(lua_State* L)
    {
        Scene* scene = (Scene*)lua_touserdata(L, 1);
        const char* path = luaL_checkstring(L, 2);
        dmScript::PushHash(L, scene->m_Context->m_ResolvePathCallback(scene, path, strlen(path)));
        return 1;
    }

    static int GuiScriptInstanceIsValid(lua_State* L)
    {
        Scene* scene = (Scene*)lua_touserdata(L, 1);
        lua_pushboolean(L, scene != 0x0 && scene->m_Context != 0x0);
        return 1;
    }

    static const luaL_reg GuiScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg GuiScriptInstance_meta[] =
    {
        {"__gc",        GuiScriptInstance_gc},
        {"__tostring",  GuiScriptInstance_tostring},
        {"__index",     GuiScriptInstance_index},
        {"__newindex",  GuiScriptInstance_newindex},
        {dmScript::META_TABLE_GET_URL,      GuiScriptInstanceGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH, GuiScriptInstanceResolvePath},
        {dmScript::META_TABLE_IS_VALID,     GuiScriptInstanceIsValid},
        {0, 0}
    };

    static NodeProxy* NodeProxy_Check(lua_State *L, int index)
    {
        return (NodeProxy*)dmScript::CheckUserType(L, index, NODE_PROXY_TYPE_NAME);
    }

    static bool LuaIsNode(lua_State *L, int index)
    {
        return dmScript::IsUserType(L, index, NODE_PROXY_TYPE_NAME);
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
        if (np->m_Scene != GetScene(L))
            luaL_error(L, "Node used in the wrong scene");
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

        return 0; // Never reached
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

        Scene* scene = GuiScriptInstance_Check(L);

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

    /*# sets the id of the specified node
     *
     * @name gui.set_id
     * @param node node to set the id for (node)
     * @param id id to set (string|hash)
     */
    int LuaSetId(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);
        HNode hnode;
        LuaCheckNode(L, 1, &hnode);

        dmhash_t id = 0;
        if (lua_isstring(L, 2))
        {
            id = dmHashString64(lua_tostring(L, 2));
        }
        else
        {
            id = dmScript::CheckHash(L, 2);
        }
        dmGui::SetNodeId(scene, hnode, id);

        assert(top == lua_gettop(L));

        return 0;
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

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);

        uint32_t index = 0;
        uint16_t i = scene->m_RenderHead;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            i = parent->m_ChildHead;
        }
        while (i != INVALID_INDEX && i != n->m_Index)
        {
            ++index;
            i = scene->m_Nodes[i].m_NextIndex;
        }
        lua_pushnumber(L, index);

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

    void LuaCurveRelease(dmEasing::Curve* curve)
    {
        lua_State* L = (lua_State*)curve->userdata1;

        int top = lua_gettop(L);
        (void) top;

        int ref = (int) (((uintptr_t) curve->userdata2) & 0xffffffff);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);

        curve->release_callback = 0x0;
        curve->userdata1 = 0x0;
        curve->userdata2 = 0x0;

        assert(top == lua_gettop(L));
    }

    void LuaAnimationComplete(HScene scene, HNode node, void* userdata1, void* userdata2)
    {
        lua_State* L = scene->m_Context->m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
        dmScript::SetInstance(L);

        int ref = (int) ((uintptr_t) userdata1 & 0xffffffff);
        int node_ref = (int) ((uintptr_t) userdata2 & 0xffffffff);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_InstanceReference);
        lua_rawgeti(L, LUA_REGISTRYINDEX, node_ref);
        assert(lua_type(L, -3) == LUA_TFUNCTION);

        dmScript::PCall(L, 2, 0);

        lua_unref(L, ref);
        lua_unref(L, node_ref);

        lua_pushnil(L);
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));
    }

    /*# once forward
     *
     * @name gui.PLAYBACK_ONCE_FORWARD
     * @variable
     */
    /*# once backward
     *
     * @name gui.PLAYBACK_ONCE_BACKWARD
     * @variable
     */
    /*# once forward and then backward
     *
     * @name gui.PLAYBACK_ONCE_PINGPONG
     * @variable
     */
    /*# loop forward
     *
     * @name gui.PLAYBACK_LOOP_FORWARD
     * @variable
     */
    /*# loop backward
     *
     * @name gui.PLAYBACK_LOOP_BACKWARD
     * @variable
     */
    /*# ping pong loop
     *
     * @name gui.PLAYBACK_LOOP_PINGPONG
     * @variable
     */

    /*# linear interpolation
     *
     * @name gui.EASING_LINEAR
     * @variable
     */
    /*# in-quadratic
     *
     * @name gui.EASING_INQUAD
     * @variable
     */
    /*# out-quadratic
     *
     * @name gui.EASING_OUTQUAD
     * @variable
     */
    /*# in-out-quadratic
     *
     * @name gui.EASING_INOUTQUAD
     * @variable
     */
    /*# out-in-quadratic
     *
     * @name gui.EASING_OUTINQUAD
     * @variable
     */
    /*# in-cubic
     *
     * @name gui.EASING_INCUBIC
     * @variable
     */
    /*# out-cubic
     *
     * @name gui.EASING_OUTCUBIC
     * @variable
     */
    /*# in-out-cubic
     *
     * @name gui.EASING_INOUTCUBIC
     * @variable
     */
    /*# out-in-cubic
     *
     * @name gui.EASING_OUTINCUBIC
     * @variable
     */
    /*# in-quartic
     *
     * @name gui.EASING_INQUART
     * @variable
     */
    /*# out-quartic
     *
     * @name gui.EASING_OUTQUART
     * @variable
     */
    /*# in-out-quartic
     *
     * @name gui.EASING_INOUTQUART
     * @variable
     */
    /*# out-in-quartic
     *
     * @name gui.EASING_OUTINQUART
     * @variable
     */
    /*# in-quintic
     *
     * @name gui.EASING_INQUINT
     * @variable
     */
    /*# out-quintic
     *
     * @name gui.EASING_OUTQUINT
     * @variable
     */
    /*# in-out-quintic
     *
     * @name gui.EASING_INOUTQUINT
     * @variable
     */
    /*# out-in-quintic
     *
     * @name gui.EASING_OUTINQUINT
     * @variable
     */
    /*# in-sine
     *
     * @name gui.EASING_INSINE
     * @variable
     */
    /*# out-sine
     *
     * @name gui.EASING_OUTSINE
     * @variable
     */
    /*# in-out-sine
     *
     * @name gui.EASING_INOUTSINE
     * @variable
     */
    /*# out-in-sine
     *
     * @name gui.EASING_OUTINSINE
     * @variable
     */
    /*# in-exponential
     *
     * @name gui.EASING_INEXPO
     * @variable
     */
    /*# out-exponential
     *
     * @name gui.EASING_OUTEXPO
     * @variable
     */
    /*# in-out-exponential
     *
     * @name gui.EASING_INOUTEXPO
     * @variable
     */
    /*# out-in-exponential
     *
     * @name gui.EASING_OUTINEXPO
     * @variable
     */
    /*# in-circlic
     *
     * @name gui.EASING_INCIRC
     * @variable
     */
    /*# out-circlic
     *
     * @name gui.EASING_OUTCIRC
     * @variable
     */
    /*# in-out-circlic
     *
     * @name gui.EASING_INOUTCIRC
     * @variable
     */
    /*# out-in-circlic
     *
     * @name gui.EASING_OUTINCIRC
     * @variable
     */
    /*# in-elastic
     *
     * @name gui.EASING_INELASTIC
     * @variable
     */
    /*# out-elastic
     *
     * @name gui.EASING_OUTELASTIC
     * @variable
     */
    /*# in-out-elastic
     *
     * @name gui.EASING_INOUTELASTIC
     * @variable
     */
    /*# out-in-elastic
     *
     * @name gui.EASING_OUTINELASTIC
     * @variable
     */
    /*# in-back
     *
     * @name gui.EASING_INBACK
     * @variable
     */
    /*# out-back
     *
     * @name gui.EASING_OUTBACK
     * @variable
     */
    /*# in-out-back
     *
     * @name gui.EASING_INOUTBACK
     * @variable
     */
    /*# out-in-back
     *
     * @name gui.EASING_OUTINBACK
     * @variable
     */
    /*# in-bounce
     *
     * @name gui.EASING_INBOUNCE
     * @variable
     */
    /*# out-bounce
     *
     * @name gui.EASING_OUTBOUNCE
     * @variable
     */
    /*# in-out-bounce
     *
     * @name gui.EASING_INOUTBOUNCE
     * @variable
     */
    /*# out-in-bounce
     *
     * @name gui.EASING_OUTINBOUNCE
     * @variable
     */

    /*# animates a node property
     * <p>
     * This starts an animation of a node property according to the specified parameters. If the node property is already being
     * animated, that animation will be canceled and replaced by the new one. Note however that several different node properties
     * can be animated simultaneously. Use <code>gui.cancel_animation</code> to stop the animation before it has completed.
     * </p>
     * <p>
     * Composite properties of type vector3, vector4 or quaternion also expose their sub-components (x, y, z and w).
     * You can address the components individually by suffixing the name with a dot '.' and the name of the component.
     * For instance, "position.x" (the position x coordinate) or "color.w" (the color alpha value).
     * </p>
     * <p>
     * If a <code>complete_function</code> (Lua function) is specified, that function will be called when the animation has completed.
     * By starting a new animation in that function, several animations can be sequenced together. See the examples for more information.
     * </p>
     *
     * @name gui.animate
     * @param node node to animate (node)
     * @param property property to animate (string|constant)
     * <ul>
     *   <li><code>"position"</code></li>
     *   <li><code>"rotation"</code></li>
     *   <li><code>"scale"</code></li>
     *   <li><code>"color"</code></li>
     *   <li><code>"outline"</code></li>
     *   <li><code>"shadow"</code></li>
     *   <li><code>"size"</code></li>
     *   <li><code>"fill_angle"</code> (pie nodes)</li>
     *   <li><code>"inner_radius"</code> (pie nodes)</li>
     *   <li><code>"slice9"</code> (slice9 nodes)</li>
     * </ul>
     * The following property constants are also defined equalling the corresponding property string names.
     * <ul>
     *   <li><code>gui.PROP_POSITION</code></li>
     *   <li><code>gui.PROP_ROTATION</code></li>
     *   <li><code>gui.PROP_SCALE</code></li>
     *   <li><code>gui.PROP_COLOR</code></li>
     *   <li><code>gui.PROP_OUTLINE</code></li>
     *   <li><code>gui.PROP_SHADOW</code></li>
     *   <li><code>gui.PROP_SIZE</code></li>
     *   <li><code>gui.PROP_FILL_ANGLE</code></li>
     *   <li><code>gui.PROP_INNER_RADIUS</code></li>
     *   <li><code>gui.PROP_SLICE9</code></li>
     * </ul>
     * <p>
     *
     * </p>
     * @param to target property value (vector3|vector4)
     * @param easing easing to use during animation. Either specify one of the gui.EASING_* constants or provide a vmath.vector with a custom curve. (constant|vector)
     * @param duration duration of the animation (number)
     * @param [delay] delay before the animation starts (number)
     * @param [complete_function] function to call when the animation has completed (function)
     * @param [playback] playback mode (constant)
     * <ul>
     *   <li><code>gui.PLAYBACK_ONCE_FORWARD</code></li>
     *   <li><code>gui.PLAYBACK_ONCE_BACKWARD</code></li>
     *   <li><code>gui.PLAYBACK_ONCE_PINGPONG</code></li>
     *   <li><code>gui.PLAYBACK_LOOP_FORWARD</code></li>
     *   <li><code>gui.PLAYBACK_LOOP_BACKWARD</code></li>
     *   <li><code>gui.PLAYBACK_LOOP_PINGPONG</code></li>
     * </ul>
     * @examples
     * <p>
     * How to start a simple color animation, where the node fades in to white during 0.5 seconds:
     * <pre>
     * gui.set_color(node, vmath.vector4(0, 0, 0, 0)) -- node is fully transparent
     * gui.animate(node, gui.COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5) -- start animation
     * </pre>
     * </p>
     * <p>
     * How to start a sequenced animation where the node fades in to white during 0.5 seconds, stays visible for 2 seconds and then fades out:
     * </p>
     * <pre>
     * local function on_animation_done(self, node)
     *     -- fade out node, but wait 2 seconds before the animation starts
     *     gui.animate(node, gui.COLOR, vmath.vector4(0, 0, 0, 0), gui.EASING_OUTQUAD, 0.5, 2.0)
     * end
     *
     * function init(self)
     *     -- fetch the node we want to animate
     *     local my_node = gui.get_node("my_node")
     *     -- node is initially set to fully transparent
     *     gui.set_color(my_node, vmath.vector4(0, 0, 0, 0))
     *     -- animate the node immediately and call on_animation_done when the animation has completed
     *     gui.animate(my_node, gui.COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5, 0.0, on_animation_done)
     * end
     * </pre>
     * <p>How to animate a node's y position using a crazy custom easing curve:</p>
     * <pre>
     * function init(self)
     *     local values = { 0, 0, 0, 0, 0, 0, 0, 0,
     *                      1, 1, 1, 1, 1, 1, 1, 1,
     *                      0, 0, 0, 0, 0, 0, 0, 0,
     *                      1, 1, 1, 1, 1, 1, 1, 1,
     *                      0, 0, 0, 0, 0, 0, 0, 0,
     *                      1, 1, 1, 1, 1, 1, 1, 1,
     *                      0, 0, 0, 0, 0, 0, 0, 0,
     *                      1, 1, 1, 1, 1, 1, 1, 1 }
     *     local vec = vmath.vector(values)
     *     local node = gui.get_node("box")
     *     gui.animate(node, "position.y", 100, vec, 4.0, 0, nil, gui.PLAYBACK_LOOP_PINGPONG)
     * end
     * </pre>
     */
    int LuaAnimate(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* node = LuaCheckNode(L, 1, &hnode);
        (void) node;

        dmhash_t property_hash;
        if (dmScript::IsHash(L, 2)) {
           property_hash = dmScript::CheckHash(L, 2);
        } else {
           property_hash = dmHashString64(luaL_checkstring(L, 2));
        }

        if (!dmGui::HasPropertyHash(scene, hnode, property_hash)) {
            luaL_error(L, "property '%s' not found", (const char*) dmHashReverse64(property_hash, 0));
        }

        Vector4 to;
        if (lua_isnumber(L, 3))
        {
            to = Vector4((float) lua_tonumber(L, 3));
        }
        else if (dmScript::IsVector3(L, 3))
        {
            Vector4 original = dmGui::GetNodePropertyHash(scene, hnode, property_hash);
            to = Vector4(*dmScript::CheckVector3(L, 3), original.getW());
        }
        else
        {
            to = *dmScript::CheckVector4(L, 3);
        }

        dmEasing::Curve curve;
        if (lua_isnumber(L, 4))
        {
            curve.type = (dmEasing::Type)luaL_checkinteger(L, 4);
            if (curve.type >= dmEasing::TYPE_COUNT)
                return luaL_error(L, "invalid easing constant");
        }
        else if (dmScript::IsVector(L, 4))
        {
            curve.type = dmEasing::TYPE_FLOAT_VECTOR;
            curve.vector = dmScript::CheckVector(L, 4);

            lua_pushvalue(L, 4);
            curve.release_callback = LuaCurveRelease;
            curve.userdata1 = (void*)L;
            curve.userdata2 = (void*)luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            return luaL_error(L, "easing must be either a easing constant or a vmath.vector");
        }

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

        Playback playback = PLAYBACK_ONCE_FORWARD;
        if (lua_isnumber(L, 8)) {
            playback = (Playback) luaL_checkinteger(L, 8);
        }

        if (animation_complete_ref == LUA_NOREF) {
            AnimateNodeHash(scene, hnode, property_hash, to, curve, playback, (float) duration, delay, 0, 0, 0);
        } else {
            AnimateNodeHash(scene, hnode, property_hash, to, curve, playback, (float) duration, delay, &LuaAnimationComplete, (void*) animation_complete_ref, (void*) node_ref);
        }

        assert(top== lua_gettop(L));
        return 0;
    }

    /*# cancels an ongoing animation
     * If an animation of the specified node is currently running (started by <code>gui.animate</code>), it will immediately be canceled.
     *
     * @name gui.cancel_animation
     * @param node node that should have its animation canceled (node)
     * @param property property for which the animation should be canceled (string|constant)
     * <ul>
     *   <li><code>"position"</code></li>
     *   <li><code>"rotation"</code></li>
     *   <li><code>"scale"</code></li>
     *   <li><code>"color"</code></li>
     *   <li><code>"outline"</code></li>
     *   <li><code>"shadow"</code></li>
     *   <li><code>"size"</code></li>
     *   <li><code>"fill_angle"</code> (pie nodes)</li>
     *   <li><code>"inner_radius"</code> (pie nodes)</li>
     *   <li><code>"slice9"</code> (slice9 nodes)</li>
     * </ul>
     */
    int LuaCancelAnimation(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* node = LuaCheckNode(L, 1, &hnode);
        (void) node;

        dmhash_t property_hash;
        if (dmScript::IsHash(L, 2)) {
           property_hash = dmScript::CheckHash(L, 2);
        } else {
           property_hash = dmHashString64(luaL_checkstring(L, 2));
        }

        if (!dmGui::HasPropertyHash(scene, hnode, property_hash)) {
            luaL_error(L, "property '%s' not found", (const char*) dmHashReverse64(property_hash, 0));
        }

        CancelAnimationHash(scene, hnode, property_hash);

        assert(top== lua_gettop(L));
        return 0;
    }

    static void LuaPushNode(lua_State* L, dmGui::HScene scene, dmGui::HNode node)
    {
        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
        lua_setmetatable(L, -2);
    }

    static int LuaDoNewNode(lua_State* L, Scene* scene, Point3 pos, Vector3 size, NodeType node_type, const char* text, void* font)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode node = NewNode(scene, pos, size, node_type);
        if (!node)
        {
            luaL_error(L, "Out of nodes (max %d)", scene->m_Nodes.Capacity());
        }
        GetNode(scene, node)->m_Node.m_Font = font;
        SetNodeText(scene, node, text);

        LuaPushNode(L, scene, node);

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
        Scene* scene = GuiScriptInstance_Check(L);
        return LuaDoNewNode(L, scene, Point3(pos), size, NODE_TYPE_BOX, 0, 0x0);
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
        const char* text = luaL_checkstring(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        void* font = scene->m_DefaultFont;
        if (font == 0x0)
            font = scene->m_Context->m_DefaultFont;
        Vector3 size = Vector3(1,1,1);
        if (font != 0x0)
        {
            dmGui::TextMetrics metrics;
            scene->m_Context->m_GetTextMetricsCallback(font, text, 0.0f, false, &metrics);
            size.setX(metrics.m_Width);
            size.setY(metrics.m_MaxAscent + metrics.m_MaxDescent);
        }

        return LuaDoNewNode(L, scene, Point3(pos), size, NODE_TYPE_TEXT, text, font);
    }

    /*# creates a new pie node
     *
     * @name gui.new_pie_node
     * @param pos node position (vector3|vector4)
     * @param size node size (vector3)
     * @return new box node (node)
     */
    static int LuaNewPieNode(lua_State* L)
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
        Scene* scene = GuiScriptInstance_Check(L);
        return LuaDoNewNode(L, scene, Point3(pos), size, NODE_TYPE_PIE, 0, 0x0);
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

    /*# get line-break mode
     * This is only useful for text nodes.
     *
     * @name gui.get_line_break
     * @param node node from which to get the line-break for (node)
     * @return line-break (bool)
     */
    static int LuaGetLineBreak(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushboolean(L, n->m_Node.m_LineBreak);
        return 1;
    }

    /*# set line-break mode
     * This is only useful for text nodes.
     *
     * @name gui.set_line_break
     * @param node node to set line-break for (node)
     * @param text text to set (string)
     */
    static int LuaSetLineBreak(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        bool line_break = (bool) lua_toboolean(L, 2);
        n->m_Node.m_LineBreak = line_break;
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
     * This is currently only useful for box or pie nodes. The texture must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_texture
     * @param node node to get texture from (node)
     * @return texture id (hash)
     */
    static int LuaGetTexture(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeTextureId(scene, hnode));
        return 1;
    }

    /*# sets the node texture
     * Set the texture on a box or pie node. The texture must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_texture
     * @param node node to set texture for (node)
     * @param texture texture id (string|hash)
     */
    static int LuaSetTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

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

    /*# gets the node flipbook animation
     * Get node flipbook animation.
     *
     * @name gui.get_flipbook
     * @param node node to get flipbook animation from (node)
     * @return animation animation id (hash)
     */
    static int LuaGetFlipbook(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeFlipbookAnimId(scene, hnode));
        return 1;
    }

    /*# play node flipbook animation
     * Play flipbook animation on a box or pie node. The current node texture must contain the animation.
     *
     * @name gui.play_flipbook
     * @param node node to set animation for (node)
     * @param animation animation id (string|hash)
     * @param [complete_function] function to call when the animation has completed (function)
     */
    static int LuaPlayFlipbook(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        int node_ref = LUA_NOREF;
        int animation_complete_ref = LUA_NOREF;
        if (lua_isfunction(L, 3))
        {
            lua_pushvalue(L, 3);
            animation_complete_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            lua_pushvalue(L, 1);
            node_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            fflush(stdout);
        }

        if (lua_isstring(L, 2))
        {
            const char* anim_id = luaL_checkstring(L, 2);
            Result r;
            if(animation_complete_ref != LUA_NOREF)
                r = PlayNodeFlipbookAnim(scene, hnode, anim_id, &LuaAnimationComplete, (void*) animation_complete_ref, (void*) node_ref);
            else
                r = PlayNodeFlipbookAnim(scene, hnode, anim_id);
            if (r != RESULT_OK)
            {
                const char* node_id_string = (const char*)dmHashReverse64(n->m_NameHash, 0x0);
                if(node_id_string != 0x0)
                    luaL_error(L, "Animation %s invalid for node %s (no animation set)", anim_id, node_id_string);
                else
                    luaL_error(L, "Animation %s invalid for node %llu (no animation set)", anim_id, n->m_NameHash);
            }
        }
        else
        {
            dmhash_t anim_id = dmScript::CheckHash(L, 2);
            Result r;
            if(animation_complete_ref != LUA_NOREF)
                r = PlayNodeFlipbookAnim(scene, hnode, anim_id, &LuaAnimationComplete, (void*) animation_complete_ref, (void*) node_ref);
            else
                r = PlayNodeFlipbookAnim(scene, hnode, anim_id);
            if (r != RESULT_OK)
            {
                const char* node_id_string = (const char*)dmHashReverse64(anim_id, 0x0);
                const char* anim_id_string = (const char*)dmHashReverse64(n->m_NameHash, 0x0);
                if(node_id_string != 0x0 && anim_id_string != 0x0)
                    luaL_error(L, "Animation %s invalid for node %s (no animation set)", anim_id_string, node_id_string);
                else
                    luaL_error(L, "Animation %llu invalid for node %llu (no animation set)", anim_id, n->m_NameHash);
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# pause or unpause the node flipbook animation
     * Pause or unpause the node flipbook animation.
     *
     * @name gui.cancel_flipbook
     * @param node node cancel flipbook animation for (node)
     */
    static int LuaCancelFlipbook(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;
        Scene* scene = GuiScriptInstance_Check(L);
        CancelNodeFlipbookAnim(scene, hnode);
        return 0;
    }

    static dmImage::Type ToImageType(lua_State*L, const char* type_str)
    {
        if (strcmp(type_str, "rgb") == 0) {
            return dmImage::TYPE_RGB;
        } else if (strcmp(type_str, "rgba") == 0) {
            return dmImage::TYPE_RGBA;
        } else if (strcmp(type_str, "l") == 0) {
            return dmImage::TYPE_LUMINANCE;
        } else {
            luaL_error(L, "unsupported texture format '%s'", type_str);
        }

        // never reached
        return (dmImage::Type) 0;
    }

    /*# create new texture
     * Dynamically create a new texture.
     *
     * @name gui.new_texture
     * @param texture texture id (string|hash)
     * @param width texture width (number)
     * @param height texture height (number)
     * @param type texture type (string|constant)
     * <ul>
     *   <li><code>"rgb"</code> - RGB</li>
     *   <li><code>"rgba"</code> - RGBA</li>
     *   <li><code>"l"</code> - LUMINANCE</li>
     * </ul>
     * @param buffer texture data (string)
     * @return texture creation was successful (boolean)
     * @examples
     * <pre>
     * function init(self)
     *      local w = 200
     *      local h = 300
     *
     *      -- A nice orange. String with the RGB values.
     *      local orange = string.char(0xff) .. string.char(0x80) .. string.char(0x10)
     *
     *      -- Create the texture. Repeat the color string for each pixel.
     *      if gui.new_texture("orange_tx", w, h, "rgb", string.rep(orange, w * h)) then
     *          -- Create a box node and apply the texture to it.
     *          local n = gui.new_box_node(vmath.vector3(200, 200, 0), vmath.vector3(w, h, 0))
     *          gui.set_texture(n, "orange_tx")
     *      else
     *          -- Could not create texture...
     *          ...
     *      end
     * end
     * </pre>
     */
    static int LuaNewTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const char* name = luaL_checkstring(L, 1);
        int width = luaL_checkinteger(L, 2);
        int height = luaL_checkinteger(L, 3);
        const char* type_str = luaL_checkstring(L, 4);
        size_t buffer_size;
        luaL_checktype(L, 5, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 5, &buffer_size);
        Scene* scene = GuiScriptInstance_Check(L);

        dmImage::Type type = ToImageType(L, type_str);
        Result r = NewDynamicTexture(scene, name, width, height, type, buffer, buffer_size);
        if (r == RESULT_OK) {
            lua_pushboolean(L, 1);
        } else {
            dmLogWarning("Failed to create dynamic gui texture (%d)", r);
            lua_pushboolean(L, 0);
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# delete texture
     * Delete a dynamically created texture.
     *
     * @name gui.delete_texture
     * @param texture texture id (string|hash)
     * @examples
     * <pre>
     * function init(self)
     *      -- Create a texture.
     *      if gui.new_texture("temp_tx", 10, 10, "rgb", string.rep('\0', 10 * 10 * 3)) then
     *          -- Do something with the texture.
     *          ...
     *
     *          -- Delete the texture
     *          gui.delete_texture("temp_tx")
     *      end
     * end
     * </pre>
     */
    static int LuaDeleteTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const char* name = luaL_checkstring(L, 1);

        Scene* scene = GuiScriptInstance_Check(L);

        Result r = DeleteDynamicTexture(scene, name);
        if (r != RESULT_OK) {
            luaL_error(L, "failed to delete texture '%s' (%d)", name, r);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set the buffer data for a texture
     * Set the texture buffer data for a dynamically created texture.
     *
     * @name gui.set_texture_data
     * @param texture texture id (string|hash)
     * @param width texture width (number)
     * @param height texture height (number)
     * @param type texture type (string|constant)
     * <ul>
     *   <li><code>"rgb"</code> - RGB</li>
     *   <li><code>"rgba"</code> - RGBA</li>
     *   <li><code>"l"</code> - LUMINANCE</li>
     * </ul>
     * @param buffer texture data (string)
     * @return setting the data was successful (boolean)
     * @examples
     * <pre>
     * function init(self)
     *      local w = 200
     *      local h = 300
     *
     *      -- Create a dynamic texture, all white.
     *      if gui.new_texture("dynamic_tx", w, h, "rgb", string.rep(string.char(0xff), w * h * 3)) then
     *          -- Create a box node and apply the texture to it.
     *          local n = gui.new_box_node(vmath.vector3(200, 200, 0), vmath.vector3(w, h, 0))
     *          gui.set_texture(n, "dynamic_tx")
     *
     *          ...
     *
     *          -- Change the data in the texture to a nice orange.
     *          local orange = string.char(0xff) .. string.char(0x80) .. string.char(0x10)
     *          if gui.set_texture_data("dynamic_tx", w, h, "rgb", string.rep(orange, w * h)) then
     *              -- Go on and to more stuff
     *              ...
     *          end
     *      else
     *          -- Something went wrong
     *          ...
     *      end
     * end
     * </pre>
     */
    static int LuaSetTextureData(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const char* name = luaL_checkstring(L, 1);
        int width = luaL_checkinteger(L, 2);
        int height = luaL_checkinteger(L, 3);
        const char* type_str = luaL_checkstring(L, 4);
        size_t buffer_size;
        luaL_checktype(L, 5, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 5, &buffer_size);
        Scene* scene = GuiScriptInstance_Check(L);

        dmImage::Type type = ToImageType(L, type_str);
        Result r = SetDynamicTextureData(scene, name, width, height, type, buffer, buffer_size);
        if (r == RESULT_OK) {
            lua_pushboolean(L, 1);
        } else {
            dmLogWarning("Failed to set texture data (%d)", r);
            lua_pushboolean(L, 0);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
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
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeFontId(scene, hnode));
        assert(top + 1 == lua_gettop(L));
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

        Scene* scene = GuiScriptInstance_Check(L);

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

    /*# gets the node layer
     * The layer must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_layer
     * @param node node from which to get the layer (node)
     * @return layer id (hash)
     */
    static int LuaGetLayer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeLayerId(scene, hnode));
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the node layer
     * The layer must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_layer
     * @param node node for which to set the layer (node)
     * @param layer layer id (string|hash)
     */
    static int LuaSetLayer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        if (lua_isstring(L, 2))
        {
            const char* layer_id = luaL_checkstring(L, 2);

            Result r = SetNodeLayer(scene, hnode, layer_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Layer %s is not specified in scene", layer_id);
            }
        }
        else
        {
            dmhash_t layer_id = dmScript::CheckHash(L, 2);
            Result r = SetNodeLayer(scene, hnode, layer_id);
            if (r != RESULT_OK)
            {
                const char* id_string = (const char*)dmHashReverse64(layer_id, 0x0);
                if (id_string != 0x0)
                    luaL_error(L, "Layer %s is not specified in scene", id_string);
                else
                    luaL_error(L, "Layer %llu is not specified in scene", layer_id);
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the scene current layout
     *
     * @name gui.get_layout
     * @return layout id (hash)
     */
    static int LuaGetLayout(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;
        Scene* scene = GuiScriptInstance_Check(L);

        dmScript::PushHash(L, dmGui::GetLayout(scene));
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# gets the node clipping mode
     * Clipping mode defines how the node will clipping it's children nodes
     *
     * @name gui.get_clipping_mode
     * @param node node from which to get the clipping mode (node)
     * @return node clipping mode (constant)
     * <ul>
     *   <li><code>gui.CLIPPING_MODE_NONE</code></li>
     *   <li><code>gui.CLIPPING_MODE_STENCIL</code></li>
     * </ul>
     */
    static int LuaGetClippingMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_ClippingMode);
        return 1;
    }

    /*# sets node clipping mode state
     * Clipping mode defines how the node will clipping it's children nodes
     *
     * @name gui.set_clipping_mode
     * @param node node to set clipping mode for (node)
     * @param clipping_mode clipping mode to set (constant)
     * <ul>
     *   <li><code>gui.CLIPPING_MODE_NONE</code></li>
     *   <li><code>gui.CLIPPING_MODE_STENCIL</code></li>
     * </ul>
     */
    static int LuaSetClippingMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int clipping_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_ClippingMode = (ClippingMode) clipping_mode;
        return 0;
    }

    /*# gets node clipping visibility state
     * If node is set as visible clipping node, it will be shown as well as clipping. Otherwise, it will only clip but not show visually.
     *
     * @name gui.get_clipping_visible
     * @param node node from which to get the clipping visibility state (node)
     * @return true or false
     */
    static int LuaGetClippingVisible(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        lua_pushboolean(L, n->m_Node.m_ClippingVisible);
        return 1;
    }

    /*# sets node clipping visibility
     * If node is set as an visible clipping node, it will be shown as well as clipping. Otherwise, it will only clip but not show visually.
     *
     * @name gui.set_clipping_visible
     * @param node node to set clipping visibility for (node)
     * @param visible true or false
     */
    static int LuaSetClippingVisible(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int visible = lua_toboolean(L, 2);
        n->m_Node.m_ClippingVisible = visible;
        return 0;
    }

    /*# gets node clipping inverted state
     * If node is set as an inverted clipping node, it will clip anything inside as opposed to outside.
     *
     * @name gui.get_clipping_inverted
     * @param node node from which to get the clipping inverted state (node)
     * @return true or false
     */
    static int LuaGetClippingInverted(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        lua_pushboolean(L, n->m_Node.m_ClippingInverted);
        return 1;
    }

    /*# sets node clipping visibility
     * If node is set as an inverted clipping node, it will clip anything inside as opposed to outside.
     *
     * @name gui.set_clipping_inverted
     * @param node node to set clipping inverted state for (node)
     * @param visible true or false
     */
    static int LuaSetClippingInverted(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        int inverted = lua_toboolean(L, 2);
        n->m_Node.m_ClippingInverted = inverted;
        return 0;
    }

    static void PushTextMetrics(lua_State* L, Scene* scene, dmhash_t font_id_hash, const char* text, float width, bool line_break)
    {
        dmGui::TextMetrics metrics;
        dmGui::Result r = dmGui::GetTextMetrics(scene, text, font_id_hash, width, line_break, &metrics);
        if (r != RESULT_OK) {
            const char* id_string = (const char*)dmHashReverse64(font_id_hash, 0x0);
            if (id_string != 0x0) {
                luaL_error(L, "Font %s is not specified in scene", id_string);
            } else {
                printf("%llu", font_id_hash);
                luaL_error(L, "Font %llu is not specified in scene", font_id_hash);
            }
        }

        lua_createtable(L, 0, 4);
        lua_pushliteral(L, "width");
        lua_pushnumber(L, metrics.m_Width);
        lua_rawset(L, -3);
        lua_pushliteral(L, "max_ascent");
        lua_pushnumber(L, metrics.m_MaxAscent);
        lua_rawset(L, -3);
        lua_pushliteral(L, "max_descent");
        lua_pushnumber(L, metrics.m_MaxDescent);
        lua_rawset(L, -3);
    }

    /*# get text metrics from node
     * Get text metrics
     *
     * @name gui.get_text_metrics_from_node
     * @param node text node to measure text from
     * @return a table with the following fields: width, max_ascent, max_descent
     */
    static int LuaGetTextMetricsFromNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void)n;

        dmhash_t font_id_hash = dmGui::GetNodeFontId(scene, hnode);
        const char* text = dmGui::GetNodeText(scene, hnode);
        float width = dmGui::GetNodeProperty(scene, hnode, PROPERTY_SIZE).getX();
        bool line_break = dmGui::GetNodeLineBreak(scene, hnode);
        PushTextMetrics(L, scene, font_id_hash, text, width, line_break);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get text metrics
     * Get text metrics
     *
     * @name gui.get_text_metrics
     * @param font font id. (hash|string)
     * @param text text to measure
     * @param width max-width. use for line-breaks
     * @param line_breaks true to break lines accordingly to width
     * @return a table with the following fields: width, max_ascent, max_descent
     */
    static int LuaGetTextMetrics(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        dmhash_t font_id_hash = 0;
        if (lua_isstring(L, 1)) {
            const char* font_id = luaL_checkstring(L, 1);
            font_id_hash = dmHashString64(font_id);
        } else {
            font_id_hash = dmScript::CheckHash(L, 1);
        }

        const char* text = luaL_checkstring(L, 2);
        float width = luaL_checknumber(L, 3);
        bool line_break = lua_toboolean(L, 4);
        PushTextMetrics(L, scene, font_id_hash, text, width, line_break);

        assert(top + 1 == lua_gettop(L));
        return 1;
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

        Scene* scene = GuiScriptInstance_Check(L);

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

        int anchor = luaL_checkint(L, 2);
        if (anchor != XANCHOR_NONE && anchor != XANCHOR_LEFT && anchor != XANCHOR_RIGHT)
        {
            luaL_error(L, "Invalid x-anchor: %d", anchor);
        }

        Scene* scene = GuiScriptInstance_Check(L);

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
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, GetNodeYAnchor(scene, hnode));

        assert(top + 1 == lua_gettop(L));
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
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int anchor = luaL_checkint(L, 2);
        if (anchor != YANCHOR_NONE && anchor != YANCHOR_TOP && anchor != YANCHOR_BOTTOM)
        {
            luaL_error(L, "Invalid y-anchor: %d", anchor);
        }

        Scene* scene = GuiScriptInstance_Check(L);

        SetNodeYAnchor(scene, hnode, (YAnchor) anchor);

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the pivot of a node
     * The pivot specifies how the node is drawn and rotated from its position.
     *
     * @name gui.get_pivot
     * @param node node to get pivot from (node)
     * @return pivot constant (constant)
     * <ul>
     *   <li><code>gui.PIVOT_CENTER</code></li>
     *   <li><code>gui.PIVOT_N</code></li>
     *   <li><code>gui.PIVOT_NE</code></li>
     *   <li><code>gui.PIVOT_E</code></li>
     *   <li><code>gui.PIVOT_SE</code></li>
     *   <li><code>gui.PIVOT_S</code></li>
     *   <li><code>gui.PIVOT_SW</code></li>
     *   <li><code>gui.PIVOT_W</code></li>
     *   <li><code>gui.PIVOT_NW</code></li>
     * </ul>
     */
    static int LuaGetPivot(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodePivot(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pivot of a node
     * The pivot specifies how the node is drawn and rotated from its position.
     *
     * @name gui.set_pivot
     * @param node node to set pivot for (node)
     * @param pivot pivot constant (constant)
     * <ul>
     *   <li><code>gui.PIVOT_CENTER</code></li>
     *   <li><code>gui.PIVOT_N</code></li>
     *   <li><code>gui.PIVOT_NE</code></li>
     *   <li><code>gui.PIVOT_E</code></li>
     *   <li><code>gui.PIVOT_SE</code></li>
     *   <li><code>gui.PIVOT_S</code></li>
     *   <li><code>gui.PIVOT_SW</code></li>
     *   <li><code>gui.PIVOT_W</code></li>
     *   <li><code>gui.PIVOT_NW</code></li>
     * </ul>
     */
    static int LuaSetPivot(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int pivot = luaL_checkint(L, 2);
        if (pivot < PIVOT_CENTER || pivot > PIVOT_NW)
        {
            luaL_error(L, "Invalid pivot: %d", pivot);
        }

        Scene* scene = GuiScriptInstance_Check(L);

        SetNodePivot(scene, hnode, (Pivot) pivot);

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the scene width
     *
     * @name gui.get_width
     * @return scene width (number)
     */
    static int LuaGetWidth(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, scene->m_Width);
        return 1;
    }

    /*# gets the scene height
     *
     * @name gui.get_height
     * @return scene height (number)
     */
    static int LuaGetHeight(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, scene->m_Height);
        return 1;
    }

    /*# set the slice9 configuration for the node
     *
     * @name gui.set_slice9
     * @param node node to manipulate
     * @param params new value (vector4)
     */
    static int LuaSetSlice9(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        if (dmScript::IsVector4(L, 2))
        {
            const Vector4 value = *(dmScript::CheckVector4(L, 2));
            Scene* scene = GuiScriptInstance_Check(L);
            dmGui::SetNodeProperty(scene, hnode, dmGui::PROPERTY_SLICE9, value);
        }
        else
        {
            luaL_error(L, "invalid parameter given");
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# get the slice9 values for the node
     *
     * @name gui.get_slice9
     * @param node node to manipulate
     * @return vector4 with configuration values
     */
    static int LuaGetSlice9(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);
        dmScript::PushVector4(L, dmGui::GetNodeProperty(scene, hnode, dmGui::PROPERTY_SLICE9));
        return 1;
    }

    /*# sets the number of generarted vertices around the perimeter
     *
     * @name gui.set_perimeter_vertices
     * @param vertex count (number)
     */
    static int LuaSetPerimeterVertices(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        const int vertices = luaL_checkint(L, 2);
        if (vertices < 2 || vertices > 100000)
        {
            luaL_error(L, "Unreasonable number of vertices: %d", vertices);
        }

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodePerimeterVertices(scene, hnode, vertices);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the number of generarted vertices around the perimeter
     *
     * @name gui.get_perimeter_vertices
     * @return vertex count (number)
     */
    static int LuaGetPerimeterVertices(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushinteger(L, dmGui::GetNodePerimeterVertices(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the angle for the filled pie sector
     *
     * @name gui.set_fill_angle
     * @param node node to set the fill angle for (node)
     * @param sector angle
     */
    static int LuaSetPieFillAngle(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        float angle = luaL_checknumber(L, 2);
        if (angle < -360.f || angle > 360.f)
        {
            luaL_error(L, "Fill angle out of bounds %f", angle);
        }

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodePieFillAngle(scene, hnode, angle);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the angle for the filled pie sector
     *
     * @name gui.get_fill_angle
     * @param node node from which to get the fill angle (node)
     * @return sector angle
     */
    static int LuaGetPieFillAngle(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodePieFillAngle(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pie inner radius (defined along the x dimension)
     *
     * @name gui.set_inner_radius
     * @param node node to set the inner radius for (node)
     * @param inner radius
     */
    static int LuaSetInnerRadius(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        float inner_radius = luaL_checknumber(L, 2);
        if (inner_radius < 0)
        {
            luaL_error(L, "Inner radius out of bounds %f", inner_radius);
        }

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodeInnerRadius(scene, hnode, inner_radius);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the pie inner radius (defined along the x dimension)
     *
     * @name gui.get_inner_radius
     * @param node node from where to get the inner radius (node)
     * @return inner radius
     */
    static int LuaGetInnerRadius(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodeInnerRadius(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pie outer bounds mode
     *
     * @name gui.set_outer_bounds
     * @param node node for which to set the outer bounds mode (node)
     * @param BOUNDS_RECTANGLE or BOUNDS_ELLIPSE
     */
    static int LuaSetOuterBounds(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        int bounds = luaL_checkint(L, 2);
        if (bounds != PIEBOUNDS_ELLIPSE && bounds != PIEBOUNDS_RECTANGLE)
        {
            luaL_error(L, "Invalid value for outer bounds! %d", bounds);
        }

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodeOuterBounds(scene, hnode, (PieBounds)bounds);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the pie outer bounds mode
     *
     * @name gui.get_outer_bounds
     * @param node node from where to get the outer bounds mode (node)
     * @return BOUNDS_RECTANGLE or BOUNDS_ELLIPSE
     */
    static int LuaGetOuterBounds(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_pushinteger(L, dmGui::GetNodeOuterBounds(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# determines if the node is pickable by the supplied coordinates
     *
     * @name gui.pick_node
     * @param node node to be tested for picking (node)
     * @param x x-coordinate (see <a href="#on_input">on_input</a> )
     * @param y y-coordinate (see <a href="#on_input">on_input</a> )
     * @return pick result (boolean)
     */
    static int LuaPickNode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        (void) n;

        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushboolean(L, PickNode(scene, hnode, (float) x, (float) y));
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

        Scene* scene = GuiScriptInstance_Check(L);

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

        Scene* scene = GuiScriptInstance_Check(L);

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

    /*# moves the first node above the second
     * Supply nil as the second argument to move the first node to the top.
     *
     * @name gui.move_above
     * @param node to move (node)
     * @param ref reference node above which the first node should be moved (node)
     */
    static int LuaMoveAbove(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        HNode r = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            r = GetNodeHandle(LuaCheckNode(L, 2, &hnode));
        }
        Scene* scene = GuiScriptInstance_Check(L);
        MoveNodeAbove(scene, GetNodeHandle(n), r);
        return 0;
    }

    /*# moves the first node below the second
     * Supply nil as the second argument to move the first node to the bottom.
     *
     * @name gui.move_below
     * @param node to move (node)
     * @param ref reference node below which the first node should be moved (node)
     */
    static int LuaMoveBelow(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        HNode r = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            r = GetNodeHandle(LuaCheckNode(L, 2, &hnode));
        }
        Scene* scene = GuiScriptInstance_Check(L);
        MoveNodeBelow(scene, GetNodeHandle(n), r);
        return 0;
    }

    /*# gets the parent of the specified node
     *
     * If the specified node does not have a parent, nil is returned.
     *
     * @name gui.get_parent
     * @param node the node from which to retrieve its parent (node)
     * @return parent instance (node)
     */
    int LuaGetParent(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);

        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
            node_proxy->m_Scene = scene;
            node_proxy->m_Node = GetNodeHandle(parent);
            luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
            lua_setmetatable(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# set the parent of the node
     *
     * @name gui.set_parent
     * @param node node for which to set its parent (node)
     * @param parent parent node to set (node)
     */
    static int LuaSetParent(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNode(L, 1, &hnode);
        HNode parent = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            parent = GetNodeHandle(LuaCheckNode(L, 2, &hnode));
        }
        Scene* scene = GuiScriptInstance_Check(L);
        dmGui::Result result = dmGui::SetNodeParent(scene, GetNodeHandle(n), parent);
        switch (result)
        {
        case dmGui::RESULT_INF_RECURSION:
            return luaL_error(L, "Unable to set parent since it would cause an infinite loop");
        case dmGui::RESULT_OK:
            return 0;
        default:
            return luaL_error(L, "An unexpected error occurred");
        }
    }

    /*# clone a node
     *
     * This does not include its children. Use gui.clone_tree for that purpose.
     *
     * @name gui.clone
     * @param node node to clone (node)
     * @return the cloned node (node)
     */
    static int LuaClone(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        LuaCheckNode(L, 1, &hnode);

        Scene* scene = GuiScriptInstance_Check(L);
        HNode out_node;
        dmGui::Result result = dmGui::CloneNode(scene, hnode, &out_node);
        switch (result)
        {
        case dmGui::RESULT_OUT_OF_RESOURCES:
            return luaL_error(L, "Not enough resources to clone the node");
        case dmGui::RESULT_OK:
            MoveNodeAbove(scene, out_node, hnode);
            LuaPushNode(L, scene, out_node);
            assert(top + 1 == lua_gettop(L));
            return 1;
        default:
            return luaL_error(L, "An unexpected error occurred");
        }
    }

    static int HashTableIndex(lua_State* L)
    {
        if (lua_isstring(L, -1))
        {
            dmScript::PushHash(L, dmHashString64(lua_tostring(L, -1)));
            lua_rawget(L, -3);
            return 1;
        }
        else
        {
            lua_pushvalue(L, -1);
            lua_rawget(L, -3);
            return 1;
        }
    }

    static dmGui::Result CloneNodeListToTable(lua_State* L, dmGui::HScene scene, uint16_t start_index, dmGui::HNode parent);

    static dmGui::Result CloneNodeToTable(lua_State* L, dmGui::HScene scene, InternalNode* n, dmGui::HNode* out_node)
    {
        dmGui::HNode node = GetNodeHandle(n);
        dmGui::Result result = CloneNode(scene, node, out_node);
        if (result == dmGui::RESULT_OK)
        {
            dmScript::PushHash(L, n->m_NameHash);
            LuaPushNode(L, scene, *out_node);
            lua_rawset(L, -3);
            result = CloneNodeListToTable(L, scene, n->m_ChildHead, *out_node);
        }
        return result;
    }

    static dmGui::Result CloneNodeListToTable(lua_State* L, dmGui::HScene scene, uint16_t start_index, dmGui::HNode parent)
    {
        uint32_t index = start_index;
        dmGui::Result result = dmGui::RESULT_OK;
        while (index != INVALID_INDEX && result == dmGui::RESULT_OK)
        {
            InternalNode* node = &scene->m_Nodes[index];
            dmGui::HNode out_node;
            result = CloneNodeToTable(L, scene, node, &out_node);
            if (result == dmGui::RESULT_OK)
            {
                dmGui::SetNodeParent(scene, out_node, parent);
            }
            index = node->m_NextIndex;
        }
        return result;
    }

    /*# clone a node including its children
     *
     * Use gui.clone to clone a node excluding its children.
     *
     * @name gui.clone_tree
     * @param node root node to clone (node)
     * @return a table mapping node ids to the corresponding cloned nodes (table)
     */
    static int LuaCloneTree(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        lua_newtable(L);

        // Set meta table to convert string indices to hashes
        lua_createtable(L, 0, 1);
        lua_pushcfunction(L, HashTableIndex);
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, -2);

        Scene* scene = GuiScriptInstance_Check(L);
        dmGui::Result result;
        if (!lua_isnil(L, 1))
        {
            dmGui::HNode hnode;
            InternalNode* root = LuaCheckNode(L, 1, &hnode);
            dmGui::HNode out_node;
            result = CloneNodeToTable(L, scene, root, &out_node);
            if (result == dmGui::RESULT_OK)
            {
                dmGui::HNode parent = INVALID_HANDLE;
                if (root->m_ParentIndex != INVALID_INDEX)
                {
                    parent = GetNodeHandle(&scene->m_Nodes[root->m_ParentIndex]);
                }
                dmGui::SetNodeParent(scene, out_node, parent);
            }
        }
        else
        {
            result = CloneNodeListToTable(L, scene, scene->m_RenderHead, INVALID_HANDLE);
        }

        switch (result)
        {
        case dmGui::RESULT_OUT_OF_RESOURCES:
            lua_pop(L, 1);
            return luaL_error(L, "Not enough resources to clone the node tree");
        case dmGui::RESULT_OK:
            assert(top + 1 == lua_gettop(L));
            return 1;
        default:
            lua_pop(L, 1);
            return luaL_error(L, "An unexpected error occurred");
        }
    }

    /*# reset all nodes to initial state
     * reset only applies to static node loaded from the scene. Nodes created dynamically from script are not affected
     *
     * @name gui.reset_nodes
     */
    static int LuaResetNodes(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        ResetNodes(scene);
        return 0;
    }

    /*# set the render ordering for the current GUI scene
     *
     * Set the order number for the current GUI scene. The number dictates the sorting of the "gui" render predicate, in other words
     * in which order the scene will be rendered in relation to other currently rendered GUI scenes.
     *
     * The number must be in the range 0 to 7.
     *
     * @name gui.set_render_order
     * @param order rendering order (number)
     */
    static int LuaSetRenderOrder(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        int order = luaL_checkinteger(L, 1);
        // NOTE: The range reflects the current bits allocated in RenderKey for order
        if (order < 0 || order > 7) {
            dmLogWarning("Render must be in range [0,7]");
        }
        order = dmMath::Clamp(order, 0, 7);
        scene->m_RenderOrder = (uint16_t) order;
        return 0;
    }

    /*# default keyboard
     *
     * @name gui.KEYBOARD_TYPE_DEFAULT
     * @variable
     */

    /*# number input keyboard
     *
     * @name gui.KEYBOARD_TYPE_NUMBER_PAD
     * @variable
     */

    /*# email keyboard
     *
     * @name gui.KEYBOARD_TYPE_EMAIL
     * @variable
     */

    /*# display on-display keyboard if available
     *
     * The specified type of keyboard is displayed, if it is available on
     * the device.
     *
     * @name gui.show_keyboard
     * @param type keyboard type (constant)
     * <ul>
     *   <li><code>gui.KEYBOARD_TYPE_DEFAULT</code></li>
     *   <li><code>gui.KEYBOARD_TYPE_EMAIL</code></li>
     *   <li><code>gui.KEYBOARD_TYPE_NUMBER_PAD</code></li>
     * </ul>
     * @param autoclose close keyboard automatically when clicking outside
     */
    static int LuaShowKeyboard(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        int type = luaL_checkinteger(L, 1);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        bool autoclose = (bool) lua_toboolean(L, 2);
        dmHID::ShowKeyboard(scene->m_Context->m_HidContext, (dmHID::KeyboardType) type, autoclose);
        return 0;
    }

    /*# hide on-display keyboard if available
     *
     * Hide the on-display keyboard on the device.
     *
     * @name gui.hide_keyboard
     */
    static int LuaHideKeyboard(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        dmHID::HideKeyboard(scene->m_Context->m_HidContext);
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
            n->m_Node.m_DirtyLocal = 1;\
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
        params->m_PhysicalWidth = 640;
        params->m_PhysicalHeight = 960;
        params->m_Dpi = 360;
    }

    /*# gets the node screen position
     *
     * @name gui.get_screen_position
     * @param node node to get the screen position from (node)
     * @return node screen position (vector3)
     */
    int LuaGetScreenPosition(lua_State* L)
    {
        InternalNode* n = LuaCheckNode(L, 1, 0);
        Scene* scene = GuiScriptInstance_Check(L);
        Matrix4 node_transform;
        Vector4 center(0.5f, 0.5f, 0.0f, 1.0f);
        CalculateNodeTransform(scene, n, CalculateNodeTransformFlags(CALCULATE_NODE_BOUNDARY | CALCULATE_NODE_INCLUDE_SIZE | CALCULATE_NODE_RESET_PIVOT), node_transform);
        Vector4 p = node_transform * center;
        dmScript::PushVector3(L, Vector3(p.getX(), p.getY(), p.getZ()));
        return 1;
    }

#define REGGETSET(name, luaname) \
        {"get_"#luaname, LuaGet##name},\
        {"set_"#luaname, LuaSet##name},\

    static const luaL_reg Gui_methods[] =
    {
        {"get_node",        LuaGetNode},
        {"get_id",          LuaGetId},
        {"set_id",          LuaSetId},
        {"get_index",       LuaGetIndex},
        {"delete_node",     LuaDeleteNode},
        {"animate",         LuaAnimate},
        {"cancel_animation",LuaCancelAnimation},
        {"new_box_node",    LuaNewBoxNode},
        {"new_text_node",   LuaNewTextNode},
        {"new_pie_node",    LuaNewPieNode},
        {"get_text",        LuaGetText},
        {"set_text",        LuaSetText},
        {"set_line_break",  LuaSetLineBreak},
        {"get_line_break",  LuaGetLineBreak},
        {"get_blend_mode",  LuaGetBlendMode},
        {"set_blend_mode",  LuaSetBlendMode},
        {"get_clipping_mode",  LuaGetClippingMode},
        {"set_clipping_mode",  LuaSetClippingMode},
        {"get_clipping_visible", LuaGetClippingVisible},
        {"set_clipping_visible",LuaSetClippingVisible},
        {"get_clipping_inverted", LuaGetClippingInverted},
        {"set_clipping_inverted",LuaSetClippingInverted},
        {"get_texture",     LuaGetTexture},
        {"set_texture",     LuaSetTexture},
        {"get_flipbook",    LuaGetFlipbook},
        {"play_flipbook",   LuaPlayFlipbook},
        {"cancel_flipbook", LuaCancelFlipbook},
        {"new_texture",     LuaNewTexture},
        {"delete_texture",  LuaDeleteTexture},
        {"set_texture_data",LuaSetTextureData},
        {"get_font",        LuaGetFont},
        {"set_font",        LuaSetFont},
        {"get_layer",        LuaGetLayer},
        {"set_layer",        LuaSetLayer},
        {"get_layout",        LuaGetLayout},
        {"get_text_metrics",LuaGetTextMetrics},
        {"get_text_metrics_from_node",LuaGetTextMetricsFromNode},
        {"get_xanchor",     LuaGetXAnchor},
        {"set_xanchor",     LuaSetXAnchor},
        {"get_yanchor",     LuaGetYAnchor},
        {"set_yanchor",     LuaSetYAnchor},
        {"get_pivot",       LuaGetPivot},
        {"set_pivot",       LuaSetPivot},
        {"get_width",       LuaGetWidth},
        {"get_height",      LuaGetHeight},
        {"get_slice9",      LuaGetSlice9},
        {"set_slice9",      LuaSetSlice9},
        {"pick_node",       LuaPickNode},
        {"is_enabled",      LuaIsEnabled},
        {"set_enabled",     LuaSetEnabled},
        {"get_adjust_mode", LuaGetAdjustMode},
        {"set_adjust_mode", LuaSetAdjustMode},
        {"move_above",      LuaMoveAbove},
        {"move_below",      LuaMoveBelow},
        {"get_parent",      LuaGetParent},
        {"set_parent",      LuaSetParent},
        {"clone",           LuaClone},
        {"clone_tree",      LuaCloneTree},
        {"show_keyboard",   LuaShowKeyboard},
        {"hide_keyboard",   LuaHideKeyboard},
        {"get_screen_position", LuaGetScreenPosition},
        {"reset_nodes",     LuaResetNodes},
        {"set_render_order",LuaSetRenderOrder},
        {"set_fill_angle", LuaSetPieFillAngle},
        {"get_fill_angle", LuaGetPieFillAngle},
        {"set_perimeter_vertices", LuaSetPerimeterVertices},
        {"get_perimeter_vertices", LuaGetPerimeterVertices},
        {"set_inner_radius", LuaSetInnerRadius},
        {"get_inner_radius", LuaGetInnerRadius},
        {"set_outer_bounds", LuaSetOuterBounds},
        {"get_outer_bounds", LuaGetOuterBounds},

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

    /*# fill_angle property
     *
     * @name gui.PROP_FILL_ANGLE
     * @variable
     */

    /*# inner_radius property
     *
     * @name gui.PROP_INNER_RADIUS
     * @variable
     */

    /*# slice9 property
     *
     * @name gui.PROP_SLICE9
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

    /*# clipping mode none
     *
     * @name gui.CLIPPING_MODE_NONE
     * @variable
     */

    /*# clipping mode stencil
     *
     * @name gui.CLIPPING_MODE_STENCIL
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

    /*# elliptical pie node bounds
     * @name gui.PIEBOUNDS_ELLIPSE
     * @variable
     */

    /*# rectangular pie node bounds
     * @name gui.PIEBOUNDS_RECTANGLE
     * @variable
     */

    lua_State* InitializeScript(dmScript::HContext script_context)
    {
        lua_State* L = dmScript::GetLuaState(script_context);

        int top = lua_gettop(L);
        (void)top;

        dmScript::RegisterUserType(L, GUI_SCRIPT, GuiScript_methods, GuiScript_meta);

        dmScript::RegisterUserType(L, GUI_SCRIPT_INSTANCE, GuiScriptInstance_methods, GuiScriptInstance_meta);

        dmScript::RegisterUserType(L, NODE_PROXY_TYPE_NAME, NodeProxy_methods, NodeProxy_meta);

        luaL_register(L, LIB_NAME, Gui_methods);

#define SETPROP(name, prop) \
        lua_pushliteral(L, #name);\
        lua_setfield(L, -2, "PROP_"#prop);\

        SETPROP(position, POSITION)
        SETPROP(rotation, ROTATION)
        SETPROP(scale, SCALE)
        SETPROP(color, COLOR)
        SETPROP(outline, OUTLINE)
        SETPROP(shadow, SHADOW)
        SETPROP(size, SIZE)
        SETPROP(fill_angle, FILL_ANGLE)
        SETPROP(inner_radius, INNER_RADIUS)
        SETPROP(slice9, SLICE9)

#undef SETPROP

// For backwards compatibility
#define SETEASINGOLD(name, easing) \
        lua_pushnumber(L, (lua_Number) easing); \
        lua_setfield(L, -2, "EASING_"#name);\

        SETEASINGOLD(NONE, dmEasing::TYPE_LINEAR)
        SETEASINGOLD(IN, dmEasing::TYPE_INCUBIC)
        SETEASINGOLD(OUT,  dmEasing::TYPE_OUTCUBIC)
        SETEASINGOLD(INOUT,  dmEasing::TYPE_INOUTCUBIC)

#undef SETEASINGOLD

#define SETEASING(name) \
        lua_pushnumber(L, (lua_Number) dmEasing::TYPE_##name); \
        lua_setfield(L, -2, "EASING_"#name);\

        SETEASING(LINEAR)
        SETEASING(INQUAD)
        SETEASING(OUTQUAD)
        SETEASING(INOUTQUAD)
        SETEASING(OUTINQUAD)
        SETEASING(INCUBIC)
        SETEASING(OUTCUBIC)
        SETEASING(INOUTCUBIC)
        SETEASING(OUTINCUBIC)
        SETEASING(INQUART)
        SETEASING(OUTQUART)
        SETEASING(INOUTQUART)
        SETEASING(OUTINQUART)
        SETEASING(INQUINT)
        SETEASING(OUTQUINT)
        SETEASING(INOUTQUINT)
        SETEASING(OUTINQUINT)
        SETEASING(INSINE)
        SETEASING(OUTSINE)
        SETEASING(INOUTSINE)
        SETEASING(OUTINSINE)
        SETEASING(INEXPO)
        SETEASING(OUTEXPO)
        SETEASING(INOUTEXPO)
        SETEASING(OUTINEXPO)
        SETEASING(INCIRC)
        SETEASING(OUTCIRC)
        SETEASING(INOUTCIRC)
        SETEASING(OUTINCIRC)
        SETEASING(INELASTIC)
        SETEASING(OUTELASTIC)
        SETEASING(INOUTELASTIC)
        SETEASING(OUTINELASTIC)
        SETEASING(INBACK)
        SETEASING(OUTBACK)
        SETEASING(INOUTBACK)
        SETEASING(OUTINBACK)
        SETEASING(INBOUNCE)
        SETEASING(OUTBOUNCE)
        SETEASING(INOUTBOUNCE)
        SETEASING(OUTINBOUNCE)

#undef SETEASING


#define SETBLEND(name) \
        lua_pushnumber(L, (lua_Number) BLEND_MODE_##name); \
        lua_setfield(L, -2, "BLEND_"#name);\

        SETBLEND(ALPHA)
        SETBLEND(ADD)
        SETBLEND(ADD_ALPHA)
        SETBLEND(MULT)

#undef SETBLEND

#define SETCLIPPINGMODE(name) \
        lua_pushnumber(L, (lua_Number) CLIPPING_MODE_##name); \
        lua_setfield(L, -2, "CLIPPING_MODE_"#name);\

        SETCLIPPINGMODE(NONE)
        SETCLIPPINGMODE(STENCIL)

#undef SETBLEND

#define SETKEYBOARD(name) \
        lua_pushnumber(L, (lua_Number) dmHID::KEYBOARD_TYPE_##name); \
        lua_setfield(L, -2, "KEYBOARD_TYPE_"#name);\

        SETKEYBOARD(DEFAULT)
        SETKEYBOARD(NUMBER_PAD)
        SETKEYBOARD(EMAIL)

#undef SETKEYBOARD

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

#define SETPLAYBACK(name) \
        lua_pushnumber(L, (lua_Number) PLAYBACK_##name); \
        lua_setfield(L, -2, "PLAYBACK_"#name);\

        SETPLAYBACK(ONCE_FORWARD)
        SETPLAYBACK(ONCE_BACKWARD)
        SETPLAYBACK(ONCE_PINGPONG)
        SETPLAYBACK(LOOP_FORWARD)
        SETPLAYBACK(LOOP_BACKWARD)
        SETPLAYBACK(LOOP_PINGPONG)

#undef SETADJUST

#define SETBOUNDS(name) \
        lua_pushnumber(L, (lua_Number) PIEBOUNDS_##name); \
        lua_setfield(L, -2, "PIEBOUNDS_"#name);\

        SETBOUNDS(RECTANGLE)
        SETBOUNDS(ELLIPSE)
#undef SETBOUNDS


        lua_pop(L, 1);

        assert(lua_gettop(L) == top);

        return L;
    }

    void FinalizeScript(lua_State* L, dmScript::HContext script_context)
    {
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
     *   <tr><td><code>screen_x</code></td><td>The screen space x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>screen_y</code></td><td>The screen space y value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dx</code></td><td>The change in x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dy</code></td><td>The change in y value of a pointer device, if present.</td></tr>
     *   <tr><td><code>screen_dx</code></td><td>The change in screen space x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>screen_dy</code></td><td>The change in screen space y value of a pointer device, if present.</td></tr>
     *   <tr><td><code>touch</code></td><td>List of touch input, one element per finger, if present. See table below about touch input</td></tr>
     * </table>
     *
     * <p>
     * Touch input table:
     * </p>
     * <table>
     *   <th>Field</th>
     *   <th>Description</th>
     *   <tr><td><code>pressed</code></td><td>True if the finger was pressed this frame.</td></tr>
     *   <tr><td><code>released</code></td><td>True if the finger was released this frame.</td></tr>
     *   <tr><td><code>tap_count</code></td><td>Number of taps, one for single, two for double-tap, etc</td></tr>
     *   <tr><td><code>x</code></td><td>The x touch location.</td></tr>
     *   <tr><td><code>y</code></td><td>The y touch location.</td></tr>
     *   <tr><td><code>dx</code></td><td>The change in x value.</td></tr>
     *   <tr><td><code>dy</code></td><td>The change in y value.</td></tr>
     *   <tr><td><code>acc_x</code></td><td>Accelerometer x value (if present).</td></tr>
     *   <tr><td><code>acc_y</code></td><td>Accelerometer y value (if present).</td></tr>
     *   <tr><td><code>acc_z</code></td><td>Accelerometer z value (if present).</td></tr>
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

    HScene GetSceneFromLua(lua_State* L) {
        return GetScene(L);
    }
}
