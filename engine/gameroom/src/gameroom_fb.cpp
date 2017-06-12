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
        // m_AccessToken = 0x0;
        // m_AccessTokenHandle = 0x0;
    }
    int                  m_Callback;
    int                  m_Self;
    lua_State*           m_MainThread;
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

static void PushError(lua_State*L, char* error)
{
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, error);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

bool IsGameroomInitialized()
{
    bool r = fbg_IsPlatformInitialized();
    if (!r) {
        dmLogError("Facebook Gameroom is not initialized.");
    }
    return r;
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

static const char* GetTableStringValue(lua_State* L, int table_index, const char* valid_keys[])
{
    const char* r = 0x0;
    int top = lua_gettop(L);

    while (*valid_keys != 0x0) {
        const char* key = *valid_keys;
        valid_keys++;

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
    }

    assert(top == lua_gettop(L));
    return r;
}

static int GetTableIntValue(lua_State* L, int table_index, const char* valid_keys[])
{
    int r = 0x0;
    int top = lua_gettop(L);

    while (*valid_keys != 0x0) {
        const char* key = *valid_keys;
        valid_keys++;

        lua_getfield(L, table_index, key);
        if (!lua_isnil(L, -1)) {

            int actual_lua_type = lua_type(L, -1);
            if (actual_lua_type != LUA_TNUMBER) {
                dmLogError("Lua conversion expected entry '%s' to be a number but got %s",
                    key, lua_typename(L, actual_lua_type));
            } else {
                r = lua_tonumber(L, -1);
            }

        }
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
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

        const char* keys_title[] = {"caption", "title", 0x0};
        const char* content_title = GetTableStringValue(L, 2, keys_title);

        const char* keys_desc[] = {"description", 0x0};
        const char* content_desc = GetTableStringValue(L, 2, keys_desc);

        const char* keys_picture[] = {"picture", 0x0};
        const char* picture = GetTableStringValue(L, 2, keys_picture);

        const char* keys_link[] = {"link", 0x0};
        const char* link_url = GetTableStringValue(L, 2, keys_link);

        const char* keys_link_title[] = {"link_title", 0x0};
        const char* link_title = GetTableStringValue(L, 2, keys_link_title);

        const char* keys_to[] = {"to", 0x0};
        const char* to = GetTableStringValue(L, 2, keys_to);

        const char* keys_media_source[] = {"media_source", 0x0};
        const char* media_source = GetTableStringValue(L, 2, keys_media_source);

        fbg_FeedShare(
            to,
            link_url,
            link_title,
            content_title,
            content_desc,
            picture,
            media_source
        );

    } else if (dialog == dmHashString64("apprequests") || dialog == dmHashString64("apprequest")) {

        const char* keys_title[] = {"title", 0x0};
        const char* title = GetTableStringValue(L, 2, keys_title);

        const char* keys_message[] = {"message", 0x0};
        const char* message = GetTableStringValue(L, 2, keys_message);

        // content.actionType = convertGameRequestAction([GetTableValue(L, 2, @[@"action_type"], LUA_TNUMBER) unsignedIntValue]);
        const char* keys_action_type[] = {"action_type", 0x0};
        int action_type = GetTableIntValue(L, 2, keys_action_type);
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

        const char* keys_filters[] = {"filters", 0x0};
        int filters = GetTableIntValue(L, 2, keys_filters);

        const char* keys_data[] = {"data", 0x0};
        const char* data = GetTableStringValue(L, 2, keys_data);

        const char* keys_object_id[] = {"object_id", 0x0};
        const char* object_id = GetTableStringValue(L, 2, keys_object_id);

        // NSArray* suggestions = GetTableValue(L, 2, @[@"suggestions"], LUA_TTABLE);
        // if (suggestions != nil) {
        //     content.recipientSuggestions = suggestions;
        // }

        // // comply with JS way of specifying recipients/to
        // NSString* to = GetTableValue(L, 2, @[@"to"], LUA_TSTRING);
        // if (to != nil && [to respondsToSelector:@selector(componentsSeparatedByString:)]) {
        //     content.recipients = [to componentsSeparatedByString:@","];
        // }
        const char* keys_to[] = {"to", 0x0};
        const char* to = GetTableStringValue(L, 2, keys_to);

        // NSArray* recipients = GetTableValue(L, 2, @[@"recipients"], LUA_TTABLE);
        // if (recipients != nil) {
        //     content.recipients = recipients;
        // }

        const char* keys_max_recipients[] = {"max_recipients", 0x0};
        int max_recipients = GetTableIntValue(L, 2, keys_max_recipients);

        // [FBSDKGameRequestDialog showWithContent:content delegate:g_Facebook.m_Delegate];

        fbg_AppRequest(
            message, // message
            action, // actionType
            object_id, // objectId
            to, // to
            nullptr, // filters
            nullptr, // excludeIDs
            max_recipients, // maxRecipients
            data, // data
            title // title
        );

    } else {
        // char error_str[] = "Invalid dialog type";
        // RunDialogResultCallback(g_GameroomFB.m_MainThread, 0, error_str);
        // TODO call error callback
    }

    return 0;
}

static int GameroomFB_DeprecatedFunc(lua_State* L)
{
    // Deprecated function, null implementation to keep API compatibility.
    return 0;
}

static const luaL_reg GameroomFB_methods[] =
{
    {"login", GameroomFB_Login},
    {"logout", GameroomFB_DeprecatedFunc}, // Deprecated
    {"access_token", GameroomFB_AccessToken},
    {"permissions", GameroomFB_Permissions},
    {"request_read_permissions", GameroomFB_DeprecatedFunc}, // Deprecated
    {"request_publish_permissions", GameroomFB_DeprecatedFunc}, // Deprecated
    {"me", GameroomFB_DeprecatedFunc},
    // {"post_event", 0},
    // {"enable_event_usage", 0},
    // {"disable_event_usage", 0},
    {"show_dialog", GameroomFB_ShowDialog},
    {"login_with_read_permissions", GameroomFB_LoginReadPermissions},
    {"login_with_publish_permissions", GameroomFB_LoginPublishPermissions},
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

    int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running facebook login callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    CLEAR_FBG_CALLBACK(L);
}

static void PushToString(lua_State* L, const char* str, char split)
{
    lua_newtable(L);
    int i = 0;
    const char* it = str;
    while (true)
    {
        char c = *it;

        if (c == split || c == '\0')
        {
            lua_pushlstring(L, str, it - str);
            lua_rawseti(L, -2, i++);

            if (c == '\0') {
                break;
            }
        }
        it++;
    }

}

static void RunDialogResultCallback(lua_State* L, const char* request_id, const char* to)
{
    DM_LUA_STACK_CHECK(L, 0);

    if (!HasFBGCallback(L)) {
        dmLogError("No callback set for dialog result.");
        return;
    }

    if (!SetupFBGCallback(L)) {
        return;
    }

    // ERROR:GAMEROOM: app request id: '35388886169371', to: '10154772854732545,1015623268487405'
    lua_newtable(L);
    lua_pushstring(L, request_id);
    lua_setfield(L, -2, "request_id");
    PushToString(L, to, ',');
    lua_setfield(L, -2, "to");

    int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
    if (ret != 0) {
        dmLogError("Error running facebook dialog callback: %s", lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    CLEAR_FBG_CALLBACK(L);
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
            fbgFeedShareHandle feedShareHandle = fbg_Message_FeedShare(message);
            fbid postId = fbg_FeedShare_GetPostID(feedShareHandle);
            char postIdString[1024];
            fbid_ToString((char*)postIdString, 1024, postId);

            dmLogError(
              "Feed Share Post ID: %s",
              postIdString
            );
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

            RunDialogResultCallback(L, request_id, to);

            free(request_id);
            free(to);
        }
        break;
        default:
            dmLogError("Unknown FB message: %u", message_type);
        break;
    }
}
