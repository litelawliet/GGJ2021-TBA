// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAudioDevice.cpp: Audiokinetic Audio interface object.
=============================================================================*/

#define AK_ENABLE_ROOMS
#define AK_ENABLE_PORTALS

#include "AkAudioDevice.h"

#include "AkAcousticPortal.h"
#include "AkAudioBank.h"
#include "AkAudioEvent.h"
#include "AkAudioModule.h"
#include "AkAuxBus.h"
#include "AkBankManager.h"
#include "AkCallbackInfoPool.h"
#include "AkComponent.h"
#include "AkComponentCallbackManager.h"
#include "AkGameObject.h"
#include "AkInitBank.h"
#include "AkLateReverbComponent.h"
#include "AkMediaAsset.h"
#include "AkRoomComponent.h"
#include "AkRtpc.h"
#include "AkSettings.h"
#include "AkSpotReflector.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "AkUEFeatures.h"
#include "AkUnrealHelper.h"
#include "AkWaapiClient.h"
#include "AkWaapiUtils.h"
#include "AssetRegistry/Public/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Async/TaskGraphInterfaces.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BrushComponent.h"
#include "EditorSupportDelegates.h"
#include "Engine/GameEngine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "IAkUnrealIOHook.h"
#include "InitializationSettings/AkInitializationSettings.h"
#include "IntegrationBehavior/AkIntegrationBehavior.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "ISettingsModule.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "Model.h"
#include "Platforms/AkUEPlatform.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "CameraController.h"
#include "Editor.h"
#include "UnrealEdMisc.h"
#endif

#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

// Wwise plug-ins
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/AkMeterFXFactory.h>

#if AK_SUPPORT_OPUS
#include <AK/Plugin/AkOpusDecoderFactory.h>
#endif // AK_SUPPORT_OPUS

DEFINE_LOG_CATEGORY(LogAkAudio);

/*------------------------------------------------------------------------------------
	Statics and Globals
------------------------------------------------------------------------------------*/

bool FAkAudioDevice::m_bSoundEngineInitialized = false;
bool FAkAudioDevice::m_EngineExiting = false;
TSet<UAkAudioType*> FAkAudioDevice::AssetsWithDelayedLoad;
TSet<UAkMediaAsset*> FAkAudioDevice::MediasWithDelayedLoad;
TMap<uint32, TArray<uint32>> FAkAudioDevice::EventToPlayingIDMap;
TMap<uint32, TSet<UAkAudioEvent*>> FAkAudioDevice::PlayingIDToPinnedAudioEventMap;
TMap<uint32, TSet<UAkExternalMediaAsset*>> FAkAudioDevice::PlayingIDToPinnedExternalSourceMap;

FCriticalSection FAkAudioDevice::EventToPlayingIDMapCriticalSection;
FCriticalSection FAkAudioDevice::PlayingIDToPinnedAudioEventMapCriticalSection;
FCriticalSection FAkAudioDevice::PlayingIDToPinnedExternalSourceMapCriticalSection;

/*------------------------------------------------------------------------------------
	Defines
------------------------------------------------------------------------------------*/

#define GAME_OBJECT_MAX_STRING_SIZE 512
#define AK_READ_SIZE DVD_MIN_READ_SIZE

static constexpr auto InvariantLCID = 0x7F;

DECLARE_STATS_GROUP(TEXT("AkAudioDevice"), STATGROUP_AkAudioDevice, STATCAT_Audiokinetic);
DECLARE_CYCLE_STAT(TEXT("Post Event Async"), STAT_PostEventAsync, STATGROUP_AkAudioDevice);

/*------------------------------------------------------------------------------------
	Helpers
------------------------------------------------------------------------------------*/

namespace FAkAudioDevice_Helpers
{
	void RegisterGameObject(AkGameObjectID in_gameObjId, const FString& Name)
	{
#ifdef AK_OPTIMIZED
		AK::SoundEngine::RegisterGameObj(in_gameObjId);
#else
		if (Name.Len() > 0)
		{
			AK::SoundEngine::RegisterGameObj(in_gameObjId, TCHAR_TO_ANSI(*Name));
		}
		else
		{
			AK::SoundEngine::RegisterGameObj(in_gameObjId);
		}
#endif
	}

	typedef TMap<AkGlobalCallbackLocation, FAkAudioDeviceDelegates::FOnAkGlobalCallback> FDelegateLocationMap;
	FDelegateLocationMap DelegateLocationMap;

	void GlobalCallback(AK::IAkGlobalPluginContext* Context, AkGlobalCallbackLocation Location, void* Cookie)
	{
		const FAkAudioDeviceDelegates::FOnAkGlobalCallback* Delegate = DelegateLocationMap.Find(Location);
		if (Delegate && Delegate->IsBound())
		{
			Delegate->Broadcast(Context, Location);
		}
	}

	void UnregisterGlobalCallbackDelegate(FAkAudioDeviceDelegates::FOnAkGlobalCallback* Delegate, FDelegateHandle Handle, AkGlobalCallbackLocation Location)
	{
		if (!Delegate)
			return;

		Delegate->Remove(Handle);
		if (Delegate->IsBound())
			return;

		AK::SoundEngine::UnregisterGlobalCallback(GlobalCallback, Location);
	}

	void UnregisterAllGlobalCallbacks()
	{
		for (auto DelegateLocationPair : FAkAudioDevice_Helpers::DelegateLocationMap)
		{
			auto Location = DelegateLocationPair.Key;
			AK::SoundEngine::UnregisterGlobalCallback(GlobalCallback, Location);
		}
		FAkAudioDevice_Helpers::DelegateLocationMap.Empty();
	}
}

/*------------------------------------------------------------------------------------
	Implementation
------------------------------------------------------------------------------------*/

FDelegateHandle FAkAudioDevice::RegisterGlobalCallback(FAkAudioDeviceDelegates::FOnAkGlobalCallback::FDelegate Callback, AkGlobalCallbackLocation Location)
{
	auto& Delegate = FAkAudioDevice_Helpers::DelegateLocationMap.FindOrAdd(Location);
	FDelegateHandle Handle = Delegate.Add(Callback);
	auto result = AK::SoundEngine::RegisterGlobalCallback(FAkAudioDevice_Helpers::GlobalCallback, Location);
	if (result != AK_Success)
	{
		FAkAudioDevice_Helpers::UnregisterGlobalCallbackDelegate(&Delegate, Handle, Location);
		Handle.Reset();
	}

	return Handle;
}

void FAkAudioDevice::UnregisterGlobalCallback(FDelegateHandle Handle, AkGlobalCallbackLocation Location)
{
	const auto& Delegate = FAkAudioDevice_Helpers::DelegateLocationMap.Find(Location);
	FAkAudioDevice_Helpers::UnregisterGlobalCallbackDelegate(Delegate, Handle, Location);
}

#if WITH_EDITORONLY_DATA
UAkComponent* FAkAudioDevice::CreateListener(UWorld* World, FEditorViewportClient* ViewportClient)
{
	if (!IsRunningGame())
	{
		FString ComponentName = TEXT("AkListener_") + World->GetName();
		if (ViewportClient)
		{
			ComponentName = TEXT("AkListener_") + FString::FromInt(ViewportClient->ViewIndex) + World->GetName();
		}
		UAkComponent* Listener = NewObject<UAkComponent>(World->GetWorldSettings(), FName(*ComponentName), RF_Transient);
		if (Listener != nullptr)
		{
			Listener->MarkAsEditorOnlySubobject();
			Listener->RegisterComponentWithWorld(World);
			AddDefaultListener(Listener);
		}

		return Listener;
	}
	else
	{
		return nullptr;
	}
}

FTransform FAkAudioDevice::GetEditorListenerPosition(int32 ViewIndex) const
{
	if (ViewIndex < ListenerTransforms.Num())
	{
		return ListenerTransforms[ViewIndex];
	}

	return FTransform();
}

#endif

namespace FAkAudioDevice_WaapiHelper
{
	void Subscribe(FAkWaapiClient* waapiClient, uint64& subscriptionId, const char* uri, const TSharedRef<FJsonObject>& options, WampEventCallback callback)
	{
		if (subscriptionId == 0)
		{
			TSharedPtr<FJsonObject> result;
			waapiClient->Subscribe(uri, options, callback, subscriptionId, result);
		}
	}

	void RemoveCallback(FAkWaapiClient* waapiClient, uint64& subscriptionId)
	{
		if (subscriptionId > 0)
		{
			waapiClient->RemoveWampEventCallback(subscriptionId);
			subscriptionId = 0;
		}
	}

	void Unsubscribe(FAkWaapiClient* waapiClient, uint64& subscriptionId)
	{
		if (subscriptionId > 0)
		{
			TSharedPtr<FJsonObject> result;
			waapiClient->Unsubscribe(subscriptionId, result);

			RemoveCallback(waapiClient, subscriptionId);
		}
	}
}

/**
 * Initializes the audio device and creates sources.
 *
 * @return true if initialization was successful, false otherwise
 */
bool FAkAudioDevice::Init()
{
#if UE_SERVER
	return false;
#endif
	AkBankManager = nullptr;
	if (!EnsureInitialized()) // ensure audiolib is initialized
	{
		UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device initialization failed."));
		return false;
	}

#if !WITH_EDITOR
	if (auto* akSettings = GetDefault<UAkSettings>())
	{
		for (auto& entry : akSettings->UnrealCultureToWwiseCulture)
		{
			auto culturePtr = FInternationalization::Get().GetCulture(entry.Key);
			if (culturePtr && culturePtr->GetLCID() != InvariantLCID)
			{
				CachedUnrealToWwiseCulture.Add(culturePtr, entry.Value);
			}
		}
	}
#endif

	

#if AK_SUPPORT_WAAPI
	if (auto waapiClient = FAkWaapiClient::Get())
	{
		ProjectLoadedHandle = waapiClient->OnProjectLoaded.AddLambda([this, waapiClient]
		{
			if (!waapiClient->IsConnected())
				return;

			TSharedRef<FJsonObject> options = MakeShareable(new FJsonObject());
			options->SetArrayField(WwiseWaapiHelper::RETURN, TArray<TSharedPtr<FJsonValue>> { MakeShareable(new FJsonValueString(WwiseWaapiHelper::PARENT)) });

			auto wampEventCallback = WampEventCallback::CreateLambda([this](uint64_t id, TSharedPtr<FJsonObject> in_UEJsonObject)
			{
				AsyncTask(ENamedThreads::GameThread, [this]
				{
#if WITH_EDITOR
					FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif
					OnWwiseProjectModification.Broadcast();
				});
			});

			FAkAudioDevice_WaapiHelper::Subscribe(waapiClient, WaapiSubscriptionIds.Renamed, ak::wwise::core::object::nameChanged, options, wampEventCallback);
			FAkAudioDevice_WaapiHelper::Subscribe(waapiClient, WaapiSubscriptionIds.PreDeleted, ak::wwise::core::object::preDeleted, options, wampEventCallback);
			FAkAudioDevice_WaapiHelper::Subscribe(waapiClient, WaapiSubscriptionIds.ChildRemoved, ak::wwise::core::object::childRemoved, options, wampEventCallback);
			FAkAudioDevice_WaapiHelper::Subscribe(waapiClient, WaapiSubscriptionIds.ChildAdded, ak::wwise::core::object::childAdded, options, wampEventCallback);
			FAkAudioDevice_WaapiHelper::Subscribe(waapiClient, WaapiSubscriptionIds.Created, ak::wwise::core::object::created, options, wampEventCallback);
		});

		ConnectionLostHandle = waapiClient->OnConnectionLost.AddLambda([this, waapiClient]
		{
			if (!waapiClient->IsConnected())
				return;

			FAkAudioDevice_WaapiHelper::RemoveCallback(waapiClient, WaapiSubscriptionIds.Renamed);
			FAkAudioDevice_WaapiHelper::RemoveCallback(waapiClient, WaapiSubscriptionIds.PreDeleted);
			FAkAudioDevice_WaapiHelper::RemoveCallback(waapiClient, WaapiSubscriptionIds.ChildRemoved);
			FAkAudioDevice_WaapiHelper::RemoveCallback(waapiClient, WaapiSubscriptionIds.ChildAdded);
			FAkAudioDevice_WaapiHelper::RemoveCallback(waapiClient, WaapiSubscriptionIds.Created);
		});

		ClientBeginDestroyHandle = waapiClient->OnClientBeginDestroy.AddLambda([this, waapiClient]
		{
			if (ProjectLoadedHandle.IsValid())
			{
				waapiClient->OnProjectLoaded.Remove(ProjectLoadedHandle);
				ProjectLoadedHandle.Reset();
			}

			if (ConnectionLostHandle.IsValid())
			{
				waapiClient->OnConnectionLost.Remove(ConnectionLostHandle);
				ConnectionLostHandle.Reset();
			}
		});
	}
#endif // AK_SUPPORT_WAAPI

	FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[&](UWorld* World, const UWorld::InitializationValues IVS)
		{
			World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateRaw(this, &FAkAudioDevice::OnActorSpawned));
		}
	);
	
	FWorldDelegates::OnWorldCleanup.AddLambda(
		[this](UWorld* World, bool bSessionEnded, bool bCleanupResources)
		{
			CleanupComponentMapsForWorld(World);
		}
	);

#if WITH_EDITOR
	FEditorSupportDelegates::PrepareToCleanseEditorObject.AddLambda
	(
		[this](UObject* Object)
		{
			auto Level = Cast<ULevel>(Object);
			if (Level != nullptr)
			{
		}
	});
#endif// WITH_EDITOR

	m_SpatialAudioListener = nullptr;

#if WITH_EDITORONLY_DATA
	if (!IsRunningGame())
	{
		static const FName kLevelEditorModuleName = TEXT("LevelEditor");

		auto MapChangedHandler = [this](UWorld* World, EMapChangeType MapChangeType)
		{
			if (World && World->AllowAudioPlayback() && World->WorldType == EWorldType::Editor)
			{
				if (MapChangeType == EMapChangeType::TearDownWorld)
				{
					if (EditorListener && World == EditorListener->GetWorld())
					{
						EditorListener->DestroyComponent();
						EditorListener = nullptr;
					}
				}
				else if (EditorListener == nullptr && MapChangeType != EMapChangeType::SaveMap)
				{
					EditorListener = CreateListener(World);

					// The Editor Listener should NEVER be the spatial audio listener
					if (m_SpatialAudioListener == EditorListener)
					{
						AK::SpatialAudio::UnregisterListener(m_SpatialAudioListener->GetAkGameObjectID());
						m_SpatialAudioListener = nullptr;
					}
				}
			}
		};

		auto LevelEditorModulePtr = FModuleManager::Get().GetModulePtr<FLevelEditorModule>(kLevelEditorModuleName);
		if (LevelEditorModulePtr)
		{
			LevelEditorModulePtr->OnMapChanged().AddLambda(MapChangedHandler);
		}
		else
		{
			FModuleManager::Get().OnModulesChanged().AddLambda([this, MapChangedHandler](FName InModuleName, EModuleChangeReason Reason)
			{
				if (InModuleName == kLevelEditorModuleName && Reason == EModuleChangeReason::ModuleLoaded)
				{
					auto& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(kLevelEditorModuleName);
					LevelEditorModule.OnMapChanged().AddLambda(MapChangedHandler);
				}
			});
		}

		FEditorDelegates::OnEditorCameraMoved.AddLambda(
			[&](const FVector& Location, const FRotator& Rotation, ELevelViewportType ViewportType, int32 ViewIndex)
			{
#if UE_4_22_OR_LATER
			auto& allViewportClient = GEditor->GetAllViewportClients();
#else
			auto& allViewportClient = GEditor->AllViewportClients;
#endif
				if (allViewportClient[ViewIndex]->Viewport && allViewportClient[ViewIndex]->Viewport->HasFocus())
				{
					if (ListenerTransforms.Num() <= ViewIndex)
					{
						ListenerTransforms.AddDefaulted(ViewIndex - ListenerTransforms.Num() + 1);
					}
					ListenerTransforms[ViewIndex].SetLocation(Location);
					ListenerTransforms[ViewIndex].SetRotation(Rotation.Quaternion());

					UWorld * ViewportWorld = allViewportClient[ViewIndex]->GetWorld();
					if (ViewportWorld && ViewportWorld->WorldType == EWorldType::PIE)
					{
						auto Quat = Rotation.Quaternion();
						AkSoundPosition soundpos;
						FVectorsToAKTransform(
							Location,
							Quat.GetForwardVector(),
							Quat.GetUpVector(),
							soundpos
						);

						SetPosition(EditorListener, soundpos);
					}
				}
			}
		);

		FEditorDelegates::BeginPIE.AddLambda(
			[&](const bool bIsSimulating)
			{
				if (!bIsSimulating)
				{
					RemoveDefaultListener(EditorListener);
				}
			}
		);

		FEditorDelegates::EndPIE.AddLambda(
			[&](const bool bIsSimulating)
			{
				if (!bIsSimulating)
				{
					AddDefaultListener(EditorListener);
					// The Editor Listener should NEVER be the spatial audio listener
					if (m_SpatialAudioListener == EditorListener)
					{
						AK::SpatialAudio::UnregisterListener(m_SpatialAudioListener->GetAkGameObjectID());
						m_SpatialAudioListener = nullptr;
					}
				}
			}
		);

		FEditorDelegates::OnSwitchBeginPIEAndSIE.AddLambda(
			[&](const bool bIsSimulating)
			{
				if (bIsSimulating)
				{
					AddDefaultListener(EditorListener);
					// The Editor Listener should NEVER be the spatial audio listener
					if (m_SpatialAudioListener == EditorListener)
					{
						AK::SpatialAudio::UnregisterListener(m_SpatialAudioListener->GetAkGameObjectID());
						m_SpatialAudioListener = nullptr;
					}
				}
				else
				{
					RemoveDefaultListener(EditorListener);
				}
			}
		);
	}
