#include "AkMediaAsset.h"

#include "AkAudioDevice.h"
#include "AkUnrealIOHook.h"
#include "HAL/PlatformProperties.h"
#include "Core/Public/Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Platforms/AkPlatformInfo.h"
#include "Interfaces/ITargetPlatform.h"
#endif


FAkMediaDataChunk::FAkMediaDataChunk() { }

#if WITH_EDITOR
FAkMediaDataChunk::FAkMediaDataChunk(IFileHandle* FileHandle, int64 BytesToRead, uint32 BulkDataFlags, FCriticalSection* DataWriteLock, bool IsPrefetch)
	: IsPrefetch(IsPrefetch)
{
	FScopeLock DataLock(DataWriteLock);
	Data.SetBulkDataFlags(BulkDataFlags);
	Data.Lock(EBulkDataLockFlags::LOCK_READ_WRITE);
	FileHandle->Read(reinterpret_cast<uint8*>(Data.Realloc(BytesToRead)), BytesToRead);
	Data.Unlock();
}
#endif

void FAkMediaDataChunk::Serialize(FArchive& Ar, UObject* Owner)
{
	Ar << IsPrefetch;
	Data.Serialize(Ar, Owner);
}

void UAkMediaAssetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	int32 numChunks = DataChunks.Num();
	Ar << numChunks;

	if (Ar.IsLoading())
	{
		DataChunks.Empty();
		for (int32 i = 0; i < numChunks; ++i)
		{
			DataChunks.Add(new FAkMediaDataChunk());
		}
	}

	for (int32 i = 0; i < numChunks; ++i)
	{
		DataChunks[i].Serialize(Ar, this);
	}
}

void UAkMediaAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.IsFilterEditorOnly())
	{
		if (Ar.IsSaving())
		{
			FString PlatformName = Ar.CookingTarget()->IniPlatformName();
			if (UAkPlatformInfo::UnrealNameToWwiseName.Contains(PlatformName))
			{
				PlatformName = *UAkPlatformInfo::UnrealNameToWwiseName.Find(PlatformName);
			}
			auto currentMediaData = MediaAssetDataPerPlatform.Find(*PlatformName);
			CurrentMediaAssetData = currentMediaData ? *currentMediaData : nullptr;
		}

		Ar << CurrentMediaAssetData;
	}
	else
	{
		Ar << MediaAssetDataPerPlatform;
	}
#else
	Ar << CurrentMediaAssetData;
#endif
}

void UAkMediaAsset::PostLoad()
{
	static FName AkAudioName = TEXT("AkAudio");
	Super::PostLoad();
	if (FModuleManager::Get().IsModuleLoaded(AkAudioName))
	{
		loadMedia();
	}
	else
	{
		FAkAudioDevice::DelayMediaLoad(this);
	}

}

void UAkMediaAsset::FinishDestroy()
{
	unloadMedia();
	Super::FinishDestroy();
}

#if WITH_EDITOR
#include "Async/Async.h"
void UAkMediaAsset::Reset()
{
	MediaAssetDataPerPlatform.Empty();
	MediaName.Empty();
	CurrentMediaAssetData = nullptr;
	AsyncTask(ENamedThreads::GameThread, [this] {
		MarkPackageDirty();
		});
}

UAkMediaAssetData* UAkMediaAsset::FindOrAddMediaAssetData(const FString& platform)
{
	auto platformData = MediaAssetDataPerPlatform.Find(platform);
	if (platformData)
	{
		return *platformData;
	}

	auto newPlatformData = NewObject<UAkMediaAssetData>(this);
	MediaAssetDataPerPlatform.Add(platform, newPlatformData);
	return newPlatformData;
}
#endif

void UAkMediaAsset::Load()
{
	loadMedia();
}

void UAkMediaAsset::Unload()
{
	unloadMedia();
}

FAkMediaDataChunk const* UAkMediaAsset::GetStreamedChunk() const
{
	auto mediaData = getMediaAssetData();
	if (!mediaData || mediaData->DataChunks.Num() <= 0)
	{
		return nullptr;
	}

	if (!mediaData->DataChunks[0].IsPrefetch)
	{
		return &mediaData->DataChunks[0];
	}

	if (mediaData->DataChunks.Num() >= 2)
	{
		return &mediaData->DataChunks[1];
	}

	return nullptr;
}

