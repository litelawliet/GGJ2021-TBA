#pragma once

#include "CreateAkAssetsVisitor.h"
#include "Containers/Map.h"
#include "Containers/Array.h"

class AssetMigrationVisitor : public CreateAkAssetsVisitor
{
	using Super = CreateAkAssetsVisitor;
public:
	void OnBeginParse() override;

	void EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void End() override;

protected:
	void collectExtraAssetsToDelete(TArray<FAssetData>& assetToDelete) override;

	template<typename AssetType>
	void migrateAssets(const FGuid& Id, const FString& Name, TMap<FString, TArray<UObject*>>& duplicatedAssets);

private:
	TMap<FString, TArray<UObject*>> duplicatedEvents;
	TMap<FString, TArray<UObject*>> duplicatedAuxBus;
	TMap<FString, TArray<UObject*>> duplicatedAcousticTextures;

	bool AkConsolidateObjects(UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate);

};