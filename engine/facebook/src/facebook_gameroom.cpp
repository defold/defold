#include <dlib/log.h>
#include <script/script.h>
#include <gameroom/gameroom.h>
#include <extension/extension.h>

#include "facebook_private.h"
#include "facebook_analytics.h"


/**
 * 20170507 Dan Engelbrecht
 * This implementation uses knowledge of the insides of the dmScript::LuaCallbackInfo
 * and the API has changed. To avoid to big changes on too many platforms the old
 * lua callback utilities has been copied in to this file.
 *
 * We want to move away from this but the individual platforms also relies on the
 * specifics on the LuaCallbackInfo structs content so these changes should be done
 * in separate PR.
 */

struct LuaCallbackInfo
{
    LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};

static void RegisterCallback(lua_State* L, int index, LuaCallbackInfo* cbk)
{
    if(cbk->m_Callback != LUA_NOREF)
    {
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
    }

    cbk->m_L = dmScript::GetMainThread(L);

    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_pushvalue(L, index);
    cbk->m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
}

typedef void (*LuaCallbackUserFn)(lua_State* L, void* user_context);

static bool IsValidCallback(LuaCallbackInfo* cbk)
{
    if (cbk->m_Callback == LUA_NOREF ||
        cbk->m_Self == LUA_NOREF ||
        cbk->m_L == NULL) {
        return false;
    }
    return true;
}

static void UnregisterCallback(LuaCallbackInfo* cbk)
{
    if(cbk->m_Callback != LUA_NOREF)
    {
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
        dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
        cbk->m_Callback = LUA_NOREF;
        cbk->m_Self = LUA_NOREF;
        cbk->m_L = 0;
    }
    else
    {
        if (cbk->m_L)
            luaL_error(cbk->m_L, "Failed to unregister callback (it was not registered)");
        else
            dmLogWarning("Failed to unregister callback (it was not registered)");
    }
}

static bool InvokeCallback(LuaCallbackInfo* cbk, LuaCallbackUserFn fn, void* user_context)
{
    if(cbk->m_Callback == LUA_NOREF)
    {
        luaL_error(cbk->m_L, "Failed to invoke callback (it was not registered)");
        return false;
    }

    lua_State* L = cbk->m_L;
    DM_LUA_STACK_CHECK(L, 0);

    lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Self); // Setup self (the script instance)
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        lua_pop(L, 2);
        dmLogWarning("Invalid script instance, failed to invoke callback");
        return false;
    }

    int user_args_start = lua_gettop(L);

    if (fn)
        fn(L, user_context);

    int user_args_end = lua_gettop(L);

    int number_of_arguments = 1 + user_args_end - user_args_start; // instance + number of arguments that the user pushed
    int ret = dmScript::PCall(L, number_of_arguments, 0);
    if (ret != 0) {
        dmLogError("Error running callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
        return false;
    }

    return true;
}

struct GameroomFB
{
    LuaCallbackInfo m_Callback;
    const dmFBGameroom::FBGameroomFunctions* m_FBFunctions;
} g_GameroomFB;

static const dmhash_t g_DialogFeed = dmHashBufferNoReverse64("feed", 4);
static const dmhash_t g_DialogAppRequests = dmHashBufferNoReverse64("apprequests", 11);
static const dmhash_t g_DialogAppRequest = dmHashBufferNoReverse64("apprequest", 10);

static bool CheckFacebookInit()
{
    if (!dmFBGameroom::CheckGameroomInit())
    {
        return false;
    }

    if (!g_GameroomFB.m_FBFunctions)
    {
        g_GameroomFB.m_FBFunctions = dmFBGameroom::GetFBFunctions();
        return g_GameroomFB.m_FBFunctions != 0;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Functions for running callbacks; dialog and login results
//


struct LoginResultArguments
{
    int m_Result;
    const char* m_Error;
};

static void PutLoginResultArguments(lua_State* L, void* user_context)
{
    LoginResultArguments* args = (LoginResultArguments*)user_context;

    lua_newtable(L);
    lua_pushnumber(L, args->m_Result);
    lua_setfield(L, -2, "status");
    if (args->m_Error) {
        lua_pushstring(L, args->m_Error);
        lua_setfield(L, -2, "error");
    }
}

static void RunLoginResultCallback(lua_State* L, int result, const char* error)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!IsValidCallback(&g_GameroomFB.m_Callback)) {
        dmLogError("No callback set for login result.");
        return;
    }

    LoginResultArguments args;
    args.m_Result = result;
    args.m_Error = error;

    InvokeCallback(&g_GameroomFB.m_Callback, PutLoginResultArguments, (void*)&args);

    UnregisterCallback(&g_GameroomFB.m_Callback);
}