void UAkMediaAsset::loadMedia()
{
	auto assetData = getMediaAssetData();
	if (!assetData || assetData->DataChunks.Num() <= 0)
	{
		return;
	}

	auto& DataChunk = assetData->DataChunks[0];
	if (assetData->IsStreamed && !DataChunk.IsPrefetch)
	{
		FAkUnrealIOHook::AddStreamingMedia(this);
		return;
	}

#if !WITH_EDITOR
	if (DataChunk.Data.GetBulkDataSize() <= 0)
	{
		return;
	}
#endif

	auto audioDevice = FAkAudioDevice::Get();
	if (!audioDevice)
	{
		return;
	}

#if UE_4_25_OR_LATER
	FBulkDataIORequestCallBack DoSetMedia = [this, assetData](bool bWasCancelled, IBulkDataIORequest* ReadRequest)
#else
	FAsyncFileCallBack DoSetMedia = [this, assetData](bool bWasCancelled, IAsyncReadRequest* ReadRequest)
#endif
	{
		if (bWasCancelled)
		{
			UE_LOG(LogAkAudio, Error, TEXT("Bulk data streaming request for %s was cancelled. Media will be unavailable."), *GetName());
			return;
		}
		auto dataBulkSize = assetData->DataChunks[0].Data.GetBulkDataSize();
		AkSourceSettings sourceSettings
		{
			Id, reinterpret_cast<AkUInt8*>(RawMediaData), static_cast<AkUInt32>(dataBulkSize)
		};

#if AK_SUPPORT_DEVICE_MEMORY
		if (assetData->UseDeviceMemory)
		{
			MediaDataDeviceMemory = (AkUInt8*)AKPLATFORM::AllocDevice(dataBulkSize, 0);
			if (MediaDataDeviceMemory)
			{
				FMemory::Memcpy(MediaDataDeviceMemory, RawMediaData, dataBulkSize);
				sourceSettings.pMediaMemory = MediaDataDeviceMemory;
			}
			else
			{
				UE_LOG(LogAkAudio, Error, TEXT("Allocating device memory failed!"))
			}
		}
#endif

		auto akAudioDevice = FAkAudioDevice::Get();
		if (akAudioDevice->SetMedia(&sourceSettings, 1) != AK_Success)
		{
			UE_LOG(LogAkAudio, Log, TEXT("SetMedia failed for ID: %u"), Id);
		}

		if (assetData->IsStreamed)
		{
			FAkUnrealIOHook::AddStreamingMedia(this);
		}
	};

	auto dataBulkSize = DataChunk.Data.GetBulkDataSize();
#if WITH_EDITOR
	const void* bulkMediaData = DataChunk.Data.LockReadOnly();
	EditorMediaData.Reset(dataBulkSize);
	RawMediaData = EditorMediaData.GetData();
	FMemory::Memcpy(RawMediaData, bulkMediaData, dataBulkSize);
	DataChunk.Data.Unlock();
	DoSetMedia(false, nullptr);
#else
#if UE_4_24_OR_LATER
	if (DataChunk.Data.IsBulkDataLoaded())
#endif
	{
		RawMediaData = DataChunk.Data.Lock(LOCK_READ_ONLY);
		DoSetMedia(false, nullptr);
		DataChunk.Data.Unlock();
	}
#if UE_4_24_OR_LATER
	else
	{
		RawMediaData = FMemory::Malloc(dataBulkSize);
		DynamicallyAllocatedRawMediaData = true;
		DataChunk.Data.CreateStreamingRequest(EAsyncIOPriorityAndFlags::AIOP_High, &DoSetMedia, reinterpret_cast<uint8*>(RawMediaData));
	}
#endif
#endif
}

