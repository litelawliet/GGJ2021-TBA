#include "AkAssetDatabase.h"

#include "AkAcousticTexture.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"
#include "AkGroupValue.h"
#include "AkInitBank.h"
#include "AkMediaAsset.h"
#include "AkRtpc.h"
#include "AkSettings.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "AkUnrealHelper.h"
#include "AkWaapiClient.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "Async/Async.h"
#include "ContentBrowser/Public/ContentBrowserModule.h"
#include "ContentBrowser/Public/IContentBrowserSingleton.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ScopedSlowTask.h"
#include "ToolBehavior/AkToolBehavior.h"
#include "AssetManagement/WaapiAssetSynchronizer.h"
#include "UnrealEd/Public/ObjectTools.h"
#include "UnrealEd/Public/PackageTools.h"

#define LOCTEXT_NAMESPACE "AkAudio"

DEFINE_LOG_CATEGORY_STATIC(LogAkAssetDatabase, Log, All);

AkAssetDatabase& AkAssetDatabase::Get()
{
	static AkAssetDatabase instance;
	return instance;
}

AkAssetDatabase::AkAssetDatabase()
{
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	auto& AssetRegistry = AssetRegistryModule->Get();
	AssetRegistry.OnAssetAdded().AddRaw(this, &AkAssetDatabase::onAssetAdded);
	AssetRegistry.OnAssetRemoved().AddRaw(this, &AkAssetDatabase::onAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &AkAssetDatabase::onAssetRenamed);

	AssetToolsModule = &FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
}

void AkAssetDatabase::Init()
{
	if (AudioTypeMap.TypeMap.Num() > 0)
	{
		Clear();
	}

	TArray<FAssetData> allAssets;
	auto& AssetRegistry = AssetRegistryModule->Get();
	AssetRegistry.GetAssetsByClass(UAkAudioType::StaticClass()->GetFName(), allAssets, true);

	FScopedSlowTask SlowTask(static_cast<float>(allAssets.Num()), LOCTEXT("AK_ScanAssets", "Scanning sound data assets..."));
	SlowTask.MakeDialog();

	for (auto& assetData : allAssets)
	{
		FString Message = FString::Printf(TEXT("Scanning sound data asset: %s"), *assetData.AssetName.ToString());
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(Message));

		if (auto akAudioType = Cast<UAkAudioType>(assetData.GetAsset()))
		{
			Add(akAudioType->ID, akAudioType);
		}
	}

	CreateInitBankIfNeeded();

	OnTick = FTickerDelegate::CreateRaw(this, &AkAssetDatabase::Tick);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(OnTick);
}

void AkAssetDatabase::Clear()
{
	{
		FScopeLock autoLock(&InitBankLock);
		InitBank = nullptr;
	}

	AcousticTextureMap.Empty();
	EventMap.Empty();
	AuxBusMap.Empty();
	GroupValueMap.Empty();
	TriggerMap.Empty();
	RtpcMap.Empty();
	BankMap.Empty();
	AudioTypeMap.Empty();
}

FString AkAssetDatabase::GetWwisePathFromAssetPath(const FString& AssetPath)
{
	const auto AkSettings = GetDefault<UAkSettings>();
	if (!AkSettings)
	{
		// todo slaptiste: should we consider using an alternate path if AkSettings is not found???
		return {};
	}

	FString Path;
	FString FileName;
	FString Extenstion;
	FPaths::Split(AssetPath, Path, FileName, Extenstion);

	Path.RemoveFromStart(AkUnrealHelper::GetBaseAssetPackagePath(), ESearchCase::IgnoreCase);
	return Path.Replace(TEXT("/"), TEXT("\\")).Replace(TEXT("_"), TEXT(" "));
}

