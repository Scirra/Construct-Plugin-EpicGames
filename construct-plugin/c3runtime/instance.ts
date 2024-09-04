
const C3 = globalThis.C3;

class EpicGames_ExtInstance extends globalThis.ISDKInstanceBase
{
	_isAvailable: boolean;
	_loadingTimerId: number;

	_productName: string;
	_productVersion: string;

	_productId: string;
	_clientId: string;
	_clientSecret: string;
	_sandboxId: string;
	_deploymentId: string;

	_scopeBasicProfile: boolean;
	_scopeFriendsList: boolean;
	_scopePresence: boolean;
	_scopeCountry: boolean;

	_loginType: number;
	_isEpicLauncher: boolean;
	_launcherExchangeCode: string;
	_loginStatus: string;

	_epicAccountIdStr: string;
	_displayName: string;
	_displayNameSanitized: string;
	_nickname: string;
	_preferredLanguage: string;
	_userCountry: string;

	_triggerAchievement: string;

	constructor()
	{
		// Set "scirra-epic-games" component ID, matching the same component ID set by the wrapper extension.
		super({ wrapperComponentId: "scirra-epic-games" });
		
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
		
		const properties = this._getInitProperties();
		if (properties)
		{
			this._productName = properties[0] as string;
			this._productVersion = properties[1] as string;

			this._productId = properties[2] as string;
			this._clientId = properties[3] as string;
			this._clientSecret = properties[4] as string;
			this._sandboxId = properties[5] as string;
			this._deploymentId = properties[6] as string;

			this._scopeBasicProfile = properties[7] as boolean;
			this._scopeFriendsList = properties[8] as boolean;
			this._scopePresence = properties[9] as boolean;
			this._scopeCountry = properties[10] as boolean;
		}

		// Listen for login status change events from the extension.
		this._addWrapperExtensionMessageHandler("on-login-status-changed", e => this._onLoginStatusChanged(e as JSONObject));

		// Corresponding wrapper extension is available
		if (this._isWrapperExtensionAvailable())
		{
			// Run async init during loading
			this.runtime.sdk.addLoadPromise(this._init());

			// Epic Games needs the app to regularly call EOS_Platform_Tick(), which is done by sending
			// the "platform-tick" message every tick. However Construct's Tick() callback only starts
			// once the loading screen finishes. In order to allow Epic Games to continue ticking while
			// the loading screen is showing, set a timer to tick every 20ms until the first tick,
			// which then clears the timer.
			this._loadingTimerId = globalThis.setInterval(() => this._platformTick(), 20);

			this._setTicking(true);
		}
	}
	
	async _init()
	{
		// Send init message to wrapper extension and wait for result.
		const result = await this._sendWrapperExtensionMessageAsync("init", [
			// First 2 parameters are product name and product version.
			// If either is left empty, the project name/version are used instead.
			this._productName || this.runtime.projectName,
			this._productVersion || this.runtime.projectVersion,

			// Next 5 parameters are SDK settings
			this._productId,
			this._clientId,
			this._clientSecret,
			this._sandboxId,
			this._deploymentId
		]) as JSONObject;
		
		// Check availability of Epic Games features.
		this._isAvailable = result["isAvailable"] as boolean;

		if (this._isAvailable)
		{
			this._isEpicLauncher = result["isEpicLauncher"] as boolean;
			this._launcherExchangeCode = result["launcherExchangeCode"] as string;
		}
	}
	
	_release()
	{
		super._release();
	}

	_tick()
	{
		// On the first tick, clear the timer running for the loading screen.
		if (this._loadingTimerId !== -1)
		{
			globalThis.clearInterval(this._loadingTimerId);
			this._loadingTimerId = -1;
		}
		
		this._platformTick();
	}

	_platformTick()
	{
		// Tell extension to call EOS_Platform_Tick().
		this._sendWrapperExtensionMessage("platform-tick");
	}

	get isAvailable()
	{
		return this._isAvailable;
	}

	get isEpicLauncher()
	{
		return this._isEpicLauncher;
	}

