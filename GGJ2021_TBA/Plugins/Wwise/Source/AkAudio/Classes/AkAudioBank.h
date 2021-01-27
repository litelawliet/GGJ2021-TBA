// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AkAssetBase.h"
#include "AkAudioBank.generated.h"

UCLASS(meta=(BlueprintSpawnableComponent))
class AKAUDIO_API UAkAudioBank : public UAkAssetBase
{
	GENERATED_BODY()

public:
	// Only applicable when you don't use the new Event-Based Packaging workflow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool AutoLoad = true;

	UPROPERTY(VisibleAnywhere, Category = "AkAudioBank")
	TMap<FString, TSoftObjectPtr<UAkAssetPlatformData>> LocalizedPlatformAssetDataMap;


	UPROPERTY(VisibleAnywhere, Category = "AkAudioBank")
	TSet<TSoftObjectPtr<UAkAudioEvent>> LinkedAkEvents;

private:
	UPROPERTY(Transient)
	UAkAssetPlatformData* CurrentLocalizedPlatformAssetData = nullptr;

public:
	void Load() override;
	void Unload() override;

	bool IsLocalized() const { return LocalizedPlatformAssetDataMap.Num() > 0; }

	bool SwitchLanguage(const FString& newAudioCulture, const SwitchLanguageCompletedFunction& Function = SwitchLanguageCompletedFunction());
	
	void AddAkAudioEvent(UAkAudioEvent* event);
	void RemoveAkAudioEvent(UAkAudioEvent* event);

#if CPP
	/**
	 * Loads an AkBank.
	 *
	 * @return Returns true if the load was successful, otherwise false
	 */
	bool LegacyLoad();

	/**
	* Loads an AkBank, using the latent action to flag completion.
	*/
	bool LegacyLoad(FWaitEndBankAction* LoadBankLatentAction);

	/**
	 * Loads an AkBank asynchronously.
	 *
	 * @param in_pfnBankCallback		Function to call on completion
	 * @param in_pCookie				Cookie to pass in callback
	 * @return Returns true if the load was successful, otherwise false
	 */
	bool LegacyLoadAsync(void* in_pfnBankCallback, void* in_pCookie);

	/**
	* Loads an AkBank asynchronously, from Blueprint
	*
	* @param BankLoadedCallback		Blueprint Delegate to call on completion
	* @return Returns true if the load was successful, otherwise false
	*/
	bool LegacyLoadAsync(const FOnAkBankCallback& BankLoadedCallback);

	/**
	 * Unloads an AkBank.
	 */
	void LegacyUnload();

	/**
	* Unloads an AkBank, using the latent action to flag completion.
	*/
	void LegacyUnload(FWaitEndBankAction* LoadBankLatentAction);

	/**
	 * Unloads an AkBank asynchronously.
	 *
	 * @param in_pfnBankCallback		Function to call on completion
	 * @param in_pCookie				Cookie to pass in callback
	 */
	void LegacyUnloadAsync(void* in_pfnBankCallback, void* in_pCookie);

	/**
	* Unloads an AkBank asynchronously, from Blueprint
	*
	* @param BankUnloadedCallback		Blueprint Delegate to call on completion
	*/
	void LegacyUnloadAsync(const FOnAkBankCallback& BankUnloadedCallback);
#endif

#if WITH_EDITOR
	UAkAssetData* FindOrAddAssetData(const FString& platform, const FString& language) override;

	void Reset() override;

	bool NeedsRebuild(const TSet<FString>& PlatformsToBuild, const TSet<FString>& LanguagesToBuild, const ISoundBankInfoCache* SoundBankInfoCache) const override;
#endif

	friend class AkEventBasedIntegrationBehavior;

public:
	FString LoadedBankName;

protected:
	UAkAssetData* createAssetData(UObject* parent) const override;
	UAkAssetData* getAssetData() const override;

private:
	void PostLoad() override;

	void loadLocalizedData(const FString& audioCulture, const SwitchLanguageCompletedFunction& Function);
	void unloadLocalizedData();
	void onLocalizedDataLoaded();

	void superLoad();
	void superUnload();

	void populateAkAudioEvents();

private:
	TSharedPtr<FStreamableHandle> localizedStreamHandle;
};