#endif
	UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device initialized."));

	return 1;
}

void FAkAudioDevice::CleanupComponentMapsForWorld(UWorld* World)
{
	HighestPriorityLateReverbComponentMap.Remove(World);
	HighestPriorityRoomComponentMap.Remove(World);
}

void FAkAudioDevice::CleanupComponentMapsForLevel(ULevel* Level)
{
	RemovePrioritizedComponentsInLevel<UAkRoomComponent>(HighestPriorityRoomComponentMap, Level);
	RemovePrioritizedComponentsInLevel<UAkLateReverbComponent>(HighestPriorityLateReverbComponentMap, Level);
}

template<class COMPONENT_TYPE>
void FAkAudioDevice::RemovePrioritizedComponentsInLevel(TMap<UWorld*, COMPONENT_TYPE*>& HighestPriorityComponentMap, ULevel* Level)
{
	auto World = Level->GetWorld();
	auto ComponentsToRemove = TArray<COMPONENT_TYPE*>();
	COMPONENT_TYPE** TopComponent = HighestPriorityComponentMap.Find(World);

	if (TopComponent)
	{
		COMPONENT_TYPE* CurrentComponent = *TopComponent;
		while (CurrentComponent)
		{
			auto Owner = CurrentComponent->GetOwner();
			if (Owner != nullptr && Owner->GetLevel() == Level)
				ComponentsToRemove.Add(CurrentComponent);
			CurrentComponent = CurrentComponent->NextLowerPriorityComponent;
		}
	}
	for (auto Component : ComponentsToRemove)
		RemovePrioritizedComponentFromList(Component, HighestPriorityComponentMap);
}



/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 */
bool FAkAudioDevice::Update( float DeltaTime )
{
	if (m_bSoundEngineInitialized)
	{
		// Suspend audio when not in VR focus
		if (FApp::UseVRFocus())
		{
			if (FApp::HasVRFocus())
			{
				WakeupFromSuspend();
			}
			else
			{
				Suspend(true);
			}
		}

		UpdateSetCurrentAudioCultureAsyncTasks();

		AK::SoundEngine::RenderAudio();
	}

	return true;
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void FAkAudioDevice::Teardown()
{
#if AK_SUPPORT_WAAPI
	if (auto waapiClient = FAkWaapiClient::Get())
	{
		FAkAudioDevice_WaapiHelper::Unsubscribe(waapiClient, WaapiSubscriptionIds.Renamed);
		FAkAudioDevice_WaapiHelper::Unsubscribe(waapiClient, WaapiSubscriptionIds.PreDeleted);
		FAkAudioDevice_WaapiHelper::Unsubscribe(waapiClient, WaapiSubscriptionIds.ChildRemoved);
		FAkAudioDevice_WaapiHelper::Unsubscribe(waapiClient, WaapiSubscriptionIds.ChildAdded);
		FAkAudioDevice_WaapiHelper::Unsubscribe(waapiClient, WaapiSubscriptionIds.Created);

		if (ProjectLoadedHandle.IsValid())
		{
			waapiClient->OnProjectLoaded.Remove(ProjectLoadedHandle);
			ProjectLoadedHandle.Reset();
		}

		if (ConnectionLostHandle.IsValid())
		{
			waapiClient->OnConnectionLost.Remove(ConnectionLostHandle);
			ConnectionLostHandle.Reset();
		}

		if (ClientBeginDestroyHandle.IsValid())
		{
			waapiClient->OnClientBeginDestroy.Remove(ClientBeginDestroyHandle);
			ClientBeginDestroyHandle.Reset();
		}

		OnWwiseProjectModification.Clear();
	}
#endif // AK_SUPPORT_WAAPI

	if (m_bSoundEngineInitialized)
	{
		InitBank = nullptr;

		m_EngineExiting = true;
		AK::Monitor::SetLocalOutput(0, NULL);

		AK::SoundEngine::UnregisterGameObj(DUMMY_GAMEOBJ);

		if (AK::SoundEngine::IsInitialized())
		{
			FAkAudioDevice_Helpers::UnregisterAllGlobalCallbacks();
		}

		// Unload all loaded banks before teardown
		if (AkBankManager)
		{
			delete AkBankManager;
			AkBankManager = nullptr;
		}

		FAkSoundEngineInitialization::Finalize();

		if (CallbackManager)
		{
			delete CallbackManager;
			CallbackManager = nullptr;
		}

		if (CallbackInfoPool)
		{
			delete CallbackInfoPool;
			CallbackInfoPool = nullptr;
		}

		if (IOHook)
		{
			delete IOHook;
			IOHook = nullptr;
		}

		// Terminate the streaming manager
		if ( AK::IAkStreamMgr::Get() )
		{
			AK::IAkStreamMgr::Get()->Destroy();
		}

		// Terminate the Memory Manager
		AK::MemoryMgr::Term();
		
		m_bSoundEngineInitialized = false;
	}

	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

#if !WITH_EDITOR
	CachedUnrealToWwiseCulture.Empty();
#endif

	UE_LOG(LogInit, Log, TEXT("Audiokinetic Audio Device terminated."));
}

/**
 * Stops all game sounds (and possibly UI) sounds
 *
 * @param bShouldStopUISounds If true, this function will stop UI sounds as well
 */
void FAkAudioDevice::StopAllSounds(bool bShouldStopUISounds)
{
	AK::SoundEngine::StopAll(DUMMY_GAMEOBJ);
	AK::SoundEngine::StopAll();
}

/**
 * Stop all audio associated with a scene
 *
 * @param SceneToFlush		Interface of the scene to flush
 */
void FAkAudioDevice::Flush(UWorld* WorldToFlush)
{
	AK::SoundEngine::StopAll(DUMMY_GAMEOBJ);
	AK::SoundEngine::StopAll();
}

/**
 * Clears all loaded soundbanks
 *
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::ClearBanks()
{
	if (m_bSoundEngineInitialized)
	{
		AkIntegrationBehavior::Get()->AkAudioDevice_ClearBanks(this);
	}

	return AK_Success;
}

AKRESULT FAkAudioDevice::LoadBank(
	UAkAudioBank* in_Bank,
	AkBankID& out_bankID
)
{
	AKRESULT eResult = LoadBank(in_Bank->GetName(), out_bankID);
	if (eResult == AK_Success && AkBankManager)
	{
		AkBankManager->AddLoadedBank(in_Bank);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::LoadBank(
	class UAkAudioBank* in_Bank,
	AkBankCallbackFunc  in_pfnBankCallback,
	void* in_pCookie,
	AkBankID& out_bankID
)
{
	if (EnsureInitialized() && in_Bank) // ensure audiolib is initialized
	{
		if (AkBankManager)
		{
			IAkBankCallbackInfo* cbInfo = new FAkBankFunctionPtrCallbackInfo(in_pfnBankCallback, in_Bank, in_pCookie);

			// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
			if (cbInfo)
			{
				return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*(in_Bank->GetName())), FAkBankManager::BankLoadCallback, cbInfo, out_bankID);
			}
		}
		else
		{
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*(in_Bank->GetName())), in_pfnBankCallback, in_pCookie, out_bankID);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBank(
	class UAkAudioBank* in_Bank,
	FWaitEndBankAction* LoadBankLatentAction
)
{
	if (EnsureInitialized() && AkBankManager && in_Bank) // ensure audiolib is initialized
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankLatentActionCallbackInfo(in_Bank, LoadBankLatentAction);

		// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
		if (cbInfo)
		{
			AkBankID BankId;
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*(in_Bank->GetName())), FAkBankManager::BankLoadCallback, cbInfo, BankId);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBankAsync(
	class UAkAudioBank* in_Bank,
	const FOnAkBankCallback& BankLoadedCallback,
	AkBankID& out_bankID
)
{
	if (EnsureInitialized() && AkBankManager && in_Bank) // ensure audiolib is initialized
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankBlueprintDelegateCallbackInfo(in_Bank, BankLoadedCallback);

		// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
		if (cbInfo)
		{
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*(in_Bank->GetName())), FAkBankManager::BankLoadCallback, cbInfo, out_bankID);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBank(
	const FString& in_BankName,
	AkBankID& out_bankID
)
{
	AKRESULT eResult = AK_Fail;
	if (EnsureInitialized()) // ensure audiolib is initialized
	{
		eResult = AK::SoundEngine::LoadBank(TCHAR_TO_AK(*in_BankName), out_bankID);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::LoadBank(
	const FString&     in_BankName,
	AkBankCallbackFunc  in_pfnBankCallback,
	void *              in_pCookie,
	AkBankID &          out_bankID
)
{
	if (EnsureInitialized()) // ensure audiolib is initialized
	{
		if (AkBankManager)
		{
			IAkBankCallbackInfo* cbInfo = new FAkBankFunctionPtrCallbackInfo(in_pfnBankCallback, nullptr, in_pCookie);

			// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
			if (cbInfo)
			{
				return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*in_BankName), FAkBankManager::BankLoadCallback, cbInfo, out_bankID);
			}
		}
		else
		{
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*in_BankName), in_pfnBankCallback, in_pCookie, out_bankID);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBank(
	const FString&     in_BankName,
	FWaitEndBankAction* LoadBankLatentAction
)
{
	if (EnsureInitialized() && AkBankManager) // ensure audiolib is initialized
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankLatentActionCallbackInfo(nullptr, LoadBankLatentAction);

		// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
		if (cbInfo)
		{
			AkBankID BankId;
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*in_BankName), FAkBankManager::BankLoadCallback, cbInfo, BankId);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBankAsync(
	const FString&     in_BankName,
	const FOnAkBankCallback& BankLoadedCallback,
	AkBankID &          out_bankID
)
{
	if (EnsureInitialized() && AkBankManager) // ensure audiolib is initialized
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankBlueprintDelegateCallbackInfo(nullptr, BankLoadedCallback);

		// Need to hijack the callback, so we can add the bank to the loaded banks list when successful.
		if (cbInfo)
		{
			return AK::SoundEngine::LoadBank(TCHAR_TO_AK(*in_BankName), FAkBankManager::BankLoadCallback, cbInfo, out_bankID);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::LoadBankFromMemory(
	const void* in_MemoryPtr,
	uint32 in_MemorySize,
	AkBankID& out_bankID
)
{
	if (EnsureInitialized() && in_MemoryPtr && in_MemorySize > 0)
	{
		return AK::SoundEngine::LoadBankMemoryView(in_MemoryPtr, in_MemorySize, out_bankID);
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBank(
	class UAkAudioBank* in_Bank
)
{
	if (!in_Bank)
		return AK_Fail;

	AKRESULT eResult = UnloadBank(in_Bank->LoadedBankName);
	if (eResult == AK_Success && AkBankManager)
	{
		AkBankManager->RemoveLoadedBank(in_Bank);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::UnloadBank(
	class UAkAudioBank* in_Bank,
	AkBankCallbackFunc  in_pfnBankCallback,
	void* in_pCookie
)
{
	if (m_bSoundEngineInitialized && in_Bank)
	{
		if (AkBankManager)
		{
			IAkBankCallbackInfo* cbInfo = new FAkBankFunctionPtrCallbackInfo(in_pfnBankCallback, in_Bank, in_pCookie);

			if (cbInfo)
			{
				return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*(in_Bank->LoadedBankName)), nullptr, FAkBankManager::BankUnloadCallback, cbInfo);
			}
		}
		else
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*(in_Bank->LoadedBankName)), nullptr, in_pfnBankCallback, in_pCookie);
		}
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBank(
	class UAkAudioBank* in_Bank,
	FWaitEndBankAction* UnloadBankLatentAction
)
{
	if (m_bSoundEngineInitialized && AkBankManager && in_Bank)
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankLatentActionCallbackInfo(in_Bank, UnloadBankLatentAction);

		if (cbInfo)
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*(in_Bank->LoadedBankName)), nullptr, FAkBankManager::BankUnloadCallback, cbInfo);
		}
	}
	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBankAsync(
	class UAkAudioBank* in_Bank,
	const FOnAkBankCallback& BankUnloadedCallback
)
{
	if (m_bSoundEngineInitialized && AkBankManager && in_Bank)
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankBlueprintDelegateCallbackInfo(in_Bank, BankUnloadedCallback);

		if (cbInfo)
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*(in_Bank->LoadedBankName)), nullptr, FAkBankManager::BankUnloadCallback, cbInfo);
		}
	}
	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBank(
	const FString&      in_BankName
    )
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized )
	{
		eResult = AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*in_BankName), nullptr );
	}
	return eResult;
}

AKRESULT FAkAudioDevice::UnloadBank(
	const FString&     in_BankName,
	AkBankCallbackFunc  in_pfnBankCallback,
	void *              in_pCookie
)
{
	if (m_bSoundEngineInitialized)
	{
		if (AkBankManager)
		{
			IAkBankCallbackInfo* cbInfo = new FAkBankFunctionPtrCallbackInfo(in_pfnBankCallback, nullptr, in_pCookie);

			if (cbInfo)
			{
				return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*in_BankName), NULL, FAkBankManager::BankUnloadCallback, cbInfo);
			}
		}
		else
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*in_BankName), NULL, in_pfnBankCallback, in_pCookie);
		}
	}
	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBank(
	const FString&     in_BankName,
	FWaitEndBankAction* UnloadBankLatentAction
)
{
	if (m_bSoundEngineInitialized && AkBankManager)
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankLatentActionCallbackInfo(nullptr, UnloadBankLatentAction);

		if (cbInfo)
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*in_BankName), NULL, FAkBankManager::BankUnloadCallback, cbInfo);
		}
	}
	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBankFromMemory(
	AkBankID in_bankID,
	const void* in_memoryPtr
)
{
	if (EnsureInitialized() && in_memoryPtr)
	{
		return AK::SoundEngine::UnloadBank(in_bankID, in_memoryPtr);
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBankFromMemoryAsync(
	AkBankID in_bankID,
	const void* in_memoryPtr,
	AkBankCallbackFunc  in_pfnBankCallback,
	void* in_pCookie
)
{
	if (EnsureInitialized() && in_memoryPtr)
	{
		return AK::SoundEngine::UnloadBank(in_bankID, in_memoryPtr, in_pfnBankCallback, in_pCookie);
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::UnloadBankAsync(
	const FString&     in_BankName,
	const FOnAkBankCallback& BankUnloadedCallback
)
{
	if (m_bSoundEngineInitialized && AkBankManager)
	{
		IAkBankCallbackInfo* cbInfo = new FAkBankBlueprintDelegateCallbackInfo(nullptr, BankUnloadedCallback);

		if (cbInfo)
		{
			return AK::SoundEngine::UnloadBank(TCHAR_TO_AK(*in_BankName), NULL, FAkBankManager::BankUnloadCallback, cbInfo);
		}
	}
	return AK_Fail;
}

/**
 * Load the audiokinetic 'init' bank
 *
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::LoadInitBank()
{
	return AkIntegrationBehavior::Get()->AkAudioDevice_LoadInitBank(this);
}

/**
 * Unload the audiokinetic 'init' bank
 *
 * @return Result from ak sound engine 
 */
void FAkAudioDevice::UnloadInitBank()
{
	if (InitBank)
	{
		InitBank->MarkPendingKill();
	}
	else
	{
		AK::SoundEngine::UnloadBank(TCHAR_TO_AK(AkInitBankName), NULL);
	}
}

bool FAkAudioDevice::LoadAllFilePackages()
{
	return AkIntegrationBehavior::Get()->AkAudioDevice_LoadAllFilePackages(this);
}

bool FAkAudioDevice::UnloadAllFilePackages()
{
	return AkIntegrationBehavior::Get()->AkAudioDevice_UnloadAllFilePackages(this);
}

/**
 * Load all banks currently being referenced
 */
void FAkAudioDevice::LoadAllReferencedBanks()
{
	AkIntegrationBehavior::Get()->AkAudioDevice_LoadAllReferencedBanks(this);
}

#if WITH_EDITOR
/**
 * Reload all banks currently being referenced
 */
void FAkAudioDevice::ReloadAllReferencedBanks()
{
	if (m_bSoundEngineInitialized)
	{
		StopAllSounds();
		AK::SoundEngine::RenderAudio();
		FPlatformProcess::Sleep(0.1f);
		ClearBanks();
		UnloadAllFilePackages();
		LoadAllReferencedBanks();
	}
}

void FAkAudioDevice::UnloadAllSoundData()
{
	if (m_bSoundEngineInitialized)
	{
		AkIntegrationBehavior::Get()->AkAudioDevice_UnloadAllSoundData(this);
	}
}

void FAkAudioDevice::ReloadAllSoundData()
{
	if (m_bSoundEngineInitialized)
	{
		AkIntegrationBehavior::Get()->AkAudioDevice_ReloadAllSoundData(this);
	}
}
#endif

AkUInt32 FAkAudioDevice::GetIDFromString(const FString& in_string)
{
	if (in_string.IsEmpty())
	{
		return AK_INVALID_UNIQUE_ID;
	}
	else
	{
		return AK::SoundEngine::GetIDFromString(TCHAR_TO_ANSI(*in_string));
	}
}

AKRESULT FAkAudioDevice::SetMedia(AkSourceSettings* in_pSourceSettings, uint32 in_uNumSourceSettings)
{
	return AK::SoundEngine::SetMedia(in_pSourceSettings, in_uNumSourceSettings);
}

AKRESULT FAkAudioDevice::UnsetMedia(AkSourceSettings* in_pSourceSettings, uint32 in_uNumSourceSettings)
{
	return AK::SoundEngine::UnsetMedia(in_pSourceSettings, in_uNumSourceSettings);
}

FString FAkAudioDevice::GetCurrentAudioCulture() const
{
	return FString(AK::StreamMgr::GetCurrentLanguage());
}

FString FAkAudioDevice::GetDefaultLanguage() const
{
	if (InitBank && InitBank->DefaultLanguage.Len() > 0)
	{
		return InitBank->DefaultLanguage;
	}

	return TEXT("English(US)");
}

TArray<FString> FAkAudioDevice::GetAvailableAudioCultures() const
{
	if (InitBank)
	{
		return InitBank->AvailableAudioCultures;
	}

	return TArray<FString>();
}

void FAkAudioDevice::SetCurrentAudioCulture(const FString& NewAudioCulture)
{
	FString newWwiseLanguage;

	if (FindWwiseLanguage(NewAudioCulture, newWwiseLanguage))
	{
		AkIntegrationBehavior::Get()->AkAudioDevice_SetCurrentAudioCulture(newWwiseLanguage);
	}
}

void FAkAudioDevice::SetCurrentAudioCultureAsync(const FString& NewAudioCulture, const FOnSetCurrentAudioCultureCompleted& CompletedCallback)
{
	FString newWwiseLanguage;

	if (FindWwiseLanguage(NewAudioCulture, newWwiseLanguage))
	{
		AkIntegrationBehavior::Get()->AkAudioDevice_SetCurrentAudioCultureAsync(this, newWwiseLanguage, CompletedCallback);
	}
	else
	{
		CompletedCallback.ExecuteIfBound(true);
	}
}

bool FAkAudioDevice::FindWwiseLanguage(const FString& NewAudioCulture, FString& FoundWwiseLanguage)
{
	auto oldAudioCulture = GetCurrentAudioCulture();

	FoundWwiseLanguage = NewAudioCulture;

	auto newCulturePtr = FInternationalization::Get().GetCulture(FoundWwiseLanguage);
	if (newCulturePtr && newCulturePtr->GetLCID() != InvariantLCID)
	{
		auto& newLanguage = newCulturePtr->GetTwoLetterISOLanguageName();
		auto& newRegion = newCulturePtr->GetRegion();
		auto& newScript = newCulturePtr->GetScript();

		int maxMatch = 0;

		auto findMostCompatibleCulture = [&maxMatch, &newLanguage, &newRegion, &newScript, &FoundWwiseLanguage](const FCulturePtr& currentCulture, const FString& currentValue) {
			auto& currentLanguageName = currentCulture->GetTwoLetterISOLanguageName();
			auto& currentRegionName = currentCulture->GetRegion();
			auto& currentScript = currentCulture->GetScript();

			int currentMatch = 0;

			if (currentLanguageName == newLanguage)
			{
				currentMatch = 1;

				// This is inspired by how UE processes culture from most to least specific. For example:
				// zh - Hans - CN is processed as "zh-Hans-CN", then "zh-CN", then "zh-Hans", then "zh".
				// This is how I selected the weight for each match.

				if (!currentScript.IsEmpty() && !newScript.IsEmpty()
					&& !currentRegionName.IsEmpty() && !newRegion.IsEmpty())
				{
					if (currentScript == newScript && currentRegionName == newRegion)
					{
						currentMatch += 4;
					}
					else
					{
						--currentMatch;
					}
				}

				if (!currentRegionName.IsEmpty() && !newRegion.IsEmpty())
				{
					if (currentRegionName == newRegion)
					{
						currentMatch += 3;
					}
					else
					{
						--currentMatch;
					}
				}

				if (!currentScript.IsEmpty() && !newScript.IsEmpty())
				{
					if (currentScript == newScript)
					{
						currentMatch += 2;
					}
					else
					{
						--currentMatch;
					}
				}
			}

			if (currentMatch > 0)
			{
				// When the current culture is missing script or region but the new culture
				// has it, give a weight boost of 1

				if (!newScript.IsEmpty() && currentScript.IsEmpty())
				{
					++currentMatch;
				}

				if (!newRegion.IsEmpty() && currentRegionName.IsEmpty())
				{
					++currentMatch;
				}
			}

			if (maxMatch < currentMatch)
			{
				FoundWwiseLanguage = currentValue;
				maxMatch = currentMatch;
			}
		};

#if WITH_EDITOR
		if (auto* akSettings = GetDefault<UAkSettings>())
		{
			for (auto& entry : akSettings->UnrealCultureToWwiseCulture)
			{
				auto currentCulture = FInternationalization::Get().GetCulture(entry.Key);
				if (currentCulture && currentCulture->GetLCID() != InvariantLCID)
				{
					findMostCompatibleCulture(currentCulture, entry.Value);
				}
			}
		}
#else
		for (auto& entry : CachedUnrealToWwiseCulture)
		{
			findMostCompatibleCulture(entry.Key, entry.Value);
		}
#endif
	}

	return oldAudioCulture != FoundWwiseLanguage;
}

void FAkAudioDevice::UpdateSetCurrentAudioCultureAsyncTasks()
{
	for (auto task : audioCultureAsyncTasks)
	{
		task->Update();
	}

	for (int32 i = audioCultureAsyncTasks.Num() - 1; i >= 0; --i)
	{
		if (audioCultureAsyncTasks[i]->IsDone)
		{
			delete audioCultureAsyncTasks[i];
			audioCultureAsyncTasks.RemoveAt(i);
		}
	}
}

template<typename FCreateCallbackPackage>
AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName,
	const AkGameObjectID in_gameObjectID,
	const TArray<AkExternalSourceInfo>& in_ExternalSources,
	FCreateCallbackPackage CreateCallbackPackage
)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if (m_bSoundEngineInitialized && CallbackManager)
	{
		auto pPackage = CreateCallbackPackage(in_gameObjectID);
		if (pPackage)
		{
			playingID = AK::SoundEngine::PostEvent(
				  TCHAR_TO_AK(*in_EventName)
				, in_gameObjectID
				, pPackage->uUserFlags | AK_EndOfEvent
				, &FAkComponentCallbackManager::AkComponentCallback
				, pPackage
				, in_ExternalSources.Num()
				, const_cast<AkExternalSourceInfo*>(in_ExternalSources.GetData())
			);
			if (playingID == AK_INVALID_PLAYING_ID)
			{
				CallbackManager->RemoveCallbackPackage(pPackage, in_gameObjectID);
			}
			else
			{
				FScopeLock Lock(&EventToPlayingIDMapCriticalSection);
				auto& PlayingIDArray = EventToPlayingIDMap.FindOrAdd(GetIDFromString(in_EventName));
				PlayingIDArray.Add(playingID);
			}
		}
	}

	return playingID;
}

