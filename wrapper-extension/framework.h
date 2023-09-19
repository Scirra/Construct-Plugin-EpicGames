#pragma once

// Windows header files
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <shellapi.h>					// CommandLineToArgv

// STL includes
#include <vector>		// std::vector
#include <map>			// std::map
#include <string>		// std::string, std::wstring

// Include Epic Games SDK.
// Add a compile check for the header as it's not shipped with this codebase.
// The SDK should be extracted in a subfolder named "epic-games-sdk", such that the include path below exists.
#if __has_include("epic-games-sdk\\Include\\eos_sdk.h")

	#include "epic-games-sdk\\Include\\eos_sdk.h"

	#include "epic-games-sdk\\Include\\eos_logging.h"
	#include "epic-games-sdk\\Include\\eos_auth.h"
	#include "epic-games-sdk\\Include\\eos_userinfo.h"
	#include "epic-games-sdk\\Include\\eos_connect.h"
	#include "epic-games-sdk\\Include\\eos_achievements.h"

	// Link Epic Games lib file
#if defined(_M_X64)
	#pragma comment(lib, "epic-games-sdk\\Lib\\EOSSDK-Win64-Shipping.lib")
#elif defined(_M_IX86)
	#pragma comment(lib, "epic-games-sdk\\Lib\\EOSSDK-Win32-Shipping.lib")
#else
	#error "Unable to identify architecture for EOS SDK lib file"
#endif

#else
	#error "Unable to find eos_sdk.h. Make sure the Epic Games SDK is extracted in the epic-games-sdk subfolder such that the file 'epic-games-sdk\\Include\\eos_sdk.h' exists."
#endif

// SDK utilities
#include "Utils.h"
