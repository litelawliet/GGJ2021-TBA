#pragma once

#if PLATFORM_MAC

#include "Platforms/AkPlatformBase.h"
#include "AkMacInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkMacInitializationSettings;

struct AKAUDIO_API FAkMacPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkMacInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("Mac");
	}
};

using FAkPlatform = FAkMacPlatform;

#endif