template<typename FCreateCallbackPackage>
AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName,
	UAkGameObject* in_pGameObject,
	const TArray<AkExternalSourceInfo>& in_ExternalSources,
	FCreateCallbackPackage CreateCallbackPackage
)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if (m_bSoundEngineInitialized && in_pGameObject && CallbackManager)
	{
		if (in_pGameObject->VerifyEventName(in_EventName) && in_pGameObject->AllowAudioPlayback())
		{
			in_pGameObject->UpdateOcclusionObstruction();

			auto gameObjID = in_pGameObject->GetAkGameObjectID();

			return PostEvent(in_EventName, gameObjID, in_ExternalSources, CreateCallbackPackage);
		}
	}

	return playingID;
}

/**
 * Post an event to ak soundengine
 *
 * @param in_pEvent			Event to post
 * @param in_pComponent		AkComponent on which to play the event
 * @param in_uFlags			Bitmask: see \ref AkCallbackType
 * @param in_pfnCallback	Callback function
 * @param in_pCookie		Callback cookie that will be sent to the callback function along with additional information.
 * @param in_bStopWhenOwnerDestroyed If true, then the sound should be stopped if the owning actor is destroyed
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEvent(
	UAkAudioEvent * in_pEvent, 
	AActor * in_pActor,
	AkUInt32 in_uFlags /*= 0*/,
	AkCallbackFunc in_pfnCallback /*= NULL*/,
	void * in_pCookie /*= NULL*/,
	bool in_bStopWhenOwnerDestroyed, /*= false*/
	const TArray<AkExternalSourceInfo>& in_ExternalSources /* = TArray<AkExternalSourceInfo>()*/
    )
{
	if (!in_pEvent)
		return AK_INVALID_PLAYING_ID;

	if (!in_pEvent->IsMediaReady())
	{
		UE_LOG(LogAkAudio, Warning, TEXT("Not all media for '%s' are loaded. Consider using PostEventAsync()."), *in_pEvent->GetName());
	}

	return PostEvent(in_pEvent->GetName(), in_pActor, in_uFlags, in_pfnCallback, in_pCookie, in_bStopWhenOwnerDestroyed, in_ExternalSources);
}

/**
 * Post an event to ak soundengine by name
 *
 * @param in_EventName		Name of the event to post
 * @param in_pComponent		AkComponent on which to play the event
 * @param in_uFlags			Bitmask: see \ref AkCallbackType
 * @param in_pfnCallback	Callback function
 * @param in_pCookie		Callback cookie that will be sent to the callback function along with additional information.
 * @param in_bStopWhenOwnerDestroyed If true, then the sound should be stopped if the owning actor is destroyed
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName, 
	AActor * in_pActor,
	AkUInt32 in_uFlags /*= 0*/,
	AkCallbackFunc in_pfnCallback /*= NULL*/,
	void * in_pCookie /*= NULL*/,
	bool in_bStopWhenOwnerDestroyed, /*= false*/
	const TArray<AkExternalSourceInfo>& in_ExternalSources /* = TArray<AkExternalSourceInfo>()*/
    )
{
	if (m_bSoundEngineInitialized)
	{
		if (!in_pActor)
		{
			return PostEvent(in_EventName, DUMMY_GAMEOBJ, in_ExternalSources, [in_pfnCallback, in_pCookie, in_uFlags, this](AkGameObjectID gameObjID) {
				return CallbackManager->CreateCallbackPackage(in_pfnCallback, in_pCookie, in_uFlags, gameObjID);
			});
		}
		else if (!in_pActor->IsActorBeingDestroyed() && !in_pActor->IsPendingKill())
		{
			UAkComponent* pComponent = GetAkComponent(in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
			if (pComponent)
			{
				pComponent->StopWhenOwnerDestroyed = in_bStopWhenOwnerDestroyed;
				return PostEvent(in_EventName, pComponent, in_uFlags, in_pfnCallback, in_pCookie, in_ExternalSources);
			}
		}
	}

	return AK_INVALID_PLAYING_ID;
}

AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName,
	AActor * in_pActor,
	const FOnAkPostEventCallback& PostEventCallback,
	AkUInt32 in_uFlags /*= 0*/,
	bool in_bStopWhenOwnerDestroyed, /*= false*/
	const TArray<AkExternalSourceInfo>& in_ExternalSources /* = TArray<AkExternalSourceInfo>()*/
)
{
	if (m_bSoundEngineInitialized)
	{
		if (!in_pActor)
		{
			UE_LOG(LogAkAudio, Error, TEXT("PostEvent accepting a FOnAkPostEventCallback delegate requires a valid actor"));
		}
		else if (!in_pActor->IsActorBeingDestroyed() && !in_pActor->IsPendingKill())
		{
			UAkComponent* pComponent = GetAkComponent(in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
			if (pComponent)
			{
				pComponent->StopWhenOwnerDestroyed = in_bStopWhenOwnerDestroyed;
				return PostEvent(in_EventName, pComponent, PostEventCallback, in_uFlags, in_ExternalSources);
			}
		}
	}

	return AK_INVALID_PLAYING_ID;
}

AkPlayingID FAkAudioDevice::PostEventLatentAction(
	const FString& EventName,
	AActor * Actor,
	bool bStopWhenOwnerDestroyed,
	FWaitEndOfEventAction* LatentAction,
	const TArray<AkExternalSourceInfo>& in_ExternalSources /* = TArray<AkExternalSourceInfo>()*/
)
{
	if (m_bSoundEngineInitialized)
	{
		if (!Actor)
		{
			UE_LOG(LogAkAudio, Error, TEXT("PostEvent accepting a FWaitEndOfEventAction requires a valid actor"));
		}
		else if (!Actor->IsActorBeingDestroyed() && !Actor->IsPendingKill())
		{
			UAkComponent* pComponent = GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
			if (pComponent)
			{
				pComponent->StopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
				return PostEventLatentAction(EventName, pComponent, LatentAction, in_ExternalSources);
			}
		}
	}

	return AK_INVALID_PLAYING_ID;
}

/**
 * Post an event to ak soundengine by name
 *
 * @param in_EventName		Name of the event to post
 * @param in_pGameObject	UAkGameObject on which to play the event
 * @param in_uFlags			Bitmask: see \ref AkCallbackType
 * @param in_pfnCallback	Callback function
 * @param in_pCookie		Callback cookie that will be sent to the callback function along with additional information.
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName,
	UAkComponent* in_pComponent,
	AkUInt32 in_uFlags /*= 0*/,
	AkCallbackFunc in_pfnCallback /*= NULL*/,
	void * in_pCookie, /*= NULL*/
	const TArray<AkExternalSourceInfo>& in_ExternalSources /*= TArray<AkExternalSourceInfo>()*/
	)
{

	return PostEvent(in_EventName, in_pComponent, in_ExternalSources, [in_pfnCallback, in_pCookie, in_uFlags, this](AkGameObjectID gameObjID) {
		return CallbackManager->CreateCallbackPackage(in_pfnCallback, in_pCookie, in_uFlags, gameObjID);
	});
}

