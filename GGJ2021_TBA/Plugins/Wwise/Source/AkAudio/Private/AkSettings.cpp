// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "AkSettings.h"

#include "AkAcousticTexture.h"
#include "AkAudioDevice.h"
#include "AkSettingsPerUser.h"
#include "AkUnrealHelper.h"
#include "AssetRegistryModule.h"
#include "StringMatchAlgos/Array2D.h"
#include "StringMatchAlgos/StringMatching.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "AkMediaAsset.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "HAL/PlatformFileManager.h"
#include "InitializationSettings/AkInitializationSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Platforms/AkUEPlatform.h"
#include "Settings/ProjectPackagingSettings.h"
#include "SettingsEditor/Public/ISettingsEditorModule.h"
#include "UnrealEd/Public/ObjectTools.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextbox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/SExpanderArrow.h"
#endif

//////////////////////////////////////////////////////////////////////////
// UAkSettings

namespace AkSettings_Helper
{
#if WITH_EDITOR
	void MigrateMultiCoreRendering(bool EnableMultiCoreRendering, const FString& PlatformName)
	{
		FString SettingsClassName = FString::Format(TEXT("Ak{0}InitializationSettings"), { *PlatformName });
		auto* SettingsClass = FindObject<UClass>(ANY_PACKAGE, *SettingsClassName);
		if (!SettingsClass)
		{
			return;
		}

		auto* MigrationFunction = SettingsClass->FindFunctionByName(TEXT("MigrateMultiCoreRendering"));
		auto* Settings = SettingsClass->GetDefaultObject();
		if (!MigrationFunction || !Settings)
		{
			return;
		}

		Settings->ProcessEvent(MigrationFunction, &EnableMultiCoreRendering);

		AkUnrealHelper::SaveConfigFile(Settings);
	}
#endif

	void MatchAcousticTextureNamesToPhysMaterialNames(
		const TArray<FAssetData>& PhysicalMaterials,
		const TArray<FAssetData>& AcousticTextures,
		TArray<int32>& assignments)
	{
		uint32 NumPhysMat = (uint32)PhysicalMaterials.Num();
		uint32 NumAcousticTex = (uint32)AcousticTextures.Num();

		// Create a scores matrix
		Array2D<float> scores(NumPhysMat, NumAcousticTex, 0);

		for (uint32 i = 0; i < NumPhysMat; ++i)
		{
			TArray<bool> perfectObjectMatches;
			perfectObjectMatches.Init(false, NumAcousticTex);

			if (PhysicalMaterials[i].GetAsset())
			{
				FString physMaterialName = PhysicalMaterials[i].GetAsset()->GetName();

				if (physMaterialName.Len() == 0)
					continue;

				for (uint32 j = 0; j < NumAcousticTex; ++j)
				{
					// Skip objects for which we already found a perfect match
					if (perfectObjectMatches[j] == true)
						continue;

					if (AcousticTextures[j].GetAsset())
					{
						FString acousticTextureName = AcousticTextures[j].GetAsset()->GetName();

						if (acousticTextureName.Len() == 0)
							continue;

						// Calculate longest common substring length
						float lcs = LCS::GetLCSScore(
							physMaterialName.ToLower(),
							acousticTextureName.ToLower());

						scores(i, j) = lcs;

						if (FMath::IsNearlyEqual(lcs, 1.f))
						{
							assignments[i] = j;
							perfectObjectMatches[j] = true;
							break;
						}
					}
				}
			}
		}

		for (uint32 i = 0; i < NumPhysMat; ++i)
		{
			if (assignments[i] == -1)
			{
				float bestScore = 0.f;
				int32 matchedIdx = -1;
				for (uint32 j = 0; j < NumAcousticTex; ++j)
				{
					if (scores(i, j) > bestScore)
					{
						bestScore = scores(i, j);
						matchedIdx = j;
					}
				}
				if (bestScore >= 0.2f)
					assignments[i] = matchedIdx;
			}
		}
	}
}

FString UAkSettings::DefaultSoundDataFolder = TEXT("WwiseAudio");