FString AkAssetDatabase::GetBaseFolderForAssetType(const UClass* Klass)
{
	if (Klass->IsChildOf<UAkAcousticTexture>())
	{
		return AkAssetTraits<UAkAcousticTexture>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkAudioEvent>())
	{
		return AkAssetTraits<UAkAudioEvent>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkAuxBus>())
	{
		return AkAssetTraits<UAkAuxBus>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkRtpc>())
	{
		return AkAssetTraits<UAkRtpc>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkStateValue>())
	{
		return AkAssetTraits<UAkStateValue>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkSwitchValue>())
	{
		return AkAssetTraits<UAkSwitchValue>::BaseFolder();
	}
	else if (Klass->IsChildOf<UAkTrigger>())
	{
		return AkAssetTraits<UAkTrigger>::BaseFolder();
	}

	return AkAssetTraits<UAkAudioType>::BaseFolder();
}

bool AkAssetDatabase::Add(const FGuid& Id, UAkAudioType* AudioType)
{
	FGuid finalId = Id;

	if (!AkToolBehavior::Get()->AkAssetDatabase_ValidateAssetId(finalId))
	{
		return false;
	}

	if (auto acousticTexture = Cast<UAkAcousticTexture>(AudioType))
	{
		AcousticTextureMap.Add(finalId, acousticTexture);
	}
	else if (auto audioEvent = Cast<UAkAudioEvent>(AudioType))
	{
		EventMap.Add(finalId, audioEvent);
	}
	else if (auto auxBus = Cast<UAkAuxBus>(AudioType))
	{
		AuxBusMap.Add(finalId, auxBus);
	}
	else if (auto groupValue = Cast<UAkGroupValue>(AudioType))
	{
		GroupValueMap.Add(finalId, groupValue);
	}
	else if (auto trigger = Cast<UAkTrigger>(AudioType))
	{
		TriggerMap.Add(finalId, trigger);
	}
	else if (auto rtpc = Cast<UAkRtpc>(AudioType))
	{
		RtpcMap.Add(finalId, rtpc);
	}
	else if (auto audioBank = Cast<UAkAudioBank>(AudioType))
	{
		BankMap.Add(finalId, audioBank);
	}
	else if (auto initBank = Cast<UAkInitBank>(AudioType))
	{
		InitBank = initBank;
	}
	else
	{
		return false;
	}

	AudioTypeMap.Add(finalId, AudioType);
	return true;
}

bool AkAssetDatabase::Remove(UAkAudioType* AudioType)
{
	return AkToolBehavior::Get()->AkAssetDatabase_Remove(this, AudioType);
}

void AkAssetDatabase::CreateInitBankIfNeeded()
{
	FScopeLock autoLock(&InitBankLock);

	if (!InitBank)
	{
		const FString InitBankPath = AkToolBehavior::Get()->AkAssetDatabase_GetInitBankPackagePath();
		auto newInitBank = AssetToolsModule->Get().CreateAsset(TEXT("InitBank"), InitBankPath, UAkInitBank::StaticClass(), nullptr);
		InitBank = Cast<UAkInitBank>(newInitBank);
		if (InitBank)
		{
			InitBank->ID = AkUnrealHelper::InitBankID;
		}
	}
}

void AkAssetDatabase::RenameAsset(const UClass* Klass, const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FString& GroupId)
{
	check(IsInGameThread());

	auto parentPath = RelativePath;
	parentPath.RemoveFromEnd(Name);

	auto AssetPackagePath = ObjectTools::SanitizeObjectPath(FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), parentPath));
	auto AssetPath = FPaths::Combine(AssetPackagePath, AssetName + TEXT(".") + AssetName);

	auto assetInstancePtr = AudioTypeMap.Find(Id);
	if (!assetInstancePtr)
		return;

	auto assetInstance = *assetInstancePtr;
	if (!assetInstance)
		return;

	auto objectPath = assetInstance->GetPathName(nullptr);
	if (objectPath == AssetPath)
		return;

	TArray<FAssetRenameData> assetsToRename = { { FSoftObjectPath(objectPath), FSoftObjectPath(AssetPath) } };

	if (auto akAudioEvent = Cast<UAkAudioEvent>(assetInstance))
	{
		renameLocalizedAssets(akAudioEvent, parentPath, AssetName, assetsToRename);
	}

	AssetToolsModule->Get().RenameAssets(assetsToRename);

	removeEmptyFolders(assetsToRename);
}

