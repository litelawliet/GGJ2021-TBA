#include "AkEventBasedIntegrationBehavior.h"

#include "AkAssetBase.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"
#include "AkInitBank.h"
#include "AkMediaAsset.h"
#include "AkUnrealHelper.h"
#include "AkUnrealIOHook.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "CoreUObject/Public/UObject/UObjectIterator.h"
#include "Core/Public/Misc/Paths.h"

namespace AkEventBasedHelpers
{
	AKRESULT LoadBankFromMemoryInternal(FAkAudioDevice* AudioDevice, int64 DataBulkSize, AkBankID& BankID, const void*& RawData)
	{
		auto result = AudioDevice->LoadBankFromMemory(RawData, static_cast<uint32>(DataBulkSize), BankID);
		if (result != AK_Success && result != AK_BankAlreadyLoaded)
		{
			BankID = AK_INVALID_BANK_ID, RawData = nullptr;
		}
		return result;
	}
}

// AkAssetData
AKRESULT AkEventBasedIntegrationBehavior::AkAssetData_Load(UAkAssetData* AkAssetData)
{
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return AK_Success;

	auto dataBulkSize = AkAssetData->Data.GetBulkDataSize();
	if (dataBulkSize <= 0)
		return AK_Success;

#if WITH_EDITOR
	AkAssetData->EditorRawData.Reset(dataBulkSize);
	AkAssetData->RawData = FMemory::Memcpy(AkAssetData->EditorRawData.GetData(), AkAssetData->Data.LockReadOnly(), dataBulkSize);
	AkAssetData->Data.Unlock();
	return AkEventBasedHelpers::LoadBankFromMemoryInternal(AudioDevice, dataBulkSize, AkAssetData->BankID, AkAssetData->RawData);
#else
	AkAssetData->RawData = AkAssetData->Data.LockReadOnly();
	auto result = AkEventBasedHelpers::LoadBankFromMemoryInternal(AudioDevice, dataBulkSize, AkAssetData->BankID, AkAssetData->RawData);
	AkAssetData->Data.Unlock();
	return result;
#endif
}

// AkAudioBank
void AkEventBasedIntegrationBehavior::AkAudioBank_Load(UAkAudioBank* AkAudioBank)
{
	if (AkAudioBank->IsLocalized())
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			AkAudioBank->loadLocalizedData(audioDevice->GetCurrentAudioCulture(), SwitchLanguageCompletedFunction{});
		}
	}
	else
	{
		AkAudioBank->superLoad();
	}
}

void AkEventBasedIntegrationBehavior::AkAudioBank_Unload(UAkAudioBank* AkAudioBank)
{
	if (AkAudioBank->IsLocalized())
	{
		AkAudioBank->unloadLocalizedData();
	}
	else
	{
		AkAudioBank->superUnload();
	}
}

// AkAudioDevice
AKRESULT AkEventBasedIntegrationBehavior::AkAudioDevice_ClearBanks(FAkAudioDevice* AkAudioDevice)
{
	return AK_Success;
}

AKRESULT AkEventBasedIntegrationBehavior::AkAudioDevice_LoadInitBank(FAkAudioDevice* AkAudioDevice)
{
	if (AkAudioDevice->InitBank)
	{
		return AK_Success;
	}

	auto& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> initBankAssets;
	assetRegistryModule.Get().GetAssetsByClass(UAkInitBank::StaticClass()->GetFName(), initBankAssets);

	if (initBankAssets.Num() > 0)
	{
		AkAudioDevice->InitBank = Cast<UAkInitBank>(initBankAssets[0].GetAsset());
		AkAudioDevice->InitBank->Load();
		AkAudioDevice->InitBank->AddToRoot(); // Prevent InitBank for being garbage collected

		return AK_Success;
	}

	return AK_Fail;
}

bool AkEventBasedIntegrationBehavior::AkAudioDevice_LoadAllFilePackages(FAkAudioDevice* AkAudioDevice)
{
	return true;
}

