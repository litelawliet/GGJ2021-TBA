#include "WaapiAssetSynchronizer.h"

#include "AkAcousticTexture.h"
#include "AkAssetDatabase.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkAudioType.h"
#include "AkAuxBus.h"
#include "AkInitBank.h"
#include "AkRtpc.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "AkWaapiUtils.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "AssetTools/Public/AssetToolsModule.h"
#include "Async/Async.h"
#include "ContentBrowser/Public/ContentBrowserModule.h"
#include "ContentBrowser/Public/IContentBrowserSingleton.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ScopedSlowTask.h"
#include "UnrealEd/Public/FileHelpers.h"

#define LOCTEXT_NAMESPACE "AkAssetFactory"

DEFINE_LOG_CATEGORY_STATIC(LogAkWaapiAssetSynchronizer, Log, All);

bool WaapiAssetSynchronizer::PauseAssetRegistryDelegates;

namespace WaapiAssetSynchronizer_Helper
{
	constexpr auto DeleteTime = 0.35f;

	void SubscribeWaapiCallback(FAkWaapiClient* waapiClient, const char* uri, WampEventCallback callback, uint64& subscriptionId, const TArray<FString>& ReturnArgs)
	{
		if (subscriptionId)
			return;

		TArray<TSharedPtr<FJsonValue>> ReturnArgsArray;
		for (auto& arg : ReturnArgs)
			ReturnArgsArray.Add(MakeShared<FJsonValueString>(arg));

		TSharedRef<FJsonObject> options = MakeShared<FJsonObject>();
		options->SetArrayField(WwiseWaapiHelper::RETURN, ReturnArgsArray);

		TSharedPtr<FJsonObject> result;
		waapiClient->Subscribe(uri, options, callback, subscriptionId, result);
	}

	void UnsubscribeWaapiCallback(FAkWaapiClient* waapiClient, uint64& subscriptionId)
	{
		if (subscriptionId == 0)
			return;

		TSharedPtr<FJsonObject> result = MakeShared<FJsonObject>();
		waapiClient->Unsubscribe(subscriptionId, result);
		subscriptionId = 0;
	}
}

void WaapiAssetSynchronizer::Init()
{
	auto waapiClient = FAkWaapiClient::Get();
	if (!waapiClient)
		return;

	subscribeWaapiCallbacks();

	projectLoadedHandle = waapiClient->OnProjectLoaded.AddRaw(this, &WaapiAssetSynchronizer::subscribeWaapiCallbacks);
	connectionLostHandle = waapiClient->OnConnectionLost.AddRaw(this, &WaapiAssetSynchronizer::unsubscribeWaapiCallbacks);
	clientBeginDestroyHandle = waapiClient->OnClientBeginDestroy.AddRaw(this, &WaapiAssetSynchronizer::unsubscribeWaapiCallbacks);

	auto& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	assetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &WaapiAssetSynchronizer::onAssetRemoved);
	assetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &WaapiAssetSynchronizer::onAssetRenamed);

	if (GEngine)
	{
		postEditorTickHandle = GEngine->OnPostEditorTick().AddRaw(this, &WaapiAssetSynchronizer::deleteTick);
	}
}

void WaapiAssetSynchronizer::Uninit()
{
	unsubscribeWaapiCallbacks();

	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		auto& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.OnAssetRemoved().Remove(assetRemovedHandle);
		AssetRegistry.OnAssetRenamed().Remove(assetRenamedHandle);
	}

	assetRemovedHandle.Reset();
	assetRenamedHandle.Reset();

	auto waapiClient = FAkWaapiClient::Get();
	if (waapiClient)
	{
		waapiClient->OnProjectLoaded.Remove(projectLoadedHandle);
		waapiClient->OnConnectionLost.Remove(connectionLostHandle);
		waapiClient->OnClientBeginDestroy.Remove(clientBeginDestroyHandle);
	}

	projectLoadedHandle.Reset();
	connectionLostHandle.Reset();
	clientBeginDestroyHandle.Reset();

	if (GEngine)
	{
		GEngine->OnPostEditorTick().Remove(postEditorTickHandle);
	}
}