UObject* AkAssetDatabase::CreateOrRenameAsset(const UClass* Klass, const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FGuid& GroupId)
{
	check(IsInGameThread());

	auto* AudioDevice = FAkAudioDevice::Get();

	auto parentPath = RelativePath;
	parentPath.RemoveFromEnd(Name);

	auto AssetPackagePath = ObjectTools::SanitizeObjectPath(FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), parentPath));
	auto AssetPath = FPaths::Combine(AssetPackagePath, AssetName + TEXT(".") + AssetName);

	auto assetInstancePtr = AudioTypeMap.Find(Id);
	if (!assetInstancePtr)
	{
		FString ValueName = AssetName;
		auto assetInstance = Cast<UAkAudioType>(AssetToolsModule->Get().CreateAsset(AssetName, AssetPackagePath, const_cast<UClass*>(Klass), nullptr));
		if (!assetInstance)
		{
			return nullptr;
		}

		assetInstance->ID = Id;
		if (auto groupValue = Cast<UAkGroupValue>(assetInstance))
		{
			FString GroupName;
			AssetName.Split(TEXT("-"), &GroupName, &ValueName);
			groupValue->GroupID = GroupId;

			if(AudioDevice)
				groupValue->GroupShortID = AudioDevice->GetIDFromString(GroupName);
		}

		if(AudioDevice)
			assetInstance->ShortID = AudioDevice->GetIDFromString(ValueName);

		Add(Id, assetInstance);
		PendingAssetCreates.Remove(Id);
		return assetInstance;
	}

	auto assetInstance = *assetInstancePtr;
	if (!assetInstance)
		return nullptr;

	auto objectPath = assetInstance->GetPathName();
	if (objectPath != AssetPath)
	{
		TArray<FAssetRenameData> assetsToRename = { { FSoftObjectPath(objectPath), FSoftObjectPath(AssetPath) } };

		if (auto akAudioEvent = Cast<UAkAudioEvent>(assetInstance))
		{
			renameLocalizedAssets(akAudioEvent, parentPath, AssetName, assetsToRename);
		}

		if (GWarn && GWarn->GetScopeStack().Num() > 0)
		{
			AssetToolsModule->Get().RenameAssets(assetsToRename);
			removeEmptyFolders(assetsToRename);
		}
		else
		{
			// WG-46823: Workaround for random crash in Unreal code
			AsyncTask(ENamedThreads::GameThread, [this, assetsToRename] {
				AssetToolsModule->Get().RenameAssets(assetsToRename);
				removeEmptyFolders(assetsToRename);
			});
		}
	}

	return assetInstance;
}

void AkAssetDatabase::RenameGroupValues(const FGuid& GroupId, const FString& GroupName, const FString& Path)
{
	TArray<FAssetData> findResults;

	{
		FScopeLock autoLock(&GroupValueMap.CriticalSection);
		for (auto& entry : GroupValueMap.TypeMap)
		{
			if (entry.Value->GroupID == GroupId)
			{
				findResults.Emplace(entry.Value);
			}
		}
	}

	if (findResults.Num() <= 0)
		return;

	auto AssetPackagePath = ObjectTools::SanitizeObjectPath(FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), Path));

	TArray<FAssetRenameData> assetsToRename;
	for (auto& assetData : findResults)
	{
		FString valueName;
		assetData.AssetName.ToString().Split(TEXT("-"), nullptr, &valueName);
		auto newAssetPath = FPaths::Combine(AssetPackagePath, FString::Printf(TEXT("%s-%s.%s-%s"), *GroupName, *valueName, *GroupName, *valueName));
		assetsToRename.Emplace(assetData.ToSoftObjectPath(), FSoftObjectPath(newAssetPath));
	}

	if (assetsToRename.Num() > 0)
	{
		AssetToolsModule->Get().RenameAssets(assetsToRename);

		removeEmptyFolders(assetsToRename);
	}
}

