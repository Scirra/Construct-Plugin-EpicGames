
const SDK = self.SDK;

////////////////////////////////////////////
// The plugin ID is how Construct identifies different kinds of plugins.
// *** NEVER CHANGE THE PLUGIN ID AFTER RELEASING A PLUGIN! ***
// If you change the plugin ID after releasing the plugin, Construct will think it is an entirely different
// plugin and assume it is incompatible with the old one, and YOU WILL BREAK ALL EXISTING PROJECTS USING THE PLUGIN.
// Only the plugin name is displayed in the editor, so to rename your plugin change the name but NOT the ID.
// If you want to completely replace a plugin, make it deprecated (it will be hidden but old projects keep working),
// and create an entirely new plugin with a different plugin ID.
const PLUGIN_ID = "EpicGames_Ext";
////////////////////////////////////////////

const PLUGIN_CATEGORY = "platform-specific";

const PLUGIN_CLASS = SDK.Plugins.EpicGames_Ext = class EpicGames_Ext extends SDK.IPluginBase
{
	constructor()
	{
		super(PLUGIN_ID);
		
		SDK.Lang.PushContext("plugins." + PLUGIN_ID.toLowerCase());
		
		this._info.SetName(self.lang(".name"));
		this._info.SetDescription(self.lang(".description"));
		this._info.SetCategory(PLUGIN_CATEGORY);
		this._info.SetAuthor("Scirra");
		this._info.SetHelpUrl(self.lang(".help-url"));
		this._info.SetIsSingleGlobal(true);
		
		SDK.Lang.PushContext(".properties");
		
		this._info.SetProperties([
			new SDK.PluginProperty("text", "product-name"),
			new SDK.PluginProperty("text", "product-version"),

			new SDK.PluginProperty("group", "sdk-settings"),
			new SDK.PluginProperty("text", "product-id"),
			new SDK.PluginProperty("text", "client-id"),
			new SDK.PluginProperty("text", "client-secret"),
			new SDK.PluginProperty("text", "sandbox-id"),
			new SDK.PluginProperty("text", "deployment-id"),

			new SDK.PluginProperty("group", "auth-scope"),
			new SDK.PluginProperty("check", "scope-basic-profile", true),
			new SDK.PluginProperty("check", "scope-friends-list"),
			new SDK.PluginProperty("check", "scope-presence"),
			new SDK.PluginProperty("check", "scope-country"),
		]);
		
		SDK.Lang.PopContext();		// .properties
		
		SDK.Lang.PopContext();
		
		// Add necessary DLLs as wrapper extension dependencies.
		this._info.AddFileDependency({
			filename: "EpicGames_x86.ext.dll",
			type: "wrapper-extension",
			platform: "windows-x86"
		});
		
		this._info.AddFileDependency({
			filename: "EOSSDK-Win32-Shipping.dll",
			type: "wrapper-extension",
			platform: "windows-x86"
		});
		
		this._info.AddFileDependency({
			filename: "EpicGames_x64.ext.dll",
			type: "wrapper-extension",
			platform: "windows-x64"
		});
		
		this._info.AddFileDependency({
			filename: "EOSSDK-Win64-Shipping.dll",
			type: "wrapper-extension",
			platform: "windows-x64"
		});
	}
};

PLUGIN_CLASS.Register(PLUGIN_ID, PLUGIN_CLASS);