void WaapiAssetSynchronizer::subscribeWaapiCallbacks()
{
	auto waapiClient = FAkWaapiClient::Get();
	if (!waapiClient)
		return;

	const TArray<FString> ReturnArgs
	{
		WwiseWaapiHelper::ID,
		WwiseWaapiHelper::NAME,
		WwiseWaapiHelper::PATH,
		WwiseWaapiHelper::TYPE,
		WwiseWaapiHelper::PARENT,
	};

	WaapiAssetSynchronizer_Helper::SubscribeWaapiCallback(waapiClient,
		ak::wwise::core::object::nameChanged,
		WampEventCallback::CreateRaw(this, &WaapiAssetSynchronizer::onRenamed),
		idRenamed,
		ReturnArgs);

	WaapiAssetSynchronizer_Helper::SubscribeWaapiCallback(waapiClient,
		ak::wwise::core::object::preDeleted,
		WampEventCallback::CreateRaw(this, &WaapiAssetSynchronizer::onPreDeleted),
		idPreDeleted,
		{
			WwiseWaapiHelper::ID,
			WwiseWaapiHelper::NAME,
			WwiseWaapiHelper::PATH,
		});

	WaapiAssetSynchronizer_Helper::SubscribeWaapiCallback(waapiClient,
		ak::wwise::core::object::childAdded,
		WampEventCallback::CreateRaw(this, &WaapiAssetSynchronizer::onChildAdded),
		idChildAdded,
		ReturnArgs);

	WaapiAssetSynchronizer_Helper::SubscribeWaapiCallback(waapiClient,
		ak::wwise::core::object::childRemoved,
		WampEventCallback::CreateRaw(this, &WaapiAssetSynchronizer::onChildRemoved),
		idChildRemoved,
		ReturnArgs);
}

void WaapiAssetSynchronizer::unsubscribeWaapiCallbacks()
{
	auto waapiClient = FAkWaapiClient::Get();
	if (!waapiClient)
		return;

	WaapiAssetSynchronizer_Helper::UnsubscribeWaapiCallback(waapiClient, idRenamed);
	WaapiAssetSynchronizer_Helper::UnsubscribeWaapiCallback(waapiClient, idPreDeleted);
	WaapiAssetSynchronizer_Helper::UnsubscribeWaapiCallback(waapiClient, idChildAdded);
	WaapiAssetSynchronizer_Helper::UnsubscribeWaapiCallback(waapiClient, idChildRemoved);
}

void WaapiAssetSynchronizer::deleteTick(float DeltaSeconds)
{
	if (deleteTimer > 0.f)
	{
		deleteTimer -= DeltaSeconds;

		if (deleteTimer <= 0.f)
		{
			if (assetsToDelete.Num() > 0)
			{
				auto copyAssetsToDelete = assetsToDelete;
				assetsToDelete.Empty();
				AkAssetDatabase::Get().DeleteAssets(copyAssetsToDelete);
			}
		}
	}
}

void WaapiAssetSynchronizer::onPreDeleted(uint64_t Id, TSharedPtr<FJsonObject> Response)
{
	if (FAkAudioDevice::Get()->IsBuildingData)
		return;

	AsyncTask(ENamedThreads::GameThread, [this, Response]
	{
		const TSharedPtr<FJsonObject>* objectPtr = nullptr;
		if (!Response->TryGetObjectField(WwiseWaapiHelper::OBJECT, objectPtr) || !objectPtr)
			return;

		auto object = *objectPtr;

		FString id;
		if (!object->TryGetStringField(WwiseWaapiHelper::ID, id))
			return; // error parsing Json

		FGuid guidId;
		FGuid::ParseExact(id, EGuidFormats::DigitsWithHyphensInBraces, guidId);

		if (AkAssetDatabase::Get().AudioTypeMap.Find(guidId))
		{
			deleteTimer = WaapiAssetSynchronizer_Helper::DeleteTime;
			assetsToDelete.Add(guidId);
		}
	});
}

