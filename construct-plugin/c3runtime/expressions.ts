
import type { SDKInstanceClass } from "./instance.ts";

const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Exps =
{
    LoginStatus(this: SDKInstanceClass)
    {
        return this.loginStatus;
    },

    LauncherExchangeCode(this: SDKInstanceClass)
    {
        return this.launcherExchangeCode;
    },

	EpicAccountIdStr(this: SDKInstanceClass)
    {
        return this.epicAccountIdString;
    },

    DisplayName(this: SDKInstanceClass)
    {
        return this.displayName;
    },

    DisplayNameSanitized(this: SDKInstanceClass)
    {
        return this.displayNameSanitized;
    },

    Nickname(this: SDKInstanceClass)
    {
        return this.nickname;
    },

    PreferredLanguage(this: SDKInstanceClass)
    {
        return this.preferredLanguage;
    },

    UserCountry(this: SDKInstanceClass)
    {
        return this.userCountry;
    },

    Achievement(this: SDKInstanceClass)
	{
		return this._triggerAchievement;
	}
};
