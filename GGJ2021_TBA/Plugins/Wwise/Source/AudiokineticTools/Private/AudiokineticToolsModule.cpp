// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AudiokineticToolsModule.cpp
=============================================================================*/
#include "AudiokineticToolsPrivatePCH.h"

// @todo sequencer uobjects: The *.generated.inl should auto-include required headers (they should always have #pragma once anyway)
#include "AkAcousticPortal.h"
#include "AkAudioBankGenerationHelpers.h"
#include "AkAudioDevice.h"
#include "AkAudioStyle.h"
#include "AkComponent.h"
#include "AkEventAssetBroker.h"
#include "AkGeometryComponent.h"
#include "AkLateReverbComponent.h"
#include "AkMatineeImportTools.h"
#include "AkRoomComponent.h"
#include "AkSettings.h"
#include "AkSettingsPerUser.h"
#include "AkSurfaceReflectorSetComponent.h"
#include "AkUnrealHelper.h"
#include "AssetManagement/AkAssetDatabase.h"
#include "AssetManagement/AkAssetManagementManager.h"
#include "AssetManagement/CookAkSoundDataTask.h"
#include "AssetManagement/CreateAkAssetsVisitor.h"
#include "AssetManagement/WaapiAssetSynchronizer.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ComponentAssetBroker.h"
#include "ContentBrowserModule.h"
#include "DetailsCustomization/AkGeometryComponentDetailsCustomization.h"
#include "DetailsCustomization/AkLateReverbComponentDetailsCustomization.h"
#include "DetailsCustomization/AkRoomComponentDetailsCustomization.h"
#include "DetailsCustomization/AkSurfaceReflectorSetDetailsCustomization.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/UnrealEdEngine.h"
#include "Factories/ActorFactoryAkAmbientSound.h"
#include "Factories/AkAssetTypeActions.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IProjectManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "InterpTrackAkAudioEvent.h"
#include "InterpTrackAkAudioRTPC.h"
#include "ISequencerModule.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "MatineeModule.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "Platforms/AkUEPlatform.h"
#include "ProjectDescriptor.h"
#include "PropertyEditorModule.h"
#include "Sequencer/InterpTrackAkAudioEventHelper.h"
#include "Sequencer/InterpTrackAkAudioRTPCHelper.h"
#include "Sequencer/MovieSceneAkAudioEventTrackEditor.h"
#include "Sequencer/MovieSceneAkAudioRTPCTrackEditor.h"
#include "Settings/ProjectPackagingSettings.h"
#include "SettingsEditor/Public/ISettingsEditorModule.h"
#include "UnrealEd/Public/EditorBuildUtils.h"
#include "UnrealEdGlobals.h"
#include "UnrealEdMisc.h"
#include "Visualizer/AkAcousticPortalVisualizer.h"
#include "Visualizer/AkComponentVisualizer.h"
#include "Visualizer/AkSurfaceReflectorSetComponentVisualizer.h"
#include "WaapiPicker/SWaapiPicker.h"
#include "WaapiPicker/WwiseTreeItem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#if !UE_4_25_OR_LATER
#include "MatineeToLevelSequenceModule.h"
#endif


#include "WwisePicker/SWwisePicker.h"
#include "WwiseProject/WwiseProjectInfo.h"
#include "WwiseProject/WwiseWorkUnitParser.h"

#if UE_4_24_OR_LATER
#include "Developer/ToolMenus/Public/ToolMenu.h"
#include "Developer/ToolMenus/Public/ToolMenus.h"
#endif

#define LOCTEXT_NAMESPACE "AkAudio"

DEFINE_LOG_CATEGORY_STATIC(LogAudiokineticTools, Log, All);

namespace
{
	struct WwiseLanguageToUnrealCulture
	{
		const TCHAR* WwiseLanguage;
		const TCHAR* UnrealCulture;
	};

	// This list come from the fixed list of languages that were used before Wwise 2017.1
	const WwiseLanguageToUnrealCulture WwiseLanguageToUnrealCultureList[] = {
		{TEXT("Arabic"), TEXT("ar")},
		{TEXT("Bulgarian"), TEXT("bg")},
		{TEXT("Chinese(HK)"), TEXT("zh-HK")},
		{TEXT("Chinese(Malaysia)"), TEXT("zh")},
		{TEXT("Chinese(PRC)"), TEXT("zh-CN")},
		{TEXT("Chinese(Taiwan)"), TEXT("zh-TW")},
		{TEXT("Czech"), TEXT("cs")},
		{TEXT("Danish"), TEXT("da")},
		{TEXT("English(Australia)"), TEXT("en-AU")},
		{TEXT("English(Canada)"), TEXT("en-CA")},
		{TEXT("English(US)"), TEXT("en-US")},
		{TEXT("English(UK)"), TEXT("en-GB")},
		{TEXT("Finnish"), TEXT("fi")},
		{TEXT("French(Canada)"), TEXT("fr-CA")},
		{TEXT("French(France)"), TEXT("fr-FR")},
		{TEXT("German"), TEXT("de")},
		{TEXT("Greek"), TEXT("el")},
		{TEXT("Hebrew"), TEXT("he")},
		{TEXT("Hungarian"), TEXT("hu")},
		{TEXT("Indonesian"), TEXT("id")},
		{TEXT("Italian"), TEXT("it")},
		{TEXT("Japanese"), TEXT("ja")},
		{TEXT("Korean"), TEXT("ko")},
		{TEXT("Norwegian "), TEXT("no")},
		{TEXT("Polish"), TEXT("pl")},
		{TEXT("Portuguese(Brazil)"), TEXT("pt-BR")},
		{TEXT("Portuguese(Portugal)"), TEXT("pt-PT")},
		{TEXT("Romanian"), TEXT("ro")},
		{TEXT("Russian"), TEXT("ru")},
		{TEXT("Slovenian"), TEXT("sl")},
		{TEXT("Spanish(Mexico)"), TEXT("es-MX")},
		{TEXT("Spanish(Spain)"), TEXT("es-ES")},
		{TEXT("Swedish"), TEXT("sv")},
		{TEXT("Thai"), TEXT("th")},
		{TEXT("Turkish"), TEXT("tr")},
		{TEXT("Ukrainian"), TEXT("uk")},
		{TEXT("Vietnamese"), TEXT("vi")},
	};
}

