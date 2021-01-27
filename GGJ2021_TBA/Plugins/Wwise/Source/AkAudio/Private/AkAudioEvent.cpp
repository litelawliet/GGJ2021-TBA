// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkEvent.cpp:
=============================================================================*/

#include "AkAudioEvent.h"

#include "AkAudioBank.h"
#include "AkAudioDevice.h"
#include "AkGroupValue.h"
#include "AkMediaAsset.h"
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "HAL/PlatformProperties.h"
#include "IntegrationBehavior/AkIntegrationBehavior.h"

#if WITH_EDITOR
#include "AssetTools/Public/AssetToolsModule.h"
#include "UnrealEd/Public/ObjectTools.h"
#endif

bool UAkAssetDataSwitchContainerData::IsMediaReady() const
{
	bool isMediaReady = true;
	for (auto child : Children)
	{
		if (!child->IsMediaReady())
		{
			isMediaReady = false;
			break;
		}
	}

	if (isMediaReady && streamHandle.IsValid())
	{
		isMediaReady = streamHandle->HasLoadCompleted();
	}

	return isMediaReady;
}

#if WITH_EDITOR
void UAkAssetDataSwitchContainerData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	internalGetMediaList(this, mediaList);
}

void UAkAssetDataSwitchContainerData::internalGetMediaList(const UAkAssetDataSwitchContainerData* data, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	for (auto& media : data->MediaList)
	{
		mediaList.AddUnique(media);
	}

	for (auto* child : data->Children)
	{
		internalGetMediaList(child, mediaList);
	}
}
#endif

void UAkAssetDataSwitchContainer::BeginDestroy()
{
	Super::BeginDestroy();

	if (auto* audioDevice = FAkAudioDevice::Get())
	{
		audioDevice->OnLoadSwitchValue.Remove(onLoadSwitchHandle);
		audioDevice->OnUnloadSwitchValue.Remove(onUnloadSwitchHandle);
	}
}

AKRESULT UAkAssetDataSwitchContainer::Load()
{
	auto result = Super::Load();

	if (result == AK_Success)
	{
		if (SwitchContainers.Num() > 0)
		{
			loadSwitchContainer(SwitchContainers);

			if (auto* audioDevice = FAkAudioDevice::Get())
			{
				audioDevice->OnLoadSwitchValue.AddUObject(this, &UAkAudioEventData::onLoadSwitchValue);
				audioDevice->OnUnloadSwitchValue.AddUObject(this, &UAkAudioEventData::onUnloadSwitchValue);
			}
		}
	}

	return result;
}

AKRESULT UAkAssetDataSwitchContainer::Unload()
{
	auto result = Super::Unload();

	if (result == AK_Success)
	{
		unloadSwitchContainerMedia(SwitchContainers);
	}

	return result;
}

bool UAkAssetDataSwitchContainer::IsMediaReady() const
{
	bool parentReady = Super::IsMediaReady();
	if (!parentReady)
	{
		return false;
	}

	for (auto* switchContainer : SwitchContainers)
	{
		if (switchContainer && !switchContainer->IsMediaReady())
		{
			return false;
		}
	}

	return true;
}

void UAkAssetDataSwitchContainer::loadSwitchContainer(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers)
{
	for (auto* container : switchContainers)
	{
		loadSwitchContainer(container);
	}
}

void UAkAssetDataSwitchContainer::loadSwitchContainer(UAkAssetDataSwitchContainerData* switchContainer)
{
	if (switchContainer && IsValid(switchContainer->GroupValue.Get()))
	{
		loadSwitchContainerMedia(switchContainer);

		loadSwitchContainer(switchContainer->Children);
	}
}

void UAkAssetDataSwitchContainer::loadSwitchContainerMedia(UAkAssetDataSwitchContainerData * switchContainer)
{
	if (switchContainer->MediaList.Num() > 0)
	{
		if (auto* AudioDevice = FAkAudioDevice::Get())
		{
			TArray<FSoftObjectPath> MediaToLoad;
			for (auto& media : switchContainer->MediaList)
			{
				MediaToLoad.AddUnique(media.ToSoftObjectPath());
			}

			switchContainer->streamHandle = AudioDevice->GetStreamableManager().RequestAsyncLoad(MediaToLoad);
		}
	}
}

