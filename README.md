# Epic Games for WebView2

This repository contains code for the [Epic Games for WebView2 Construct plugin](https://www.construct.net/en/make-games/addons/1106/epic-games-webview2), and its associated wrapper extension (a DLL which integrates the Epic Online Services SDK, aka EOS SDK). This allows integrating Construct projects with Epic Games using the Windows WebView2 export option. There are two main components in this repository:

- *construct-plugin*: the Construct plugin, written in JavaScript using the [Construct Addon SDK](https://github.com/Scirra/Construct-Addon-SDK)
- *wrapper-extension*: a Visual Studio 2022 project to build the wrapper extension DLL, written in C++.

The wrapper extension builds an *.ext.dll* file in the *construct-plugin* subfolder. The Construct plugin is configured to load that DLL in the WebView2 exporter, and then communicates with it via a messaging API.

## Build

To build the wrapper extension, you will need:

- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) or newer (the *Community* edition is free)
- The [EOS SDK](https://dev.epicgames.com/sdk) - download the *C SDK* and extract the *SDK* subfolder to the *epic-games-sdk* subfolder such that the file `epic-games-sdk\Include\eos_sdk.h` exists.
- An account on the [Epic Games Developer Portal](https://dev.epicgames.com/portal). You will need to create an organization and product and obtain the necessary IDs (*Product ID*, *Client ID*, *Client secret*, *Sandbox ID* and *Deployment ID* - see the [Epic developer documentation](https://dev.epicgames.com/docs) for more details).

The Construct plugin requires 2 DLLs in the x64 (64-bit) architecture. These are:

- **EpicGames_x64.ext.dll** - the wrapper extension DLL, built from the *wrapper-extension* files
- **EOSSDK-Win64-Shipping.dll** - the EOS SDK DLL

For convenience these DLLs are provided in this repository. However if you make changes you may want to replace some of these DLLs.

> [!WARNING]
> If you want to modify the plugin for your own purposes, we strongly advise to **change the Construct plugin ID.** This will avoid serious compatibility problems which could result in your project becoming unopenable. For more information see the [Contributing guide](https://github.com/Scirra/Construct-Plugin-EpicGames/blob/main/CONTRIBUTING.md).

## Testing

Use [developer mode](https://www.construct.net/en/make-games/manuals/addon-sdk/guide/using-developer-mode) for a more convenient way to test the Construct plugin during development.

For details about configuring and exporting projects for Epic Games, see the [Epic Games plugin documentation](https://www.construct.net/en/make-games/addons/1106/epic-games-webview2/documentation).

A sample Construct project is provided in this repository which is just a technical test of the plugin features. However its IDs have been removed so you will need to set up your own on the Developer Portal. Also note the DevAuthTool option will need to be updated with your own host and credential name before it works.

## Distributing

The Construct plugin is distributed as a [.c3addon file](https://www.construct.net/en/make-games/manuals/addon-sdk/guide/c3addon-file), which is essentially a renamed zip file with the addon files.

> [!WARNING]
> If you want to modify the plugin for your own purposes, we strongly advise to **change the Construct plugin ID.** This will avoid serious compatibility problems which could result in your project becoming unopenable. Further, if you wish to add support for more EOS SDK API methods, you may be better off creating an independent plugin rather than modifying this one. For more information see the [Contributing guide](https://github.com/Scirra/Construct-Plugin-EpicGames/blob/main/CONTRIBUTING.md).

## Support

If you think there is an issue with this plugin, please file it following all the guidelines at the [main Construct issue tracker](https://github.com/Scirra/Construct-bugs), so we can keep all our issues in the same place. Issues have been disabled for this repository.

## Contributing

Due to Scirra Ltd's obligation to provide long-term commercial support, we may reject any submitted changes to this plugin. However there are other options such as developing an independent companion plugin. For more information see the [Contributing guide](https://github.com/Scirra/Construct-Plugin-EpicGames/blob/main/CONTRIBUTING.md).

## License

This code is published under the [MIT license](LICENSE).