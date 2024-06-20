
const SDK = self.SDK;

const PLUGIN_CLASS = SDK.Plugins.EpicGames_Ext;

PLUGIN_CLASS.Type = class EpicGamesExtType extends SDK.ITypeBase
{
	constructor(sdkPlugin: SDK.IPluginBase, iObjectType: SDK.IObjectType)
	{
		super(sdkPlugin, iObjectType);
	}
};

export {}