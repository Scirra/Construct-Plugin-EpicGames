
const C3 = self.C3;

C3.Plugins.EpicGames_Ext.Instance = class EpicGames_ExtInstance extends C3.SDKInstanceBase
{
	constructor(inst, properties)
	{
		super(inst);
		
		// Set "scirra-epic-games" component ID, matching the same component ID set by the wrapper extension.
		this.SetWrapperExtensionComponentId("scirra-epic-games");
		
		this._isAvailable = false;

		// For ticking while loading
		this._loadingTimerId = -1;

		// Properties
		this._productName = "";
		this._productVersion = "";

		this._productId = "";
		this._clientId = "";
		this._clientSecret = "";
		this._sandboxId = "";
		this._deploymentId = "";

		// Auth scope flags
		this._scopeBasicProfile = true;
		this._scopeFriendsList = false;
		this._scopePresence = false;
		this._scopeCountry = false;

		// Other auth details
		this._loginType = -1;					// 0 = portal, 1 = persistent, 2 = exchange code, 3 = DevAuthTool
		this._isEpicLauncher = false;
		this._launcherExchangeCode = "";
		this._loginStatus = "not-logged-in";	// one of: "not-logged-in", "logged-in", "using-local-profile"

		// User information
		this._epicAccountIdStr = "";
		this._displayName = "";
		this._displayNameSanitized = "";
		this._nickname = "";
		this._preferredLanguage = "";
		this._userCountry = "";

		// For triggers
		this._triggerAchievement = "";
		
		if (properties)
		{
			this._productName = properties[0];
			this._productVersion = properties[1];

			this._productId = properties[2];
			this._clientId = properties[3];
			this._clientSecret = properties[4];
			this._sandboxId = properties[5];
			this._deploymentId = properties[6];

			this._scopeBasicProfile = properties[7];
			this._scopeFriendsList = properties[8];
			this._scopePresence = properties[9];
			this._scopeCountry = properties[10];
		}

		// Listen for login status change events from the extension.
		this.AddWrapperExtensionMessageHandler("on-login-status-changed", e => this._OnLoginStatusChanged(e));

		// Corresponding wrapper extension is available
		if (this.IsWrapperExtensionAvailable())
		{
			// Run async init during loading
			this._runtime.AddLoadPromise(this._Init());

			// Epic Games needs the app to regularly call EOS_Platform_Tick(), which is done by sending
			// the "platform-tick" message every tick. However Construct's Tick() callback only starts
			// once the loading screen finishes. In order to allow Epic Games to continue ticking while
			// the loading screen is showing, set a timer to tick every 20ms until the first tick,
			// which then clears the timer.
			this._loadingTimerId = self.setInterval(() => this._PlatformTick(), 20);

			this._StartTicking();
		}
	}
	
	async _Init()
	{
		// Send init message to wrapper extension and wait for result.
		const result = await this.SendWrapperExtensionMessageAsync("init", [
			// First 2 parameters are product name and product version.
			// If either is left empty, the project name/version are used instead.
			this._productName || this._runtime.GetProjectName(),
			this._productVersion || this._runtime.GetProjectVersion(),

			// Next 5 parameters are SDK settings
			this._productId,
			this._clientId,
			this._clientSecret,
			this._sandboxId,
			this._deploymentId
		]);
		
		// Check availability of Epic Games features.
		this._isAvailable = result["isAvailable"];

		if (this._isAvailable)
		{
			this._isEpicLauncher = result["isEpicLauncher"];
			this._launcherExchangeCode = result["launcherExchangeCode"];
		}
	}
	
	Release()
	{
		super.Release();
	}

	Tick()
	{
		// On the first tick, clear the timer running for the loading screen.
		if (this._loadingTimerId !== -1)
		{
			self.clearInterval(this._loadingTimerId);
			this._loadingTimerId = -1;
		}
		
		this._PlatformTick();
	}

	_PlatformTick()
	{
		// Tell extension to call EOS_Platform_Tick().
		this.SendWrapperExtensionMessage("platform-tick");
	}

	_IsAvailable()
	{
		return this._isAvailable;
	}

	_IsEpicLauncher()
	{
		return this._isEpicLauncher;
	}

	_GetLauncherExchangeCode()
	{
		return this._launcherExchangeCode;
	}

	async _LogInPortal()
	{
		if (!this._IsAvailable())
			return;

		// Set portal login type for 'Compare login type' condition
		this._loginType = 0;

		const result = await this.SendWrapperExtensionMessageAsync("log-in-portal", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry
		]);

		this._HandleLogInResult(result);
	}

	async _LogInPersistent()
	{
		if (!this._IsAvailable())
			return;

		// Set persistent login type for 'Compare login type' condition
		this._loginType = 1;

		const result = await this.SendWrapperExtensionMessageAsync("log-in-persistent", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry
		]);

		this._HandleLogInResult(result);
	}

	async _LogInExchangeCode(exchangeCode)
	{
		if (!this._IsAvailable())
			return;

		// Set exchange code login type for 'Compare login type' condition
		this._loginType = 2;

		const result = await this.SendWrapperExtensionMessageAsync("log-in-exchange-code", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry,
			exchangeCode
		]);

		this._HandleLogInResult(result);
	}

	async _LogInDevAuthTool(host, credentialName)
	{
		if (!this._IsAvailable())
			return;

		// Set DevAuthTool login type for 'Compare login type' condition
		this._loginType = 3;

		const result = await this.SendWrapperExtensionMessageAsync("log-in-devauthtool", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry,
			host,
			credentialName
		]);

		this._HandleLogInResult(result);
	}

	_HandleLogInResult(result)
	{
		if (result["isOk"])
		{
			// Save available user details after successful login
			this._epicAccountIdStr = result["epicAccountIdStr"];
			this._displayName = result["displayName"];
			this._displayNameSanitized = result["displayNameSanitized"];
			this._nickname = result["nickname"];
			this._preferredLanguage = result["preferredLanguage"];
			this._userCountry = result["country"];
			
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginComplete);
		}
		else
		{
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginFailed);
		}
	}

	async _LogOut()
	{
		if (!this._IsAvailable())
			return;

		const result = await this.SendWrapperExtensionMessageAsync("log-out");

		if (result["isOk"])
		{
			// Clear all details provided when logged in
			this._loginType = -1;				// no login type
			this._epicAccountIdStr = "";
			this._displayName = "";
			this._displayNameSanitized = "";
			this._nickname = "";
			this._preferredLanguage = "";
			this._userCountry = "";
			
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLogoutComplete);
		}
		else
		{
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLogoutFailed);
		}
	}

	_OnLoginStatusChanged(e)
	{
		const loginStatus = e["loginStatus"];

		// The passed login status is an EOS_ELoginStatus enum converted to a number.
		// Refer to the EOS SDK for corresponding values if any more statuses are added in future.
		switch (loginStatus) {
		case 0:		// EOS_LS_NotLoggedIn
			this._loginStatus = "not-logged-in";
			this._loginType = -1;			// also reset login type
			break;
		case 1:		// EOS_LS_UsingLocalProfile
			this._loginStatus = "using-local-profile";
			break;
		case 2:		// EOS_LS_LoggedIn
			this._loginStatus = "logged-in";
			break;
		default:
			console.warn("[EpicExt] Unknown login status: " + loginStatus);
			break;
		}

		this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginStatusChanged);
	}

	_GetLoginStatus()
	{
		return this._loginStatus;
	}

	_GetEpicAccountIdString()
	{
		return this._epicAccountIdStr;
	}

	_GetDisplayName()
	{
		return this._displayName;
	}

	_GetDisplayNameSanitized()
	{
		return this._displayNameSanitized;
	}

	_GetNickname()
	{
		return this._nickname;
	}

	_GetPreferredLanguage()
	{
		return this._preferredLanguage;
	}

	_GetUserCountry()
	{
		return this._userCountry;
	}

	async _UnlockAchievement(achievement)
	{
		if (!this._IsAvailable())
			return false;
		
		const result = await this.SendWrapperExtensionMessageAsync("unlock-achievement", [achievement]);

		this._triggerAchievement = achievement;

		const isOk = result["isOk"];
		if (isOk)
		{
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAnyAchievementUnlockSuccess);
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAchievementUnlockSuccess);
		}
		else
		{
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAnyAchievementUnlockError);
			this.Trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAchievementUnlockError);
		}

		// Return result for script interface
		return isOk;
	}
	
	SaveToJson()
	{
		return {
			// data to be saved for savegames
		};
	}
	
	LoadFromJson(o)
	{
		// load state for savegames
	}

	GetScriptInterfaceClass()
	{
		return self.IEpicGamesExtGlobalInstance;
	}
};

// Script interface. Use a WeakMap to safely hide the internal implementation details from the
// caller using the script interface.
const map = new WeakMap();

self.IEpicGamesExtGlobalInstance = class IEpicGamesExtGlobalInstance extends self.IInstance {
	constructor()
	{
		super();
		
		// Map by SDK instance
		map.set(this, self.IInstance._GetInitInst().GetSdkInstance());
	}

	get isAvailable()
	{
		return map.get(this)._IsAvailable();
	}
};
