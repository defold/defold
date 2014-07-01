#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../components/comp_spine_model.h"

#include "script_spine_model.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}


namespace dmGameSystem
{

    /*# retrieve the game object corresponding to a spine model skeleton bone
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity O(n), where n is the number of bones in the spine model skeleton.
     *
     * @name spine.get_go
     * @param url the spine model to query (url)
     * @param bone_id id of the corresponding bone (string|hash)
     * @return id of the game object
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel".
     * <p>
     * How to parent the game object of the calling script to the "right_hand" bone of the spine model in a player game object:
     * </p>
     * <pre>
     * function init(self)
     *     local parent = spine.get_go("player#spinemodel", "right_hand")
     *     msg.post(".", "set_parent", {parent_id = parent})
     * end
     * </pre>
     */
    int SpineComp_GetGO(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        dmMessage::URL receiver;
        // TODO make lookup type safe, described in DEF-407
        dmGameObject::GetInstanceFromLua(L, 1, &user_data, &receiver);
        SpineModelComponent* component = (SpineModelComponent*) user_data;

        dmhash_t bone_id;
        if (lua_isstring(L, 2))
        {
            bone_id = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            bone_id = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "bone_id must be either a hash or a string");
        }

        dmGameSystemDDF::Skeleton* skeleton = &component->m_Resource->m_Scene->m_SpineScene->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        uint32_t bone_index = ~0u;
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            if (skeleton->m_Bones[i].m_Id == bone_id)
            {
                bone_index = i;
                break;
            }
        }
        if (bone_index == ~0u)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }
        dmGameObject::HInstance instance = component->m_NodeInstances[bone_index];
        if (instance == 0x0)
        {
            return luaL_error(L, "no game object found for the bone '%s'", lua_tostring(L, 2));
        }
        dmScript::PushHash(L, dmGameObject::GetIdentifier(instance));

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg SPINE_COMP_FUNCTIONS[] =
    {
            {"get_go",  SpineComp_GetGO},
            {0, 0}
    };

    void ScriptSpineModelRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "spine", SPINE_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
