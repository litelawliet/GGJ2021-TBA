#pragma once

#include "AkInclude.h"

struct AKAUDIO_API FAkPlatformBase
{
	static FString GetWwisePluginDirectory();
	static FString GetDSPPluginsDirectory(const FString& PlatformArchitecture);
};