void UAkMediaAsset::unloadMedia()
{
	auto assetData = getMediaAssetData();
	auto mediaData = RawMediaData;
#if AK_SUPPORT_DEVICE_MEMORY
	if (MediaDataDeviceMemory)
	{
		mediaData = MediaDataDeviceMemory;
	}
#endif
	if (assetData && assetData->IsStreamed)
	{
		FAkUnrealIOHook::RemoveStreamingMedia(this);
	}

	if (assetData && mediaData && assetData->DataChunks.Num() > 0)
	{
		if (auto audioDevice = FAkAudioDevice::Get())
		{
			AkSourceSettings sourceSettings
			{
				Id, reinterpret_cast<AkUInt8*>(mediaData), static_cast<AkUInt32>(assetData->DataChunks[0].Data.GetBulkDataSize())
			};
			audioDevice->UnsetMedia(&sourceSettings, 1);
		}
#if !WITH_EDITOR
		if (DynamicallyAllocatedRawMediaData)
		{
			FMemory::Free(RawMediaData);
			DynamicallyAllocatedRawMediaData = false;
		}
#endif
		RawMediaData = nullptr;

#if AK_SUPPORT_DEVICE_MEMORY
		if (MediaDataDeviceMemory)
		{
			AKPLATFORM::FreeDevice(MediaDataDeviceMemory, assetData->DataChunks[0].Data.GetBulkDataSize(), 0, true);
			MediaDataDeviceMemory = nullptr;
		}
#endif
	}
}

UAkMediaAssetData* UAkMediaAsset::getMediaAssetData() const
{
#if! WITH_EDITORONLY_DATA
	return CurrentMediaAssetData;
#else
	const FString runningPlatformName(FPlatformProperties::IniPlatformName());
	if (auto platformMediaData = MediaAssetDataPerPlatform.Find(runningPlatformName))
	{
		return *platformMediaData;
	}

	return nullptr;
#endif
}

TTuple<void*, int64> UAkExternalMediaAsset::GetExternalSourceData()
{
	auto* mediaData = getMediaAssetData();

	if (mediaData && mediaData->DataChunks.Num() > 0)
	{
		loadMedia();
		auto result = MakeTuple(RawMediaData, mediaData->DataChunks[0].Data.GetBulkDataSize());
		return result;
	}

	return {};
}

void UAkExternalMediaAsset::AddPlayingID(uint32 EventID, uint32 PlayingID)
{
	auto& PlayingIDArray = ActiveEventToPlayingIDMap.FindOrAdd(EventID);
	PlayingIDArray.Add(PlayingID);
}

bool UAkExternalMediaAsset::HasActivePlayingIDs()
{
	AK::SoundEngine::RenderAudio();
	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		for (auto pair : ActiveEventToPlayingIDMap)
		{
			uint32 EventID = pair.Key;
			for (auto PlayingID : pair.Value)
			{
				if (AudioDevice->IsPlayingIDActive(EventID, PlayingID))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void UAkExternalMediaAsset::BeginDestroy()
{
	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		for (auto pair : ActiveEventToPlayingIDMap)
		{
			uint32 EventID = pair.Key;
			for (auto PlayingID : pair.Value)
			{
				if (AudioDevice->IsPlayingIDActive(EventID, PlayingID))
				{
					UE_LOG(LogAkAudio, Warning, TEXT("Stopping PlayingID %u because media file %s is being unloaded."), PlayingID, *GetName());
					AudioDevice->StopPlayingID(PlayingID);
				}
			}
		}

		AudioDevice->Update(0.0);
	}
	
	Super::BeginDestroy();
}


bool UAkExternalMediaAsset::IsReadyForFinishDestroy()
{
	bool IsReady = true;
	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		IsReady = !HasActivePlayingIDs();
		if (!IsReady)
		{
			// Give time for the sounds what were stopped in BeginDestroy to finish processing
			FPlatformProcess::Sleep(0.05);
			AudioDevice->Update(0.0);
		}
	}

	return IsReady;
}

void UAkExternalMediaAsset::PinInGarbageCollector(uint32 PlayingID)
{
	if (TimesPinnedToGC.GetValue() == 0)
	{
		AddToRoot();
	}
	TimesPinnedToGC.Increment();

	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		AudioDevice->AddToPinnedMediasMap(PlayingID, this);
	}
}

void UAkExternalMediaAsset::UnpinFromGarbageCollector(uint32 PlayingID)
{
	TimesPinnedToGC.Decrement();
	if (TimesPinnedToGC.GetValue() == 0)
	{
		RemoveFromRoot();
	}
}
