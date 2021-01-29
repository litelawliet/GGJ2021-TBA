#pragma once

#include "WwiseProject/WorkUnitXmlVisitor.h"

#include "Containers/Array.h"
#include "Misc/TextFilter.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "WaapiPicker/WwiseTreeItem.h"

using StringFilter = TTextFilter<const FString&>;

class WwisePickerBuilderVisitor : public WorkUnitXmlVisitor
{
public:
	WwisePickerBuilderVisitor();

	void Init(EWwiseItemType::Type Type) override;
	void ForceInit() override;

	void EnterWorkUnit(const FString& WorkUnitPath, const FString& RelativePath, EWwiseItemType::Type ItemType, bool IsStandalone, bool ForceRefresh) override;
	void ExitWorkUnit(bool IsStandalone) override;

	void EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	void ExitAuxBus() override;

	void EnterFolderOrBus(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType) override;
	void ExitFolderOrBus() override;

	void EnterStateGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	void ExitStateGroup() override;

	void EnterState(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterSwitchGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) override;
	void ExitSwitchGroup() override;

	void EnterSwitch(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterGameParameter(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void EnterTrigger(const FGuid& Id, const FString& Name, const FString& RelativePath) override;

	void ExitChildrenList() override;

	void RemoveWorkUnit(const FString& WorkUnitPath) override;

	TSharedPtr<FWwiseTreeItem> GetTree(TSharedPtr<StringFilter> SearchBoxFilter, TSharedPtr<FWwiseTreeItem> CurrentTreeRootItem, EWwiseItemType::Type ItemType);
	TSharedPtr<FWwiseTreeItem> CreatePhysicalFolderItems(const FString& RelativePhysicalPath, TSharedPtr<FWwiseTreeItem> RootItem);
	void CopyTree(TSharedPtr<FWwiseTreeItem> SourceTreeItem, TSharedPtr<FWwiseTreeItem> DestTreeItem);
	void FilterTree(TSharedPtr<FWwiseTreeItem> TreeItem, TSharedPtr<StringFilter> SearchBoxFilter);

private:
	void createChildTreeItem(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType, const FGuid& Id);
	void createParentTreeItem(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType, const FGuid& Id);
	void exitParent();

private:
	TMap<EWwiseItemType::Type, TMap<FString, TSharedPtr<FWwiseTreeItem>>> WwuSubTrees;
	TSharedPtr<FWwiseTreeItem> FullTreeRootItems[EWwiseItemType::LastWwiseDraggable - EWwiseItemType::Event + 1];
	FString OldFilterText;
	TMap<FString, TSharedPtr<FWwiseTreeItem>>* WwuTreesForType = nullptr;
	TArray<TWeakPtr<FWwiseTreeItem>> rootTreeItemStack;
	FString projectRootFolder;
};
