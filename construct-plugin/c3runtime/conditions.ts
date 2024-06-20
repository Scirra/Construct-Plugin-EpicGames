
import type { SDKInstanceClass } from "./instance.ts";

const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Cnds =
{
	IsAvailable(this: SDKInstanceClass)
	{
		return this.isAvailable;
	},

	IsEpicLauncher(this: SDKInstanceClass)
	{
		return this.isEpicLauncher;
	},

	OnLoginComplete(this: SDKInstanceClass)
	{
		return true;
	},

	OnLoginFailed(this: SDKInstanceClass)
	{
		return true;
	},

	OnLogoutComplete(this: SDKInstanceClass)
	{
		return true;
	},

	OnLogoutFailed(this: SDKInstanceClass)
	{
		return true;
	},

	CompareLoginType(this: SDKInstanceClass, loginType: number)
	{
		return this._loginType === loginType;
	},

	OnLoginStatusChanged(this: SDKInstanceClass)
	{
		return true;
	},

	OnAnyAchievementUnlockSuccess(this: SDKInstanceClass)
	{
		return true;
	},

	OnAchievementUnlockSuccess(this: SDKInstanceClass, achievement: string)
	{
		return achievement.toLowerCase() === this._triggerAchievement.toLowerCase();
	},

	OnAnyAchievementUnlockError(this: SDKInstanceClass)
	{
		return true;
	},

	OnAchievementUnlockError(this: SDKInstanceClass, achievement: string)
	{
		return achievement.toLowerCase() === this._triggerAchievement.toLowerCase();
	}
};