class FAudiokineticToolsModule : public IAudiokineticTools
{
	TSharedRef<SDockTab> CreateWaapiPickerWindow(const FSpawnTabArgs& Args)
	{
		return
			SNew(SDockTab)
			.Icon(FSlateIcon(FAkAudioStyle::GetStyleSetName(), "AudiokineticTools.AkPickerTabIcon").GetIcon())
			.Label(LOCTEXT("AkAudioWaapiPickerTabTitle", "Waapi Picker"))
			.TabRole(ETabRole::NomadTab)
			.ContentPadding(5)
			[
				SAssignNew(AkWaapiPicker, SWaapiPicker)
				.ShowGenerateSoundBanksButton(true)
				.OnDragDetected(FOnDragDetected::CreateRaw(this, &FAudiokineticToolsModule::HandleOnDragDetected))
				.OnGenerateSoundBanksClicked_Lambda([] {
					AkAudioBankGenerationHelper::CreateGenerateSoundDataWindow();
				})
			];
	}

	TSharedRef<SDockTab> CreateWwisePickerWindow(const FSpawnTabArgs& Args)
	{
		return
			SNew(SDockTab)
			.Icon(FSlateIcon(FAkAudioStyle::GetStyleSetName(), "AudiokineticTools.AkPickerTabIcon").GetIcon())
			.Label(LOCTEXT("AkAudioWwisePickerTabTitle", "Wwise Picker"))
			.TabRole(ETabRole::NomadTab)
			.ContentPadding(5)
			[
				SAssignNew(AkWwisePicker, SWwisePicker)
			];
	}

	FReply HandleOnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
	{
		return SWwisePicker::DoDragDetected(MouseEvent, AkWaapiPicker->GetSelectedItems());
	}
	
	void OpenOnlineHelp()
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(TEXT("https://www.audiokinetic.com/library/?source=UE4&id=index.html"));
	}

#if UE_4_24_OR_LATER
	void RegisterWwiseHelpMenu()
	{
		FToolMenuOwnerScoped ownerScoped(this);
		UToolMenu* helpMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Help");
		FToolMenuSection& wwiseSection = helpMenu->AddSection("AkHelp", LOCTEXT("AkHelpLabel", "Audiokinetic"), FToolMenuInsert("HelpBrowse", EToolMenuInsertType::Default));

		wwiseSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			NAME_None,
			LOCTEXT("AkWwiseHelpEntry", "Wwise Help"),
			LOCTEXT("AkWwiseHelpEntryToolTip", "Shows the online Wwise documentation."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FAudiokineticToolsModule::OpenOnlineHelp))
		));
	}
#else
	void AddWwiseHelp(FMenuBuilder& MenuBuilder)
	{
#if !UE_4_24_OR_LATER
		MenuBuilder.BeginSection("AkHelp", LOCTEXT("AkHelpLabel", "Audiokinetic"));
#endif
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AkWwiseHelpEntry", "Wwise Help"),
			LOCTEXT("AkWwiseHelpEntryToolTip", "Shows the online Wwise documentation."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FAudiokineticToolsModule::OpenOnlineHelp)));
#if !UE_4_24_OR_LATER
		MenuBuilder.EndSection();
#endif
	}
