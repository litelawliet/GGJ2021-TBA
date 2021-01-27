// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	AkAudioBankGenerationHelpers.cpp: Wwise Helpers to generate banks from the editor and when cooking.
------------------------------------------------------------------------------------*/

#include "AkAudioBankGenerationHelpers.h"

#include "AkAudioBank.h"
#include "AkAudioType.h"
#include "AkMediaAsset.h"
#include "AkSettings.h"
#include "AkSettingsPerUser.h"
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "Editor/UnrealEd/Public/ObjectTools.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Slate/Public/Framework/Application/SlateApplication.h"
#include "Slate/Public/Widgets/Input/SButton.h"
#include "Slate/Public/Widgets/Layout/SSpacer.h"
#include "Slate/Public/Widgets/Text/STextBlock.h"
#include "SlateCore/Public/Widgets/SBoxPanel.h"
#include "SlateCore/Public/Widgets/SWindow.h"
#include "ToolBehavior/AkToolBehavior.h"
#include "UI/SClearSoundData.h"
#include "UObject/UObjectIterator.h"
#include "WwiseProject/WwiseProjectInfo.h"

#define LOCTEXT_NAMESPACE "AkAudio"

DEFINE_LOG_CATEGORY(LogAkSoundData);

namespace AkAudioBankGenerationHelper
{
	FString GetWwiseConsoleApplicationPath()
	{
		const UAkSettingsPerUser* AkSettingsPerUser = GetDefault<UAkSettingsPerUser>();
		FString ApplicationToRun;
		ApplicationToRun.Empty();

		if (AkSettingsPerUser)
		{
#if PLATFORM_WINDOWS
			ApplicationToRun = AkSettingsPerUser->WwiseWindowsInstallationPath.Path;
#else
			ApplicationToRun = AkSettingsPerUser->WwiseMacInstallationPath.FilePath;
#endif
			if (FPaths::IsRelative(ApplicationToRun))
			{
				ApplicationToRun = FPaths::ConvertRelativePathToFull(AkUnrealHelper::GetProjectDirectory(), ApplicationToRun);
			}
			if (!(ApplicationToRun.EndsWith(TEXT("/")) || ApplicationToRun.EndsWith(TEXT("\\"))))
			{
				ApplicationToRun += TEXT("/");
			}

#if PLATFORM_WINDOWS
			if (FPaths::FileExists(ApplicationToRun + TEXT("Authoring/x64/Release/bin/WwiseConsole.exe")))
			{
				ApplicationToRun += TEXT("Authoring/x64/Release/bin/WwiseConsole.exe");
			}
			else
			{
				ApplicationToRun += TEXT("Authoring/Win32/Release/bin/WwiseConsole.exe");
			}
			ApplicationToRun.ReplaceInline(TEXT("/"), TEXT("\\"));
#elif PLATFORM_MAC
			ApplicationToRun += TEXT("Contents/Tools/WwiseConsole.sh");
			ApplicationToRun = TEXT("\"") + ApplicationToRun + TEXT("\"");
#endif
		}

		return ApplicationToRun;
	}

	void CreateGenerateSoundDataWindow(TArray<TWeakObjectPtr<UAkAudioBank>>* SoundBanks, bool ProjectSave)
	{
		auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			return;
		}

		TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
			.Title(LOCTEXT("AkAudioGenerateSoundData", "Generate Sound Data"))
			.ClientSize(FVector2D(600.f, 332.f))
			.SupportsMaximize(false).SupportsMinimize(false)
			.SizingRule(ESizingRule::FixedSize)
			.FocusWhenFirstShown(true);

		if (!AkToolBehavior::Get()->CreateSoundDataWidget(WidgetWindow, SoundBanks, ProjectSave))
		{
			return;
		}

		// This creates a windows that blocks the rest of the UI. You can only interact with the "Generate SoundBanks" window.
		// If you choose to use this, comment the rest of the function.
		//GEditor->EditorAddModalWindow(WidgetWindow);

