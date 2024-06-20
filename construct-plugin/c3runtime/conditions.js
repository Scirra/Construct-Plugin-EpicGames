
const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Cnds =
{
	IsAvailable()
	{
		return this.isAvailable;
	},

	IsEpicLauncher()
	{
		return this.isEpicLauncher;
	},

	OnLoginComplete()
	{
		return true;
	},

	OnLoginFailed()
	{
		return true;
	},

	OnLogoutComplete()
	{
		return true;
	},

	OnLogoutFailed()
	{
		return true;
	},

	CompareLoginType(loginType)
	{
		return this._loginType === loginType;
	},

	OnLoginStatusChanged()
	{
		return true;
	},

	OnAnyAchievementUnlockSuccess()
	{
		return true;
	},

	OnAchievementUnlockSuccess(achievement)
	{
		return achievement.toLowerCase() === this._triggerAchievement.toLowerCase();
	},

	OnAnyAchievementUnlockError()
	{
		return true;
	},

	OnAchievementUnlockError(achievement)
	{
		return achievement.toLowerCase() === this._triggerAchievement.toLowerCase();
	}
};