#endif

	void UpdateUnrealCultureToWwiseCultureMap(const WwiseProjectInfo& wwiseProjectInfo)
	{
		static constexpr auto InvariantCultureLCID = 0x007F;

		UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
		if (!AkSettings)
		{
			return;
		}
		if (!AkSettings->UseEventBasedPackaging)
		{
			return;
		}

		TMap<FString, FString> wwiseToUnrealMap;
		for (auto& entry : WwiseLanguageToUnrealCultureList)
		{
			wwiseToUnrealMap.Add(entry.WwiseLanguage, entry.UnrealCulture);
		}

		TMap<FString, int> languageCountMap;

		for (auto& language : wwiseProjectInfo.SupportedLanguages())
		{
			if (auto* foundUnrealCulture = wwiseToUnrealMap.Find(language.Name))
			{
				auto culturePtr = FInternationalization::Get().GetCulture(*foundUnrealCulture);

				if (culturePtr && culturePtr->GetLCID() != InvariantCultureLCID)
				{
					int& langCount = languageCountMap.FindOrAdd(culturePtr->GetTwoLetterISOLanguageName());
					++langCount;
				}
			}
		}

		TSet<FString> foundCultures;

		bool modified = false;
		for (auto& language : wwiseProjectInfo.SupportedLanguages())
		{
			if (auto* foundUnrealCulture = wwiseToUnrealMap.Find(language.Name))
			{
				auto culturePtr = FInternationalization::Get().GetCulture(*foundUnrealCulture);

				if (culturePtr && culturePtr->GetLCID() != InvariantCultureLCID)
				{
					int* langCount = languageCountMap.Find(culturePtr->GetTwoLetterISOLanguageName());

					if (langCount && *langCount > 1)
					{
						auto newKey = *foundUnrealCulture;
						if (!AkSettings->UnrealCultureToWwiseCulture.Contains(newKey))
						{
							AkSettings->UnrealCultureToWwiseCulture.Add(newKey, language.Name);
							modified = true;
						}

						foundCultures.Add(newKey);
					}
					else
					{
						auto newKey = culturePtr->GetTwoLetterISOLanguageName();
						if (!AkSettings->UnrealCultureToWwiseCulture.Contains(newKey))
						{
							AkSettings->UnrealCultureToWwiseCulture.Add(newKey, language.Name);
							modified = true;
						}

						foundCultures.Add(newKey);
					}
				}
			}
			else
			{
				for (auto& entry : AkSettings->UnrealCultureToWwiseCulture)
				{
					if (entry.Value == language.Name)
					{
						foundCultures.Add(entry.Key);
						break;
					}
				}
			}
		}

		TSet<FString> keysToRemove;
		for (auto& entry : AkSettings->UnrealCultureToWwiseCulture)
		{
			if (!foundCultures.Contains(entry.Key))
			{
				keysToRemove.Add(entry.Key);
			}
		}

		for (auto& keyToRemove : keysToRemove)
		{
			AkSettings->UnrealCultureToWwiseCulture.Remove(keyToRemove);
			modified = true;
		}

		if (modified)
		{
			AkSettings->SaveConfig();
		}
	}

	void VerifyWwiseProjectPath(UAkSettings* AkSettings, UAkSettingsPerUser* AkSettingsPerUser)
	{
		if (AkSettings->WwiseProjectPath.FilePath.IsEmpty())
		{
			if (!AkSettingsPerUser->SuppressWwiseProjectPathWarnings && FApp::CanEverRender())
			{
				if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SettingsNotSet", "Wwise settings do not seem to be set. Would you like to open the settings window to set them?")))
				{
					FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Wwise"), FName("User Settings"));
				}
			}
			else
			{
				UE_LOG(LogAudiokineticTools, Log, TEXT("Wwise project not found. The Wwise picker will not be usable."));
			}
		}
		else
		{
			// First-time plugin migration: Project might be relative to Engine path. Fix-up the path to make it relative to the game.
			const auto ProjectDir = FPaths::ProjectDir();
			FString FullGameDir = FPaths::ConvertRelativePathToFull(ProjectDir);
			FString TempPath = FPaths::ConvertRelativePathToFull(FullGameDir, AkSettings->WwiseProjectPath.FilePath);
			if (!FPaths::FileExists(TempPath))
			{
				if (!AkSettingsPerUser->SuppressWwiseProjectPathWarnings)
				{
					TSharedPtr<SWindow> Dialog = SNew(SWindow)
						.Title(LOCTEXT("ResetWwisePath", "Re-set Wwise Path"))
						.SupportsMaximize(false)
						.SupportsMinimize(false)
						.FocusWhenFirstShown(true)
						.SizingRule(ESizingRule::Autosized);

					TSharedRef<SWidget> DialogContent = SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.25f)
						[
							SNew(SSpacer)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkUpdateWwisePath", "The Wwise UE4 Integration plug-in's update process requires the Wwise Project Path to be set in the Project Settings dialog. Would you like to open the Project Settings?"))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.75f)
						[
							SNew(SSpacer)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SCheckBox)
							.Padding(FMargin(6.0, 2.0))
							.OnCheckStateChanged_Lambda([&](ECheckBoxState DontAskState) {
								AkSettingsPerUser->SuppressWwiseProjectPathWarnings = (DontAskState == ECheckBoxState::Checked);
							})
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AkDontShowAgain", "Don't show this again"))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SSpacer)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 3.0f, 0.0f, 3.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("Yes", "Yes"))
								.OnClicked_Lambda([&]() -> FReply {
									FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("Wwise"));
									Dialog->RequestDestroyWindow();
									AkSettingsPerUser->SaveConfig();
									return FReply::Handled();
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 3.0f, 0.0f, 3.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("No", "No"))
								.OnClicked_Lambda([&]() -> FReply {
									Dialog->RequestDestroyWindow();
									AkSettingsPerUser->SaveConfig();
									return FReply::Handled();
								})
							]
						]
					;

					Dialog->SetContent(DialogContent);
					FSlateApplication::Get().AddModalWindow(Dialog.ToSharedRef(), nullptr);
				}
				else
				{
					UE_LOG(LogAudiokineticTools, Log, TEXT("Wwise project not found. The Wwise picker will not be usable."));
				}
			}
			else
			{
				FPaths::MakePathRelativeTo(TempPath, *ProjectDir);
				if (AkSettings->WwiseProjectPath.FilePath != TempPath)
				{
					AkSettings->WwiseProjectPath.FilePath = TempPath;
					AkUnrealHelper::SaveConfigFile(AkSettings);
				}
			}
		}
	}

	void AskToUseNewAssetManagement(bool& doAssetSync, bool& doModifyProject, UAkSettings* AkSettings, const FString& CacheDirectory)
	{
		if (!AkSettings->AskedToUseNewAssetManagement)
		{
			auto doEnableNewAssetManagement = [this, &doAssetSync, &AkSettings, &CacheDirectory] {
				if (!assetManagementManager.IsInited())
				{
					assetManagementManager.Init();
				}

				AkSettings->AskedToUseNewAssetManagement = true;
				AkSettings->UseEventBasedPackaging = true;
				AkSettings->bAutoConnectToWAAPI = true;
				AkSettings->RemoveSoundDataFromAlwaysStageAsUFS(AkSettings->WwiseSoundDataFolder.Path);
				AkUnrealHelper::SaveConfigFile(AkSettings);

				AkSettings->OnAutoConnectChanged.Broadcast();

				FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*CacheDirectory);
				AkUnrealHelper::DeleteOldSoundBanks();

				doAssetSync = false;
			};

			TArray<FAssetData> akAudioTypes;
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetAssetsByClass(UAkAudioType::StaticClass()->GetFName(), akAudioTypes, true);

			if (akAudioTypes.Num() > 0)
			{
				auto YesDelegate = FOnButtonClickedMigration::CreateLambda([this, &doEnableNewAssetManagement, &AkSettings]() -> FReply {
					doEnableNewAssetManagement();
					assetManagementManager.DoAssetMigration();
					return FReply::Handled();
				});

				auto NoDelegate = FOnButtonClickedMigration::CreateLambda([&AkSettings, &doModifyProject]() -> FReply {
					AkSettings->AskedToUseNewAssetManagement = true;
					AkSettings->UseEventBasedPackaging = false;
					AkSettings->EnableAutomaticAssetSynchronization = false;
					AkSettings->SplitSwitchContainerMedia = false;
					AkUnrealHelper::SaveConfigFile(AkSettings);
					doModifyProject = false;

					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MigrationDone", "Unreal Editor must be restarted for migration changes to take effect."));
					ISettingsEditorModule& SettingsEditorModule = FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor");
					SettingsEditorModule.OnApplicationRestartRequired();
					return FReply::Handled();
				});

				AkUnrealHelper::ShowEventBasedPackagingMigrationDialog(YesDelegate, NoDelegate);
			}
			else
			{
				doEnableNewAssetManagement();
				assetManagementManager.DoAssetMigration();
			}
		}
	}

	void OnAssetRegistryFilesLoaded()
	{
		UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
		UAkSettingsPerUser* AkSettingsPerUser = GetMutableDefault<UAkSettingsPerUser>();
		auto* CurrentProject = IProjectManager::Get().GetCurrentProject();

		bool doAssetSync = true;
		bool doModifyProject = true;

		WwiseProjectInfo wwiseProjectInfo;
		wwiseProjectInfo.Parse();

		// If we're on the project loader screen, we don't want to display the dialog.
		// In that case, CurrentProject is nullptr.
		if (CurrentProject && AkSettings && AkSettingsPerUser)
		{
			VerifyWwiseProjectPath(AkSettings, AkSettingsPerUser);

#if 0 // Disable the migration prompt until EBP stability improves
			AskToUseNewAssetManagement(doAssetSync, doModifyProject, AkSettings, wwiseProjectInfo.CacheDirectory());
#endif

			if (doModifyProject)
			{
				assetManagementManager.ModifyProjectSettings();
			}
		}

		if (!AkSettings || !CurrentProject || (AkSettings && !AkSettings->UseEventBasedPackaging) || (AkSettings && !AkSettings->EnableAutomaticAssetSynchronization))
		{
			assetManagementManager.Uninit();
			doAssetSync = false;
		}

		UpdateUnrealCultureToWwiseCultureMap(wwiseProjectInfo);

		if (GUnrealEd != NULL)
		{
			GUnrealEd->RegisterComponentVisualizer(UAkComponent::StaticClass()->GetFName(), MakeShareable(new FAkComponentVisualizer));
			GUnrealEd->RegisterComponentVisualizer(UAkSurfaceReflectorSetComponent::StaticClass()->GetFName(), MakeShareable(new FAkSurfaceReflectorSetComponentVisualizer));
			GUnrealEd->RegisterComponentVisualizer(UAkPortalComponent::StaticClass()->GetFName(), MakeShareable(new UAkPortalComponentVisualizer));
		}

		AkSettings->InitAkGeometryMap();
		AkSettings->EnsureSoundDataPathIsInAlwaysCook();
		AkSettings->EnsurePluginContentIsInAlwaysCook();

		if (doAssetSync)
		{
			assetManagementManager.DoAssetSynchronization();
		}
	}

	void LateRegistrationOfMatineeToLevelSequencer()
	{
#if !UE_4_25_OR_LATER
		IMatineeToLevelSequenceModule& Module = FModuleManager::LoadModuleChecked<IMatineeToLevelSequenceModule>(TEXT("MatineeToLevelSequence"));

		ConvertMatineeRTPCTrackHandle = Module.RegisterTrackConverterForMatineeClass(UInterpTrackAkAudioRTPC::StaticClass(), 
			IMatineeToLevelSequenceModule::FOnConvertMatineeTrack::CreateLambda([](UInterpTrack* Track, FGuid PossessableGuid, UMovieScene* NewMovieScene)
		{
			if (Track->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
			{
				const UInterpTrackAkAudioRTPC* MatineeAkAudioRTPCTrack = StaticCast<const UInterpTrackAkAudioRTPC*>(Track);
				UMovieSceneAkAudioRTPCTrack* AkAudioRTPCTrack = NewMovieScene->AddTrack<UMovieSceneAkAudioRTPCTrack>(PossessableGuid);
				FAkMatineeImportTools::CopyInterpAkAudioRTPCTrack(MatineeAkAudioRTPCTrack, AkAudioRTPCTrack);
			}
		}));

		ConvertMatineeEventTrackHandle = Module.RegisterTrackConverterForMatineeClass(UInterpTrackAkAudioEvent::StaticClass(), 
			IMatineeToLevelSequenceModule::FOnConvertMatineeTrack::CreateLambda([](UInterpTrack* Track, FGuid PossessableGuid, UMovieScene* NewMovieScene)
		{
			if (Track->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
			{
				const UInterpTrackAkAudioEvent* MatineeAkAudioEventTrack = StaticCast<const UInterpTrackAkAudioEvent*>(Track);
				UMovieSceneAkAudioEventTrack* AkAudioEventTrack = NewMovieScene->AddTrack<UMovieSceneAkAudioEventTrack>(PossessableGuid);
				FAkMatineeImportTools::CopyInterpAkAudioEventTrack(MatineeAkAudioEventTrack, AkAudioEventTrack);
			}
		}));
#endif

		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
		RTPCTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FMovieSceneAkAudioRTPCTrackEditor::CreateTrackEditor));
		EventTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FMovieSceneAkAudioEventTrackEditor::CreateTrackEditor));

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnFilesLoaded().Remove(LateRegistrationOfMatineeToLevelSequencerHandle);
	}

	void OnActivatedNewAssetManagement()
	{
		assetManagementManager.ModifyProjectSettings();
		assetManagementManager.DoAssetMigration();
	}

	virtual void StartupModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			auto AudiokineticAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Audiokinetic")), LOCTEXT("AudiokineticAssetCategory", "Audiokinetic"));

			AkAssetTypeActionsArray =
			{
				MakeShared<FAssetTypeActions_AkAudioBank>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkAudioEvent>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkAcousticTexture>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkAuxBus>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkMediaAsset>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkRtpc>(AudiokineticAssetCategoryBit),
				MakeShared<FAssetTypeActions_AkTrigger>(AudiokineticAssetCategoryBit),
			};

			for (auto& AkAssetTypeActions : AkAssetTypeActionsArray)
				AssetTools.RegisterAssetTypeActions(AkAssetTypeActions.ToSharedRef());
		}

		if (FModuleManager::Get().IsModuleLoaded("LevelEditor") && !IsRunningCommandlet())
		{
			// Extend the build menu to handle Audiokinetic-specific entries
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			LevelViewportToolbarBuildMenuExtenderAk = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FAudiokineticToolsModule::ExtendBuildContextMenuForAudiokinetic);
			LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().Add(LevelViewportToolbarBuildMenuExtenderAk);
			LevelViewportToolbarBuildMenuExtenderAkHandle = LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().Last().GetHandle();

			// Add Wwise to the help menu
