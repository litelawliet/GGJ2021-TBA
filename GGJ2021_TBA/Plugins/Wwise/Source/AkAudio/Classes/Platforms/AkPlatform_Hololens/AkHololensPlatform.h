#pragma once

#ifdef PLATFORM_HOLOLENS

#include "Platforms/AkPlatformBase.h"
#include "AkHololensInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const WIDECHAR*)(Text)

using UAkInitializationSettings = UAkHololensInitializationSettings;

struct FAkHololensPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkHololensInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("Hololens");
	}
};

using FAkPlatform = FAkHololensPlatform;

#endif
