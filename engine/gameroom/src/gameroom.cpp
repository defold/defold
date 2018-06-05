#include <dlib/log.h>
#include <dlib/array.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <extension/extension.h>

#include "gameroom.h"

#include <FBG_Platform_Internal.h>
#include <FBG_Platform.h>
#include <Windows.h>

#if defined(fbg_PlatformInitializeWindows)
    #undef fbg_PlatformInitializeWindows
#endif

HMODULE g_FBGLibrary = 0;
dmFBGameroom::FBGameroomFunctions g_FBFunctions;

static bool LoadFBGameRoom()
{
#if defined(_WIN64)
    const char* path = "LibFBGPlatform64.dll";
#else
    const char* path = "LibFBGPlatform32.dll";
#endif
    g_FBGLibrary = LoadLibraryA(path);
    if (!g_FBGLibrary) {
        dmLogError("Failed to load %s", path);
    }

    memset(&g_FBFunctions, 0, sizeof(g_FBFunctions));

#define LOAD_FUNC(_NAME) \
    g_FBFunctions. _NAME = (dmFBGameroom:: _NAME ## _Function)GetProcAddress(g_FBGLibrary, #_NAME); \
    if (g_FBFunctions. _NAME == 0) \
    { \
        FreeLibrary(g_FBGLibrary); \
        g_FBGLibrary = 0; \
        dmLogError("Failed to get function address: %s", #_NAME); \
        return false; \
    }

    LOAD_FUNC(fbg_AccessToken_GetActiveAccessToken);
    LOAD_FUNC(fbg_AccessToken_GetPermissions);
    LOAD_FUNC(fbg_AccessToken_GetTokenString);
    LOAD_FUNC(fbg_AccessToken_IsValid);
    LOAD_FUNC(fbg_AppRequest);
    LOAD_FUNC(fbg_AppRequest_GetRequestObjectId);
    LOAD_FUNC(fbg_AppRequest_GetTo);
    LOAD_FUNC(fbg_FeedShare);
    LOAD_FUNC(fbg_FeedShare_GetPostID);
    LOAD_FUNC(fbg_FormData_CreateNew);
    LOAD_FUNC(fbg_FormData_Set);
    LOAD_FUNC(fbg_FreeMessage);
    LOAD_FUNC(fbg_HasLicense);
    LOAD_FUNC(fbg_HasLicense_GetHasLicense);
    LOAD_FUNC(fbg_IsPlatformInitialized);
    LOAD_FUNC(fbg_LogAppEventWithValueToSum);
    LOAD_FUNC(fbg_Login);
    LOAD_FUNC(fbg_Login_WithScopes);
    LOAD_FUNC(fbg_Message_AccessToken);
    LOAD_FUNC(fbg_Message_AppRequest);
    LOAD_FUNC(fbg_Message_FeedShare);
    LOAD_FUNC(fbg_Message_GetType);
    LOAD_FUNC(fbg_Message_HasLicense);
    LOAD_FUNC(fbg_Message_Purchase);
    LOAD_FUNC(fbg_PayPremium);
    LOAD_FUNC(fbg_PlatformInitializeWindows);
    LOAD_FUNC(fbg_PopMessage);
    LOAD_FUNC(fbg_Purchase_GetAmount);
    LOAD_FUNC(fbg_Purchase_GetErrorCode);
    LOAD_FUNC(fbg_Purchase_GetPurchaseTime);
    LOAD_FUNC(fbg_Purchase_GetQuantity);
    LOAD_FUNC(fbg_PurchaseIAP);
    LOAD_FUNC(fbg_PurchaseIAPWithProductURL);
    LOAD_FUNC(fbg_SetPlatformLogFunc);
    LOAD_FUNC(fbgLoginScope_ToString);
    LOAD_FUNC(fbgMessageType_ToString);
    LOAD_FUNC(fbgPlatformInitializeResult_ToString);
    LOAD_FUNC(fbid_ToString);
    LOAD_FUNC(fbg_Purchase_GetCurrency);
    LOAD_FUNC(fbg_Purchase_GetErrorMessage);
    LOAD_FUNC(fbg_Purchase_GetPaymentID);
    LOAD_FUNC(fbg_Purchase_GetProductId);
    LOAD_FUNC(fbg_Purchase_GetPurchaseToken);
    LOAD_FUNC(fbg_Purchase_GetRequestId);
    LOAD_FUNC(fbg_Purchase_GetSignedRequest);
    LOAD_FUNC(fbg_Purchase_GetStatus);

#undef LOAD_FUNC

    return true;
}

const struct dmFBGameroom::FBGameroomFunctions* dmFBGameroom::GetFBFunctions()
{
    return g_FBGLibrary != 0 ? &g_FBFunctions : 0;
}

static void UnloadFBGameRoom()
{
    if (g_FBGLibrary) {
        FreeLibrary(g_FBGLibrary);
    }
    g_FBGLibrary = 0;
}

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

    if (!g_FBFunctions.fbg_IsPlatformInitialized()) {
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
        g_FBGameroom.m_Enabled = LoadFBGameRoom();

        const char* app_id = dmConfigFile::GetString(params->m_ConfigFile, "facebook.appid", 0);
        if( !app_id )
        {
            dmLogError("No facebook.appid. Disabling module");
            return dmExtension::RESULT_OK;
        }

        g_FBFunctions.fbg_SetPlatformLogFunc(GameroomLogFunction);

        fbgPlatformInitializeResult fb_init_res = g_FBFunctions.fbg_PlatformInitializeWindows(app_id);
        if (fb_init_res != fbgPlatformInitialize_Success)
        {
            dmLogError("Could not init Facebook Gameroom: %s", g_FBFunctions.fbgPlatformInitializeResult_ToString(fb_init_res));
            return dmExtension::RESULT_INIT_ERROR;
        }
    }

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeGameroom(dmExtension::AppParams* params)
{
    UnloadFBGameRoom();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateGameroom(dmExtension::Params* params)
{
    if (!g_FBGameroom.m_Enabled || !g_FBFunctions.fbg_IsPlatformInitialized())
    {
        return dmExtension::RESULT_OK;
    }

    fbgMessageHandle message;
    while ((message = g_FBFunctions.fbg_PopMessage()) != nullptr) {
        // Freeing of 'message' must be done in Facebook and IAP modules/extensions.

        fbgMessageType message_type = g_FBFunctions.fbg_Message_GetType(message);
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
                dmLogError("Got unknown message: %s", g_FBFunctions.fbgMessageType_ToString(message_type));
            break;
        }
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(GameroomExt, "Gameroom", AppInitializeGameroom, AppFinalizeGameroom, InitializeGameroom, UpdateGameroom, 0, 0)