#if UE_4_24_OR_LATER
			RegisterWwiseHelpMenu();
#else
			MainMenuExtender = MakeShareable(new FExtender);
			MainMenuExtender->AddMenuExtension("HelpBrowse", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FAudiokineticToolsModule::AddWwiseHelp));
			LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
#endif
		}

		RegisterSettings();

		AkEventBroker = MakeShared<FAkEventAssetBroker>();
		FComponentAssetBrokerage::RegisterBroker(AkEventBroker, UAkComponent::StaticClass(), true, true);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SWaapiPicker::WaapiPickerTabName, FOnSpawnTab::CreateRaw(this, &FAudiokineticToolsModule::CreateWaapiPickerWindow))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
			.SetIcon(FSlateIcon(FAkAudioStyle::GetStyleSetName(), "AudiokineticTools.AkPickerTabIcon"));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SWwisePicker::WwisePickerTabName, FOnSpawnTab::CreateRaw(this, &FAudiokineticToolsModule::CreateWwisePickerWindow))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
			.SetIcon(FSlateIcon(FAkAudioStyle::GetStyleSetName(), "AudiokineticTools.AkPickerTabIcon"));

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		OnAssetRegistryFilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAudiokineticToolsModule::OnAssetRegistryFilesLoaded);

		LateRegistrationOfMatineeToLevelSequencerHandle = AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAudiokineticToolsModule::LateRegistrationOfMatineeToLevelSequencer);

		FEditorDelegates::EndPIE.AddRaw(this, &FAudiokineticToolsModule::OnEndPIE);

		// Since we are initialized in the PostEngineInit phase, our Ambient Sound actor factory is not registered. We need to register it ourselves.
		if (GEditor)
		{
			if (auto NewFactory = NewObject<UActorFactoryAkAmbientSound>())
			{
				GEditor->ActorFactories.Add(NewFactory);
			}
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UAkSurfaceReflectorSetComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAkSurfaceReflectorSetDetailsCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout(UAkLateReverbComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAkLateReverbComponentDetailsCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout(UAkRoomComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAkRoomComponentDetailsCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout(UAkGeometryComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FAkGeometryComponentDetailsCustomization::MakeInstance));

		if (auto akSettings = GetDefault<UAkSettings>())
		{
			akSettings->OnActivatedNewAssetManagement.AddLambda([this]() {
				OnActivatedNewAssetManagement();
			});

			akSettings->OnSoundDataFolderChanged.AddLambda([](const FString& OldBasePackagePath, const FString& NewBasePackagePath) {
				AkAssetDatabase::Get().MoveAllAssets(OldBasePackagePath, NewBasePackagePath);
			});

			if (akSettings->UseEventBasedPackaging || akSettings->EnableAutomaticAssetSynchronization)
			{
				assetManagementManager.Init();
			}
		}

		AkAssetDatabase::Get();
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			auto& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

			for (auto AkAssetTypeActions : AkAssetTypeActionsArray)
				if (AkAssetTypeActions.IsValid())
					AssetTools.UnregisterAssetTypeActions(AkAssetTypeActions.ToSharedRef());
		}

		AkAssetTypeActionsArray.Empty();

		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			auto& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders().RemoveAll([=](const FLevelEditorModule::FLevelEditorMenuExtender& Extender)
			{
				return Extender.GetHandle() == LevelViewportToolbarBuildMenuExtenderAkHandle;
			});

			if (MainMenuExtender.IsValid())
			{
				LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MainMenuExtender);
			}
		}
		LevelViewportToolbarBuildMenuExtenderAkHandle.Reset();

		UnregisterSettings();

		if (GUnrealEd != NULL)
		{
			GUnrealEd->UnregisterComponentVisualizer(UAkComponent::StaticClass()->GetFName());
		}

		FGlobalTabmanager::Get()->UnregisterTabSpawner(SWaapiPicker::WaapiPickerTabName);
		FGlobalTabmanager::Get()->UnregisterTabSpawner(SWwisePicker::WwisePickerTabName);

