
const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Exps =
{
    LoginStatus()
    {
        return this.loginStatus;
    },

    LauncherExchangeCode()
    {
        return this.launcherExchangeCode;
    },

	EpicAccountIdStr()
    {
        return this.epicAccountIdString;
    },

    DisplayName()
    {
        return this.displayName;
    },

    DisplayNameSanitized()
    {
        return this.displayNameSanitized;
    },

    Nickname()
    {
        return this.nickname;
    },

    PreferredLanguage()
    {
        return this.preferredLanguage;
    },

    UserCountry()
    {
        return this.userCountry;
    },

    Achievement()
	{
		return this._triggerAchievement;
	}
};