void AkAssetDatabase::DeleteAsset(const FGuid& Id)
{
	check(IsInGameThread());

	auto foundAssetIt = AudioTypeMap.Find(Id);

	if (!foundAssetIt)
	{
		return;
	}

	Remove(*foundAssetIt);

	TArray<FAssetData> assetsToDelete = { *foundAssetIt };
	FillAssetsToDelete(Cast<UAkAudioType>(*foundAssetIt), assetsToDelete);

	if (assetsToDelete.Num() > 0)
	{
		ObjectTools::DeleteAssets(assetsToDelete, true);
	}
}

void AkAssetDatabase::DeleteAssets(const TSet<FGuid>& AssetsId)
{
	check(IsInGameThread());

	TArray<FAssetData> assetsToDelete;

	for (auto& id : AssetsId)
	{
		auto foundAssetIt = AudioTypeMap.Find(id);

		if (!foundAssetIt)
		{
			return;
		}

		auto foundAsset = *foundAssetIt;

		Remove(foundAsset);

		assetsToDelete.Emplace(*foundAssetIt);
		FillAssetsToDelete(Cast<UAkAudioType>(foundAsset), assetsToDelete);
	}

	if (assetsToDelete.Num() > 0)
	{
		ObjectTools::DeleteAssets(assetsToDelete, true);
	}
}

void AkAssetDatabase::FillAssetsToDelete(UAkAudioType* Asset, TArray<FAssetData>& AssetDataToDelete)
{
	if (auto akAudioEvent = Cast<UAkAudioEvent>(Asset))
	{
		for (auto& entry : akAudioEvent->LocalizedPlatformAssetDataMap)
		{
			if (auto platformData = entry.Value.LoadSynchronous())
			{
				AssetDataToDelete.Emplace(platformData);

				TArray<TSoftObjectPtr<UAkMediaAsset>> mediaList;
				platformData->GetMediaList(mediaList);
				processMediaToDelete(platformData, mediaList, AssetDataToDelete);
			}
		}
	}

	if (auto assetBase = Cast<UAkAssetBase>(Asset))
	{
		TArray<TSoftObjectPtr<UAkMediaAsset>> mediaList;
		assetBase->GetMediaList(mediaList);
		processMediaToDelete(assetBase, mediaList, AssetDataToDelete);
	}
}

void AkAssetDatabase::MoveAllAssets(const FString& OldBaseAssetPath, const FString& NewBaseAssetPath)
{
	TArray<FAssetRenameData> assetsToRename;

	TArray<FAssetData> allAssets;
	AssetRegistryModule->Get().GetAssetsByClass(UAkMediaAsset::StaticClass()->GetFName(), allAssets, true);
	AssetRegistryModule->Get().GetAssetsByClass(UAkAssetPlatformData::StaticClass()->GetFName(), allAssets, true);

	for (auto& entry : AudioTypeMap.TypeMap)
	{
		allAssets.Emplace(entry.Value);
	}

	for (auto& assetData : allAssets)
	{
		auto packagePath = assetData.ObjectPath.ToString();

		if (packagePath.StartsWith(OldBaseAssetPath))
		{
			auto index = assetsToRename.Emplace(FSoftObjectPath(packagePath), FSoftObjectPath(packagePath.Replace(*OldBaseAssetPath, *NewBaseAssetPath)));
			assetsToRename[index].Asset = assetData.GetAsset();
		}
	}

	if (assetsToRename.Num() > 0)
	{
		AssetToolsModule->Get().RenameAssets(assetsToRename);

		FixUpRedirectors(NewBaseAssetPath);
	}

	// Move the loose external source .wems
	TArray<FString> WemsToMove;
	auto OldWwiseAudioFolder = OldBaseAssetPath;
	auto NewWwiseAudioFolder = NewBaseAssetPath;
	OldWwiseAudioFolder.RemoveFromStart(TEXT("/Game/"));
	NewWwiseAudioFolder.RemoveFromStart(TEXT("/Game/"));
	auto OldAudioFolderFullPath = AkUnrealHelper::GetContentDirectory() / OldWwiseAudioFolder;
	IFileManager::Get().FindFilesRecursive(WemsToMove, *OldAudioFolderFullPath, TEXT("*.wem"), true, false);
	IFileManager::Get().FindFilesRecursive(WemsToMove, *OldAudioFolderFullPath, TEXT("Wwise.dat"), true, false, false);
	for (auto WemFile : WemsToMove)
	{
		auto NewPath = WemFile.Replace(*OldWwiseAudioFolder, *NewWwiseAudioFolder);
		IFileManager::Get().Move(*NewPath, *WemFile);
	}

	// Delete the old folder hierarchy
	IFileManager::Get().DeleteDirectory(*OldAudioFolderFullPath, false, true);
}