UAkSettings::UAkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WwiseSoundDataFolder.Path = DefaultSoundDataFolder;

#if WITH_EDITOR
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	//register to asset modification delegates
	auto& AssetRegistry = AssetRegistryModule->Get();
	AssetRegistry.OnAssetAdded().AddUObject(this, &UAkSettings::OnAssetAdded);
	AssetRegistry.OnAssetRemoved().AddUObject(this, &UAkSettings::OnAssetRemoved);
#endif
}

void UAkSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	UAkSettingsPerUser* AkSettingsPerUser = GetMutableDefault<UAkSettingsPerUser>();

	if (AkSettingsPerUser)
	{
		bool didChanges = false;

		if (!WwiseWindowsInstallationPath_DEPRECATED.Path.IsEmpty())
		{
			AkSettingsPerUser->WwiseWindowsInstallationPath = WwiseWindowsInstallationPath_DEPRECATED;
			WwiseWindowsInstallationPath_DEPRECATED.Path.Reset();
			didChanges = true;
		}

		if (!WwiseMacInstallationPath_DEPRECATED.FilePath.IsEmpty())
		{
			AkSettingsPerUser->WwiseMacInstallationPath = WwiseMacInstallationPath_DEPRECATED;
			WwiseMacInstallationPath_DEPRECATED.FilePath.Reset();
			didChanges = true;
		}

		if (didChanges)
		{
			AkUnrealHelper::SaveConfigFile(this);
			AkSettingsPerUser->SaveConfig();
		}
	}

	if (!MigratedEnableMultiCoreRendering)
	{
		MigratedEnableMultiCoreRendering = true;

		for (const auto& PlatformName : AkUnrealPlatformHelper::GetAllSupportedWwisePlatforms())
		{
			AkSettings_Helper::MigrateMultiCoreRendering(bEnableMultiCoreRendering_DEPRECATED, *PlatformName);
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
#if UE_4_25_OR_LATER
void UAkSettings::PreEditChange(FProperty* PropertyAboutToChange)
#else
void UAkSettings::PreEditChange(UProperty* PropertyAboutToChange)
#endif
{
	PreviousWwiseProjectPath = WwiseProjectPath.FilePath;
	PreviousWwiseSoundBankFolder = WwiseSoundDataFolder.Path;
}

#define LOCTEXT_NAMESPACE "AkAudio"
void UAkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	ISettingsEditorModule& SettingsEditorModule = FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor");

	if ( PropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, MaxSimultaneousReverbVolumes) )
	{
		MaxSimultaneousReverbVolumes = FMath::Clamp<uint8>( MaxSimultaneousReverbVolumes, 0, AK_MAX_AUX_PER_OBJ );
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if( AkAudioDevice )
		{
			AkAudioDevice->SetMaxAuxBus(MaxSimultaneousReverbVolumes);
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseProjectPath))
	{
		AkUnrealHelper::SanitizeProjectPath(WwiseProjectPath.FilePath, PreviousWwiseProjectPath, FText::FromString("Please enter a valid Wwise project"), bRequestRefresh);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, bAutoConnectToWAAPI))
	{
		OnAutoConnectChanged.Broadcast();
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, WwiseSoundDataFolder))
	{
		if (UseEventBasedPackaging)
		{
			FString oldSoundDataPackagePath = FString::Printf(TEXT("/Game/%s"), *PreviousWwiseSoundBankFolder);
			FString newSoundDataPackagePath = FString::Printf(TEXT("/Game/%s"), *WwiseSoundDataFolder.Path);

			OnSoundDataFolderChanged.Broadcast(oldSoundDataPackagePath, newSoundDataPackagePath);

			UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

			FDirectoryPath newPath;
			newPath.Path = newSoundDataPackagePath;
			PackagingSettings->DirectoriesToAlwaysCook.Add(newPath);

			RemoveSoundDataFromAlwaysCook(oldSoundDataPackagePath);
			AkUnrealHelper::SaveConfigFile(PackagingSettings);
		}

		SettingsEditorModule.OnApplicationRestartRequired();
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, UseEventBasedPackaging))
	{
		bool RestartRequired = true;
		if (UseEventBasedPackaging)
		{
			auto YesDelegate = FOnButtonClickedMigration::CreateLambda([this]() -> FReply {
				EnsureSoundDataPathIsInAlwaysCook();
				RemoveSoundDataFromAlwaysStageAsUFS(WwiseSoundDataFolder.Path);
				AkUnrealHelper::DeleteOldSoundBanks();
				OnActivatedNewAssetManagement.Broadcast();
				return FReply::Handled();
			});

			auto NoDelegate = FOnButtonClickedMigration::CreateLambda([this, &RestartRequired]() -> FReply {
				UseEventBasedPackaging = false;
				RestartRequired = false;
				return FReply::Handled();
			});

			AkUnrealHelper::ShowEventBasedPackagingMigrationDialog(YesDelegate, NoDelegate);
		}
		else
		{
			TArray<FAssetData> AssetsFound;
			FString AssetPath = FString(TEXT("/Game")) / WwiseSoundDataFolder.Path;
			AssetRegistryModule->Get().GetAssetsByPath(FName(*AssetPath), AssetsFound, true);
			if (AssetsFound.Num() > 0)
			{
				FText NewSoundDataFolder;

				TSharedPtr<SWindow> Dialog = SNew(SWindow)
					.Title(LOCTEXT("DisableEBPTitle", "Disable Event-Based Packaging"))
					.SupportsMaximize(false)
					.SupportsMinimize(false)
					.FocusWhenFirstShown(true)
					.SizingRule(ESizingRule::Autosized);

				TSharedRef<SWidget> DialogContent = 
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(4.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.45f)
						[
							SNew(SSpacer)
						]
						+ SVerticalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Left)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AssetsInSoundData", "Assets found in the Sound Data folder. Please choose a new destination folder for your SoundBanks."))
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.1f)
						[
							SNew(SSpacer)
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 2, 6, 2)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SoundDataFolder", "Sound Data Folder:"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 0, 2, 0)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("SoundDataFolderHint", "Name of the Sound Data Folder"))
								.OnTextCommitted_Lambda([&NewSoundDataFolder, &Dialog](const FText& InText, ETextCommit::Type InCommitType) {
									if (!InText.IsEmpty())
									{
										NewSoundDataFolder = InText;
										Dialog->RequestDestroyWindow();
									}
								})
								.MinDesiredWidth(200)
								.RevertTextOnEscape(true)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 0, 2, 0)
							[
								SNew(SButton)
								.OnClicked_Lambda([Dialog]() -> FReply { Dialog->RequestDestroyWindow(); return FReply::Handled(); })
								.Text(LOCTEXT("OkButtonLabel", "OK"))
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.45f)
						[
							SNew(SSpacer)
						]
					];

				Dialog->SetContent(DialogContent);
				FSlateApplication::Get().AddModalWindow(Dialog.ToSharedRef(), nullptr);
				WwiseSoundDataFolder.Path = NewSoundDataFolder.ToString();
				AkUnrealHelper::SaveConfigFile(this);
			}

			AddSoundDataToAlwaysStageAsUFS();
			RemoveSoundDataFromAlwaysCook(FString::Printf(TEXT("/Game/%s"), *PreviousWwiseSoundBankFolder));
		}

		if (RestartRequired)
		{
			SettingsEditorModule.OnApplicationRestartRequired();
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, EnableAutomaticAssetSynchronization))
	{
		SettingsEditorModule.OnApplicationRestartRequired();
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, AkGeometryMap))
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FAkGeometrySurfacePropertiesToMap, AcousticTexture))
		{
			for (auto& elem : AkGeometryMap)
			{
				PhysicalMaterialAcousticTextureMap[elem.Key.LoadSynchronous()] = elem.Value.AcousticTexture.LoadSynchronous();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAkGeometrySurfacePropertiesToMap, OcclusionValue))
		{
			for (auto& elem : AkGeometryMap)
			{
				PhysicalMaterialOcclusionMap[elem.Key.LoadSynchronous()] = elem.Value.OcclusionValue;
			}
		}
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAkSettings, SplitMediaPerFolder))
	{
		SplitOrMergeMedia();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#undef LOCTEXT_NAMESPACE

void UAkSettings::FillAkGeometryMap(
	const TArray<FAssetData>& PhysicalMaterialAssets,
	const TArray<FAssetData>& AcousticTextureAssets)
{
	TArray<int32> assignments;
	assignments.Init(-1, PhysicalMaterialAssets.Num());

	AkSettings_Helper::MatchAcousticTextureNamesToPhysMaterialNames(PhysicalMaterialAssets, AcousticTextureAssets, assignments);

	for (int i = 0; i < PhysicalMaterialAssets.Num(); i++)
	{
		auto physicalMaterial = Cast<UPhysicalMaterial>(PhysicalMaterialAssets[i].GetAsset());
		if (!PhysicalMaterialAcousticTextureMap.Contains(physicalMaterial))
		{
			if (assignments[i] != -1)
			{
				int32 acousticTextureIdx = assignments[i];
				auto acousticTexture = Cast<UAkAcousticTexture>(AcousticTextureAssets[acousticTextureIdx].GetAsset());
				PhysicalMaterialAcousticTextureMap.Add(physicalMaterial, acousticTexture);
			}
			else
			{
				PhysicalMaterialAcousticTextureMap.Add(physicalMaterial);
			}
		}
		else
		{
			if (assignments[i] != -1)
			{
				if (!PhysicalMaterialAcousticTextureMap[physicalMaterial])
				{
					int32 acousticTextureIdx = assignments[i];
					auto acousticTexture = Cast<UAkAcousticTexture>(AcousticTextureAssets[acousticTextureIdx].GetAsset());
					PhysicalMaterialAcousticTextureMap[physicalMaterial] = acousticTexture;
				}
			}
		}

		if (!PhysicalMaterialOcclusionMap.Contains(physicalMaterial))
			PhysicalMaterialOcclusionMap.Add(physicalMaterial, 1.f);
	}

	UpdateAkGeometryMap();
}

void UAkSettings::UpdateAkGeometryMap()
{
	AkGeometryMap.Empty();
	for (auto& elem : PhysicalMaterialAcousticTextureMap)
	{
		TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(elem.Key);
		TSoftObjectPtr<class UAkAcousticTexture> acousticTexPtr(elem.Value);
		FAkGeometrySurfacePropertiesToMap props;
		props.AcousticTexture = acousticTexPtr;
		props.OcclusionValue = PhysicalMaterialOcclusionMap[elem.Key];
		AkGeometryMap.Emplace(physMatPtr, props);
	}

	AkGeometryMap.KeySort([](const TSoftObjectPtr<class UPhysicalMaterial>& Lhs, const TSoftObjectPtr<class UPhysicalMaterial>& Rhs) {
		UPhysicalMaterial* lhs = Lhs.Get();
		UPhysicalMaterial* rhs = Rhs.Get();
		if (lhs && rhs)
		{
#if UE_4_23_OR_LATER
			return Lhs.Get()->GetFName().LexicalLess(Rhs.Get()->GetFName());
#else
			return Lhs.Get()->GetFName() < Rhs.Get()->GetFName();
#endif
		}
		else
		{
			return !lhs ? true : false;
		}
	});
}

void UAkSettings::InitAkGeometryMap()
{
	PhysicalMaterialAcousticTextureMap.Empty();
	PhysicalMaterialOcclusionMap.Empty();

	// copy everything from the ini file
	for (auto& elem : AkGeometryMap)
	{
		auto physMat = elem.Key.LoadSynchronous();
		auto surfaceProps = elem.Value;
		PhysicalMaterialAcousticTextureMap.Add(physMat, surfaceProps.AcousticTexture.LoadSynchronous());
		PhysicalMaterialOcclusionMap.Add(physMat, surfaceProps.OcclusionValue);
	}
	bAkGeometryMapInitialized = true;

	// Obtain the 2 list of children we want to match
	TArray<FAssetData> PhysicalMaterials, AcousticTextures;
	AssetRegistryModule->Get().GetAssetsByClass(UPhysicalMaterial::StaticClass()->GetFName(), PhysicalMaterials);
	AssetRegistryModule->Get().GetAssetsByClass(UAkAcousticTexture::StaticClass()->GetFName(), AcousticTextures);

	FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
}

void UAkSettings::OnAssetAdded(const FAssetData& NewAssetData)
{
	if (!bAkGeometryMapInitialized)
		return;

	if (auto physicalMaterial = Cast<UPhysicalMaterial>(NewAssetData.GetAsset()))
	{
		TArray<FAssetData> PhysicalMaterials, AcousticTextures;
		PhysicalMaterials.Add(NewAssetData);
		AssetRegistryModule->Get().GetAssetsByClass(UAkAcousticTexture::StaticClass()->GetFName(), AcousticTextures);

		FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
	}

	if (auto acousticTexture = Cast<UAkAcousticTexture>(NewAssetData.GetAsset()))
	{
		TArray<FAssetData> PhysicalMaterials, AcousticTextures;
		AssetRegistryModule->Get().GetAssetsByClass(UPhysicalMaterial::StaticClass()->GetFName(), PhysicalMaterials);
		AcousticTextures.Add(NewAssetData);

		FillAkGeometryMap(PhysicalMaterials, AcousticTextures);
	}
}

void UAkSettings::OnAssetRemoved(const struct FAssetData& AssetData)
{
	if (auto physicalMaterial = Cast<UPhysicalMaterial>(AssetData.GetAsset()))
	{
		PhysicalMaterialAcousticTextureMap.Remove(physicalMaterial);
		PhysicalMaterialOcclusionMap.Remove(physicalMaterial);
		UpdateAkGeometryMap();
	}
}

void UAkSettings::EnsureSoundDataPathIsInAlwaysCook() const
{
	if (!UseEventBasedPackaging)
	{
		return;
	}

	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	bool packageSettingsNeedUpdate = false;
	FString soundDataGamePath = FString::Printf(TEXT("/Game/%s"), *WwiseSoundDataFolder.Path);

	if (!PackagingSettings->DirectoriesToAlwaysCook.ContainsByPredicate([soundDataGamePath](FDirectoryPath PathInArray) { return PathInArray.Path == soundDataGamePath; }))
	{
		FDirectoryPath newPath;
		newPath.Path = soundDataGamePath;

		PackagingSettings->DirectoriesToAlwaysCook.Add(newPath);
		packageSettingsNeedUpdate = true;
	}

	for (int32 i = PackagingSettings->DirectoriesToAlwaysCook.Num() - 1; i >= 0; --i)
	{
		auto ContentPath = FPackageName::LongPackageNameToFilename(PackagingSettings->DirectoriesToAlwaysCook[i].Path);

		if (!FPaths::DirectoryExists(ContentPath))
		{
			PackagingSettings->DirectoriesToAlwaysCook.RemoveAt(i);
			packageSettingsNeedUpdate = true;
		}
	}

	if (packageSettingsNeedUpdate)
	{
		AkUnrealHelper::SaveConfigFile(PackagingSettings);
	}
}

void UAkSettings::EnsurePluginContentIsInAlwaysCook() const
{
	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	bool packageSettingsNeedUpdate = false;

	TArray<FString> PathsToCheck = { TEXT("/Wwise/WwiseTree"), TEXT("/Wwise/WwiseTypes") };

	for (auto pathToCheck : PathsToCheck)
	{
		if (!PackagingSettings->DirectoriesToAlwaysCook.ContainsByPredicate([pathToCheck](FDirectoryPath PathInArray) { return PathInArray.Path == pathToCheck; }))
		{
			FDirectoryPath newPath;
			newPath.Path = pathToCheck;

			PackagingSettings->DirectoriesToAlwaysCook.Add(newPath);
			packageSettingsNeedUpdate = true;
		}
	}

	if (packageSettingsNeedUpdate)
	{
		AkUnrealHelper::SaveConfigFile(PackagingSettings);
	}
}


void UAkSettings::RemoveSoundDataFromAlwaysCook(const FString& SoundDataPath)
{
	bool changed = false;

	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	for (int32 i = PackagingSettings->DirectoriesToAlwaysCook.Num() - 1; i >= 0; --i)
	{
		if (PackagingSettings->DirectoriesToAlwaysCook[i].Path == SoundDataPath)
		{
			PackagingSettings->DirectoriesToAlwaysCook.RemoveAt(i);
			changed = true;
			break;
		}
	}

	if (changed)
	{
		AkUnrealHelper::SaveConfigFile(PackagingSettings);
	}
}

void UAkSettings::AddSoundDataToAlwaysStageAsUFS()
{
	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	bool foundPackageDirectory = false;
	bool packageSettingsNeedUpdate = false;

	for (int32 i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); i++)
	{
		if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == WwiseSoundDataFolder.Path)
		{
			foundPackageDirectory = true;
			break;
		}
	}

	if (!foundPackageDirectory)
	{
		PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(WwiseSoundDataFolder);
		packageSettingsNeedUpdate = true;
	}

	if (!PreviousWwiseSoundBankFolder.IsEmpty())
	{
		for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); i++)
		{
			if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == PreviousWwiseSoundBankFolder)
			{
				PackagingSettings->DirectoriesToAlwaysStageAsUFS.RemoveAt(i);
				packageSettingsNeedUpdate = true;
			}
		}
	}

	if (packageSettingsNeedUpdate)
	{
		AkUnrealHelper::SaveConfigFile(PackagingSettings);
	}
}

