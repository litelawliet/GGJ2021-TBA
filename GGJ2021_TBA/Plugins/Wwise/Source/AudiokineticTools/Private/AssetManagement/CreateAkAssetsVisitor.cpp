#include "CreateAkAssetsVisitor.h"

#include "AkAcousticTexture.h"
#include "AkAssetDatabase.h"
#include "AkAudioBank.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"
#include "AkInitBank.h"
#include "AkMediaAsset.h"
#include "AkRtpc.h"
#include "AkSettings.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "AkUnrealHelper.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "UnrealEd/Public/FileHelpers.h"
#include "UnrealEd/Public/ObjectTools.h"
#include "UnrealEd/Public/PackageTools.h"

#define LOCTEXT_NAMESPACE "AkAudio"

void CreateAkAssetsVisitor::OnBeginParse()
{
	AkAssetDatabase::Get().Init();
}

void CreateAkAssetsVisitor::EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkAudioEvent>(Id, Name, Name, RelativePath);
}

void CreateAkAssetsVisitor::EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkAcousticTexture>(Id, Name, Name, RelativePath);
}

void CreateAkAssetsVisitor::EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkAuxBus>(Id, Name, Name, RelativePath);
}

void CreateAkAssetsVisitor::EnterStateGroup(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	currentStateGroupId = Id;
	currentStateGroupName = Name;
}

void CreateAkAssetsVisitor::EnterState(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkStateValue>(Id, Name, FString::Printf(TEXT("%s-%s"), *currentStateGroupName, *Name), RelativePath, currentStateGroupId);
}

void CreateAkAssetsVisitor::EnterSwitchGroup(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	currentSwitchGroupId = Id;
	currentSwitchGroupName = Name;
}

void CreateAkAssetsVisitor::EnterSwitch(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkSwitchValue>(Id, Name, FString::Printf(TEXT("%s-%s"), *currentSwitchGroupName, *Name), RelativePath, currentSwitchGroupId);
}

void CreateAkAssetsVisitor::EnterGameParameter(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkRtpc>(Id, Name, Name, RelativePath);
}

void CreateAkAssetsVisitor::EnterTrigger(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createAsset<UAkTrigger>(Id, Name, Name, RelativePath);
}

void CreateAkAssetsVisitor::End()
{
	auto& assetDatabase = AkAssetDatabase::Get();

	if (doSave)
	{
		if (auto* InitBank = assetDatabase.InitBank)
		{
			if (auto Outermost = InitBank->GetOutermost())
			{
				if (Outermost->IsDirty())
				{
					packagesToSave.Add(Outermost);
				}
			}
		}

		if (packagesToSave.Num() > 0)
		{
			FScopedSlowTask SlowTask(0.f, LOCTEXT("AK_SaveNewAkAssets", "Saving new sound data assets..."));
			SlowTask.MakeDialog();
			UEditorLoadingAndSavingUtils::SavePackages(packagesToSave, true);
		}
	}

	TArray<FAssetData> assetDataToDelete;

	{
		FScopeLock autoLock(&assetDatabase.AudioTypeMap.CriticalSection);

		for (auto& typeEntry : assetDatabase.AudioTypeMap.TypeMap)
		{
			if (typeEntry.Value && typeEntry.Value->IsValidLowLevel() && !typeEntry.Value->IsA<UAkInitBank>() && !typeEntry.Value->IsA<UAkAudioBank>() && !foundAssets.Contains(typeEntry.Key) )
			{
				assetDataToDelete.Emplace(typeEntry.Value);
				assetDatabase.FillAssetsToDelete(Cast<UAkAudioType>(typeEntry.Value), assetDataToDelete);
			}
		}
	}

	collectExtraAssetsToDelete(assetDataToDelete);

	if (assetDataToDelete.Num() > 0)
	{
		for (const auto& entry : assetDataToDelete)
		{
			if (auto akAudioTypeToDelete = Cast<UAkAudioType>(entry.GetAsset()))
			{
				assetDatabase.Remove(akAudioTypeToDelete);
			}
		}

		ObjectTools::DeleteAssets(assetDataToDelete, true);
	}
}

template<typename AssetType>
void CreateAkAssetsVisitor::createAsset(const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FGuid& GroupId)
{
	auto asset = AkAssetDatabase::Get().CreateOrRenameAsset(AssetType::StaticClass(), Id, Name, AssetName, RelativePath, GroupId);
	if (!asset)
		return;

	foundAssets.Add(Id);

	if (!doSave)
		return;

	auto package = asset->GetOutermost();
	if (package && package->IsDirty())
	{
		packagesToSave.AddUnique(package);
	}
}

#undef LOCTEXT_NAMESPACE
