#pragma once

#include "AssetData.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "WwiseProject/WwiseProjectInfo.h"
#include "AssetManagement/AkAssetDatabase.h"

class FAssetRegistryModule;
class FAssetToolsModule;
class FJsonObject;
class IPlatformFile;
class ISoundBankInfoCache;
class SNotificationItem;
class UAkAssetData;
class UAkAssetDataSwitchContainer;
class UAkAssetDataSwitchContainerData;
class UAkAssetDataWithMedia;
class UAkAudioBank;
class UAkAudioEvent;
class UAkAudioEventData;
class UAkAuxBus;
class UAkAuxBusPlatformData;
class UAkGroupValue;
class UAkInitBank;
class UAkInitBankPlatformData;
class UAkMediaAsset;
class UAkMediaAssetData;
class UAkRtpc;
class UAkTrigger;
class UPackage;

struct MediaAssetPerPlatformData
{
	bool IsStreamed = false;
	bool UseDeviceMemory = false;
	uint32 PrefetchSize = 0;
};

struct MediaAssetToCook
{
	FSoftObjectPath AssetPath;
	uint32 Id = 0;
	FString Language;
	FString CachePath;
	FString MediaName;
	TMap<FString, MediaAssetPerPlatformData> ParsedPerPlatformData;
	UAkMediaAsset* Instance = nullptr;
	UAkMediaAssetData* CurrentPlatformData = nullptr;
};

struct AutoSetIsBuilding
{
	AutoSetIsBuilding();
	~AutoSetIsBuilding();
};

struct AudioBankInfoEntry
{
	TSet<FString> Events;
	TSet<FString> AuxBusses;
	bool NeedsRebuild = false;
};

using MediaToCookMap = TMap<uint32, TSharedPtr<MediaAssetToCook>>;
using AudioBankInfoMap = TMap<FString, AudioBankInfoEntry>;

DECLARE_STATS_GROUP(TEXT("AkSoundDataBuilder"), STATGROUP_AkSoundDataSource, STATCAT_Audiokinetic);

class AkSoundDataBuilder : public TSharedFromThis<AkSoundDataBuilder, ESPMode::ThreadSafe>
{
public:
	struct InitParameters
	{
		TArray<FString> Platforms;
		TArray<FWwiseLanguageInfo> Languages;
		TSet<FString> BanksToGenerate;
		bool SkipLanguages = false;
	};

	AkSoundDataBuilder(const InitParameters& InitParameter);
	virtual ~AkSoundDataBuilder();

	virtual void Init();

	virtual void DoWork() = 0;

	friend class AkEventBasedToolBehavior;
	friend class AkLegacyToolBehavior;

protected:
	void createNotificationItem();
	void notifyGenerationFailed();
	void notifyCookingData();
	void notifyGenerationSucceeded();
	void notifyProfilingInProgress();

	template<typename MainAsset, typename PlatformAsset>
	bool parseSoundBankInfo(MainAsset* mainAsset, PlatformAsset* platformAsset, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankJson, bool isInitBank)
	{
		bool changed = false;

		MediaToCookMap& mediaToCookMap = getMediaToCookMap(platform);

		if (auto* mediaPlatformData = Cast<UAkAssetDataWithMedia>(platformAsset))
		{
			parseMedia(soundBankJson, mediaToCookMap, mediaPlatformData->MediaList, platform, isInitBank);
		}

		changed |= parseAssetInfo(mainAsset, platformAsset, platform, language, soundBankJson, mediaToCookMap);

		parseGameSyncs(soundBankJson);
		changed |= parseBankHash(platformAsset, soundBankJson);

		return changed;
	}

	bool processMediaEntry(MediaToCookMap& mediaToCookMap, const FString& platform, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList, const TSharedPtr<FJsonObject>& mediaData, bool isStreamed, bool isInitBank);

	bool parseMedia(const TSharedPtr<FJsonObject>& soundBankJson, MediaToCookMap& mediaToCookMap, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList, const FString& platform, bool isInitBank);
	void parseGameSyncs(const TSharedPtr<FJsonObject>& soundBankJson);
	bool parseBankHash(UAkAssetData* bankData, const TSharedPtr<FJsonObject>& soundBankJson);

