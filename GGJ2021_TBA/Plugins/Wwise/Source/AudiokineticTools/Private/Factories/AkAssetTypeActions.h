// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AssetTypeActions_Base.h"

#include "AkAcousticTexture.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"
#include "AkMediaAsset.h"
#include "AkTrigger.h"
#include "AkRtpc.h"

template<typename AkAssetType>
class FAkAssetTypeActions_Base : public FAssetTypeActions_Base
{
public:
	FAkAssetTypeActions_Base(EAssetTypeCategories::Type InAssetCategory) : MyAssetCategory(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual UClass* GetSupportedClass() const override { return AkAssetType::StaticClass(); }
	virtual uint32 GetCategories() override { return MyAssetCategory; }
	virtual bool ShouldForceWorldCentric() { return true; }

private:
	EAssetTypeCategories::Type MyAssetCategory;
};

class FAssetTypeActions_AkAcousticTexture : public FAkAssetTypeActions_Base<UAkAcousticTexture>
{
public:
	FAssetTypeActions_AkAcousticTexture(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkAcousticTexture", "Audiokinetic Texture"); }
	virtual FColor GetTypeColor() const override { return FColor(1, 185, 251); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>());
};


class FAssetTypeActions_AkAudioBank : public FAkAssetTypeActions_Base<UAkAudioBank>
{
public:
	FAssetTypeActions_AkAudioBank(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkAudioBank", "Audiokinetic Bank"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 192, 128); }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>());
#if UE_4_24_OR_LATER
	bool AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
#else
	virtual void AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType);
#endif

private:
	void CreateGenerateSoundDataWindow(TArray<TWeakObjectPtr<UAkAudioBank>> Objects);
	void RefreshAllBanks(TArray<TWeakObjectPtr<UAkAudioBank>> Objects);
};


class FAssetTypeActions_AkAudioEvent : public FAkAssetTypeActions_Base<UAkAudioEvent>
{
public:
	FAssetTypeActions_AkAudioEvent(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkAudioEvent", "Audiokinetic Event"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 128, 192); }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>());
#if UE_4_24_OR_LATER
	bool AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
#else
	virtual void AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType);
#endif

private:
	void PlayEvent(TArray<TWeakObjectPtr<UAkAudioEvent>> Objects);
	void StopEvent(TArray<TWeakObjectPtr<UAkAudioEvent>> Objects);
	void GroupIntoSoundBank(TArray<TWeakObjectPtr<UAkAudioEvent>> Objects);
}; 


class FAssetTypeActions_AkAuxBus : public FAkAssetTypeActions_Base<UAkAuxBus>
{
public:
	FAssetTypeActions_AkAuxBus(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkAuxBus", "Audiokinetic Auxiliary Bus"); }
	virtual FColor GetTypeColor() const override { return FColor(192, 128, 0); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() );

private:
	void GroupIntoSoundBank(TArray<TWeakObjectPtr<UAkAuxBus>> Objects);
};

class FAssetTypeActions_AkMediaAsset : public FAkAssetTypeActions_Base<UAkMediaAsset>
{
public:
	FAssetTypeActions_AkMediaAsset(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkMediaAsset", "Audiokinetic Media Asset"); }
	virtual FColor GetTypeColor() const override { return FColor(128, 150, 128); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
};

class FAssetTypeActions_AkRtpc : public FAkAssetTypeActions_Base<UAkRtpc>
{
public:
	FAssetTypeActions_AkRtpc(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkRtpc", "Audiokinetic Game Parameter"); }
	virtual FColor GetTypeColor() const override { return FColor(192, 128, 128); }
};

class FAssetTypeActions_AkTrigger : public FAkAssetTypeActions_Base<UAkTrigger>
{
public:
	FAssetTypeActions_AkTrigger(EAssetTypeCategories::Type InAssetCategory) : FAkAssetTypeActions_Base(InAssetCategory) {}

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AkAssetTypeActions", "AssetTypeActions_AkTrigger", "Audiokinetic Trigger"); }
	virtual FColor GetTypeColor() const override { return FColor(128, 192, 128); }
};

