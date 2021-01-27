#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "UObject/SoftObjectPtr.h"
#include "AkMediaAsset.h"

class FAssetRegistryModule;
class FAssetToolsModule;
class UAkAcousticTexture;
class UAkAudioBank;
class UAkAudioEvent;
class UAkAudioType;
class UAkAuxBus;
class UAkGroupValue;
class UAkInitBank;
class UAkMediaAsset;
class UAkRtpc;
class UAkSettings;
class UAkStateValue;
class UAkSwitchValue;
class UAkTrigger;

struct FAssetData;
struct FAssetRenameData;

template<typename AkAssetType>
struct AkAssetTraits
{
	static const FString Name() { return ""; }
	static const FString BaseFolder() { return ""; };
};

template<>
struct AkAssetTraits<UAkAcousticTexture>
{
	static const FString Name() { return "AcousticTexture"; }
	static const FString BaseFolder() { return "Virtual_Acoustics"; }
};

template<>
struct AkAssetTraits<UAkAudioEvent>
{
	static const FString Name() { return "Event"; }
	static const FString BaseFolder() { return "Events"; }
};

template<>
struct AkAssetTraits<UAkAuxBus>
{
	static const FString Name() { return "AuxBus"; }
	static const FString BaseFolder() { return "Master-Mixer_Hierarchy"; }
};

template<>
struct AkAssetTraits<UAkRtpc>
{
	static const FString Name() { return "GameParameter"; }
	static const FString BaseFolder() { return "Game_Parameters"; }
};

template<>
struct AkAssetTraits<UAkStateValue>
{
	static const FString Name() { return "State"; }
	static const FString BaseFolder() { return "States"; }
};

template<>
struct AkAssetTraits<UAkSwitchValue>
{
	static const FString Name() { return "Switch"; }
	static const FString BaseFolder() { return "Switches"; }
};

template<>
struct AkAssetTraits<UAkTrigger>
{
	static const FString Name() { return "Trigger"; }
	static const FString BaseFolder() { return "Triggers"; }
};

class AkAssetDatabase
{
public:
	static AkAssetDatabase& Get();

	void Init();
	void Clear();

	static FString GetWwisePathFromAssetPath(const FString& AssetPath);
	static FString GetBaseFolderForAssetType(const UClass* Klass);

	bool Add(const FGuid& Id, UAkAudioType* AudioType);
	bool Remove(UAkAudioType* AudioType);

	UObject* CreateOrRenameAsset(const UClass* Klass, const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FGuid& GroupId);
	
	void RenameAsset(const UClass* Klass, const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FString& GroupId);

	void RenameGroupValues(const FGuid& GroupId, const FString& GroupName, const FString& Path);
	void DeleteAsset(const FGuid& Id);
	void DeleteAssets(const TSet<FGuid>& AssetsId);

	void FillAssetsToDelete(UAkAudioType* Asset, TArray<FAssetData>& AssetDataToDelete);

	void MoveAllAssets(const FString& OldBaseAssetPath, const FString& NewBaseAssetPath);

	void MoveWorkUnit(const FString& OldWorkUnitPath, const FString& NewWorkUnitPath);

	void CreateInitBankIfNeeded();

	void FixUpRedirectors(const FString& AssetPackagePath);

	enum class CanBeDroppedSource
	{
		FromPicker,
		FromContentBrowser
	};

	bool CanBeDropped(const FAssetData& AssetData, FName PackagePath, CanBeDroppedSource Source) const;

	bool IsInited() const { return InitBank != nullptr; }

public:
	template<typename AkAssetType>
	struct AkTypeMap
	{
		mutable FCriticalSection CriticalSection;
		TMap<FGuid, AkAssetType*> TypeMap;

		AkAssetType* const* Find(const FGuid& Key) const
		{
			FScopeLock autoLock(&CriticalSection);
			return TypeMap.Find(Key);
		}

		TArray<AkAssetType*> FindByName(const FString& Name) const
		{
			FScopeLock autoLock(&CriticalSection);

			TArray<AkAssetType*> FoundAssets;
			for (auto& entry : TypeMap)
			{
				if (entry.Value && entry.Value->GetName().Compare(Name) == 0)
				{
					FoundAssets.Add(entry.Value);
				}
			}
			return FoundAssets;
		}

		void Add(const FGuid& Key, AkAssetType* AkAsset)
		{
			if (!AkAsset)
				return;

			FScopeLock autoLock(&CriticalSection);

			if (!TypeMap.Contains(Key))
			{
				TypeMap.Add(Key, AkAsset);
			}
		}

		int32 Remove(const FGuid& Key)
		{
			FScopeLock autoLock(&CriticalSection);
			return TypeMap.Remove(Key);
		}

		int32 RemoveByName(const FString& Name)
		{
			int32 result = 0;
			FScopeLock autoLock(&CriticalSection);

			TSet<FGuid> guidToRemove;
			for (auto& entry : TypeMap)
			{
				if (entry.Value && entry.Value->GetName().Compare(Name) == 0)
				{
					guidToRemove.Add(entry.Key);
				}
			}

			for (auto& guid : guidToRemove)
			{
				result += TypeMap.Remove(guid);
			}

			return result;
		}

		void Empty()
		{
			FScopeLock autoLock(&CriticalSection);
			TypeMap.Empty();
		}
	};

	AkTypeMap<UAkAcousticTexture> AcousticTextureMap;
	AkTypeMap<UAkAudioEvent> EventMap;
	AkTypeMap<UAkAuxBus> AuxBusMap;
	AkTypeMap<UAkAudioBank> BankMap;
	AkTypeMap<UAkGroupValue> GroupValueMap;
	AkTypeMap<UAkTrigger> TriggerMap;
	AkTypeMap<UAkRtpc> RtpcMap;

	AkTypeMap<UAkAudioType> AudioTypeMap;
	TSet<FGuid> PendingAssetCreates;
	mutable FCriticalSection InitBankLock;
	UAkInitBank* InitBank = nullptr;

private:
	AkAssetDatabase();

	bool IsAkAudioType(const FAssetData& AssetData);

	void onAssetAdded(const FAssetData& NewAssetData);
	void onAssetRemoved(const FAssetData& RemovedAssetData);
	void onAssetRenamed(const FAssetData& NewAssetData, const FString& OldPath);
	void processMediaToDelete(UObject* Asset, const TArray<TSoftObjectPtr<UAkMediaAsset>>& MediaList, TArray<FAssetData>& AssetDataToDelete);
	void renameLocalizedAssets(const UAkAudioEvent* akAudioEvent, const FString& parentPath, const FString& AssetName, TArray<FAssetRenameData>& assetsToRename);
	void removeEmptyFolders(const TArray<FAssetRenameData>& FoldersToCheck);
	
private:
	FAssetRegistryModule* AssetRegistryModule = nullptr;
	FAssetToolsModule* AssetToolsModule = nullptr;

	TSet<FGuid> ignoreRenames;

	TArray<FAssetData> CopiedAssetsToUndo;
	FTickerDelegate OnTick;
	FDelegateHandle TickDelegateHandle;
	bool Tick(float DeltaTime);

};