void UAkSettings::RemoveSoundDataFromAlwaysStageAsUFS(const FString& SoundDataPath)
{
	bool changed = false;

	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	for (int32 i = PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num() - 1; i >= 0; --i)
	{
		if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == SoundDataPath)
		{
			PackagingSettings->DirectoriesToAlwaysStageAsUFS.RemoveAt(i);
			changed = true;
			break;
		}
	}

	if (changed)
	{
		AkUnrealHelper::SaveConfigFile(PackagingSettings);
	}
}

void UAkSettings::SplitOrMergeMedia()
{
	TArray<FAssetData> MediaList;
	AssetRegistryModule->Get().GetAssetsByClass(UAkMediaAsset::StaticClass()->GetFName(), MediaList, false);

	auto& AssetToolModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<FAssetRenameData> assetsToRename;

	for (auto& entry : MediaList)
	{
		if (auto mediaAsset = Cast<UAkMediaAsset>(entry.GetAsset()))
		{
			auto id = mediaAsset->Id;
			FString stringId = FString::Printf(TEXT("%u"), id);

			FString subFolder;

			if (SplitMediaPerFolder)
			{
				subFolder = FString::Printf(TEXT("%02x/%02x"), (id >> 24) & 0xFF, (id >> 16) & 0xFF);
			}

			FString mediaNewPath = ObjectTools::SanitizeObjectPath(FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), AkUnrealHelper::MediaFolderName, subFolder, stringId + TEXT(".") + stringId));

			assetsToRename.Emplace(FSoftObjectPath(entry.ObjectPath.ToString()), FSoftObjectPath(mediaNewPath));
		}
	}

	AssetToolModule.Get().RenameAssets(assetsToRename);
}

#endif // WITH_EDITOR

bool UAkSettings::GetAssociatedAcousticTexture(const UPhysicalMaterial* physMaterial, UAkAcousticTexture*& acousticTexture) const
{
	TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(physMaterial);
	auto props = AkGeometryMap.Find(physMatPtr);

	if (!props)
		return false;

	TSoftObjectPtr<class UAkAcousticTexture> texturePtr = props->AcousticTexture;
	acousticTexture = texturePtr.LoadSynchronous();
	return true;
}

bool UAkSettings::GetAssociatedOcclusionValue(const UPhysicalMaterial* physMaterial, float& occlusionValue) const
{
	TSoftObjectPtr<class UPhysicalMaterial> physMatPtr(physMaterial);
	auto props = AkGeometryMap.Find(physMatPtr);

	if (!props)
		return false;

	occlusionValue = props->OcclusionValue;
	return true;
}
