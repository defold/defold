#ifndef GAMEROOM_PRIVATE_H
#define GAMEROOM_PRIVATE_H

#include <extension/extension.h>

#include <FBG_Platform.h>

namespace dmFBGameroom
{
    bool CheckGameroomInit();
    fbgMessageHandle PopFacebookMessage();
    fbgMessageHandle PopIAPMessage();
}


// bool IsGameroomInitialized();
// void InitGameroomIAP(dmExtension::AppParams* params);
// void InitGameroomFB(dmExtension::AppParams* params);
// void RegisterGameroomIAP(lua_State* L);
// void RegisterGameroomFB(lua_State* L);
// void HandleGameroomIAPMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type);
// void HandleGameroomFBMessages(lua_State* L, fbgMessageHandle message, fbgMessageType message_type);

#endif // GAMEROOM_PRIVATE_H
