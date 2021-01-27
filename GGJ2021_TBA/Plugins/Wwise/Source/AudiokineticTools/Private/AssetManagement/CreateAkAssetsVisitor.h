#pragma once

#include "WwiseProject/WorkUnitXmlVisitor.h"

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Misc/Guid.h"

struct FAssetData;
class UPackage;

class CreateAkAssetsVisitor : public WorkUnitXmlVisitor
{
public:
	void OnBeginParse() override;

	void EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	
	void EnterStateGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	void EnterState(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterSwitchGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	void EnterSwitch(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterGameParameter(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterTrigger(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	
	void End() override;

	void SetDoSave(bool value) { doSave = false; }

protected:
	virtual void collectExtraAssetsToDelete(TArray<FAssetData>& assetToDelete) {}

private:
	template<typename AssetType>
	void createAsset(const FGuid& Id, const FString& Name, const FString& AssetName, const FString& RelativePath, const FGuid& GroupId = FGuid());

protected:
	TArray<UPackage*> packagesToSave;

private:
	FString currentFolderPath;
	TSet<FGuid> foundAssets;

	FString currentStateGroupName;
	FGuid currentStateGroupId;

	FString currentSwitchGroupName;
	FGuid currentSwitchGroupId;
	bool doSave = true;
};

