// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#include "AkBankManager.h"

#include "AkAudioBank.h"
#include "AkAudioDevice.h"
#include "AkInclude.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"

FAkBankManager* FAkBankManager::Instance = nullptr;

FAkBankManager* FAkBankManager::GetInstance()
{
	return Instance;
}

void FAkBankFunctionPtrCallbackInfo::HandleAction(AkUInt32 BankID, const void * InMemoryBankPtr, AKRESULT ActionResult)
{
	if (CallbackFunc)
	{
		// Call the user's callback function
		CallbackFunc(BankID, InMemoryBankPtr, ActionResult, UserCookie);
	}
}

void FAkBankLatentActionCallbackInfo::HandleAction(AkUInt32 BankID, const void * InMemoryBankPtr, AKRESULT ActionResult)
{
	if (BankLatentAction)
	{
		BankLatentAction->ActionDone = true;
	}
}

void FAkBankBlueprintDelegateCallbackInfo::HandleAction(AkUInt32 BankID, const void * InMemoryBankPtr, AKRESULT ActionResult)
{
	if (BankBlueprintCallback.IsBound())
	{
		auto CachedBlueprintCallback = BankBlueprintCallback;
		AsyncTask(ENamedThreads::GameThread, [ActionResult, CachedBlueprintCallback]()
		{
			CachedBlueprintCallback.ExecuteIfBound((EAkResult)ActionResult);
		});
	}
}

void FAkBankManager::BankLoadCallback(
	AkUInt32		in_bankID,
	const void *	in_pInMemoryBankPtr,
	AKRESULT		in_eLoadResult,
	void *			in_pCookie
)
{
	if (in_pCookie)
	{
		IAkBankCallbackInfo* BankCbInfo = (IAkBankCallbackInfo*)in_pCookie;
		if (in_eLoadResult == AK_Success)
		{
			if (BankCbInfo->Bank)
			{
				// Load worked; put the bank in the list.
				GetInstance()->AddLoadedBank(BankCbInfo->Bank);
			}
		}

		BankCbInfo->HandleAction(in_bankID, in_pInMemoryBankPtr, in_eLoadResult);

		delete BankCbInfo;
	}
}

void FAkBankManager::BankUnloadCallback(
	AkUInt32		in_bankID,
	const void *	in_pInMemoryBankPtr,
	AKRESULT		in_eUnloadResult,
	void *			in_pCookie
)
{
	if (in_pCookie)
	{
		IAkBankCallbackInfo* BankCbInfo = (IAkBankCallbackInfo*)in_pCookie;
		if (in_eUnloadResult == AK_Success)
		{
			if (BankCbInfo->Bank)
			{
				// Load worked; put the bank in the list.
				GetInstance()->RemoveLoadedBank(BankCbInfo->Bank);
			}
		}

		BankCbInfo->HandleAction(in_bankID, in_pInMemoryBankPtr, in_eUnloadResult);

		delete BankCbInfo;
	}
}

FAkBankManager::FAkBankManager()
{
	if (Instance)
	{
		UE_LOG(LogInit, Error, TEXT("FAkBankManager has already been instantiated."));
	}

	Instance = this;
}

FAkBankManager::~FAkBankManager()
{
	if (Instance == this)
	{
		TSet<UAkAudioBank*> LoadedBanksCopy(m_LoadedBanks);
		for (auto* AudioBank : LoadedBanksCopy)
		{
			if (AudioBank != nullptr && AudioBank->IsValidLowLevel())
			{
				AudioBank->Unload();
			}
		}

		Instance = nullptr;
	}
}

void FAkBankManager::addToGCStorage(UAkAudioBank* Bank)
{
	if (IsInGameThread())
	{
		addToGCStorageInternal(Bank);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, Bank] {
			addToGCStorageInternal(Bank);
		});
	}
}

void FAkBankManager::addToGCStorageInternal(UAkAudioBank* Bank)
{
	for (auto& entry : gcStorage)
	{
		if (entry.Get() == Bank)
		{
			return;
		}
	}

	gcStorage.Emplace(Bank);
}

void FAkBankManager::removeFromGCStorage(UAkAudioBank* Bank)
{
	if (IsInGameThread())
	{
		removeFromGCStorageInternal(Bank);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, Bank] {
			removeFromGCStorageInternal(Bank);
		});
	}
}

void FAkBankManager::removeFromGCStorageInternal(UAkAudioBank* Bank)
{
	auto removeIndex = gcStorage.IndexOfByPredicate([Bank](auto& item) { return item.Get() == Bank; });
	if (removeIndex != -1)
	{
		gcStorage.RemoveAt(removeIndex);
	}
}
