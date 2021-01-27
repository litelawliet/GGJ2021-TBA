#pragma once

#include "Containers/IndirectArray.h"
#include "Serialization/BulkData.h"
#include "UObject/Object.h"

#include "AkMediaAsset.generated.h"

struct AKAUDIO_API FAkMediaDataChunk
{
	FAkMediaDataChunk();

#if WITH_EDITOR
	FAkMediaDataChunk(IFileHandle* FileHandle, int64 BytesToRead, uint32 BulkDataFlags, FCriticalSection* DataWriteLock, bool IsPrefetch = false);
#endif

	FByteBulkData Data;
	bool IsPrefetch = false;

	void Serialize(FArchive& Ar, UObject* Owner);
};

UCLASS()
class AKAUDIO_API UAkMediaAssetData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkMediaAsset")
	bool IsStreamed = false;

	UPROPERTY(VisibleAnywhere, Category = "AkMediaAsset")
	bool UseDeviceMemory = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int64 LastWriteTime = 0;

	UPROPERTY(VisibleAnywhere, Category = "AkMediaAsset")
	uint64 MediaContentHash = 0;
#endif

	TIndirectArray<FAkMediaDataChunk> DataChunks;

#if WITH_EDITOR
	FCriticalSection DataLock;
#endif

public:
	void Serialize(FArchive& Ar) override;
};

UCLASS()
class AKAUDIO_API UAkMediaAsset : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkMediaAsset")
	uint32 Id;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "AkMediaAsset")
	TMap<FString, UAkMediaAssetData*> MediaAssetDataPerPlatform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "AkMediaAsset")
	FString MediaName;
#endif

	UPROPERTY(EditAnywhere, Category = "AkMediaAsset")
	TArray<UObject*> UserData;

private:
	UPROPERTY()
	UAkMediaAssetData* CurrentMediaAssetData;

public:
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;
	void FinishDestroy() override;

	void Load();
	void Unload();

#if WITH_EDITOR
	UAkMediaAssetData* FindOrAddMediaAssetData(const FString& platform);

	virtual void Reset();

	FCriticalSection BulkDataWriteLock;
#endif

	FAkMediaDataChunk const* GetStreamedChunk() const;

	template<typename T>
	T* GetUserData()
	{
		for (auto entry : UserData)
		{
			if (entry && entry->GetClass()->IsChildOf(T::StaticClass()))
			{
				return entry;
			}
		}

		return nullptr;
	}

protected:
	void loadMedia();
	void unloadMedia();

	UAkMediaAssetData* getMediaAssetData() const;

	void* RawMediaData = nullptr;
	bool DynamicallyAllocatedRawMediaData = false;
private:
#if WITH_EDITOR
	FCriticalSection platformLock;
	TArray<uint8> EditorMediaData;
#endif

#if AK_SUPPORT_DEVICE_MEMORY
	uint8* MediaDataDeviceMemory = nullptr;
#endif
};

UCLASS()
class AKAUDIO_API UAkLocalizedMediaAsset : public UAkMediaAsset
{
	GENERATED_BODY()
};

UCLASS()
class AKAUDIO_API UAkExternalMediaAsset : public UAkMediaAsset
{
	GENERATED_BODY()

public:
	TTuple<void*, int64> GetExternalSourceData();
	FThreadSafeCounter NumStreamingHandles;

	void AddPlayingID(uint32 EventID, uint32 PlayingID);
	bool HasActivePlayingIDs();
	TMap<uint32, TArray<uint32>> ActiveEventToPlayingIDMap;

	bool IsReadyForFinishDestroy() override;
	void BeginDestroy() override;

	void PinInGarbageCollector(uint32 PlayingID);
	void UnpinFromGarbageCollector(uint32 PlayingID);

private:
	FThreadSafeCounter TimesPinnedToGC;
};