void UAkAssetDataSwitchContainer::onLoadSwitchValue(const FSoftObjectPath& path)
{
	loadSwitchValue(path, SwitchContainers);
}

void UAkAssetDataSwitchContainer::loadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers)
{
	for (auto* switchContainer : switchContainers)
	{
		loadSwitchValue(path, switchContainer);
	}
}

void UAkAssetDataSwitchContainer::loadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer)
{
	if (switchContainer)
	{
		if (switchContainer->GroupValue.ToSoftObjectPath() == path)
		{
			if (!switchContainer->streamHandle.IsValid())
			{
				loadSwitchContainerMedia(switchContainer);
			}

			loadSwitchContainer(switchContainer->Children);
		}
		else if (IsValid(switchContainer->GroupValue.Get()))
		{
			loadSwitchValue(path, switchContainer->Children);
		}
	}
}

void UAkAssetDataSwitchContainer::onUnloadSwitchValue(const FSoftObjectPath& path)
{
	unloadSwitchValue(path, SwitchContainers);
}

void UAkAssetDataSwitchContainer::unloadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers)
{
	for (auto* switchContainer : switchContainers)
	{
		unloadSwitchValue(path, switchContainer);
	}
}

void UAkAssetDataSwitchContainer::unloadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer)
{
	if (switchContainer)
	{
		if (switchContainer->GroupValue.ToSoftObjectPath() == path)
		{
			unloadSwitchContainerMedia(switchContainer);
		}
		else
		{
			unloadSwitchValue(path, switchContainer->Children);
		}
	}
}

void UAkAssetDataSwitchContainer::unloadSwitchContainerMedia(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers)
{
	for (auto* container : switchContainers)
	{
		unloadSwitchContainerMedia(container);
	}
}

void UAkAssetDataSwitchContainer::unloadSwitchContainerMedia(UAkAssetDataSwitchContainerData* switchContainer)
{
	if (switchContainer)
	{
		if (switchContainer->streamHandle.IsValid())
		{
			switchContainer->streamHandle->CancelHandle();
			switchContainer->streamHandle.Reset();
		}

		unloadSwitchContainerMedia(switchContainer->Children);
	}
}

#if WITH_EDITOR
void UAkAssetDataSwitchContainer::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	Super::GetMediaList(mediaList);

	for (auto* switchContainer : SwitchContainers)
	{
		switchContainer->GetMediaList(mediaList);
	}
}
#endif

UAkAssetDataSwitchContainer* UAkAudioEventData::FindOrAddLocalizedData(const FString& language)
{
	FScopeLock autoLock(&LocalizedMediaLock);

	auto* localizedData = LocalizedMedia.Find(language);
	if (localizedData)
	{
		return *localizedData;
	}
	else
	{
		auto* newLocalizedData = NewObject<UAkAssetDataSwitchContainer>(this);
		LocalizedMedia.Add(language, newLocalizedData);
		return newLocalizedData;
	}
}

bool UAkAudioEventData::IsMediaReady() const
{
	bool parentReady = Super::IsMediaReady();
	if (!parentReady)
	{
		return false;
	}

	for (auto& entry : LocalizedMedia)
	{
		if (!entry.Value->IsMediaReady())
		{
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
void UAkAudioEventData::GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const
{
	Super::GetMediaList(mediaList);

	for (auto& entry : LocalizedMedia)
	{
		entry.Value->GetMediaList(mediaList);
	}
}
#endif

float UAkAudioEvent::GetMaxAttenuationRadius() const
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		return eventData->MaxAttenuationRadius;
	}

	return 0.f;
}

bool UAkAudioEvent::GetIsInfinite() const
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		return eventData->IsInfinite;
	}

	return false;
}

float UAkAudioEvent::GetMinimumDuration() const
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		return eventData->MinimumDuration;
	}

	return 0.f;
}

void UAkAudioEvent::SetMinimumDuration(float value)
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		eventData->MinimumDuration = value;
	}
}

float UAkAudioEvent::GetMaximumDuration() const
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		return eventData->MaximumDuration;
	}

	return 0.f;
}

void UAkAudioEvent::SetMaximumDuration(float value)
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		eventData->MaximumDuration = value;
	}
}

