
const C3 = self.C3;

C3.Plugins.EpicGames_Ext.Exps =
{
    LoginStatus()
    {
        return this._GetLoginStatus();
    },

    LauncherExchangeCode()
    {
        return this._GetLauncherExchangeCode();
    },

	EpicAccountIdStr()
    {
        return this._GetEpicAccountIdString();
    },

    DisplayName()
    {
        return this._GetDisplayName();
    },

    DisplayNameSanitized()
    {
        return this._GetDisplayNameSanitized();
    },

    Nickname()
    {
        return this._GetNickname();
    },

    PreferredLanguage()
    {
        return this._GetPreferredLanguage();
    },

    UserCountry()
    {
        return this._GetUserCountry();
    },

    Achievement()
	{
		return this._triggerAchievement;
	}
};