AkPlayingID FAkAudioDevice::PostEvent(
	const FString& in_EventName,
	UAkGameObject* in_pGameObject,
	const FOnAkPostEventCallback& PostEventCallback,
	AkUInt32 in_uFlags, /*= 0*/
	const TArray<AkExternalSourceInfo>& in_ExternalSources /*= TArray<AkExternalSourceInfo>()*/
)
{
	return PostEvent(in_EventName, in_pGameObject, in_ExternalSources, [PostEventCallback, in_uFlags, this](AkGameObjectID gameObjID) {
		return CallbackManager->CreateCallbackPackage(PostEventCallback, in_uFlags, gameObjID);
	});
}

AkPlayingID FAkAudioDevice::PostEventLatentAction(
	const FString& in_EventName,
	UAkComponent* in_pComponent,
	FWaitEndOfEventAction* LatentAction,
	const TArray<AkExternalSourceInfo>& in_ExternalSources /*= TArray<AkExternalSourceInfo>()*/
	)
{
	return PostEvent(in_EventName, in_pComponent, in_ExternalSources, [LatentAction, this](AkGameObjectID gameObjID) {
		return CallbackManager->CreateCallbackPackage(LatentAction, gameObjID);
	});
}

TFuture<AkPlayingID> FAkAudioDevice::PostEventAsync(class UAkAudioEvent* in_pEvent,
	AActor* in_pActor,
	const FOnAkPostEventCallback& PostEventCallback,
	AkUInt32 in_uFlags,
	bool in_bStopWhenOwnerDestroyed,
	const TArray<AkExternalSourceInfo>& in_ExternalSources
)
{
	TPromise<AkPlayingID> playingIDPromise;
	auto playingIDFuture = playingIDPromise.GetFuture();

	auto pollMediaReadyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([in_pEvent]()
		{
			while (in_pEvent && !in_pEvent->IsMediaReady())
			{
				FPlatformProcess::Sleep(1.f / 60.f);
			}
		}, GET_STATID(STAT_PostEventAsync), nullptr, ENamedThreads::AnyThread);

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, in_pEvent, in_pActor, PostEventCallback, in_uFlags, in_bStopWhenOwnerDestroyed, in_ExternalSources, playingIDPromiseCopy(MoveTemp(playingIDPromise))]() mutable {
		AkPlayingID playingID = AK_INVALID_PLAYING_ID;

		if (m_bSoundEngineInitialized && in_pEvent)
		{
			if (!in_pActor)
			{
				UE_LOG(LogAkAudio, Error, TEXT("PostEvent accepting a FOnAkPostEventCallback delegate requires a valid actor"));
			}
			else if (!in_pActor->IsActorBeingDestroyed() && !in_pActor->IsPendingKill())
			{
				UAkComponent* pComponent = GetAkComponent(in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
				if (pComponent)
				{
					pComponent->StopWhenOwnerDestroyed = in_bStopWhenOwnerDestroyed;
					playingID = PostEvent(in_pEvent->GetName(), pComponent, PostEventCallback, in_uFlags, in_ExternalSources);
				}
			}
		}

		playingIDPromiseCopy.SetValue(playingID);
	}, GET_STATID(STAT_PostEventAsync), pollMediaReadyTask, ENamedThreads::GameThread);

	return playingIDFuture;
}

TFuture<AkPlayingID> FAkAudioDevice::PostEventAsync(
	UAkAudioEvent* in_pEvent,
	UAkGameObject* in_pGameObject,
	const FOnAkPostEventCallback& PostEventCallback,
	AkUInt32 in_uFlags,
	const TArray<AkExternalSourceInfo>& in_ExternalSources
)
{
	auto playingIDFuture = Async(EAsyncExecution::TaskGraph, [in_pEvent] {
		while (in_pEvent && !in_pEvent->IsMediaReady())
		{
			FPlatformProcess::Sleep(1.f / 60.f);
		}
	}).Then([this, in_pEvent, in_pGameObject, PostEventCallback, in_uFlags, in_ExternalSources](auto previousFuture) {
		if (!in_pEvent)
		{
			return AK_INVALID_PLAYING_ID;
		}
		return PostEvent(in_pEvent->GetName(), in_pGameObject, in_ExternalSources, [PostEventCallback, in_uFlags, this](AkGameObjectID gameObjID) {
			return CallbackManager->CreateCallbackPackage(PostEventCallback, in_uFlags, gameObjID);
		});
	});

	return playingIDFuture;
}

TFuture<AkPlayingID> FAkAudioDevice::PostEventAtLocationAsync(
	class UAkAudioEvent* in_pEvent,
	FVector in_Location,
	FRotator in_Orientation,
	class UWorld* in_World
)
{
	TPromise<AkPlayingID> playingIDPromise;
	auto playingIDFuture = playingIDPromise.GetFuture();

	auto pollMediaReadyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([in_pEvent]()
		{
			while (in_pEvent && !in_pEvent->IsMediaReady())
			{
				FPlatformProcess::Sleep(1.f / 60.f);
			}
		}, GET_STATID(STAT_PostEventAsync), nullptr, ENamedThreads::AnyThread);

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, in_pEvent, in_Location, in_Orientation, in_World, playingIDPromiseCopy(MoveTemp(playingIDPromise))]() mutable {
		AkPlayingID playingID = AK_INVALID_PLAYING_ID;

		if (in_pEvent)
		{
			playingID = PostEventAtLocation(in_pEvent->GetName(), in_Location, in_Orientation, in_World);
		}

		playingIDPromiseCopy.SetValue(playingID);
	}, GET_STATID(STAT_PostEventAsync), pollMediaReadyTask, ENamedThreads::GameThread);

	return playingIDFuture;
}

TFuture<AkPlayingID> FAkAudioDevice::PostEventLatentActionAsync(
	UAkAudioEvent* in_pEvent,
	AActor* Actor,
	bool bStopWhenOwnerDestroyed,
	FWaitEndOfEventAction* LatentAction,
	const TArray<AkExternalSourceInfo>& in_ExternalSources
)
{
	TPromise<AkPlayingID> playingIDPromise;
	auto playingIDFuture = playingIDPromise.GetFuture();

	auto pollMediaReadyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([in_pEvent]()
		{
			while (in_pEvent && !in_pEvent->IsMediaReady())
			{
				FPlatformProcess::Sleep(1.f / 60.f);
			}
		}, GET_STATID(STAT_PostEventAsync), nullptr, ENamedThreads::AnyThread);

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, in_pEvent, Actor, bStopWhenOwnerDestroyed, LatentAction, in_ExternalSources, playingIDPromiseCopy(MoveTemp(playingIDPromise))]() mutable {
		AkPlayingID playingID = AK_INVALID_PLAYING_ID;

		if (m_bSoundEngineInitialized && in_pEvent)
		{
			if (!Actor)
			{
				UE_LOG(LogAkAudio, Error, TEXT("PostEvent accepting a FWaitEndOfEventAction requires a valid actor"));
			}
			else if (!Actor->IsActorBeingDestroyed() && !Actor->IsPendingKill())
			{
				UAkComponent* pComponent = GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
				if (pComponent)
				{
					pComponent->StopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
					playingID = PostEvent(in_pEvent->GetName(), pComponent, in_ExternalSources, [LatentAction, this](AkGameObjectID gameObjID) {
						return CallbackManager->CreateCallbackPackage(LatentAction, gameObjID);
					});
				}
			}
		}

		playingIDPromiseCopy.SetValue(playingID);
	}, GET_STATID(STAT_PostEventAsync), pollMediaReadyTask, ENamedThreads::GameThread);

	return playingIDFuture;
}

TFuture<AkPlayingID> FAkAudioDevice::PostEventLatentActionAsync(
	UAkAudioEvent* in_pEvent,
	UAkComponent* in_pComponent,
	FWaitEndOfEventAction* LatentAction,
	const TArray<AkExternalSourceInfo>& in_ExternalSources
)
{
	auto playingIDFuture = Async(EAsyncExecution::TaskGraph, [in_pEvent] {
		while (in_pEvent && !in_pEvent->IsMediaReady())
		{
			FPlatformProcess::Sleep(1.f / 60.f);
		}
	}).Then([this, in_pEvent, in_pComponent, LatentAction, in_ExternalSources](auto previousFuture) {
		if (m_bSoundEngineInitialized && in_pEvent)
		{
			return PostEvent(in_pEvent->GetName(), in_pComponent, in_ExternalSources, [LatentAction, this](AkGameObjectID gameObjID) {
				return CallbackManager->CreateCallbackPackage(LatentAction, gameObjID);
			});
		}

		return AK_INVALID_PLAYING_ID;
	});

	return playingIDFuture;
}

/** Find UAkLateReverbComponents at a given location. */
TArray<class UAkLateReverbComponent*> FAkAudioDevice::FindLateReverbComponentsAtLocation(const FVector& Loc, const UWorld* in_World, int32 depth)
{
	return FindPrioritizedComponentsAtLocation(Loc, in_World, HighestPriorityLateReverbComponentMap, depth);
}

/** Add a UAkLateReverbComponent to the linked list. */
void FAkAudioDevice::AddLateReverbComponentToPrioritizedList(class UAkLateReverbComponent* in_ComponentToAdd)
{
	AddPrioritizedComponentInList(in_ComponentToAdd, HighestPriorityLateReverbComponentMap);
}

/** Remove a UAkLateReverbComponent from the linked list. */
void FAkAudioDevice::RemoveLateReverbComponentFromPrioritizedList(class UAkLateReverbComponent* in_ComponentToRemove)
{
	RemovePrioritizedComponentFromList(in_ComponentToRemove, HighestPriorityLateReverbComponentMap);
}

bool FAkAudioDevice::WorldHasActiveRooms(UWorld* in_World)
{
	UAkRoomComponent** TopComponent = HighestPriorityRoomComponentMap.Find(in_World);

	return TopComponent && *TopComponent;
}

/** Find UAkRoomComponents at a given location. */
TArray<class UAkRoomComponent*> FAkAudioDevice::FindRoomComponentsAtLocation(const FVector& Loc, const UWorld* in_World, int32 depth)
{
	return FindPrioritizedComponentsAtLocation(Loc, in_World, HighestPriorityRoomComponentMap, depth);
}

/** Add a UAkRoomComponent to the linked list. */
void FAkAudioDevice::AddRoomComponentToPrioritizedList(class UAkRoomComponent* in_ComponentToAdd)
{
	AddPrioritizedComponentInList(in_ComponentToAdd, HighestPriorityRoomComponentMap);
}

/** Remove a UAkRoomComponent from the linked list. */
void FAkAudioDevice::RemoveRoomComponentFromPrioritizedList(class UAkRoomComponent* in_ComponentToRemove)
{
	RemovePrioritizedComponentFromList(in_ComponentToRemove, HighestPriorityRoomComponentMap);
}

/** Return true if any UAkRoomComponents have been added to the prioritized list of rooms **/
bool FAkAudioDevice::UsingSpatialAudioRooms(const UWorld* in_World)
{
	return HighestPriorityRoomComponentMap.Find(in_World) != NULL;
}

