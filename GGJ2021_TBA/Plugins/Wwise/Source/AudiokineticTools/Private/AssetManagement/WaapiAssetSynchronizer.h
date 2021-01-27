#pragma once

#include "AkWaapiClient.h"

struct FAssetData;

class WaapiAssetSynchronizer
{
public:
	void Init();
	void Uninit();

	static bool PauseAssetRegistryDelegates;

private:
	void subscribeWaapiCallbacks();
	void unsubscribeWaapiCallbacks();

	void onAssetRemoved(const FAssetData& RemovedAssetData);
	void onAssetRenamed(const FAssetData& NewAssetData, const FString& OldPath);

	void onRenamed(uint64_t Id, TSharedPtr<FJsonObject> Response);
	void onPreDeleted(uint64_t Id, TSharedPtr<FJsonObject> Response);
	void onChildAdded(uint64_t Id, TSharedPtr<FJsonObject> Response);
	void onChildRemoved(uint64_t Id, TSharedPtr<FJsonObject> Response);

	void deleteTick(float DeltaSeconds);

	static UClass* GetClassByName(const FString& stringAssetType);

private:
	FDelegateHandle projectLoadedHandle;
	FDelegateHandle connectionLostHandle;
	FDelegateHandle clientBeginDestroyHandle;

	FDelegateHandle assetAddedHandle;
	FDelegateHandle assetRemovedHandle;
	FDelegateHandle assetRenamedHandle;

	uint64 idRenamed = 0;
	uint64 idPreDeleted = 0;
	uint64 idChildAdded = 0;
	uint64 idChildRemoved = 0;

	TSet<FGuid> ignoreRenames;

	FDelegateHandle postEditorTickHandle;
	TSet<FGuid> assetsToDelete;
	float deleteTimer = 0.f;

	TMap<FGuid, FString> workUnitsToMove;
};
