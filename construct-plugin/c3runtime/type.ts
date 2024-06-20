
import type { SDKInstanceClass } from "./instance.ts";

const C3 = globalThis.C3;

C3.Plugins.EpicGames_Ext.Type = class EpicGamesExtType extends globalThis.ISDKObjectTypeBase<SDKInstanceClass>
{
	constructor()
	{
		super();
	}
	
	_onCreate()
	{	
	}
};
