#ifndef GAMEROOM_PRIVATE_H
#define GAMEROOM_PRIVATE_H

#include <FBG_Platform.h>
#include <dlib/array.h>

namespace dmFBGameroom
{
    bool CheckGameroomInit();
    dmArray<fbgMessageHandle>* GetFacebookMessages();
    dmArray<fbgMessageHandle>* GetIAPMessages();

#define FBG_DEFINE_FUNC(_RETURNTYPE, _NAME, ...) \
    typedef _RETURNTYPE (* _NAME ## _Function )( ... );

	FBG_DEFINE_FUNC(fbgMessageType, fbg_Message_GetType, const fbgMessageHandle);
	FBG_DEFINE_FUNC(const char*, fbgMessageType_ToString, fbgMessageType);
	FBG_DEFINE_FUNC(fbgMessageHandle, fbg_PopMessage);
	FBG_DEFINE_FUNC(void, fbg_SetPlatformLogFunc, LogFunctionPtr);
	FBG_DEFINE_FUNC(bool, fbg_IsPlatformInitialized);
	FBG_DEFINE_FUNC(fbgPlatformInitializeResult, fbg_PlatformInitializeWindows, const char*, int, int);
	FBG_DEFINE_FUNC(const char *, fbgPlatformInitializeResult_ToString, fbgPlatformInitializeResult);

	FBG_DEFINE_FUNC(fbgRequest, fbg_PurchaseIAP,    const char* product,
	                                              uint32_t quantity,
	                                              uint32_t quantityMin,
	                                              uint32_t quantityMax,
	                                              const char* requestId,
	                                              const char* pricePointId,
	                                              const char* testCurrency);

	FBG_DEFINE_FUNC(fbgRequest, fbg_PurchaseIAPWithProductURL, const char* product,
	                                                          uint32_t quantity,
	                                                          uint32_t quantityMin,
	                                                          uint32_t quantityMax,
	                                                          const char* requestId,
	                                                          const char* pricePointId,
	                                                          const char* testCurrency);

	FBG_DEFINE_FUNC(fbgRequest, fbg_PayPremium, void);
	FBG_DEFINE_FUNC(fbgRequest, fbg_HasLicense, void);

	FBG_DEFINE_FUNC(void, fbg_FreeMessage, fbgMessageHandle);

	FBG_DEFINE_FUNC(fbgPurchaseHandle, fbg_Message_Purchase, const fbgMessageHandle);

	FBG_DEFINE_FUNC(uint32_t, fbg_Purchase_GetAmount, const fbgPurchaseHandle);
	FBG_DEFINE_FUNC(uint64_t, fbg_Purchase_GetPurchaseTime, const fbgPurchaseHandle);
	FBG_DEFINE_FUNC(uint32_t, fbg_Purchase_GetQuantity, const fbgPurchaseHandle);
	FBG_DEFINE_FUNC(uint64_t, fbg_Purchase_GetErrorCode, const fbgPurchaseHandle);

	FBG_DEFINE_FUNC(fbgHasLicenseHandle, fbg_Message_HasLicense, const fbgMessageHandle);
	FBG_DEFINE_FUNC(fbid, fbg_HasLicense_GetHasLicense, const fbgHasLicenseHandle);

	FBG_DEFINE_FUNC(fbgRequest, fbg_Login);
	FBG_DEFINE_FUNC(fbgRequest, fbg_Login_WithScopes, uint32_t scopeCount,fbgLoginScope* loginScopes);
	FBG_DEFINE_FUNC(size_t, fbg_AccessToken_GetTokenString, const fbgAccessTokenHandle obj, char* buffer, size_t bufferIn);
	FBG_DEFINE_FUNC(fbgAccessTokenHandle, fbg_AccessToken_GetActiveAccessToken);
	FBG_DEFINE_FUNC(bool, fbg_AccessToken_IsValid, const fbgAccessTokenHandle obj);
	FBG_DEFINE_FUNC(size_t, fbg_AccessToken_GetPermissions, const fbgAccessTokenHandle obj, fbgLoginScope* buffer, size_t bufferIn);
	FBG_DEFINE_FUNC(fbid, fbg_FeedShare_GetPostID, const fbgFeedShareHandle obj);
	FBG_DEFINE_FUNC(fbgRequest, fbg_FeedShare, const char* toId,const char* link,const char* linkName,const char* linkCaption,const char* linkDescription,const char* pictureLink,const char* mediaSource);
	FBG_DEFINE_FUNC(fbgRequest, fbg_AppRequest, const char* message,const char* actionType,const char* objectId,const char* to,const char* filters,const char* excludeIDs,uint32_t maxRecipients,const char* data,const char* title);
	FBG_DEFINE_FUNC(fbgRequest, fbg_LogAppEventWithValueToSum, const char* eventName,const fbgFormDataHandle formData,float valueToSum);
	FBG_DEFINE_FUNC(fbgAccessTokenHandle, fbg_Message_AccessToken, const fbgMessageHandle obj);
	FBG_DEFINE_FUNC(fbgFeedShareHandle, fbg_Message_FeedShare, const fbgMessageHandle obj);
	FBG_DEFINE_FUNC(bool, fbid_ToString, char *outParam, size_t bufferLength, fbid id);
	FBG_DEFINE_FUNC(fbgAppRequestHandle, fbg_Message_AppRequest, const fbgMessageHandle obj);
	FBG_DEFINE_FUNC(size_t, fbg_AppRequest_GetRequestObjectId, const fbgAppRequestHandle obj,char *buffer,size_t bufferIn);
	FBG_DEFINE_FUNC(size_t, fbg_AppRequest_GetTo, const fbgAppRequestHandle obj,char *buffer,size_t bufferIn);

	struct FBGameroomFunctions
	{
		fbg_Message_GetType_Function 					fbg_Message_GetType;
		fbgMessageType_ToString_Function 				fbgMessageType_ToString;
		fbg_PopMessage_Function 						fbg_PopMessage;
		fbg_SetPlatformLogFunc_Function 				fbg_SetPlatformLogFunc;
		fbg_IsPlatformInitialized_Function 				fbg_IsPlatformInitialized;
		fbg_PlatformInitializeWindows_Function 			fbg_PlatformInitializeWindows;
		fbgPlatformInitializeResult_ToString_Function 	fbgPlatformInitializeResult_ToString;
		fbg_PurchaseIAP_Function 						fbg_PurchaseIAP;
		fbg_PurchaseIAPWithProductURL_Function 			fbg_PurchaseIAPWithProductURL;
		fbg_PayPremium_Function 						fbg_PayPremium;
		fbg_HasLicense_Function 						fbg_HasLicense;
		fbg_FreeMessage_Function 						fbg_FreeMessage;
		fbg_Message_Purchase_Function 					fbg_Message_Purchase;
		fbg_Purchase_GetAmount_Function 				fbg_Purchase_GetAmount;
		fbg_Purchase_GetPurchaseTime_Function 			fbg_Purchase_GetPurchaseTime;
		fbg_Purchase_GetQuantity_Function 				fbg_Purchase_GetQuantity;
		fbg_Purchase_GetErrorCode_Function 				fbg_Purchase_GetErrorCode;
		fbg_Message_HasLicense_Function 				fbg_Message_HasLicense;
		fbg_HasLicense_GetHasLicense_Function 			fbg_HasLicense_GetHasLicense;
		fbg_Login_Function 								fbg_Login;
		fbg_Login_WithScopes_Function 					fbg_Login_WithScopes;
		fbg_AccessToken_GetTokenString_Function 		fbg_AccessToken_GetTokenString;
		fbg_AccessToken_GetActiveAccessToken_Function 	fbg_AccessToken_GetActiveAccessToken;
		fbg_AccessToken_IsValid_Function 				fbg_AccessToken_IsValid;
		fbg_AccessToken_GetPermissions_Function 		fbg_AccessToken_GetPermissions;
		fbg_FeedShare_GetPostID_Function 				fbg_FeedShare_GetPostID;
		fbg_FeedShare_Function 							fbg_FeedShare;
		fbg_AppRequest_Function 						fbg_AppRequest;
		fbg_LogAppEventWithValueToSum_Function 			fbg_LogAppEventWithValueToSum;
		fbg_Message_AccessToken_Function 				fbg_Message_AccessToken;
		fbg_Message_FeedShare_Function 					fbg_Message_FeedShare;
		fbid_ToString_Function 							fbid_ToString;
		fbg_Message_AppRequest_Function 				fbg_Message_AppRequest;
		fbg_AppRequest_GetRequestObjectId_Function 		fbg_AppRequest_GetRequestObjectId;
		fbg_AppRequest_GetTo_Function 					fbg_AppRequest_GetTo;
	};

    const struct FBGameroomFunctions* GetFBFunctions();

#undef FBG_DEFINE_FUNC

}

#endif // GAMEROOM_PRIVATE_H
