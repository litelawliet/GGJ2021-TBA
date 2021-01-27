#include "WwisePickerBuilderVisitor.h"

#include "AkUnrealHelper.h"

WwisePickerBuilderVisitor::WwisePickerBuilderVisitor()
{
	projectRootFolder = FPaths::GetPath(AkUnrealHelper::GetWwiseProjectPath()) + TEXT("/");
}

void WwisePickerBuilderVisitor::Init(EWwiseItemType::Type ItemType)
{
	FullTreeRootItems[ItemType] = MakeShared<FWwiseTreeItem>(EWwiseItemType::ItemNames[ItemType], EWwiseItemType::ItemNames[ItemType], nullptr, EWwiseItemType::PhysicalFolder, FGuid());

	WwuTreesForType = WwuSubTrees.Find(ItemType);
}

void WwisePickerBuilderVisitor::ForceInit()
{
	WwuSubTrees.Empty();
}

void WwisePickerBuilderVisitor::EnterWorkUnit(const FString& WorkUnitPath, const FString& RelativePath, EWwiseItemType::Type ItemType, bool IsStandaloneWorkUnit, bool ForceRefresh)
{
	TSharedPtr<FWwiseTreeItem> RootWwuItem;
	FString workUnitFileName = FPaths::GetBaseFilename(WorkUnitPath);
	if (WwuSubTrees.Contains(ItemType) && WwuSubTrees[ItemType].Contains(WorkUnitPath))
	{
		if (!ForceRefresh)
		{
			rootTreeItemStack.Push(WwuSubTrees[ItemType][WorkUnitPath]);
			return;
		}

		RootWwuItem = WwuSubTrees[ItemType][WorkUnitPath];
		RootWwuItem->Children.Empty();
	}
	else
	{
		auto workUnitType = IsStandaloneWorkUnit ? EWwiseItemType::StandaloneWorkUnit : EWwiseItemType::NestedWorkUnit;
		RootWwuItem = MakeShared<FWwiseTreeItem>(workUnitFileName, RelativePath, nullptr, workUnitType, FGuid());
		WwuSubTrees.FindOrAdd(ItemType).Add(WorkUnitPath, RootWwuItem);
	}

	rootTreeItemStack.Push(RootWwuItem);
}

void WwisePickerBuilderVisitor::ExitWorkUnit(bool IsStandalone)
{
	if (rootTreeItemStack.Num() > 0)
	{
		auto recentItem = rootTreeItemStack.Pop();

		if (!IsStandalone && rootTreeItemStack.Num() > 0)
		{
			rootTreeItemStack.Top().Pin()->Children.Add(recentItem.Pin());
		}
	}
}

void WwisePickerBuilderVisitor::EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::Type::Event, Id);
}

void WwisePickerBuilderVisitor::EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::AcousticTexture, Id);
}

void WwisePickerBuilderVisitor::EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createParentTreeItem(Name, RelativePath, EWwiseItemType::AuxBus, Id);
}

void WwisePickerBuilderVisitor::ExitAuxBus()
{
	exitParent();
}

void WwisePickerBuilderVisitor::EnterFolderOrBus(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType)
{
	createParentTreeItem(Name, RelativePath, ItemType, FGuid());
}

void WwisePickerBuilderVisitor::ExitFolderOrBus()
{
	exitParent();
}

void WwisePickerBuilderVisitor::EnterStateGroup(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createParentTreeItem(Name, RelativePath, EWwiseItemType::StateGroup, Id);
}

void WwisePickerBuilderVisitor::ExitStateGroup()
{
	exitParent();
}

void WwisePickerBuilderVisitor::EnterState(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::State, Id);
}

void WwisePickerBuilderVisitor::EnterSwitchGroup(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createParentTreeItem(Name, RelativePath, EWwiseItemType::SwitchGroup, Id);
}

void WwisePickerBuilderVisitor::ExitSwitchGroup()
{
	exitParent();
}

void WwisePickerBuilderVisitor::EnterSwitch(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::Switch, Id);
}

void WwisePickerBuilderVisitor::EnterGameParameter(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::GameParameter, Id);
}

void WwisePickerBuilderVisitor::EnterTrigger(const FGuid& Id, const FString& Name, const FString& RelativePath)
{
	createChildTreeItem(Name, RelativePath, EWwiseItemType::Trigger, Id);
}

void WwisePickerBuilderVisitor::ExitChildrenList()
{
	if (rootTreeItemStack.Num() > 0)
	{
		rootTreeItemStack.Top().Pin()->SortChildren();
	}
}

void WwisePickerBuilderVisitor::RemoveWorkUnit(const FString& WorkUnitPath)
{
	if (WwuTreesForType)
	{
		WwuTreesForType->Remove(WorkUnitPath);
	}
}

