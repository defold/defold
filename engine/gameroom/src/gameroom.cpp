#include <dlib/log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gameroom_private.h"
#include <FBG_Platform_Internal.h>

static void GameroomLogFunction(const char* tag, const char* message)
{
    dmLogDebug("Gameroom Log [%s]: %s", tag, message);
}

static dmExtension::Result InitializeGameroom(dmExtension::Params* params)
{
    // Register IAP and Facebook modules for Gameroom
    RegisterGameroomIAP(params->m_L);
    RegisterGameroomFB(params->m_L);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeGameroom(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeGameroom(dmExtension::AppParams* params)
{
    const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0);
    if( !app_id )
    {
        dmLogError("No facebook.appid. Disabling module");
        return dmExtension::RESULT_OK;
    }

    fbg_SetPlatformLogFunc(GameroomLogFunction);

    fbgPlatformInitializeResult fb_init_res = fbg_PlatformInitializeWindows(app_id);
    if (fb_init_res != fbgPlatformInitialize_Success)
    {
        dmLogError("Could not init Facebook Gameroom: %s", fbgPlatformInitializeResult_ToString(fb_init_res));
        return dmExtension::RESULT_INIT_ERROR;
    }

    InitGameroomIAP(params);
    InitGameroomFB(params);

    return dmExtension::RESULT_OK;
}


static dmExtension::Result UpdateGameroom(dmExtension::Params* params)
{
    lua_State* L = params->m_L;

    fbgMessageHandle message;
    while ((message = fbg_PopMessage()) != nullptr) {
        fbgMessageType message_type = fbg_Message_GetType(message);
        switch (message_type) {

            case fbgMessage_AccessToken:
            case fbgMessage_FeedShare:
            case fbgMessage_AppRequest:
                HandleGameroomFBMessages(L, message, message_type);
            break;

            case fbgMessage_Purchase:
                HandleGameroomIAPMessages(L, message, message_type);
            break;

            default:
                dmLogError("Got unkown message: %s", fbgMessageType_ToString(message_type));
            break;
        }
        fbg_FreeMessage(message);
    }

    return dmExtension::RESULT_OK;
}

// TEMP Should remove this once Facebook Gameroom client passes the correct arguments to the binary.
static dmExtension::Result EngineInitializeGameroom(dmExtension::EngineParams* params)
{
    dmArray<char*> &args = *params->m_Args;
    if (params->m_Args->Size() > 3)
    {
        args.SetSize(2);
        args[1] = "dmengine_Data/game.projectc";
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSIONV2(GameroomExt, "Gameroom", AppInitializeGameroom, 0, InitializeGameroom, UpdateGameroom, 0, FinalizeGameroom, EngineInitializeGameroom)