// There appears to be four different ways childAdded can be called, and it is intimately linked with renamed.
// SCENARIO #1: Use the "New" button in the corresponding hierarchy;
//		1.	Renamed is called first, with an empty old name, no parent, and an invalid path. We ignore this.
//		2.	ChildAdded is called, with correct information. We realize we have never seen this GUID before, 
//			so we push it on a "pending assets to create" stack. We then create the corresponding uasset 
//			immediately, removing the GUID from the stack.
// SCENARIO #2: Copy/paste an existing item
//		1.	ChildAdded is called first, but with the same name as the source item. We realize we have never 
//			seen this GUID before, so we push it on a "pending assets to create" stack. We then realize we 
//			already have an asset with that name, so we	wait for the rename event to give us the correct name.
//		2.	Rename is called with the correct name. We then create the asset, and remove the GUID from the stack.
// SCENARIO #3: Reparenting an object
//		1.	We only get a childAdded in that case. We realize we already know about that GUID, so we treat this 
//			as a rename.
// SCENARIO #4: Creating the item from the Actor-Mixer Hierarchy context menu (mostly New Event)
//		1.	ChildAdded gets called first, but with an empty name. We realize we have never seen this GUID before, 
//			so we push it on a "pending assets to create" stack. The empty name makes us exit this function early, 
//			waiting for the rename callback to be called.
//		2.	Renamed is called with the correct name. We then create the asset, and remove the GUID from the stack.
void WaapiAssetSynchronizer::onChildAdded(uint64_t Id, TSharedPtr<FJsonObject> Response)
{
	if (FAkAudioDevice::Get()->IsBuildingData)
		return;

	AsyncTask(ENamedThreads::GameThread, [this, Response]
	{
		const TSharedPtr<FJsonObject>* childObjectPtr = nullptr;
		if (!Response->TryGetObjectField(WwiseWaapiHelper::CHILD, childObjectPtr) || !childObjectPtr)
			return;

		auto& akAssetDatabase = AkAssetDatabase::Get();
		auto childObject = *childObjectPtr;

		FString id;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::ID, id))
			return; // error parsing Json

		FGuid guidId;
		FGuid::ParseExact(id, EGuidFormats::DigitsWithHyphensInBraces, guidId);

		FString stringAssetType;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::TYPE, stringAssetType))
			return; // error parsing Json

		const UClass* assetType = WaapiAssetSynchronizer::GetClassByName(stringAssetType);
		if (akAssetDatabase.AudioTypeMap.Find(guidId) == nullptr && assetType != nullptr)
		{
			// We don't know about this object. It will either be created here, or in a rename. 
			// Put it in a stack, so that rename knows to do an actual "create" in that case.
			akAssetDatabase.PendingAssetCreates.Add(guidId);
		}

		FString name;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::NAME, name) || name.IsEmpty())
			return; // error parsing Json

		FString path;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::PATH, path))
			return; // error parsing Json

		FString assetName = name;

		FGuid groupId;

		if (assetType == UAkStateValue::StaticClass() || assetType == UAkSwitchValue::StaticClass())
		{
			const TSharedPtr<FJsonObject>* parentPtr = nullptr;
			if (!Response->TryGetObjectField(WwiseWaapiHelper::PARENT, parentPtr) || !parentPtr)
			{
				return; // error parsing Json
			}
			
			auto parent = *parentPtr;

			FString parentName;
			if (!parent->TryGetStringField(WwiseWaapiHelper::NAME, parentName))
			{
				return;
			}

			if (parentName.IsEmpty())
			{
				parentName = TEXT("NewStateGroup");
			}

			assetName = FString::Printf(TEXT("%s-%s"), *parentName, *name);
			auto stringGroupId = parent->GetStringField(WwiseWaapiHelper::ID);

			FGuid::ParseExact(stringGroupId, EGuidFormats::DigitsWithHyphensInBraces, groupId);
		}

		auto ExistingObjectsByName = akAssetDatabase.AudioTypeMap.FindByName(assetName);
		if (ExistingObjectsByName.Num() > 0)
		{
			for (auto ExistingObject : ExistingObjectsByName)
			{
				UClass* existingClass = ExistingObject->GetClass();
				if (existingClass == assetType && akAssetDatabase.PendingAssetCreates.Contains(guidId))
				{
					// Found a new object with an existing name, we need to create it in rename
					return;
				}
			}
		}

		if (assetType)
		{
			if (akAssetDatabase.AudioTypeMap.Find(guidId))
			{
				ignoreRenames.Add(guidId);
			}

			akAssetDatabase.CreateOrRenameAsset(assetType, guidId, name, assetName, path.Replace(TEXT("\\"), TEXT("/")), groupId);
		}
		else
		{
			if (stringAssetType == TEXT("StateGroup") || stringAssetType == TEXT("SwitchGroup"))
			{
				akAssetDatabase.RenameGroupValues(guidId, name, path.Replace(TEXT("\\"), TEXT("/")));
			}
			else if (stringAssetType == TEXT("WorkUnit"))
			{
				const TSharedPtr<FJsonObject>* parentPtr = nullptr;
				if (!Response->TryGetObjectField(WwiseWaapiHelper::PARENT, parentPtr) || !parentPtr)
				{
					return; // error parsing Json
				}

				auto parent = *parentPtr;

				FString parentType;
				if (!parent->TryGetStringField(WwiseWaapiHelper::TYPE, parentType))
				{
					return;
				}

				if (parentType != TEXT("WorkUnit"))
				{
					return;
				}

				auto* oldPath = workUnitsToMove.Find(guidId);
				if (oldPath)
				{
					akAssetDatabase.MoveWorkUnit(*oldPath, path.Replace(TEXT("\\"), TEXT("/")));

					workUnitsToMove.Remove(guidId);
				}
			}
		}
	});
}

