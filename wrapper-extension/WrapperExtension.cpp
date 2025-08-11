
#include "pch.h"
#include "WrapperExtension.h"

#include "json.hpp"

const char* COMPONENT_ID = "scirra-epic-games";

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
	  isEpicLauncher(false),
	  sharedHandles{},
	  hAuth(nullptr),
	  hConnect(nullptr),
	  hUserInfo(nullptr),
	  hAchievements(nullptr)
{
	LogMessage("Loaded extension");

	// Tell the host application the SDK version used. Don't change this.
	iApplication->SetSdkVersion(WRAPPER_EXT_SDK_VERSION);

	// Register the "scirra-epic-games" component for JavaScript messaging
	iApplication->RegisterComponentId(COMPONENT_ID);

	// Set a shared pointer to the EOS_Shared_Handles struct, so companion plugins
	// can access the handles created by this extension.
	iApplication->SetSharedPtr("scirra-epic-games-handles", &sharedHandles);
}

void WrapperExtension::Init()
{
	// Called during startup after all other extensions have been loaded.
	// Parse the content of package.json and read the exported properties like the product ID and client ID.
	// These are exported with the SetWrapperExportProperties() method and end up in package.json like this:
	//{
	//	...
	//	"exported-properties": {
	//		"scirra-epic-games": {
	//			"product-id": "...",
	//			"client-id": "...",
	//			...
	//		}
	//	}
	//}

	std::string productName;
	std::string productVersion;

	try {
		auto packageJson = nlohmann::json::parse(iApplication->GetPackageJsonContent());

		// Get the project name and version from the "project-details" section of package.json, as these
		// are used as fallbacks for the product name and version if those aren't specified.
		const auto& projectDetails = packageJson["project-details"];
		std::string projectName =		projectDetails["name"].get<std::string>();
		std::string projectVersion =	projectDetails["version"].get<std::string>();
		TrimString(projectName);
		TrimString(projectVersion);

		// Read the exported properties from the Epic Games plugin.
		const auto& epicProps = packageJson["exported-properties"][COMPONENT_ID];
		productName =		epicProps["product-name"].get<std::string>();
		productVersion =	epicProps["product-version"].get<std::string>();
		productId =			epicProps["product-id"].get<std::string>();
		clientId =			epicProps["client-id"].get<std::string>();
		clientSecret =		epicProps["client-secret"].get<std::string>();
		sandboxId =			epicProps["sandbox-id"].get<std::string>();
		deploymentId =		epicProps["deployment-id"].get<std::string>();

		// Trim whitespace from all the above strings.
		TrimString(productName);
		TrimString(productVersion);
		TrimString(productId);
		TrimString(clientId);
		TrimString(clientSecret);
		TrimString(sandboxId);
		TrimString(deploymentId);

		// If the product name or version are omitted, use the project name or version.
		if (productName.empty())
			productName = projectName;
		if (productVersion.empty())
			productVersion = projectVersion;

		std::stringstream ss;
		ss << "Parsed package JSON (product name '" << productName << "', product version '" << productVersion << "', product id '" << productId << "', client id '"
			<< clientId << "', client secret '" << clientSecret << "', sandbox id '" << sandboxId << "', deployment id '" << deploymentId << "'";
		LogMessage(ss.str());
	}
	catch (...)
	{
		LogMessage("Failed to read properties package JSON");
		return;
	}

	InitEpicGamesSDK(productName, productVersion);
}

void WrapperExtension::InitEpicGamesSDK(const std::string& productName, const std::string& productVersion)
{
	// Dump the full command line to the debug log for diagnostic purposes,
	// as command line parsing is used to activate some features
	std::wstring commandLine = GetCommandLine();
	LogMessage(std::string("Command line: ") + WideToUtf8(GetCommandLine()));

	// Detect the Epic launcher and the provided exchange code from the command line.
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
		LogMessage("Successfully initialized Epic Games SDK");

		// Initialize logging
		EOS_Logging_SetCallback([](const EOS_LogMessage* Message) {
			g_Extension->OnEOSLogMessage(Message);
		});

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

		// Note encryption key is not used so it is just set to a dummy value
		platOpts.EncryptionKey = "1111111111111111111111111111111111111111111111111111111111111111";

		sharedHandles.hPlatform = EOS_Platform_Create(&platOpts);

		// Get other interfaces
		hAuth = EOS_Platform_GetAuthInterface(sharedHandles.hPlatform);
		hConnect = EOS_Platform_GetConnectInterface(sharedHandles.hPlatform);
		hUserInfo = EOS_Platform_GetUserInfoInterface(sharedHandles.hPlatform);
		hAchievements = EOS_Platform_GetAchievementsInterface(sharedHandles.hPlatform);

		// Add callbacks
		EOS_Auth_AddNotifyLoginStatusChangedOptions nlscOpts = {};
		nlscOpts.ApiVersion = EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
		EOS_Auth_AddNotifyLoginStatusChanged(hAuth, &nlscOpts, nullptr, [](const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
		{
			g_Extension->OnLogInStatusChanged(Data);
		});

		EOS_Connect_AddNotifyAuthExpirationOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
		EOS_Connect_AddNotifyAuthExpiration(hConnect, &Options, nullptr, [](const EOS_Connect_AuthExpirationCallbackInfo* Data)
		{
			g_Extension->OnConnectAuthExpiration(Data);
		});
	}
	else
	{
		LogMessage("Failed to initialize Epic Games SDK");
	}
}

