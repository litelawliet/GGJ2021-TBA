// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkGameObject.cpp:
=============================================================================*/

#include "AkGameObject.h"
#include "AkAudioEvent.h"
#include "AkMediaAsset.h"
#include "AkComponentCallbackManager.h"
#include "AkMediaAsset.h"

class FPostAssociatedEventAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink = 0;
	FWeakObjectPtr CallbackTarget;
	int32* PlayingID = nullptr;
	TFuture<AkPlayingID> FuturePlayingID;
	TArray<FAkExternalSourceInfo> ExternalSources;
	UAkAudioEvent* AkEvent = nullptr;

	FPostAssociatedEventAction(const FLatentActionInfo& LatentInfo, int32* PlayingID, const TArray<FAkExternalSourceInfo>& ExtSrc, UAkAudioEvent* Event)
		: ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
		, PlayingID(PlayingID)
		, ExternalSources(ExtSrc)
		, AkEvent(Event)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		bool futureIsReady = FuturePlayingID.IsReady();
		if (futureIsReady)
		{
			*PlayingID = FuturePlayingID.Get();
		}
		else if (AkEvent)
		{
			for (auto ExtSrc : ExternalSources)
			{
				if (ExtSrc.ExternalSourceAsset)
				{
					ExtSrc.ExternalSourceAsset->AddPlayingID(AkEvent->ShortID, *PlayingID);
				}
			}
		}

		Response.FinishAndTriggerIf(futureIsReady, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("Waiting for posted AkEvent to load media.");
	}
#endif
};

UAkGameObject::UAkGameObject(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	bStarted = false;
}

int32 UAkGameObject::PostAssociatedAkEvent(int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources)
{
	return PostAkEvent(AkAudioEvent, CallbackMask, PostEventCallback, ExternalSources, EventName);
}

int32 UAkGameObject::PostAssociatedAkEvent(int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback)
{
	return PostAkEvent(AkAudioEvent, CallbackMask, PostEventCallback, TArray<FAkExternalSourceInfo>(), EventName);
}

int32 UAkGameObject::PostAkEvent(class UAkAudioEvent * AkEvent, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources, const FString& in_EventName)
{
	return PostAkEventByNameWithDelegate(GET_AK_EVENT_NAME(AkEvent, in_EventName), CallbackMask, PostEventCallback, ExternalSources);
}

int32 UAkGameObject::PostAkEvent(class UAkAudioEvent * AkEvent, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const FString& in_EventName)
{
	return PostAkEventByNameWithDelegate(GET_AK_EVENT_NAME(AkEvent, in_EventName), CallbackMask, PostEventCallback, TArray<FAkExternalSourceInfo>());
}

AkPlayingID UAkGameObject::PostAkEventByNameWithDelegate(const FString& in_EventName, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	auto AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		if (ExternalSources.Num() > 0)
		{
			FAkSDKExternalSourceArray SDKExternalSrcInfo(ExternalSources);
			playingID = AudioDevice->PostEvent(in_EventName, this, PostEventCallback, CallbackMask, SDKExternalSrcInfo.ExternalSourceArray);
			if (playingID != AK_INVALID_PLAYING_ID)
			{
				for (auto ExtSrc : ExternalSources)
				{
					if (ExtSrc.ExternalSourceAsset)
					{
						ExtSrc.ExternalSourceAsset->AddPlayingID(AudioDevice->GetIDFromString(in_EventName), playingID);
					}
				}
			}
		}
		else
		{
			playingID = AudioDevice->PostEvent(in_EventName, this, PostEventCallback, CallbackMask);
		}
		if (playingID != AK_INVALID_PLAYING_ID)
			bStarted = true;
	}

	return playingID;
}

void UAkGameObject::PostAssociatedAkEventAsync(const UObject* WorldContextObject, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources, FLatentActionInfo LatentInfo, int32& PlayingID)
{
	PostAkEventAsyncByEvent(WorldContextObject, AkAudioEvent, CallbackMask, PostEventCallback, ExternalSources, LatentInfo, PlayingID);
}

void UAkGameObject::PostAkEventAsync(const UObject* WorldContextObject,
	UAkAudioEvent* AkEvent,
	int32& PlayingID,
	int32 CallbackMask,
	const FOnAkPostEventCallback& PostEventCallback,
	const TArray<FAkExternalSourceInfo>& ExternalSources,
	FLatentActionInfo LatentInfo
)
{
	PostAkEventAsyncByEvent(WorldContextObject, AkEvent, CallbackMask, PostEventCallback, ExternalSources, LatentInfo, PlayingID);
}

void UAkGameObject::PostAkEventAsyncByEvent(const UObject* WorldContextObject,
	class UAkAudioEvent* AkEvent,
	int32 CallbackMask,
	const FOnAkPostEventCallback& PostEventCallback,
	const TArray<FAkExternalSourceInfo>& ExternalSources,
	FLatentActionInfo LatentInfo,
	int32& PlayingID
)
{
	AkDeviceAndWorld DeviceAndWorld(WorldContextObject);
	FLatentActionManager& LatentActionManager = DeviceAndWorld.CurrentWorld->GetLatentActionManager();
	FPostAssociatedEventAction* NewAction = LatentActionManager.FindExistingAction<FPostAssociatedEventAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
	if (!NewAction)
	{
		NewAction = new FPostAssociatedEventAction(LatentInfo, &PlayingID, ExternalSources, AkEvent);
		if (ExternalSources.Num() > 0)
		{
			FAkSDKExternalSourceArray SDKExternalSrcInfo(ExternalSources);
			NewAction->FuturePlayingID = DeviceAndWorld.AkAudioDevice->PostEventAsync(AkEvent, this, PostEventCallback, CallbackMask, SDKExternalSrcInfo.ExternalSourceArray);
		}
		else
		{
			NewAction->FuturePlayingID = DeviceAndWorld.AkAudioDevice->PostEventAsync(AkEvent, this, PostEventCallback, CallbackMask);
		}

		LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
	}
}

bool UAkGameObject::VerifyEventName(const FString& in_EventName) const
{
	const bool IsEventNameEmpty = in_EventName.IsEmpty();
	if (IsEventNameEmpty)
	{
		FString OwnerName = FString(TEXT(""));
		FString ObjectName = GetName();

		const auto owner = GetOwner();
		if (owner)
			OwnerName = owner->GetName();

		UE_LOG(LogAkAudio, Warning, TEXT("[%s.%s] AkGameObject: Attempted to post an empty AkEvent name."), *OwnerName, *ObjectName);
	}

	return !IsEventNameEmpty;
}

bool UAkGameObject::AllowAudioPlayback() const
{
	UWorld* CurrentWorld = GetWorld();
	return (CurrentWorld && CurrentWorld->AllowAudioPlayback() && !IsBeingDestroyed());
}

AkGameObjectID UAkGameObject::GetAkGameObjectID() const
{
	return (AkGameObjectID)this;
}

void UAkGameObject::Stop()
{
	if (FAkAudioDevice::Get() && IsRegisteredWithWwise)
	{
		AK::SoundEngine::StopAll(GetAkGameObjectID());
		AK::SoundEngine::RenderAudio();
	}
}

bool UAkGameObject::HasActiveEvents() const
{
	auto CallbackManager = FAkComponentCallbackManager::GetInstance();
	return (CallbackManager != nullptr) && CallbackManager->HasActiveEvents(GetAkGameObjectID());
}