void WaapiAssetSynchronizer::onChildRemoved(uint64_t Id, TSharedPtr<FJsonObject> Response)
{
	if (FAkAudioDevice::Get()->IsBuildingData)
		return;

	AsyncTask(ENamedThreads::GameThread, [this, Response] {
		const TSharedPtr<FJsonObject>* childObjectPtr = nullptr;
		if (!Response->TryGetObjectField(WwiseWaapiHelper::CHILD, childObjectPtr) || !childObjectPtr)
			return;

		auto childObject = *childObjectPtr;

		FString stringId;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::ID, stringId))
			return; // error parsing Json

		FGuid childId;
		FGuid::ParseExact(stringId, EGuidFormats::DigitsWithHyphensInBraces, childId);

		FString childAssetType;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::TYPE, childAssetType))
			return; // error parsing Json

		FString childPath;
		if (!childObject->TryGetStringField(WwiseWaapiHelper::PATH, childPath))
			return; // error parsing Json

		if (childAssetType == TEXT("WorkUnit"))
		{
			const TSharedPtr<FJsonObject>* parentPtr = nullptr;
			if (!Response->TryGetObjectField(WwiseWaapiHelper::PARENT, parentPtr) || !parentPtr)
			{
				return; // error parsing Json
			}

			auto parent = *parentPtr;

			FString parentType;
			if (!parent->TryGetStringField(WwiseWaapiHelper::TYPE, parentType))
			{
				return;
			}

			if (parentType != TEXT("WorkUnit"))
			{
				return;
			}

			FString parentPath;
			if (!parent->TryGetStringField(WwiseWaapiHelper::PATH, parentPath))
			{
				return;
			}

			workUnitsToMove.Add(childId, FPaths::Combine(parentPath.Replace(TEXT("\\"), TEXT("/")), childPath.Replace(TEXT("\\"), TEXT("/"))));
		}
	});

}

