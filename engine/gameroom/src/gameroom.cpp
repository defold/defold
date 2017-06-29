#include <dlib/log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <extension/extension.h>

#include "gameroom.h"

#include <FBG_Platform_Internal.h>

struct FBGameroom
{
    FBGameroom() {
        memset(this, 0x0, sizeof(FBGameroom));
        m_MessagesFB.SetCapacity(4);
        m_MessagesIAP.SetCapacity(4);
        m_MessagesFB_Iter = 0;
        m_MessagesIAP_Iter = 0;
        m_Enabled = false;
    }

    dmArray<fbgMessageHandle> m_MessagesFB;
    dmArray<fbgMessageHandle> m_MessagesIAP;
    int m_MessagesFB_Iter;
    int m_MessagesIAP_Iter;

    // We need to keep track if the gameroom extension is enabled
    // to avoid calling any FBG functions since they would try to
    // load the FBG DLL.
    bool m_Enabled;

} g_FBGameroom;

static void PushQueueMessage(dmArray<fbgMessageHandle> &queue, fbgMessageHandle msg)
{
    if (queue.Full()) {
        queue.OffsetCapacity(4);
    }
    queue.Push(msg);
}

static fbgMessageHandle PopQueueMessage(dmArray<fbgMessageHandle> &queue, int &iter)
{
    if (iter < queue.Size()) {
        return queue[iter++];
    }

    // No messages left to pop, reset iterator and size of queue.
    iter = 0;
    queue.SetSize(0);

    return NULL;
}

fbgMessageHandle dmFBGameroom::PopFacebookMessage()
{
    return PopQueueMessage(g_FBGameroom.m_MessagesFB, g_FBGameroom.m_MessagesFB_Iter);
}

fbgMessageHandle dmFBGameroom::PopIAPMessage()
{
    return PopQueueMessage(g_FBGameroom.m_MessagesIAP, g_FBGameroom.m_MessagesIAP_Iter);
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

    lua_State* L = params->m_L;

    fbgMessageHandle message;
    while ((message = fbg_PopMessage()) != nullptr) {
        // Freeing of 'message' must be done in Facebook and IAP modules/extensions.

        fbgMessageType message_type = fbg_Message_GetType(message);
        switch (message_type) {

            case fbgMessage_AccessToken:
            case fbgMessage_FeedShare:
            case fbgMessage_AppRequest:
                PushQueueMessage(g_FBGameroom.m_MessagesFB, message);
            break;

            case fbgMessage_Purchase:
            case fbgMessage_HasLicense:
                PushQueueMessage(g_FBGameroom.m_MessagesIAP, message);
            break;

            default:
                dmLogError("Got unkown message: %s", fbgMessageType_ToString(message_type));
            break;
        }
    }

    return dmExtension::RESULT_OK;
}

// TODO Should remove this once Facebook Gameroom client passes the correct arguments to the binary.
static char tmp_projectc_path[] = "dmengine_Data/game.projectc";
static dmExtension::Result EngineInitializeGameroom(dmExtension::EngineParams* params)
{
    dmArray<char*> &args = *params->m_Args;
    if (params->m_Args->Size() > 3)
    {
        args.SetSize(2);
        args[1] = tmp_projectc_path;
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSIONV2(GameroomExt, "Gameroom", AppInitializeGameroom, 0, InitializeGameroom, UpdateGameroom, 0, 0, EngineInitializeGameroom)
