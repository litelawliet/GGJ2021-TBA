#include "AkGroupValue.h"

#include "AkAudioDevice.h"

void UAkGroupValue::Load()
{
	Super::Load();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GetPathName(nullptr, packagePath);

		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			audioDevice->OnLoadSwitchValue.Broadcast(packagePath);
		}

		if (GroupShortID == 0)
		{
			if (auto AudioDevice = FAkAudioDevice::Get())
			{
				FString GroupName;
				GetName().Split(TEXT("-"), &GroupName, nullptr);
				auto idFromName = AudioDevice->GetIDFromString(GroupName);
				GroupShortID = idFromName;
			}
		}
	}
}

void UAkGroupValue::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			audioDevice->OnUnloadSwitchValue.Broadcast(packagePath);

			if (GEngine)
			{
				GEngine->ForceGarbageCollection();
			}
		}
	}
}