		// This creates a window that still allows you to interact with the rest of the editor. If there is an attempt to delete
		// a UAkAudioBank (from the content browser) while this window is opened, the editor will generate a (cryptic) error message
		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		if (ParentWindow.IsValid())
		{
			// Parent the window to the main frame 
			FSlateApplication::Get().AddWindowAsNativeChild(WidgetWindow, ParentWindow.ToSharedRef());
		}
		else
		{
			// Spawn new window
			FSlateApplication::Get().AddWindow(WidgetWindow);
		}
	}

	void CreateClearSoundDataWindow()
	{
		auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			return;
		}

		TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
			.Title(LOCTEXT("AkAudioGenerateSoundData", "Clear Sound Data"))
			.SupportsMaximize(false).SupportsMinimize(false)
			.SizingRule(ESizingRule::Autosized)
			.FocusWhenFirstShown(true);

		TSharedRef<SClearSoundData> WindowContent = SNew(SClearSoundData);

		// Add our SClearSoundData to the window
		WidgetWindow->SetContent(WindowContent);

		// Set focus to our SClearSoundData widget, so our keyboard keys work right off the bat
		WidgetWindow->SetWidgetToFocusOnActivate(WindowContent);

		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		if (ParentWindow.IsValid())
		{
			// Parent the window to the main frame 
			FSlateApplication::Get().AddWindowAsNativeChild(WidgetWindow, ParentWindow.ToSharedRef());
		}
		else
		{
			// Spawn new window
			FSlateApplication::Get().AddWindow(WidgetWindow);
		}
	}

	void DoClearSoundData(AkSoundDataClearFlags ClearFlags)
	{
		if (!AkUnrealHelper::IsUsingEventBased())
		{
			return;
		}

		WwiseProjectInfo wwiseProjectInfo;
		wwiseProjectInfo.Parse();

		auto start = FPlatformTime::Cycles64();

		FScopedSlowTask SlowTask(0.f, LOCTEXT("AK_ClearingSoundData", "Clearing Wwise Sound Data..."));
		SlowTask.MakeDialog();

		TArray<FString> clearCommands;

		if ((ClearFlags & AkSoundDataClearFlags::AssetData) == AkSoundDataClearFlags::AssetData)
		{
			if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
			{
				AkAudioDevice->UnloadAllSoundData();
			}

			for (TObjectIterator<UAkAudioType> akAssetIt; akAssetIt; ++akAssetIt)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Clearing data from asset {0}"), FText::FromString(akAssetIt->GetName())));
				akAssetIt->Reset();
			}

			for (TObjectIterator<UAkMediaAsset> mediaIt; mediaIt; ++mediaIt)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Clearing data from asset {0}"), FText::FromString(mediaIt->GetName())));
				mediaIt->Reset();
			}

			for (TObjectIterator<UAkAssetPlatformData> platformDataIt; platformDataIt; ++platformDataIt)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Clearing data from asset {0}"), FText::FromString(platformDataIt->GetName())));
				platformDataIt->Reset();
			}

			clearCommands.Add(TEXT("Asset Data"));
		}

		if ((ClearFlags & AkSoundDataClearFlags::SoundBankInfoCache) == AkSoundDataClearFlags::SoundBankInfoCache)
		{
			SlowTask.EnterProgressFrame(0.f, LOCTEXT("AK_ClearSoundBankInfoCache", "Clearing SoundBankInfoCache.dat"));

			auto soundBankInfoCachePath = FPaths::Combine(wwiseProjectInfo.CacheDirectory(), TEXT("SoundBankInfoCache.dat"));
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*soundBankInfoCachePath);

			clearCommands.Add(TEXT("SoundBank Info Cache"));
		}

		if ((ClearFlags & AkSoundDataClearFlags::MediaCache) == AkSoundDataClearFlags::MediaCache)
		{
			auto cacheFolderPath = wwiseProjectInfo.CacheDirectory();
			TArray<FString> foldersInCache;

			auto& platformFile = FPlatformFileManager::Get().GetPlatformFile();
			platformFile.IterateDirectory(*cacheFolderPath, [&foldersInCache](const TCHAR* Path, bool IsDir) {
				if (IsDir)
				{
					foldersInCache.Add(Path);
				}

				return true;
			});

			for (auto& folder : foldersInCache)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Clearing media cache {0}"), FText::FromString(folder)));

				platformFile.DeleteDirectoryRecursively(*folder);
			}

			clearCommands.Add(TEXT("Media Cache"));
		}

		if ((ClearFlags & AkSoundDataClearFlags::ExternalSource) == AkSoundDataClearFlags::ExternalSource)
		{
			auto externalSourceFolder = AkUnrealHelper::GetExternalSourceDirectory();
			TArray<FString> foldersInExternalSource;

			auto& platformFile = FPlatformFileManager::Get().GetPlatformFile();
			platformFile.IterateDirectory(*externalSourceFolder, [&foldersInExternalSource](const TCHAR* Path, bool IsDir) {
				if (IsDir)
				{
					foldersInExternalSource.Add(Path);
				}

				return true;
			});

			for (auto& folder : foldersInExternalSource)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Clearing external source cache folder {0}"), FText::FromString(folder)));

				platformFile.DeleteDirectoryRecursively(*folder);
			}

			clearCommands.Add(TEXT("External Source"));
		}

		if ((ClearFlags & AkSoundDataClearFlags::OrphanMedia) == AkSoundDataClearFlags::OrphanMedia)
		{
			auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

			TArray<FAssetData> mediaToDelete;

			for (TObjectIterator<UAkMediaAsset> mediaIt; mediaIt; ++mediaIt)
			{
				SlowTask.EnterProgressFrame(0.f, FText::FormatOrdered(LOCTEXT("AK_ClearAssetData", "Checking if media asset {0} is an orphan"), FText::FromString(mediaIt->GetName())));

				if (mediaIt->IsA<UAkExternalMediaAsset>())
				{
					continue;
				}

				TArray<FName> dependencies;

#if UE_4_26_OR_LATER
				AssetRegistryModule.Get().GetReferencers(FName(*mediaIt->GetOuter()->GetPathName()), dependencies, UE::AssetRegistry::EDependencyCategory::All);
#else
				AssetRegistryModule.Get().GetReferencers(FName(*mediaIt->GetOuter()->GetPathName()), dependencies, EAssetRegistryDependencyType::All);
#endif

				if (dependencies.Num() == 0)
				{
					mediaToDelete.Emplace(*mediaIt);
				}
			}

			if (mediaToDelete.Num() > 0)
			{
				clearCommands.Add(TEXT("Orphan Media"));
				ObjectTools::DeleteAssets(mediaToDelete, false);
			}
		}

		auto end = FPlatformTime::Cycles64();

		UE_LOG(LogAkSoundData, Display, TEXT("Clear Wwise Sound Data(%s) took %f seconds."), *FString::Join(clearCommands, TEXT(", ")), FPlatformTime::ToSeconds64(end - start));
	}
}

#undef LOCTEXT_NAMESPACE
