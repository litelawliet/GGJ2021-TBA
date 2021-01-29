// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkPlatformBase.h"
#include "AkAudioDevice.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "AkUnrealHelper.h"

FString FAkPlatformBase::GetWwisePluginDirectory()
{
	return FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("Wwise"))->GetBaseDir());
}

FString FAkPlatformBase::GetDSPPluginsDirectory(const FString& PlatformArchitecture)
{
#if UE_BUILD_SHIPPING
	auto* Configuration = "Release";
#elif UE_BUILD_DEBUG
	auto* Configuration = "Debug";
#else
	auto* Configuration = "Profile";
#endif

	return AkUnrealHelper::GetThirdPartyDirectory() / PlatformArchitecture / Configuration / "bin" / "";
}