AKRESULT FAkAudioDevice::ExecuteActionOnEvent(
	const FString& in_EventName,
	AkActionOnEventType in_ActionType,
	AActor* in_pActor,
	AkTimeMs in_uTransitionDuration,
	EAkCurveInterpolation in_eFadeCurve,
	AkPlayingID in_PlayingID 
)
{
	if (!in_pActor)
	{
		return AK::SoundEngine::ExecuteActionOnEvent(TCHAR_TO_AK(*in_EventName),
			static_cast<AK::SoundEngine::AkActionOnEventType>(in_ActionType),
			DUMMY_GAMEOBJ,
			in_uTransitionDuration,
			static_cast<AkCurveInterpolation>(in_eFadeCurve),
			in_PlayingID
		);
	}
	else if (!in_pActor->IsActorBeingDestroyed() && !in_pActor->IsPendingKill())
	{
		UAkComponent* pComponent = GetAkComponent(in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
		if (pComponent)
		{
			return AK::SoundEngine::ExecuteActionOnEvent(TCHAR_TO_AK(*in_EventName),
				static_cast<AK::SoundEngine::AkActionOnEventType>(in_ActionType),
				pComponent->GetAkGameObjectID(),
				in_uTransitionDuration,
				static_cast<AkCurveInterpolation>(in_eFadeCurve),
				in_PlayingID
			);
		}
	}

	return AKRESULT::AK_Fail;
}

void FAkAudioDevice::ExecuteActionOnPlayingID(
	AkActionOnEventType in_ActionType,
	AkPlayingID in_PlayingID,
	AkTimeMs in_uTransitionDuration,
	EAkCurveInterpolation in_eFadeCuve
)
{
	AK::SoundEngine::ExecuteActionOnPlayingID(
		static_cast<AK::SoundEngine::AkActionOnEventType>(in_ActionType),
		in_PlayingID,
		in_uTransitionDuration,
		static_cast<AkCurveInterpolation>(in_eFadeCuve)
	);
}

/** Seek on an event in the ak soundengine.
* @param in_EventName            Name of the event on which to seek.
* @param in_pActor               The associated Actor. If this is nullptr, defaul object will be used.
* @param in_fPercent             Desired percent where playback should restart.
* @param in_bSeekToNearestMarker If true, the final seeking position will be made equal to the nearest marker.
*
* @return Success or failure.
*/
AKRESULT FAkAudioDevice::SeekOnEvent(
    const FString& in_EventName,
    AActor* in_pActor,
    AkReal32 in_fPercent,
    bool in_bSeekToNearestMarker /*= false*/,
    AkPlayingID InPlayingID      /*= AK_INVALID_PLAYING_ID*/
)
{
    if (!in_pActor)
    {
        // SeekOnEvent must be bound to a game object. Passing DUMMY_GAMEOBJ as default game object.
        return AK::SoundEngine::SeekOnEvent(TCHAR_TO_AK(*in_EventName), DUMMY_GAMEOBJ, in_fPercent, in_bSeekToNearestMarker, InPlayingID);
    }
    else if (!in_pActor->IsActorBeingDestroyed() && !in_pActor->IsPendingKill())
    {
        UAkComponent* pComponent = GetAkComponent(in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
        if (pComponent)
        {
            return SeekOnEvent(in_EventName, pComponent, in_fPercent, in_bSeekToNearestMarker, InPlayingID);
        }
    }

    return AKRESULT::AK_Fail;
}

/** Seek on an event in the ak soundengine.
* @param in_EventName            Name of the event on which to seek.
* @param in_pComponent           The associated AkComponent.
* @param in_fPercent             Desired percent where playback should restart.
* @param in_bSeekToNearestMarker If true, the final seeking position will be made equal to the nearest marker.
*
* @return Success or failure.
*/
AKRESULT FAkAudioDevice::SeekOnEvent(
    const FString& in_EventName,
    UAkComponent* in_pComponent,
    AkReal32 in_fPercent,
    bool in_bSeekToNearestMarker /*= false*/,
    AkPlayingID InPlayingID      /*= AK_INVALID_PLAYING_ID*/
    )
{
    if (m_bSoundEngineInitialized && in_pComponent)
    {
        if (in_pComponent->VerifyEventName(in_EventName) && in_pComponent->AllowAudioPlayback())
        {
            return AK::SoundEngine::SeekOnEvent(TCHAR_TO_AK(*in_EventName), in_pComponent->GetAkGameObjectID(), in_fPercent, in_bSeekToNearestMarker, InPlayingID);
        }
    }
    return AKRESULT::AK_Fail;
}


/** Find Components that are prioritized (either UAkLateReverbComponent or UAkRoomComponent) at a given location

 *
 * @param							Loc	Location at which to find Reverb Volumes
 * @param FoundComponents		Array containing all found components at this location
 */
template<class COMPONENT_TYPE>
TArray<COMPONENT_TYPE*> FAkAudioDevice::FindPrioritizedComponentsAtLocation(const FVector& Loc, const UWorld* in_World, TMap<UWorld*, COMPONENT_TYPE*>& HighestPriorityComponentMap, int32 depth)
{
	TArray<COMPONENT_TYPE*> FoundComponents;

	COMPONENT_TYPE** TopComponent = HighestPriorityComponentMap.Find(in_World);
	if(TopComponent)
	{
		COMPONENT_TYPE* CurrentComponent = *TopComponent;

		while(CurrentComponent)
		{
  			if(CurrentComponent->HasEffectOnLocation(Loc) && CurrentComponent->bEnable )
			{
				FoundComponents.Add(CurrentComponent);
				if (depth != FIND_COMPONENTS_DEPTH_INFINITE && FoundComponents.Num() == depth)
					break;
				}

			CurrentComponent = CurrentComponent->NextLowerPriorityComponent;
		}
	}

	return FoundComponents;
}

/** Add a Component that is prioritized (either UAkLateReverbComponent or UAkRoomComponent) in the active linked list. */
template<class COMPONENT_TYPE>
void FAkAudioDevice::AddPrioritizedComponentInList(COMPONENT_TYPE* in_ComponentToAdd, TMap<UWorld*, COMPONENT_TYPE*>& HighestPriorityComponentMap)
{
	UWorld* CurrentWorld = in_ComponentToAdd->GetWorld();
	COMPONENT_TYPE*& HighestPriorityComponent = HighestPriorityComponentMap.FindOrAdd(CurrentWorld);

	if(HighestPriorityComponent == NULL )
	{
		// First volume in the list. Set head.
		HighestPriorityComponent = in_ComponentToAdd;
		in_ComponentToAdd->NextLowerPriorityComponent = NULL;
	}
	else
	{
		COMPONENT_TYPE* CurrentComponent = HighestPriorityComponent;
		COMPONENT_TYPE* PreviousComponent = NULL;

		while(CurrentComponent && CurrentComponent != in_ComponentToAdd) // Don't add twice to the list!
		{
			if(in_ComponentToAdd->Priority > CurrentComponent->Priority )
			{
				// Found our spot in the list!
				if (PreviousComponent)
				{
					PreviousComponent->NextLowerPriorityComponent = in_ComponentToAdd;
				}
				else
				{
					// No previous, so we are at the top.
					HighestPriorityComponent = in_ComponentToAdd;
				}

				in_ComponentToAdd->NextLowerPriorityComponent = CurrentComponent;
				return;
			}

			// List traversal.
			PreviousComponent = CurrentComponent;
			CurrentComponent = CurrentComponent->NextLowerPriorityComponent;
		}

		// We're at the end!
		if(!CurrentComponent)
		{
			// Just to make sure...
			if(PreviousComponent)
			{
				PreviousComponent->NextLowerPriorityComponent = in_ComponentToAdd;
				in_ComponentToAdd->NextLowerPriorityComponent = NULL;
			}
		}
	}
}

/** Remove a Component that is prioritized (either UAkLateReverbComponent or UAkRoomComponent) from the linked list. */
template<class COMPONENT_TYPE>
void FAkAudioDevice::RemovePrioritizedComponentFromList(COMPONENT_TYPE* in_ComponentToRemove, TMap<UWorld*, COMPONENT_TYPE*>& HighestPriorityComponentMap)
{
	UWorld* CurrentWorld = in_ComponentToRemove->GetWorld();
	COMPONENT_TYPE** HighestPriorityComponent = HighestPriorityComponentMap.Find(CurrentWorld);

	if(HighestPriorityComponent)
	{
		COMPONENT_TYPE* CurrentComponent = *HighestPriorityComponent;
		COMPONENT_TYPE* PreviousComponent = NULL;
		while(CurrentComponent)
		{
			if(CurrentComponent == in_ComponentToRemove)
			{
				// Found our volume, remove it from the list
				if(PreviousComponent)
				{
					PreviousComponent->NextLowerPriorityComponent = CurrentComponent->NextLowerPriorityComponent;
				}
				else
				{
					// The one to remove was the highest, reset the head.
					*HighestPriorityComponent = CurrentComponent->NextLowerPriorityComponent;
				}
				
				break;
			}

			PreviousComponent = CurrentComponent;
			CurrentComponent = CurrentComponent->NextLowerPriorityComponent;
		}

		// Don't leave dangling pointers.
		in_ComponentToRemove->NextLowerPriorityComponent = NULL;

		if( *HighestPriorityComponent == NULL )
		{
			HighestPriorityComponentMap.Remove(CurrentWorld);
		}
	}
}

void FAkAudioDevice::UpdateAllSpatialAudioRooms(UWorld* InWorld)
{
	UAkRoomComponent** ppRoom = HighestPriorityRoomComponentMap.Find(InWorld);
#ifdef AK_ENABLE_PORTALS
	if (ppRoom)
	{
		for (UAkRoomComponent* pRoom = *ppRoom; pRoom != nullptr; pRoom = pRoom->NextLowerPriorityComponent)
		{
			pRoom->UpdateSpatialAudioRoom();
		}
	}
#endif
}

void FAkAudioDevice::UpdateAllSpatialAudioPortals(UWorld* InWorld)
{
#ifdef AK_ENABLE_PORTALS
	for (TActorIterator<AAkAcousticPortal> ActorItr(InWorld); ActorItr; ++ActorItr)
	{
		SetSpatialAudioPortal(*ActorItr);
	}
#endif
}

void FAkAudioDevice::SetSpatialAudioPortal(const AAkAcousticPortal* in_Portal)
{
	if(IsRunningCommandlet())
		return;

#ifdef AK_ENABLE_PORTALS
	AkPortalID portalID = in_Portal->GetPortalID();

	FString nameStr = in_Portal->GetName();

	FVector location = in_Portal->GetActorLocation();

	FRotator rotation = in_Portal->GetActorRotation();
	FVector front = rotation.RotateVector(FVector(0.f, 1.f, 0.f));
	FVector up = rotation.RotateVector(FVector(0.f, 0.f, 1.f));

	
	AkVector Front, Up, Center;
	FVectorToAKVector(front, Front);
	FVectorToAKVector(up, Up);
	FVectorToAKVector(location, Center);

	AkPortalParams params;
	params.Transform.SetPosition(Center);
	params.Transform.SetOrientation(Front, Up);

	FVector scale = in_Portal->GetExtent();
	params.Extent.X = scale.X; // <- don't want to negate the X
	params.Extent.Y = scale.Z;
	params.Extent.Z = scale.Y;

	params.bEnabled = in_Portal->GetCurrentState() == AkAcousticPortalState::Open;
	params.strName = TCHAR_TO_ANSI(*nameStr); // This will copy the string inside operator=

	params.FrontRoom = in_Portal->GetFrontRoom();
	params.BackRoom = in_Portal->GetBackRoom();

	AK::SpatialAudio::SetPortal(portalID, params);
#endif
}

void FAkAudioDevice::RemoveSpatialAudioPortal(const AAkAcousticPortal* in_Portal)
{
	if (IsRunningCommandlet())
		return;

#ifdef AK_ENABLE_PORTALS
	AkPortalID portalID = in_Portal->GetPortalID();
	AK::SpatialAudio::RemovePortal(portalID);

	UpdateAllSpatialAudioRooms(in_Portal->GetWorld());
#endif
}


/** Get a sorted list of AkAuxSendValue at a location
 *
 * @param					Loc	Location at which to find Reverb Volumes
 * @param AkReverbVolumes	Array of AkAuxSendValue at this location
 */
void FAkAudioDevice::GetAuxSendValuesAtLocation(FVector Loc, TArray<AkAuxSendValue>& AkAuxSendValues, const UWorld* in_World)
{
	// Check if there are AkReverbVolumes at this location
	TArray<UAkLateReverbComponent*> FoundComponents = FindPrioritizedComponentsAtLocation(Loc, in_World, HighestPriorityLateReverbComponentMap);

	// Sort the found Volumes
	if (FoundComponents.Num() > 1)
	{
		FoundComponents.Sort([](const UAkLateReverbComponent& A, const UAkLateReverbComponent& B)
		{
			return A.Priority > B.Priority;
		});
	}

	// Apply the found Aux Sends
	AkAuxSendValue	TmpSendValue;
	// Build a list to set as AuxBusses
	for( uint8 Idx = 0; Idx < FoundComponents.Num() && Idx < MaxAuxBus; Idx++ )
	{
		TmpSendValue.listenerID = AK_INVALID_GAME_OBJECT;
		TmpSendValue.auxBusID = FoundComponents[Idx]->GetAuxBusId();
		TmpSendValue.fControlValue = FoundComponents[Idx]->SendLevel;
		AkAuxSendValues.Add(TmpSendValue);
	}
}

/**
 * Post an event and location to ak soundengine
 *
 * @param in_pEvent			Name of the event to post
 * @param in_Location		Location at which to play the event
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEventAtLocation(
	UAkAudioEvent * in_pEvent,
	FVector in_Location,
	FRotator in_Orientation,
	UWorld* in_World)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if ( in_pEvent )
	{
		if (!in_pEvent->IsMediaReady())
		{
			UE_LOG(LogAkAudio, Warning, TEXT("Not all media for '%s' are loaded. Consider using PostEventAtLocationAsync()."), *in_pEvent->GetName());
		}

		playingID = PostEventAtLocation(in_pEvent->GetName(), in_Location, in_Orientation, in_World);
	}

	return playingID;
}

void FAkAudioDevice::PostEventAtLocationEndOfEventCallback(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
{
	if (auto* Device = FAkAudioDevice::Get())
	{
		Device->RemovePlayingID(((AkEventCallbackInfo*)in_pCallbackInfo)->eventID, ((AkEventCallbackInfo*)in_pCallbackInfo)->playingID);
	}
}

/**
 * Post an event by name at location to ak soundengine
 *
 * @param in_pEvent			Name of the event to post
 * @param in_Location		Location at which to play the event
 * @return ID assigned by ak soundengine
 */
AkPlayingID FAkAudioDevice::PostEventAtLocation(
	const FString& in_EventName,
	FVector in_Location,
	FRotator in_Orientation,
	UWorld* in_World)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	if ( m_bSoundEngineInitialized )
	{
		const AkGameObjectID objId = (AkGameObjectID)&in_EventName;
		FAkAudioDevice_Helpers::RegisterGameObject(objId, in_EventName);

		TArray<AkAuxSendValue> AkReverbVolumes;
		GetAuxSendValuesAtLocation(in_Location, AkReverbVolumes, in_World);
		AK::SoundEngine::SetGameObjectAuxSendValues(objId, AkReverbVolumes.GetData(), AkReverbVolumes.Num());

		AkRoomID RoomID;
		TArray<UAkRoomComponent*> AkRooms = FindPrioritizedComponentsAtLocation(in_Location, in_World, HighestPriorityRoomComponentMap, 1);
		if (AkRooms.Num() > 0)
			RoomID = AkRooms[0]->GetRoomID();

		SetInSpatialAudioRoom(objId, RoomID);

		AkSoundPosition soundpos;
		FQuat tempQuat(in_Orientation);
		FVectorsToAKTransform(in_Location, tempQuat.GetForwardVector(), tempQuat.GetUpVector(), soundpos);

		AK::SoundEngine::SetPosition(objId, soundpos);

		uint32 eventID = GetIDFromString(in_EventName);
		playingID = AK::SoundEngine::PostEvent(eventID, objId, AK_EndOfEvent, &FAkAudioDevice::PostEventAtLocationEndOfEventCallback);
		if (playingID != AK_INVALID_PLAYING_ID)
		{
			FScopeLock Lock(&EventToPlayingIDMapCriticalSection);
			auto& PlayingIDs = EventToPlayingIDMap.FindOrAdd(eventID);
			PlayingIDs.Add(playingID);
		}
		AK::SoundEngine::UnregisterGameObj( objId );
	}

	return playingID;
}

UAkComponent* FAkAudioDevice::SpawnAkComponentAtLocation( class UAkAudioEvent* in_pAkEvent, FVector Location, FRotator Orientation, bool AutoPost, const FString& EventName, bool AutoDestroy, UWorld* in_World)
{
	UAkComponent * AkComponent = NULL;
	if (in_World)
	{
		AkComponent = NewObject<UAkComponent>(in_World->GetWorldSettings());
	}
	else
	{
		AkComponent = NewObject<UAkComponent>();
	}

	if( AkComponent )
	{
		AkComponent->AkAudioEvent = in_pAkEvent;
		AkComponent->EventName = EventName;
		AkComponent->SetWorldLocationAndRotation(Location, Orientation.Quaternion());
		if(in_World)
		{
			AkComponent->RegisterComponentWithWorld(in_World);
		}

		AkComponent->SetAutoDestroy(AutoDestroy);

		if(AutoPost)
		{
			if (AkComponent->PostAssociatedAkEvent(0, FOnAkPostEventCallback(), TArray<FAkExternalSourceInfo>()) == AK_INVALID_PLAYING_ID && AutoDestroy)
			{
				AkComponent->ConditionalBeginDestroy();
				AkComponent = NULL;
			}
		}
	}

	return AkComponent;
}

/**
 * Post a trigger to ak soundengine
 *
 * @param in_pszTrigger		Name of the trigger
 * @param in_pAkComponent	AkComponent on which to post the trigger
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::PostTrigger( 
	const TCHAR * in_pszTrigger,
	AActor * in_pActor
	)
{
	AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT;
	AKRESULT eResult = GetGameObjectID( in_pActor, GameObjID );
	if ( m_bSoundEngineInitialized && eResult == AK_Success)
	{
		eResult = AK::SoundEngine::PostTrigger(TCHAR_TO_AK(in_pszTrigger), GameObjID );
	}
	return eResult;
}

AKRESULT FAkAudioDevice::PostTrigger(
	const UAkTrigger* in_TriggerValue,
	AActor* in_pActor
)
{
	AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT;
	AKRESULT eResult = GetGameObjectID(in_pActor, GameObjID);
	if (m_bSoundEngineInitialized && in_TriggerValue && eResult == AK_Success)
	{
		eResult = AK::SoundEngine::PostTrigger(in_TriggerValue->ShortID, GameObjID);
	}
	return eResult;
}

/**
* Set a RTPC in ak soundengine
*
* @param in_pszRtpcName	Name of the RTPC
* @param in_value			Value to set
* @param in_pActor			Actor on which to set the RTPC
* @return Result from ak sound engine
*/
AKRESULT FAkAudioDevice::SetRTPCValue(
	const TCHAR * in_pszRtpcName,
	AkRtpcValue in_value,
	int32 in_interpolationTimeMs = 0,
	AActor * in_pActor = NULL
)
{
	AKRESULT eResult = AK_Success;
	if (m_bSoundEngineInitialized)
	{
		AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT; // RTPC at global scope is supported
		if (in_pActor)
		{
			eResult = GetGameObjectID(in_pActor, GameObjID);
			if (eResult != AK_Success)
				return eResult;
		}

		eResult = AK::SoundEngine::SetRTPCValue(TCHAR_TO_AK(in_pszRtpcName), in_value, GameObjID, in_interpolationTimeMs);
	}
	return eResult;
}

AKRESULT FAkAudioDevice::SetRTPCValue(
	const UAkRtpc* in_RtpcValue,
	AkRtpcValue in_value,
	int32 in_interpolationTimeMs = 0,
	AActor * in_pActor = NULL
)
{
	AKRESULT eResult = AK_Success;
	if (m_bSoundEngineInitialized && in_RtpcValue)
	{
		AkGameObjectID GameObjID = AK_INVALID_GAME_OBJECT; // RTPC at global scope is supported
		if (in_pActor)
		{
			eResult = GetGameObjectID(in_pActor, GameObjID);
			if (eResult != AK_Success)
				return eResult;
		}

		eResult = AK::SoundEngine::SetRTPCValue(in_RtpcValue->ShortID, in_value, GameObjID, in_interpolationTimeMs);
	}
	return eResult;
}

/**
 *  Get the value of a real-time parameter control (by ID)
 *  An RTPC can have a any combination of a global value, a unique value for each game object, or a unique value for each playing ID.  
 *  The value requested is determined by RTPCValue_type, in_gameObjectID and in_playingID.  
 *  If a value at the requested scope (determined by RTPCValue_type) is not found, the value that is available at the the next broadest scope will be returned, and io_rValueType will be changed to indicate this.
 *  @note
 * 		When looking up RTPC values via playing ID (ie. io_rValueType is RTPC_PlayingID), in_gameObjectID can be set to a specific game object (if it is available to the caller) to use as a fall back value.
 * 		If the game object is unknown or unavailable, AK_INVALID_GAME_OBJECT can be passed in in_gameObjectID, and the game object will be looked up via in_playingID.  
 * 		However in this case, it is not possible to retrieve a game object value as a fall back value if the playing id does not exist.  It is best to pass in the game object if possible.
 * 		
 *  @return AK_Success if succeeded, AK_IDNotFound if the game object was not registered, or AK_Fail if the RTPC value could not be obtained
 */
AKRESULT FAkAudioDevice::GetRTPCValue(
	const TCHAR * in_pszRtpcName,
	AkGameObjectID in_gameObjectID,		///< Associated game object ID, ignored if io_rValueType is RTPCValue_Global.
	AkPlayingID	in_playingID,			///< Associated playing ID, ignored if io_rValueType is not RTPC_PlayingID.
	AkRtpcValue& out_rValue, 			///< Value returned
	AK::SoundEngine::Query::RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.				);
)
{
	AKRESULT eResult = AK_Success;
	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SoundEngine::Query::GetRTPCValue(TCHAR_TO_AK(in_pszRtpcName), in_gameObjectID, in_playingID, out_rValue, io_rValueType);
	}
	return eResult;
}