TSharedPtr<FWwiseTreeItem> WwisePickerBuilderVisitor::GetTree(TSharedPtr<StringFilter> SearchBoxFilter, TSharedPtr<FWwiseTreeItem> CurrentTreeRootItem, EWwiseItemType::Type ItemType)
{
	if (!FullTreeRootItems[ItemType])
	{
		return FullTreeRootItems[ItemType];
	}

	FullTreeRootItems[ItemType]->Children.Empty();
	TMap<FString, TSharedPtr<FWwiseTreeItem>>& WwuSubTreesForType = WwuSubTrees.FindOrAdd(ItemType);
	for (TMap<FString, TSharedPtr<FWwiseTreeItem>>::TConstIterator iter = WwuSubTreesForType.CreateConstIterator(); iter; ++iter)
	{
		TSharedPtr<FWwiseTreeItem> CurrentTreeItem = iter->Value;

		if (CurrentTreeItem->ItemType == EWwiseItemType::StandaloneWorkUnit)
		{
			FString WwuRelativePhysicalPath = iter->Key;
			WwuRelativePhysicalPath.RemoveFromStart(projectRootFolder);

			TSharedPtr<FWwiseTreeItem> Parent = CreatePhysicalFolderItems(WwuRelativePhysicalPath, FullTreeRootItems[ItemType]);
			Parent->Children.Add(CurrentTreeItem);
			CurrentTreeItem->Parent = Parent;
		}
	}
	FullTreeRootItems[ItemType]->SortChildren();

	FString CurrentFilterText = SearchBoxFilter->GetRawFilterText().ToString();
	if (!CurrentFilterText.IsEmpty() && CurrentTreeRootItem.IsValid())
	{
		TSharedPtr<FWwiseTreeItem> FilteredTreeRootItem = MakeShared<FWwiseTreeItem>(EWwiseItemType::ItemNames[ItemType], EWwiseItemType::ItemNames[ItemType], nullptr, EWwiseItemType::PhysicalFolder, FGuid());

		if (!OldFilterText.IsEmpty() && CurrentFilterText.StartsWith(OldFilterText))
		{
			CopyTree(CurrentTreeRootItem, FilteredTreeRootItem);
		}
		else
		{
			CopyTree(FullTreeRootItems[ItemType], FilteredTreeRootItem);
		}

		FilterTree(FilteredTreeRootItem, SearchBoxFilter);
		OldFilterText = CurrentFilterText;
		return FilteredTreeRootItem;
	}

	return FullTreeRootItems[ItemType];
}

void WwisePickerBuilderVisitor::CopyTree(TSharedPtr<FWwiseTreeItem> SourceTreeItem, TSharedPtr<FWwiseTreeItem> DestTreeItem)
{
	for (int32 i = 0; i < SourceTreeItem->Children.Num(); i++)
	{
		TSharedPtr<FWwiseTreeItem> CurrItem = SourceTreeItem->Children[i];
		TSharedPtr<FWwiseTreeItem> NewItem = MakeShared<FWwiseTreeItem>(CurrItem->DisplayName, CurrItem->FolderPath, CurrItem->Parent.Pin(), CurrItem->ItemType, CurrItem->ItemId);
		DestTreeItem->Children.Add(NewItem);

		CopyTree(CurrItem, NewItem);
	}
}

void WwisePickerBuilderVisitor::FilterTree(TSharedPtr<FWwiseTreeItem> TreeItem, TSharedPtr<StringFilter> SearchBoxFilter)
{
	TArray<TSharedPtr<FWwiseTreeItem>> ItemsToRemove;
	for (int32 i = 0; i < TreeItem->Children.Num(); i++)
	{
		TSharedPtr<FWwiseTreeItem> CurrItem = TreeItem->Children[i];
		FilterTree(CurrItem, SearchBoxFilter);

		if (!SearchBoxFilter->PassesFilter(CurrItem->DisplayName) && CurrItem->Children.Num() == 0)
		{
			ItemsToRemove.Add(CurrItem);
		}
	}

	for (int32 i = 0; i < ItemsToRemove.Num(); i++)
	{
		TreeItem->Children.Remove(ItemsToRemove[i]);
	}
}

TSharedPtr<FWwiseTreeItem> WwisePickerBuilderVisitor::CreatePhysicalFolderItems(const FString& RelativePhysicalPath, TSharedPtr<FWwiseTreeItem> RootItem)
{
	TArray<FString> SplitPath;
	RelativePhysicalPath.ParseIntoArray(SplitPath, TEXT("/"), true);
	TSharedPtr<FWwiseTreeItem> CurrentItem = RootItem;
	for (int32 i = 1; i < SplitPath.Num() - 1; i++) // Start at 1 because item 0 is "Events" (already created), and end at Num - 1 because last item is the work unit filename
	{
		TSharedPtr<FWwiseTreeItem> ChildItem = CurrentItem->GetChild(SplitPath[i]);
		if (!ChildItem.IsValid())
		{
			FString ItemPath = FPaths::Combine(*CurrentItem->FolderPath, *SplitPath[i]);
			ChildItem = MakeShared<FWwiseTreeItem>(SplitPath[i], ItemPath, CurrentItem, EWwiseItemType::PhysicalFolder, FGuid());
			CurrentItem->Children.Add(ChildItem);
		}

		CurrentItem->SortChildren();
		CurrentItem = ChildItem;
	}

	return CurrentItem;
}

void WwisePickerBuilderVisitor::createChildTreeItem(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType, const FGuid& Id)
{
	if (rootTreeItemStack.Num() > 0)
	{
		rootTreeItemStack.Top().Pin()->Children.Add(MakeShared<FWwiseTreeItem>(Name, RelativePath, rootTreeItemStack.Top().Pin(), ItemType, Id));
	}
}

void WwisePickerBuilderVisitor::createParentTreeItem(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType, const FGuid& Id)
{
	TSharedPtr<FWwiseTreeItem> folderItem = MakeShared<FWwiseTreeItem>(Name, RelativePath, rootTreeItemStack.Top().Pin(), ItemType, Id);

	if (rootTreeItemStack.Num() > 0)
	{
		rootTreeItemStack.Top().Pin()->Children.Add(folderItem);
	}

	rootTreeItemStack.Push(folderItem);
}

void WwisePickerBuilderVisitor::exitParent()
{
	if (rootTreeItemStack.Num() > 0)
	{
		rootTreeItemStack.Pop();
	}
}