struct AppRequestArguments
{
    const char* m_RequestId;
    const char* m_To;
};

static void PutAppRequestArguments(lua_State* L, void* user_context)
{
    AppRequestArguments* args = (AppRequestArguments*)user_context;

    // result table in a apprequest dialog callback looks like this;
    // result = {
    //     request_id = "",
    //     to = {
    //         [1] = "fbid_str",
    //         [2] = "fbid_str",
    //         ...
    //     }
    // }
    lua_newtable(L);
    lua_pushstring(L, args->m_RequestId);
    lua_setfield(L, -2, "request_id");
    lua_newtable(L);
    if (strlen(args->m_To) > 0) {
        dmFacebook::SplitStringToTable(L, lua_gettop(L), args->m_To, ',');
    }
    lua_setfield(L, -2, "to");
}

static void RunAppRequestCallback(const char* request_id, const char* to)
{
    if (!IsValidCallback(&g_GameroomFB.m_Callback)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    AppRequestArguments args;
    args.m_RequestId = request_id;
    args.m_To = to;

    InvokeCallback(&g_GameroomFB.m_Callback, PutAppRequestArguments, (void*)&args);

    UnregisterCallback(&g_GameroomFB.m_Callback);
}

static void PutFeedArguments(lua_State* L, void* user_context)
{
    // result table in a feed dialog callback looks like this;
    // result = {
    //     post_id = "post_id_str"
    // }
    lua_newtable(L);
    lua_pushstring(L, (const char*)user_context);
    lua_setfield(L, -2, "post_id");
}

static void RunFeedCallback(const char* post_id)
{
    if (!IsValidCallback(&g_GameroomFB.m_Callback)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    InvokeCallback(&g_GameroomFB.m_Callback, PutFeedArguments, (void*)post_id);

    UnregisterCallback(&g_GameroomFB.m_Callback);
}

static void PutDialogErrorArguments(lua_State* L, void* user_context)
{
    // Push a table with an "error" field with the error string.
    lua_newtable(L);
    lua_pushstring(L, "error");
    lua_pushstring(L, (const char*)user_context);
    lua_rawset(L, -3);
}

static void RunDialogErrorCallback(const char* error_str)
{
    if (!IsValidCallback(&g_GameroomFB.m_Callback)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    InvokeCallback(&g_GameroomFB.m_Callback, PutDialogErrorArguments, (void*)error_str);

    UnregisterCallback(&g_GameroomFB.m_Callback);
}


////////////////////////////////////////////////////////////////////////////////
// Lua API
//

namespace dmFacebook {

int Facebook_Login(lua_State* L)
{
    if (!CheckFacebookInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }
    DM_LUA_STACK_CHECK(L, 0);

    RegisterCallback(L, 1, &g_GameroomFB.m_Callback);

    g_GameroomFB.m_FBFunctions->fbg_Login();

    return 0;
}

static void LoginWithScopes(const char** permissions,
    uint32_t permission_count, int callback, int context, lua_State* thread)
{
    fbgLoginScope* login_scopes = (fbgLoginScope*)malloc(permission_count * sizeof(fbgLoginScope));

    size_t i = 0;
    for (uint32_t j = 0; j < permission_count; ++j)
    {
        const char* permission = permissions[j];

        if (strcmp("public_profile", permission) == 0) {
            login_scopes[i++] = fbgLoginScope::public_profile;
        } else if (strcmp("email", permission) == 0) {
            login_scopes[i++] = fbgLoginScope::email;
        } else if (strcmp("user_friends", permission) == 0) {
            login_scopes[i++] = fbgLoginScope::user_friends;
        } else if (strcmp("publish_actions", permission) == 0) {
            login_scopes[i++] = fbgLoginScope::publish_actions;
        }
    }

    g_GameroomFB.m_FBFunctions->fbg_Login_WithScopes(i, login_scopes);

    free(login_scopes);
}

void PlatformFacebookLoginWithReadPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int callback, int context, lua_State* thread)
{
    if (!CheckFacebookInit()) {
        return;
    }
    DM_LUA_STACK_CHECK(L, 0);

    if (IsValidCallback(&g_GameroomFB.m_Callback)) {
        UnregisterCallback(&g_GameroomFB.m_Callback);
    }

    g_GameroomFB.m_Callback.m_Callback = callback;
    g_GameroomFB.m_Callback.m_Self = context;
    g_GameroomFB.m_Callback.m_L = thread;
    LoginWithScopes(permissions, permission_count, callback, context, thread);
}

void PlatformFacebookLoginWithPublishPermissions(lua_State* L, const char** permissions,
    uint32_t permission_count, int audience, int callback, int context, lua_State* thread)
{
    if (!CheckFacebookInit()) {
        return;
    }
    DM_LUA_STACK_CHECK(L, 0);

    if (IsValidCallback(&g_GameroomFB.m_Callback)) {
        UnregisterCallback(&g_GameroomFB.m_Callback);
    }

    g_GameroomFB.m_Callback.m_Callback = callback;
    g_GameroomFB.m_Callback.m_Self = context;
    g_GameroomFB.m_Callback.m_L = thread;
    LoginWithScopes(permissions, permission_count, callback, context, thread);
}

int Facebook_AccessToken(lua_State* L)
{
    if (!CheckFacebookInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    DM_LUA_STACK_CHECK(L, 1);

    // No access token available? Return empty string.
    fbgAccessTokenHandle access_token_handle = g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetActiveAccessToken();
    if (!access_token_handle || !g_GameroomFB.m_FBFunctions->fbg_AccessToken_IsValid(access_token_handle)) {
        lua_pushnil(L);
        return 1;
    }

    size_t access_token_size = g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetTokenString(access_token_handle, 0, 0) + 1;
    char* access_token_str = (char*)malloc(access_token_size * sizeof(char));
    g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetTokenString(access_token_handle, access_token_str, access_token_size);
    lua_pushstring(L, access_token_str);
    free(access_token_str);

    return 1;
}

int Facebook_Permissions(lua_State* L)
{
    if (!CheckFacebookInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    DM_LUA_STACK_CHECK(L, 1);

    lua_newtable(L);

    // If there is no access token, push an empty table.
    fbgAccessTokenHandle access_token_handle = g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetActiveAccessToken();
    if (!access_token_handle || !g_GameroomFB.m_FBFunctions->fbg_AccessToken_IsValid(access_token_handle)) {
        return 1;
    }

    // Initial call to figure out how many permissions we need to allocate for.
    size_t permission_count = g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetPermissions(access_token_handle, 0, 0);

    fbgLoginScope* permissions = (fbgLoginScope*)malloc(permission_count * sizeof(fbgLoginScope));
    g_GameroomFB.m_FBFunctions->fbg_AccessToken_GetPermissions(access_token_handle, permissions, permission_count);

    for (size_t i = 0; i < permission_count; ++i) {
        lua_pushnumber(L, i);
        lua_pushstring(L, g_GameroomFB.m_FBFunctions->fbgLoginScope_ToString(permissions[i]));
        lua_rawset(L, -3);
    }

    free(permissions);

    return 1;
}

int Facebook_ShowDialog(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!CheckFacebookInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }

    size_t dialog_str_len = 0;
    const char* dialog_str = luaL_checklstring(L, 1, &dialog_str_len);
    dmhash_t dialog = dmHashBufferNoReverse64(dialog_str, dialog_str_len);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    RegisterCallback(L, 3, &g_GameroomFB.m_Callback);

    if (dialog == g_DialogFeed) {

        // For compatibility, we check if either "caption" or "title" is set.
        const char* content_title = dmScript::GetTableStringValue(L, 2, "caption", NULL);
        if (!content_title) {
            content_title = dmScript::GetTableStringValue(L, 2, "title", NULL);
        }

        g_GameroomFB.m_FBFunctions->fbg_FeedShare(
            dmScript::GetTableStringValue(L, 2, "to", NULL),
            dmScript::GetTableStringValue(L, 2, "link", NULL),
            dmScript::GetTableStringValue(L, 2, "link_title", NULL),
            content_title,
            dmScript::GetTableStringValue(L, 2, "description", NULL),
            dmScript::GetTableStringValue(L, 2, "picture", NULL),
            dmScript::GetTableStringValue(L, 2, "media_source", NULL)
        );

    } else if (dialog == g_DialogAppRequests || dialog == g_DialogAppRequest) {

        int action_type = dmScript::GetTableIntValue(L, 2, "action_type", GAMEREQUEST_ACTIONTYPE_NONE);
        const char* action = 0x0;
        switch (action_type)
        {
            case GAMEREQUEST_ACTIONTYPE_SEND:
                action = "send";
            break;
            case GAMEREQUEST_ACTIONTYPE_ASKFOR:
                action = "askfor";
            break;
            case GAMEREQUEST_ACTIONTYPE_TURN:
                action = "turn";
            break;
        }

        int filters = dmScript::GetTableIntValue(L, 2, "filters", 0);
        const char* filters_str = 0x0;
        switch (filters)
        {
            case GAMEREQUEST_FILTER_APPUSERS:
                filters_str = "app_users";
            break;
            case GAMEREQUEST_FILTER_APPNONUSERS:
                filters_str = "app_non_users";
            break;
            default:
                filters_str = 0x0;
            break;
        }

        char* to_str = (char*)dmScript::GetTableStringValue(L, 2, "to", NULL);

        // Check if recipients is set, it will override "to" field.
        lua_getfield(L, 2, "recipients");
        int top = lua_gettop(L);
        int has_recipients = lua_istable(L, top);
        if (has_recipients) {
            size_t entry_count = 0;
            size_t buf_size = CountStringArrayLength(L, top, entry_count);
            buf_size += entry_count;
            to_str = (char*)malloc(buf_size);
            LuaStringCommaArray(L, top, to_str, buf_size);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "exclude_ids");
        top = lua_gettop(L);
        int has_exclude_ids = lua_istable(L, top);
        char* exclude_ids = 0x0;
        if (has_exclude_ids) {
            size_t entry_count = 0;
            size_t buf_size = CountStringArrayLength(L, top, entry_count);
            buf_size += entry_count;
            exclude_ids = (char*)malloc(buf_size);
            LuaStringCommaArray(L, top, exclude_ids, buf_size);
        }
        lua_pop(L, 1);

        g_GameroomFB.m_FBFunctions->fbg_AppRequest(
            dmScript::GetTableStringValue(L, 2, "message", NULL),
            action,
            dmScript::GetTableStringValue(L, 2, "object_id", NULL),
            to_str,
            filters_str,
            exclude_ids,
            dmScript::GetTableIntValue(L, 2, "max_recipients", 0),
            dmScript::GetTableStringValue(L, 2, "data", NULL),
            dmScript::GetTableStringValue(L, 2, "title", NULL)
        );

        if (has_recipients) {
            free(to_str);
        }

        if (has_exclude_ids) {
            free(exclude_ids);
        }

    } else {
        RunDialogErrorCallback("Invalid dialog type.");
    }

    return 0;
}

int Facebook_PostEvent(lua_State* L)
{
    if (!CheckFacebookInit()) {
        return luaL_error(L, "Facebook Gameroom module isn't initialized! Did you set the windows.iap_provider in game.project?");
    }
    DM_LUA_STACK_CHECK(L, 0);

    const char* event = Analytics::GetEvent(L, 1);
    float value_to_sum = (float)luaL_checknumber(L, 2);
    const fbgFormDataHandle form_data_handle = g_GameroomFB.m_FBFunctions->fbg_FormData_CreateNew();

    // Table is an optional argument and should only be parsed if provided.
    if (lua_gettop(L) >= 3)
    {
        // Transform Lua table to a format that can be used by all platforms.
        char* keys[Analytics::MAX_PARAMS];
        char* values[Analytics::MAX_PARAMS];
        unsigned int length = Analytics::MAX_PARAMS;
        Analytics::GetParameterTable(L, 3, (const char**)keys, (const char**)values, &length);

        // Prepare for Gameroom API
        for (unsigned int i = 0; i < length; ++i)
        {
            g_GameroomFB.m_FBFunctions->fbg_FormData_Set(form_data_handle, keys[i], strlen(keys[i]), values[i], strlen(values[i]));
        }
    }

    g_GameroomFB.m_FBFunctions->fbg_LogAppEventWithValueToSum(event, form_data_handle, value_to_sum);

    return 0;
}

bool PlatformFacebookInitialized()
{
    return dmFBGameroom::CheckGameroomInit();
}

////////////////////////////////////////////////////////////////////////////////
// Deprecated, or unavailable, functions, null implementations to keep API compatibility.
//

UNAVAILBLE_FACEBOOK_FUNC(Logout, logout);
UNAVAILBLE_FACEBOOK_FUNC(EnableEventUsage, enable_event_usage);
UNAVAILBLE_FACEBOOK_FUNC(DisableEventUsage, disable_event_usage);
DEPRECATED_FACEBOOK_FUNC(Me, me);
DEPRECATED_FACEBOOK_FUNC(RequestReadPermissions, request_read_permissions);
DEPRECATED_FACEBOOK_FUNC(RequestPublishPermissions, request_publish_permissions);

} // namespace dmFacebook


////////////////////////////////////////////////////////////////////////////////
// Extension functions
//

static dmExtension::Result AppInitializeFacebook(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeFacebook(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeFacebook(dmExtension::Params* params)
{
    const char* iap_provider = dmConfigFile::GetString(params->m_ConfigFile, "windows.iap_provider", 0);
    if (iap_provider != 0x0 && strcmp(iap_provider, "Gameroom") == 0)
    {
        dmFacebook::LuaInit(params->m_L);
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateFacebook(dmExtension::Params* params)
{
    if (!dmFBGameroom::CheckGameroomInit())
    {
        return dmExtension::RESULT_OK;
    }

    if (!g_GameroomFB.m_FBFunctions)
    {
        g_GameroomFB.m_FBFunctions = dmFBGameroom::GetFBFunctions();
    }

    lua_State* L = params->m_L;

    dmArray<fbgMessageHandle>* messages = dmFBGameroom::GetFacebookMessages();
    for (uint32_t i = 0; i < messages->Size(); ++i)
    {
        fbgMessageHandle message = (*messages)[i];
        fbgMessageType message_type = g_GameroomFB.m_FBFunctions->fbg_Message_GetType(message);
        switch (message_type) {
            case fbgMessage_AccessToken: {

                fbgAccessTokenHandle access_token = g_GameroomFB.m_FBFunctions->fbg_Message_AccessToken(message);
                if (g_GameroomFB.m_FBFunctions->fbg_AccessToken_IsValid(access_token))
                {
                    RunLoginResultCallback(L, dmFacebook::STATE_OPEN, 0x0);
                } else {
                    RunLoginResultCallback(L, dmFacebook::STATE_CLOSED_LOGIN_FAILED, "Login was failed.");
                }

            break;
            }
            case fbgMessage_FeedShare: {

                fbgFeedShareHandle feed_share_handle = g_GameroomFB.m_FBFunctions->fbg_Message_FeedShare(message);
                fbid post_id = g_GameroomFB.m_FBFunctions->fbg_FeedShare_GetPostID(feed_share_handle);

                // If the post id is invalid, we interpret it as the dialog was closed
                // since there is no other way to know if it was closed or not.
                if (post_id != fbgInvalidRequestID)
                {
                    char post_id_str[128];
                    g_GameroomFB.m_FBFunctions->fbid_ToString((char*)post_id_str, 128, post_id);
                    RunFeedCallback(post_id_str);
                } else {
                    RunDialogErrorCallback("Feed share dialog failed.");
                }

            break;
            }
            case fbgMessage_AppRequest: {

                fbgAppRequestHandle app_request = g_GameroomFB.m_FBFunctions->fbg_Message_AppRequest(message);

                // Get app request id
                size_t request_id_size = g_GameroomFB.m_FBFunctions->fbg_AppRequest_GetRequestObjectId(app_request, 0, 0);
                if (request_id_size > 0) {
                    request_id_size += 1; // Include nullterm
                    char* request_id = (char*)malloc(request_id_size * sizeof(char));
                    g_GameroomFB.m_FBFunctions->fbg_AppRequest_GetRequestObjectId(app_request, request_id, request_id_size);

                    // Get "to" list
                    size_t to_size = g_GameroomFB.m_FBFunctions->fbg_AppRequest_GetTo(app_request, 0, 0) + 1;
                    char* to = (char*)malloc(to_size * sizeof(char));
                    g_GameroomFB.m_FBFunctions->fbg_AppRequest_GetTo(app_request, to, to_size);

                    RunAppRequestCallback(request_id, to);

                    free(request_id);
                    free(to);
                } else {
                    RunDialogErrorCallback("App request dialog failed.");
                }

            }
            break;
            default:
                dmLogError("Unknown FB message: %u", message_type);
            break;
        }

        g_GameroomFB.m_FBFunctions->fbg_FreeMessage(message);
    }
    messages->SetSize(0);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", AppInitializeFacebook, AppFinalizeFacebook, InitializeFacebook, UpdateFacebook, 0, 0)
