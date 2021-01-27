// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#include "AkUnrealHelper.h"

#include "AkSettings.h"
#include "AkUEFeatures.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Platforms/AkUEPlatform.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Docking/SDockTab.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "ISourceControlModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "HAL/PlatformFileManager.h"
#endif

namespace AkUnrealHelper
{
	const TCHAR* MediaFolderName = TEXT("Media");
	const TCHAR* ExternalSourceFolderName = TEXT("ExternalSources");
	constexpr auto SoundBankNamePrefix = TEXT("SB_");
	const FGuid InitBankID(0x701ECBBD, 0x9C7B4030, 0x8CDB749E, 0xE5D1C7B9);

	void TrimPath(FString& Path)
	{
		Path.TrimStartAndEndInline();
	}

	FString GetProjectDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	FString GetWwisePluginDirectory()
	{
		return FAkPlatform::GetWwisePluginDirectory();
	}

	FString GetContentDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	}

	FString GetThirdPartyDirectory()
	{
		return FPaths::Combine(GetWwisePluginDirectory(), TEXT("ThirdParty"));
	}

	FString GetSoundBankDirectory()
	{
		if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
		{
			return FPaths::Combine(GetContentDirectory(), AkSettings->WwiseSoundDataFolder.Path);
		}
		else
		{
			return FPaths::Combine(GetContentDirectory(), UAkSettings::DefaultSoundDataFolder);
		}
	}

	FString GetExternalSourceDirectory()
	{
		return FPaths::Combine(GetSoundBankDirectory(), ExternalSourceFolderName);
	}

	FString GetBaseAssetPackagePath()
	{
		if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
		{
			return FPaths::Combine(TEXT("/Game/"), AkSettings->WwiseSoundDataFolder.Path);
		}

		return TEXT("/Game/WwiseAudio");
	}

	FString GetLocalizedAssetPackagePath()
	{
		return FPaths::Combine(GetBaseAssetPackagePath(), TEXT("Localized"));
	}

	FString GetExternalSourceAssetPackagePath()
	{
		return FPaths::Combine(GetBaseAssetPackagePath(), ExternalSourceFolderName);
	}

	FString GetWwiseProjectPath()
	{
		FString projectPath;

		if (auto* settings = GetDefault<UAkSettings>())
		{
			projectPath = settings->WwiseProjectPath.FilePath;

			if (FPaths::IsRelative(projectPath))
			{
				projectPath = FPaths::ConvertRelativePathToFull(GetProjectDirectory(), projectPath);
			}

#if PLATFORM_WINDOWS
			projectPath.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
		}

		return projectPath;
	}

	FString GetWwiseSoundBankInfoCachePath()
	{
		return FPaths::Combine(FPaths::GetPath(AkUnrealHelper::GetWwiseProjectPath()), TEXT(".cache"), TEXT("SoundBankInfoCache.dat"));
	}

	FString GetGeneratedSoundBanksFolder()
	{
		if (IsUsingEventBased())
		{
			return FPaths::Combine(FPaths::GetPath(GetWwiseProjectPath()), TEXT("GeneratedSoundBanks"));
		}
		
		return GetSoundBankDirectory();
	}

	bool IsUsingEventBased()
	{
		static struct UsingAssetManagementCache
		{
			UsingAssetManagementCache()
			{
				if (auto AkSettings = GetDefault<UAkSettings>())
				{
					UseEventBasedPackaging = AkSettings->UseEventBasedPackaging;
				}
			}

			bool UseEventBasedPackaging = false;
		} AssetManagementCache;

		return AssetManagementCache.UseEventBasedPackaging;
	}

