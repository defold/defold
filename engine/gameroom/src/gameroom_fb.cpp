#include <dlib/log.h>
#include <script/script.h>

#include "gameroom_private.h"
#include "../../facebook/src/facebook_private.h"

struct GameroomFB
{
    GameroomFB()
    {
        memset(this, 0, sizeof(*this));
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_MainThread = NULL;
        m_DisableFaceBookEvents = 0;
        // m_AccessToken = 0x0;
        // m_AccessTokenHandle = 0x0;
    }
    int        m_Callback;
    int        m_Self;
    lua_State* m_MainThread;
    int        m_DisableFaceBookEvents;
    // char*                m_AccessToken;
    // fbgAccessTokenHandle m_AccessTokenHandle;

} g_GameroomFB;

#define SET_FBG_CALLBACK(L, index) \
{ \
    lua_pushvalue(L, index); \
    g_GameroomFB.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX); \
 \
    dmScript::GetInstance(L); \
    g_GameroomFB.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX); \
    g_GameroomFB.m_MainThread = dmScript::GetMainThread(L); \
}

#define CLEAR_FBG_CALLBACK(L) \
{ \
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_GameroomFB.m_Callback); \
    dmScript::Unref(L, LUA_REGISTRYINDEX, g_GameroomFB.m_Self); \
    g_GameroomFB.m_Callback = LUA_NOREF; \
    g_GameroomFB.m_Self = LUA_NOREF; \
    g_GameroomFB.m_MainThread = NULL; \
}

static bool SetupFBGCallback(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomFB.m_Callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_GameroomFB.m_Self);
    lua_pushvalue(L, -1);
    dmScript::SetInstance(L);

    if (!dmScript::IsInstanceValid(L))
    {
        dmLogError("Could not run facebook callback because the instance has been deleted.");
        lua_pop(L, 2);
        return false;
    }

    return true;
}

static bool HasFBGCallback(lua_State* L)
{
    if (g_GameroomFB.m_Callback == LUA_NOREF ||
        g_GameroomFB.m_Self == LUA_NOREF ||
        g_GameroomFB.m_MainThread == NULL) {
        return false;
    }
    return true;
}

bool IsGameroomInitialized()
{
    bool r = fbg_IsPlatformInitialized();
    if (!r) {
        dmLogError("Facebook Gameroom is not initialized.");
    }
    return r;
}


