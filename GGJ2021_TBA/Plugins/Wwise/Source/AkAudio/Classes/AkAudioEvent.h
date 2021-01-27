// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AkAssetBase.h"
#include "Serialization/BulkData.h"
#include "AkAudioEvent.generated.h"

class UAkGroupValue;
class UAkMediaAsset;
class UAkAudioBank;
class UAkAuxBus;
class UAkTrigger;
struct FStreamableHandle;

UCLASS(EditInlineNew)
class AKAUDIO_API UAkAssetDataSwitchContainerData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TSoftObjectPtr<UAkGroupValue> GroupValue;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	UAkGroupValue* DefaultGroupValue;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<TSoftObjectPtr<UAkMediaAsset>> MediaList;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<UAkAssetDataSwitchContainerData*> Children;

	friend class UAkAssetDataSwitchContainer;

public:
	bool IsMediaReady() const;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;
#endif

private:
	void internalGetMediaList(const UAkAssetDataSwitchContainerData* data, TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const;

private:
	TSharedPtr<FStreamableHandle> streamHandle;
};

UCLASS()
class AKAUDIO_API UAkAssetDataSwitchContainer : public UAkAssetDataWithMedia
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TArray<UAkAssetDataSwitchContainerData*> SwitchContainers;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	UAkGroupValue* DefaultGroupValue;

public:
	void BeginDestroy() override;

	AKRESULT Load() override;
	AKRESULT Unload() override;

	bool IsMediaReady() const override;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const override;
#endif

private:
	void loadSwitchContainer(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void loadSwitchContainer(UAkAssetDataSwitchContainerData* switchContainer);

	void onLoadSwitchValue(const FSoftObjectPath& path);
	void loadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void loadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer);

	void onUnloadSwitchValue(const FSoftObjectPath& path);
	void unloadSwitchValue(const FSoftObjectPath& path, const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void unloadSwitchValue(const FSoftObjectPath& path, UAkAssetDataSwitchContainerData* switchContainer);

	void loadSwitchContainerMedia(UAkAssetDataSwitchContainerData* switchContainer);

	void unloadSwitchContainerMedia(const TArray<UAkAssetDataSwitchContainerData*>& switchContainers);
	void unloadSwitchContainerMedia(UAkAssetDataSwitchContainerData* switchContainer);

private:
	FDelegateHandle onLoadSwitchHandle;
	FDelegateHandle onUnloadSwitchHandle;
};

UCLASS()
class AKAUDIO_API UAkAudioEventData : public UAkAssetDataSwitchContainer
{
	GENERATED_BODY()

public:
	/** Maximum attenuation radius for this event */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MaxAttenuationRadius;

	/** Whether this event is infinite (looping) or finite (duration parameters are valid) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	bool IsInfinite;

	/** Minimum duration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MinimumDuration;

	/** Maximum duration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	float MaximumDuration;

	// This map is used when the event is part of a AkAudioBank
	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TMap<FString, UAkAssetDataSwitchContainer*> LocalizedMedia;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	TSet<UAkAudioEvent*> PostedEvents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	TSet<UAkAuxBus*> UserDefinedSends;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	TSet<UAkTrigger*> PostedTriggers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AkAudioEvent")
	TSet<UAkGroupValue*> GroupValues;

public:
	UAkAssetDataSwitchContainer* FindOrAddLocalizedData(const FString& language);

	bool IsMediaReady() const override;

#if WITH_EDITOR
	void GetMediaList(TArray<TSoftObjectPtr<UAkMediaAsset>>& mediaList) const override;
#endif

private:
	mutable FCriticalSection LocalizedMediaLock;
};

UCLASS(BlueprintType)
class AKAUDIO_API UAkAudioEvent : public UAkAssetBase
{
	GENERATED_BODY()

public:
	/** Maximum attenuation radius for this event */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float GetMaxAttenuationRadius() const;

	/** Whether this event is infinite (looping) or finite (duration parameters are valid) */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	bool GetIsInfinite() const;

	/** Minimum duration */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float GetMinimumDuration() const;
	void SetMinimumDuration(float value);

	/** Maximum duration */
	UFUNCTION(BlueprintGetter, Category = "AkAudioEvent")
	float GetMaximumDuration() const;
	void SetMaximumDuration(float value);

	UPROPERTY(VisibleAnywhere, Category = "AkAudioEvent")
	TMap<FString, TSoftObjectPtr<UAkAssetPlatformData>> LocalizedPlatformAssetDataMap;

	UPROPERTY(EditAnywhere, Category = "AkAudioEvent")
	UAkAudioBank* RequiredBank = nullptr;
	UAkAudioBank* LastRequiredBank = nullptr;
		
	void UpdateRequiredBanks();

#if WITH_EDITOR
	UAkAudioBank* UndoCompareBank = nullptr;
	void PostEditUndo() override;
	void PreEditUndo() override;
#if UE_4_25_OR_LATER
	void PreEditChange(FProperty* PropertyAboutToChange) override;
#else
	void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	UAkAssetPlatformData* CurrentLocalizedPlatformData = nullptr;

	/** Maximum attenuation radius for this event */
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetMaxAttenuationRadius, Category = "AkAudioEvent")
	float MaxAttenuationRadius;

	/** Whether this event is infinite (looping) or finite (duration parameters are valid) */
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetIsInfinite, Category = "AkAudioEvent")
	bool IsInfinite;

	/** Minimum duration */
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetMinimumDuration, Category = "AkAudioEvent")
	float MinimumDuration;

	/** Maximum duration */
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetMaximumDuration, Category = "AkAudioEvent")
	float MaximumDuration;

public:
	void Load() override;
	void Unload() override;
	void BeginDestroy() override;
	bool IsReadyForFinishDestroy() override;
	bool IsLocalized() const;

	bool SwitchLanguage(const FString& newAudioCulture, const SwitchLanguageCompletedFunction& Function = SwitchLanguageCompletedFunction(), FAkAudioDevice::SetCurrentAudioCultureAsyncTask* SetCultureTask = nullptr);

	bool IsMediaReady() const;
	void PinInGarbageCollector(uint32 PlayingID);
	void UnpinFromGarbageCollector(uint32 PlayingID);

#if WITH_EDITOR
	UAkAssetData* FindOrAddAssetData(const FString& platform, const FString& language) override;
	void Reset() override;
	bool NeedsRebuild(const TSet<FString>& PlatformsToBuild, const TSet<FString>& LanguagesToBuild, const ISoundBankInfoCache* SoundBankInfoCache) const override;
	bool UndoFlag = false;
#endif

	friend class AkEventBasedIntegrationBehavior;

protected:
	UAkAssetData* createAssetData(UObject* parent) const override;
	UAkAssetData* getAssetData() const override;

private:
	void loadLocalizedData(const FString& audioCulture, const SwitchLanguageCompletedFunction& Function);
	void unloadLocalizedData();
	void onLocalizedDataLoaded();
	void superLoad();

private:
	TSharedPtr<FStreamableHandle> localizedStreamHandle;

	FThreadSafeCounter TimesPinnedToGC;
};