void WrapperExtension::Release()
{
	LogMessage("Releasing extension");

	if (didEpicGamesInitOk)
	{
		if (sharedHandles.hPlatform != nullptr)
		{
			EOS_Platform_Release(sharedHandles.hPlatform);
			sharedHandles.hPlatform = nullptr;
		}

		EOS_EResult shutdownResult = EOS_Shutdown();
		if (shutdownResult != EOS_EResult::EOS_Success)
		{
			LogMessage("Warning: EOS_Shutdown() did not complete successfully");
		}
	}
}

void WrapperExtension::LogMessage(const std::string& msg)
{
	// Log messages both to the browser console with the LogToConsole() method, and also to the debug output
	// with the DebugLog() helper function, to ensure whichever log we're looking at includes the log messages.
	std::stringstream ss;
	ss << "[EpicExt] " << msg;
	iApplication->LogToConsole(IApplication::LogLevel::normal, ss.str().c_str());

	// Add trailing newline for debug output
	ss << "\n";
	DebugLog(ss.str().c_str());
}

void WrapperExtension::OnEOSLogMessage(const EOS_LogMessage* Message)
{
	// As above but outputting EOS logs directly, tagged [EOSLog].
	std::stringstream ss;
	ss << "[EOS][" << Message->Category << "/" << std::to_string(static_cast<int>(Message->Level)) << "] " << Message->Message;

	// Copy browser log level from the EOS log level.
	IApplication::LogLevel logLevel = IApplication::LogLevel::normal;
	if (Message->Level <= EOS_ELogLevel::EOS_LOG_Error)
		logLevel = IApplication::LogLevel::error;
	else if (Message->Level <= EOS_ELogLevel::EOS_LOG_Warning)
		logLevel = IApplication::LogLevel::warning;

	iApplication->LogToConsole(logLevel, ss.str().c_str());

	// Also send to debug output with trailing newline
	ss << "\n";
	DebugLog(ss.str().c_str());
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
		OnInitMessage(asyncId);
	}
	else if (messageId == "platform-tick")
	{
		if (sharedHandles.hPlatform != nullptr)
		{
			EOS_Platform_Tick(sharedHandles.hPlatform);
		}
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
void WrapperExtension::OnInitMessage(double asyncId)
{
	// Note the actual initialization is done in InitEpicGamesSDK(). This just sends the result
	// of initialization back to the Construct plugin.
	if (didEpicGamesInitOk)
	{
		// Send init data back to JavaScript with key details from the API.
		SendAsyncResponse({
			{ "isAvailable", true },
			{ "isEpicLauncher", isEpicLauncher },
			{ "launcherExchangeCode", launcherExchangeCode }
		}, asyncId);
	}
	else
	{
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
	LogMessage("Starting log in via portal");

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
		LogMessage("OnLogInPortalCallback: success");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		LogMessage("OnLogInPortalCallback: failed");

		SendAsyncResponse({
			{ "isOk", false }
		}, asyncId);
	}
}

void WrapperExtension::HandleSuccessfulLogIn(const EOS_Auth_LoginCallbackInfo* Data, double asyncId)
{
	sharedHandles.epicAccountId = Data->LocalUserId;

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
		LogMessage("EOS_UserInfo_CopyUserInfo succeeded");

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
		LogMessage("EOS_UserInfo_CopyUserInfo failed");
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

void WrapperExtension::OnDeletePersistentAuthCallback(const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
{
	// Just log the success/failure result for debugging.
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		LogMessage("DeletePersistentAuthCallbackFn: success");
	}
	else
	{
		LogMessage("DeletePersistentAuthCallbackFn: failed");
	}
}

void WrapperExtension::OnLogInPersistentMessage(bool basicProfile, bool friendsList, bool presence, bool country, double asyncId)
{
	LogMessage("Starting log in via persistent auth");

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
		LogMessage("OnLogInPersistentCallback: success");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		LogMessage("OnLogInPersistentCallback: failed");

		// Persistent login failed, so delete the persistent auth so it does not use the same
		// persisted auth again next time.
		EOS_Auth_DeletePersistentAuthOptions delOpts = {};
		delOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
		EOS_Auth_DeletePersistentAuth(hAuth, &delOpts, nullptr, [](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			g_Extension->OnDeletePersistentAuthCallback(Data);
		});

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
	LogMessage("Starting log in via exchange code");

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
		LogMessage("OnLogInExchangeCodeCallback: success");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		LogMessage("OnLogInExchangeCodeCallback: failed");

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
	LogMessage("Starting log in via DevAuthTool");

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
		LogMessage("OnLogInDevAuthToolCallback: success");

		HandleSuccessfulLogIn(Data, asyncId);
	}
	else	// login failed
	{
		LogMessage("OnLogInDevAuthToolCallback: failed");

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
	LogMessage("Starting log out");

	EOS_Auth_LogoutOptions LogoutOptions = {};
	LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
	LogoutOptions.LocalUserId = sharedHandles.epicAccountId;

	ExtCallbackInfo* callbackInfo = new ExtCallbackInfo();
	callbackInfo->extension = this;
	callbackInfo->asyncId = asyncId;

	EOS_Auth_Logout(hAuth, &LogoutOptions, callbackInfo, LogoutCompleteCallbackFn);
}

void WrapperExtension::OnLogOutCallback(const EOS_Auth_LogoutCallbackInfo* Data, double asyncId)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		LogMessage("OnLogOutCallback: success");

		// Clear details set when logged in
		sharedHandles.epicAccountId = nullptr;
		sharedHandles.productUserId = nullptr;
		userDisplayName = "";
		userDisplayNameSanitized = "";
		userNickname = "";
		userPreferredLanguage = "";
		userCountry = "";

		// User logged out OK, so also delete any persisted auth to prevent any future
		// automatic login.
		EOS_Auth_DeletePersistentAuthOptions delOpts = {};
		delOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
		EOS_Auth_DeletePersistentAuth(hAuth, &delOpts, nullptr, [](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			g_Extension->OnDeletePersistentAuthCallback(Data);
		});

		SendAsyncResponse({
			{ "isOk", true }
		}, asyncId);
	}
	else
	{
		LogMessage("OnLogOutCallback: failed");

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
	LogMessage("Unlocking achievement");

	const char* achievementIdPtr = achievementId.c_str();

	EOS_Achievements_UnlockAchievementsOptions achievementOpts = {};
	achievementOpts.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
	achievementOpts.UserId = sharedHandles.productUserId;			// from ConnectLogin()
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
		LogMessage("OnUnlockAchievementCallback: success");

		SendAsyncResponse({
			{ "isOk", true }
		}, asyncId);
	}
	else
	{
		LogMessage("OnUnlockAchievementCallback: failed");

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
	LogMessage("ConnectLogin()");

	// Note the SDK samples use EOS_Auth_CopyUserAuthToken() with EOS_EExternalCredentialType::EOS_ECT_EPIC,
	// but the documentation states EOS_Auth_CopyIdToken() with EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN
	// is preferred. See: https://dev.epicgames.com/docs/api-ref/enums/eos-e-external-credential-type
	EOS_Auth_IdToken* authIdToken = nullptr;

	EOS_Auth_CopyIdTokenOptions copyIdTokenOpts = {};
	copyIdTokenOpts.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
	copyIdTokenOpts.AccountId = sharedHandles.epicAccountId;

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

// Callback for EOS_Connect_CreateUser() that forwards to WrapperExtension::OnConnectCreateUserCallback()
void EOS_CALL ConnectCreateUserCallbackFn(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	static_cast<WrapperExtension*>(Data->ClientData)->OnConnectCreateUserCallback(Data);
}

void WrapperExtension::OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		LogMessage("OnConnectLoginCallback: success");

		// Save the product user ID for use with achievements
		sharedHandles.productUserId = Data->LocalUserId;
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
	{
		LogMessage("OnConnectLoginCallback: creating user");

		// Note always resolve by calling EOS_Connect_CreateUser. (Support for linking accounts is not currently implemented.)
		EOS_Connect_CreateUserOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
		Options.ContinuanceToken = Data->ContinuanceToken;

		EOS_Connect_CreateUser(hConnect, &Options, this, ConnectCreateUserCallbackFn);
	}
	else
	{
		LogMessage("OnConnectLoginCallback: failed");
	}
}

void WrapperExtension::OnConnectCreateUserCallback(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		LogMessage("OnConnectCreateUserCallback: success");

		// Save the product user ID for use with achievements
		sharedHandles.productUserId = Data->LocalUserId;
	}
	else
	{
		LogMessage("OnConnectCreateUserCallback: failed");
	}
}

void WrapperExtension::OnConnectAuthExpiration(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
	LogMessage("OnConnectAuthExpiration()");

	// If still logged in, start ConnectLogin() again to refresh the product user ID
	if (sharedHandles.epicAccountId != nullptr)
	{
		ConnectLogin();
	}
}

