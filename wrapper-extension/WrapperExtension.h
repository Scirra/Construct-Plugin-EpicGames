
#include "IApplication.h"
#include "IExtension.h"

// Handles shared with other extensions via the shared pointer API
struct EOS_Shared_Handles {
	EOS_HPlatform hPlatform;
	EOS_EpicAccountId epicAccountId;
	EOS_ProductUserId productUserId;
};

class WrapperExtension : public IExtension {
public:
	WrapperExtension(IApplication* iApplication_);

	void InitEpicGamesSDK(const std::string& productName, const std::string& productVersion);
	void LogMessage(const std::string& msg);
	void OnEOSLogMessage(const EOS_LogMessage* Message);

	// IExtension overrides
	void Init();
	void Release();
	void OnMainWindowCreated(HWND hWnd_);

	// Web messaging methods	
	void OnWebMessage(LPCSTR messageId, size_t paramCount, const ExtensionParameterPOD* paramArr, double asyncId);
	void HandleWebMessage(const std::string& messageId, const std::vector<ExtensionParameter>& params, double asyncId);

	void SendWebMessage(const std::string& messageId, const std::map<std::string, ExtensionParameter>& params, double asyncId = -1.0);
	void SendAsyncResponse(const std::map<std::string, ExtensionParameter>& params, double asyncId);

	// Handler methods for specific kinds of message, and associated callback methods
	void OnInitMessage(double asyncId);
	void OnLogInStatusChanged(const EOS_Auth_LoginStatusChangedCallbackInfo* Data);

	void OnLogInPortalMessage(bool basicProfile, bool friendsList, bool presence, bool country, double asyncId);
	void OnLogInPortalCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId);
	void HandleSuccessfulLogIn(const EOS_Auth_LoginCallbackInfo* Data, double asyncId);

	void OnLogInPersistentMessage(bool basicProfile, bool friendsList, bool presence, bool country, double asyncId);
	void OnLogInPersistentCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId);
	void OnDeletePersistentAuthCallback(const EOS_Auth_DeletePersistentAuthCallbackInfo* Data);

	void OnLogInExchangeCodeMessage(bool basicProfile, bool friendsList, bool presence, bool country, const std::string& exchangeCode, double asyncId);
	void OnLogInExchangeCodeCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId);

	void OnLogInDevAuthToolMessage(bool basicProfile, bool friendsList, bool presence, bool country, const std::string& host, const std::string& credentialName, double asyncId);
	void OnLogInDevAuthToolCallback(const EOS_Auth_LoginCallbackInfo* Data, double asyncId);

	void OnLogOutMessage(double asyncId);
	void OnLogOutCallback(const EOS_Auth_LogoutCallbackInfo* Data, double asyncId);

	void OnUnlockAchievementMessage(const std::string& achievementId, double asyncId);
	void OnUnlockAchievementCallback(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data, double asyncId);

	void ConnectLogin();
	void OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data);
	void OnConnectAuthExpiration(const EOS_Connect_AuthExpirationCallbackInfo* Data);

protected:
	IApplication* iApplication;
	HWND hWndMain;

	bool didEpicGamesInitOk;
	bool isEpicLauncher;
	std::string launcherExchangeCode;

	// Shared handles that other extensions can access with the
	// GetSharedPtr API, allowing use of companion plugins which can
	// access the handles this extension creates.
	EOS_Shared_Handles sharedHandles;

	EOS_HAuth hAuth;
	EOS_HConnect hConnect;
	EOS_HUserInfo hUserInfo;
	EOS_HAchievements hAchievements;

	// Epic Games SDK settings
	std::string productId;
	std::string clientId;
	std::string clientSecret;
	std::string sandboxId;
	std::string deploymentId;

	// Local user information
	std::string userDisplayName;
	std::string userDisplayNameSanitized;
	std::string userNickname;
	std::string userPreferredLanguage;
	std::string userCountry;
};

// For passing to callbacks
struct ExtCallbackInfo {
	WrapperExtension* extension;
	double asyncId;
};