static void RunLoginResultCallback(lua_State* L, int result, const char* error)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasFBGCallback(L)) {
        dmLogError("No callback set for login result.");
        return;
    }

    if (!SetupFBGCallback(L)) {
        return;
    }

    lua_newtable(L);
    lua_pushnumber(L, result);
    lua_setfield(L, -2, "status");
    if (error) {
        lua_pushstring(L, error);
        lua_setfield(L, -2, "error");
    }

    int ret = lua_pcall(L, 2, 0, 0);
    if (ret != 0) {
        dmLogError("Error running facebook login callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    CLEAR_FBG_CALLBACK(L);
}

static void ParseToTable(lua_State* L, int table_index, const char* str, char split)
{
    int i = 1;
    const char* it = str;
    while (true)
    {
        char c = *it;

        if (c == split || c == '\0')
        {
            lua_pushlstring(L, str, it - str);
            lua_rawseti(L, table_index, i++);
            if (c == '\0') {
                break;
            }

            str = it+1;
        }
        it++;
    }
}

static void RunAppRequestCallback(lua_State* L, const char* request_id, const char* to)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasFBGCallback(L)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    if (!SetupFBGCallback(L)) {
        return;
    }

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
    lua_pushstring(L, request_id);
    lua_setfield(L, -2, "request_id");
    lua_newtable(L);
    if (strlen(to) > 0) {
        ParseToTable(L, lua_gettop(L), to, ',');
    }
    lua_setfield(L, -2, "to");

    int ret = lua_pcall(L, 2, 0, 0);
    if (ret != 0) {
        dmLogError("Error running facebook dialog callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    CLEAR_FBG_CALLBACK(L);
}

static void RunFeedCallback(lua_State* L, const char* post_id)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasFBGCallback(L)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    if (!SetupFBGCallback(L)) {
        return;
    }

    // result table in a feed dialog callback looks like this;
    // result = {
    //     post_id = "post_id_str"
    // }
    lua_newtable(L);
    lua_pushstring(L, post_id);
    lua_setfield(L, -2, "post_id");

    int ret = lua_pcall(L, 2, 0, 0);
    if (ret != 0) {
        dmLogError("Error running facebook dialog callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    CLEAR_FBG_CALLBACK(L);
}

static void RunDialogErrorCallback(lua_State* L, const char* error_str)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasFBGCallback(L)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    if (!SetupFBGCallback(L)) {
        return;
    }

    // Push a table with an "error" field with the error string.
    lua_newtable(L);
    lua_pushstring(L, "error");
    lua_pushstring(L, error_str);
    lua_rawset(L, -3);

    int ret = lua_pcall(L, 2, 0, 0);
    if (ret != 0) {
        dmLogError("Error running facebook dialog callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    CLEAR_FBG_CALLBACK(L);
}

static int GameroomFB_Login(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 0);

    fbg_Login();

    return 0;
}

static void LoginWithScopes(lua_State* L, int table_index)
{
    size_t arr_len = lua_objlen(L, table_index);
    fbgLoginScope* login_scopes = (fbgLoginScope*)malloc(arr_len * sizeof(fbgLoginScope));

    size_t i = 0;
    lua_pushnil(L);
    while (lua_next(L, table_index))
    {
        if (lua_isstring(L, -1))
        {
            if (i < arr_len)
            {
                const char* permission = lua_tostring(L, -1);

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
        }

        lua_pop(L, 1);
    }

    fbg_Login_WithScopes(
      i,
      login_scopes
    );

    free(login_scopes);
}

static int GameroomFB_LoginReadPermissions(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 0);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    SET_FBG_CALLBACK(L, 2);

    LoginWithScopes(L, 1);
    return 0;
}

static int GameroomFB_LoginPublishPermissions(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 0);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION); // Stack index 2 not used since "audience" parameter is not supported in Gameroom.
    SET_FBG_CALLBACK(L, 3);

    LoginWithScopes(L, 1);
    return 0;
}

static int GameroomFB_AccessToken(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 1);

    // No access token available? Return empty string.
    fbgAccessTokenHandle access_token_handle = fbg_AccessToken_GetActiveAccessToken();
    if (!access_token_handle || !fbg_AccessToken_IsValid(access_token_handle)) {
        lua_pushstring(L, "");
        return 1;
    }

    size_t access_token_size = fbg_AccessToken_GetTokenString(access_token_handle, 0, 0);
    char* access_token_str = (char*)malloc(access_token_size * sizeof(char));
    fbg_AccessToken_GetTokenString(access_token_handle, access_token_str, access_token_size);
    lua_pushstring(L, access_token_str);
    free(access_token_str);

    return 1;
}

static int GameroomFB_Permissions(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 1);

    int top = lua_gettop(L);

    lua_newtable(L);

    // If there is no access token, push an empty table.
    fbgAccessTokenHandle access_token_handle = fbg_AccessToken_GetActiveAccessToken();
    if (!access_token_handle || !fbg_AccessToken_IsValid(access_token_handle)) {
        return 1;
    }

    // Initial call to figure out how many permissions we need to allocate for.
    size_t permission_count = fbg_AccessToken_GetPermissions(access_token_handle, 0, 0);

    fbgLoginScope* permissions = (fbgLoginScope*)malloc(permission_count * sizeof(fbgLoginScope));
    fbg_AccessToken_GetPermissions(access_token_handle, permissions, permission_count);

    for (size_t i = 0; i < permission_count; ++i) {
        lua_pushnumber(L, i);
        lua_pushstring(L, fbgLoginScope_ToString(permissions[i]));
        lua_rawset(L, -3);
    }

    free(permissions);

    return 1;
}

