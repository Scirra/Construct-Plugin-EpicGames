
const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Instance = class EpicGames_ExtInstance extends globalThis.ISDKInstanceBase
{
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
		this._addWrapperExtensionMessageHandler("on-login-status-changed", e => this._onLoginStatusChanged(e));

		// Corresponding wrapper extension is available
		if (this._isWrapperExtensionAvailable())
		{
			// Run async init during loading
			this.runtime.addLoadPromise(this._init());

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
		]);
		
		// Check availability of Epic Games features.
		this._isAvailable = result["isAvailable"];

		if (this._isAvailable)
		{
			this._isEpicLauncher = result["isEpicLauncher"];
			this._launcherExchangeCode = result["launcherExchangeCode"];
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

		this._handleLogInResult(result);
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

		this._handleLogInResult(result);
	}

	async logInExchangeCode(exchangeCode)
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

		this._handleLogInResult(result);
	}

	async logInDevAuthTool(host, credentialName)
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

		this._handleLogInResult(result);
	}

	_handleLogInResult(result)
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

		const result = await this._sendWrapperExtensionMessageAsync("log-out");

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

	_onLoginStatusChanged(e)
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

	async unlockAchievement(achievement)
	{
		if (!this._isAvailable)
			return false;
		
		const result = await this._sendWrapperExtensionMessageAsync("unlock-achievement", [achievement]);

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
	
	_loadFromJson(o)
	{
		// load state for savegames
	}
};
