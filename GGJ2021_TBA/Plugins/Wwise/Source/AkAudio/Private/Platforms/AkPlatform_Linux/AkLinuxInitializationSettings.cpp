// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "Platforms/AkPlatform_Linux/AkLinuxInitializationSettings.h"
#include "AkAudioDevice.h"

//////////////////////////////////////////////////////////////////////////
// UAkLinuxInitializationSettings

UAkLinuxInitializationSettings::UAkLinuxInitializationSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAkLinuxInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
#if PLATFORM_64BITS
	InitializationStructure.SetPluginDllPath("Linux_x64");
#else
	ensure(!"The Wwise Unreal Engine 4 integration does not support 32-bit Linux distributions.");
	InitializationStructure.SetPluginDllPath("Linux_x32");
#endif

	InitializationStructure.SetupLLMAllocFunctions();

	CommonSettings.FillInitializationStructure(InitializationStructure);
	CommunicationSettings.FillInitializationStructure(InitializationStructure);
	AdvancedSettings.FillInitializationStructure(InitializationStructure);

#if PLATFORM_LINUX
	InitializationStructure.PlatformInitSettings.uSampleRate = CommonSettings.SampleRate;
#endif
}