AKRESULT FAkAudioDevice::GetRTPCValue(
	const UAkRtpc* in_RtpcValue,
	AkGameObjectID in_gameObjectID,		///< Associated game object ID, ignored if io_rValueType is RTPCValue_Global.
	AkPlayingID	in_playingID,			///< Associated playing ID, ignored if io_rValueType is not RTPC_PlayingID.
	AkRtpcValue& out_rValue, 			///< Value returned
	AK::SoundEngine::Query::RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.				);
)
{
	AKRESULT eResult = AK_Success;
	if (m_bSoundEngineInitialized && in_RtpcValue)
	{
		eResult = AK::SoundEngine::Query::GetRTPCValue(in_RtpcValue->ShortID, in_gameObjectID, in_playingID, out_rValue, io_rValueType);
	}
	return eResult;
}

/**
 * Set a state in ak soundengine
 *
 * @param in_pszStateGroup	Name of the state group
 * @param in_pszState		Name of the state
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetState( 
	const TCHAR * in_pszStateGroup,
	const TCHAR * in_pszState
    )
{
	AKRESULT eResult = AK_Success;
	if ( m_bSoundEngineInitialized )
	{
		auto StateGroupID = AK::SoundEngine::GetIDFromString(TCHAR_TO_AK(in_pszStateGroup));
		auto StateID = AK::SoundEngine::GetIDFromString(TCHAR_TO_AK(in_pszState));
		eResult = AK::SoundEngine::SetState(StateGroupID, StateID);
	}
	return eResult;
}

AKRESULT FAkAudioDevice::SetState(
	const UAkStateValue* in_stateValue
)
{
	AKRESULT eResult = AK_Success;
	if (m_bSoundEngineInitialized && in_stateValue)
	{
		eResult = AK::SoundEngine::SetState(in_stateValue->GroupShortID, in_stateValue->ShortID);
	}
	return eResult;
}

/**
 * Set a switch in ak soundengine
 *
 * @param in_pszSwitchGroup	Name of the switch group
 * @param in_pszSwitchState	Name of the switch
 * @param in_pComponent		AkComponent on which to set the switch
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetSwitch(
	const TCHAR * in_pszSwitchGroup,
	const TCHAR * in_pszSwitchState,
	AActor * in_pActor
	)
{
	AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
	// Switches must be bound to a game object. passing DUMMY_GAMEOBJ as default game object.
	AKRESULT eResult = GetGameObjectID( in_pActor, GameObjID );
	if ( m_bSoundEngineInitialized && eResult == AK_Success)
	{
		auto SwitchGroupID = AK::SoundEngine::GetIDFromString(TCHAR_TO_AK(in_pszSwitchGroup));
		auto SwitchStateID = AK::SoundEngine::GetIDFromString(TCHAR_TO_AK(in_pszSwitchState));
		eResult = AK::SoundEngine::SetSwitch(SwitchGroupID, SwitchStateID, GameObjID);
	}
	return eResult;
}

AKRESULT FAkAudioDevice::SetSwitch(
	const UAkSwitchValue* in_switchValue,
	AActor * in_pActor
)
{
	AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
	// Switches must be bound to a game object. passing DUMMY_GAMEOBJ as default game object.
	AKRESULT eResult = GetGameObjectID(in_pActor, GameObjID);
	if (m_bSoundEngineInitialized && in_switchValue && eResult == AK_Success)
	{
		eResult = AK::SoundEngine::SetSwitch(in_switchValue->GroupShortID, in_switchValue->ShortID, GameObjID);
	}
	return eResult;
}

static AK::SoundEngine::MultiPositionType GetSoundEngineMultiPositionType(AkMultiPositionType in_eType)
{
    switch (in_eType)
    {
    case AkMultiPositionType::SingleSource: return AK::SoundEngine::MultiPositionType_SingleSource;
    case AkMultiPositionType::MultiSources: return AK::SoundEngine::MultiPositionType_MultiSources;
    case AkMultiPositionType::MultiDirections: return AK::SoundEngine::MultiPositionType_MultiDirections;
        // Unknown multi position type!
    default: AKASSERT(false); return AK::SoundEngine::MultiPositionType_SingleSource;
    }
}

/** Sets multiple positions to a single game object.
*  Setting multiple positions on a single game object is a way to simulate multiple emission sources while using the resources of only one voice.
*  This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area.
*  Note: Calling AK::SoundEngine::SetMultiplePositions() with only one position is the same as calling AK::SoundEngine::SetPosition()
*  @param in_pGameObjectAkComponent Game Object AkComponent.
*  @param in_pPositions Array of positions to apply.
*  @param in_eMultiPositionType Position type
*  @return AK_Success when successful, AK_InvalidParameter if parameters are not valid.
*
*/
AKRESULT FAkAudioDevice::SetMultiplePositions(
    UAkComponent* in_pGameObjectAkComponent,
    TArray<FTransform> in_aPositions,
    AkMultiPositionType in_eMultiPositionType /*= AkMultiPositionType::MultiDirections*/
)
{
	if (!in_pGameObjectAkComponent)
	{
		return AK_Fail;
	}

	const int numPositions = in_aPositions.Num();
    TArray<AkSoundPosition> aPositions;
    aPositions.Empty();
    for (int i = 0; i < numPositions; ++i)
    {
        AkSoundPosition soundpos;
        FAkAudioDevice::FVectorsToAKTransform(in_aPositions[i].GetLocation(), in_aPositions[i].GetRotation().GetForwardVector(), in_aPositions[i].GetRotation().GetUpVector(), soundpos);
        aPositions.Add(soundpos);
    }
    return AK::SoundEngine::SetMultiplePositions(in_pGameObjectAkComponent->GetAkGameObjectID(), aPositions.GetData(),
                                                 aPositions.Num(), GetSoundEngineMultiPositionType(in_eMultiPositionType));
}

template<typename ChannelConfig>
AKRESULT FAkAudioDevice::SetMultiplePositions(
	UAkComponent* in_pGameObjectAkComponent,
	const TArray<ChannelConfig>& in_aChannelConfigurations,
	const TArray<FTransform>& in_aPositions,
	AkMultiPositionType in_eMultiPositionType /*= AkMultiPositionType::MultiDirections*/
)
{
	if (!in_pGameObjectAkComponent)
	{
		return AK_Fail;
	}

	const int32 numPositions = FMath::Min(in_aPositions.Num(), in_aChannelConfigurations.Num());

	TArray<AkChannelEmitter> emitters;
	emitters.Reserve(numPositions);
	for (int i = 0; i < numPositions; ++i)
	{
		AkSoundPosition soundpos;
		FAkAudioDevice::FVectorsToAKTransform(in_aPositions[i].GetLocation(), in_aPositions[i].GetRotation().GetForwardVector(), in_aPositions[i].GetRotation().GetUpVector(), soundpos);

		AkChannelConfig config;
		GetChannelConfig(in_aChannelConfigurations[i], config);

		emitters.Add(AkChannelEmitter());
		emitters[i].uInputChannels = config.uChannelMask;
		emitters[i].position = soundpos;
	}

	return AK::SoundEngine::SetMultiplePositions(in_pGameObjectAkComponent->GetAkGameObjectID(), emitters.GetData(),
		emitters.Num(), GetSoundEngineMultiPositionType(in_eMultiPositionType));
}

AKRESULT FAkAudioDevice::SetMultiplePositions(
	UAkComponent* in_pGameObjectAkComponent,
	const TArray<AkChannelConfiguration>& in_aChannelConfigurations,
	const TArray<FTransform>& in_aPositions,
	AkMultiPositionType in_eMultiPositionType
)
{
	return SetMultiplePositions<AkChannelConfiguration>(in_pGameObjectAkComponent, in_aChannelConfigurations, in_aPositions, in_eMultiPositionType);
}

AKRESULT FAkAudioDevice::SetMultiplePositions(
	UAkComponent* in_pGameObjectAkComponent,
	const TArray<FAkChannelMask>& in_channelMasks,
	const TArray<FTransform>& in_aPositions,
	AkMultiPositionType in_eMultiPositionType
)
{
	return SetMultiplePositions<FAkChannelMask>(in_pGameObjectAkComponent, in_channelMasks, in_aPositions, in_eMultiPositionType);
}

/** Sets multiple positions to a single game object.
*  Setting multiple positions on a single game object is a way to simulate multiple emission sources while using the resources of only one voice.
*  This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area.
*  Note: Calling AK::SoundEngine::SetMultiplePositions() with only one position is the same as calling AK::SoundEngine::SetPosition()
*  @param in_GameObjectID Game Object identifier.
*  @param in_pPositions Array of positions to apply.
*  @param in_NumPositions Number of positions specified in the provided array.
*  @param in_eMultiPositionType Position type
*  @return AK_Success when successful, AK_InvalidParameter if parameters are not valid.
*/
AKRESULT FAkAudioDevice::SetMultiplePositions(
    AkGameObjectID in_GameObjectID,
    const AkSoundPosition * in_pPositions,
    AkUInt16 in_NumPositions,
    AK::SoundEngine::MultiPositionType in_eMultiPositionType /*= AK::SoundEngine::MultiDirections*/
    )
{
    return AK::SoundEngine::SetMultiplePositions(in_GameObjectID, in_pPositions, in_NumPositions, in_eMultiPositionType);
}

/** Sets multiple positions to a single game object, with flexible assignment of input channels.
*  Setting multiple positions on a single game object is a way to simulate multiple emission sources while using the resources of only one voice.
*  This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area.
*  Note: Calling AK::SoundEngine::SetMultiplePositions() with only one position is the same as calling AK::SoundEngine::SetPosition()
*  @param in_GameObjectID Game Object identifier.
*  @param in_pPositions Array of positions to apply.
*  @param in_NumPositions Number of positions specified in the provided array.
*  @param in_eMultiPositionType Position type
*  @return AK_Success when successful, AK_InvalidParameter if parameters are not valid.
*/
AKRESULT FAkAudioDevice::SetMultiplePositions(
    AkGameObjectID in_GameObjectID,
    const AkChannelEmitter * in_pPositions,
    AkUInt16 in_NumPositions,
    AK::SoundEngine::MultiPositionType in_eMultiPositionType /*= AK::SoundEngine::MultiDirections*/
    )
{
    return AK::SoundEngine::SetMultiplePositions(in_GameObjectID, in_pPositions, in_NumPositions, in_eMultiPositionType);
}

/**
 * Set auxiliary sends
 *
 * @param in_GameObjId		Wwise Game Object ID
 * @param in_AuxSendValues	Array of AkAuxSendValue, containins all Aux Sends to set on the game objectt
 * @return Result from ak sound engine
 */
AKRESULT FAkAudioDevice::SetAuxSends(
	const UAkComponent* in_akComponent,
	TArray<AkAuxSendValue>& in_AuxSendValues
	)
{
	AKRESULT eResult = AK_Success;
	if ( m_bSoundEngineInitialized )
	{
		eResult = AK::SoundEngine::SetGameObjectAuxSendValues(in_akComponent->GetAkGameObjectID(), in_AuxSendValues.GetData(), in_AuxSendValues.Num());
	}
	
	return eResult;
}

void FAkAudioDevice::GetChannelConfig(AkChannelConfiguration ChannelConfiguration, AkChannelConfig& config)
{
	switch (ChannelConfiguration)
	{
	case AkChannelConfiguration::Ak_LFE:
		config.SetStandard(AK_SPEAKER_SETUP_0POINT1);
		break;
	case AkChannelConfiguration::Ak_1_0:
		config.SetStandard(AK_SPEAKER_SETUP_MONO);
		break;
	case AkChannelConfiguration::Ak_2_0:
		config.SetStandard(AK_SPEAKER_SETUP_STEREO);
		break;
	case AkChannelConfiguration::Ak_2_1:
		config.SetStandard(AK_SPEAKER_SETUP_2POINT1);
		break;
	case AkChannelConfiguration::Ak_3_0:
		config.SetStandard(AK_SPEAKER_SETUP_3STEREO);
		break;
	case AkChannelConfiguration::Ak_3_1:
		config.SetStandard(AK_SPEAKER_SETUP_3POINT1);
		break;
	case AkChannelConfiguration::Ak_4_0:
		config.SetStandard(AK_SPEAKER_SETUP_4);
		break;
	case AkChannelConfiguration::Ak_4_1:
		config.SetStandard(AK_SPEAKER_SETUP_4POINT1);
		break;
	case AkChannelConfiguration::Ak_5_0:
		config.SetStandard(AK_SPEAKER_SETUP_5);
		break;
	case AkChannelConfiguration::Ak_5_1:
		config.SetStandard(AK_SPEAKER_SETUP_5POINT1);
		break;
	case AkChannelConfiguration::Ak_7_1:
		config.SetStandard(AK_SPEAKER_SETUP_7POINT1);
		break;
	case AkChannelConfiguration::Ak_5_1_2:
		config.SetStandard(AK_SPEAKER_SETUP_DOLBY_5_1_2);
		break;
	case AkChannelConfiguration::Ak_7_1_2:
		config.SetStandard(AK_SPEAKER_SETUP_DOLBY_7_1_2);
		break;
	case AkChannelConfiguration::Ak_7_1_4:
		config.SetStandard(AK_SPEAKER_SETUP_DOLBY_7_1_4);
		break;
	case AkChannelConfiguration::Ak_Auro_9_1:
		config.SetStandard(AK_SPEAKER_SETUP_AURO_9POINT1);
		break;
	case AkChannelConfiguration::Ak_Auro_10_1:
		config.SetStandard(AK_SPEAKER_SETUP_AURO_10POINT1);
		break;
	case AkChannelConfiguration::Ak_Auro_11_1:
		config.SetStandard(AK_SPEAKER_SETUP_AURO_11POINT1);
		break;
	case AkChannelConfiguration::Ak_Auro_13_1:
		config.SetStandard(AK_SPEAKER_SETUP_AURO_13POINT1_751);
		break;
	case AkChannelConfiguration::Ak_Ambisonics_1st_order:
		config.SetAmbisonic(4);
		break;
	case AkChannelConfiguration::Ak_Ambisonics_2nd_order:
		config.SetAmbisonic(9);
		break;
	case AkChannelConfiguration::Ak_Ambisonics_3rd_order:
		config.SetAmbisonic(16);
		break;
	case AkChannelConfiguration::Ak_Parent:
	default:
		config.Clear();
		break;
	}
}

void FAkAudioDevice::GetChannelConfig(FAkChannelMask SpeakerConfig, AkChannelConfig& config)
{
	config.SetStandard(SpeakerConfig.ChannelMask);
}

/**
* Set spatial audio room
*
* @param in_GameObjId		Wwise Game Object ID
* @param in_RoomID	ID of the room that the game object is inside.
* @return Result from ak sound engine
*/
AKRESULT FAkAudioDevice::SetInSpatialAudioRoom(
	const AkGameObjectID in_GameObjId,
	AkRoomID in_RoomID
)
{
	AKRESULT eResult = AK_Success;
#ifdef AK_ENABLE_ROOMS
	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SpatialAudio::SetGameObjectInRoom(in_GameObjId, in_RoomID);
	}
#endif
	return eResult;
}

