#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkSwitchPlatformInfo.generated.h"

UCLASS()
class UAkSwitchPlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkSwitchPlatformInfo()
	{
		WwisePlatform = "Switch";
		Architecture = "NX64";
		bUsesStaticLibraries = true;

#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("Switch", "Switch");
#endif
	}
};
