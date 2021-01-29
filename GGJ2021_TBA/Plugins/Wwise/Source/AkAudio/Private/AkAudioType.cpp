#include "AkAudioType.h"

#include "Async/Async.h"
#include "AkAudioDevice.h"
#include "AkGroupValue.h"
#include "Core/Public/Modules/ModuleManager.h"

void UAkAudioType::PostLoad()
{
	static FName AkAudioName = TEXT("AkAudio");
	Super::PostLoad();
	if (FModuleManager::Get().IsModuleLoaded(AkAudioName))
	{
		Load();
	}
	else
	{
		FAkAudioDevice::DelayAssetLoad(this);
	}
}

void UAkAudioType::Load()
{
	if (auto AudioDevice = FAkAudioDevice::Get())
	{
		auto idFromName = AudioDevice->GetIDFromString(GetName());
		if (ShortID == 0)
		{
			ShortID = idFromName;
		}
		else if (!IsA<UAkGroupValue>() && ShortID != 0 && ShortID != idFromName)
		{
			UE_LOG(LogAkAudio, Error, TEXT("%s - Current Short ID '%u' is different from ID from the name '%u'"), *GetName(), ShortID, idFromName);
		}
	}
}

#if WITH_EDITOR
void UAkAudioType::Reset()
{
	ShortID = 0;

	AsyncTask(ENamedThreads::GameThread, [this] {
		MarkPackageDirty();
	});
}
#endif
