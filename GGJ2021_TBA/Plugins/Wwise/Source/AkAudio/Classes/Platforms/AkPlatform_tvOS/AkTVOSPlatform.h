#pragma once

#if PLATFORM_TVOS

#include "Platforms/AkPlatformBase.h"
#include "AkTVOSInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkTVOSInitializationSettings;

struct FAkTVOSPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkTVOSInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("tvOS");
	}
};

using FAkPlatform = FAkTVOSPlatform;

#endif