AKRESULT FAkAudioDevice::SetBusConfig(
	const FString&	in_BusName,
	AkChannelConfig	in_Config
	)
{
	AKRESULT eResult = AK_Fail;
	if (in_BusName.IsEmpty())
	{
		return eResult;
	}

	if (m_bSoundEngineInitialized)
	{
		AkUniqueID BusId = GetIDFromString(in_BusName);
		eResult = AK::SoundEngine::SetBusConfig(BusId, in_Config);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetPanningRule(
	AkPanningRule		in_ePanningRule
	)
{
	AKRESULT eResult = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SoundEngine::SetPanningRule(in_ePanningRule);
	}

	return eResult;
}

AkOutputDeviceID FAkAudioDevice::GetOutputID(
	const FString& in_szShareSet,
	AkUInt32 in_idDevice
	)
{
	return AK::SoundEngine::GetOutputID(TCHAR_TO_AK(*in_szShareSet), in_idDevice);
}

AKRESULT FAkAudioDevice::GetSpeakerAngles(
	TArray<AkReal32>& out_pfSpeakerAngles,
	AkReal32& out_fHeightAngle,
	AkOutputDeviceID in_idOutput
	)
{
	AKRESULT eResult = AK_Fail;

	if (m_bSoundEngineInitialized)
	{
		AkUInt32 numSpeakers;

		// Retrieve the number of speaker and height angle
		eResult = AK::SoundEngine::GetSpeakerAngles(NULL, numSpeakers, out_fHeightAngle);
		if (eResult != AK_Success)
			return eResult;

		// Retrieve the speaker angles
		out_pfSpeakerAngles.SetNum(numSpeakers);
		eResult = AK::SoundEngine::GetSpeakerAngles(out_pfSpeakerAngles.GetData(), numSpeakers, out_fHeightAngle, in_idOutput);
	}
	
	return eResult;
}

AKRESULT FAkAudioDevice::SetSpeakerAngles(
	const TArray<AkReal32>& in_pfSpeakerAngles,
	AkReal32 in_fHeightAngle,
	AkOutputDeviceID in_idOutput
	)
{
	AKRESULT eResult = AK_Fail;

	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SoundEngine::SetSpeakerAngles(in_pfSpeakerAngles.GetData(), in_pfSpeakerAngles.Num(), in_fHeightAngle, in_idOutput);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetGameObjectOutputBusVolume(
	const UAkComponent* in_pEmitter,
	const UAkComponent* in_pListener,
	float in_fControlValue	
	)
{
	AKRESULT eResult = AK_Success;

	if (m_bSoundEngineInitialized)
	{
		const AkGameObjectID emitterId = in_pEmitter ? in_pEmitter->GetAkGameObjectID() : DUMMY_GAMEOBJ;
		const AkGameObjectID listenerId = in_pListener ? in_pListener->GetAkGameObjectID() : DUMMY_GAMEOBJ;
		eResult = AK::SoundEngine::SetGameObjectOutputBusVolume(emitterId, listenerId, in_fControlValue);
	}

	return eResult;
}

/**
 * Obtain a pointer to the singleton instance of FAkAudioDevice
 *
 * @return Pointer to the singleton instance of FAkAudioDevice
 */
FAkAudioDevice * FAkAudioDevice::Get()
{
	if (UNLIKELY(m_EngineExiting))
	{
		return nullptr;
	}

	if (LIKELY(FAkAudioModule::AkAudioModuleIntance != nullptr))
	{
		return FAkAudioModule::AkAudioModuleIntance->GetAkAudioDevice();
	}
	else
	{
		static FName AkAudioName = TEXT("AkAudio");
		FAkAudioModule* mod = FModuleManager::LoadModulePtr<FAkAudioModule>(AkAudioName);
		ensure(FAkAudioModule::AkAudioModuleIntance == mod);
		return mod ? mod->GetAkAudioDevice() : nullptr;
	}
}

/**
 * Stop all audio associated with a game object
 *
 * @param in_GameObjID		ID of the game object
 */
void FAkAudioDevice::StopGameObject( UAkComponent * in_pComponent )
{
	AkGameObjectID gameObjId = DUMMY_GAMEOBJ;
	if ( in_pComponent )
	{
		gameObjId = in_pComponent->GetAkGameObjectID();
	}
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopAll( gameObjId );
	}
}

/**
 * Stop all audio associated with a playing ID
 *
 * @param in_playingID		Playing ID to stop
 * @param in_uTransitionDuration Fade duration
 * @param in_eFadeCurve          Curve type to be used for the transition
 */
void FAkAudioDevice::StopPlayingID( AkPlayingID in_playingID, 
                                    AkTimeMs in_uTransitionDuration /*= 0*/, 
                                    AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/)
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType_Stop, in_playingID, in_uTransitionDuration, in_eFadeCurve );
	}
}

/**
 * Register an ak audio component with ak sound engine
 *
 * @param in_pComponent		Pointer to the component to register
 */
void FAkAudioDevice::RegisterComponent( UAkComponent * in_pComponent )
{
	if (m_bSoundEngineInitialized && in_pComponent)
	{
		if (in_pComponent->UseDefaultListeners())
			m_defaultEmitters.Add(in_pComponent);

		FString WwiseGameObjectName = TEXT("");
		in_pComponent->GetAkGameObjectName(WwiseGameObjectName);
		
		const AkGameObjectID gameObjId = in_pComponent->GetAkGameObjectID();
		FAkAudioDevice_Helpers::RegisterGameObject(gameObjId, WwiseGameObjectName);

		if (CallbackManager != nullptr)
			CallbackManager->RegisterGameObject(gameObjId);
	}
}

/**
 * Unregister an ak audio component with ak sound engine
 *
 * @param in_pComponent		Pointer to the component to unregister
 */
void FAkAudioDevice::UnregisterComponent( UAkComponent * in_pComponent )
{
	if (m_bSoundEngineInitialized && in_pComponent)
	{
		const AkGameObjectID gameObjId = in_pComponent->GetAkGameObjectID();
		AK::SoundEngine::UnregisterGameObj(gameObjId);

		if (CallbackManager != nullptr)
		{
			CallbackManager->UnregisterGameObject(gameObjId);
		}
	}

	if (m_defaultListeners.Contains(in_pComponent))
	{
		RemoveDefaultListener(in_pComponent);
	}

	if (in_pComponent->UseDefaultListeners())
	{
		m_defaultEmitters.Remove(in_pComponent);
	}

	check(!m_defaultListeners.Contains(in_pComponent) && !m_defaultEmitters.Contains(in_pComponent));

	if (m_SpatialAudioListener == in_pComponent)
		m_SpatialAudioListener = nullptr;
}
AKRESULT FAkAudioDevice::SetGeometry(AkGeometrySetID AcousticZoneID, const AkGeometryParams& Params)
{
	AKRESULT eResult = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SpatialAudio::SetGeometry(AcousticZoneID, Params);  
	}

	return eResult;
}

AKRESULT FAkAudioDevice::RemoveGeometrySet(AkGeometrySetID AcousticZoneID)
{
	AKRESULT eResult = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		eResult = AK::SpatialAudio::RemoveGeometry(AcousticZoneID);
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetEarlyReflectionsAuxBus(UAkComponent* in_pComponent, const FString& in_AuxBusName)
{
	AKRESULT eResult = AK_Fail;
	if (m_bSoundEngineInitialized && in_pComponent)
	{
		const AkGameObjectID gameObjId = in_pComponent->GetAkGameObjectID();
		eResult = AK::SpatialAudio::SetEarlyReflectionsAuxSend(gameObjId, GetIDFromString(in_AuxBusName));
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetEarlyReflectionsVolume(UAkComponent* in_pComponent, float in_fSendVolume)
{
	AKRESULT eResult = AK_Fail;
	if (m_bSoundEngineInitialized && in_pComponent)
	{
		const AkGameObjectID gameObjId = in_pComponent->GetAkGameObjectID();
		eResult = AK::SpatialAudio::SetEarlyReflectionsVolume(gameObjId, in_fSendVolume);
	}

	return eResult;
}

void FAkAudioDevice::UpdateDefaultActiveListeners()
{
	if (m_bSoundEngineInitialized)
	{
		auto NumDefaultListeners = m_defaultListeners.Num();
		auto pListenerIds = (AkGameObjectID*)alloca(NumDefaultListeners * sizeof(AkGameObjectID));
		int index = 0;
		for (auto DefaultListenerIter = m_defaultListeners.CreateConstIterator(); DefaultListenerIter; ++DefaultListenerIter)
			pListenerIds[index++] = (*DefaultListenerIter)->GetAkGameObjectID();

		AK::SoundEngine::SetDefaultListeners(pListenerIds, NumDefaultListeners);
	}
}

AKRESULT FAkAudioDevice::SetPosition(UAkComponent* in_akComponent, const AkTransform& in_SoundPosition)
{
	if (m_bSoundEngineInitialized)
	{
		return AK::SoundEngine::SetPosition(in_akComponent->GetAkGameObjectID(), in_SoundPosition);
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::AddRoom(UAkRoomComponent* in_pRoom, const AkRoomParams& in_RoomParams)
{
	AKRESULT result = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		result = AK::SpatialAudio::SetRoom(in_pRoom->GetRoomID(), in_RoomParams);
		if (result == AK_Success)
		{
			AddRoomComponentToPrioritizedList(in_pRoom);
		}
	}
	return result;
}

AKRESULT FAkAudioDevice::UpdateRoom(UAkRoomComponent* in_pRoom, const AkRoomParams& in_RoomParams)
{
	AKRESULT result = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		result = AK::SpatialAudio::SetRoom(in_pRoom->GetRoomID(), in_RoomParams);
	}
	return result;
}

AKRESULT FAkAudioDevice::RemoveRoom(UAkRoomComponent* in_pRoom)
{
	AKRESULT result = AK_Fail;
	if (m_bSoundEngineInitialized)
	{
		result = AK::SpatialAudio::RemoveRoom(in_pRoom->GetRoomID());
		if (result == AK_Success)
		{
			RemoveRoomComponentFromPrioritizedList(in_pRoom);
		}
	}

	return result;
}

AKRESULT FAkAudioDevice::SetImageSource(AAkSpotReflector* in_pSpotReflector, const AkImageSourceSettings& in_ImageSourceInfo, AkUniqueID in_AuxBusID, AkRoomID in_RoomID, UAkComponent* in_AkComponent)
{
	if (m_bSoundEngineInitialized)
	{
		return AK::SpatialAudio::SetImageSource(in_pSpotReflector->GetImageSourceID(), in_ImageSourceInfo, in_AuxBusID, in_RoomID, in_AkComponent->GetAkGameObjectID());
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::RemoveImageSource(AAkSpotReflector* in_pSpotReflector, AkUniqueID in_AuxBusID, UAkComponent* in_AkComponent)
{
	if (m_bSoundEngineInitialized)
	{
		return AK::SpatialAudio::RemoveImageSource(in_pSpotReflector->GetImageSourceID(), in_AuxBusID, in_AkComponent->GetAkGameObjectID());
	}

	return AK_Fail;
}

AKRESULT FAkAudioDevice::ClearImageSources(AkUniqueID in_AuxBusID, UAkComponent* in_AkComponent)
{
	if (m_bSoundEngineInitialized)
	{
		return AK::SpatialAudio::ClearImageSources(in_AuxBusID, in_AkComponent == NULL ? AK_INVALID_GAME_OBJECT : in_AkComponent->GetAkGameObjectID());
	}

	return AK_Fail;
}

void FAkAudioDevice::SetListeners(UAkComponent* in_pEmitter, const TArray<UAkComponent*>& in_listenerSet)
{
	check(!in_pEmitter->UseDefaultListeners());

	m_defaultEmitters.Remove(in_pEmitter); //This emitter is no longer using the default listener set.

	auto NumListeners = in_listenerSet.Num();
	auto pListenerIds = (AkGameObjectID*)alloca(NumListeners * sizeof(AkGameObjectID));
	int index = 0;
	for (const auto& Listener : in_listenerSet)
		pListenerIds[index++] = Listener->GetAkGameObjectID();

	AK::SoundEngine::SetListeners(in_pEmitter->GetAkGameObjectID(), pListenerIds, NumListeners);
}

bool FAkAudioDevice::SetSpatialAudioListener(UAkComponent* in_pListener)
{
#if WITH_EDITOR
	if (in_pListener == EditorListener)
	{
		return false;
	}
#endif
	m_SpatialAudioListener = in_pListener;

	AK::SpatialAudio::RegisterListener((AkGameObjectID)m_SpatialAudioListener);
	return true;
}

UAkComponent* FAkAudioDevice::GetSpatialAudioListener() const
{
	return m_SpatialAudioListener;
}

UAkComponent* FAkAudioDevice::GetAkComponent(class USceneComponent* AttachToComponent, FName AttachPointName, const FVector * Location, EAttachLocation::Type LocationType)
{
	bool ComponentCreated;
	return GetAkComponent(AttachToComponent, AttachPointName, Location, LocationType, ComponentCreated);
}

UAkComponent* FAkAudioDevice::GetAkComponent( class USceneComponent* AttachToComponent, FName AttachPointName, const FVector * Location, EAttachLocation::Type LocationType, bool& ComponentCreated )
{
	if (!AttachToComponent)
	{
		return NULL;
	}

	UAkComponent* AkComponent = NULL;
	FAttachmentTransformRules AttachRules = FAttachmentTransformRules::KeepRelativeTransform;

	if( GEngine && AK::SoundEngine::IsInitialized())
	{
		AActor * Actor = AttachToComponent->GetOwner();
		if( Actor ) 
		{
			if( Actor->IsPendingKill() )
			{
				// Avoid creating component if we're trying to play a sound on an already destroyed actor.
				return NULL;
			}

			TArray<UAkComponent*> AkComponents;
			Actor->GetComponents(AkComponents);
			for ( int32 CompIdx = 0; CompIdx < AkComponents.Num(); CompIdx++ )
			{
				UAkComponent* pCompI = AkComponents[CompIdx];
				if ( pCompI && pCompI->IsRegistered() )
				{
					if ( AttachToComponent == pCompI )
					{
						return pCompI;
					}

					if ( AttachToComponent != pCompI->GetAttachParent() 
						|| AttachPointName != pCompI->GetAttachSocketName() )
					{
						continue;
					}

					// If a location is requested, try to match location.
					if ( Location )
					{
						if (LocationType == EAttachLocation::KeepWorldPosition)
						{
							AttachRules = FAttachmentTransformRules::KeepWorldTransform;
							if ( !FVector::PointsAreSame(*Location, pCompI->GetComponentLocation()) )
								continue;
						}
						else
						{
							AttachRules = FAttachmentTransformRules::KeepRelativeTransform;
#if UE_4_24_OR_LATER
							auto RelLoc = pCompI->GetRelativeLocation();
#else
							auto RelLoc = pCompI->RelativeLocation;
#endif
							if ( !FVector::PointsAreSame(*Location, RelLoc) )
								continue;
						}
					}

					// AkComponent found which exactly matches the attachment: reuse it.
					ComponentCreated = false;
					return pCompI;
				}
			}
		}
		else
		{
			// Try to find if there is an AkComponent attached to AttachToComponent (will be the case if AttachToComponent has no owner)
			const TArray<USceneComponent*> AttachChildren = AttachToComponent->GetAttachChildren();
			for(int32 CompIdx = 0; CompIdx < AttachChildren.Num(); CompIdx++)
			{
				UAkComponent* pCompI = Cast<UAkComponent>(AttachChildren[CompIdx]);
				if ( pCompI && pCompI->IsRegistered() )
				{
					// There is an associated AkComponent to AttachToComponent, no need to add another one.
					ComponentCreated = false;
					return pCompI;
				}
			}
		}

		if ( AkComponent == NULL )
		{
			if( Actor )
			{
				AkComponent = NewObject<UAkComponent>(Actor);
			}
			else
			{
				AkComponent = NewObject<UAkComponent>();
			}
		}

		ComponentCreated = true;
		check( AkComponent );

		if (Location)
		{
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				AttachRules = FAttachmentTransformRules::KeepWorldTransform;
				AkComponent->SetWorldLocation(*Location);
			}
			else
			{
				AttachRules = FAttachmentTransformRules::KeepRelativeTransform;
				AkComponent->SetRelativeLocation(*Location);
			}
		}

		AkComponent->RegisterComponentWithWorld(AttachToComponent->GetWorld());
		AkComponent->AttachToComponent(AttachToComponent, AttachRules, AttachPointName);
	}

	return( AkComponent );
}


/**
* Cancel the callback cookie for a dispatched event
*
* @param in_cookie			The cookie to cancel
*/
void FAkAudioDevice::CancelEventCallbackCookie(void* in_cookie)
{
	if (m_bSoundEngineInitialized)
	{
		CallbackManager->CancelEventCallback(in_cookie);
	}
}

/**
* Cancel the callback cookie for a dispatched event
*
* @param in_cookie			The cookie to cancel
*/
void FAkAudioDevice::CancelEventCallbackDelegate(const FOnAkPostEventCallback& in_Delegate)
{
	if (m_bSoundEngineInitialized)
	{
		CallbackManager->CancelEventCallback(in_Delegate);
	}
}

AKRESULT FAkAudioDevice::SetAttenuationScalingFactor(AActor* Actor, float ScalingFactor)
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized )
	{
		AkGameObjectID GameObjID = DUMMY_GAMEOBJ;
		eResult = GetGameObjectID( Actor, GameObjID );
		if( eResult == AK_Success )
		{
			eResult = AK::SoundEngine::SetScalingFactor(GameObjID, ScalingFactor);
		}
	}

	return eResult;
}

AKRESULT FAkAudioDevice::SetAttenuationScalingFactor(UAkComponent* AkComponent, float ScalingFactor)
{
	AKRESULT eResult = AK_Fail;
	if ( m_bSoundEngineInitialized && AkComponent)
	{
		eResult = AK::SoundEngine::SetScalingFactor(AkComponent->GetAkGameObjectID(), ScalingFactor);
	}
	return eResult;
}

static void UELocalOutputFunc(
	AK::Monitor::ErrorCode in_eErrorCode,
	const AkOSChar* in_pszError,
	AK::Monitor::ErrorLevel in_eErrorLevel,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID )
{
    FString AkError(in_pszError);
    
	if( !IsRunningCommandlet() )
	{
		if ( in_eErrorLevel == AK::Monitor::ErrorLevel_Message )
		{
			UE_LOG( LogAkAudio, Log, TEXT("%s"), *AkError);
		}
		else
		{
			UE_LOG( LogAkAudio, Error, TEXT("%s"), *AkError);
		}
	}
}

bool FAkAudioDevice::EnsureInitialized()
{
	// We don't want sound in those cases.
	if (FParse::Param(FCommandLine::Get(), TEXT("nosound")) || FApp::IsBenchmarking() || IsRunningDedicatedServer() || IsRunningCommandlet())
	{
		return false;
	}

	if (m_bSoundEngineInitialized)
	{
		return true;
	}

	UE_LOG(LogAkAudio,
		Log,
		TEXT("Wwise(R) SDK Version %d.%d.%d Build %d. Copyright (c) 2006-%d Audiokinetic Inc."),
		AK_WWISESDK_VERSION_MAJOR,
		AK_WWISESDK_VERSION_MINOR,
		AK_WWISESDK_VERSION_SUBMINOR,
		AK_WWISESDK_VERSION_BUILD,
		AK_WWISESDK_VERSION_MAJOR);

	AkIntegrationBehavior::Get()->AkAudioDevice_CreateIOHook(this);

	if (FAkSoundEngineInitialization::Initialize(IOHook))
	{
		UE_LOG(LogAkAudio, Log, TEXT("Wwise successfully initialized."));
	}
	else
	{
		delete IOHook;
		IOHook = nullptr;

		FAkSoundEngineInitialization::Finalize();
		return false;
	}

	// Enable AK error redirection to UE log.
	AK::Monitor::SetLocalOutput(AK::Monitor::ErrorLevel_All, UELocalOutputFunc);

	// Init dummy game object
	AK::SoundEngine::RegisterGameObj(DUMMY_GAMEOBJ, "Unreal Global");

#if WITH_EDITOR
	if (!IsRunningGame())
	{
		AkGameObjectID tempID = DUMMY_GAMEOBJ;
		AK::SoundEngine::SetListeners(DUMMY_GAMEOBJ, &tempID, 1);
	}
#endif

	m_bSoundEngineInitialized = true;

	AkBankManager = new FAkBankManager;

	CallbackInfoPool = new AkCallbackInfoPool;
	// Go get the max number of Aux busses
	MaxAuxBus = AK_MAX_AUX_PER_OBJ;
	if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
	{
		MaxAuxBus = AkSettings->MaxSimultaneousReverbVolumes;
	}

	//TUniquePtr
	CallbackManager = new FAkComponentCallbackManager();

	AkIntegrationBehavior::Get()->AkAudioDevice_LoadInitialData(this);

	SetCurrentAudioCulture(GetDefaultLanguage());

	UE_LOG(LogAkAudio, Log, TEXT("Initialization complete."));

	return CallbackManager != nullptr;
}

void FAkAudioDevice::AddDefaultListener(UAkComponent* in_pListener)
{
	bool bAlreadyInSet;
	m_defaultListeners.Add(in_pListener, &bAlreadyInSet);
	if (!bAlreadyInSet)
	{
		for (auto& Emitter : m_defaultEmitters)
			Emitter->OnDefaultListenerAdded(in_pListener);

		in_pListener->IsDefaultListener = true;
		UpdateDefaultActiveListeners();

		if (m_SpatialAudioListener == nullptr)
			SetSpatialAudioListener(in_pListener);
	}
}

void FAkAudioDevice::RemoveDefaultListener(UAkComponent* in_pListener)
{
	for (auto& Emitter : m_defaultEmitters)
	{
		Emitter->OnListenerUnregistered(in_pListener);
	}

	m_defaultListeners.Remove(in_pListener);
	in_pListener->IsDefaultListener = false;
	UpdateDefaultActiveListeners();

	// We are setting Aux Sends with the SpatialAudio API, and that requires a Spatial Audio listener.
	// When running dedicated server, Unreal creates a camera manager (default listener 1 gets set as spatial audio listener), then another one (default listener 2), and then destroys the first. This leaves us with a default listener, but no spatial audio listener. This fix targets that issue.
	if (m_SpatialAudioListener == in_pListener && m_defaultListeners.Num() > 0)
	{
		for (auto listener : m_defaultListeners)
		{
			if (SetSpatialAudioListener(m_defaultListeners.Array()[0]))
			{
				break;
			}
		}
	}
}

void FAkAudioDevice::OnActorSpawned(AActor* SpawnedActor)
{
	APlayerCameraManager* AsPlayerCameraManager = Cast<APlayerCameraManager>(SpawnedActor);
	if (AsPlayerCameraManager && AsPlayerCameraManager->GetWorld()->AllowAudioPlayback())
	{
		APlayerController* CameraOwner = Cast<APlayerController>(AsPlayerCameraManager->GetOwner());
		if (CameraOwner && CameraOwner->IsLocalPlayerController())
		{
			UAkComponent* pAkComponent = NewObject<UAkComponent>(SpawnedActor);
			if (pAkComponent != nullptr)
			{
				pAkComponent->RegisterComponentWithWorld(SpawnedActor->GetWorld());
				pAkComponent->AttachToComponent(SpawnedActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform, FName());
				AddDefaultListener(pAkComponent);
			}
		}
	}
}

FString FAkAudioDevice::GetBasePath()
{
	return FPaths::Combine(AkUnrealHelper::GetSoundBankDirectory(), FAkPlatform::GetPlatformBasePath());
}

/**
 * Allocates memory from permanent pool. This memory will NEVER be freed.
 *
 * @param	Size	Size of allocation.
 *
 * @return pointer to a chunk of memory with size Size
 */
void* FAkAudioDevice::AllocatePermanentMemory( int32 Size, bool& AllocatedInPool )
{
	return 0;
}

AKRESULT FAkAudioDevice::GetGameObjectID( AActor * in_pActor, AkGameObjectID& io_GameObject )
{
	if ( IsValid(in_pActor) )
	{
		UAkComponent * pComponent = GetAkComponent( in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset );
		if ( pComponent )
		{
			io_GameObject = pComponent->GetAkGameObjectID();
			return AK_Success;
		}
		else
			return AK_Fail;
	}

	// we do not modify io_GameObject, letting it to the specified default value.
	return AK_Success;
}

AKRESULT FAkAudioDevice::GetGameObjectID( AActor * in_pActor, AkGameObjectID& io_GameObject, bool in_bStopWhenOwnerDestroyed )
{
	if ( IsValid(in_pActor) )
	{
		UAkComponent * pComponent = GetAkComponent( in_pActor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset );
		if ( pComponent )
		{
			pComponent->StopWhenOwnerDestroyed = in_bStopWhenOwnerDestroyed;
			io_GameObject = pComponent->GetAkGameObjectID();
			return AK_Success;
		}
		else
			return AK_Fail;
	}

	// we do not modify io_GameObject, letting it to the specified default value.
	return AK_Success;
}

void FAkAudioDevice::Suspend(bool in_bRenderAnyway /* = false */)
{
	if (!m_isSuspended)
	{
		AK::SoundEngine::Suspend(in_bRenderAnyway);
		m_isSuspended = true;
	}
}

void FAkAudioDevice::WakeupFromSuspend()
{
	if (m_isSuspended)
	{
		AK::SoundEngine::WakeupFromSuspend();
		m_isSuspended = false;
	}
}

void FAkAudioDevice::StartOutputCapture(const FString& Filename)
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StartOutputCapture(TCHAR_TO_AK(*Filename));
	}
}

