// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkPlatform_Windows/AkWindowsInitializationSettings.h"
#include "AkAudioDevice.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"

//////////////////////////////////////////////////////////////////////////
// FAkWindowsAdvancedInitializationSettings

void FAkWindowsAdvancedInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
	Super::FillInitializationStructure(InitializationStructure);

#if PLATFORM_WINDOWS
	if (UseHeadMountedDisplayAudioDevice && IHeadMountedDisplayModule::IsAvailable())
	{
		FString AudioOutputDevice = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();
		if (!AudioOutputDevice.IsEmpty())
			InitializationStructure.InitSettings.settingsMainOutput.idDevice = AK::GetDeviceIDFromName((wchar_t*)*AudioOutputDevice);
	}

	InitializationStructure.PlatformInitSettings.eAudioAPI = static_cast<AkAudioAPI>(AudioAPI);
	InitializationStructure.PlatformInitSettings.bGlobalFocus = GlobalFocus;
#endif // PLATFORM_WINDOWS
}


//////////////////////////////////////////////////////////////////////////
// UAkWindowsInitializationSettings

UAkWindowsInitializationSettings::UAkWindowsInitializationSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAkWindowsInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
#if PLATFORM_64BITS
	#define AK_WINDOWS_ARCHITECTURE "x64_"
#else
	#define AK_WINDOWS_ARCHITECTURE "Win32_"
#endif

#ifdef AK_WINDOWS_VS_VERSION
	constexpr auto PlatformArchitecture = AK_WINDOWS_ARCHITECTURE AK_WINDOWS_VS_VERSION;
#else
	constexpr auto PlatformArchitecture = AK_WINDOWS_ARCHITECTURE "vc150";
#endif

#undef AK_WINDOWS_ARCHITECTURE

	InitializationStructure.SetPluginDllPath(PlatformArchitecture);
	InitializationStructure.SetupLLMAllocFunctions();

	CommonSettings.FillInitializationStructure(InitializationStructure);
	CommunicationSettings.FillInitializationStructure(InitializationStructure);
	AdvancedSettings.FillInitializationStructure(InitializationStructure);

#if PLATFORM_WINDOWS
	InitializationStructure.PlatformInitSettings.uSampleRate = CommonSettings.SampleRate;
#endif
}
