#ifndef GAMEROOM_PRIVATE_H
#define GAMEROOM_PRIVATE_H

#include <extension/extension.h>

#include "FBG_Platform.h"

#define CHECK_GAMEROOM_INIT() \
{ \
    bool r = fbg_IsPlatformInitialized(); \
    if (!r) { \
        dmLogError("Facebook Gameroom is not initialized."); \
        return 0; \
    } \
}

// bool IsGameroomInitialized();
void InitGameroomIAP(dmExtension::AppParams* params);
void InitGameroomFB(dmExtension::AppParams* params);
void RegisterGameroomIAP(lua_State* L);
void RegisterGameroomFB(lua_State* L);
void HandleGameroomIAPMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type);
void HandleGameroomFBMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type);

#endif // GAMEROOM_PRIVATE_H
