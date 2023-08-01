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

#include "gui_script.h"
#include <float.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>

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
    /*# GUI API documentation
     *
     * GUI core hooks, functions, messages, properties and constants for
     * creation and manipulation of GUI nodes. The "gui" namespace is
     * accessible only from gui scripts.
     *
     * @document
     * @name GUI
     * @namespace gui
     */

    /*# [type:hash] gui material
     *
     * The main material (the default material assigned to a GUI) used when rendering the gui. The type of the property is hash.
     *
     * @name material
     * @property
     *
     * @examples
     *
     * How to set material using a script property (see [ref:resource.material])
     *
     * ```lua
     * go.property("desaturate_material", resource.material("/desaturate.material"))
     *
     * function init(self)
     *   go.set("#gui", "material", self.desaturate_material)
     * end
     * ```
     */

    /*# [type:hash] gui materials
     *
     * The materials used when rendering the gui. The type of the property is hash.
     * Key must be specified in options table.
     *
     * @name materials
     * @property
     *
     * @examples
     *
     * How to change a named material resource using a script property from a script
     *
     * ```lua
     * go.property("my_material", resource.material("/my_material.material"))
     *
     * function init(self)
     *   -- this will update the "my_gui_material" entry in the GUI to use the material
     *   -- specified in the "my_material" script property.
     *   go.set("#gui", "materials", self.my_material, { key = "my_gui_material" })
     * end
     * ```
     */

    /*# [type:hash] gui textures
     *
     * The textures used in the gui. The type of the property is hash.
     * Key must be specified in options table.
     *
     * @name textures
     * @property
     *
     * @examples
     *
     * How to set texture using a script property (see [ref:resource.atlas])
     *
     * ```lua
     * go.property("cards_red", resource.atlas("/cards_red.atlas"))
     * go.property("cards_blue", resource.atlas("/cards_blue.atlas"))
     *
     * function init(self)
     *   go.set("#gui", "textures", self.cards_red, {key = "cards"})
     * end
     * ```
     */

    /*# [type:hash] gui fonts
     *
     * The fonts used in the gui. The type of the property is hash.
     * Key must be specified in options table.
     *
     * @name fonts
     * @property
     *
     * @examples
     *
     * How to set font using a script property (see [ref:resource.font])
     *
     * ```lua
     * go.property("title_latin", resource.font("/open_sans.font"))
     * go.property("title_cyrillic", resource.font("/open_sans_cyrillic.font"))
     *
     * function init(self)
     *   go.set("#gui", "fonts", self.title_cyrillic, {key = "title"})
     * end
     * ```
     */

    using namespace dmVMath;

    #define LIB_NAME "gui"
    #define NODE_PROXY_TYPE_NAME "NodeProxy"

    static uint32_t GUI_SCRIPT_TYPE_HASH = 0;
    static uint32_t GUI_SCRIPT_INSTANCE_TYPE_HASH = 0;
    static uint32_t NODE_PROXY_TYPE_HASH = 0;

    static int GuiScriptGetURL(lua_State* L)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
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

    HScene GetScene(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;
        dmScript::GetInstance(L);
        HScene scene = (Scene*)dmScript::ToUserType(L, -1, GUI_SCRIPT_INSTANCE_TYPE_HASH);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return scene;
    }

    static Scene* GuiScriptInstance_Check(lua_State* L, int index)
    {
        return (Scene*)dmScript::CheckUserType(L, index, GUI_SCRIPT_INSTANCE_TYPE_HASH, "You can only access gui.* functions and values from a gui script instance (.gui_script file)");
    }

    static HScene GuiScriptInstance_Check(lua_State* L)
    {
        dmScript::GetInstance(L);
        HScene scene = GuiScriptInstance_Check(L, -1);
        lua_pop(L, 1);
        return scene;
    }

    HScene LuaCheckScene(lua_State* L)
    {
        return GuiScriptInstance_Check(L);
    }

    static int GuiScriptInstance_tostring(lua_State *L)
    {
        lua_pushfstring(L, "GuiScript: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int GuiScriptInstance_index(lua_State *L)
    {
        Scene* i = (Scene*)lua_touserdata(L, 1);
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

        Scene* i = (Scene*)lua_touserdata(L, 1);
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

    static int GuiScriptGetInstanceContextTableRef(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const int self_index = 1;

        Scene* i = (Scene*)lua_touserdata(L, self_index);
        lua_pushnumber(L, i ? i->m_ContextTableReference : LUA_NOREF);

        return 1;
    }

    static int GuiScriptGetInstanceDataTableRef(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        Scene* i = (Scene*)lua_touserdata(L, 1);
        lua_pushnumber(L, i ? i->m_DataReference : LUA_NOREF);

        return 1;
    }

    static const luaL_reg GuiScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg GuiScriptInstance_meta[] =
    {
        {"__tostring",                                  GuiScriptInstance_tostring},
        {"__index",                                     GuiScriptInstance_index},
        {"__newindex",                                  GuiScriptInstance_newindex},
        {dmScript::META_TABLE_GET_URL,                  GuiScriptInstanceGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH,             GuiScriptInstanceResolvePath},
        {dmScript::META_TABLE_IS_VALID,                 GuiScriptInstanceIsValid},
        {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, GuiScriptGetInstanceContextTableRef},
        {dmScript::META_GET_INSTANCE_DATA_TABLE_REF,    GuiScriptGetInstanceDataTableRef},
        {0, 0}
    };

    static NodeProxy* NodeProxy_Check(lua_State *L, int index)
    {
        return (NodeProxy*)dmScript::CheckUserType(L, index, NODE_PROXY_TYPE_HASH, 0);
    }

    static bool IsValidNode(HScene scene, HNode node)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        if (index < scene->m_Nodes.Size())
        {
            InternalNode* n = &scene->m_Nodes[index];
            return n->m_Version == version && n->m_Index == index && !n->m_Node.m_HasHeadlessPfx;
        }
        else
        {
            return false;
        }
    }

    static InternalNode* LuaCheckNodeInternal(lua_State* L, int index, HNode* hnode)
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

    HNode LuaCheckNode(lua_State* L, int index)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, index, &hnode);
        (void)n;
        return hnode;
    }

    static int NodeProxy_tostring (lua_State *L)
    {
        DM_LUA_STACK_CHECK(L,1);

        NodeProxy* np = NodeProxy_Check(L, 1);

        if (np->m_Scene == GetScene(L))
        {
            InternalNode* n = 0;

            if (IsValidNode(np->m_Scene, np->m_Node))
            {
                n = GetNode(np->m_Scene, np->m_Node);
            }
            else
            {
                luaL_error(L, "Deleted node");
            }

            Vector4 pos = n->m_Node.m_Properties[PROPERTY_POSITION];
            switch (n->m_Node.m_NodeType)
            {
                case NODE_TYPE_BOX:
                    lua_pushfstring(L, "box@(%f, %f, %f)", pos.getX(), pos.getY(), pos.getZ());
                    break;
                case NODE_TYPE_TEXT:
                    lua_pushfstring(L, "%s@(%f, %f, %f)", n->m_Node.m_Text, pos.getX(), pos.getY(), pos.getZ());
                    break;
                case NODE_TYPE_CUSTOM:
                    lua_pushfstring(L, "custom_%d@(%f, %f, %f)", n->m_Node.m_CustomType, pos.getX(), pos.getY(), pos.getZ());
                    break;
                case NODE_TYPE_PARTICLEFX:
                    lua_pushfstring(L, "particlefx@(%f, %f, %f)", pos.getX(), pos.getY(), pos.getZ());
                    break;
                default:
                    lua_pushfstring(L, "unknown@(%f, %f, %f)", pos.getX(), pos.getY(), pos.getZ());
                    break;
            }
        }
        else
        {
            lua_pushstring(L,"<foreign scene node>");
        }

        return 1;
    }

    static int NodeProxy_index(lua_State *L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        (void)n;

        const char* key = luaL_checkstring(L, 2);
        return luaL_error(L, "Illegal operation, try %s.get_%s(<node>)", LIB_NAME, key);
    }

    static int NodeProxy_newindex(lua_State *L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;
        const char* key = luaL_checkstring(L, 2);

        return luaL_error(L, "Illegal operation, try %s.set_%s(<node>, <value>)", LIB_NAME, key);
    }

    static int NodeProxy_eq(lua_State *L)
    {
        NodeProxy* np1 = (NodeProxy*)dmScript::ToUserType(L, 1, NODE_PROXY_TYPE_HASH);
        NodeProxy* np2 = (NodeProxy*)dmScript::ToUserType(L, 2, NODE_PROXY_TYPE_HASH);
        if (np1 == 0 || np2 == 0)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        if (np1->m_Scene != np2->m_Scene)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        HNode hn1, hn2;
        InternalNode* n1 = LuaCheckNodeInternal(L, 1, &hn1);
        InternalNode* n2 = LuaCheckNodeInternal(L, 2, &hn2);
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
        {"__tostring", NodeProxy_tostring},
        {"__index",    NodeProxy_index},
        {"__newindex", NodeProxy_newindex},
        {"__eq",       NodeProxy_eq},
        {0, 0}
    };

    /*# gets the node with the specified id
     *
     * Retrieves the node with the specified id.
     *
     * @name gui.get_node
     * @param id [type:string|hash] id of the node to retrieve
     * @return instance [type:node] a new node instance
     * @examples
     *
     * Gets a node by id and change its color:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     * local red = vmath.vector4(1.0, 0.0, 0.0, 1.0)
     * gui.set_color(node, red)
     * ```
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
                luaL_error(L, "No such node: '%s'", dmHashReverseSafe64(id));
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
     * Retrieves the id of the specified node.
     *
     * @name gui.get_id
     * @param node [type:node] the node to retrieve the id from
     * @return id [type:hash] the id of the node
     * @examples
     *
     * Gets the id of a node:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     *
     * local id = gui.get_id(node)
     * print(id) --> hash: [my_node]
     * ```
     */
    int LuaGetId(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);

        dmScript::PushHash(L, n->m_NameHash);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# sets the id of the specified node
     *
     * Set the id of the specicied node to a new value.
     * Nodes created with the gui.new_*_node() functions get
     * an empty id. This function allows you to give dynamically
     * created nodes an id.
     *
     * [icon:attention] No checking is done on the uniqueness of supplied ids.
     * It is up to you to make sure you use unique ids.
     *
     * @name gui.set_id
     * @param node [type:node] node to set the id for
     * @param id [type:string|hash] id to set
     * @examples
     *
     * Create a new node and set its id:
     *
     * ```lua
     * local pos = vmath.vector3(100, 100, 0)
     * local size = vmath.vector3(100, 100, 0)
     * local node = gui.new_box_node(pos, size)
     * gui.set_id(node, "my_new_node")
     * ```
     */
    int LuaSetId(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);
        HNode hnode;
        LuaCheckNodeInternal(L, 1, &hnode);

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
     *
     * Retrieve the index of the specified node among its siblings.
     * The index defines the order in which a node appear in a GUI scene.
     * Higher index means the node is drawn on top of lower indexed nodes.
     *
     * @name gui.get_index
     * @param node [type:node] the node to retrieve the id from
     * @return index [type:number] the index of the node
     * @examples
     *
     * Compare the index order of two sibling nodes:
     *
     * ```lua
     * local node1 = gui.get_node("my_node_1")
     * local node2 = gui.get_node("my_node_2")
     *
     * if gui.get_index(node1) < gui.get_index(node2) then
     *     -- node1 is drawn below node2
     * else
     *     -- node2 is drawn below node1
     * end
     * ```
     */
    int LuaGetIndex(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);

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
     * Deletes the specified node. Any child nodes of the specified node will be
     * recursively deleted.
     *
     * @name gui.delete_node
     * @param node [type:node] node to delete
     * @examples
     *
     * Delete a particular node and any child nodes it might have:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     * gui.delete_node(node)
     * ```
     */
    int LuaDeleteNode(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        if (n->m_Node.m_IsBone) {
            return luaL_error(L, "Unable to delete bone nodes");
            return 0;
        }

        // Set deferred delete flag
        n->m_Deleted = 1;

        return 0;
    }

    void LuaCurveRelease(dmEasing::Curve* curve)
    {
        HScene scene = (HScene)curve->userdata1;
        lua_State* L = scene->m_Context->m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);

        int ref = (int) (((uintptr_t) curve->userdata2) & 0xffffffff);

        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);
        dmScript::Unref(L, -1, ref);
        lua_pop(L, 1);

        curve->release_callback = 0x0;
        curve->userdata1 = 0x0;
        curve->userdata2 = 0x0;
    }

    struct LuaAnimationCompleteArgs
    {
        LuaAnimationCompleteArgs(HScene scene, int node_ref)
            : m_Scene(scene), m_NodeRef(node_ref)
        {}
        HScene m_Scene;
        int m_NodeRef;
    };

    static void LuaCallbackCustomArgsCB(lua_State* L, void* user_args)
    {
        LuaAnimationCompleteArgs* args = (LuaAnimationCompleteArgs*)user_args;
        lua_rawgeti(L, LUA_REGISTRYINDEX, args->m_Scene->m_ContextTableReference);
        lua_rawgeti(L, -1, args->m_NodeRef);
        lua_insert(L, -2);
        lua_pop(L, 1);
    }

    void LuaAnimationComplete(HScene scene, HNode node, bool finished, void* userdata1, void* userdata2)
    {
        lua_State* L = scene->m_Context->m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);

        dmScript::LuaCallbackInfo* cbk = (dmScript::LuaCallbackInfo*)userdata1;
        int node_ref = (int) ((uintptr_t) userdata2 & 0xffffffff);

        if( finished && dmScript::IsCallbackValid(cbk))
        {
            LuaAnimationCompleteArgs args(scene, node_ref);
            dmScript::InvokeCallback(cbk, LuaCallbackCustomArgsCB, &args);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);
        luaL_unref(L, -1, node_ref);
        lua_pop(L, 1);
        dmScript::DestroyCallback(cbk);
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
     * This starts an animation of a node property according to the specified parameters.
     * If the node property is already being animated, that animation will be canceled and
     * replaced by the new one. Note however that several different node properties
     * can be animated simultaneously. Use `gui.cancel_animation` to stop the animation
     * before it has completed.
     *
     * Composite properties of type vector3, vector4 or quaternion
     * also expose their sub-components (x, y, z and w).
     * You can address the components individually by suffixing the name with a dot '.'
     * and the name of the component.
     * For instance, `"position.x"` (the position x coordinate) or `"color.w"`
     * (the color alpha value).
     *
     * If a `complete_function` (Lua function) is specified, that function will be called
     * when the animation has completed.
     * By starting a new animation in that function, several animations can be sequenced
     * together. See the examples below for more information.
     *
     * @name gui.animate
     * @param node [type:node] node to animate
     * @param property [type:string|constant] property to animate
     *
     * - `"position"`
     * - `"rotation"`
     * - `"scale"`
     * - `"color"`
     * - `"outline"`
     * - `"shadow"`
     * - `"size"`
     * - `"fill_angle"` (pie)
     * - `"inner_radius"` (pie)
     * - `"slice9"` (slice9)
     *
     * The following property constants are defined equaling the corresponding property string names.
     *
     * - `gui.PROP_POSITION`
     * - `gui.PROP_ROTATION`
     * - `gui.PROP_SCALE`
     * - `gui.PROP_COLOR`
     * - `gui.PROP_OUTLINE`
     * - `gui.PROP_SHADOW`
     * - `gui.PROP_SIZE`
     * - `gui.PROP_FILL_ANGLE`
     * - `gui.PROP_INNER_RADIUS`
     * - `gui.PROP_SLICE9`
     *
     * @param to [type:number|vector3|vector4|quaternion] target property value
     * @param easing [type:constant|vector] easing to use during animation.
     *      Either specify one of the `gui.EASING_*` constants or provide a
     *      [type:vector] with a custom curve. See the <a href="/manuals/animation#_easing">animation guide</a> for more information.
     * @param duration [type:number] duration of the animation in seconds.
     * @param [delay] [type:number] delay before the animation starts in seconds.
     * @param [complete_function] [type:function(self, node)] function to call when the
     *      animation has completed
     * @param [playback] [type:constant] playback mode
     *
     * - `gui.PLAYBACK_ONCE_FORWARD`
     * - `gui.PLAYBACK_ONCE_BACKWARD`
     * - `gui.PLAYBACK_ONCE_PINGPONG`
     * - `gui.PLAYBACK_LOOP_FORWARD`
     * - `gui.PLAYBACK_LOOP_BACKWARD`
     * - `gui.PLAYBACK_LOOP_PINGPONG`
     *
     * @examples
     *
     * How to start a simple color animation, where the node fades in to white during 0.5 seconds:
     *
     * ```lua
     * gui.set_color(node, vmath.vector4(0, 0, 0, 0)) -- node is fully transparent
     * gui.animate(node, gui.PROP_COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5) -- start animation
     * ```
     *
     * How to start a sequenced animation where the node fades in to white during 0.5 seconds, stays visible for 2 seconds and then fades out:
     *
     * ```lua
     * local function on_animation_done(self, node)
     *     -- fade out node, but wait 2 seconds before the animation starts
     *     gui.animate(node, gui.PROP_COLOR, vmath.vector4(0, 0, 0, 0), gui.EASING_OUTQUAD, 0.5, 2.0)
     * end
     *
     * function init(self)
     *     -- fetch the node we want to animate
     *     local my_node = gui.get_node("my_node")
     *     -- node is initially set to fully transparent
     *     gui.set_color(my_node, vmath.vector4(0, 0, 0, 0))
     *     -- animate the node immediately and call on_animation_done when the animation has completed
     *     gui.animate(my_node, gui.PROP_COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5, 0.0, on_animation_done)
     * end
     * ```
     *
     * How to animate a node's y position using a crazy custom easing curve:
     *
     * ```lua
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
     * ```
     */
    int LuaAnimate(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* node = LuaCheckNodeInternal(L, 1, &hnode);
        (void) node;

        dmhash_t property_hash;
        if (dmScript::IsHash(L, 2)) {
           property_hash = dmScript::CheckHash(L, 2);
        } else {
           property_hash = dmHashString64(luaL_checkstring(L, 2));
        }

        if (!dmGui::HasPropertyHash(scene, hnode, property_hash)) {
            char buffer[128];
            luaL_error(L, "property '%s' not found", dmScript::GetStringFromHashOrString(L, 2, buffer, sizeof(buffer)));
        }

        Vector3* v3;
        Vector4 to;
        if (lua_isnumber(L, 3))
        {
            to = Vector4((float) lua_tonumber(L, 3));
        }
        else if ((v3 = dmScript::ToVector3(L, 3)))
        {
            Vector4 original = dmGui::GetNodePropertyHash(scene, hnode, property_hash);
            to = Vector4(*v3, original.getW());
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

            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);
            lua_pushvalue(L, 4);

            curve.release_callback = LuaCurveRelease;
            curve.userdata1 = (void*)scene;
            curve.userdata2 = (void*)(uintptr_t)dmScript::Ref(L, -2);
            lua_pop(L, 1);
        }
        else
        {
            return luaL_error(L, "easing must be either a easing constant or a vmath.vector");
        }

        lua_Number duration = luaL_checknumber(L, 5);
        float delay = 0.0f;

        int node_ref = LUA_NOREF;
        dmScript::LuaCallbackInfo* cbk = 0x0;
        if (lua_isnumber(L, 6))
        {
            delay = (float) lua_tonumber(L, 6);
            if (lua_isfunction(L, 7))
            {
                cbk = dmScript::CreateCallback(L, 7);

                lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);
                lua_pushvalue(L, 1);
                node_ref = luaL_ref(L, -2);
                lua_pop(L, 1);
            }
        } else if (!lua_isnone(L, 6)) {
            // If argument 6 is specified is has to be a number
            luaL_typerror(L, 6, "number");
        }

        Playback playback = PLAYBACK_ONCE_FORWARD;
        if (lua_isnumber(L, 8)) {
            playback = (Playback) luaL_checkinteger(L, 8);
        }

        if (cbk == 0x0) {
            AnimateNodeHash(scene, hnode, property_hash, to, curve, playback, (float) duration, delay, 0, 0, 0);
        } else {
            AnimateNodeHash(scene, hnode, property_hash, to, curve, playback, (float) duration, delay, &LuaAnimationComplete, cbk, (void*)(uintptr_t) node_ref);
        }
        return 0;
    }

    /*# cancels an ongoing animation
     *
     * If an animation of the specified node is currently running (started by <code>gui.animate</code>), it will immediately be canceled.
     *
     * @name gui.cancel_animation
     * @param node [type:node] node that should have its animation canceled
     * @param property [type:string|constant] property for which the animation should be canceled
     *
     * - `"position"`
     * - `"rotation"`
     * - `"scale"`
     * - `"color"`
     * - `"outline"`
     * - `"shadow"`
     * - `"size"`
     * - `"fill_angle"` (pie)
     * - `"inner_radius"` (pie)
     * - `"slice9"` (slice9)
     *
     * @examples
     *
     * Start an animation of the position property of a node, then cancel parts of
     * the animation:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     * -- animate to new position
     * local pos = vmath.vector3(100, 100, 0)
     * gui.animate(node, "position", pos, go.EASING_LINEAR, 2)
     * ...
     * -- cancel animation of the x component.
     * gui.cancel_animation(node, "position.x")
     * ```
     */
    int LuaCancelAnimation(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* node = LuaCheckNodeInternal(L, 1, &hnode);
        (void) node;

        dmhash_t property_hash;
        if (dmScript::IsHash(L, 2)) {
           property_hash = dmScript::CheckHash(L, 2);
        } else {
           property_hash = dmHashString64(luaL_checkstring(L, 2));
        }

        if (!dmGui::HasPropertyHash(scene, hnode, property_hash)) {
            luaL_error(L, "property '%s' not found", dmHashReverseSafe64(property_hash));
        }

        CancelAnimationHash(scene, hnode, property_hash);

        assert(top== lua_gettop(L));
        return 0;
    }

    void LuaPushNode(lua_State* L, dmGui::HScene scene, dmGui::HNode node)
    {
        NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
        node_proxy->m_Scene = scene;
        node_proxy->m_Node = node;
        luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
        lua_setmetatable(L, -2);
    }

    static int LuaDoNewNode(lua_State* L, Scene* scene, Point3 pos, Vector3 size, NodeType node_type, uint32_t custom_type, const char* text, void* font)
    {
        DM_LUA_STACK_CHECK(L, 1);

        HNode node = dmGui::NewNode(scene, pos, size, node_type, custom_type);
        if (!node)
        {
            return DM_LUA_ERROR("Out of nodes (max %d)", scene->m_Nodes.Capacity());
        }
        GetNode(scene, node)->m_Node.m_Font = font;
        SetNodeText(scene, node, text);
        LuaPushNode(L, scene, node);
        return 1;
    }

    static inline Point3 GetPositionFromArgumentIndex(lua_State* L, int index)
    {
        Vector4* v4;
        if ((v4 = dmScript::ToVector4(L, index)))
        {
            return Point3(v4->getXYZ());
        }

        Vector3* v3 = dmScript::CheckVector3(L, index);
        return Point3(*v3);
    }

    /*# creates a new box node
     * Dynamically create a new box node.
     *
     * @name gui.new_box_node
     * @param pos [type:vector3|vector4] node position
     * @param size [type:vector3] node size
     * @return node [type:node] new box node
     */
    static int LuaNewBoxNode(lua_State* L)
    {
        Point3 pos = GetPositionFromArgumentIndex(L, 1);
        Vector3 size = *dmScript::CheckVector3(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        return LuaDoNewNode(L, scene, pos, size, NODE_TYPE_BOX, 0, 0, 0x0);
    }

    /*# creates a new text node
     * Dynamically create a new text node.
     *
     * @name gui.new_text_node
     * @param pos [type:vector3|vector4] node position
     * @param text [type:string] node text
     * @return node [type:node] new text node
     */
    static int LuaNewTextNode(lua_State* L)
    {
        Point3 pos = GetPositionFromArgumentIndex(L, 1);
        const char* text = luaL_checkstring(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        void* font = scene->m_DefaultFont;
        if (font == 0x0)
            font = scene->m_Context->m_DefaultFont;
        Vector3 size = Vector3(1,1,1);
        if (font != 0x0)
        {
            dmGui::TextMetrics metrics;
            scene->m_Context->m_GetTextMetricsCallback(font, text, 0.0f, false, 1, 0, &metrics);
            size.setX(metrics.m_Width);
            size.setY(metrics.m_MaxAscent + metrics.m_MaxDescent);
        }

        return LuaDoNewNode(L, scene, pos, size, NODE_TYPE_TEXT, 0, text, font);
    }

    /*# creates a new pie node
     * Dynamically create a new pie node.
     *
     * @name gui.new_pie_node
     * @param pos [type:vector3|vector4] node position
     * @param size [type:vector3] node size
     * @return node [type:node] new pie node
     */
    static int LuaNewPieNode(lua_State* L)
    {
        Point3 pos = GetPositionFromArgumentIndex(L, 1);
        Vector3 size = *dmScript::CheckVector3(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        return LuaDoNewNode(L, scene, pos, size, NODE_TYPE_PIE, 0, 0, 0x0);
    }

    /*# gets the node text
     * Returns the text value of a text node. This is only useful for text nodes.
     *
     * @name gui.get_text
     * @param node [type:node] node from which to get the text
     * @return text [type:string] text value
     */
    static int LuaGetText(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushstring(L, n->m_Node.m_Text);
        return 1;
    }

    /*# sets the node text
     * Set the text value of a text node. This is only useful for text nodes.
     *
     * @name gui.set_text
     * @param node [type:node] node to set text for
     * @param text [type:string] text to set
     */
    static int LuaSetText(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        const char* text = luaL_checkstring(L, 2);
        if (n->m_Node.m_Text)
            free((void*) n->m_Node.m_Text);
        n->m_Node.m_Text = strdup(text);
        return 0;
    }

    /*# get line-break mode
     * Returns whether a text node is in line-break mode or not.
     * This is only useful for text nodes.
     *
     * @name gui.get_line_break
     * @param node [type:node] node from which to get the line-break for
     * @return line_break [type:boolean] `true` or `false`
     */
    static int LuaGetLineBreak(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushboolean(L, n->m_Node.m_LineBreak);
        return 1;
    }

    /*# set line-break mode
     * Sets the line-break mode on a text node.
     * This is only useful for text nodes.
     *
     * @name gui.set_line_break
     * @param node [type:node] node to set line-break for
     * @param line_break [type:boolean] true or false
     */
    static int LuaSetLineBreak(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        bool line_break = (bool) lua_toboolean(L, 2);
        n->m_Node.m_LineBreak = line_break;
        return 0;
    }

    /*# gets the node blend mode
     * Returns the blend mode of a node.
     * Blend mode defines how the node will be blended with the background.
     *
     * @name gui.get_blend_mode
     * @param node [type:node] node from which to get the blend mode
     * @return blend_mode [type:constant] blend mode
     *
     * - `gui.BLEND_ALPHA`
     * - `gui.BLEND_ADD`
     * - `gui.BLEND_ADD_ALPHA`
     * - `gui.BLEND_MULT`
     */
    static int LuaGetBlendMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_BlendMode);
        return 1;
    }

    /*# sets node blend mode
     * Set the blend mode of a node.
     * Blend mode defines how the node will be blended with the background.
     *
     * @name gui.set_blend_mode
     * @param node [type:node] node to set blend mode for
     * @param blend_mode [type:constant] blend mode to set
     *
     * - `gui.BLEND_ALPHA`
     * - `gui.BLEND_ADD`
     * - `gui.BLEND_ADD_ALPHA`
     * - `gui.BLEND_MULT`
     */
    static int LuaSetBlendMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int blend_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_BlendMode = (BlendMode) blend_mode;
        return 0;
    }

    /*# gets node texture
     * Returns the texture of a node.
     * This is currently only useful for box or pie nodes.
     * The texture must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_texture
     * @param node [type:node] node to get texture from
     * @return texture [type:hash] texture id
     */
    static int LuaGetTexture(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeTextureId(scene, hnode));
        return 1;
    }

    /*# sets the node texture
     * Set the texture on a box or pie node. The texture must be mapped to
     * the gui scene in the gui editor. The function points out which texture
     * the node should render from. If the texture is an atlas, further
     * information is needed to select which image/animation in the atlas
     * to render. In such cases, use <code>gui.play_flipbook()</code> in
     * addition to this function.
     *
     * @name gui.set_texture
     * @param node [type:node] node to set texture for
     * @param texture [type:string|hash] texture id
     * @examples
     *
     * To set a texture (or animation) from an atlas:
     *
     * ```lua
     * local node = gui.get_node("box_node")
     * gui.set_texture(node, "my_atlas")
     * gui.play_flipbook(node, "image")
     * ```
     *
     * Set a dynamically created texture to a node. Note that there is only
     * one texture image in this case so <code>gui.set_texture()</code> is
     * sufficient.
     *
     * ```lua
     * local w = 200
     * local h = 300
     * -- A nice orange. String with the RGB values.
     * local orange = string.char(0xff) .. string.char(0x80) .. string.char(0x10)
     * -- Create the texture. Repeat the color string for each pixel.
     * if gui.new_texture("orange_tx", w, h, "rgb", string.rep(orange, w * h)) then
     *     local node = gui.get_node("box_node")
     *     gui.set_texture(node, "orange_tx")
     * end
     * ```
     */
    static int LuaSetTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;
        if (lua_isstring(L, 2))
        {
            const char* texture_id = luaL_checkstring(L, 2);

            Result r = SetNodeTexture(scene, hnode, texture_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Texture '%s' is not specified in scene", texture_id);
            }
        }
        else
        {
            dmhash_t texture_id = dmScript::CheckHash(L, 2);

            Result r = SetNodeTexture(scene, hnode, texture_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Texture '%s' is not specified in scene", dmHashReverseSafe64(texture_id));
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the node flipbook animation
     * Get node flipbook animation.
     *
     * @name gui.get_flipbook
     * @param node [type:node] node to get flipbook animation from
     * @return animation [type:hash] animation id
     */
    static int LuaGetFlipbook(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeFlipbookAnimId(scene, hnode));
        return 1;
    }

    /*# play node flipbook animation
     * Play flipbook animation on a box or pie node.
     * The current node texture must contain the animation.
     * Use this function to set one-frame still images on the node.
     *
     * @name gui.play_flipbook
     * @param node [type:node] node to set animation for
     * @param animation [type:string|hash] animation id
     * @param [complete_function] [type:function(self, node)] optional function to call when the animation has completed
     *
     * `self`
     * :        [type:object] The current object.
     *
     * `node`
     * :        [type:node] The node that is animated.
     *
     * @param [play_properties] [type:table] optional table with properties
     *
     * `offset`
     * : [type:number] The normalized initial value of the animation cursor when the animation starts playing
     *
     * `playback_rate`
     * : [type:number] The rate with which the animation will be played. Must be positive
     *
     * @examples
     *
     * Set the texture of a node to a flipbook animation from an atlas:
     *
     * ```lua
     * local function anim_callback(self, node)
     *     -- Take action after animation has played.
     * end
     *
     * function init(self)
     *     -- Create a new node and set the texture to a flipbook animation
     *     local node = gui.get_node("button_node")
     *     gui.set_texture(node, "gui_sprites")
     *     gui.play_flipbook(node, "animated_button")
     * end
     * ```
     *
     * Set the texture of a node to an image from an atlas:
     *
     * ```lua
     * -- Create a new node and set the texture to a "button.png" from atlas
     * local node = gui.get_node("button_node")
     * gui.set_texture(node, "gui_sprites")
     * gui.play_flipbook(node, "button")
     * ```
     */
    static int LuaPlayFlipbook(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        int node_ref = LUA_NOREF;
        dmScript::LuaCallbackInfo* cbk = 0x0;
        if (lua_isfunction(L, 3))
        {
            cbk = dmScript::CreateCallback(L, 3);
            lua_rawgeti(L, LUA_REGISTRYINDEX, scene->m_ContextTableReference);
            lua_pushvalue(L, 1);
            node_ref = luaL_ref(L, -2);
            lua_pop(L, 1);
        }

        lua_Number offset = 0.0, playback_rate = 1.0;

        if (top > 3) // table with args
        {
            luaL_checktype(L, 4, LUA_TTABLE);
            lua_pushvalue(L, 4);

            lua_getfield(L, -1, "offset");
            offset = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "playback_rate");
            playback_rate = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        dmhash_t anim_id = dmScript::CheckHashOrString(L, 2);
        Result r;
        if(cbk != 0x0)
            r = PlayNodeFlipbookAnim(scene, hnode, anim_id, offset, playback_rate, &LuaAnimationComplete, cbk, (void*)(uintptr_t) node_ref);
        else
            r = PlayNodeFlipbookAnim(scene, hnode, anim_id, offset, playback_rate);
        if (r != RESULT_OK)
        {
            luaL_error(L, "Animation '%s' invalid for node '%s' (no animation set)", dmHashReverseSafe64(anim_id), dmHashReverseSafe64(n->m_NameHash));
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# cancel a node flipbook animation
     * Cancels any running flipbook animation on the specified node.
     *
     * @name gui.cancel_flipbook
     * @param node [type:node] node cancel flipbook animation for
     * @examples
     *
     * ```lua
     * local node = gui.get_node("anim_node")
     * gui.cancel_flipbook(node)
     * ```
     */
    static int LuaCancelFlipbook(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * @param texture [type:string|hash] texture id
     * @param width [type:number] texture width
     * @param height [type:number] texture height
     * @param type [type:string|constant] texture type
     *
     * - `"rgb"` - RGB</li>
     * - `"rgba"` - RGBA</li>
     * - `"l"` - LUMINANCE</li>
     *
     * @param buffer [type:string] texture data
     * @param flip [type:boolean] flip texture vertically
     * @return success [type:boolean] texture creation was successful
     * @return code [type:number] one of the gui.RESULT_* codes if unsuccessful
     * @examples
     *
     * How to create a texture and apply it to a new box node:
     *
     * ```lua
     * function init(self)
     *      local w = 200
     *      local h = 300
     *
     *      -- A nice orange. String with the RGB values.
     *      local orange = string.char(0xff) .. string.char(0x80) .. string.char(0x10)
     *
     *      -- Create the texture. Repeat the color string for each pixel.
     *      local ok, reason = gui.new_texture("orange_tx", w, h, "rgb", string.rep(orange, w * h))
     *      if ok then
     *          -- Create a box node and apply the texture to it.
     *          local n = gui.new_box_node(vmath.vector3(200, 200, 0), vmath.vector3(w, h, 0))
     *          gui.set_texture(n, "orange_tx")
     *      else
     *          -- Could not create texture for some reason...
     *          if reason == gui.RESULT_TEXTURE_ALREADY_EXISTS then
     *              ...
     *          else
     *              ...
     *          end
     *      end
     * end
     * ```
     */
    static int LuaNewTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const dmhash_t name = dmScript::CheckHashOrString(L, 1);

        int width = luaL_checkinteger(L, 2);
        int height = luaL_checkinteger(L, 3);
        const char* type_str = luaL_checkstring(L, 4);
        size_t buffer_size;
        luaL_checktype(L, 5, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 5, &buffer_size);
        Scene* scene = GuiScriptInstance_Check(L);

        bool flip = false;
        if (top > 5) {
            luaL_checktype(L, 6, LUA_TBOOLEAN);
            flip = (bool)lua_toboolean(L, 6);
        }

        // If we don't flip the image, it will appear upside down using OGL texture coords,
        // since we will upload the data top-row first instead of bottom-row first.
        // This is actually what users expect the flip switch to mean, so we invert the
        // flip here so the default case will be to "correctly" flip the image.
        flip = !flip;

        dmImage::Type type = ToImageType(L, type_str);
        Result r = NewDynamicTexture(scene, name, width, height, type, flip, buffer, buffer_size);
        if (r == RESULT_OK) {
            lua_pushboolean(L, 1);
            lua_pushnil(L);
        } else {
            lua_pushboolean(L, 0);
            lua_pushnumber(L, r);
        }
        assert(top + 2 == lua_gettop(L));
        return 2;
    }

    /*# delete texture
     * Delete a dynamically created texture.
     *
     * @name gui.delete_texture
     * @param texture [type:string|hash] texture id
     * @examples
     *
     * ```lua
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
     * ```
     */
    static int LuaDeleteTexture(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const dmhash_t name = dmScript::CheckHashOrString(L, 1);

        Scene* scene = GuiScriptInstance_Check(L);

        Result r = DeleteDynamicTexture(scene, name);
        if (r != RESULT_OK) {
            char buffer[128];
            luaL_error(L, "failed to delete texture '%s' (result = %d)", dmScript::GetStringFromHashOrString(L, 1, buffer, sizeof(buffer)), r);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set the buffer data for a texture
     * Set the texture buffer data for a dynamically created texture.
     *
     * @name gui.set_texture_data
     * @param texture [type:string|hash] texture id
     * @param width [type:number] texture width
     * @param height [type:number] texture height
     * @param type [type:string|constant] texture type
     * <ul>
     *   <li><code>"rgb"</code> - RGB</li>
     *   <li><code>"rgba"</code> - RGBA</li>
     *   <li><code>"l"</code> - LUMINANCE</li>
     * </ul>
     * @param buffer [type:string] texture data
     * @param flip [type:boolean] flip texture vertically
     * @return success [type:boolean] setting the data was successful
     * @examples
     *
     * ```lua
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
     * ```
     */
    static int LuaSetTextureData(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        const dmhash_t name = dmScript::CheckHashOrString(L, 1);
        int width = luaL_checkinteger(L, 2);
        int height = luaL_checkinteger(L, 3);
        const char* type_str = luaL_checkstring(L, 4);
        size_t buffer_size;
        luaL_checktype(L, 5, LUA_TSTRING);
        const char* buffer = lua_tolstring(L, 5, &buffer_size);
        Scene* scene = GuiScriptInstance_Check(L);

        bool flip = false;
        if (top > 5) {
            luaL_checktype(L, 6, LUA_TBOOLEAN);
            flip = (bool)lua_toboolean(L, 6);
        }

        // If we don't flip the image, it will appear upside down using OGL texture coords,
        // since we will upload the data top-row first instead of bottom-row first.
        // This is actually what users expect the flip switch to mean, so we invert the
        // flip here so the default case will be to "correctly" flip the image.
        flip = !flip;

        dmImage::Type type = ToImageType(L, type_str);
        Result r = SetDynamicTextureData(scene, name, width, height, type, flip, buffer, buffer_size);
        if (r == RESULT_OK) {
            lua_pushboolean(L, 1);
        } else {
            dmLogWarning("Failed to set texture data (%d)", r);
            lua_pushboolean(L, 0);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# gets the assigned node material
     * Returns the material of a node.
     * The material must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_material
     * @param node [type:node] node to get the material for
     * @examples
     *
     * Getting the material for a node, and assign it to another node:
     *
     * ```lua
     * local node1 = gui.get_node("my_node")
     * local node2 = gui.get_node("other_node")
     * local node1_material = gui.get_material(node1)
     * gui.set_material(node2, node1_material)
     * ```
     */
    static int LuaGetMaterial(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeMaterialId(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the node material
     * Set the material on a node. The material must be mapped to the gui scene in the gui editor, 
     * and assigning a material is supported for all node types. To set the default material that
     * is assigned to the gui scene node, use `gui.reset_material(node_id)` instead.
     *
     * @name gui.set_material
     * @param node [type:node] node to set material for
     * @param material [type:string|hash] material id
     * @examples
     *
     * Assign an existing material to a node:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     * gui.set_material(node, "my_material")
     * ```
     */
    static int LuaSetMaterial(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmhash_t material_id = dmScript::CheckHashOrString(L, 2);

        if (SetNodeMaterial(scene, hnode, material_id) != RESULT_OK)
        {
            luaL_error(L, "Material '%s' is not specified in scene", dmHashReverseSafe64(material_id));
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# resets the node material
     * Resets the node material to the material assigned in the gui scene.
     *
     * @name gui.set_material
     * @param node [type:node] node to reset the material for
     * @examples
     *
     * Resetting the material for a node:
     *
     * ```lua
     * local node = gui.get_node("my_node")
     * gui.reset_material(node)
     * ```
     */
    static int LuaResetMaterial(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        SetNodeMaterial(scene, hnode, (dmhash_t) 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the node font
     * This is only useful for text nodes. The font must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_font
     * @param node [type:node] node from which to get the font
     * @return font [type:hash] font id
     */
    static int LuaGetFont(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeFontId(scene, hnode));
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# gets the node font resource
     * This is only useful for text nodes. The font must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_font_resource
     * @param font_name [type:hash|string] font of which to get the path hash
     * @return hash [type:hash] path hash to resource
     * @examples
     *
     * Get the text metrics for a text
     *
     * ```lua
     * function init(self)
     *   local node = gui.get_node("name")
     *   local font_name = gui.get_font(node)
     *   local font = gui.get_font_resource(font_name)
     *   local metrics = resource.get_text_metrics(font, "The quick brown fox\n jumps over the lazy dog")
     * end
     * ```
     */
    static int LuaGetFontResource(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        dmhash_t font_id_hash = dmScript::CheckHashOrString(L, 1);

        dmhash_t path_hash = dmGui::GetFontPath(scene, font_id_hash);
        if (path_hash) {
            dmScript::PushHash(L, path_hash);
        } else {
            return luaL_error(L, "Failed to get path hash for resource %s", dmHashReverseSafe64(font_id_hash));
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }


    /*# sets the node font
     * This is only useful for text nodes.
     * The font must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_font
     * @param node [type:node] node for which to set the font
     * @param font [type:string|hash] font id
     */
    static int LuaSetFont(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        if (lua_isstring(L, 2))
        {
            const char* font_id = luaL_checkstring(L, 2);

            Result r = SetNodeFont(scene, hnode, font_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Font '%s' is not specified in scene", font_id);
            }
        }
        else
        {
            dmhash_t font_id = dmScript::CheckHash(L, 2);
            Result r = SetNodeFont(scene, hnode, font_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Font '%s' is not specified in scene", dmHashReverseSafe64(font_id));
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the node layer
     * The layer must be mapped to the gui scene in the gui editor.
     *
     * @name gui.get_layer
     * @param node [type:node] node from which to get the layer
     * @return layer [type:hash] layer id
     */
    static int LuaGetLayer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmScript::PushHash(L, dmGui::GetNodeLayerId(scene, hnode));
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the node layer
     * The layer must be mapped to the gui scene in the gui editor.
     *
     * @name gui.set_layer
     * @param node [type:node] node for which to set the layer
     * @param layer [type:string|hash] layer id
     */
    static int LuaSetLayer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        if (lua_isstring(L, 2))
        {
            const char* layer_id = luaL_checkstring(L, 2);

            Result r = SetNodeLayer(scene, hnode, layer_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Layer '%s' is not specified in scene", layer_id);
            }
        }
        else
        {
            dmhash_t layer_id = dmScript::CheckHash(L, 2);
            Result r = SetNodeLayer(scene, hnode, layer_id);
            if (r != RESULT_OK)
            {
                luaL_error(L, "Layer '%s' is not specified in scene", dmHashReverseSafe64(layer_id));
            }
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the scene current layout
     *
     * @name gui.get_layout
     * @return layout [type:hash] layout id
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
     * Clipping mode defines how the node will clip it's children nodes
     *
     * @name gui.get_clipping_mode
     * @param node [type:node] node from which to get the clipping mode
     * @return clipping_mode [type:constant] clipping mode
     * <ul>
     *   <li><code>gui.CLIPPING_MODE_NONE</code></li>
     *   <li><code>gui.CLIPPING_MODE_STENCIL</code></li>
     * </ul>
     */
    static int LuaGetClippingMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_ClippingMode);
        return 1;
    }

    /*# sets node clipping mode state
     * Clipping mode defines how the node will clip it's children nodes
     *
     * @name gui.set_clipping_mode
     * @param node [type:node] node to set clipping mode for
     * @param clipping_mode [type:constant] clipping mode to set
     * <ul>
     *   <li><code>gui.CLIPPING_MODE_NONE</code></li>
     *   <li><code>gui.CLIPPING_MODE_STENCIL</code></li>
     * </ul>
     */
    static int LuaSetClippingMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int clipping_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_ClippingMode = (ClippingMode) clipping_mode;
        return 0;
    }

    /*# gets node clipping visibility state
     * If node is set as visible clipping node, it will be shown as well as clipping. Otherwise, it will only clip but not show visually.
     *
     * @name gui.get_clipping_visible
     * @param node [type:node] node from which to get the clipping visibility state
     * @return visible [type:boolean] true or false
     */
    static int LuaGetClippingVisible(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        lua_pushboolean(L, n->m_Node.m_ClippingVisible);
        return 1;
    }

    /*# sets node clipping visibility
     * If node is set as an visible clipping node, it will be shown as well as clipping. Otherwise, it will only clip but not show visually.
     *
     * @name gui.set_clipping_visible
     * @param node [type:node] node to set clipping visibility for
     * @param visible [type:boolean] true or false
     */
    static int LuaSetClippingVisible(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int visible = lua_toboolean(L, 2);
        n->m_Node.m_ClippingVisible = visible;
        return 0;
    }

    /*# gets node clipping inverted state
     * If node is set as an inverted clipping node, it will clip anything inside as opposed to outside.
     *
     * @name gui.get_clipping_inverted
     * @param node [type:node] node from which to get the clipping inverted state
     * @return inverted [type:boolean] true or false
     */
    static int LuaGetClippingInverted(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        lua_pushboolean(L, n->m_Node.m_ClippingInverted);
        return 1;
    }

    /*# sets node clipping inversion
     * If node is set as an inverted clipping node, it will clip anything inside as opposed to outside.
     *
     * @name gui.set_clipping_inverted
     * @param node [type:node] node to set clipping inverted state for
     * @param inverted [type:boolean] true or false
     */
    static int LuaSetClippingInverted(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int inverted = lua_toboolean(L, 2);
        n->m_Node.m_ClippingInverted = inverted;
        return 0;
    }

    static void PushTextMetrics(lua_State* L, Scene* scene, dmhash_t font_id_hash, const char* text, float width, bool line_break, float leading, float tracking)
    {
        dmGui::TextMetrics metrics;
        dmGui::Result r = dmGui::GetTextMetrics(scene, text, font_id_hash, width, line_break, leading, tracking, &metrics);
        if (r != RESULT_OK) {
            luaL_error(L, "Font '%s' is not specified in scene", dmHashReverseSafe64(font_id_hash));
        }

        lua_createtable(L, 0, 4);
        lua_pushliteral(L, "width");
        lua_pushnumber(L, metrics.m_Width);
        lua_rawset(L, -3);
        lua_pushliteral(L, "height");
        lua_pushnumber(L, metrics.m_Height);
        lua_rawset(L, -3);
        lua_pushliteral(L, "max_ascent");
        lua_pushnumber(L, metrics.m_MaxAscent);
        lua_rawset(L, -3);
        lua_pushliteral(L, "max_descent");
        lua_pushnumber(L, metrics.m_MaxDescent);
        lua_rawset(L, -3);
    }

    /* DEPRECATED in favor of resource.get_text_metrics
     */
    static int LuaGetTextMetricsFromNode(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void)n;

        dmhash_t font_id_hash = dmGui::GetNodeFontId(scene, hnode);
        const char* text = dmGui::GetNodeText(scene, hnode);
        float width = dmGui::GetNodeProperty(scene, hnode, PROPERTY_SIZE).getX();
        bool line_break = dmGui::GetNodeLineBreak(scene, hnode);
        float leading = dmGui::GetNodeTextLeading(scene, hnode);
        float tracking = dmGui::GetNodeTextTracking(scene, hnode);
        PushTextMetrics(L, scene, font_id_hash, text, width, line_break, leading, tracking);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static inline float LuaUtilGetDefaultFloat(lua_State* L, int index, float defaultvalue)
    {
        if( lua_isnoneornil(L, index) )
        {
            return defaultvalue;
        }
        return (float) luaL_checknumber(L, index);
    }

    static inline bool LuaUtilGetDefaultBool(lua_State* L, int index, bool defaultvalue)
    {
        if( lua_isnoneornil(L, index) )
        {
            return defaultvalue;
        }
        return lua_toboolean(L, index);
    }

    /* DEPRECATED in favor of resource.get_text_metrics
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

        float width     = LuaUtilGetDefaultFloat(L, 3, FLT_MAX);
        bool line_break = LuaUtilGetDefaultBool(L, 4, false);
        float leading   = LuaUtilGetDefaultFloat(L, 5, 1.0f);
        float tracking  = LuaUtilGetDefaultFloat(L, 6, 0.0f);
        PushTextMetrics(L, scene, font_id_hash, text, width, line_break, leading, tracking);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# gets the x-anchor of a node
     * The x-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.get_xanchor
     * @param node [type:node] node to get x-anchor from
     * @return anchor [type:constant] anchor constant
     *
     * - `gui.ANCHOR_NONE`
     * - `gui.ANCHOR_LEFT`
     * - `gui.ANCHOR_RIGHT`
     */
    static int LuaGetXAnchor(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, GetNodeXAnchor(scene, hnode));

        return 1;
    }

    /*# sets the x-anchor of a node
     * The x-anchor specifies how the node is moved when the game is run in a different resolution.
     *
     * @name gui.set_xanchor
     * @param node [type:node] node to set x-anchor for
     * @param anchor [type:constant] anchor constant
     *
     * - `gui.ANCHOR_NONE`
     * - `gui.ANCHOR_LEFT`
     * - `gui.ANCHOR_RIGHT`
     */
    static int LuaSetXAnchor(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * @param node [type:node] node to get y-anchor from
     * @return anchor [type:constant] anchor constant
     *
     * - `gui.ANCHOR_NONE`
     * - `gui.ANCHOR_TOP`
     * - `gui.ANCHOR_BOTTOM`
     */
    static int LuaGetYAnchor(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * @param node [type:node] node to set y-anchor for
     * @param anchor [type:constant] anchor constant
     *
     * - `gui.ANCHOR_NONE`
     * - `gui.ANCHOR_TOP`
     * - `gui.ANCHOR_BOTTOM`
     */
    static int LuaSetYAnchor(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * @param node [type:node] node to get pivot from
     * @return pivot [type:constant] pivot constant
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
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodePivot(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pivot of a node
     * The pivot specifies how the node is drawn and rotated from its position.
     *
     * @name gui.set_pivot
     * @param node [type:node] node to set pivot for
     * @param pivot [type:constant] pivot constant
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
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * Returns the scene width.
     *
     * @name gui.get_width
     * @return width [type:number] scene width
     */
    static int LuaGetWidth(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, scene->m_Width);
        return 1;
    }

    /*# gets the scene height
     * Returns the scene height.
     *
     * @name gui.get_height
     * @return height [type:number] scene height
     */
    static int LuaGetHeight(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushnumber(L, scene->m_Height);
        return 1;
    }

    /*# set the slice9 configuration for the node
     * Set the slice9 configuration values for the node.
     *
     * @name gui.set_slice9
     * @param node [type:node] node to manipulate
     * @param values [type:vector4] new values
     */
    static int LuaSetSlice9(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        Vector4* v4;
        if ((v4 = dmScript::ToVector4(L, 2)))
        {
            Scene* scene = GuiScriptInstance_Check(L);
            dmGui::SetNodeProperty(scene, hnode, dmGui::PROPERTY_SLICE9, *v4);
        }
        else
        {
            luaL_error(L, "invalid parameter given");
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# get the slice9 values for the node
     * Returns the slice9 configuration values for the node.
     *
     * @name gui.get_slice9
     * @param node [type:node] node to manipulate
     * @return values [type:vector4] configuration values
     */
    static int LuaGetSlice9(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);
        dmScript::PushVector4(L, dmGui::GetNodeProperty(scene, hnode, dmGui::PROPERTY_SLICE9));
        return 1;
    }

    /*# sets the number of generated vertices around the perimeter
     * Sets the number of generated vertices around the perimeter of a pie node.
     *
     * @name gui.set_perimeter_vertices
     * @param node [type:node] pie node
     * @param vertices [type:number] vertex count
     */
    static int LuaSetPerimeterVertices(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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

    /*# gets the number of generated vertices around the perimeter
     * Returns the number of generated vertices around the perimeter
     * of a pie node.
     *
     * @name gui.get_perimeter_vertices
     * @param node [type:node] pie node
     * @return vertices [type:number] vertex count
     */
    static int LuaGetPerimeterVertices(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushinteger(L, dmGui::GetNodePerimeterVertices(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the angle for the filled pie sector
     * Set the sector angle of a pie node.
     *
     * @name gui.set_fill_angle
     * @param node [type:node] node to set the fill angle for
     * @param angle [type:number] sector angle
     */
    static int LuaSetPieFillAngle(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * Returns the sector angle of a pie node.
     *
     * @name gui.get_fill_angle
     * @param node [type:node] node from which to get the fill angle
     * @return angle [type:number] sector angle
     */
    static int LuaGetPieFillAngle(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodePieFillAngle(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pie inner radius
     * Sets the inner radius of a pie node.
     * The radius is defined along the x-axis.
     *
     * @name gui.set_inner_radius
     * @param node [type:node] node to set the inner radius for
     * @param radius [type:number] inner radius
     */
    static int LuaSetInnerRadius(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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

    /*# gets the pie inner radius
     * Returns the inner radius of a pie node.
     * The radius is defined along the x-axis.
     *
     * @name gui.get_inner_radius
     * @param node [type:node] node from where to get the inner radius
     * @return radius [type:number] inner radius
     */
    static int LuaGetInnerRadius(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, dmGui::GetNodeInnerRadius(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the pie node outer bounds mode
     * Sets the outer bounds mode for a pie node.
     *
     * @name gui.set_outer_bounds
     * @param node [type:node] node for which to set the outer bounds mode
     * @param bounds_mode [type:constant] the outer bounds mode of the pie node:
     *
     * - `gui.PIEBOUNDS_RECTANGLE`
     * - `gui.PIEBOUNDS_ELLIPSE`
     */
    static int LuaSetOuterBounds(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
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
     * Returns the outer bounds mode for a pie node.
     *
     * @name gui.get_outer_bounds
     * @param node [type:node] node from where to get the outer bounds mode
     * @return bounds_mode [type:constant] the outer bounds mode of the pie node:
     *
     * - `gui.PIEBOUNDS_RECTANGLE`
     * - `gui.PIEBOUNDS_ELLIPSE`
     */
    static int LuaGetOuterBounds(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushinteger(L, dmGui::GetNodeOuterBounds(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the leading of the text node
     * Sets the leading value for a text node. This value is used to
     * scale the line spacing of text.
     *
     * @name gui.set_leading
     * @param node [type:node] node for which to set the leading
     * @param leading [type:number] a scaling value for the line spacing (default=1)
     */
    static int LuaSetLeading(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_Number leading = luaL_checknumber(L, 2);

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodeTextLeading(scene, hnode, leading);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the leading of the text node
     * Returns the leading value for a text node.
     *
     * @name gui.get_leading
     * @param node [type:node] node from where to get the leading
     * @return leading [type:number] leading scaling value (default=1)
     */
    static int LuaGetLeading(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, (lua_Number) dmGui::GetNodeTextLeading(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the tracking of the text node
     * Sets the tracking value of a text node. This value is used to
     * adjust the vertical spacing of characters in the text.
     *
     * @name gui.set_tracking
     * @param node [type:node] node for which to set the tracking
     * @param tracking [type:number] a scaling number for the letter spacing (default=0)
     */
    static int LuaSetTracking(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_Number tracking = luaL_checknumber(L, 2);

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodeTextTracking(scene, hnode, tracking);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the tracking of the text node
     * Returns the tracking value of a text node.
     *
     * @name gui.get_tracking
     * @param node [type:node] node from where to get the tracking
     * @return tracking [type:number] tracking scaling number (default=0)
     */
    static int LuaGetTracking(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, (lua_Number) dmGui::GetNodeTextTracking(scene, hnode));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# determines if the node is pickable by the supplied coordinates
     * Tests whether a coordinate is within the bounding box of a
     * node.
     *
     * @name gui.pick_node
     * @param node [type:node] node to be tested for picking
     * @param x [type:number] x-coordinate (see <a href="#on_input">on_input</a> )
     * @param y [type:number] y-coordinate (see <a href="#on_input">on_input</a> )
     * @return pickable [type:boolean] pick result
     */
    static int LuaPickNode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_Number x = luaL_checknumber(L, 2);
        lua_Number y = luaL_checknumber(L, 3);

        Scene* scene = GuiScriptInstance_Check(L);

        lua_pushboolean(L, PickNode(scene, hnode, (float) x, (float) y));
        return 1;
    }

    /*# returns if a node is enabled or not
     * Returns `true` if a node is enabled and `false` if it's not.
     * Disabled nodes are not rendered and animations acting on them are not evaluated.
     *
     * @name gui.is_enabled
     * @param node [type:node] node to query
     * @param recursive [type:boolean] check hierarchy recursively
     * @return enabled [type:boolean] whether the node is enabled or not
     */
    static int LuaIsEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);
        int top = lua_gettop(L);
        bool recursive = top >= 2 && lua_toboolean(L, 2);
        lua_pushboolean(L, dmGui::IsNodeEnabled(scene, hnode, recursive));

        return 1;
    }

    /*# enables/disables a node
     * Sets a node to the disabled or enabled state.
     * Disabled nodes are not rendered and animations acting on them are not evaluated.
     *
     * @name gui.set_enabled
     * @param node [type:node] node to be enabled/disabled
     * @param enabled [type:boolean] whether the node should be enabled or not
     */
    static int LuaSetEnabled(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        int enabled = lua_toboolean(L, 2);

        Scene* scene = GuiScriptInstance_Check(L);

        dmGui::SetNodeEnabled(scene, hnode, enabled != 0);

        return 0;
    }

   /*# returns if a node is visible or not
     * Returns `true` if a node is visible and `false` if it's not.
     * Invisible nodes are not rendered.
     *
     * @name gui.get_visible
     * @param node [type:node] node to query
     * @return visible [type:boolean] whether the node is visible or not
     */
    static int LuaGetVisible(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        Scene* scene = GuiScriptInstance_Check(L);
        lua_pushboolean(L, dmGui::GetNodeVisible(scene, hnode));

        return 1;
    }

    /*# set visibility for a node
     * Set if a node should be visible or not. Only visible nodes are rendered.
     *
     * @name gui.set_visible
     * @param node [type:node] node to be visible or not
     * @param visible [type:boolean] whether the node should be visible or not
     */
    static int LuaSetVisible(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        int visible = lua_toboolean(L, 2);

        Scene* scene = GuiScriptInstance_Check(L);

        dmGui::SetNodeVisible(scene, hnode, visible != 0);

        return 0;
    }

    /*# gets the node adjust mode
     * Returns the adjust mode of a node.
     * The adjust mode defines how the node will adjust itself to screen
     * resolutions that differs from the one in the project settings.
     *
     * @name gui.get_adjust_mode
     * @param node [type:node] node from which to get the adjust mode (node)
     * @return adjust_mode [type:constant] the current adjust mode
     *
     * - `gui.ADJUST_FIT`
     * - `gui.ADJUST_ZOOM`
     * - `gui.ADJUST_STRETCH`
     */
    static int LuaGetAdjustMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_AdjustMode);
        return 1;
    }

    /*# sets node adjust mode
     * Sets the adjust mode on a node.
     * The adjust mode defines how the node will adjust itself to screen
     * resolutions that differs from the one in the project settings.
     *
     * @name gui.set_adjust_mode
     * @param node [type:node] node to set adjust mode for
     * @param adjust_mode [type:constant] adjust mode to set
     *
     * - `gui.ADJUST_FIT`
     * - `gui.ADJUST_ZOOM`
     * - `gui.ADJUST_STRETCH`
     */
    static int LuaSetAdjustMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int adjust_mode = (int) luaL_checknumber(L, 2);
        n->m_Node.m_AdjustMode = (AdjustMode) adjust_mode;
        return 0;
    }

    /*# gets the node size mode
     * Returns the size of a node.
     * The size mode defines how the node will adjust itself in size. Automatic
     * size mode alters the node size based on the node's content. Automatic size
     * mode works for Box nodes and Pie nodes which will both adjust their size
     * to match the assigned image. Particle fx and Text nodes will ignore
     * any size mode setting.
     *
     * @name gui.get_size_mode
     * @param node [type:node] node from which to get the size mode (node)
     * @return size_mode [type:constant] the current size mode
     *
     *   - `gui.SIZE_MODE_MANUAL`
     *   - `gui.SIZE_MODE_AUTO`
     */
    static int LuaGetSizeMode(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        lua_pushnumber(L, (lua_Number) n->m_Node.m_SizeMode);
        return 1;
    }

    /*# sets node size mode
     * Sets the size mode of a node.
     * The size mode defines how the node will adjust itself in size. Automatic
     * size mode alters the node size based on the node's content. Automatic size
     * mode works for Box nodes and Pie nodes which will both adjust their size
     * to match the assigned image. Particle fx and Text nodes will ignore
     * any size mode setting.
     *
     * @name gui.set_size_mode
     * @param node [type:node] node to set size mode for
     * @param size_mode [type:constant] size mode to set
     *
     *   - `gui.SIZE_MODE_MANUAL`
     *   - `gui.SIZE_MODE_AUTO`
     */
    static int LuaSetSizeMode(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int size_mode = (int) luaL_checknumber(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        dmGui::SetNodeSizeMode(scene, GetNodeHandle(n), (SizeMode)size_mode);
        return 0;
    }

    /*# moves the first node above the second
     * Alters the ordering of the two supplied nodes by moving the first node
     * above the second.
     * If the second argument is `nil` the first node is moved to the top.
     *
     * @name gui.move_above
     * @param node [type:node] to move
     * @param node [type:node|nil] reference node above which the first node should be moved
     */
    static int LuaMoveAbove(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        HNode r = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            r = GetNodeHandle(LuaCheckNodeInternal(L, 2, &hnode));
        }
        Scene* scene = GuiScriptInstance_Check(L);
        MoveNodeAbove(scene, GetNodeHandle(n), r);
        return 0;
    }

    /*# moves the first node below the second
     * Alters the ordering of the two supplied nodes by moving the first node
     * below the second.
     * If the second argument is `nil` the first node is moved to the bottom.
     *
     * @name gui.move_below
     * @param node [type:node] to move
     * @param node [type:node|nil] reference node below which the first node should be moved
     */
    static int LuaMoveBelow(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        HNode r = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            r = GetNodeHandle(LuaCheckNodeInternal(L, 2, &hnode));
        }
        Scene* scene = GuiScriptInstance_Check(L);
        MoveNodeBelow(scene, GetNodeHandle(n), r);
        return 0;
    }

    /*# gets the parent of the specified node
     * Returns the parent node of the specified node.
     * If the supplied node does not have a parent, `nil` is returned.
     *
     * @name gui.get_parent
     * @param node [type:node] the node from which to retrieve its parent
     * @return parent [type:node] parent instance or nil
     */
    int LuaGetParent(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGui::HScene scene = dmGui::LuaCheckScene(L);
        dmGui::HNode node = dmGui::LuaCheckNode(L, 1);
        dmGui::HNode parent = dmGui::GetNodeParent(scene, node);

        if (parent != dmGui::INVALID_HANDLE)
        {
            NodeProxy* node_proxy = (NodeProxy *)lua_newuserdata(L, sizeof(NodeProxy));
            node_proxy->m_Scene = scene;
            node_proxy->m_Node = parent;
            luaL_getmetatable(L, NODE_PROXY_TYPE_NAME);
            lua_setmetatable(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }

        return 1;
    }

    /*# sets the parent of the node
     * Sets the parent node of the specified node.
     *
     * @name gui.set_parent
     * @param node [type:node] node for which to set its parent
     * @param parent [type:node] parent node to set
     * @param keep_scene_transform [type:boolean] optional flag to make the scene position being perserved
     */
    static int LuaSetParent(lua_State* L)
    {
        int top = lua_gettop(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        if(n->m_Node.m_IsBone) {
            return 0;
        }
        HNode parent = INVALID_HANDLE;
        if (!lua_isnil(L, 2))
        {
            parent = GetNodeHandle(LuaCheckNodeInternal(L, 2, &hnode));
        }
        bool keep_scene_transform = false;
        if (top > 2 && lua_isboolean(L, 3) && lua_toboolean(L, 3))
        {
            keep_scene_transform = true;
        }
        Scene* scene = GuiScriptInstance_Check(L);
        dmGui::Result result = dmGui::SetNodeParent(scene, GetNodeHandle(n), parent, keep_scene_transform);
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
     * Make a clone instance of a node.
     * This function does not clone the supplied node's children nodes.
     * Use gui.clone_tree for that purpose.
     *
     * @name gui.clone
     * @param node [type:node] node to clone
     * @return clone [type:node] the cloned node
     */
    static int LuaClone(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNode hnode;
        LuaCheckNodeInternal(L, 1, &hnode);

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
        dmGui::Result result = dmGui::CloneNode(scene, node, out_node);
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
                dmGui::SetNodeParent(scene, out_node, parent, false);
            }
            index = node->m_NextIndex;
        }
        return result;
    }

    /*# clone a node including its children
     * Make a clone instance of a node and all its children.
     * Use gui.clone to clone a node excluding its children.
     *
     * @name gui.clone_tree
     * @param node [type:node] root node to clone
     * @return clones [type:table] a table mapping node ids to the corresponding cloned nodes
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
            InternalNode* root = LuaCheckNodeInternal(L, 1, &hnode);
            dmGui::HNode out_node;
            result = CloneNodeToTable(L, scene, root, &out_node);
            if (result == dmGui::RESULT_OK)
            {
                dmGui::HNode parent = INVALID_HANDLE;
                if (root->m_ParentIndex != INVALID_INDEX)
                {
                    parent = GetNodeHandle(&scene->m_Nodes[root->m_ParentIndex]);
                }
                dmGui::SetNodeParent(scene, out_node, parent, false);
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

    static void PushNodeListToTable(lua_State* L, dmGui::HScene scene, uint16_t start_index);

    static void PushNodeToTable(lua_State* L, dmGui::HScene scene, InternalNode* n)
    {
        dmGui::HNode node = GetNodeHandle(n);
        dmScript::PushHash(L, n->m_NameHash);
        LuaPushNode(L, scene, node);
        lua_rawset(L, -3);
        PushNodeListToTable(L, scene, n->m_ChildHead);
    }

    static void PushNodeListToTable(lua_State* L, dmGui::HScene scene, uint16_t start_index)
    {
        uint32_t index = start_index;
        while (index != INVALID_INDEX)
        {
            InternalNode* node = &scene->m_Nodes[index];
            PushNodeToTable(L, scene, node);
            index = node->m_NextIndex;
        }
    }

    /*# get a node including its children
     * Get a node and all its children as a Lua table.
     *
     * @name gui.get_tree
     * @param node [type:node] root node to get node tree from
     * @return clones [type:table] a table mapping node ids to the corresponding nodes
     */
    static int LuaGetTree(lua_State* L)
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
        if (!lua_isnil(L, 1))
        {
            dmGui::HNode hnode;
            InternalNode* root = LuaCheckNodeInternal(L, 1, &hnode);
            PushNodeToTable(L, scene, root);
        }
        else
        {
            PushNodeListToTable(L, scene, scene->m_RenderHead);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# resets all nodes to initial state
     * Resets all nodes in the current GUI scene to their initial state.
     * The reset only applies to static node loaded from the scene.
     * Nodes that are created dynamically from script are not affected.
     *
     * @name gui.reset_nodes
     */
    static int LuaResetNodes(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        ResetNodes(scene);
        return 0;
    }

    /*# sets the render ordering for the current GUI scene
     * Set the order number for the current GUI scene.
     * The number dictates the sorting of the "gui" render predicate,
     * in other words in which order the scene will be rendered in relation
     * to other currently rendered GUI scenes.
     *
     * The number must be in the range 0 to 15.
     *
     * @name gui.set_render_order
     * @param order [type:number] rendering order (0-15)
     */
    static int LuaSetRenderOrder(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        int order = luaL_checkinteger(L, 1);
        // NOTE: The range reflects the current bits allocated in RenderKey for order
        if (order < 0 || order > 15) {
            dmLogWarning("Render must be in range [0,15]");
        }
        order = dmMath::Clamp(order, 0, 15);
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

    /*# password keyboard
     *
     * @name gui.KEYBOARD_TYPE_PASSWORD
     * @variable
     */

    /*# shows the on-display keyboard if available [icon:ios] [icon:android]
     * Shows the on-display touch keyboard.
     * The specified type of keyboard is displayed if it is available on
     * the device.
     *
     * This function is only available on iOS and Android. [icon:ios] [icon:android].
     *
     * @name gui.show_keyboard
     * @param type [type:constant] keyboard type
     *
     * - `gui.KEYBOARD_TYPE_DEFAULT`
     * - `gui.KEYBOARD_TYPE_EMAIL`
     * - `gui.KEYBOARD_TYPE_NUMBER_PAD`
     * - `gui.KEYBOARD_TYPE_PASSWORD`
     * @param autoclose [type:boolean] if the keyboard should automatically close when clicking outside
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

    /*# hides on-display keyboard if available
     * Hides the on-display touch keyboard on the device.
     *
     * @name gui.hide_keyboard
     */
    static int LuaHideKeyboard(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        dmHID::HideKeyboard(scene->m_Context->m_HidContext);
        return 0;
    }

    /*# resets on-display keyboard if available
     * Resets the input context of keyboard. This will clear marked text.
     *
     * @name gui.reset_keyboard
     */
    static int LuaResetKeyboard(lua_State* L)
    {
        Scene* scene = GuiScriptInstance_Check(L);
        dmHID::ResetKeyboard(scene->m_Context->m_HidContext);
        return 0;
    }

    /*# gets the node position
     * Returns the position of the supplied node.
     *
     * @name gui.get_position
     * @param node [type:node] node to get the position from
     * @return position [type:vector3] node position
     */

    /*# sets the node position
     * Sets the position of the supplied node.
     *
     * @name gui.set_position
     * @param node [type:node] node to set the position for
     * @param position [type:vector3|vector4] new position
     */

    /*# gets the node rotation
     * Returns the rotation of the supplied node.
     * The rotation is expressed in degree Euler angles.
     *
     * @name gui.get_rotation
     * @param node [type:node] node to get the rotation from
     * @return rotation [type:vector3] node rotation
     */

    /*# sets the node rotation
     * Sets the rotation of the supplied node.
     * The rotation is expressed in degree Euler angles.
     *
     * @name gui.set_rotation
     * @param node [type:node] node to set the rotation for
     * @param rotation [type:vector3|vector4] new rotation
     */

    /*# gets the node scale
     * Returns the scale of the supplied node.
     *
     * @name gui.get_scale
     * @param node [type:node] node to get the scale from
     * @return scale [type:vector3] node scale
     */

    /*# sets the node scale
     * Sets the scaling of the supplied node.
     *
     * @name gui.set_scale
     * @param node [type:node] node to set the scale for
     * @param scale [type:vector3|vector4] new scale
     */

    /*# gets the node color
     * Returns the color of the supplied node. The components
     * of the returned vector4 contains the color channel values:
     *
     * Component | Color value
     * --------- | -------------
     * x         | Red value
     * y         | Green value
     * z         | Blue value
     * w         | Alpha value
     *
     * @name gui.get_color
     * @param node [type:node] node to get the color from
     * @return color [type:vector4] node color
     */

    /*# sets the node color
     * Sets the color of the supplied node. The components
     * of the supplied vector3 or vector4 should contain the color channel values:
     *
     * Component        | Color value
     * ---------------- | -------------
     * x                | Red value
     * y                | Green value
     * z                | Blue value
     * w [type:vector4] | Alpha value
     *
     * @name gui.set_color
     * @param node [type:node] node to set the color for
     * @param color [type:vector3|vector4] new color
     */

    /*# gets the node outline color
     * Returns the outline color of the supplied node.
     * See [ref:gui.get_color] for info how vectors encode color values.
     *
     * @name gui.get_outline
     * @param node [type:node] node to get the outline color from
     * @return color [type:vector4] outline color
     */

    /*# sets the node outline color
     * Sets the outline color of the supplied node.
     * See [ref:gui.set_color] for info how vectors encode color values.
     *
     * @name gui.set_outline
     * @param node [type:node] node to set the outline color for
     * @param color [type:vector3|vector4] new outline color
     */

    /*# gets the node shadow color
     * Returns the shadow color of the supplied node.
     * See [ref:gui.get_color] for info how vectors encode color values.
     *
     * @name gui.get_shadow
     * @param node [type:node] node to get the shadow color from
     * @return color [type:vector4] node shadow color
     */

    /*# sets the node shadow color
     * Sets the shadow color of the supplied node.
     * See [ref:gui.set_color] for info how vectors encode color values.
     *
     * @name gui.set_shadow
     * @param node [type:node] node to set the shadow color for
     * @param color [type:vector3|vector4] new shadow color
     */

#define LUASET(name, property) \
        int LuaSet##name(lua_State* L)\
        {\
            HNode hnode;\
            InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);\
            if (n->m_Node.m_IsBone) {\
                return 0;\
            }\
            Vector4 v;\
            Vector3* v3;\
            if ((v3 = dmScript::ToVector3(L, 2)))\
            {\
                Scene* scene = GetScene(L);\
                Vector4 original = dmGui::GetNodeProperty(scene, hnode, property);\
                v = Vector4(*v3, original.getW());\
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
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);\
        const Vector4& v = n->m_Node.m_Properties[property];\
        dmScript::PushVector3(L, Vector3(v.getX(), v.getY(), v.getZ()));\
        return 1;\
    }\
    LUASET(name, property)\

#define LUAGETSETV4(name, property) \
    int LuaGet##name(lua_State* L)\
    {\
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);\
        dmScript::PushVector4(L, n->m_Node.m_Properties[property]);\
        return 1;\
    }\
    LUASET(name, property)\

    LUAGETSETV3(Position, PROPERTY_POSITION)
    LUAGETSETV3(Scale, PROPERTY_SCALE)
    LUAGETSETV4(Color, PROPERTY_COLOR)
    LUAGETSETV4(Outline, PROPERTY_OUTLINE)
    LUAGETSETV4(Shadow, PROPERTY_SHADOW)

#undef LUAGETSET

    // Custom setter and getter for gui.set_rotation to be able to handle
    // pass a quaternion for rotations of GUI nodes.
    int LuaGetRotation(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        const Vector4& v = n->m_Node.m_Properties[PROPERTY_ROTATION];
        dmScript::PushVector3(L, Vector3(v.getX(), v.getY(), v.getZ()));
        return 1;
    }

    int LuaSetRotation(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        if (n->m_Node.m_IsBone) {
            return 0;
        }
        Vector4 v;
        Vector3* v3;
        Vector4* v4;
        if ((v3 = dmScript::ToVector3(L, 2)))
        {
            Scene* scene = GetScene(L);
            Vector4 original = dmGui::GetNodeProperty(scene, hnode, PROPERTY_ROTATION);
            v = Vector4(*v3, original.getW());
        } else if ((v4 = dmScript::ToVector4(L, 2))) {
            v = *v4;
        } else {
            Scene* scene = GetScene(L);
            Vector4 original = dmGui::GetNodeProperty(scene, hnode, PROPERTY_ROTATION);
            Quat* q = dmScript::CheckQuat(L, 2);
            v = Vector4(dmVMath::QuatToEuler(q->getX(), q->getY(), q->getZ(), q->getW()), original.getW());
        }
        n->m_Node.m_Properties[PROPERTY_ROTATION] = v;
        n->m_Node.m_DirtyLocal = 1;
        return 0;
    }

    void SetDefaultNewContextParams(NewContextParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_PhysicalWidth = 640;
        params->m_PhysicalHeight = 960;
        params->m_Dpi = 360;
    }

    /*# sets the node size
     *
     * Sets the size of the supplied node.
     *
     * [icon:attention] You can only set size on nodes with size mode set to SIZE_MODE_MANUAL
     *
     * @name gui.set_size
     * @param node [type:node] node to set the size for
     * @param size [type:vector3|vector4] new size
     */
    int LuaSetSize(lua_State* L)
    {
        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        if(n->m_Node.m_SizeMode != SIZE_MODE_MANUAL)
        {
            dmLogWarning("Can not set size on auto-sized nodes.");
            return 0;
        }
        if(n->m_Node.m_IsBone)
        {
            return 0;
        }
        Vector3* v3;
        Vector4 v;
        if ((v3 = dmScript::ToVector3(L, 2)))
        {
            Scene* scene = GetScene(L);
            Vector4 original = dmGui::GetNodeProperty(scene, hnode, PROPERTY_SIZE);
            v = Vector4(*v3, original.getW());
        }
        else
            v = *dmScript::CheckVector4(L, 2);
        n->m_Node.m_Properties[PROPERTY_SIZE] = v;
        n->m_Node.m_DirtyLocal = 1;
        return 0;
    }

    /*# gets the node size
     * Returns the size of the supplied node.
     *
     * @name gui.get_size
     * @param node [type:node] node to get the size from
     * @return size [type:vector3] node size
     */
    int LuaGetSize(lua_State* L)
    {
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        Vector3 v(n->m_Node.m_Properties[PROPERTY_SIZE].getX(), n->m_Node.m_Properties[PROPERTY_SIZE].getY(), n->m_Node.m_Properties[PROPERTY_SIZE].getZ());
        dmScript::PushVector3(L, v);
        return 1;
    }

    /*# gets the node screen position
     * Returns the screen position of the supplied node. This function returns the
     * calculated transformed position of the node, taking into account any parent node
     * transforms.
     *
     * @name gui.get_screen_position
     * @param node [type:node] node to get the screen position from
     * @return position [type:vector3] node screen position
     */
    int LuaGetScreenPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        InternalNode* n = LuaCheckNodeInternal(L, 1, 0);
        Scene* scene = GuiScriptInstance_Check(L);
        Matrix4 node_transform;
        CalculateNodeTransform(scene, n, CalculateNodeTransformFlags(), node_transform);
        Vector3 node_screen_pos = node_transform.getCol3().getXYZ();
        dmScript::PushVector3(L, node_screen_pos);
        return 1;
    }

    /*# sets screen position to the node
     * 
     * Set the screen position to the supplied node
     *
     * @name gui.set_screen_position
     * @param node [type:node] node to set the screen position to
     * @param screen_position [type:vector3] screen position
     */
    int LuaSetScreenPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Scene* scene = GuiScriptInstance_Check(L);
        InternalNode* node = LuaCheckNodeInternal(L, 1, 0);
        Point3 screen_position = GetPositionFromArgumentIndex(L, 2);
        SetScreenPosition(scene, GetNodeHandle(node), screen_position);
        return 0;
    }

    /*# convert screen position to the local node position
     * 
     * Convert the screen position to the local position of supplied node
     *
     * @name gui.screen_to_local
     * @param node [type:node] node used for getting local transformation matrix
     * @param screen_position [type:vector3] screen position
     * @return local_position [type:vector3] local position
     */
    int LuaScreenToLocal(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Scene* scene = GuiScriptInstance_Check(L);
        InternalNode* node = LuaCheckNodeInternal(L, 1, 0);
        Point3 screen_position = GetPositionFromArgumentIndex(L, 2);
        Point3 local_position = ScreenToLocalPosition(scene, GetNodeHandle(node), screen_position);
        dmScript::PushVector3(L, Vector3(local_position));
        return 1;
    }

    /*# gets the normalized cursor of the animation on a node with flipbook animation
     * This is only useful nodes with flipbook animations. Gets the normalized cursor of the flipbook animation on a node.
     *
     * @name gui.get_flipbook_cursor
     * @param node [type:node] node to get the cursor for (node)
     * @return cursor value [type:number] cursor value
     */
    static int LuaGetFlipbookCursor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        Scene* scene = GuiScriptInstance_Check(L);
        HNode node;
        LuaCheckNodeInternal(L, 1, &node);

        if (dmGui::GetNodeIsBone(scene, node))
        {
            return DM_LUA_ERROR("cannot get cursor for bone");
        }

        float cursor = dmGui::GetNodeFlipbookCursor(scene, node);
        lua_pushnumber(L, cursor);

        return 1;
    }

    /*# sets the normalized cursor of the animation on a node with flipbook animation
     * This is only useful nodes with flipbook animations. The cursor is normalized.
     *
     * @name gui.set_flipbook_cursor
     * @param node [type:node] node to set the cursor for
     * @param cursor [type:number] cursor value
     */
    static int LuaSetFlipbookCursor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        Scene* scene = GuiScriptInstance_Check(L);
        HNode node;
        LuaCheckNodeInternal(L, 1, &node);

        if (dmGui::GetNodeIsBone(scene, node))
        {
            return DM_LUA_ERROR("cannot set cursor for bone");
        }

        dmGui::SetNodeFlipbookCursor(scene, node, luaL_checknumber(L, 2));

        return 0;
    }

    /*# gets the playback rate of the flipbook animation on a node
     * This is only useful nodes with flipbook animations. Gets the playback rate of the flipbook animation on a node.
     *
     * @name gui.get_flipbook_playback_rate
     * @param node [type:node] node to set the cursor for
     * @return rate [type:number] playback rate
     */
    static int LuaGetFlipbookPlaybackRate(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        Scene* scene = GuiScriptInstance_Check(L);
        HNode node;
        LuaCheckNodeInternal(L, 1, &node);

        if (dmGui::GetNodeIsBone(scene, node))
        {
            return DM_LUA_ERROR("cannot get playback rate for bone");
        }

        lua_pushnumber(L, dmGui::GetNodeFlipbookPlaybackRate(scene, node));

        return 1;
    }

    /*# sets the playback rate of the flipbook animation on a node
     * This is only useful nodes with flipbook animations. Sets the playback rate of the flipbook animation on a node. Must be positive.
     *
     * @name gui.set_flipbook_playback_rate
     * @param node [type:node] node to set the cursor for
     * @param playback_rate [type:number] playback rate
     */
    static int LuaSetFlipbookPlaybackRate(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HNode node;
        Scene* scene = GuiScriptInstance_Check(L);
        LuaCheckNodeInternal(L, 1, &node);

        if(dmGui::GetNodeIsBone(scene, node))
        {
            return DM_LUA_ERROR("cannot set playback rate for bone");
        }

        SetNodeFlipbookPlaybackRate(scene, node, luaL_checknumber(L, 2));

        return 0;
    }

    /*# creates a new particle fx node
     * Dynamically create a particle fx node.
     *
     * @name gui.new_particlefx_node
     * @param pos [type:vector3|vector4] node position
     * @param particlefx [type:hash|string] particle fx resource name
     * @return node [type:node] new particle fx node
     */
    static int LuaNewParticlefxNode(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        Point3 pos = GetPositionFromArgumentIndex(L, 1);
        dmhash_t particlefx = dmScript::CheckHashOrString(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);

        // The default size comes from the CalculateNodeExtents()
        HNode node = dmGui::NewNode(scene, pos, Vector3(1,1,0), NODE_TYPE_PARTICLEFX, 0);
        if (!node)
        {
            return DM_LUA_ERROR("Out of nodes (max %d)", scene->m_Nodes.Capacity());
        }

        dmGui::Result r = dmGui::SetNodeParticlefx(scene, node, particlefx);
        if (r == RESULT_RESOURCE_NOT_FOUND) {
            char name[128];
            return DM_LUA_ERROR("No particlefx resource '%s' found.", dmScript::GetStringFromHashOrString(L, 2, name, sizeof(name)));
        }
        LuaPushNode(L, scene, node);
        return 1;
    }

    // Only used locally here in this file
    struct GuiLuaCallback
    {
        dmScript::LuaCallbackInfo*  m_Callback;
        HScene                      m_Scene;
        HNode                       m_Node;
    };

    // For storing the Lua callback info and particle state change function together
    struct GuiEmitterStateChangedData
    {
        dmParticle::EmitterStateChangedData m_ParticleCallback;
        GuiLuaCallback                      m_LuaInfo;
    };

    // For locally passing data to the invokation of the Lua callback
    struct GuiPfxEmitterScriptCallbackData
    {
        GuiEmitterStateChangedData* m_Data;
        dmhash_t                    m_EmitterID;
        dmParticle::EmitterState    m_EmitterState;
    };

    // Called when the Lua callback is invoked
    static void PushPfxCallbackArguments(lua_State* L, void* user_data)
    {
        GuiPfxEmitterScriptCallbackData* data = (GuiPfxEmitterScriptCallbackData*)user_data;
        GuiLuaCallback* luainfo = &data->m_Data->m_LuaInfo;
        if (dmGui::IsNodeValid(luainfo->m_Scene, luainfo->m_Node)) {
            LuaPushNode(L, luainfo->m_Scene, luainfo->m_Node);
        } else {
            lua_pushnil(L);
        }
        dmScript::PushHash(L, data->m_EmitterID);
        lua_pushinteger(L, data->m_EmitterState);
    }

    static void EmitterStateChangedCallback(uint32_t num_awake_emitters, dmhash_t emitter_id, dmParticle::EmitterState emitter_state, void* user_data)
    {
        GuiEmitterStateChangedData* data = (GuiEmitterStateChangedData*)(user_data);

        if (!dmScript::IsCallbackValid(data->m_LuaInfo.m_Callback))
            return;

        GuiPfxEmitterScriptCallbackData callback_data = { data, emitter_id, emitter_state };
        dmScript::InvokeCallback(data->m_LuaInfo.m_Callback, PushPfxCallbackArguments, &callback_data);

        // The last emitter belonging to this particlefx har gone to sleep, release lua reference.
        if(num_awake_emitters == 0 && emitter_state == dmParticle::EMITTER_STATE_SLEEPING)
        {
            dmScript::DestroyCallback(data->m_LuaInfo.m_Callback);
            data->m_LuaInfo.m_Callback = 0x0;
        }
    }

    /*# Plays a particle fx
     * Plays the paricle fx for a gui node
     *
     * @name gui.play_particlefx
     * @param node [type:node] node to play particle fx for
     * @param [emitter_state_function] [type:function(self, node, emitter, state)] optional callback function that will be called when an emitter attached to this particlefx changes state.
     *
     * `self`
     * : [type:object] The current object
     *
     * `node`
     * : [type:hash] The particle fx node, or `nil` if the node was deleted
     *
     * `emitter`
     * : [type:hash] The id of the emitter
     *
     * `state`
     * : [type:constant] the new state of the emitter:
     *
     * - `particlefx.EMITTER_STATE_SLEEPING`
     * - `particlefx.EMITTER_STATE_PRESPAWN`
     * - `particlefx.EMITTER_STATE_SPAWNING`
     * - `particlefx.EMITTER_STATE_POSTSPAWN`
     *
     * @examples
     *
     * How to play a particle fx when a gui node is created.
     * The callback receives the gui node, the hash of the id
     * of the emitter, and the new state of the emitter as particlefx.EMITTER_STATE_<STATE>.
     *
     * ```lua
     * local function emitter_state_change(self, node, emitter, state)
     *   if emitter == hash("exhaust") and state == particlefx.EMITTER_STATE_POSTSPAWN then
     *     -- exhaust is done spawning particles...
     *   end
     * end
     *
     * function init(self)
     *     gui.play_particlefx(gui.get_node("particlefx"), emitter_state_change)
     * end
     * ```
     */
    static int LuaParticlefxPlay(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HNode hnode;
        Scene* scene = GuiScriptInstance_Check(L);
        LuaCheckNodeInternal(L, 1, &hnode);

        GuiEmitterStateChangedData* script_data = 0;
        if (lua_gettop(L) > 1 && !lua_isnil(L, 2) )
        {
            dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, 2);
            if (callback == 0x0)
            {
                return DM_LUA_ERROR("Could not create callback for particlefx.");
            }

            // if we reached here, the callback was registered
            script_data = (GuiEmitterStateChangedData*)malloc(sizeof(GuiEmitterStateChangedData)); // Released by the particle system (or actually the m_UserData)

            script_data->m_LuaInfo.m_Callback = callback;
            script_data->m_LuaInfo.m_Scene = scene;
            script_data->m_LuaInfo.m_Node = hnode;

            // Point the userdata to itself, for easy deletion later on
            script_data->m_ParticleCallback.m_UserData = script_data; // Released by the particle system
            script_data->m_ParticleCallback.m_StateChangedCallback = EmitterStateChangedCallback;
        }

        dmGui::Result res;
        res = dmGui::PlayNodeParticlefx(scene, hnode, (dmParticle::EmitterStateChangedData*)script_data);

        if (res == RESULT_WRONG_TYPE)
        {
            if (script_data)
                free(script_data);
            return DM_LUA_ERROR("Could not play particlefx on non-particlefx node.");
        }

    	return 0;
    }

    /*# Stops a particle fx
     *
     * Stops the particle fx for a gui node
     *
     * @name gui.stop_particlefx
     * @param node [type:node] node to stop particle fx for
     * @param options [type:table] options when stopping the particle fx. Supported options:
     *
     * - [type:boolean] `clear`: instantly clear spawned particles
     */
    static int LuaParticlefxStop(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HNode hnode;
        Scene* scene = GuiScriptInstance_Check(L);
        LuaCheckNodeInternal(L, 1, &hnode);

        int clear_particles = 0;
        if (!lua_isnone(L, 2)) {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                const char* option = lua_tostring(L, -2);
                if (strcmp(option, "clear") == 0)
                {
                    clear_particles = lua_toboolean(L, -1);
                }
                else
                {
                    dmLogWarning("Unknown option to gui.stop_particlefx() %s", option);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }

        dmGui::Result res;
        res = dmGui::StopNodeParticlefx(scene, hnode, clear_particles);

        if (res == RESULT_WRONG_TYPE)
        {
            return DM_LUA_ERROR("Could not stop particlefx on GUI node");
        }

    	return 0;
    }

    /*# Sets a particle fx
     *
     * Set the paricle fx for a gui node
     *
     * @name gui.set_particlefx
     * @param node [type:node] node to set particle fx for
     * @param particlefx [type:hash|string] particle fx id
     */
    static int LuaSetParticlefx(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HNode hnode;
        LuaCheckNodeInternal(L, 1, &hnode);

        dmhash_t particlefx_id = dmScript::CheckHashOrString(L, 2);
        Scene* scene = GuiScriptInstance_Check(L);
        Result r = SetNodeParticlefx(scene, hnode, particlefx_id);
        if (r == RESULT_WRONG_TYPE) {
            return DM_LUA_ERROR("Can only set particle system on particlefx nodes!");
        }
        else if (r == RESULT_RESOURCE_NOT_FOUND) {
            char name[128];
            return DM_LUA_ERROR("No particle system named: '%s'", dmScript::GetStringFromHashOrString(L, 2, name, sizeof(name)));
        }

        return 0;
    }

    /*# Gets a particle fx
     *
     * Get the paricle fx for a gui node
     *
     * @name gui.get_particlefx
     * @param node [type:node] node to get particle fx for
     * @return [type:hash] particle fx id
     */
    static int LuaGetParticlefx(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        HNode hnode;
        LuaCheckNodeInternal(L, 1, &hnode);
        Scene* scene = GuiScriptInstance_Check(L);
        dmhash_t particlefx_id = 0;
        Result r = GetNodeParticlefx(scene, hnode, particlefx_id);
        if (r == RESULT_WRONG_TYPE) {
            return DM_LUA_ERROR("Can only get particle system on particlefx nodes!");
        }

        dmScript::PushHash(L, particlefx_id);
        return 1;
    }

    /*# gets the node inherit alpha state
     *
     * @name gui.get_inherit_alpha
     * @param node [type:node] node from which to get the inherit alpha state
     */
    static int LuaGetInheritAlpha(lua_State* L)
    {
        int top = lua_gettop(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        lua_pushboolean(L, n->m_Node.m_InheritAlpha);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# sets the node inherit alpha state
     *
     * @name gui.set_inherit_alpha
     * @param node [type:node] node from which to set the inherit alpha state
     * @param inherit_alpha [type:boolean] true or false
     */
    static int LuaSetInheritAlpha(lua_State* L)
    {
        int top = lua_gettop(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        int inherit_alpha = lua_toboolean(L, 2);
        n->m_Node.m_InheritAlpha = inherit_alpha;

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# gets the node alpha
     *
     * @name gui.get_alpha
     * @param node [type:node] node from which to get alpha
     */
    static int LuaGetAlpha(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1)

        Scene* scene = GuiScriptInstance_Check(L);

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_pushnumber(L, (lua_Number) dmGui::GetNodeAlpha(scene, hnode));

        return 1;
    }

    /*# sets the node alpha
     *
     * @name gui.set_alpha
     * @param node [type:node] node for which to set alpha
     * @param alpha [type:number] 0..1 alpha color
     */
    static int LuaSetAlpha(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0)

        HNode hnode;
        InternalNode* n = LuaCheckNodeInternal(L, 1, &hnode);
        (void) n;

        lua_Number alpha = luaL_checknumber(L, 2);

        Scene* scene = GuiScriptInstance_Check(L);
        SetNodeAlpha(scene, hnode, alpha);

        return 0;
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
        {"get_material",    LuaGetMaterial},
        {"set_material",    LuaSetMaterial},
        {"reset_material",  LuaResetMaterial},
        {"get_font",        LuaGetFont},
        {"get_font_resource", LuaGetFontResource},
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
        {"get_visible",     LuaGetVisible},
        {"set_visible",     LuaSetVisible},
        {"get_adjust_mode", LuaGetAdjustMode},
        {"set_adjust_mode", LuaSetAdjustMode},
        {"get_size_mode",   LuaGetSizeMode},
        {"set_size_mode",   LuaSetSizeMode},
        {"move_above",      LuaMoveAbove},
        {"move_below",      LuaMoveBelow},
        {"get_parent",      LuaGetParent},
        {"set_parent",      LuaSetParent},
        {"clone",           LuaClone},
        {"clone_tree",      LuaCloneTree},
        {"get_tree",        LuaGetTree},
        {"show_keyboard",   LuaShowKeyboard},
        {"hide_keyboard",   LuaHideKeyboard},
        {"reset_keyboard",  LuaResetKeyboard},
        {"get_screen_position", LuaGetScreenPosition},
        {"set_screen_position", LuaSetScreenPosition},
        {"screen_to_local", LuaScreenToLocal},
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
        {"set_leading",     LuaSetLeading},
        {"get_leading",     LuaGetLeading},
        {"set_tracking",    LuaSetTracking},
        {"get_tracking",    LuaGetTracking},
        {"set_size",        LuaSetSize},
        {"get_size",        LuaGetSize},
        {"get_flipbook_cursor", LuaGetFlipbookCursor},
        {"set_flipbook_cursor", LuaSetFlipbookCursor},
        {"get_flipbook_playback_rate", LuaGetFlipbookPlaybackRate},
        {"set_flipbook_playback_rate", LuaSetFlipbookPlaybackRate},
        {"new_particlefx_node",  LuaNewParticlefxNode},
        {"set_particlefx",  LuaSetParticlefx},
        {"get_particlefx",  LuaGetParticlefx},
        {"play_particlefx", LuaParticlefxPlay},
        {"stop_particlefx", LuaParticlefxStop},
        {"get_inherit_alpha", LuaGetInheritAlpha},
        {"set_inherit_alpha", LuaSetInheritAlpha},
        {"get_alpha", LuaGetAlpha},
        {"set_alpha", LuaSetAlpha},

        REGGETSET(Position, position)
        REGGETSET(Rotation, rotation)
        REGGETSET(Scale, scale)
        REGGETSET(Color, color)
        REGGETSET(Outline, outline)
        REGGETSET(Shadow, shadow)
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

    /*# no anchor
     *
     * @name gui.ANCHOR_NONE
     * @variable
     */

    /*# center pivot
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


    /*# manual size mode
     * The size of the node is determined by the size set in the editor, the constructor or by gui.set_size()
     * @name gui.SIZE_MODE_MANUAL
     * @variable
     */

    /*# automatic size mode
     * The size of the node is determined by the currently assigned texture.
     * @name gui.SIZE_MODE_AUTO
     * @variable
     */


    /*# texture already exists
     * The texture id already exists when trying to use gui.new_texture().
     * @name gui.RESULT_TEXTURE_ALREADY_EXISTS
     * @variable
     */

    /*# out of resource
     * The system is out of resources, for instance when trying to create a new
     * texture using gui.new_texture().
     * @name gui.RESULT_OUT_OF_RESOURCES
     * @variable
     */

    /*# data error
     * The provided data is not in the expected format or is in some other way
     * incorrect, for instance the image data provided to gui.new_texture().
     * @name gui.RESULT_DATA_ERROR
     * @variable
     */

    lua_State* InitializeScript(dmScript::HContext script_context)
    {
        lua_State* L = dmScript::GetLuaState(script_context);

        int top = lua_gettop(L);
        (void)top;

        GUI_SCRIPT_TYPE_HASH = dmScript::RegisterUserType(L, GUI_SCRIPT, GuiScript_methods, GuiScript_meta);

        GUI_SCRIPT_INSTANCE_TYPE_HASH = dmScript::RegisterUserType(L, GUI_SCRIPT_INSTANCE, GuiScriptInstance_methods, GuiScriptInstance_meta);

        NODE_PROXY_TYPE_HASH = dmScript::RegisterUserType(L, NODE_PROXY_TYPE_NAME, NodeProxy_methods, NodeProxy_meta);

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
        SETKEYBOARD(PASSWORD)

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

#define SETRESULT(name) \
        lua_pushnumber(L, (lua_Number) RESULT_##name); \
        lua_setfield(L, -2, "RESULT_"#name);\

        SETRESULT(TEXTURE_ALREADY_EXISTS)
        SETRESULT(OUT_OF_RESOURCES)
        SETRESULT(DATA_ERROR)
#undef SETRESULT

        lua_pushnumber(L, (lua_Number) SIZE_MODE_MANUAL);
        lua_setfield(L, -2, "SIZE_MODE_MANUAL");
        lua_pushnumber(L, (lua_Number) SIZE_MODE_AUTO);
        lua_setfield(L, -2, "SIZE_MODE_AUTO");

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
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * ```lua
     * function init(self)
     *     -- set up useful data
     *     self.my_value = 1
     * end
     * ```
     */

    /*# called when a gui component is finalized
     * This is a callback-function, which is called by the engine when a gui component is finalized (destroyed). It can
     * be used to e.g. take some last action, report the finalization to other game object instances
     * or release user input focus (see <code>release_input_focus</code>). There is no use in starting any animations or similar
     * from this function since the gui component is about to be destroyed.
     *
     * @name final
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * ```lua
     * function final(self)
     *     -- report finalization
     *     msg.post("my_friend_instance", "im_dead", {my_stats = self.some_value})
     * end
     * ```
     */

    /*# called every frame to update the gui component
     * This is a callback-function, which is called by the engine every frame to update the state of a gui component.
     * It can be used to perform any kind of gui related tasks, e.g. animating nodes.
     *
     * @name update
     * @param self [type:object] reference to the script state to be used for storing data
     * @param dt [type:number] the time-step of the frame update
     * @examples
     *
     * This example demonstrates how to update a text node that displays game score in a counting fashion.
     * It is assumed that the gui component receives messages from the game when a new score is to be shown.
     *
     * ```lua
     * function init(self)
     *     -- fetch the score text node for later use (assumes it is called "score")
     *     self.score_node = gui.get_node("score")
     *     -- keep track of the current score counted up so far
     *     self.current_score = 0
     *     -- keep track of the target score we should count up to
     *     self.target_score = 0
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
     * ```
     */

    /*# called when a message has been sent to the gui component
     *
     * This is a callback-function, which is called by the engine whenever a message has been sent to the gui component.
     * It can be used to take action on the message, e.g. update the gui or send a response back to the sender of the message.
     *
     * The <code>message</code> parameter is a table containing the message data. If the message is sent from the engine, the
     * documentation of the message specifies which data is supplied.
     *
     * See the [ref:update] function for examples on how to use this callback-function.
     *
     * @name on_message
     * @param self [type:object] reference to the script state to be used for storing data
     * @param message_id [type:hash] id of the received message
     * @param message [type:table] a table containing the message data
     */

    /*# called when user input is received
     * This is a callback-function, which is called by the engine when user input is sent to the instance of the gui component.
     * It can be used to take action on the input, e.g. modify the gui according to the input.
     *
     * For an instance to obtain user input, it must first acquire input
     * focus through the message `acquire_input_focus`.
     *
     * Any instance that has obtained input will be put on top of an
     * input stack. Input is sent to all listeners on the stack until the
     * end of stack is reached, or a listener returns `true`
     * to signal that it wants input to be consumed.
     *
     * See the documentation of <a href="/ref/go/#acquire_input_focus">acquire_input_focus</a> for more
     * information.
     *
     * The `action` parameter is a table containing data about the input mapped to the
     * `action_id`.
     * For mapped actions it specifies the value of the input and if it was just pressed or released.
     * Actions are mapped to input in an input_binding-file.
     *
     * Mouse movement is specifically handled and uses `nil` as its `action_id`.
     * The `action` only contains positional parameters in this case, such as x and y of the pointer.
     *
     * Here is a brief description of the available table fields:
     *
     * Field       | Description
     * ----------- | ----------------------------------------------------------
     * `value`     | The amount of input given by the user. This is usually 1 for buttons and 0-1 for analogue inputs. This is not present for mouse movement.
     * `pressed`   | If the input was pressed this frame. This is not present for mouse movement.
     * `released`  | If the input was released this frame. This is not present for mouse movement.
     * `repeated`  | If the input was repeated this frame. This is similar to how a key on a keyboard is repeated when you hold it down. This is not present for mouse movement.
     * `x`         | The x value of a pointer device, if present.
     * `y`         | The y value of a pointer device, if present.
     * `screen_x`  | The screen space x value of a pointer device, if present.
     * `screen_y`  | The screen space y value of a pointer device, if present.
     * `dx`        | The change in x value of a pointer device, if present.
     * `dy`        | The change in y value of a pointer device, if present.
     * `screen_dx` | The change in screen space x value of a pointer device, if present.
     * `screen_dy` | The change in screen space y value of a pointer device, if present.
     * `gamepad`   | The index of the gamepad device that provided the input.
     * `touch`     | List of touch input, one element per finger, if present. See table below about touch input
     *
     * Touch input table:
     *
     * Field       | Description
     * ----------- | ----------------------------------------------------------
     * `id`        | A number identifying the touch input during its duration.
     * `pressed`   | True if the finger was pressed this frame.
     * `released`  | True if the finger was released this frame.
     * `tap_count` | Number of taps, one for single, two for double-tap, etc
     * `x`         | The x touch location.
     * `y`         | The y touch location.
     * `dx`        | The change in x value.
     * `dy`        | The change in y value.
     * `acc_x`     | Accelerometer x value (if present).
     * `acc_y`     | Accelerometer y value (if present).
     * `acc_z`     | Accelerometer z value (if present).
     *
     * @name on_input
     * @param self [type:object] reference to the script state to be used for storing data
     * @param action_id [type:hash] id of the received input action, as mapped in the input_binding-file
     * @param action [type:table] a table containing the input data, see above for a description
     * @return [consume] [type:boolean] optional boolean to signal if the input should be consumed (not passed on to others) or not, default is false
     * @examples
     *
     * ```lua
     * function on_input(self, action_id, action)
     *     -- check for input
     *     if action_id == hash("my_action") then
     *         -- take appropritate action
     *         self.my_value = action.value
     *     end
     *     -- consume input
     *     return true
     * end
     * ```
     */

    /*# called when the gui script is reloaded
     * <p>
     * This is a callback-function, which is called by the engine when the gui script is reloaded, e.g. from the editor.
     * It can be used for live development, e.g. to tweak constants or set up the state properly for the script.
     * </p>
     *
     * @name on_reload
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * ```lua
     * function on_reload(self)
     *     -- restore some color (or similar)
     *     gui.set_color(gui.get_node("my_node"), self.my_original_color)
     * end
     * ```
     */

    HScene GetSceneFromLua(lua_State* L) {
        return GetScene(L);
    }
}