void WaapiAssetSynchronizer::onRenamed(uint64_t Id, TSharedPtr<FJsonObject> Response)
{
	if (FAkAudioDevice::Get()->IsBuildingData)
		return;

	AsyncTask(ENamedThreads::GameThread, [this, Response]
		{
			const TSharedPtr<FJsonObject>* resultObjectPtr = nullptr;
			if (!Response->TryGetObjectField(WwiseWaapiHelper::OBJECT, resultObjectPtr) || !resultObjectPtr)
				return;
			auto resultObject = *resultObjectPtr;

			FString id;
			if (!resultObject->TryGetStringField(WwiseWaapiHelper::ID, id))
				return; // error parsing Json

			FGuid guidId;
			FGuid::ParseExact(id, EGuidFormats::DigitsWithHyphensInBraces, guidId);

			auto& akAssetDatabase = AkAssetDatabase::Get();

			FString oldName;
			Response->TryGetStringField(WwiseWaapiHelper::OLD_NAME, oldName);
			if (oldName.IsEmpty() && !akAssetDatabase.PendingAssetCreates.Contains(guidId))
			{
				// Invalid old name, and we're not trying to create this object, error happened
				return;
			}

			FString newName;
			Response->TryGetStringField(WwiseWaapiHelper::NEW_NAME, newName);

			FString assetName = newName;
			FString path;
			if (!resultObject->TryGetStringField(WwiseWaapiHelper::PATH, path))
				return; // error parsing Json

			FString assetType;
			if (!resultObject->TryGetStringField(WwiseWaapiHelper::TYPE, assetType))
				return; // error parsing Json

			FGuid groupId;
			if (assetType == TEXT("State") || assetType == TEXT("Switch"))
			{
				const TSharedPtr<FJsonObject>* parentPtr = nullptr;
				if (!resultObject->TryGetObjectField(WwiseWaapiHelper::PARENT, parentPtr) || !parentPtr)
					return; // error parsing Json

				auto parent = *parentPtr;
				assetName = FString::Printf(TEXT("%s-%s"), *parent->GetStringField(WwiseWaapiHelper::NAME), *newName);

				auto stringGroupId = parent->GetStringField(WwiseWaapiHelper::ID);
				FGuid::ParseExact(stringGroupId, EGuidFormats::DigitsWithHyphensInBraces, groupId);
			}

			if (assetType == TEXT("StateGroup") || assetType == TEXT("SwitchGroup"))
			{
				AkAssetDatabase::Get().RenameGroupValues(guidId, newName, path.Replace(TEXT("\\"), TEXT("/")));
			}
			else
			{
				const UClass* assetClass = WaapiAssetSynchronizer::GetClassByName(assetType);
				if (assetClass != nullptr)
				{
					ignoreRenames.Emplace(guidId);
					AkAssetDatabase::Get().CreateOrRenameAsset(assetClass, guidId, newName, assetName, path.Replace(TEXT("\\"), TEXT("/")), groupId);
				}
			}
		});
}

UClass* WaapiAssetSynchronizer::GetClassByName(const FString& stringAssetType)
{
	if (stringAssetType == TEXT("AuxBus"))
	{
		return UAkAuxBus::StaticClass();
	}
	else if (stringAssetType == TEXT("AcousticTexture"))
	{
		return UAkAcousticTexture::StaticClass();
	}
	else if (stringAssetType == TEXT("Event"))
	{
		return UAkAudioEvent::StaticClass();
	}
	else if (stringAssetType == TEXT("GameParameter"))
	{
		return UAkRtpc::StaticClass();
	}
	else if (stringAssetType == TEXT("State"))
	{
		return UAkStateValue::StaticClass();
	}
	else if (stringAssetType == TEXT("Switch"))
	{
		return UAkSwitchValue::StaticClass();
	}
	else if (stringAssetType == TEXT("Trigger"))
	{
		return UAkTrigger::StaticClass();
	}
	return nullptr;
}

