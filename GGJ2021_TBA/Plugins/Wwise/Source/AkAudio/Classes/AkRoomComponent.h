// Copyright Audiokinetic 2015

#pragma once
#include "AkAudioDevice.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Volume.h"
#include "AkGameObject.h"
#include "AkRoomComponent.generated.h"


UCLASS(ClassGroup = Audiokinetic, BlueprintType, hidecategories = (Transform, Rendering, Mobility, LOD, Component, Activation, Tags), meta = (BlueprintSpawnableComponent))
class AKAUDIO_API UAkRoomComponent : public UAkGameObject
{
	GENERATED_UCLASS_BODY()

	/** Enable room occlusion feature, additional properties are available in the Room category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Toggle, meta = (DisplayName = "Enable Room"))
	uint32 bEnable:1;

	/** We keep a  linked list of ReverbVolumes sorted by priority for faster finding of reverb volumes at a specific location.
	 *	This points to the next volume in the list.
	 */
	UPROPERTY(transient)
	class UAkRoomComponent* NextLowerPriorityComponent;

	/**
	* The precedence in which the Rooms will be applied. In the case of overlapping rooms, only the one
	* with the highest priority is chosen. If two or more overlapping rooms have the same
	* priority, the chosen room is unpredictable.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float Priority;

	/**
	* Used to set the occlusion value in wwise, on emitters in the room, when no audio paths to the listener are found via sound propagation in Wwise Spatial Audio.
	* This value can be thought of as 'thickness', as it relates to how much sound energy is transmitted through the wall. 
	* Valid range 0.0f-1.0f, and is mapped to the occlusion curve as defined in the Wwise project.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room", meta = (ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f))
	float WallOcclusion;

	/**
	* Send level for sounds that are posted on the room. Valid range: (0.f-1.f). A value of 0 disables the aux send.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AkEvent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AuxSendLevel;

	/** Automatically post the associated AkAudioEvent on BeginPlay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AkEvent", SimpleDisplay)
	bool AutoPost;

	/** Posts this game object's AkAudioEvent to Wwise, using this as the game object source */
	virtual int32 PostAssociatedAkEvent(
		UPARAM(meta = (Bitmask, BitmaskEnum = EAkCallbackType)) int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback,
		const TArray<FAkExternalSourceInfo>& ExternalSources
	);

	/** Register a room in AK Spatial Audio. */
	void AddSpatialAudioRoom();

	/** Modify a room in AK Spatial Audio. */
	void UpdateSpatialAudioRoom();

	/** Remove a room from AK Spatial Audio	*/
	void RemoveSpatialAudioRoom();

	bool HasEffectOnLocation(const FVector& Location) const;

	bool RoomIsActive() const { return ParentVolume && bEnable && !IsRunningCommandlet(); }

	AkRoomID GetRoomID() const { return AkRoomID(this); }

	virtual void PostLoad() override;
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;

	FName GetName() const;

	virtual AkPlayingID PostAkEventByNameWithDelegate(
		const FString& in_EventName,
		int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback,
		const TArray<FAkExternalSourceInfo>& ExternalSources = TArray<FAkExternalSourceInfo>());

	// Begin USceneComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	class AVolume* ParentVolume;

	void InitializeParentVolume();

	void GetRoomParams(AkRoomParams& outParams);
};
