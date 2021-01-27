// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkUEPlatform.h"
#include "AkAudioDevice.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"

#if WITH_EDITOR
#include "PlatformInfo.h"
#include "Platforms/AkPlatformInfo.h"
TMap<FString, FString> UAkPlatformInfo::UnrealNameToWwiseName;

TSet<FString> AkUnrealPlatformHelper::GetAllSupportedUnrealPlatforms()
{
	TSet<FString> SupportedPlatforms;

#if UE_4_23_OR_LATER
	for (const auto& Info : PlatformInfo::GetPlatformInfoArray())
#else
	for (const auto& Info : PlatformInfo::EnumeratePlatformInfoArray())
#endif
	{
#if UE_4_24_OR_LATER
		bool bIsGame = Info.PlatformType == EBuildTargetType::Game;
#else
		bool bIsGame = Info.PlatformType == PlatformInfo::EPlatformType::Game;
#endif
		if (Info.IsVanilla() && bIsGame && (Info.PlatformInfoName != TEXT("AllDesktop")))
		{
			FString VanillaName = Info.VanillaPlatformName.ToString();
			VanillaName.RemoveFromEnd(TEXT("NoEditor"));
			SupportedPlatforms.Add(VanillaName);
		}
	}

	return SupportedPlatforms;
}

TSet<FString> AkUnrealPlatformHelper::GetAllSupportedUnrealPlatformsForProject()
{
	TSet<FString> SupportedPlatforms = GetAllSupportedUnrealPlatforms();
	IProjectManager& ProjectManager = IProjectManager::Get();
	auto* CurrentProject = ProjectManager.GetCurrentProject();
	if (CurrentProject && CurrentProject->TargetPlatforms.Num() > 0)
	{
		auto& TargetPlatforms = CurrentProject->TargetPlatforms;
		TSet<FString> AvailablePlatforms;
		for (const auto& TargetPlatform : TargetPlatforms)
		{
			FString PlatformName = TargetPlatform.ToString();
			PlatformName.RemoveFromEnd(TEXT("NoEditor"));
			AvailablePlatforms.Add(PlatformName);
		}

		auto Intersection = SupportedPlatforms.Intersect(AvailablePlatforms);
		if (Intersection.Num() > 0)
		{
			SupportedPlatforms = Intersection;
		}
	}

	return SupportedPlatforms;
}

TArray<TSharedPtr<FString>> AkUnrealPlatformHelper::GetAllSupportedWwisePlatforms(bool ProjectScope /* = false */)
{
	TArray<TSharedPtr<FString>> WwisePlatforms;

	TSet<FString> TemporaryWwisePlatformNames;
	auto UnrealPlatforms = ProjectScope ? GetAllSupportedUnrealPlatformsForProject() : GetAllSupportedUnrealPlatforms();
	for (const auto& AvailablePlatform : UnrealPlatforms)
	{
		FString SettingsClassName = FString::Format(TEXT("Ak{0}InitializationSettings"), { *AvailablePlatform });
		if (FindObject<UClass>(ANY_PACKAGE, *SettingsClassName))
		{
			TemporaryWwisePlatformNames.Add(AvailablePlatform);
		}
	}

	TemporaryWwisePlatformNames.Sort([](const FString& L, const FString& R) { return L.Compare(R) < 0; });

	for (const auto WwisePlatformName : TemporaryWwisePlatformNames)
	{
		WwisePlatforms.Add(TSharedPtr<FString>(new FString(WwisePlatformName)));
	}

	return WwisePlatforms;
}
#endif // WITH_EDITOR
