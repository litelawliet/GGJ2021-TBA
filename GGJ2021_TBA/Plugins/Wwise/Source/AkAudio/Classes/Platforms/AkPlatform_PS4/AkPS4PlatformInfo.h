#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkPS4PlatformInfo.generated.h"

UCLASS()
class UAkPS4PlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkPS4PlatformInfo()
	{
		WwisePlatform = "PS4";
		Architecture = "PS4";
		LibraryFileNameFormat = "{0}.prx";

#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("PS4", "PS4");
#endif
	}
};
