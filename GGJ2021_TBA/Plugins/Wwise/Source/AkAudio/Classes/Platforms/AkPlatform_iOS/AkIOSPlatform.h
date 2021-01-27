#pragma once

#if PLATFORM_IOS && !PLATFORM_TVOS

#include "Platforms/AkPlatformBase.h"
#include "AkIOSInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkIOSInitializationSettings;

struct AKAUDIO_API FAkIOSPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkIOSInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("iOS");
	}
};

using FAkPlatform = FAkIOSPlatform;

#endif
