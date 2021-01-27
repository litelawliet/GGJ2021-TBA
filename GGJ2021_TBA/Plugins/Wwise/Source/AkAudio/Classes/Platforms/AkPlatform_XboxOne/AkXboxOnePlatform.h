#pragma once

#if PLATFORM_XBOXONE

#include "Platforms/AkPlatformBase.h"
#include "AkXboxOneInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const WIDECHAR*)(Text)

using UAkInitializationSettings = UAkXboxOneInitializationSettings;

struct FAkXboxOnePlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkXboxOneInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("XboxOne");
	}
};

using FAkPlatform = FAkXboxOnePlatform;

#endif