bool UAkAudioEvent::SwitchLanguage(const FString& newAudioCulture, const SwitchLanguageCompletedFunction& Function, FAkAudioDevice::SetCurrentAudioCultureAsyncTask* SetCultureTask)
{
	bool switchLanguage = false;

	TSoftObjectPtr<UAkAssetPlatformData>* eventDataSoftObjectPtr = LocalizedPlatformAssetDataMap.Find(newAudioCulture);

	if (eventDataSoftObjectPtr)
	{
		auto& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		auto assetData = assetRegistryModule.Get().GetAssetByObjectPath(*eventDataSoftObjectPtr->ToSoftObjectPath().ToString(), true);

		if (assetData.IsValid() && !assetData.PackagePath.IsNone())
		{
			switchLanguage = true;
		}
		else
		{
			if (auto* audioDevice = FAkAudioDevice::Get())
			{
				FString pathWithDefaultLanguage = eventDataSoftObjectPtr->ToSoftObjectPath().ToString().Replace(*newAudioCulture, *audioDevice->GetDefaultLanguage());
				assetData = assetRegistryModule.Get().GetAssetByObjectPath(FName(*pathWithDefaultLanguage), true);
				if (assetData.IsValid() && !assetData.PackagePath.IsNone())
				{
					switchLanguage = true;
				}
			}
		}
	}
	else
	{
		if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
		{
			if (eventData->LocalizedMedia.Contains(newAudioCulture))
			{
				switchLanguage = true;
			}
		}
	}

	if (switchLanguage)
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			if (audioDevice->IsEventIDActive(ShortID))
			{
				UE_LOG(LogAkAudio, Warning, TEXT("Stopping all instances of the \"%s\" event because it is being unloaded during a language change."), *GetName());
				audioDevice->StopEventID(ShortID);
			}
		}

		parentSetCultureTask = SetCultureTask;
		unloadLocalizedData();

		loadLocalizedData(newAudioCulture, Function);
	}

	return switchLanguage;
}

void UAkAudioEvent::Load()
{
	AkIntegrationBehavior::Get()->AkAudioEvent_Load(this);
}

void UAkAudioEvent::BeginDestroy()
{
	if (auto* audioDevice = FAkAudioDevice::Get())
	{
		if (audioDevice->IsEventIDActive(ShortID))
		{
			UE_LOG(LogAkAudio, Warning, TEXT("Stopping all instances of the \"%s\" event because it is being unloaded."), *GetName());
			audioDevice->StopEventID(ShortID);
		}
	}

	Super::BeginDestroy();
}

bool UAkAudioEvent::IsReadyForFinishDestroy()
{
	bool IsReady = true;
	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		IsReady = !AudioDevice->IsEventIDActive(ShortID);
		if (!IsReady)
		{
			// Give time for the sounds what were stopped in BeginDestroy to finish processing
			FPlatformProcess::Sleep(0.05);
			AudioDevice->Update(0.0);
		}
	}

	return IsReady;
}

void UAkAudioEvent::Unload()
{
	if (IsLocalized())
	{
		unloadLocalizedData();
	}
	else
	{
		Super::Unload();
	}
}

