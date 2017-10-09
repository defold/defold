#ifndef DM_FACEBOOK_H
#define DM_FACEBOOK_H

#define LIB_NAME "facebook"

#include <script/script.h>
#include "facebook_util.h"

#define DEPRECATED_FACEBOOK_FUNC(CFunc_Name, LuaFunc_name) \
int Facebook_##CFunc_Name (lua_State* L) { \
    dmLogOnceWarning("facebook.%s() is deprecated.", #LuaFunc_name); \
    return 0; \
}

#define UNAVAILBLE_FACEBOOK_FUNC(CFunc_Name, LuaFunc_name) \
int Facebook_##CFunc_Name (lua_State* L) { \
    dmLogOnceWarning("facebook.%s() not available on this platform.", #LuaFunc_name); \
    return 0; \
}

void RunCallback(lua_State* L, int* _self, int* _callback, const char* error, int status);

namespace dmFacebook {

    const char* const GRAPH_API_VERSION = "v2.6";

    enum State {
        STATE_CREATED              = 1,
        STATE_CREATED_TOKEN_LOADED = 2,
        STATE_CREATED_OPENING      = 3,
        STATE_OPEN                 = 4,
        STATE_OPEN_TOKEN_EXTENDED  = 5,
        STATE_CLOSED_LOGIN_FAILED  = 6,
        STATE_CLOSED               = 7,
    };

    enum GameRequestAction {
        GAMEREQUEST_ACTIONTYPE_NONE   = 1,
        GAMEREQUEST_ACTIONTYPE_SEND   = 2,
        GAMEREQUEST_ACTIONTYPE_ASKFOR = 3,
        GAMEREQUEST_ACTIONTYPE_TURN   = 4,
    };

    enum GameRequestFilters {
        GAMEREQUEST_FILTER_NONE        = 1,
        GAMEREQUEST_FILTER_APPUSERS    = 2,
        GAMEREQUEST_FILTER_APPNONUSERS = 3,
    };

    enum Audience {
        AUDIENCE_NONE     = 1,
        AUDIENCE_ONLYME   = 2,
        AUDIENCE_FRIENDS  = 3,
        AUDIENCE_EVERYONE = 4,
    };

/*
    Notes on facebook.show_dialog in regards to FB SDK 4

    Dialog parameters have been updated to align with field names in the
    current SDK version. Names have been chosen with a preference of JS
    field names (object_id vs objectID for example).

    Some fields that have changed names in the SDK still exist, but are
    considered deprecated (title -> caption).

    Results are forwarded as-is from each platform.

    facebook.show_dialog( dialog_type, param_table, callback_func ):

    dialog_type == "apprequests":
        Details for each parameter: https://developers.facebook.com/docs/games/services/gamerequests/v2.6#dialogparameters

                           arg     type            JS                   iOS                Android
        -                title : string [       title,                title,              setTitle ]
        -              message : string [     message,              message,            setMessage ]
        -          action_type :    int [ action_type,           actionType,         setActionType ]
        -              filters :    int [     filters,              filters,            setFilters ]
        -                 data : string [        data,                 data,               setData ]
        -            object_id : string [   object_id,             objectID,           setObjectId ]
        -          suggestions :  array [ suggestions, recipientSuggestions,        setSuggestions ]
        -                   to : string [          to,           recipients,                 setTo ]

    dialog_type == "feed":
        Details for each parameter: https://developers.facebook.com/docs/sharing/reference/feed-dialog/v2.6#params

                           arg     type            JS                   iOS                Android
        -              caption : string [     caption,         contentTitle,       setContentTitle ]
        -          description : string [ description,   contentDescription, setContentDescription ]
        -              picture : string [     picture,             imageURL,           setImageUrl ]
        -                 link : string [        link,           contentURL,         setContentUrl ]
        -           people_ids :  array [           -,            peopleIDs,          setPeopleIds ]
        -             place_id : string [           -,              placeID,            setPlaceId ]
        -                  ref : string [         ref,                  ref,                setRef ]

        Deprecated fields:
        -                title : string, use caption instead

    dialog_type == "appinvite": (Only available under iOS and Android)
        Details for each parameter: https://developers.facebook.com/docs/reference/ios/current/class/FBSDKAppInviteContent/

                           arg     type                        iOS / Android
        -                  url : string [               appLinkURL / setApplinkUrl      ]
        -    preview_image_url : string [ appInvitePreviewImageURL / setPreviewImageUrl ]

*/

    void LuaInit(lua_State* L);

    int Facebook_Login(lua_State* L);
    int Facebook_Logout(lua_State* L);
    int Facebook_AccessToken(lua_State* L);
    int Facebook_Permissions(lua_State* L);
    int Facebook_RequestReadPermissions(lua_State* L);
    int Facebook_RequestPublishPermissions(lua_State* L);
    int Facebook_Me(lua_State* L);
    int Facebook_PostEvent(lua_State* L);
    int Facebook_EnableEventUsage(lua_State* L);
    int Facebook_DisableEventUsage(lua_State* L);
    int Facebook_ShowDialog(lua_State* L);

    int Facebook_LoginWithPublishPermissions(lua_State* L);
    int Facebook_LoginWithReadPermissions(lua_State* L);

    bool PlatformFacebookInitialized();
    void PlatformFacebookLoginWithReadPermissions(lua_State* L, const char** permissions,
        uint32_t permission_count, int callback, int context, lua_State* thread);
    void PlatformFacebookLoginWithPublishPermissions(lua_State* L, const char** permissions,
        uint32_t permission_count, int audience, int callback, int context, lua_State* thread);
}

#endif // #ifndef DM_FACEBOOK_H