void WaapiAssetSynchronizer::onAssetRemoved(const FAssetData& RemovedAssetData)
{
	if (PauseAssetRegistryDelegates)
	{
		return;
	}

	auto waapiClient = FAkWaapiClient::Get();
	if (!waapiClient)
		return;

	if (!FAkWaapiClient::IsProjectLoaded())
	{
		return;
	}

	auto assetInstance = Cast<UAkAudioType>(RemovedAssetData.GetAsset());
	if (!assetInstance)
	{
		return;
	}

	if (!assetInstance->ID.IsValid())
	{
		return;
	}

	if (!AkAssetDatabase::Get().Remove(assetInstance))
	{
		return;
	}

	TSharedRef<FJsonObject> args = MakeShared<FJsonObject>();
	args->SetStringField(WwiseWaapiHelper::OBJECT, assetInstance->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	TSharedRef<FJsonObject> options = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> result;
	if (!waapiClient->Call(ak::wwise::core::object::delete_, args, options, result))
	{
		UE_LOG(LogAkWaapiAssetSynchronizer, Error, TEXT("Unable to delete <%s> from path <%s> due to WAAPI error"), *RemovedAssetData.AssetName.GetPlainNameString(), *RemovedAssetData.ObjectPath.GetPlainNameString());
	}
}

void WaapiAssetSynchronizer::onAssetRenamed(const FAssetData& NewAssetData, const FString& OldPath)
{
	if (PauseAssetRegistryDelegates)
	{
		return;
	}

	auto waapiClient = FAkWaapiClient::Get();
	if (!waapiClient)
	{
		return;
	}

	if (!FAkWaapiClient::IsProjectLoaded())
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

	if (assetInstance->IsA<UAkGroupValue>())
	{
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

	TSharedRef<FJsonObject> options = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> result;

	auto guidString = assetInstance->ID.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	auto renameSucceeded = true;
	if (newFileName != oldFileName)
	{
		TSharedRef<FJsonObject> args = MakeShared<FJsonObject>();
		args->SetStringField(WwiseWaapiHelper::OBJECT, guidString);
		args->SetStringField(WwiseWaapiHelper::VALUE, newFileName);
		renameSucceeded = waapiClient->Call(ak::wwise::core::object::setName, args, options, result);
	}

	FString setNameErrorMessage;
	if (!renameSucceeded)
	{
		result->TryGetStringField(TEXT("message"), setNameErrorMessage);
	}

	auto moveSucceeded = true;
	if (newDirectory != oldDirectory)
	{
		TSharedRef<FJsonObject> args = MakeShared<FJsonObject>();
		args->SetStringField(WwiseWaapiHelper::OBJECT, guidString);
		args->SetStringField(WwiseWaapiHelper::PARENT, AkAssetDatabase::GetWwisePathFromAssetPath(newPath));
		moveSucceeded = waapiClient->Call(ak::wwise::core::object::move, args, options, result);
	}

	FString moveErrorMessage;
	if (!moveSucceeded)
	{
		result->TryGetStringField(TEXT("message"), moveErrorMessage);
	}

	if (renameSucceeded && moveSucceeded)
	{
		return;
	}

	if (!renameSucceeded)
	{
		const FFormatNamedArguments Args
		{
			{ TEXT("OldName"), FText::FromString(oldFileName) },
			{ TEXT("NewName"), FText::FromString(newFileName) },
			{ TEXT("Reason"), FText::FromString(setNameErrorMessage) }
		};

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("AkWaapiAssetSynchronizer", "CannotRenameAsset", "Cannot rename asset from '{OldName}' to '{NewName}'. Reason: {Reason}."), Args));
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

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("AkWaapiAssetSynchronizer", "CannotMoveAsset", "Cannot move '{OldName}' from '{OldPath}' to '{NewPath}'. Reason: {Reason}."), Args));
	}

	ignoreRenames.Add(assetInstance->ID);

	// This is required to avoid an assert in RenameAssets() when the asset is referenced somewhere
	GRedirectCollector.AddAssetPathRedirection(FName(*OldPath), FName(*newPath));

	FAssetRenameData renameData{ NewAssetData.ToSoftObjectPath(), FSoftObjectPath(OldPath) };

	auto& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().RenameAssets(TArray<FAssetRenameData>{renameData});

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets({ FAssetData(FSoftObjectPath(OldPath).TryLoad()) });
}

#undef LOCTEXT_NAMESPACE
