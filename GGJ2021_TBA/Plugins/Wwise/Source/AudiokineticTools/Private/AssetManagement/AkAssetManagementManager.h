#pragma once

#include "DirectoryWatcher/Public/IDirectoryWatcher.h"
#include "WaapiAssetSynchronizer.h"
#include "WwiseProject/WwiseWorkUnitParser.h"
#include "ContentBrowser/Public/ContentBrowserDelegates.h"

class AkAssetManagementManager
{
public:
	void Init();
	void Uninit();

	void DoAssetSynchronization();
	void DoAssetMigration();

	bool IsInited() const { return isInited; }

	static void ClearSoundBanksForMigration();
	static void ModifyProjectSettings();

private:
	void onWwiseDirectoryChanged(const TArray<FFileChangeData>& ChangedFiles);
	bool onAssetViewDrop(const FAssetViewDragAndDropExtender::FPayload& Payload);
	bool onAssetViewDragOver(const FAssetViewDragAndDropExtender::FPayload& Payload);
	bool onAssetViewDragLeave(const FAssetViewDragAndDropExtender::FPayload& Payload);

private:
	bool isInited = false;

	WaapiAssetSynchronizer waapiAssetSync;
	WwiseWorkUnitParser workUnitParser;

	FString wwiseProjectFolder;
	FDelegateHandle wwiseDirectoryChangedHandle;

	TUniquePtr<FAssetViewDragAndDropExtender> dragDropExtender;
	bool canDrop = false;
};