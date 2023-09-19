
const C3 = self.C3;

C3.Plugins.EpicGames_Ext.Cnds =
{
	IsAvailable()
	{
		return this._IsAvailable();
	},

	IsEpicLauncher()
	{
		return this._IsEpicLauncher();
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