bool AkEventBasedIntegrationBehavior::AkAudioDevice_UnloadAllFilePackages(FAkAudioDevice* AkAudioDevice)
{
	return true;
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_LoadAllReferencedBanks(FAkAudioDevice* AkAudioDevice)
{
	// Do nothing
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_SetCurrentAudioCulture(const FString& NewWwiseLanguage)
{
	bool switchedLanguage = true;

	for (TObjectIterator<UAkAudioEvent> eventIt; eventIt; ++eventIt)
	{
		if (eventIt->IsLocalized())
		{
			if (!eventIt->SwitchLanguage(NewWwiseLanguage))
			{
				switchedLanguage = false;
				break;
			}
		}
	}

	if (switchedLanguage)
	{
		for (TObjectIterator<UAkAudioBank> audioBankIt; audioBankIt; ++audioBankIt)
		{
			if (audioBankIt->IsLocalized())
			{
				if (!audioBankIt->SwitchLanguage(NewWwiseLanguage))
				{
					switchedLanguage = false;
					break;
				}
			}
		}
	}

	if (switchedLanguage)
	{
		AK::StreamMgr::SetCurrentLanguage(TCHAR_TO_AK(*NewWwiseLanguage));

		if (GEngine)
		{
			GEngine->ForceGarbageCollection();
		}
	}
	else
	{
		UE_LOG(LogAkAudio, Error, TEXT("Cannot switch to Wwise language '%s'"), *NewWwiseLanguage);
	}
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_SetCurrentAudioCultureAsync(FAkAudioDevice* AkAudioDevice, const FString& NewWwiseLanguage, const FOnSetCurrentAudioCultureCompleted& CompletedCallback)
{
	FAkAudioDevice::SetCurrentAudioCultureAsyncTask* newTask = new FAkAudioDevice::SetCurrentAudioCultureAsyncTask{};
	newTask->NewWwiseLanguage = NewWwiseLanguage;
	newTask->CompletedCallback = CompletedCallback;
	if (newTask->Start())
	{
		AkAudioDevice->audioCultureAsyncTasks.Add(newTask);
	}
	else
	{
		CompletedCallback.ExecuteIfBound(false);
		delete newTask;
	}
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_CreateIOHook(FAkAudioDevice* AkAudioDevice)
{
	AkAudioDevice->IOHook = new FAkUnrealIOHook();
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_LoadInitialData(FAkAudioDevice* AkAudioDevice)
{
	AkAudioDevice->LoadInitBank();
	AkAudioDevice->LoadDelayedAssets();
	AkAudioDevice->LoadDelayedMedias();
}

#if WITH_EDITOR
void AkEventBasedIntegrationBehavior::AkAudioDevice_UnloadAllSoundData(FAkAudioDevice* AkAudioDevice)
{
	if (!AkAudioDevice)
	{
		return;
	}

	AkAudioDevice->StopAllSounds();
	AK::SoundEngine::RenderAudio();
	FPlatformProcess::Sleep(0.1f);

	for (TObjectIterator<UAkMediaAsset> mediaIt; mediaIt; ++mediaIt)
	{
		mediaIt->Unload();
	}

	for (TObjectIterator<UAkAudioEvent> eventIt; eventIt; ++eventIt)
	{
		eventIt->Unload();
	}

	for (TObjectIterator<UAkAuxBus> auxBusIt; auxBusIt; ++auxBusIt)
	{
		auxBusIt->Unload();
	}

	for (TObjectIterator<UAkAudioBank> audioBankIt; audioBankIt; ++audioBankIt)
	{
		audioBankIt->Unload();
	}

	for (TObjectIterator<UAkInitBank> initBankIt; initBankIt; ++initBankIt)
	{
		initBankIt->Unload();
	}

	AkAudioDevice->InitBank = nullptr;
}

void AkEventBasedIntegrationBehavior::AkAudioDevice_ReloadAllSoundData(FAkAudioDevice* AkAudioDevice)
{
	AkAudioDevice_UnloadAllSoundData(AkAudioDevice);

	AkAudioDevice->LoadInitBank();

	for (TObjectIterator<UAkInitBank> initBankIt; initBankIt; ++initBankIt)
	{
		initBankIt->Load();
	}

	for (TObjectIterator<UAkAudioBank> audioBankIt; audioBankIt; ++audioBankIt)
	{
		audioBankIt->Load();
	}

	for (TObjectIterator<UAkAudioEvent> eventIt; eventIt; ++eventIt)
	{
		eventIt->Load();
	}

	for (TObjectIterator<UAkAuxBus> auxBusIt; auxBusIt; ++auxBusIt)
	{
		auxBusIt->Load();
	}

	for (TObjectIterator<UAkMediaAsset> mediaIt; mediaIt; ++mediaIt)
	{
		mediaIt->Load();
	}

	AkAudioDevice->OnSoundbanksLoaded.Broadcast();
}
#endif

// AkAudioEvent
void AkEventBasedIntegrationBehavior::AkAudioEvent_Load(UAkAudioEvent* AkAudioEvent)
{
	if (AkAudioEvent->IsLocalized())
	{
		if (auto* audioDevice = FAkAudioDevice::Get())
		{
			AkAudioEvent->loadLocalizedData(audioDevice->GetCurrentAudioCulture(), SwitchLanguageCompletedFunction{});
		}
	}
	else
	{
		AkAudioEvent->superLoad();
	}
}

// AkGameplayStatics
void AkEventBasedIntegrationBehavior::AkGameplayStatics_LoadBank(UAkAudioBank* AkAudioBank, const FString& BankName, FWaitEndBankAction* NewAction)
{
	NewAction->ActionDone = true;
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_LoadBankAsync(UAkAudioBank* AkAudioBank, const FOnAkBankCallback& BankLoadedCallback)
{
	BankLoadedCallback.ExecuteIfBound(EAkResult::Success);
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_LoadBankByName(const FString& BankName)
{
	// Do nothing
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_LoadBanks(const TArray<UAkAudioBank*>& SoundBanks, bool SynchronizeSoundBanks)
{
	// Do nothing
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_UnloadBank(UAkAudioBank* Bank, const FString& BankName, FWaitEndBankAction* NewAction)
{
	NewAction->ActionDone = true;
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_UnloadBankAsync(UAkAudioBank* Bank, const FOnAkBankCallback& BankUnloadedCallback)
{
	BankUnloadedCallback.ExecuteIfBound(EAkResult::Success);
}

void AkEventBasedIntegrationBehavior::AkGameplayStatics_UnloadBankByName(const FString& BankName)
{
	// Do nothing
}

// FAkSDKExternalSourceArray
void AkEventBasedIntegrationBehavior::FAkSDKExternalSourceArray_Ctor(FAkSDKExternalSourceArray* Instance, const TArray<FAkExternalSourceInfo>& BlueprintArray)
{
	if (auto* AudioDevice = FAkAudioDevice::Get())
	{
		for (auto& externalSourceInfo : BlueprintArray)
		{
			AkOSChar* OsCharArray = nullptr;
			void* mediaData = nullptr;
			AkUInt32 mediaSize = 0;

			if (externalSourceInfo.ExternalSourceAsset)
			{
				if (externalSourceInfo.IsStreamed)
				{
					auto assetName = externalSourceInfo.ExternalSourceAsset->GetName();
					OsCharArray = (AkOSChar*)FMemory::Malloc((assetName.Len() + 1) * sizeof(AkOSChar));
					FPlatformString::Strcpy(OsCharArray, assetName.Len(), TCHAR_TO_AK(*(assetName)));
					FAkUnrealIOHook::AddExternalMedia(externalSourceInfo.ExternalSourceAsset);
				}
				else
				{
					// TODO: Use C++17 structured binding when available
					auto dataInfo = externalSourceInfo.ExternalSourceAsset->GetExternalSourceData();
					mediaData = dataInfo.Key;
					mediaSize = static_cast<AkUInt32>(dataInfo.Value);
				}
			}
			else
			{
				auto externalFileName = externalSourceInfo.FileName;
				if (FPaths::GetExtension(externalFileName).IsEmpty())
				{
					externalFileName += TEXT(".wem");
				}
				OsCharArray = (AkOSChar*)FMemory::Malloc((externalFileName.Len() + 1) * sizeof(AkOSChar));
				FPlatformString::Strcpy(OsCharArray, externalFileName.Len(), TCHAR_TO_AK(*(externalFileName)));

				auto assetName = FPaths::GetBaseFilename(externalFileName);
				auto assetPath = FPaths::Combine(AkUnrealHelper::GetExternalSourceAssetPackagePath(), FString::Format(TEXT("{0}.{0}"), { *assetName }));

				auto& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				auto assetData = assetRegistryModule.Get().GetAssetByObjectPath(FName(*assetPath));
				if (assetData.IsValid())
				{
					auto* asset = assetData.GetAsset();
					asset->AddToRoot();
					FAkUnrealIOHook::AddExternalMedia(Cast<UAkExternalMediaAsset>(asset));
				}
			}

			if (mediaData && mediaSize)
			{
				Instance->ExternalSourceArray.Emplace(mediaData, mediaSize, AudioDevice->GetIDFromString(TCHAR_TO_AK(*externalSourceInfo.ExternalSrcName)), (AkCodecID)externalSourceInfo.CodecID);
			}
			else
			{
				Instance->ExternalSourceArray.Emplace(OsCharArray, AudioDevice->GetIDFromString(TCHAR_TO_AK(*externalSourceInfo.ExternalSrcName)), (AkCodecID)externalSourceInfo.CodecID);
			}
		}
	}
}