void AkAssetDatabase::MoveWorkUnit(const FString& OldWorkUnitPath, const FString& NewWorkUnitPath)
{
	TArray<FAssetData> allAssets;
	for (auto& entry : AudioTypeMap.TypeMap)
	{
		allAssets.Emplace(entry.Value);
	}

	FString sanitizedOldWorkUnitPath = ObjectTools::SanitizeObjectPath(OldWorkUnitPath);
	auto workUnitOldPackagePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), sanitizedOldWorkUnitPath);

	FString sanitizedNewWorkUnitPath = ObjectTools::SanitizeObjectPath(NewWorkUnitPath);
	auto workUnitNewPackagePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), sanitizedNewWorkUnitPath);

	TArray<FString> pathParts;
	sanitizedNewWorkUnitPath.ParseIntoArray(pathParts, TEXT("/"));

	TArray<FAssetRenameData> assetsToRename;

	for (auto& assetData : allAssets)
	{
		auto packagePath = assetData.ObjectPath.ToString();

		if (packagePath.StartsWith(workUnitOldPackagePath))
		{
			auto assetName = assetData.AssetName.ToString();

			FString newPackagePath = FString::Printf(TEXT("%s/%s.%s"), *workUnitNewPackagePath, *assetName, *assetName);

			assetsToRename.Emplace(FSoftObjectPath(packagePath), FSoftObjectPath(newPackagePath));

			if (auto* akAudioEvent = Cast<UAkAudioEvent>(assetData.GetAsset()))
			{
				renameLocalizedAssets(akAudioEvent, sanitizedNewWorkUnitPath, assetName, assetsToRename);
			}
		}
	}

	if (assetsToRename.Num() > 0)
	{
		WaapiAssetSynchronizer::PauseAssetRegistryDelegates = true;
		AssetToolsModule->Get().RenameAssets(assetsToRename);

		FixUpRedirectors(workUnitNewPackagePath);

		removeEmptyFolders(assetsToRename);
		WaapiAssetSynchronizer::PauseAssetRegistryDelegates = false;
	}
}

void AkAssetDatabase::FixUpRedirectors(const FString& AssetPackagePath)
{
	TArray<UObjectRedirector*> redirectorsToFix;

	TArray<FAssetData> foundRedirectorsData;
	AssetRegistryModule->Get().GetAssetsByClass(UObjectRedirector::StaticClass()->GetFName(), foundRedirectorsData);

	if (foundRedirectorsData.Num() > 0)
	{
		for (auto& entry : foundRedirectorsData)
		{
			if (auto redirector = Cast<UObjectRedirector>(entry.GetAsset()))
			{
				if (redirector->DestinationObject)
				{
					auto pathName = redirector->DestinationObject->GetPathName();
					if (pathName.StartsWith(AssetPackagePath))
					{
						redirectorsToFix.Add(redirector);
					}
				}
			}
		}
	}

	if (redirectorsToFix.Num() > 0)
	{
		AssetToolsModule->Get().FixupReferencers(redirectorsToFix);
	}
}

