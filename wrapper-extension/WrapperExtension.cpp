
#include "pch.h"
#include "WrapperExtension.h"

//////////////////////////////////////////////////////
// Boilerplate stuff
WrapperExtension* g_Extension = nullptr;

// Main DLL export function to initialize extension.
extern "C" {
	__declspec(dllexport) IExtension* WrapperExtInit(IApplication* iApplication)
	{
		g_Extension = new WrapperExtension(iApplication);
		return g_Extension;
	}
}

// Helper method to call HandleWebMessage() with more useful types, as OnWebMessage() must deal with
// plain-old-data types for crossing a DLL boundary.
void WrapperExtension::OnWebMessage(LPCSTR messageId_, size_t paramCount, const ExtensionParameterPOD* paramArr, double asyncId)
{
	HandleWebMessage(messageId_, UnpackExtensionParameterArray(paramCount, paramArr), asyncId);
}

void WrapperExtension::SendWebMessage(const std::string& messageId, const std::map<std::string, ExtensionParameter>& params, double asyncId)
{
	std::vector<NamedExtensionParameterPOD> paramArr = PackNamedExtensionParameters(params);
	iApplication->SendWebMessage(messageId.c_str(), paramArr.size(), paramArr.empty() ? nullptr : paramArr.data(), asyncId);
}

// Helper method for sending a response to an async message (when asyncId is not -1.0).
// In this case the message ID is not used, so this just calls SendWebMessage() with an empty message ID.
void WrapperExtension::SendAsyncResponse(const std::map<std::string, ExtensionParameter>& params, double asyncId)
{
	SendWebMessage("", params, asyncId);
}

//////////////////////////////////////////////////////
// WrapperExtension
WrapperExtension::WrapperExtension(IApplication* iApplication_)
	: iApplication(iApplication_),
	  hWndMain(NULL),
	  didEpicGamesInitOk(false),
	  hPlatform(nullptr),
	  hAuth(nullptr),
	  hConnect(nullptr),
	  hUserInfo(nullptr),
	  hAchievements(nullptr),
	  epicAccountId(nullptr),
	  productUserId(nullptr)
{
	OutputDebugString(L"[EpicExt] Loaded extension\n");

	// Register the "scirra-epic-games" component for JavaScript messaging
	iApplication->RegisterComponentId("scirra-epic-games");
}

void WrapperExtension::Release()
{
	OutputDebugString(L"[EpicExt] Releasing extension\n");

	if (didEpicGamesInitOk)
	{
		if (hPlatform != nullptr)
		{
			EOS_Platform_Release(hPlatform);
			hPlatform = nullptr;
		}

		EOS_EResult shutdownResult = EOS_Shutdown();
		if (shutdownResult != EOS_EResult::EOS_Success)
		{
			OutputDebugString(L"[EpicExt] Warning: EOS_Shutdown() did not complete successfully\n");
		}
	}
}

void WrapperExtension::OnMainWindowCreated(HWND hWnd)
{
	hWndMain = hWnd;
}