#if WITH_EDITOR
	void SanitizePath(FString& Path, const FString& PreviousPath, const FText& DialogMessage)
	{
		AkUnrealHelper::TrimPath(Path);

		FText FailReason;
		if (!FPaths::ValidatePath(Path, &FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
			Path = PreviousPath;
		}

		const FString TempPath = FPaths::IsRelative(Path) ? FPaths::ConvertRelativePathToFull(AkUnrealHelper::GetProjectDirectory(), Path) : Path;
		if (!FPaths::DirectoryExists(TempPath))
		{
			FMessageDialog::Open(EAppMsgType::Ok, DialogMessage);
			Path = PreviousPath;
		}
	}

	void SanitizeProjectPath(FString& Path, const FString& PreviousPath, const FText& DialogMessage, bool &bRequestRefresh)
	{
		AkUnrealHelper::TrimPath(Path);

		FString TempPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path);

		FText FailReason;
		if (!FPaths::ValidatePath(TempPath, &FailReason))
		{
			if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, FailReason))
			{
				Path = PreviousPath;
				return;
			}
		}

		auto ProjectDirectory = AkUnrealHelper::GetProjectDirectory();
		if (!FPaths::FileExists(TempPath))
		{
			// Path might be a valid one (relative to game) entered manually. Check that.
			TempPath = FPaths::ConvertRelativePathToFull(ProjectDirectory, Path);

			if (!FPaths::FileExists(TempPath))
			{
				if (EAppReturnType::Ok == FMessageDialog::Open(EAppMsgType::Ok, DialogMessage))
				{
					Path = PreviousPath;
					return;
				}
			}
		}

		// Make the path relative to the game dir
		FPaths::MakePathRelativeTo(TempPath, *ProjectDirectory);
		Path = TempPath;

		if (Path != PreviousPath)
		{
#if UE_4_26_OR_LATER
			auto WaapiPickerTab = FGlobalTabmanager::Get()->TryInvokeTab(FName("WaapiPicker"));
			auto WwisePickerTab = FGlobalTabmanager::Get()->TryInvokeTab(FName("WwisePicker"));
#else
			TSharedRef<SDockTab> WaapiPickerTab = FGlobalTabmanager::Get()->InvokeTab(FName("WaapiPicker"));
			TSharedRef<SDockTab> WwisePickerTab = FGlobalTabmanager::Get()->InvokeTab(FName("WwisePicker"));
#endif
			bRequestRefresh = true;
		}
	}

	void SaveConfigFile(UObject* ConfigObject)
	{
		FString ConfigFilename = ConfigObject->GetDefaultConfigFilename();
		if (!ISourceControlModule::Get().IsEnabled() || SettingsHelpers::IsCheckedOut(ConfigFilename) || SettingsHelpers::CheckOutOrAddFile(ConfigFilename))
		{
			ConfigObject->UpdateDefaultConfigFile();
		}
	}

	FString GuidToBankName(const FGuid& Guid)
	{
		if (Guid == InitBankID)
		{
			return TEXT("Init");
		}

		return FString(SoundBankNamePrefix) + Guid.ToString(EGuidFormats::Digits);
	}

	FGuid BankNameToGuid(const FString& BankName)
	{
		FString copy = BankName;
		copy.RemoveFromStart(SoundBankNamePrefix);

		FGuid result;
		FGuid::ParseExact(copy, EGuidFormats::Digits, result);

		return result;
	}