	get launcherExchangeCode()
	{
		return this._launcherExchangeCode;
	}

	async logInPortal()
	{
		if (!this._isAvailable)
			return;

		// Set portal login type for 'Compare login type' condition
		this._loginType = 0;

		const result = await this._sendWrapperExtensionMessageAsync("log-in-portal", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry
		]);

		this._handleLogInResult(result as JSONObject);
	}

	async logInPersistent()
	{
		if (!this._isAvailable)
			return;

		// Set persistent login type for 'Compare login type' condition
		this._loginType = 1;

		const result = await this._sendWrapperExtensionMessageAsync("log-in-persistent", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry
		]);

		this._handleLogInResult(result as JSONObject);
	}

	async logInExchangeCode(exchangeCode: string)
	{
		if (!this._isAvailable)
			return;

		// Set exchange code login type for 'Compare login type' condition
		this._loginType = 2;

		const result = await this._sendWrapperExtensionMessageAsync("log-in-exchange-code", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry,
			exchangeCode
		]);

		this._handleLogInResult(result as JSONObject);
	}

	async logInDevAuthTool(host: string, credentialName: string)
	{
		if (!this._isAvailable)
			return;

		// Set DevAuthTool login type for 'Compare login type' condition
		this._loginType = 3;

		const result = await this._sendWrapperExtensionMessageAsync("log-in-devauthtool", [
			this._scopeBasicProfile,
			this._scopeFriendsList,
			this._scopePresence,
			this._scopeCountry,
			host,
			credentialName
		]);

		this._handleLogInResult(result as JSONObject);
	}

	_handleLogInResult(result: JSONObject)
	{
		if (result["isOk"])
		{
			// Save available user details after successful login
			this._epicAccountIdStr = result["epicAccountIdStr"] as string;
			this._displayName = result["displayName"] as string;
			this._displayNameSanitized = result["displayNameSanitized"] as string;
			this._nickname = result["nickname"] as string;
			this._preferredLanguage = result["preferredLanguage"] as string;
			this._userCountry = result["country"] as string;
			
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginComplete);
		}
		else
		{
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginFailed);
		}
	}

	async logOut()
	{
		if (!this._isAvailable)
			return;

		const result = await this._sendWrapperExtensionMessageAsync("log-out") as JSONObject;

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
			
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLogoutComplete);
		}
		else
		{
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLogoutFailed);
		}
	}

	_onLoginStatusChanged(e: JSONObject)
	{
		const loginStatus = e["loginStatus"] as number;

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

		this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnLoginStatusChanged);
	}

	get loginStatus()
	{
		return this._loginStatus;
	}

	get epicAccountIdString()
	{
		return this._epicAccountIdStr;
	}

	get displayName()
	{
		return this._displayName;
	}

	get displayNameSanitized()
	{
		return this._displayNameSanitized;
	}

	get nickname()
	{
		return this._nickname;
	}

	get preferredLanguage()
	{
		return this._preferredLanguage;
	}

	get userCountry()
	{
		return this._userCountry;
	}

	async unlockAchievement(achievement: string)
	{
		if (!this._isAvailable)
			return false;
		
		const result = await this._sendWrapperExtensionMessageAsync("unlock-achievement", [achievement]) as JSONObject;

		this._triggerAchievement = achievement;

		const isOk = result["isOk"];
		if (isOk)
		{
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAnyAchievementUnlockSuccess);
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAchievementUnlockSuccess);
		}
		else
		{
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAnyAchievementUnlockError);
			this._trigger(C3.Plugins.EpicGames_Ext.Cnds.OnAchievementUnlockError);
		}

		// Return result for script interface
		return isOk;
	}
	
	_saveToJson()
	{
		return {
			// data to be saved for savegames
		};
	}
	
	_loadFromJson(o: JSONValue)
	{
		// load state for savegames
	}
};

C3.Plugins.EpicGames_Ext.Instance = EpicGames_ExtInstance;

export type { EpicGames_ExtInstance as SDKInstanceClass };