static const char* GetTableStringValue(lua_State* L, int table_index, const char* key)
{
    const char* r = 0x0;

    lua_getfield(L, table_index, key);
    if (!lua_isnil(L, -1)) {

        int actual_lua_type = lua_type(L, -1);
        if (actual_lua_type != LUA_TSTRING) {
            dmLogError("Lua conversion expected entry '%s' to be a string but got %s",
                key, lua_typename(L, actual_lua_type));
        } else {
            r = lua_tostring(L, -1);
        }

    }
    lua_pop(L, 1);

    return r;
}

static int GetTableIntValue(lua_State* L, int table_index, const char* key)
{
    int r = 0x0;

    lua_getfield(L, table_index, key);
    if (!lua_isnil(L, -1)) {

        int actual_lua_type = lua_type(L, -1);
        if (actual_lua_type != LUA_TNUMBER) {
            dmLogError("Lua conversion expected entry '%s' to be a number but got %s",
                key, lua_typename(L, actual_lua_type));
        } else {
            r = lua_tointeger(L, -1);
        }

    }
    lua_pop(L, 1);

    return r;
}

static int GameroomFB_ShowDialog(lua_State* L)
{
    CHECK_GAMEROOM_INIT();
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t dialog = dmHashString64(luaL_checkstring(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    g_GameroomFB.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::GetInstance(L);
    g_GameroomFB.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_GameroomFB.m_MainThread = dmScript::GetMainThread(L);

    if (dialog == dmHashString64("feed")) {

        // For compability, we check if either "caption" or "title" is set.
        const char* content_title = GetTableStringValue(L, 2, "caption");
        if (!content_title) {
            content_title = GetTableStringValue(L, 2, "title");
        }

        fbg_FeedShare(
            GetTableStringValue(L, 2, "to"),
            GetTableStringValue(L, 2, "link"),
            GetTableStringValue(L, 2, "link_title"),
            content_title,
            GetTableStringValue(L, 2, "description"),
            GetTableStringValue(L, 2, "picture"),
            GetTableStringValue(L, 2, "media_source")
        );

    } else if (dialog == dmHashString64("apprequests") || dialog == dmHashString64("apprequest")) {

        // TODO Waiting for response from Inga/FBG how to correctly handle action type.
        int action_type = GetTableIntValue(L, 2, "action_type");
        const char* action = 0x0;
        switch (action_type)
        {
            case dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND:
                action = "send";
            break;
            case dmFacebook::GAMEREQUEST_ACTIONTYPE_ASKFOR:
                action = "askfor";
            break;
            case dmFacebook::GAMEREQUEST_ACTIONTYPE_TURN:
                action = "turn";
            break;
        }

        // int filters = GetTableIntValue(L, 2, "filters"); // TODO Waiting on response from Inga/FBG team.
        // NSArray* suggestions = GetTableValue(L, 2, @[@"suggestions"], LUA_TTABLE);
        // if (suggestions != nil) {
        //     content.recipientSuggestions = suggestions;
        // }

        // // comply with JS way of specifying recipients/to
        // NSString* to = GetTableValue(L, 2, @[@"to"], LUA_TSTRING);
        // if (to != nil && [to respondsToSelector:@selector(componentsSeparatedByString:)]) {
        //     content.recipients = [to componentsSeparatedByString:@","];
        // }

        // NSArray* recipients = GetTableValue(L, 2, @[@"recipients"], LUA_TTABLE);
        // if (recipients != nil) {
        //     content.recipients = recipients;
        // }

        // [FBSDKGameRequestDialog showWithContent:content delegate:g_Facebook.m_Delegate];

        fbg_AppRequest(
            GetTableStringValue(L, 2, "message"), // message
            action, // actionType
            GetTableStringValue(L, 2, "object_id"), // objectId
            GetTableStringValue(L, 2, "to"), // to
            nullptr, // filters
            nullptr, // excludeIDs
            GetTableIntValue(L, 2, "max_recipients"), // maxRecipients
            GetTableStringValue(L, 2, "data"), // data
            GetTableStringValue(L, 2, "title") // title
        );

    } else {
        RunDialogErrorCallback(g_GameroomFB.m_MainThread, "Invalid dialog type");
    }

    return 0;
}

static int GameroomFB_PostEvent(lua_State* L)
{
#if 0
    int argc = lua_gettop(L);
    const char* event = dmFacebook::Analytics::GetEvent(L, 1);
    double valueToSum = luaL_checknumber(L, 2);

    // Transform LUA table to a format that can be used by all platforms.
    const char* keys[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    const char* values[dmFacebook::Analytics::MAX_PARAMS] = { 0 };
    unsigned int length = 0;
    // TABLE is an optional argument and should only be parsed if provided.
    if (argc == 3)
    {
        length = dmFacebook::Analytics::MAX_PARAMS;
        dmFacebook::Analytics::GetParameterTable(L, 3, keys, values, &length);
    }

    // Prepare for Objective-C
    NSString* objcEvent = [NSString stringWithCString:event encoding:NSASCIIStringEncoding];
    NSMutableDictionary* params = [NSMutableDictionary dictionary];
    for (unsigned int i = 0; i < length; ++i)
    {
        NSString* objcKey = [NSString stringWithCString:keys[i] encoding:NSASCIIStringEncoding];
        NSString* objcValue = [NSString stringWithCString:values[i] encoding:NSASCIIStringEncoding];

        params[objcKey] = objcValue;
    }

    [FBSDKAppEvents logEvent:objcEvent valueToSum:valueToSum parameters:params];
#endif

    const fbgFormDataHandle form_data_handle = fbg_FormData_CreateNew();
    fbg_FormData_Set(form_data_handle, "abc", 3, "def", 3);
    fbg_FormData_Set(form_data_handle, "ghi", 3, "jkl", 3);
    fbg_LogAppEventWithValueToSum(
        "libfbgplatform",
        form_data_handle,
        1
    );

    return 0;
}

// Stub function used to expose previously deprecated functions in the 'facebook.*' API.
static int GameroomFB_DeprecatedFunc(lua_State* L)
{
    // Deprecated function, null implementation to keep API compatibility.
    return 0;
}

static const luaL_reg GameroomFB_methods[] =
{
    {"login", GameroomFB_Login},
    {"access_token", GameroomFB_AccessToken},
    {"permissions", GameroomFB_Permissions},
    {"post_event", GameroomFB_PostEvent},
    // {"enable_event_usage", 0},
    // {"disable_event_usage", 0},
    {"show_dialog", GameroomFB_ShowDialog},
    {"login_with_read_permissions", GameroomFB_LoginReadPermissions},
    {"login_with_publish_permissions", GameroomFB_LoginPublishPermissions},

    // Deprecated functions
    {"logout", GameroomFB_DeprecatedFunc},
    {"request_read_permissions", GameroomFB_DeprecatedFunc},
    {"request_publish_permissions", GameroomFB_DeprecatedFunc},
    {"me", GameroomFB_DeprecatedFunc},
    {0, 0}
};

void RegisterGameroomFB(lua_State* L)
{
    luaL_register(L, "facebook", GameroomFB_methods);

#define SETCONSTANT(name, val) \
        lua_pushnumber(L, (lua_Number) val); \
        lua_setfield(L, -2, #name);\

    SETCONSTANT(STATE_CREATED,              dmFacebook::STATE_CREATED);
    SETCONSTANT(STATE_CREATED_TOKEN_LOADED, dmFacebook::STATE_CREATED_TOKEN_LOADED);
    SETCONSTANT(STATE_CREATED_OPENING,      dmFacebook::STATE_CREATED_OPENING);
    SETCONSTANT(STATE_OPEN,                 dmFacebook::STATE_OPEN);
    SETCONSTANT(STATE_OPEN_TOKEN_EXTENDED,  dmFacebook::STATE_OPEN_TOKEN_EXTENDED);
    SETCONSTANT(STATE_CLOSED,               dmFacebook::STATE_CLOSED);
    SETCONSTANT(STATE_CLOSED_LOGIN_FAILED,  dmFacebook::STATE_CLOSED_LOGIN_FAILED);

    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_NONE,   dmFacebook::GAMEREQUEST_ACTIONTYPE_NONE);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_SEND,   dmFacebook::GAMEREQUEST_ACTIONTYPE_SEND);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_ASKFOR, dmFacebook::GAMEREQUEST_ACTIONTYPE_ASKFOR);
    SETCONSTANT(GAMEREQUEST_ACTIONTYPE_TURN,   dmFacebook::GAMEREQUEST_ACTIONTYPE_TURN);

    SETCONSTANT(GAMEREQUEST_FILTER_NONE,        dmFacebook::GAMEREQUEST_FILTER_NONE);
    SETCONSTANT(GAMEREQUEST_FILTER_APPUSERS,    dmFacebook::GAMEREQUEST_FILTER_APPUSERS);
    SETCONSTANT(GAMEREQUEST_FILTER_APPNONUSERS, dmFacebook::GAMEREQUEST_FILTER_APPNONUSERS);

    SETCONSTANT(AUDIENCE_NONE,     dmFacebook::AUDIENCE_NONE);
    SETCONSTANT(AUDIENCE_ONLYME,   dmFacebook::AUDIENCE_ONLYME);
    SETCONSTANT(AUDIENCE_FRIENDS,  dmFacebook::AUDIENCE_FRIENDS);
    SETCONSTANT(AUDIENCE_EVERYONE, dmFacebook::AUDIENCE_EVERYONE);

#undef SETCONSTANT

    lua_pop(L, 1);
}

void InitGameroomFB(dmExtension::AppParams* params)
{
    g_GameroomFB.m_DisableFaceBookEvents = dmConfigFile::GetInt(params->m_ConfigFile, "facebook.disable_events", 0);

    if(!g_GameroomFB.m_DisableFaceBookEvents)
    {
        fbg_ActivateApp();
    }
}

void HandleGameroomFBMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type)
{
    switch (message_type)
    {
        case fbgMessage_AccessToken: {
            fbgAccessTokenHandle access_token = fbg_Message_AccessToken(message);

            // TODO Find a better way to know if a login went as expected or not.
            if (fbg_AccessToken_IsValid(access_token))
            {
                RunLoginResultCallback(L, dmFacebook::STATE_OPEN, 0x0);
            } else {
                RunLoginResultCallback(L, dmFacebook::STATE_CLOSED_LOGIN_FAILED, "Login failed");
            }
        break;
        }
        case fbgMessage_FeedShare: {
            fbgFeedShareHandle feed_share_handle = fbg_Message_FeedShare(message);
            fbid post_id = fbg_FeedShare_GetPostID(feed_share_handle);
            if (post_id != invalidRequestID)
            {
                char post_id_str[128];
                fbid_ToString((char*)post_id_str, 128, post_id);
                RunFeedCallback(L, post_id_str);
            } else {
                RunDialogErrorCallback(L, "Error while posting to feed.");
            }
        break;
        }
        case fbgMessage_AppRequest: {
            fbgAppRequestHandle app_request = fbg_Message_AppRequest(message);

            // Get app request id
            size_t request_id_size = fbg_AppRequest_GetRequestObjectId(app_request, 0, 0);
            char* request_id = (char*)malloc(request_id_size * sizeof(char));
            fbg_AppRequest_GetRequestObjectId(app_request, request_id, request_id_size);

            // Get "to" list
            size_t to_size = fbg_AppRequest_GetTo(app_request, 0, 0);
            char* to = (char*)malloc(to_size * sizeof(char));
            fbg_AppRequest_GetTo(app_request, to, to_size);

            RunAppRequestCallback(L, request_id, to);

            free(request_id);
            free(to);
        }
        break;
        default:
            dmLogError("Unknown FB message: %u", message_type);
        break;
    }
}
