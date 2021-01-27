#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkWindowsPlatformInfo.generated.h"

UCLASS()
class UAkWin32PlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkWin32PlatformInfo()
	{
		WwisePlatform = "Windows";

#ifdef AK_WINDOWS_VS_VERSION
		Architecture = "Win32_" AK_WINDOWS_VS_VERSION;
#else
		Architecture = "Win32_vc150";
#endif

		LibraryFileNameFormat = "{0}.dll";
		DebugFileNameFormat = "{0}.pdb";
#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("Win32", "Windows");
#endif
	}
};

UCLASS()
class UAkWin64PlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkWin64PlatformInfo()
	{
		WwisePlatform = "Windows";

#ifdef AK_WINDOWS_VS_VERSION
		Architecture = "x64_" AK_WINDOWS_VS_VERSION;
#else
		Architecture = "x64_vc150";
#endif

		LibraryFileNameFormat = "{0}.dll";
		DebugFileNameFormat = "{0}.pdb";

#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("Win64", "Windows");
#endif
	}
};

UCLASS()
class UAkWindowsPlatformInfo : public UAkWin64PlatformInfo
{
	GENERATED_BODY()
};
