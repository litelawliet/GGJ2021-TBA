#include "AkAssetBase.h"

#include "Async/Async.h"
#include "AkMediaAsset.h"
#include "AkUnrealHelper.h"
#include "Platforms/AkPlatformInfo.h"
#include "IntegrationBehavior/AkIntegrationBehavior.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"

#if WITH_EDITOR
#include "TargetPlatform/Public/Interfaces/ITargetPlatform.h"
#include "ISoundBankInfoCache.h"
#endif

void UAkAssetData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Data.Serialize(Ar, this);
}

AKRESULT UAkAssetData::Load()
{
	return AkIntegrationBehavior::Get()->AkAssetData_Load(this);
}

AKRESULT UAkAssetData::Unload()
{
	if (BankID == AK_INVALID_BANK_ID)
		return Data.GetBulkDataSize() == 0 ? AK_Success : AK_Fail;

	if (auto AudioDevice = FAkAudioDevice::Get())
	{
		// DO NOT USE ASYNC if bank has media in it. Wwise needs access to the bank pointer in order to stop all playing sounds
		// contained within the bank. Depending on the timing, this can take up to an audio frame to process, so the memory
		// needs to remain available to the SoundEngine during that time. Since we are currently destroying the UObject, we 
		// can't guarantee that the memory will remain available that long.
		// In our case, though, the SoundBank does NOT contain media, so it is safe to unload the bank in a "fire and forget"
		// fashion.
		AudioDevice->UnloadBankFromMemoryAsync(BankID, RawData, [](AkUInt32 in_bankID, const void* in_pInMemoryBankPtr, AKRESULT in_eLoadResult, void* in_pCookie) {}, nullptr);
	}

	BankID = AK_INVALID_BANK_ID;
	RawData = nullptr;
	return AK_Success;
}

bool UAkAssetData::IsMediaReady() const
{
	return true;
}

#if WITH_EDITOR
void UAkAssetData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaList) const
{
}
#endif

AKRESULT UAkAssetDataWithMedia::Load()
{
	auto result = Super::Load();
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return result;

	if (result != AK_Success)
		return result;

	if (MediaList.Num() <= 0)
		return result;

	TArray<FSoftObjectPath> MediaToLoad;
	for (auto& media : MediaList)
		MediaToLoad.AddUnique(media.ToSoftObjectPath());


	if (parentSetCultureTask)
	{
		parentSetCultureTask->TasksRemaining.Increment(); // increment first to protect against the cases where the RequestAsyncLoad call can be seen as "synchronous"
	}

	TWeakObjectPtr< UAkAssetDataWithMedia > weakThis(this);
	mediaStreamHandle = AudioDevice->GetStreamableManager().RequestAsyncLoad(MediaToLoad, [weakThis]() {
		if (weakThis.IsValid() && weakThis->parentSetCultureTask)
		{
			weakThis->parentSetCultureTask->TasksRemaining.Decrement();
			weakThis->parentSetCultureTask = nullptr;
		}
	});

	if (!mediaStreamHandle && parentSetCultureTask)
	{
		parentSetCultureTask->TasksRemaining.Decrement();
		parentSetCultureTask = nullptr;
	}
	return result;
}

AKRESULT UAkAssetDataWithMedia::Unload()
{
	auto result = Super::Unload();
	if (result != AK_Success)
		return result;

	if (!mediaStreamHandle.IsValid())
		return result;

	mediaStreamHandle->CancelHandle();
	mediaStreamHandle.Reset();
	return result;
}

bool UAkAssetDataWithMedia::IsMediaReady() const
{
	if (mediaStreamHandle.IsValid())
	{
		return mediaStreamHandle->HasLoadCompleted();
	}

	return true;
}

#if WITH_EDITOR
void UAkAssetDataWithMedia::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaListArray) const
{
	Super::GetMediaList(MediaListArray);

	for (auto& media : MediaList)
	{
		MediaListArray.AddUnique(media);
	}
}
#endif

void UAkAssetPlatformData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << AssetDataPerPlatform;
		return;
	}

	if (Ar.IsSaving())
	{

		FString PlatformName = Ar.CookingTarget()->IniPlatformName();
		if (UAkPlatformInfo::UnrealNameToWwiseName.Contains(PlatformName))
		{
			PlatformName = *UAkPlatformInfo::UnrealNameToWwiseName.Find(PlatformName);
		}
		auto cookedAssetData = AssetDataPerPlatform.Find(PlatformName);
		CurrentAssetData = cookedAssetData ? *cookedAssetData : nullptr;
	}