// For handling a message sent from JavaScript.
// This method mostly just unpacks parameters and calls a dedicated method to handle the message.
void WrapperExtension::HandleWebMessage(const std::string& messageId, const std::vector<ExtensionParameter>& params, double asyncId)
{
	if (messageId == "init")
	{
		// Get product name and version
		const std::string& productName = params[0].GetString();
		const std::string& productVersion = params[1].GetString();

		// Save the SDK settings
		productId = params[2].GetString();
		clientId = params[3].GetString();
		clientSecret = params[4].GetString();
		sandboxId = params[5].GetString();
		deploymentId = params[6].GetString();

		OnInitMessage(productName, productVersion, asyncId);
	}
	else if (messageId == "platform-tick")
	{
		if (hPlatform != nullptr)
			EOS_Platform_Tick(hPlatform);
	}
	else if (messageId == "log-in-portal")
	{
		bool basicProfile = params[0].GetBool();
		bool friendsList = params[1].GetBool();
		bool presence = params[2].GetBool();
		bool country = params[3].GetBool();

		OnLogInPortalMessage(basicProfile, friendsList, presence, country, asyncId);
	}
	else if (messageId == "log-in-persistent")
	{
		bool basicProfile = params[0].GetBool();
		bool friendsList = params[1].GetBool();
		bool presence = params[2].GetBool();
		bool country = params[3].GetBool();

		OnLogInPersistentMessage(basicProfile, friendsList, presence, country, asyncId);
	}
	else if (messageId == "log-in-exchange-code")
	{
		bool basicProfile = params[0].GetBool();
		bool friendsList = params[1].GetBool();
		bool presence = params[2].GetBool();
		bool country = params[3].GetBool();
		const std::string& exchangeCode = params[4].GetString();

		OnLogInExchangeCodeMessage(basicProfile, friendsList, presence, country, exchangeCode, asyncId);
	}
	else if (messageId == "log-in-devauthtool")
	{
		bool basicProfile = params[0].GetBool();
		bool friendsList = params[1].GetBool();
		bool presence = params[2].GetBool();
		bool country = params[3].GetBool();
		const std::string& host = params[4].GetString();
		const std::string& credentialName = params[5].GetString();

		OnLogInDevAuthToolMessage(basicProfile, friendsList, presence, country, host, credentialName, asyncId);
	}
	else if (messageId == "log-out")
	{
		OnLogOutMessage(asyncId);
	}
	else if (messageId == "unlock-achievement")
	{
		const std::string& achievementId = params[0].str;

		OnUnlockAchievementMessage(achievementId, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization

void EOS_CALL OnEOSLogMessage(const EOS_LogMessage* Message)
{
	OutputDebugString(L"[EOS][");
	OutputDebugString(Utf8ToWide(Message->Category).c_str());
	OutputDebugString(L"/");
	OutputDebugString(std::to_wstring(static_cast<int>(Message->Level)).c_str());
	OutputDebugString(L"] ");
	OutputDebugString(Utf8ToWide(Message->Message).c_str());
	OutputDebugString(L"\n");
}

// Callback for EOS_Auth_AddNotifyLoginStatusChanged() that forwards to WrapperExtension::OnLogInStatusChanged()
void EOS_CALL LoginStatusChangedCallbackFn(const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
{
	WrapperExtension* extension = static_cast<WrapperExtension*>(Data->ClientData);
	extension->OnLogInStatusChanged(Data);
}

// Callback for EOS_Connect_AddNotifyAuthExpiration() that forwards to WrapperExtension::OnConnectAuthExpiration()
void EOS_CALL ConnectAuthExpirationCallbackFn(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
	WrapperExtension* extension = static_cast<WrapperExtension*>(Data->ClientData);
	extension->OnConnectAuthExpiration(Data);
}

void WrapperExtension::OnInitMessage(const std::string& productName, const std::string& productVersion, double asyncId)
{
	// Dump the full command line to the debug log for diagnostic purposes,
	// as command line parsing is used to activate some features
	std::wstring commandLine = GetCommandLine();
	OutputDebugString(L"[EpicExt] Command line:\n");
	OutputDebugString(commandLine.c_str());
	OutputDebugString(L"\n");

	// Detect the Epic launcher and the provided exchange code from the command line
	bool isEpicLauncher = false;
	std::string launcherExchangeCode;

	// Use CommandLineToArgvW() to parse the command line for us
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(commandLine.c_str(), &argc);
	if (argv != nullptr)
	{
		// For each command line option
		for (int i = 0; i < argc; ++i)
		{
			std::wstring arg = argv[i];

			// Check for the -EpicPortal argument indicating started from the Epic Games launcher
			if (arg == L"-EpicPortal")
			{
				isEpicLauncher = true;
			}
			// Check for an argument prefixed -AUTH_PASSWORD= and use the provided value as the
			// launcher exchange code. This can be used for an automatic login.
			else if (arg.substr(0, 15) == L"-AUTH_PASSWORD=")
			{
				launcherExchangeCode = WideToUtf8(arg.substr(15));
			}
		}
	}

	EOS_InitializeOptions initOpts = {};
	initOpts.ApiVersion = EOS_INITIALIZE_API_LATEST;
	initOpts.ProductName = productName.c_str();
	initOpts.ProductVersion = productVersion.c_str();

	EOS_EResult initResult = EOS_Initialize(&initOpts);
	didEpicGamesInitOk = (initResult == EOS_EResult::EOS_Success);

	if (didEpicGamesInitOk)
	{
		OutputDebugString(L"[EpicExt] Successfully initialized Epic Games SDK\n");

		// Initialize logging
		EOS_Logging_SetCallback(OnEOSLogMessage);

#ifdef _DEBUG
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Verbose);
#else
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Warning);
#endif

		// Initialize platform
		EOS_Platform_Options platOpts = {};
		std::string appDataFolder = iApplication->GetCurrentAppDataFolder();
		std::string cacheDir = appDataFolder + "\\EOSCache\\";

		platOpts.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
		platOpts.ProductId = productId.c_str();
		platOpts.SandboxId = sandboxId.c_str();
		platOpts.ClientCredentials.ClientId = clientId.c_str();
		platOpts.ClientCredentials.ClientSecret = clientSecret.c_str();
		platOpts.DeploymentId = deploymentId.c_str();
		platOpts.bIsServer = EOS_FALSE;
		platOpts.CacheDirectory = cacheDir.c_str();

		// HACK: for now disable the EOS overlay, as it doesn't seem to work with
		// WebView2 or even the D3D11Overlay workaround.
		platOpts.Flags = EOS_PF_DISABLE_OVERLAY;

		// Note encryption key is not used so it is just set to a dummy value
		platOpts.EncryptionKey = "1111111111111111111111111111111111111111111111111111111111111111";

		hPlatform = EOS_Platform_Create(&platOpts);

		// Get other interfaces
		hAuth = EOS_Platform_GetAuthInterface(hPlatform);
		hConnect = EOS_Platform_GetConnectInterface(hPlatform);
		hUserInfo = EOS_Platform_GetUserInfoInterface(hPlatform);
		hAchievements = EOS_Platform_GetAchievementsInterface(hPlatform);

		// Add callbacks
		EOS_Auth_AddNotifyLoginStatusChangedOptions nlscOpts = {};
		nlscOpts.ApiVersion = EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
		EOS_Auth_AddNotifyLoginStatusChanged(hAuth, &nlscOpts, this, LoginStatusChangedCallbackFn);

		EOS_Connect_AddNotifyAuthExpirationOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
		EOS_Connect_AddNotifyAuthExpiration(hConnect, &Options, NULL, ConnectAuthExpirationCallbackFn);

		// Send init data back to JavaScript with key details from the API.
		SendAsyncResponse({
			{ "isAvailable", true },
			{ "isEpicLauncher", isEpicLauncher },
			{ "launcherExchangeCode", launcherExchangeCode }
		}, asyncId);
	}
	else
	{
		OutputDebugString(L"[EpicExt] Failed to initialize Epic Games SDK\n");

		SendAsyncResponse({
			{ "isAvailable", false }
		}, asyncId);
	}
}

void WrapperExtension::OnLogInStatusChanged(const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
{
	// Send message to JavaScript to fire trigger.
	SendWebMessage("on-login-status-changed", {
		{ "loginStatus", static_cast<double>(Data->CurrentStatus) }
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log in (portal)

// Helper for determining EOS_EAuthScopeFlags
EOS_EAuthScopeFlags GetAuthScopeFlags(bool basicProfile, bool friendsList, bool presence, bool country)
{
	EOS_EAuthScopeFlags ret = EOS_EAuthScopeFlags::EOS_AS_NoFlags;

	if (basicProfile)
		ret |= EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
	if (friendsList)
		ret |= EOS_EAuthScopeFlags::EOS_AS_FriendsList;
	if (presence)
		ret |= EOS_EAuthScopeFlags::EOS_AS_Presence;
	if (country)
		ret |= EOS_EAuthScopeFlags::EOS_AS_Country;

	return ret;
}

// Callback for EOS_Auth_Login() that forwards to WrapperExtension::OnLogInPortalCallback()
void EOS_CALL LoginPortalCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnLogInPortalCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;
}

void WrapperExtension::OnLogInPortalMessage(bool basicProfile, bool friendsList, bool presence, bool country, double asyncId)
{
	OutputDebugString(L"[EpicExt] Starting log in via portal\n");

	EOS_Auth_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;

	EOS_Auth_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	LoginOptions.ScopeFlags = GetAuthScopeFlags(basicProfile, friendsList, presence, country);
	LoginOptions.Credentials = &Credentials;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Login(hAuth, &LoginOptions, callbackInfo, LoginPortalCompleteCallbackFn);
}

void WrapperExtension::OnLogInPortalCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnLogInCallback: success\n");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		OutputDebugString(L"[EpicExt] OnLogInCallback: failed\n");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

void WrapperExtension::HandleSuccessfulLogIn(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	epicAccountId = Data->LocalUserId;

	// Convert epic account ID to string
	std::string epicAccountIdStr(EOS_EPICACCOUNTID_MAX_LENGTH + 1, 0);
	int32_t outSize = static_cast<int32_t>(epicAccountIdStr.size());
	if (EOS_EpicAccountId_ToString(Data->LocalUserId, &epicAccountIdStr[0], &outSize) != EOS_EResult::EOS_Success)
	{
		epicAccountIdStr = "";
	}

	// Call EOS_UserInfo_CopyUserInfo to get a EOS_UserInfo struct with details about the user
	EOS_UserInfo_CopyUserInfoOptions copyOpts = {};
	copyOpts.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
	copyOpts.LocalUserId = Data->LocalUserId;
	copyOpts.TargetUserId = Data->LocalUserId;

	EOS_UserInfo* userInfo = nullptr;
	if (EOS_UserInfo_CopyUserInfo(hUserInfo, &copyOpts, &userInfo) == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] EOS_UserInfo_CopyUserInfo succeeded\n");

		// Copy user info to std::strings. Use a utility method that returns an empty
		// string if passed nullptr since unavailable fields are set to nullptr.
		userDisplayName = StrFromPtr(userInfo->DisplayName);
		userDisplayNameSanitized = StrFromPtr(userInfo->DisplayNameSanitized);
		userNickname = StrFromPtr(userInfo->Nickname);
		userPreferredLanguage = StrFromPtr(userInfo->PreferredLanguage);
		userCountry = StrFromPtr(userInfo->Country);

		EOS_UserInfo_Release(userInfo);
		userInfo = nullptr;
	}
	else
	{
		OutputDebugString(L"[EpicExt] EOS_UserInfo_CopyUserInfo failed\n");
	}

	// Use the Connect service to automatically attempt to obtain a Product User ID (PUID) for the
	// user who just logged in associated with Epic services, in order to use achievements.
	// Note that for the convenience of the Construct addon, the login process does not wait for this
	// to complete; instead it's just assumed this will be ready by the time any achievements are
	// attempted to be activated. If no PUID could be obtained, then unlocking an achievement will fail.
	ConnectLogin();

	// Send async response to resolve JavaScript promise
	SendAsyncResponse({
		{ "isOk", true },
		{ "epicAccountIdStr", epicAccountIdStr },
		{ "displayName", userDisplayName },
		{ "displayNameSanitized", userDisplayNameSanitized },
		{ "nickname", userNickname },
		{ "preferredLanguage", userPreferredLanguage },
		{ "country", userCountry }
	}, asyncId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log in (persistent)

// Callback for EOS_Auth_Login() that forwards to WrapperExtension::OnLogInPersistentCallback()
void EOS_CALL LoginPersistentCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnLogInPersistentCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;
}

// Callback for EOS_Auth_DeletePersistentAuth(). This just logs the success/failure result for debugging.
void EOS_CALL DeletePersistentAuthCallbackFn(const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] DeletePersistentAuthCallbackFn: success\n");
	}
	else
	{
		OutputDebugString(L"[EpicExt] DeletePersistentAuthCallbackFn: failed\n");
	}
}

void WrapperExtension::OnLogInPersistentMessage(bool basicProfile, bool friendsList, bool presence, bool country, double asyncId)
{
	OutputDebugString(L"[EpicExt] Starting log in via persistent auth\n");

	EOS_Auth_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

	EOS_Auth_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	LoginOptions.ScopeFlags = GetAuthScopeFlags(basicProfile, friendsList, presence, country);
	LoginOptions.Credentials = &Credentials;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Login(hAuth, &LoginOptions, callbackInfo, LoginPersistentCompleteCallbackFn);
}

void WrapperExtension::OnLogInPersistentCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnLogInPersistentCallback: success\n");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		OutputDebugString(L"[EpicExt] OnLogInPersistentCallback: failed\n");

		// Persistent login failed, so delete the persistent auth so it does not use the same
		// persisted auth again next time.
		EOS_Auth_DeletePersistentAuthOptions delOpts = {};
		delOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
		EOS_Auth_DeletePersistentAuth(hAuth, &delOpts, nullptr, DeletePersistentAuthCallbackFn);

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log in (exchange code)

// Callback for EOS_Auth_Login() that forwards to WrapperExtension::OnLogInExchangeCodeCallback()
void EOS_CALL LoginExchangeCodeCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnLogInExchangeCodeCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;
}

void WrapperExtension::OnLogInExchangeCodeMessage(bool basicProfile, bool friendsList, bool presence, bool country, const std::string& exchangeCode, double asyncId)
{
	OutputDebugString(L"[EpicExt] Starting log in via exchange code\n");

	EOS_Auth_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
	Credentials.Token = exchangeCode.c_str();

	EOS_Auth_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	LoginOptions.ScopeFlags = GetAuthScopeFlags(basicProfile, friendsList, presence, country);
	LoginOptions.Credentials = &Credentials;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Login(hAuth, &LoginOptions, callbackInfo, LoginExchangeCodeCompleteCallbackFn);
}

void WrapperExtension::OnLogInExchangeCodeCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnLogInExchangeCodeCallback: success\n");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		OutputDebugString(L"[EpicExt] OnLogInExchangeCodeCallback: failed\n");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log in (DevAuthTool)

// Callback for EOS_Auth_Login() that forwards to WrapperExtension::OnLogInDevAuthToolCallback()
void EOS_CALL LoginDevAuthToolCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnLogInDevAuthToolCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;
}

void WrapperExtension::OnLogInDevAuthToolMessage(bool basicProfile, bool friendsList, bool presence, bool country, const std::string& host, const std::string& credentialName, double asyncId)
{
	OutputDebugString(L"[EpicExt] Starting log in via DevAuthTool\n");

	EOS_Auth_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
	Credentials.Id = host.c_str();
	Credentials.Token = credentialName.c_str();

	EOS_Auth_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	LoginOptions.ScopeFlags = GetAuthScopeFlags(basicProfile, friendsList, presence, country);
	LoginOptions.Credentials = &Credentials;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Login(hAuth, &LoginOptions, callbackInfo, LoginDevAuthToolCompleteCallbackFn);
}

void WrapperExtension::OnLogInDevAuthToolCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnLogInDevAuthToolCallback: success\n");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		OutputDebugString(L"[EpicExt] OnLogInDevAuthToolCallback: failed\n");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log out

// Callback for EOS_Auth_Logout() that forwards to WrapperExtension::OnLogOutCallback()
void EOS_CALL LogoutCompleteCallbackFn(const EOS_Auth_LogoutCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnLogOutCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;
}

void WrapperExtension::OnLogOutMessage(double asyncId)
{
	OutputDebugString(L"[EpicExt] Starting log out\n");

	EOS_Auth_LogoutOptions LogoutOptions = {};
	LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
	LogoutOptions.LocalUserId = epicAccountId;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Logout(hAuth, &LogoutOptions, callbackInfo, LogoutCompleteCallbackFn);
}

void WrapperExtension::OnLogOutCallback(const EOS_Auth_LogoutCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnLogOutCallback: success\n");

		// Clear details set when logged in
		epicAccountId = nullptr;
		productUserId = nullptr;
		userDisplayName = "";
		userDisplayNameSanitized = "";
		userNickname = "";
		userPreferredLanguage = "";
		userCountry = "";

		// User logged out OK, so also delete any persisted auth to prevent any future
		// automatic login.
		EOS_Auth_DeletePersistentAuthOptions delOpts = {};
		delOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
		EOS_Auth_DeletePersistentAuth(hAuth, &delOpts, nullptr, DeletePersistentAuthCallbackFn);

		SendAsyncResponse({
			{ "isOk", true }
		}, asyncId);
	}
	else
	{
		OutputDebugString(L"[EpicExt] OnLogOutCallback: failed\n");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Achievements
// Note this assumes ConnectLogin() completed successfully and set a valid productUserId.

// Callback for EOS_Achievements_UnlockAchievements() that forwards to WrapperExtension::OnUnlockAchievementCallback()
void EOS_CALL UnlockAchievementCompleteCallbackFn(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data)
{
	ExtCallbackInfo* callbackInfo = static_cast<ExtCallbackInfo*>(Data->ClientData);
	callbackInfo->extension->OnUnlockAchievementCallback(Data, callbackInfo->asyncId);
	delete callbackInfo;

}
void WrapperExtension::OnUnlockAchievementMessage(const std::string& achievementId, double asyncId)
{
	OutputDebugString(L"[EpicExt] Unlocking achievement\n");

	const char* achievementIdPtr = achievementId.c_str();

	EOS_Achievements_UnlockAchievementsOptions achievementOpts = {};
	achievementOpts.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
	achievementOpts.UserId = productUserId;					// from ConnectLogin()
	achievementOpts.AchievementsCount = 1;
	achievementOpts.AchievementIds = &achievementIdPtr;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Achievements_UnlockAchievements(hAchievements, &achievementOpts, callbackInfo, UnlockAchievementCompleteCallbackFn);
}

void WrapperExtension::OnUnlockAchievementCallback(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnUnlockAchievementCallback: success\n");

		SendAsyncResponse({
			{ "isOk", true }
		}, asyncId);
	}
	else
	{
		OutputDebugString(L"[EpicExt] OnUnlockAchievementCallback: failed\n");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Connect login (to establish Product User ID for achievements)

// Callback for EOS_Connect_Login() that forwards to WrapperExtension::OnConnectLoginCallback()
void EOS_CALL ConnectLoginCompleteCallbackFn(const EOS_Connect_LoginCallbackInfo* Data)
{
	static_cast<WrapperExtension*>(Data->ClientData)->OnConnectLoginCallback(Data);
}

void WrapperExtension::ConnectLogin()
{
	OutputDebugString(L"[EpicExt] ConnectLogin()\n");

	// Note the SDK samples use EOS_Auth_CopyUserAuthToken() with EOS_EExternalCredentialType::EOS_ECT_EPIC,
	// but the documentation states EOS_Auth_CopyIdToken() with EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN
	// is preferred. See: https://dev.epicgames.com/docs/api-ref/enums/eos-e-external-credential-type
	EOS_Auth_IdToken* authIdToken = nullptr;

	EOS_Auth_CopyIdTokenOptions copyIdTokenOpts = {};
	copyIdTokenOpts.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
	copyIdTokenOpts.AccountId = epicAccountId;

	if (EOS_Auth_CopyIdToken(hAuth, &copyIdTokenOpts, &authIdToken) == EOS_EResult::EOS_Success)
	{
		EOS_Connect_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
		Credentials.Token = authIdToken->JsonWebToken;
		Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN;

		EOS_Connect_LoginOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
		Options.Credentials = &Credentials;
		Options.UserLoginInfo = nullptr;

		EOS_Connect_Login(hConnect, &Options, this, ConnectLoginCompleteCallbackFn);
		EOS_Auth_IdToken_Release(authIdToken);
	}
}

void WrapperExtension::OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		OutputDebugString(L"[EpicExt] OnConnectLoginCallback: success\n");

		// Save the product user ID for use with achievements
		productUserId = Data->LocalUserId;
	}
	else
	{
		OutputDebugString(L"[EpicExt] OnConnectLoginCallback: failed\n");
	}
}

void WrapperExtension::OnConnectAuthExpiration(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
	OutputDebugString(L"[EpicExt] OnConnectAuthExpiration()\n");

	// If still logged in, start ConnectLogin() again to refresh the product user ID
	if (epicAccountId != nullptr)
	{
		ConnectLogin();
	}
}

