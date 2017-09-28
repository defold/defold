#include <dlib/log.h>
#include <dlib/array.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <extension/extension.h>

#include "gameroom.h"

#include <FBG_Platform_Internal.h>

struct FBGameroom
{
    FBGameroom() {
        m_MessagesFB.SetCapacity(4);
        m_MessagesIAP.SetCapacity(4);
        m_Enabled = false;
    }

    dmArray<fbgMessageHandle> m_MessagesFB;
    dmArray<fbgMessageHandle> m_MessagesIAP;

    // We need to keep track if the gameroom extension is enabled
    // to avoid calling any FBG functions since they would try to
    // load the FBG DLL.
    bool m_Enabled;

} g_FBGameroom;

static void PushMessage(dmArray<fbgMessageHandle> &queue, fbgMessageHandle msg)
{
    if (queue.Full()) {
        queue.OffsetCapacity(4);
    }
    queue.Push(msg);
}

dmArray<fbgMessageHandle>* dmFBGameroom::GetFacebookMessages()
{
    return &g_FBGameroom.m_MessagesFB;
}

dmArray<fbgMessageHandle>* dmFBGameroom::GetIAPMessages()
{
    return &g_FBGameroom.m_MessagesIAP;
}

bool dmFBGameroom::CheckGameroomInit()
{
    if (!g_FBGameroom.m_Enabled) {
        return false;
    }

    if (!fbg_IsPlatformInitialized()) {
        dmLogOnceError("Facebook Gameroom is not initialized.");
        return false;
    }
    return true;
}

static void GameroomLogFunction(const char* tag, const char* message)
{
    dmLogDebug("Gameroom Log [%s]: %s", tag, message);
}

static dmExtension::Result InitializeGameroom(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppInitializeGameroom(dmExtension::AppParams* params)
{
    const char* iap_provider = dmConfigFile::GetString(params->m_ConfigFile, "windows.iap_provider", 0);
    if (iap_provider != 0x0 && strcmp(iap_provider, "Gameroom") == 0)
    {
        g_FBGameroom.m_Enabled = true;

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
    }

    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateGameroom(dmExtension::Params* params)
{
    if (!g_FBGameroom.m_Enabled || !fbg_IsPlatformInitialized())
    {
        return dmExtension::RESULT_OK;
    }

    fbgMessageHandle message;
    while ((message = fbg_PopMessage()) != nullptr) {
        // Freeing of 'message' must be done in Facebook and IAP modules/extensions.

        fbgMessageType message_type = fbg_Message_GetType(message);
        switch (message_type) {

            case fbgMessage_AccessToken:
            case fbgMessage_FeedShare:
            case fbgMessage_AppRequest:
                PushMessage(g_FBGameroom.m_MessagesFB, message);
            break;

            case fbgMessage_Purchase:
            case fbgMessage_HasLicense:
                PushMessage(g_FBGameroom.m_MessagesIAP, message);
            break;

            default:
                dmLogError("Got unkown message: %s", fbgMessageType_ToString(message_type));
            break;
        }
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(GameroomExt, "Gameroom", AppInitializeGameroom, 0, InitializeGameroom, UpdateGameroom, 0, 0)
