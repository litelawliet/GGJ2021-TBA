#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkHololensPlatformInfo.generated.h"

UCLASS()
class UAkHololensPlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkHololensPlatformInfo()
	{
		WwisePlatform = "Hololens";

#ifdef AK_HOLOLENS_VS_VERSION
		Architecture = "UWP_ARM64_" AK_HOLOLENS_VS_VERSION;
#else
		Architecture = "UWP_ARM64_vc150";
#endif

		LibraryFileNameFormat = "{0}.dll";
		DebugFileNameFormat = "{0}.pdb";
#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("Hololens", "Hololens");
#endif
	}
};