#endif

	Ar << CurrentAssetData;
}

#if WITH_EDITOR
void UAkAssetPlatformData::Reset()
{
	AssetDataPerPlatform.Reset();
}

bool UAkAssetPlatformData::NeedsRebuild(const TSet<FString>& PlatformsToBuild, const FString& Language, const FGuid& ID, const ISoundBankInfoCache* SoundBankInfoCache) const
{
	TSet<FString> avaiablePlatforms;

	for (auto& entry : AssetDataPerPlatform)
	{
		avaiablePlatforms.Add(entry.Key);

		if (PlatformsToBuild.Contains(entry.Key))
		{
			if (!SoundBankInfoCache->IsSoundBankUpToUpdate(ID, entry.Key, Language, entry.Value->CachedHash))
			{
				return true;
			}
		}
	}

	if (!avaiablePlatforms.Includes(PlatformsToBuild))
	{
		return true;
	}

	return false;
}

void UAkAssetPlatformData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaList) const
{
	for (auto& entry : AssetDataPerPlatform)
	{
		entry.Value->GetMediaList(MediaList);
	}
}
#endif

void UAkAssetBase::FinishDestroy()
{
	Unload();
	Super::FinishDestroy();
}

void UAkAssetBase::Load()
{
	Super::Load();
	if (!bDelayLoadAssetMedia)
	{
		if (auto assetData = getAssetData())
		{
			assetData->parentSetCultureTask = parentSetCultureTask;
			assetData->Load();
		}
	}
}

void UAkAssetBase::Unload()
{
	if (auto assetData = getAssetData())
	{
		assetData->Unload();
	}
}

UAkAssetData* UAkAssetBase::getAssetData() const
{
	if (!PlatformAssetData)
		return nullptr;

#if WITH_EDITORONLY_DATA
	if (auto assetData = PlatformAssetData->AssetDataPerPlatform.Find(FPlatformProperties::IniPlatformName()))
		return *assetData;

	return nullptr;
#else
	return PlatformAssetData->CurrentAssetData;
#endif
}

UAkAssetData* UAkAssetBase::createAssetData(UObject* Parent) const
{
	return NewObject<UAkAssetData>(Parent);
}

#if WITH_EDITOR
UAkAssetData* UAkAssetBase::FindOrAddAssetData(const FString& Platform, const FString& Language)
{
	FScopeLock autoLock(&assetDataLock);

	if (!PlatformAssetData)
	{
		PlatformAssetData = NewObject<UAkAssetPlatformData>(this);
	}

	return internalFindOrAddAssetData(PlatformAssetData, Platform, this);
}

UAkAssetData* UAkAssetBase::internalFindOrAddAssetData(UAkAssetPlatformData* Data, const FString& Platform, UObject* Parent)
{
	auto assetData = Data->AssetDataPerPlatform.Find(Platform);
	if (assetData)
		return *assetData;

	auto newAssetData = createAssetData(Parent);
	Data->AssetDataPerPlatform.Add(Platform, newAssetData);
	return newAssetData;
}

void UAkAssetBase::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaList) const
{
	if (PlatformAssetData)
	{
		PlatformAssetData->GetMediaList(MediaList);
	}
}

bool UAkAssetBase::NeedsRebuild(const TSet<FString>& PlatformsToBuild, const TSet<FString>& LanguagesToBuild, const ISoundBankInfoCache* SoundBankInfoCache) const
{
	bool needsRebuild = false;

	if (PlatformAssetData)
	{
		needsRebuild = PlatformAssetData->NeedsRebuild(PlatformsToBuild, FString(), ID, SoundBankInfoCache);
	}
	else
	{
		needsRebuild = true;
	}

	TArray<TSoftObjectPtr<UAkMediaAsset>> mediaList;
	GetMediaList(mediaList);

	for (auto& media : mediaList)
	{
		if (media.ToSoftObjectPath().IsValid() && !media.IsValid())
		{
			needsRebuild = true;
		}
	}

	return needsRebuild;
}

void UAkAssetBase::Reset()
{
	PlatformAssetData = nullptr;

	Super::Reset();
}
#endif
