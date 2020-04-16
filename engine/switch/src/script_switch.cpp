#if defined(__NX__)

#include <assert.h>
#include <malloc.h>
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/script/script.h>

#include "switch_private.h"

namespace dmSwitchScript
{
    dmSwitch::Account::UserInfo g_LastOpenedUser;

    /*# Switch API documentation
     *
     * Helper functions to more easily modify the default behavior 
     *
     * @document
     * @name switch
     * @namespace switch
     */

    static dmSwitch::Account::UserInfo* CheckuserInfo(lua_State* L, int index)
    {
        if (!lua_islightuserdata(L, index))
        {
            luaL_error(L, "Provided UserInfo handle is invalid");
        }
        return (dmSwitch::Account::UserInfo*)lua_touserdata(L, index);
    }

    /*# initialize the user account system
     *
     * Initialize the user account system
     *
     * @name switch.account_init_user
     * @return success [type:boolean] true if successful, false otherwise
     */
    static int Switch_Account_InitUser(lua_State* L)
    {
        dmSwitch::Account::UserResult result = dmSwitch::Account::InitUser();
        if (result != dmSwitch::Account::RESULT_USER_OK)
        {
            dmLogError("Failed to initialize user account system");
        }
        lua_pushboolean(L, result == dmSwitch::Account::RESULT_USER_OK);
        return 1;
    }

    /*# open the last opened user
     *
     * Open the last opened user.
     * Close the user handle after usage, using `switch.account_close_user`
     *
     * @name switch.account_open_last_user
     * @return handdle, success [type:userdata] [type:boolean] true if successful, false otherwise
     */
    static int Switch_Account_OpenLastUser(lua_State* L)
    {
        dmSwitch::Account::UserInfo* info = (dmSwitch::Account::UserInfo*)malloc(sizeof(dmSwitch::Account::UserInfo));
        dmSwitch::Account::UserResult result = dmSwitch::Account::OpenLastUser(info);
        if (result != dmSwitch::Account::RESULT_USER_OK)
        {
            dmLogError("Failed to open last opened user account");
            free(info);
            info = 0;
        }
        lua_pushlightuserdata(L, info);
        lua_pushboolean(L, result == dmSwitch::Account::RESULT_USER_OK);
        return 2;
    }

    /*# open user selection screen
     *
     * Open user selection screen
     * Close the user handle after usage, using `switch.account_close_user`
     *
     * @name switch.account_select_user
     * @return handdle, success [type:userdata] [type:boolean] true if successful, false otherwise
     */
    static int Switch_Account_SelectUser(lua_State* L)
    {
        dmSwitch::Account::UserInfo* info = (dmSwitch::Account::UserInfo*)malloc(sizeof(dmSwitch::Account::UserInfo));
        dmSwitch::Account::UserResult result = dmSwitch::Account::SelectUser(info);
        if (result != dmSwitch::Account::RESULT_USER_OK)
        {
            dmLogError("Failed to open selected user account");
            free(info);
            info = 0;
        }
        lua_pushlightuserdata(L, info);
        lua_pushboolean(L, result == dmSwitch::Account::RESULT_USER_OK);
        return 2;
    }

    /*# close the user info handle
     *
     * Close the user info handle
     *
     * @name switch.account_close_user
     */
    static int Switch_Account_CloseUser(lua_State* L)
    {
        dmSwitch::Account::UserInfo* info = CheckuserInfo(L, 1);
        dmSwitch::Account::CloseUser(info);
        free(info);
        return 0;
    }

    static const luaL_reg Switch_methods[] =
    {
        {"account_init_user", Switch_Account_InitUser},
        {"account_open_last_user", Switch_Account_OpenLastUser},
        {"account_select_user", Switch_Account_SelectUser},
        {"account_close_user", Switch_Account_CloseUser},
        {0, 0}
    };

    #define SETCONSTANT(name, value) \
        lua_pushnumber(L, (lua_Number) (value)); \
        lua_setfield(L, -2, #name);\

    static void InitLua(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "switch", Switch_methods);

        /*# Return value ok
         * @name switch.USER_RESULT_OK
         * @variable
         */
        SETCONSTANT(USER_RESULT_OK, dmSwitch::Account::RESULT_USER_OK);

        /*# The user cancelled the operation.
         * @name switch.USER_RESULT_CANCELLED
         * @variable
         */
        SETCONSTANT(USER_RESULT_CANCELLED, dmSwitch::Account::RESULT_USER_CANCELLED);

        /*# The user does not exist any more
         * @name switch.USER_RESULT_NOT_EXIST
         * @variable
         */
        SETCONSTANT(USER_RESULT_NOT_EXIST, dmSwitch::Account::RESULT_USER_NOT_EXIST);

        /*# Unspecified user error
         * @name switch.USER_RESULT_ERROR
         * @variable
         */
        SETCONSTANT(USER_RESULT_ERROR, dmSwitch::Account::RESULT_USER_ERROR);


        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    #undef SETCONSTANT

    static dmExtension::Result AppInitializeSwitch(dmExtension::AppParams* params)
    {
        dmSwitch::Account::InitUser();

        int automatic_user_select = dmConfigFile::GetInt(params->m_ConfigFile, "switch.automatic_user_select", 1);
        if( automatic_user_select )
        {
            dmSwitch::Account::OpenLastUser(&g_LastOpenedUser);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result AppFinalizeSwitch(dmExtension::AppParams* params)
    {
        int automatic_user_select = dmConfigFile::GetInt(params->m_ConfigFile, "switch.automatic_user_select", 1);
        if( automatic_user_select )
        {
            dmSwitch::Account::CloseUser(&g_LastOpenedUser);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result InitializeSwitch(dmExtension::Params* params)
    {
        InitLua(params->m_L);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeSwitch(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(SwitchExt, "SwitchExt", AppInitializeSwitch, AppFinalizeSwitch, InitializeSwitch, 0, 0, FinalizeSwitch)
}

#else // DM_PLATFORM_SWITCH

extern "C" void SwitchExt()
{
}

#endif