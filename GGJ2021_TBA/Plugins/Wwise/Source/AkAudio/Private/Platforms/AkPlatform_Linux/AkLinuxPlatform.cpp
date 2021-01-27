// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#if defined(PLATFORM_LINUX) && PLATFORM_LINUX

#include "Platforms/AkPlatform_Linux/AkLinuxPlatform.h"
#include "Misc/Paths.h"

FString FAkLinuxPlatform::GetDSPPluginsDirectory(const FString& PlatformArchitecture)
{
	return AkUnrealHelper::GetThirdPartyDirectory() / PlatformArchitecture / "Release" / "bin" / "";
}

#endif