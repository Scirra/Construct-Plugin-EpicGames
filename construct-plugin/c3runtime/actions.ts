
import type { SDKInstanceClass } from "./instance.ts";

const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Acts =
{
	async LogInPortal(this: SDKInstanceClass)
    {
        await this.logInPortal();
    },

    async LogInPersistent(this: SDKInstanceClass)
    {
        await this.logInPersistent();
    },

    async LogInExchangeCode(this: SDKInstanceClass, exchangeCode: string)
    {
        await this.logInExchangeCode(exchangeCode);
    },

    async LogInDevAuthTool(this: SDKInstanceClass, host: string, credentialName: string)
    {
        await this.logInDevAuthTool(host, credentialName);
    },

    async LogOut(this: SDKInstanceClass)
    {
        await this.logOut();
    },

    async UnlockAchievement(this: SDKInstanceClass, achievementId: string)
    {
        await this.unlockAchievement(achievementId);
    }
};