void FAkAudioDevice::StopOutputCapture()
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopOutputCapture();
	}
}

void FAkAudioDevice::StartProfilerCapture(const FString& Filename)
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StartProfilerCapture(TCHAR_TO_AK(*Filename));
	}
}

void FAkAudioDevice::AddOutputCaptureMarker(const FString& MarkerText)
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::AddOutputCaptureMarker(TCHAR_TO_ANSI(*MarkerText));
	}
}

void FAkAudioDevice::StopProfilerCapture()
{
	if ( m_bSoundEngineInitialized )
	{
		AK::SoundEngine::StopProfilerCapture();
	}
}

AKRESULT FAkAudioDevice::RegisterPluginDLL(const FString& in_DllName, const FString& in_DllPath)
{
	AkOSChar* szPath = nullptr;

	if (!in_DllPath.IsEmpty())
	{
		auto Length = in_DllPath.Len() + 1;
		szPath = new AkOSChar[Length];
		AKPLATFORM::SafeStrCpy(szPath, TCHAR_TO_AK(*in_DllPath), Length);
	}

	AKRESULT eResult = AK::SoundEngine::RegisterPluginDLL(TCHAR_TO_AK(*in_DllName), szPath);
	delete[] szPath;
	return eResult;
}
// end

bool FAkAudioDevice::SetCurrentAudioCultureAsyncTask::Start()
{
	bool switchLanguageSuccess = true;

	auto onNewLanguageLoadedLambda = [this](bool IsValid) {
		onNewLanguageLoaded(IsValid);
	};

	for (TObjectIterator<UAkAudioEvent> eventIt; eventIt; ++eventIt)
	{
		if (eventIt->IsLocalized())
		{
			if (eventIt->SwitchLanguage(NewWwiseLanguage, onNewLanguageLoadedLambda, this))
			{
				TasksRemaining.Increment();
			}
			else
			{
				switchLanguageSuccess = false;
				break;
			}
		}
	}

	if (switchLanguageSuccess)
	{
		for (TObjectIterator<UAkAudioBank> audioBankIt; audioBankIt; ++audioBankIt)
		{
			if (audioBankIt->IsLocalized())
			{
				if (audioBankIt->SwitchLanguage(NewWwiseLanguage, onNewLanguageLoadedLambda))
				{
					TasksRemaining.Increment();
				}
				else
				{
					switchLanguageSuccess = false;
					break;
				}
			}
		}
	}

	if (!switchLanguageSuccess)
	{
		UE_LOG(LogAkAudio, Error, TEXT("Cannot switch to Wwise language '%s'"), *NewWwiseLanguage);
	}

	return switchLanguageSuccess;
}

void FAkAudioDevice::SetCurrentAudioCultureAsyncTask::Update()
{
	if (TasksRemaining.GetValue() == 0)
	{
		if (Succeeded)
		{
			AK::StreamMgr::SetCurrentLanguage(TCHAR_TO_AK(*NewWwiseLanguage));

			if (GEngine)
			{
				GEngine->ForceGarbageCollection();
			}
		}

		IsDone = true;
	}

	if (IsDone)
	{
		CompletedCallback.ExecuteIfBound(Succeeded);
	}
}

void FAkAudioDevice::SetCurrentAudioCultureAsyncTask::onNewLanguageLoaded(bool IsValid)
{
	Succeeded = IsValid;
	TasksRemaining.Decrement();
}

void FAkAudioDevice::LoadDelayedAssets()
{
	for (auto* delayedAsset : AssetsWithDelayedLoad)
	{
		delayedAsset->Load();
	}
	AssetsWithDelayedLoad.Empty();
}

void FAkAudioDevice::LoadDelayedMedias()
{
	for (auto* delayedMedia : MediasWithDelayedLoad)
	{
		delayedMedia->Load();
	}
	MediasWithDelayedLoad.Empty();
}

bool FAkAudioDevice::IsPlayingIDActive(uint32 EventID, uint32 PlayingID)
{
	FScopeLock Lock(&EventToPlayingIDMapCriticalSection);
	auto* PlayingIDArray = EventToPlayingIDMap.Find(EventID);
	if (PlayingIDArray && PlayingIDArray->Contains(PlayingID))
	{
		return true;
	}

	return false;
}

bool FAkAudioDevice::IsEventIDActive(uint32 EventID)
{
	FScopeLock Lock(&EventToPlayingIDMapCriticalSection);
	return EventToPlayingIDMap.Contains(EventID);
}

void FAkAudioDevice::RemovePlayingID(uint32 EventID, uint32 PlayingID)
{
	FScopeLock Lock(&EventToPlayingIDMapCriticalSection);
	auto* PlayingIDArray = EventToPlayingIDMap.Find(EventID);
	if (PlayingIDArray)
	{
		PlayingIDArray->Remove(PlayingID);
		if (PlayingIDArray->Num() == 0)
		{
			EventToPlayingIDMap.Remove(EventID);
		}
	}
}

void FAkAudioDevice::StopEventID(uint32 EventID)
{
	FScopeLock Lock(&EventToPlayingIDMapCriticalSection);

	auto* PlayingIDs = EventToPlayingIDMap.Find(EventID);
	if (PlayingIDs)
	{
		for (auto pID : *PlayingIDs)
		{
			StopPlayingID(pID);
		}
		AK::SoundEngine::RenderAudio();
	}
}

void FAkAudioDevice::AddToPinnedEventsMap(uint32 PlayingID, UAkAudioEvent* EventToPin) 
{
	FScopeLock Lock(&PlayingIDToPinnedAudioEventMapCriticalSection);
	auto& EventSet = PlayingIDToPinnedAudioEventMap.FindOrAdd(PlayingID);
	EventSet.Add(EventToPin);
}

void FAkAudioDevice::AddToPinnedMediasMap(uint32 PlayingID, UAkExternalMediaAsset* MediaToPin)
{
	FScopeLock Lock(&PlayingIDToPinnedExternalSourceMapCriticalSection);
	auto& MediaSet = PlayingIDToPinnedExternalSourceMap.FindOrAdd(PlayingID);
	MediaSet.Add(MediaToPin);
}

void FAkAudioDevice::CleanPinnedObjects(uint32 PlayingID)
{
	{
		FScopeLock Lock(&PlayingIDToPinnedAudioEventMapCriticalSection);
		auto* EventSetPtr = PlayingIDToPinnedAudioEventMap.Find(PlayingID);
		if (EventSetPtr)
		{
			TSet<UAkAudioEvent*> EventSet = *EventSetPtr;
			for (auto AkEvent : EventSet)
			{
				AkEvent->UnpinFromGarbageCollector(PlayingID);

			}
			EventSet.Empty();
			PlayingIDToPinnedAudioEventMap.Remove(PlayingID);
		}
	}

	{
		FScopeLock Lock(&PlayingIDToPinnedExternalSourceMapCriticalSection);
		auto* MediaSetPtr = PlayingIDToPinnedExternalSourceMap.Find(PlayingID);
		if (MediaSetPtr)
		{
			TSet<UAkExternalMediaAsset*> MediaSet = *MediaSetPtr;
			for (auto Media : MediaSet)
			{
				Media->UnpinFromGarbageCollector(PlayingID);
			}
			MediaSet.Empty();
			PlayingIDToPinnedExternalSourceMap.Remove(PlayingID);
		}
	}
}