#define LOCTEXT_NAMESPACE "AkAudio"
	void ShowEventBasedPackagingMigrationDialog(FOnButtonClickedMigration in_OnclickedYes, FOnButtonClickedMigration in_OnclickedNo)
	{
		TSharedPtr<SWindow> Dialog = SNew(SWindow)
			.Title(LOCTEXT("NewAssetManagementTitle", "New Asset Management"))
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.FocusWhenFirstShown(true)
			.SizingRule(ESizingRule::Autosized);

		TSharedPtr<SVerticalBox> ContentVerticalBox;
		TSharedRef<SWidget> DialogContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(ContentVerticalBox, SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(0.15f)
				[
					SNew(SSpacer)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NewAssetManagementMessageBegin",
						"New in Wwise 2019.2: The Wwise Unreal Integration uses a new asset management system called \"Event-based Packaging.\"\n"
						"Unreal assets are now automatically synchronized from the Wwise project, and manually managing SoundBanks is not needed anymore.\n"
					))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("AkMigration_WhatHappens", "Changes made to your project during the migration process"))
					.InitiallyCollapsed(true)
					.BodyContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NewAssetManagementMessageWhatHappens",
							"- This new workflow is greatly improved by using a WAAPI connection. Therefore, WAAPI will be enabled. It is recommended to\n"
							"open the Wwise project in the Wwise Authoring Application after completing the migration process.\n"
							"- Wwise-related assets will be moved within the content browser. Assets not found in the Wwise project will be deleted. It is\n"
							"strongly recommended to back up your Unreal project, or to use source control.\n"
							"- One SoundBank is created for each event, and SoundBank data is directly embedded inside the AkAudioEvent assets.\n"
							"- Leftover SoundBanks (.bnk) and media files (.wem) will be deleted from disk, as they are no longer needed. Ensure to check.\n"
							"them out of source control."
						))
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NewAssetManagementMessageOptOut", "\nIt is possible to opt-in to the new workflow at a later time, through the Integration Settings. Note that some changes will be done to your\nWwise and Unreal project even if you opt-out of \"Event-based Packaging\".\n"))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(SHyperlink)
					.Text(LOCTEXT("NewAssetManagementMessageLink", "For more information, please refer to the integration documentation"))
					.OnNavigate_Lambda([=]() { FPlatformProcess::LaunchURL(TEXT("https://www.audiokinetic.com/library/edge/?source=UE4&id=releasenotes.html"), nullptr, nullptr); })
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NewAssetManagementMessageEnd", "\nWould you like to enable Event-based Packaging?"))
					.AutoWrapText(true)
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.10f)
		[
			SNew(SSpacer)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		[
			SNew(SCheckBox)
			.IsChecked(ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) { GetMutableDefault<UAkSettings>()->FixupRedirectorsDuringMigration = NewState == ECheckBoxState::Checked; })
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AkFixupRedirectors", "Fixup redirectors during migration"))
				.ToolTipText(LOCTEXT("AkFixupRedirectorsToolTip", "After all Wwise-related assets have been moved to their new destination, fixup the redirectors left behind."))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 3.0f, 0.0f, 3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Yes", "Yes"))
				.OnClicked_Lambda([&Dialog, &in_OnclickedYes]() -> FReply {
					Dialog->RequestDestroyWindow();
					return in_OnclickedYes.Execute();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 3.0f, 0.0f, 3.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("No", "No"))
				.OnClicked_Lambda([&Dialog, &in_OnclickedNo]() -> FReply {
					Dialog->RequestDestroyWindow();
					return in_OnclickedNo.Execute();
				})
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.05f)
		[
			SNew(SSpacer)
		];

		if (!ISourceControlModule::Get().IsEnabled())
		{
			ContentVerticalBox->AddSlot()
				.AutoHeight()
				.Padding(2.0f)
				.HAlign(EHorizontalAlignment::HAlign_Left)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NewAssetManagementMessageSourceControlWarning", "\nWARNING: It is strongly recommended to enable Source Control in your project before proceeding with migration."))
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor::Yellow)
				];
		}

		Dialog->SetContent(DialogContent);
		FSlateApplication::Get().AddModalWindow(Dialog.ToSharedRef(), nullptr);
	}

	void DeleteOldSoundBanks()
	{
		const TArray<FString> ExtensionsToDelete = { "bnk", "wem", "json", "txt", "xml" };
		bool SuccessfulDelete = true;
		for (auto& Extension : ExtensionsToDelete)
		{
			TArray<FString> FoundFiles;
			FPlatformFileManager::Get().GetPlatformFile().FindFilesRecursively(FoundFiles, *AkUnrealHelper::GetSoundBankDirectory(), *Extension);
			for (auto& File : FoundFiles)
			{
				SuccessfulDelete = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*File);
			}
		}

		if (!SuccessfulDelete)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CannotDeleteOldBanks", "Unable to delete legacy SoundBank files. Please ensure to manually delete them after migration is complete."));
		}
	}
#undef LOCTEXT_NAMESPACE
#endif
}
