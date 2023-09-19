
const C3 = self.C3;

C3.Plugins.EpicGames_Ext.Acts =
{
	async LogInPortal()
    {
        await this._LogInPortal();
    },

    async LogInPersistent()
    {
        await this._LogInPersistent();
    },

    async LogInExchangeCode(exchangeCode)
    {
        await this._LogInExchangeCode(exchangeCode);
    },

    async LogInDevAuthTool(host, credentialName)
    {
        await this._LogInDevAuthTool(host, credentialName);
    },

    async LogOut()
    {
        await this._LogOut();
    },

    async UnlockAchievement(achievementId)
    {
        await this._UnlockAchievement(achievementId);
    }
};
