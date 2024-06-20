
const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Acts =
{
	async LogInPortal()
    {
        await this.logInPortal();
    },

    async LogInPersistent()
    {
        await this.logInPersistent();
    },

    async LogInExchangeCode(exchangeCode)
    {
        await this.logInExchangeCode(exchangeCode);
    },

    async LogInDevAuthTool(host, credentialName)
    {
        await this.logInDevAuthTool(host, credentialName);
    },

    async LogOut()
    {
        await this.logOut();
    },

    async UnlockAchievement(achievementId)
    {
        await this.unlockAchievement(achievementId);
    }
};
