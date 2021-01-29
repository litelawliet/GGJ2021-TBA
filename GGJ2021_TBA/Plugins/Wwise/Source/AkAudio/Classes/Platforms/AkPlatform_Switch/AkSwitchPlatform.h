#pragma once

#if PLATFORM_SWITCH

#include "Platforms/AkPlatformBase.h"
#include "AkSwitchInitializationSettings.h"

#define TCHAR_TO_AK(Text) (const ANSICHAR*)(TCHAR_TO_ANSI(Text))

using UAkInitializationSettings = UAkSwitchInitializationSettings;

struct AKAUDIO_API FAkSwitchPlatform : FAkPlatformBase
{
	static const UAkInitializationSettings* GetInitializationSettings()
	{
		return GetDefault<UAkSwitchInitializationSettings>();
	}

	static const FString GetPlatformBasePath()
	{
		return FString("Switch");
	}
};

using FAkPlatform = FAkSwitchPlatform;

#endif