	bool MediaListsAreDifferent(const TArray<TSoftObjectPtr<UAkMediaAsset>>& OldList, const TArray<TSoftObjectPtr<UAkMediaAsset>>& NewList);
	bool DidSwitchContainerDataChange(const TArray<UAkAssetDataSwitchContainerData*>& OldData, const TArray<UAkAssetDataSwitchContainerData*>& NewData);
	bool ParseMediaAndSwitchContainers(TSharedPtr<FJsonObject> eventJson, UAkAssetDataSwitchContainer* AssetData, const FString& platform, MediaToCookMap& mediaToCookMap);

	template<typename AssetType>
	bool ParseEventMetadataSection(TSharedPtr<FJsonObject> eventJson, const FString& SectionName, const AkAssetDatabase::AkTypeMap<AssetType>* AssetMap, TSet<AssetType*>* DestinationSet);

	bool parseAssetInfo(UAkAudioEvent* akEvent, UAkAssetData* eventPlatformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap);
	bool parseAssetInfo(UAkAuxBus* akAuxBus, UAkAssetData* auxBusPlatformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap);
	bool GetAuxBusFromBankInfo(const TSharedPtr<FJsonObject>& soundBankData);
	bool parseAssetInfo(UAkInitBank* akInitBank, UAkAssetData* initBankPlatformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap);
	bool parseAssetInfo(UAkAudioBank* akAudioBank, UAkAssetData* eventGroupPlatformData, const FString& platform, const FString& language, const TSharedPtr<FJsonObject>& soundBankData, MediaToCookMap& mediaToCookMap);

	bool parseEventInfo(UAkAudioEvent* akEvent, UAkAudioEventData* eventPlatformData, const TSharedPtr<FJsonObject>& eventJson);

	void parseGroupValue(const TSharedPtr<FJsonObject>& groupValueObject, uint32 groupId);

	void parseSwitchContainer(const TSharedPtr<FJsonObject>& switchContainerJson, UAkAssetDataSwitchContainerData* switchContainerEntry, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList, UObject* parent);

	void parseGameParameter(const TSharedPtr<FJsonObject>& gameParameterObject);
	void parseTrigger(const TSharedPtr<FJsonObject>& triggerObject);

	bool parsePluginInfo(UAkInitBank* initBank, const FString& platform, const TSharedPtr<FJsonObject>& pluginInfoJson);

	void cookMediaAsset(const MediaAssetToCook& assetToCook, const FString& platform);

	void dispatchAndWaitMediaCookTasks();

	void markAssetDirty(UObject* obj);

	enum class FillAudioBankInfoKind
	{
		GUID,
		AssetName
	};
	FString fillAudioBankInfoMap(AudioBankInfoMap& AudioBankInfoMap, FillAudioBankInfoKind InfoKind, const TSet<FString>& PlatformsToBuild, const TSet<FString>& LanguagesToBuild, ISoundBankInfoCache* SoundBankInfoCache);

	bool fillBankDataInAsset(UAkAssetData* AssetData, const TArray<uint8>& NewBankData, FCriticalSection* DataLock);

private:
	void dispatchMediaCookTask(const MediaToCookMap& media, const FString& platform);

	MediaToCookMap& getMediaToCookMap(const FString& Platform)
	{
		FScopeLock autoLock(&mediaLock);
		return mediaToCookPerPlatform.FindOrAdd(Platform);
	}

protected:
	FAssetRegistryModule* assetRegistryModule = nullptr;
	FAssetToolsModule* assetToolsModule = nullptr;

	FString basePackagePath;
	FString localizedPackagePath;
	FString cacheDirectory;
	FString defaultLanguage;

	InitParameters initParameters;

	bool splitSwitchContainerMedia = false;
	bool splitMediaPerFolder = false;

	TSharedPtr<SNotificationItem> notificationItem;

	IPlatformFile* platformFile = nullptr;

	WwiseProjectInfo wwiseProjectInfo;

private:
	mutable FCriticalSection mediaLock;
	TMap<FString, MediaToCookMap> mediaToCookPerPlatform;
	TMap<uint32, FSoftObjectPath> mediaIdToAssetPath;
	FGraphEventArray allMediaTasks;
};