bool AkAssetDatabase::CanBeDropped(const FAssetData& AssetData, FName PackagePath, CanBeDroppedSource Source) const
{
	auto akAudioType = Cast<UAkAudioType>(AssetData.GetAsset());
	if (!akAudioType)
	{
		return false;
	}

	auto expectedPackagePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), GetBaseFolderForAssetType(akAudioType->GetClass()));

	if (!PackagePath.ToString().StartsWith(expectedPackagePath, ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (!AudioTypeMap.Find(akAudioType->ID))
	{
		return true;
	}

	if (PackagePath == AssetData.PackagePath)
	{
		return false;
	}

	auto assetName = AssetData.AssetName.ToString();

	auto isValidName = [&PackagePath, Source](const auto& map, const FString& assetName) {
		auto assetsFound = map.FindByName(assetName);

		if (assetsFound.Num() > 0)
		{
			if (Source == CanBeDroppedSource::FromContentBrowser)
			{
				for (UObject* asset : assetsFound)
				{
					auto assetPackagePath = FPaths::GetPath(asset->GetPathName());
					if (assetPackagePath != PackagePath.ToString())
					{
						return true;
					}
				}
			}

			return false;
		}
		else
		{
			return true;
		}
	};

	if (akAudioType->IsA<UAkAcousticTexture>())
	{
		return isValidName(AcousticTextureMap, assetName);
	}
	else if (akAudioType->IsA<UAkAudioEvent>())
	{
		return isValidName(EventMap, assetName);
	}
	else if (akAudioType->IsA<UAkAuxBus>())
	{
		return isValidName(AuxBusMap, assetName);
	}
	else if (akAudioType->IsA<UAkGroupValue>())
	{
		return isValidName(GroupValueMap, assetName);
	}
	else if (akAudioType->IsA<UAkTrigger>())
	{
		return isValidName(TriggerMap, assetName);
	}
	else if (akAudioType->IsA<UAkRtpc>())
	{
		return isValidName(RtpcMap, assetName);
	}

	return false;
}

void AkAssetDatabase::processMediaToDelete(UObject* Asset, const TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaList, TArray<FAssetData>& AssetDataToDelete)
{
	auto assetPackageName = Asset->GetOutermost()->GetFName();

	for (auto& media : MediaList)
	{
		auto mediaPackageName = FName(*media.GetLongPackageName());

		TArray<FName> dependencies;
#if UE_4_26_OR_LATER
		AssetRegistryModule->Get().GetReferencers(mediaPackageName, dependencies, UE::AssetRegistry::EDependencyCategory::All);
#else
		AssetRegistryModule->Get().GetReferencers(mediaPackageName, dependencies, EAssetRegistryDependencyType::All);
#endif

		if ((dependencies.Num() == 1 && dependencies[0] == assetPackageName) || dependencies.Num() == 0)
		{
			AssetDataToDelete.Emplace(media.LoadSynchronous());
		}
	}
}

void AkAssetDatabase::renameLocalizedAssets(const UAkAudioEvent* akAudioEvent, const FString& parentPath, const FString& AssetName, TArray<FAssetRenameData>& assetsToRename)
{
	auto localizedAssetPath = AkUnrealHelper::GetLocalizedAssetPackagePath();
	for (auto& entry : akAudioEvent->LocalizedPlatformAssetDataMap)
	{
		auto localizedPackagePath = ObjectTools::SanitizeObjectPath(FPaths::Combine(localizedAssetPath, entry.Key, parentPath));
		auto newLocalizedAssetPath = FPaths::Combine(localizedPackagePath, AssetName + TEXT(".") + AssetName);
		entry.Value.LoadSynchronous();
		assetsToRename.Emplace(entry.Value.ToSoftObjectPath(), FSoftObjectPath(newLocalizedAssetPath));
	}
}

void AkAssetDatabase::removeEmptyFolders(const TArray<FAssetRenameData>& AssetsToRename)
{
	TSet<FString> foldersToCheck;

	for (auto& renameData : AssetsToRename)
	{
		auto PathToCheck = FPaths::GetPath(renameData.OldObjectPath.GetLongPackageName());
		if(PathToCheck != TEXT("/Game"))
			foldersToCheck.Add(PathToCheck);
	}

	auto& platformFile = FPlatformFileManager::Get().GetPlatformFile();

	for (auto& assetPath : foldersToCheck)
	{
		auto directoryPath = FPackageName::LongPackageNameToFilename(assetPath);
		uint32 itemCount = 0;
		platformFile.IterateDirectoryRecursively(*directoryPath, [&itemCount](const TCHAR* path, bool) {
			++itemCount;
			return true;
			});

		if (itemCount == 0)
		{
			if (platformFile.DeleteDirectoryRecursively(*directoryPath))
			{
				AssetRegistryModule->Get().RemovePath(assetPath);
			}
		}
	}
}

bool AkAssetDatabase::IsAkAudioType(const FAssetData& AssetData)
{
	static const TArray<FName> AkAudioClassNames = {
		UAkAcousticTexture::StaticClass()->GetFName(),
		UAkAudioBank::StaticClass()->GetFName(),
		UAkAudioEvent::StaticClass()->GetFName(),
		UAkAuxBus::StaticClass()->GetFName(),
		UAkInitBank::StaticClass()->GetFName(),
		UAkRtpc::StaticClass()->GetFName(),
		UAkStateValue::StaticClass()->GetFName(),
		UAkSwitchValue::StaticClass()->GetFName(),
		UAkTrigger::StaticClass()->GetFName()
	};

	if (AkAudioClassNames.Contains(AssetData.AssetClass))
		return true;

	return false;

}

void AkAssetDatabase::onAssetAdded(const FAssetData& NewAssetData)
{
	if (IsAkAudioType(NewAssetData))
	{
		auto akAudioType = Cast<UAkAudioType>(NewAssetData.GetAsset());
		if (akAudioType->ID.IsValid())
		{
			if (AkUnrealHelper::IsUsingEventBased() && akAudioType->GetClass() != UAkAudioBank::StaticClass())
			{
				FString assetBaseFolder = AkAssetDatabase::Get().GetBaseFolderForAssetType(akAudioType->GetClass());
				FString assetBasePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), assetBaseFolder);
				auto newPath = NewAssetData.ObjectPath.ToString();

				FString addErrorMessage;
				auto addSucceeded = true;

				if (auto foundAkAudioTypeIt = AudioTypeMap.Find(akAudioType->ID))
				{
					auto foundAssetPath = (*foundAkAudioTypeIt)->GetPathName();

					if (foundAssetPath != akAudioType->GetPathName())
					{
						addErrorMessage = FString::Printf(TEXT("An asset with ID '%s' already exists here '%s"), *akAudioType->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces), *foundAssetPath);
						addSucceeded = false;
					}
				}

				if (!assetBaseFolder.IsEmpty() && !newPath.StartsWith(assetBasePath, ESearchCase::IgnoreCase))
				{
					addErrorMessage = FString::Printf(TEXT("Asset is not contained within '%s'"), *assetBasePath);
					addSucceeded = false;
				}

				if (!addSucceeded)
				{
					const FFormatNamedArguments Args
					{
						{ TEXT("NewPath"), FText::FromString(newPath) },
						{ TEXT("Reason"), FText::FromString(addErrorMessage) }
					};

					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("AkAssetDatabase", "CannotCreateAsset", "Cannot create asset in '{NewPath}'. Reason: {Reason}."), Args));
					CopiedAssetsToUndo.Add(NewAssetData);
					return;
				}
			}

			Add(akAudioType->ID, akAudioType);
		}
	}
}

