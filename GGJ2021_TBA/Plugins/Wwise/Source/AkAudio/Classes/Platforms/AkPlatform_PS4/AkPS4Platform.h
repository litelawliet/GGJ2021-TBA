#pragma once

#if PLATFORM_PS4

#include "Platforms/AkPlatformBase.h"
#include "AkPS4InitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkPS4InitializationSettings;

struct AKAUDIO_API FAkPS4Platform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkPS4InitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("PS4");
	}

	static FString GetWwisePluginDirectory();
	static FString GetDSPPluginsDirectory(const FString& PlatformArchitecture);
};

using FAkPlatform = FAkPS4Platform;

#endif
