#pragma once

#include "Platforms/AkPlatformInfo.h"
#include "AkXboxOnePlatformInfo.generated.h"

UCLASS()
class UAkXboxOnePlatformInfo : public UAkPlatformInfo
{
	GENERATED_BODY()

public:
	UAkXboxOnePlatformInfo()
	{
		WwisePlatform = "XboxOne";

#ifdef AK_XBOXONE_VS_VERSION
		Architecture = "XboxOne_" AK_XBOXONE_VS_VERSION;
#else
		Architecture = "XboxOne_vc140";
#endif

		LibraryFileNameFormat = "{0}.dll";
		DebugFileNameFormat = "{0}.pdb";

#if WITH_EDITOR
		UAkPlatformInfo::UnrealNameToWwiseName.Add("XboxOne", "XboxOne");
#endif
	}
};
