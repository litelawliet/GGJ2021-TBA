#pragma once

#if PLATFORM_WINDOWS

#include "Platforms/AkPlatformBase.h"
#include "AkWindowsInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const WIDECHAR*)(Text)

using UAkInitializationSettings = UAkWindowsInitializationSettings;

struct AKAUDIO_API FAkWindowsPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkWindowsInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("Windows");
	}
};

using FAkPlatform = FAkWindowsPlatform;

#endif
