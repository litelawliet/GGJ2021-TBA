#pragma once

#include "AkToolBehavior.h"

class AkLegacyToolBehavior : public AkToolBehavior
{
public:
	using Super = AkToolBehavior;

	// CreateSoundDataWidget
	bool CreateSoundDataWidget(const TSharedRef<SWindow>& Window, TArray<TWeakObjectPtr<UAkAudioBank>>* SoundBanks, bool ProjectSave) override;

	// AkAssetManagementManager
	bool AkAssetManagementManager_ModifyProjectSettings(FString& ProjectContent) override;

	// AkAssetDatabase
	FString AkAssetDatabase_GetInitBankPackagePath() const override;
	bool AkAssetDatabase_ValidateAssetId(FGuid& outId) override;
	bool AkAssetDatabase_Remove(AkAssetDatabase* Instance, UAkAudioType* AudioType) override;

	// AkSoundDataBuilder
	bool AkSoundDataBuilder_GetBankName(AkSoundDataBuilder* Instance, UAkAudioBank* Bank, const TSet<FString>& BanksToGenerate, FString& bankName) override;

	// CookAkSoundDataTask
	TSharedPtr<AkSoundDataBuilder, ESPMode::ThreadSafe> CookAkSoundDataTask_CreateBuilder(const AkSoundDataBuilder::InitParameters& InitParameters) override;

	// WwiseConsoleAkSoundDataBuilder
	FString WwiseConsoleAkSoundDataBuilder_AudioBankEventIncludes() const override;
	FString WwiseConsoleAkSoundDataBuilder_AudioBankAuxBusIncludes() const override;

	// AkAssetFactory
	bool AkAssetFactory_ValidNewAssetPath(FName Name, const FString& AssetPath, const UClass* AssetClass) const override;
};