void AkAssetDatabase::onAssetRemoved(const FAssetData& RemovedAssetData)
{
	if (FAkWaapiClient::IsProjectLoaded())
	{
		return;
	}

	if (auto assetInstance = Cast<UAkAudioType>(RemovedAssetData.GetAsset()))
	{
		if (assetInstance->ID.IsValid())
		{
			AkAssetDatabase::Get().Remove(assetInstance);
		}
	}
}

void AkAssetDatabase::onAssetRenamed(const FAssetData& NewAssetData, const FString& OldPath)
{
	static const auto InvalidCharacters = FString(TEXT(":<>*?\"\\/|.%"));

	if (!AkUnrealHelper::IsUsingEventBased())
	{
		return;
	}

	auto assetInstance = Cast<UAkAudioType>(NewAssetData.GetAsset());
	if (!assetInstance)
	{
		return;
	}

	if (!assetInstance->ID.IsValid())
	{
		return;
	}

	if (ignoreRenames.Contains(assetInstance->ID))
	{
		ignoreRenames.Remove(assetInstance->ID);
		return;
	}

	auto newPath = NewAssetData.ObjectPath.GetPlainNameString();

	FString oldDirectory;
	FString oldFileName;
	FString oldExtension;
	FPaths::Split(OldPath, oldDirectory, oldFileName, oldExtension);

	FString newDirectory;
	FString newFileName;
	FString newExtension;
	FPaths::Split(newPath, newDirectory, newFileName, newExtension);

	FString assetBaseFolder = AkAssetDatabase::Get().GetBaseFolderForAssetType(assetInstance->GetClass());
	FString assetBasePath = FPaths::Combine(AkUnrealHelper::GetBaseAssetPackagePath(), assetBaseFolder);

	FString moveErrorMessage;
	auto moveSucceeded = true;

	auto invalidName = false;

	if (!assetBaseFolder.IsEmpty() && !newDirectory.StartsWith(assetBasePath, ESearchCase::IgnoreCase))
	{
		moveErrorMessage = FString::Printf(TEXT("Asset is not contained within '%s'"), *assetBasePath);
		moveSucceeded = false;
	}

	if (newFileName.Len() > 0 && !FChar::IsAlpha(newFileName[0]))
	{
		invalidName = true;
	}

	for (auto character : newFileName)
	{
		int32 dummy = 0;
		if (InvalidCharacters.FindChar(character, dummy))
		{
			invalidName = true;
			break;
		}
	}

	if (moveSucceeded && !invalidName)
	{
		return;
	}

	if (!moveSucceeded)
	{
		const FFormatNamedArguments Args
		{
			{ TEXT("OldName"), FText::FromString(oldFileName) },
			{ TEXT("OldPath"), FText::FromString(oldDirectory) },
			{ TEXT("NewPath"), FText::FromString(newDirectory) },
			{ TEXT("Reason"), FText::FromString(moveErrorMessage) }
		};

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("AkAssetDatabase", "CannotMoveAsset", "Cannot move '{OldName}' from '{OldPath}' to '{NewPath}'. Reason: {Reason}."), Args));
	}
	else if (invalidName)
	{
		const FFormatNamedArguments Args
		{
			{ TEXT("NewName"), FText::FromString(newFileName) },
			{ TEXT("InvalidChars"), FText::FromString(InvalidCharacters) }
		};

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("AkAssetDatabase", "InvalidAssetName", "Invalid characters found in the new name '{NewName}'. The name must start with a letter and must not contains these characters\n\n{InvalidChars}"), Args));
	}

	ignoreRenames.Add(assetInstance->ID);

	// This is required to avoid an assert in RenameAssets() when the asset is referenced somewhere
	GRedirectCollector.AddAssetPathRedirection(FName(*OldPath), FName(*newPath));

	FAssetRenameData renameData{ NewAssetData.ToSoftObjectPath(), FSoftObjectPath(OldPath) };

	AssetToolsModule->Get().RenameAssets(TArray<FAssetRenameData>{renameData});

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets({ FAssetData(FSoftObjectPath(OldPath).TryLoad()) });
}

bool AkAssetDatabase::Tick(float DeltaTime)
{
	if(CopiedAssetsToUndo.Num() > 0)
	{
		ObjectTools::DeleteAssets(CopiedAssetsToUndo, false);
		CopiedAssetsToUndo.Empty();
	}
	return true;
}
#undef LOCTEXT_NAMESPACE