#if !UE_4_25_OR_LATER
		auto MatineeToLevelSequenceModule = FModuleManager::GetModulePtr<IMatineeToLevelSequenceModule>(TEXT("MatineeToLevelSequence"));
		if (0 && MatineeToLevelSequenceModule)
		{
			// Currently, UnregisterTrackConverterForMatineeClass crashes
			MatineeToLevelSequenceModule->UnregisterTrackConverterForMatineeClass(ConvertMatineeRTPCTrackHandle);
			MatineeToLevelSequenceModule->UnregisterTrackConverterForMatineeClass(ConvertMatineeEventTrackHandle);
		}
#endif

		if (FModuleManager::Get().IsModuleLoaded(TEXT("Sequencer")))
		{
			auto& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>(TEXT("Sequencer"));
			SequencerModule.UnRegisterTrackEditor(RTPCTrackEditorHandle);
			SequencerModule.UnRegisterTrackEditor(EventTrackEditorHandle);
		}

		FEditorDelegates::EndPIE.RemoveAll(this);

		// Only found way to close the tab in the case of a hot-reload. We need a pointer to the DockTab, and the only way of getting it seems to be InvokeTab.
		if (GUnrealEd && !GUnrealEd->IsPendingKill())
		{
#if UE_4_26_OR_LATER
			auto WaapiPickerTab = FGlobalTabmanager::Get()->TryInvokeTab(SWaapiPicker::WaapiPickerTabName);
			if (WaapiPickerTab.IsValid())
			{
				WaapiPickerTab->RequestCloseTab();
			}

			auto WwisePickerTab = FGlobalTabmanager::Get()->TryInvokeTab(SWwisePicker::WwisePickerTabName);
			if (WwisePickerTab.IsValid())
			{
				WwisePickerTab->RequestCloseTab();
			}
#else
			FGlobalTabmanager::Get()->InvokeTab(SWaapiPicker::WaapiPickerTabName)->RequestCloseTab();
			FGlobalTabmanager::Get()->InvokeTab(SWwisePicker::WwisePickerTabName)->RequestCloseTab();
#endif
		}

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SWaapiPicker::WaapiPickerTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SWwisePicker::WwisePickerTabName);

		if (UObjectInitialized())
		{
			FComponentAssetBrokerage::UnregisterBroker(AkEventBroker);
		}

		auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UAkSurfaceReflectorSetComponent::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UAkLateReverbComponent::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UAkRoomComponent::StaticClass()->GetFName());

		assetManagementManager.Uninit();
	}

	/**
	* Extends the Build context menu with Audiokinetic-specific menu items
	*/
	TSharedRef<FExtender> ExtendBuildContextMenuForAudiokinetic(const TSharedRef<FUICommandList> CommandList)
	{
		TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddMenuExtension("LevelEditorGeometry", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Audiokinetic", LOCTEXT("Audiokinetic", "Audiokinetic"));
			{
				FUIAction GenerateSoundDataUIAction;
				GenerateSoundDataUIAction.ExecuteAction.BindStatic(&AkAudioBankGenerationHelper::CreateGenerateSoundDataWindow, (TArray<TWeakObjectPtr<UAkAudioBank>>*)nullptr, false);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AkAudioBank_GenerateSoundBanks", "Generate Sound Data..."),
					LOCTEXT("AkAudioBank_GenerateSoundBanksTooltip", "Generates Wwise Sound Data."),
					FSlateIcon(),
					GenerateSoundDataUIAction
				);

				FUIAction ClearSoundDataUIAction;
				ClearSoundDataUIAction.ExecuteAction.BindStatic(&AkAudioBankGenerationHelper::CreateClearSoundDataWindow);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AkAudioBank_ClearSoundData", "Clear Sound Data..."),
					LOCTEXT("AkAudioBank_ClearSoundDataTooltip", "Clear Wwise Sound Data."),
					FSlateIcon(),
					ClearSoundDataUIAction
				);

				if (AkUnrealHelper::IsUsingEventBased())
				{
					FUIAction ForceAssetSynchronizationUIAction;
					ForceAssetSynchronizationUIAction.ExecuteAction.BindRaw(&assetManagementManager, &AkAssetManagementManager::DoAssetSynchronization);

					MenuBuilder.AddMenuEntry(
						LOCTEXT("AkAudioMenu_ForceAssetSynchronization", "Force Asset Synchronization"),
						LOCTEXT("AkAudioMenu_ForceAssetSynchronizationTooltip", "Force synchronization of assets from the Wwise project by parsing the work units"),
						FSlateIcon(),
						ForceAssetSynchronizationUIAction
					);
				}
			}
			MenuBuilder.EndSection();

		}));
		return Extender.ToSharedRef();
	}

	static EEditorBuildResult BuildAkEventData(UWorld* world, FName name)
	{
		auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (!AssetRegistryModule.Get().IsLoadingAssets())
		{
			CookAkSoundDataTask::ExecuteForEditorPlatform();
			return EEditorBuildResult::InProgress;
		}
		else
		{
			return EEditorBuildResult::Skipped;
		}
	}

