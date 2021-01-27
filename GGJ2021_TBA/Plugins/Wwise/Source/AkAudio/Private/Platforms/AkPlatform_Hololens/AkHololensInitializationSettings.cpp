// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkPlatform_Hololens/AkHololensInitializationSettings.h"
#include "AkAudioDevice.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"

//////////////////////////////////////////////////////////////////////////
// FAkHololensAdvancedInitializationSettings

void FAkHololensAdvancedInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
	Super::FillInitializationStructure(InitializationStructure);

#if PLATFORM_HOLOLENS
	if (UseHeadMountedDisplayAudioDevice && IHeadMountedDisplayModule::IsAvailable())
	{
		FString AudioOutputDevice = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();
		if (!AudioOutputDevice.IsEmpty())
			InitializationStructure.InitSettings.settingsMainOutput.idDevice = AK::GetDeviceIDFromName((wchar_t*)*AudioOutputDevice);
	}

	InitializationStructure.PlatformInitSettings.eAudioAPI = static_cast<AkAudioAPI>(AudioAPI);
	InitializationStructure.PlatformInitSettings.bGlobalFocus = GlobalFocus;
#endif // AK_GDX
}


//////////////////////////////////////////////////////////////////////////
// UAkHololensInitializationSettings

UAkHololensInitializationSettings::UAkHololensInitializationSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAkHololensInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{

#ifdef AK_HOLOLENS_VS_VERSION
	constexpr auto PlatformArchitecture = "UWP_ARM64_" AK_HOLOLENS_VS_VERSION;
#else
	constexpr auto PlatformArchitecture = "UWP_ARM64_vc150";
#endif

	InitializationStructure.SetPluginDllPath(PlatformArchitecture);
	InitializationStructure.SetupLLMAllocFunctions();

	CommonSettings.FillInitializationStructure(InitializationStructure);
	CommunicationSettings.FillInitializationStructure(InitializationStructure);
	AdvancedSettings.FillInitializationStructure(InitializationStructure);

#if PLATFORM_HOLOLENS
	InitializationStructure.PlatformInitSettings.uSampleRate = CommonSettings.SampleRate;
#endif
}
