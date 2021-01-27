// Copyright Audiokinetic 2015

#pragma once
#include "Platforms/AkUEPlatform.h"
#include "AkAcousticTexture.h"
#include "Components/SceneComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AkGeometryComponent.generated.h"

class UAkSettings;

DECLARE_DELEGATE(FOnMeshTypeChanged);

UENUM()
enum class AkMeshType : uint8
{
	StaticMesh,
	CollisionMesh UMETA(DisplayName = "Simple Collision")
};

USTRUCT()
struct FAkAcousticSurface
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 Texture;
	
	UPROPERTY()
	float Occlusion;
	
	UPROPERTY()
	FString Name;
};

USTRUCT()
struct FAkTriangle
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint16 Point0;

	UPROPERTY()
	uint16 Point1;
	
	UPROPERTY()
	uint16 Point2;

	UPROPERTY()
	uint16 Surface;
};

USTRUCT()
struct FAkGeometryData
{
	GENERATED_USTRUCT_BODY()

	void Clear()
	{
		Vertices.Empty();
		Surfaces.Empty();
		Triangles.Empty();
		ToOverrideAcousticTexture.Empty();
		ToOverrideOcclusion.Empty();
	}

	UPROPERTY()
	TArray<FVector> Vertices;
	
	UPROPERTY()
	TArray<FAkAcousticSurface> Surfaces;

	UPROPERTY()
	TArray<FAkTriangle> Triangles;

	UPROPERTY()
	TArray<UPhysicalMaterial*> ToOverrideAcousticTexture;

	UPROPERTY()
	TArray<UPhysicalMaterial*> ToOverrideOcclusion;
};

USTRUCT(BlueprintType)
struct FAkGeometrySurfaceOverride
{
	GENERATED_USTRUCT_BODY()

	/** The Acoustic Texture represents the sound absorption on the surface of the geometry when a sound bounces off of it.
	* If left to None, the mesh's physical material will be used to fetch an acoustic texture.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	UAkAcousticTexture* AcousticTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	uint32 bEnableOcclusionOverride : 1;

	/** Occlusion value to set when modeling sound transmission through geometry. Transmission is modeled only when there is no direct line of sight from the emitter to the listener.
	* If more that one surface is between the emitter and the listener, than the maximum of each surface's occlusion value is used. If the emitter and listener are in different rooms, the room's WallOcclusion is taken into account.
	* Valid range : (0.0, 1.0)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (EditCondition = "bEnableOcclusionOverride", ClampMin = "0.0", ClampMax = "1.0"))
	float OcclusionValue = 1.f;

	FAkGeometrySurfaceOverride()
	{
		AcousticTexture = nullptr;
		bEnableOcclusionOverride = false;
		OcclusionValue = 1.f;
	}
};

UCLASS(ClassGroup = Audiokinetic, BlueprintType, hidecategories = (Transform, Rendering, Mobility, LOD, Component, Activation, Tags), meta = (BlueprintSpawnableComponent))
class AKAUDIO_API UAkGeometryComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkGeometry")
	void ConvertMesh();

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkGeometry")
	void RemoveGeometry();

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkGeometry")
	void UpdateGeometry();

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Geometry")
	AkMeshType MeshType;

	/** The Static Mesh's LOD to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.0"))
	int LOD;

	/** The local distance in Unreal units between two vertices to be welded together.
	* Any two vertices closer than this threshold will be treated as the same unique vertex and assigned the same position.
	* Increasing this threshold decreases the number of gaps between triangles, resulting in a more continuous mesh and less sound leaking though, as well as eliminating triangles that are too small to be significant.
	* Increasing this threshold also helps Spatial Audio's edge-finding algorithm to find more valid diffraction edges.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.0"))
	float WeldingThreshold;

	/** Override the acoustic properties of this mesh per material.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", DisplayName = "Acoustic Properties Override")
	TMap<UMaterialInterface*, FAkGeometrySurfaceOverride> StaticMeshSurfaceOverride;

	/** Override the acoustic properties of the collision mesh.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", DisplayName = "Acoustic Properties Override")
	FAkGeometrySurfaceOverride CollisionMeshSurfaceOverride;

	/** Enable or disable geometric diffraction for this mesh. Check this box to have Wwise Spatial Audio generate diffraction edges on the geometry. The diffraction edges will be visible in the Wwise game object viewer when connected to the game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Diffraction")
	uint32 bEnableDiffraction : 1;

	/** Enable or disable geometric diffraction on boundary edges for this Geometry. Boundary edges are edges that are connected to only one triangle. Depending on the specific shape of the geometry, boundary edges may or may not be useful and it is beneficial to reduce the total number of diffraction edges to process.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Diffraction", meta = (EditCondition = "bEnableDiffraction"))
	uint32 bEnableDiffractionOnBoundaryEdges : 1;

	/** (Optional) Associate this Surface Reflector Set with a Room.
	* Associating a spatial audio geometry with a particular room will limit the scope in which the geometry is visible/accessible. Leave it to None and this geometry will have a global scope.
	* It is recommended to associate geometry with a room when the geometry is (1) fully contained within the room (ie. not visible to other rooms except by portals), and (2) the room does not share geometry with other rooms. Doing so reduces the search space for ray casting performed by reflection and diffraction calculations.
	* Take note that once one or more geometry sets are associated with a room, that room will no longer be able to access geometry that is in the global scope.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Optimization")
	AActor* AssociatedRoom;

#if WITH_EDITORONLY_DATA
	void SetOnMeshTypeChanged(const FOnMeshTypeChanged& OnMeshTypeChangedDelegate) { OnMeshTypeChanged = OnMeshTypeChangedDelegate; }
	void ClearOnMeshTypeChanged() { OnMeshTypeChanged.Unbind(); }
	const FOnMeshTypeChanged* GetOnMeshTypeChanged() { return &OnMeshTypeChanged; }
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
#endif

	virtual void OnRegister() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual bool MoveComponentImpl
	(
		const FVector & Delta,
		const FQuat & NewRotation,
		bool bSweep,
		FHitResult * Hit,
		EMoveComponentFlags MoveFlags,
		ETeleportType Teleport
	) override;

private:
	UStaticMeshComponent* Parent = nullptr;
	void InitializeParent();

	inline bool ShouldSendGeometry() const;
	void SendGeometry();

	void ConvertStaticMesh(UStaticMeshComponent* StaticMeshComponent, const UAkSettings* AkSettings);
	void ConvertCollisionMesh(UStaticMeshComponent* StaticMeshComponent, const UAkSettings* AkSettings);
	void UpdateStaticMeshOverride(UStaticMeshComponent* StaticMeshComponent);
	void UpdateGeometryTransform();

	bool GeometryHasBeenSent = false;

	UPROPERTY()
	FAkGeometryData GeometryData;

	TMap<UMaterialInterface*, FAkGeometrySurfaceOverride> PreviousStaticMeshSurfaceOverride;

#if WITH_EDITORONLY_DATA
	FOnMeshTypeChanged OnMeshTypeChanged;
	FDelegateHandle OnMeshTypeChangedHandle;
#endif
};
