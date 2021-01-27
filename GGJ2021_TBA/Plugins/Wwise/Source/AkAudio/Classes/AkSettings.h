// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "Engine/EngineTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AssetRegistryModule.h"
#include "AkUEFeatures.h"
#include "AkSettings.generated.h"

class UAkAcousticTexture;

USTRUCT()
struct FAkGeometrySurfacePropertiesToMap
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "AkGeometry Surface Properties Map")
	TSoftObjectPtr<class UAkAcousticTexture> AcousticTexture = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "AkGeometry Surface Properties Map", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OcclusionValue = 1.f;
};

struct AkGeometrySurfaceProperties
{
	UAkAcousticTexture* AcousticTexture = nullptr;
	float OcclusionValue = 1.f;
};

#define AK_MAX_AUX_PER_OBJ	4

DECLARE_EVENT(UAkSettings, AutoConnectChanged);
DECLARE_EVENT(UAkSettings, ActivatedNewAssetManagement);
DECLARE_EVENT_TwoParams(UAkSettings, SoundDataFolderChanged, const FString&, const FString&);

UCLASS(config = Game, defaultconfig)
class AKAUDIO_API UAkSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// The number of AkReverbVolumes that will be simultaneously applied to a sound source
	UPROPERTY(Config, EditAnywhere, Category="Ak Reverb Volume")
	uint8 MaxSimultaneousReverbVolumes = AK_MAX_AUX_PER_OBJ;

	// Wwise Project Path
	UPROPERTY(Config, EditAnywhere, Category="Installation", meta=(FilePathFilter="wproj", AbsolutePath))
	FFilePath WwiseProjectPath;

	// Where the Sound Data will be generated in the Content Folder
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data", meta=(RelativeToGameContentDir))
	FDirectoryPath WwiseSoundDataFolder;

	UPROPERTY(Config, EditAnywhere, Category = "Installation")
	bool bAutoConnectToWAAPI = false;

	// Default value for Occlusion Collision Channel when creating a new Ak Component.
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion")
	TEnumAsByte<ECollisionChannel> DefaultOcclusionCollisionChannel = ECollisionChannel::ECC_Visibility;
	
	// PhysicalMaterial to AcousticTexture and Occlusion Value Map
	UPROPERTY(Config, EditAnywhere, EditFixedSize, Category = "AkGeometry Surface Properties Map")
	TMap<TSoftObjectPtr<class UPhysicalMaterial>, FAkGeometrySurfacePropertiesToMap> AkGeometryMap;

	// When generating the event data, the media contained in switch containers will be splitted by state/switch value
	// and only loaded if the state/switch value are currently loaded
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data", meta=(EditCondition="UseEventBasedPackaging"))
	bool SplitSwitchContainerMedia = false;

	// Split Media folder into several folders.
	// Perforce has a limit of 32000 files per folder, if you are using Perforce you are strongly suggested to enable this.
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data", meta = (EditCondition = "UseEventBasedPackaging"))
	bool SplitMediaPerFolder = false;

	// Enable the new Event-based Soundbank Pipeline
	// When ticking this to true, it will delete the content of the SoundBank folder
	// and modify the Wwise project for the required changes in the project settings.
	// The new assets will be created the next time you open the editor.
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data", meta=(DisplayName="Use Event-Based Packaging"))
	bool UseEventBasedPackaging = false;

	// Enable automatic asset synchronization with the new asset management system
	// Changes to this settings will only reflect after restarting the editor
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data")
	bool EnableAutomaticAssetSynchronization = true;

	// Commit message that GenerateSoundBanksCommandlet will use
	UPROPERTY(Config, EditAnywhere, Category = "Sound Data")
	FString CommandletCommitMessage = TEXT("Unreal Wwise Sound Data auto-generation");

	UPROPERTY(Config, EditAnywhere, Category = "Sound Data")
	TMap<FString, FString> UnrealCultureToWwiseCulture;

	UPROPERTY(Config)
	bool AskedToUseNewAssetManagement = false;

	UPROPERTY(Config)
	bool bEnableMultiCoreRendering_DEPRECATED = false;

	UPROPERTY(Config)
	bool MigratedEnableMultiCoreRendering = false;

	UPROPERTY(Config)
	bool FixupRedirectorsDuringMigration = false;

	UPROPERTY(Config)
	FDirectoryPath WwiseWindowsInstallationPath_DEPRECATED;

	UPROPERTY(Config)
	FFilePath WwiseMacInstallationPath_DEPRECATED;

	static FString DefaultSoundDataFolder;

	virtual void PostInitProperties() override;

	bool GetAssociatedAcousticTexture(const UPhysicalMaterial* physMaterial, UAkAcousticTexture*& acousticTexture) const;
	bool GetAssociatedOcclusionValue(const UPhysicalMaterial* physMaterial, float& occlusionValue) const;

#if WITH_EDITOR
	void EnsureSoundDataPathIsInAlwaysCook() const;
	void RemoveSoundDataFromAlwaysStageAsUFS(const FString& SoundDataPath);
	void EnsurePluginContentIsInAlwaysCook() const;
	void InitAkGeometryMap();
#endif

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
#if UE_4_25_OR_LATER
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
#else
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif
#endif

private:
#if WITH_EDITOR
	FString PreviousWwiseProjectPath;
	FString PreviousWwiseSoundBankFolder;
	bool bTextureMapInitialized = false;
	TMap< UPhysicalMaterial*, UAkAcousticTexture* > TextureMapInternal;
	FAssetRegistryModule* AssetRegistryModule;

private:
	void OnAssetAdded(const FAssetData& NewAssetData);
	void OnAssetRemoved(const struct FAssetData& AssetData);
	void FillAkGeometryMap(const TArray<FAssetData>& PhysicalMaterials, const TArray<FAssetData>& AcousticTextureAssets);
	void UpdateAkGeometryMap();
	void RemoveSoundDataFromAlwaysCook(const FString& SoundDataPath);
	void AddSoundDataToAlwaysStageAsUFS();
	void SplitOrMergeMedia();

	bool bAkGeometryMapInitialized = false;
	TMap< UPhysicalMaterial*, UAkAcousticTexture* > PhysicalMaterialAcousticTextureMap;
	TMap< UPhysicalMaterial*, float > PhysicalMaterialOcclusionMap;
#endif

public:
	bool bRequestRefresh = false;
    mutable AutoConnectChanged OnAutoConnectChanged;
#if WITH_EDITOR
	mutable ActivatedNewAssetManagement OnActivatedNewAssetManagement;
	mutable SoundDataFolderChanged OnSoundDataFolderChanged;
#endif
};