bool UAkAudioEvent::IsLocalized() const
{
	if (RequiredBank)
	{
		return false;
	}

	if (LocalizedPlatformAssetDataMap.Num() > 0)
	{
		return true;
	}

	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		if (eventData->LocalizedMedia.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool UAkAudioEvent::IsMediaReady() const
{
	if (AkUnrealHelper::IsUsingEventBased())
	{
		if (auto* assetData = getAssetData())
		{
			if (!assetData->IsMediaReady())
			{
				return false;
			}
		}

		if (localizedStreamHandle.IsValid())
		{
			return localizedStreamHandle->HasLoadCompleted();
		}
	}

	return true;
}

void UAkAudioEvent::PinInGarbageCollector(uint32 PlayingID)
{
	if (TimesPinnedToGC.GetValue() == 0)
	{
		AddToRoot();
	}
	TimesPinnedToGC.Increment();

	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		AudioDevice->AddToPinnedEventsMap(PlayingID, this);
	}
}

void UAkAudioEvent::UnpinFromGarbageCollector(uint32 PlayingID)
{
	TimesPinnedToGC.Decrement();
	if (TimesPinnedToGC.GetValue() == 0)
	{
		RemoveFromRoot();
	}
}

UAkAssetData* UAkAudioEvent::createAssetData(UObject* parent) const
{
	return NewObject<UAkAudioEventData>(parent);
}

UAkAssetData* UAkAudioEvent::getAssetData() const
{
#if WITH_EDITORONLY_DATA
	if (LocalizedPlatformAssetDataMap.Num() > 0 && CurrentLocalizedPlatformData)
	{
		const FString runningPlatformName(FPlatformProperties::IniPlatformName());

		if (auto platformEventData = CurrentLocalizedPlatformData->AssetDataPerPlatform.Find(runningPlatformName))
		{
			return *platformEventData;
		}
	}

	return Super::getAssetData();
#else
	if (LocalizedPlatformAssetDataMap.Num() > 0 && CurrentLocalizedPlatformData)
	{
		return CurrentLocalizedPlatformData->CurrentAssetData;
	}

	return Super::getAssetData();
#endif
}

void UAkAudioEvent::loadLocalizedData(const FString& audioCulture, const SwitchLanguageCompletedFunction& Function)
{
	if (auto* audioDevice = FAkAudioDevice::Get())
	{
		if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
		{
			if (eventData->LocalizedMedia.Num() > 0)
			{
				if (auto* localizedData = eventData->LocalizedMedia.Find(audioCulture))
				{
					(*localizedData)->Load();

					if (Function)
					{
						Function(true);
					}
					return;
				}
			}
		}

		TSoftObjectPtr<UAkAssetPlatformData>* eventDataSoftObjectPtr = LocalizedPlatformAssetDataMap.Find(audioCulture);
		if (eventDataSoftObjectPtr)
		{
			auto& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

			FSoftObjectPath localizedDataPath = eventDataSoftObjectPtr->ToSoftObjectPath();

			if (!assetRegistryModule.Get().GetAssetByObjectPath(*localizedDataPath.ToString(), true).IsValid())
			{
				FString pathWithDefaultLanguage = eventDataSoftObjectPtr->ToSoftObjectPath().ToString().Replace(*audioCulture, *audioDevice->GetDefaultLanguage());
				auto assetData = assetRegistryModule.Get().GetAssetByObjectPath(FName(*pathWithDefaultLanguage), true);
				if (assetRegistryModule.Get().GetAssetByObjectPath(FName(*pathWithDefaultLanguage), true).IsValid())
				{
					localizedDataPath = FSoftObjectPath(pathWithDefaultLanguage);
				}
			}

			TWeakObjectPtr<UAkAudioEvent> weakThis(this);
			localizedStreamHandle = audioDevice->GetStreamableManager().RequestAsyncLoad(localizedDataPath, [weakThis, Function] {
				if (weakThis.IsValid())
				{
					weakThis->onLocalizedDataLoaded();

					if (Function)
					{
						Function(weakThis->localizedStreamHandle.IsValid());
					}
				}
			});
		}
	}
}

void UAkAudioEvent::onLocalizedDataLoaded()
{
	if (localizedStreamHandle.IsValid())
	{
		CurrentLocalizedPlatformData = Cast<UAkAssetPlatformData>(localizedStreamHandle->GetLoadedAsset());

		Super::Load();
	}
}

void UAkAudioEvent::unloadLocalizedData()
{
	if (auto* eventData = Cast<UAkAudioEventData>(getAssetData()))
	{
		if (eventData->LocalizedMedia.Num() > 0)
		{
			if (auto* audioDevice = FAkAudioDevice::Get())
			{
				if (auto* localizedData = eventData->LocalizedMedia.Find(audioDevice->GetCurrentAudioCulture()))
				{
					(*localizedData)->Unload();
				}
			}
		}
		else
		{
			if (localizedStreamHandle.IsValid())
			{
				Super::Unload();

				CurrentLocalizedPlatformData = nullptr;

				localizedStreamHandle->CancelHandle();
				localizedStreamHandle.Reset();
			}
		}
	}
}

void UAkAudioEvent::superLoad()
{
	Super::Load();
}

#if WITH_EDITOR
UAkAssetData* UAkAudioEvent::FindOrAddAssetData(const FString& platform, const FString& language)
{
	check(IsInGameThread());

	UAkAssetPlatformData* eventData = nullptr;
	UObject* parent = this;

	if (language.Len() > 0)
	{
		auto* languageIt = LocalizedPlatformAssetDataMap.Find(language);
		if (languageIt)
		{
			eventData = languageIt->LoadSynchronous();

			if (eventData)
			{
				parent = eventData;
			}
			else
			{
				LocalizedPlatformAssetDataMap.Remove(language);
			}
		}

		if (!eventData)
		{
			auto& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			auto& assetToolModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			FSoftObjectPath objectPath(this);

			auto basePackagePath = AkUnrealHelper::GetBaseAssetPackagePath();

			auto packagePath = objectPath.GetLongPackageName();
			packagePath.RemoveFromStart(basePackagePath);
			packagePath.RemoveFromEnd(objectPath.GetAssetName());
			packagePath = ObjectTools::SanitizeObjectPath(FPaths::Combine(AkUnrealHelper::GetLocalizedAssetPackagePath(), language, packagePath));

			auto assetName = GetName();

			auto foundAssetData = assetRegistryModule.Get().GetAssetByObjectPath(*FPaths::Combine(packagePath, FString::Printf(TEXT("%s.%s"), *assetName, *assetName)));
			if (foundAssetData.IsValid())
			{
				eventData = Cast<UAkAssetPlatformData>(foundAssetData.GetAsset());
			}
			else
			{
				eventData = Cast<UAkAssetPlatformData>(assetToolModule.Get().CreateAsset(assetName, packagePath, UAkAssetPlatformData::StaticClass(), nullptr));
			}

			if (eventData)
			{
				parent = eventData;

				LocalizedPlatformAssetDataMap.Add(language, eventData);
			}
		}

		if (eventData)
		{
			return internalFindOrAddAssetData(eventData, platform, parent);
		}
	}

	return UAkAssetBase::FindOrAddAssetData(platform, language);
}

void UAkAudioEvent::Reset()
{
	LocalizedPlatformAssetDataMap.Empty();

	Super::Reset();
}

bool UAkAudioEvent::NeedsRebuild(const TSet<FString>& PlatformsToBuild, const TSet<FString>& LanguagesToBuild, const ISoundBankInfoCache* SoundBankInfoCache) const
{
	bool result = Super::NeedsRebuild(PlatformsToBuild, LanguagesToBuild, SoundBankInfoCache);

	if (!PlatformAssetData)
	{
		result = false;

		TArray<TSoftObjectPtr<UAkMediaAsset>> mediaList;
		GetMediaList(mediaList);

		for (auto& media : mediaList)
		{
			if (media.ToSoftObjectPath().IsValid() && !media.IsValid())
			{
				result = true;
			}
		}
	}

	if (!result)
	{
		TSet<FString> availableLanguages;
		for (auto& entry : LocalizedPlatformAssetDataMap)
		{
			availableLanguages.Add(entry.Key);

			if (LanguagesToBuild.Contains(entry.Key))
			{
				if (auto assetData = entry.Value.Get())
				{
					result |= assetData->NeedsRebuild(PlatformsToBuild, entry.Key, ID, SoundBankInfoCache);
				}
				else
				{
					result = true;
				}
			}
		}

		if (!PlatformAssetData && !availableLanguages.Includes(LanguagesToBuild))
		{
			result = true;
		}
	}

	return result;
}

void UAkAudioEvent::PreEditUndo() 
{
	UndoCompareBank = RequiredBank;
	Super::PreEditUndo();
}

void UAkAudioEvent::PostEditUndo()
{
	if (UndoCompareBank != RequiredBank) {
		UndoFlag = true;
	}
	Super::PostEditUndo();
	UndoFlag = false;
}

#if UE_4_25_OR_LATER
void UAkAudioEvent::PreEditChange(FProperty* PropertyAboutToChange)
#else
void UAkAudioEvent::PreEditChange(UProperty* PropertyAboutToChange)
#endif
{
	if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UAkAudioEvent, RequiredBank))
	{
		LastRequiredBank = RequiredBank;
	}
	Super::PreEditChange(PropertyAboutToChange);
}

void UAkAudioEvent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
 	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAkAudioEvent, RequiredBank))
		{
			UpdateRequiredBanks();
		}
	}
	else if (UndoFlag) {
		LastRequiredBank = UndoCompareBank;
		UpdateRequiredBanks();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
#endif

void UAkAudioEvent::UpdateRequiredBanks() {
	if (LastRequiredBank->IsValidLowLevel())
	{
		LastRequiredBank->RemoveAkAudioEvent(this);
	}

	if (RequiredBank->IsValidLowLevel())
	{
		RequiredBank->AddAkAudioEvent(this);
	}
}