private:
	struct SettingsRegistrationStruct
	{
		SettingsRegistrationStruct(UClass* SettingsClass, const FName& SectionName, const FText& DisplayName, const FText& Description)
			: SettingsClass(SettingsClass), SectionName(SectionName), DisplayName(DisplayName), Description(Description)
		{}

		void Register(ISettingsModule* SettingsModule) const
		{
			SettingsModule->RegisterSettings("Project", "Wwise", SectionName, DisplayName, Description, SettingsObject());
		}

		void Unregister(ISettingsModule* SettingsModule) const
		{
			SettingsModule->UnregisterSettings("Project", "Wwise", SectionName);
		}

	private:
		UClass* SettingsClass;
		const FName SectionName;
		const FText DisplayName;
		const FText Description;

		UObject* SettingsObject() const { return SettingsClass->GetDefaultObject(); }
	};

	static TMap<FString, SettingsRegistrationStruct>& GetWwisePlatformNameToSettingsRegistrationMap()
	{
		static TMap<FString, SettingsRegistrationStruct> WwisePlatformNameToWwiseSettingsRegistrationMap;
		if (WwisePlatformNameToWwiseSettingsRegistrationMap.Num() == 0)
		{
			auto RegisterIntegrationSettings = SettingsRegistrationStruct(UAkSettings::StaticClass(),
				"Integration",
				LOCTEXT("WwiseIntegrationSettingsName", "Integration Settings"),
				LOCTEXT("WwiseIntegrationSettingsDescription", "Configure the Wwise Integration"));

			auto RegisterPerUserSettings = SettingsRegistrationStruct(UAkSettingsPerUser::StaticClass(),
				"User Settings",
				LOCTEXT("WwiseRuntimePerUserSettingsName", "User Settings"),
				LOCTEXT("WwiseRuntimePerUserSettingsDescription", "Configure the Wwise Integration per user"));

			WwisePlatformNameToWwiseSettingsRegistrationMap.Add(FString("Integration"), RegisterIntegrationSettings);
			WwisePlatformNameToWwiseSettingsRegistrationMap.Add(FString("User"), RegisterPerUserSettings);

			for (const auto& AvailablePlatform : AkUnrealPlatformHelper::GetAllSupportedUnrealPlatforms())
			{
				FString SettingsClassName = FString::Format(TEXT("Ak{0}InitializationSettings"), { *AvailablePlatform });
				UClass* SettingsClass = FindObject<UClass>(ANY_PACKAGE, *SettingsClassName);
				if (SettingsClass)
				{
					FString CategoryNameKey = FString::Format(TEXT("Wwise{0}SettingsName"), { *AvailablePlatform });
					FString DescriptionNameKey = FString::Format(TEXT("Wwise{0}SettingsDescription"), { *AvailablePlatform });
					FString DescriptionText = FString::Format(TEXT("Configure the Wwise {0} Initialization Settings"), { *AvailablePlatform });
					auto PlatformNameText = FText::FromString(*AvailablePlatform);
					auto RegisterPlatform = SettingsRegistrationStruct(SettingsClass, FName(*AvailablePlatform),
						PlatformNameText,
						FText::Format(LOCTEXT("WwiseSettingsDescription", "Configure the Wwise {0} Initialization Settings"), PlatformNameText));
					WwisePlatformNameToWwiseSettingsRegistrationMap.Add(*AvailablePlatform, RegisterPlatform);
				}
			}
		}
		return WwisePlatformNameToWwiseSettingsRegistrationMap;
	}

	TSet<FString> RegisteredSettingsNames;

	void RegisterSettings()
	{
		if (auto SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			auto UpdatePlatformSettings = [SettingsModule, this]
			{
				auto SettingsRegistrationMap = GetWwisePlatformNameToSettingsRegistrationMap();

				TSet<FString> SettingsThatShouldBeRegistered = { FString("Integration"), FString("User") };
				
				for (const auto& AvailablePlatform : AkUnrealPlatformHelper::GetAllSupportedUnrealPlatformsForProject())
				{
					if (SettingsRegistrationMap.Contains(AvailablePlatform))
					{
						SettingsThatShouldBeRegistered.Add(AvailablePlatform);
					}
				}

				auto SettingsToBeUnregistered = RegisteredSettingsNames.Difference(SettingsThatShouldBeRegistered);
				for (const auto& SettingsName : SettingsToBeUnregistered)
				{
					SettingsRegistrationMap[SettingsName].Unregister(SettingsModule);
					RegisteredSettingsNames.Remove(SettingsName);
				}

				auto SettingsToBeRegistered = SettingsThatShouldBeRegistered.Difference(RegisteredSettingsNames);
				for (const auto& SettingsName : SettingsToBeRegistered)
				{
					if (RegisteredSettingsNames.Contains(SettingsName))
						continue;

					SettingsRegistrationMap[SettingsName].Register(SettingsModule);
					RegisteredSettingsNames.Add(SettingsName);
				}
			};

			UpdatePlatformSettings();

			IProjectManager& ProjectManager = IProjectManager::Get();
			ProjectManager.OnTargetPlatformsForCurrentProjectChanged().AddLambda(UpdatePlatformSettings);
		}
	}

	void UnregisterSettings()
	{
		if (auto SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			auto SettingsRegistrationMap = GetWwisePlatformNameToSettingsRegistrationMap();
			for (const auto& SettingsName : RegisteredSettingsNames)
			{
				SettingsRegistrationMap[SettingsName].Unregister(SettingsModule);
			}
			RegisteredSettingsNames.Empty();
		}
	}

	void OnEndPIE(const bool bIsSimulating)
	{
		if (auto* AkAudioDevice = FAkAudioDevice::Get())
		{
			AkAudioDevice->StopAllSounds(true);
		}
	}

	TArray<TSharedPtr<FAssetTypeActions_Base>> AkAssetTypeActionsArray;
	TSharedPtr<FExtender> MainMenuExtender;
	FLevelEditorModule::FLevelEditorMenuExtender LevelViewportToolbarBuildMenuExtenderAk;
	FDelegateHandle LevelViewportToolbarBuildMenuExtenderAkHandle;
	FDelegateHandle OnAssetRegistryFilesLoadedHandle;
	FDelegateHandle LateRegistrationOfMatineeToLevelSequencerHandle;
	FDelegateHandle RTPCTrackEditorHandle;
	FDelegateHandle EventTrackEditorHandle;
	FDelegateHandle ConvertMatineeRTPCTrackHandle;
	FDelegateHandle ConvertMatineeEventTrackHandle;

	/** Allow to create an AkComponent when Drag & Drop of an AkEvent */
	TSharedPtr<IComponentAssetBroker> AkEventBroker;

	TSharedPtr<SWaapiPicker> AkWaapiPicker;
	TSharedPtr<SWwisePicker> AkWwisePicker;

	FDoEditorBuildDelegate buildDelegate;
	AkAssetManagementManager assetManagementManager;
};

IMPLEMENT_MODULE(FAudiokineticToolsModule, AudiokineticTools);

#undef LOCTEXT_NAMESPACE
