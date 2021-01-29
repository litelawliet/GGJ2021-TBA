#pragma once

#if PLATFORM_LINUX

#include "Platforms/AkPlatformBase.h"
#include "AkLinuxInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkLinuxInitializationSettings;

struct AKAUDIO_API FAkLinuxPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkLinuxInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("Linux");
	}

	static FString GetDSPPluginsDirectory(const FString& PlatformArchitecture);
};

using FAkPlatform = FAkLinuxPlatform;

#endif
