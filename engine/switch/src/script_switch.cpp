#if defined(__NX__)

#include <assert.h>
#include <malloc.h>
#include <dlib/log.h>
#include <extension/extension.h>
#include <script/script.h>
#include <nn/fs.h>

#include <hid/hid.h>

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

    /*# get the currently opened user
     *
     * Cet the currently opened user
     * Close the user handle after usage, using `switch.account_close_user`
     *
     * @name switch.account_get_user
     * @return handdle, success [type:userdata] [type:boolean] true if successful, false otherwise
     */
    static int Switch_Account_GetUser(lua_State* L)
    {
        dmSwitch::Account::UserInfo* info = (dmSwitch::Account::UserInfo*)malloc(sizeof(dmSwitch::Account::UserInfo));
        dmSwitch::Account::UserResult result = dmSwitch::Account::GetUser(info);
        if (result != dmSwitch::Account::RESULT_USER_OK)
        {
            dmLogError("Failed to get last opened user account");
            free(info);
            info = 0;
        }
        lua_pushlightuserdata(L, info);
        lua_pushboolean(L, result == dmSwitch::Account::RESULT_USER_OK);
        return 2;
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

    /*# mounts the save data filesystem for a user (save:)
     *
     * Mounts the save data filesystem for a user (save:)
     *
     * @name switch.account_mount_user_save_data
     * @param user the user info
     */
    static int Switch_Account_MountUserSaveData(lua_State* L)
    {
        dmSwitch::Account::UserInfo* info = CheckuserInfo(L, 1);
        dmSwitch::Account::CloseUser(info);
        free(info);
        return 0;
    }

    static inline int CheckGamePadNumber(lua_State* L, int index)
    {
        int gamepad = luaL_checkint(L, index);
        if (gamepad < 0 || gamepad >= dmHID::MAX_GAMEPAD_COUNT)
            return luaL_error(L, "Invalid gamepad number: %d. Must be in range [0,%d]", gamepad, dmHID::MAX_GAMEPAD_COUNT-1);
        return gamepad;
    }

    /*# gets the color of a gamepad
     *
     * gets the color of a gamepad
     *
     * @name switch.hid_get_gamepad_color
     * @param gamepad the gamepad number
     * @return colors [type:vector4] 2-tuple of left and right color
     */
    static int Switch_Hid_GetGamepadColor(lua_State* L)
    {
        int gamepad = CheckGamePadNumber(L, 1);

        Vectormath::Aos::Vector4 left;
        Vectormath::Aos::Vector4 right;
        if (dmSwitch::Hid::RESULT_OK != dmSwitch::Hid::GetGamepadColor(gamepad, (float*)&left, (float*)&right)) 
        {
            lua_pushnil(L);
            lua_pushnil(L);
        } else {
            dmScript::PushVector4(L, left);
            dmScript::PushVector4(L, right);
        }
        return 2;
    }

    /*# sets the assignment mode for the gamepad
     *
     * Sets the assignment mode for the gamepad
     *
     * @name switch.hid_set_gamepad_assignmentmode
     * @param gamepad the gamepad number
     * @param mode Assignment mode. (switch.HID_ASSIGN_MODE_DUAL or switch.HID_ASSIGN_MODE_SINGLE)
     */
    static int Switch_Hid_SetGamePadAssignmentMode(lua_State* L)
    {
        int gamepad = CheckGamePadNumber(L, 1);
        int mode = luaL_checkint(L, 2);
        if (mode <  0 || mode > 1)
            return luaL_error(L, "Invalid mode %d. Must be in either of switch.HID_ASSIGN_MODE_DUAL or switch.HID_ASSIGN_MODE_SINGLE", mode);

        dmSwitch::Hid::SetGamepadAssignmentMode(gamepad, mode);
        return 0;
    }

    /*# sets the supported stylesets
     *
     * Sets the supported stylesets
     *
     * @name switch.hid_set_gamepad_supported_styleset
     * @param mask a bitmask of the supported styles
     */
    static int Switch_Hid_SetGamepadSupportedStyleset(lua_State* L)
    {
        int mask = luaL_checkint(L, 1);
        dmSwitch::Hid::SetGamepadSupportedStyleset(mask);
        return 0;
    }

    /*# shows the dialog that let's the user configure the controllers
     *
     * Shows the dialog that let's the user configure the controllers
     *
     * @name switch.hid_show_gamepad_dialog
     * @param multiplayer 0 = single player, non-zero = multiplayer
     * @return `switch.HID_RESULT_USER_CANCELLED`, `switch.HID_RESULT_UNSUPPORTED` or `switch.HID_RESULT_OK`
     */
    static int Switch_Hid_ShowGamepadDialog(lua_State* L)
    {
        int multiplayer = luaL_checkint(L, 1);
        dmSwitch::Hid::Result result = dmSwitch::Hid::ShowControllerSupport(multiplayer!=0);
        lua_pushinteger(L, (int)result);
        return 1;
    }

    static const luaL_reg Switch_methods[] =
    {
        // account
        {"account_init_user", Switch_Account_InitUser},
        {"account_get_user", Switch_Account_GetUser},
        {"account_open_last_user", Switch_Account_OpenLastUser},
        {"account_select_user", Switch_Account_SelectUser},
        {"account_close_user", Switch_Account_CloseUser},
        {"account_mount_user_save_data", Switch_Account_MountUserSaveData},

        // hid
        {"hid_get_gamepad_color", Switch_Hid_GetGamepadColor},
        {"hid_set_gamepad_assignmentmode", Switch_Hid_SetGamePadAssignmentMode},
        {"hid_set_gamepad_supported_styleset", Switch_Hid_SetGamepadSupportedStyleset},
        {"hid_show_gamepad_dialog", Switch_Hid_ShowGamepadDialog},
        
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


        /*# return value OK
         * @name switch.HID_RESULT_OK
         * @variable
         */
        SETCONSTANT(HID_RESULT_OK, dmSwitch::Hid::RESULT_OK);

        /*# The device was is not connected
         * @name switch.HID_RESULT_NOT_CONNECTED
         * @variable
         */
        SETCONSTANT(HID_RESULT_NOT_CONNECTED, dmSwitch::Hid::RESULT_NOT_CONNECTED);

        /*# The device has no color assigned
         * @name switch.HID_RESULT_NO_COLOR
         * @variable
         */
        SETCONSTANT(HID_RESULT_NO_COLOR, dmSwitch::Hid::RESULT_NO_COLOR);

        /*# The operation was cancelled
         * @name switch.HID_RESULT_USER_CANCELLED
         * @variable
         */
        SETCONSTANT(HID_RESULT_USER_CANCELLED, dmSwitch::Hid::RESULT_USER_CANCELLED);

        /*# The chosen parameters are unsupported
         * @name switch.HID_RESULT_UNSUPPORTED
         * @variable
         */
        SETCONSTANT(HID_RESULT_UNSUPPORTED, dmSwitch::Hid::RESULT_UNSUPPORTED);


        /*# Dual assignment mode 
         * @name switch.HID_ASSIGN_MODE_DUAL
         * @variable
         */
        SETCONSTANT(HID_ASSIGN_MODE_DUAL, dmSwitch::Hid::ASSIGN_MODE_DUAL);

        /*# Single assignment mode 
         * @name switch.HID_ASSIGN_MODE_SINGLE
         * @variable
         */
        SETCONSTANT(HID_ASSIGN_MODE_SINGLE, dmSwitch::Hid::ASSIGN_MODE_SINGLE);


        /*# Use with switch.hid_set_gamepad_supported_styleset
         * @name switch.HID_GAMEPAD_STYLE_HANDHELD
         * @variable
         */
        SETCONSTANT(HID_GAMEPAD_STYLE_HANDHELD, dmSwitch::Hid::GAMEPAD_STYLE_HANDHELD);
        /*# Use with switch.hid_set_gamepad_supported_styleset
         * @name switch.HID_GAMEPAD_STYLE_FULLKEY
         * @variable
         */
        SETCONSTANT(HID_GAMEPAD_STYLE_FULLKEY, dmSwitch::Hid::GAMEPAD_STYLE_FULLKEY);
        /*# Use with switch.hid_set_gamepad_supported_styleset
         * @name switch.HID_GAMEPAD_STYLE_DUAL
         * @variable
         */
        SETCONSTANT(HID_GAMEPAD_STYLE_DUAL, dmSwitch::Hid::GAMEPAD_STYLE_DUAL);
        /*# Use with switch.hid_set_gamepad_supported_styleset
         * @name switch.HID_GAMEPAD_STYLE_JOYLEFT
         * @variable
         */
        SETCONSTANT(HID_GAMEPAD_STYLE_JOYLEFT, dmSwitch::Hid::GAMEPAD_STYLE_JOYLEFT);
        /*# Use with switch.hid_set_gamepad_supported_styleset
         * @name switch.HID_GAMEPAD_STYLE_JOYRIGHT
         * @variable
         */
        SETCONSTANT(HID_GAMEPAD_STYLE_JOYRIGHT, dmSwitch::Hid::GAMEPAD_STYLE_JOYRIGHT);


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
            dmSwitch::Account::GetUser(&g_LastOpenedUser); // there should already be selected user at startup
            dmSwitch::Account::MountUserSaveData(&g_LastOpenedUser);
        }

        int enable_cache_storage = dmConfigFile::GetInt(params->m_ConfigFile, "switch.cache_storage_enabled", 1);
        if (enable_cache_storage)
        {
            nn::fs::MountCacheStorage("cache");
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

        int enable_cache_storage = dmConfigFile::GetInt(params->m_ConfigFile, "switch.cache_storage_enabled", 1);
        if (enable_cache_storage)
        {
            nn::fs::Unmount("cache");
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