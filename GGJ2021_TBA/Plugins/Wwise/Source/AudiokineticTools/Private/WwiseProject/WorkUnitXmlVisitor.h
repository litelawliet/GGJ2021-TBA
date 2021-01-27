#pragma once

#include "Containers/UnrealString.h"
#include "WwiseItemType.h"

class WorkUnitXmlVisitor
{
public:
	WorkUnitXmlVisitor() {}
	virtual ~WorkUnitXmlVisitor() {}

	virtual void Init(EWwiseItemType::Type Type) {}
	virtual void ForceInit() {}
	virtual void OnBeginParse() {}

	virtual void EnterWorkUnit(const FString& Path, const FString& RelativePath, EWwiseItemType::Type ItemType, bool IsStandaloneWorkUnit, bool ForceRefresh) {}
	virtual void ExitWorkUnit(bool IsStandaloneWorkUnit) {}

	// todo slaptiste: consider changing these functions to accept FGuids as arguments
	virtual void EnterEvent(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void EnterAcousticTexture(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void EnterAuxBus(const FGuid& Id, const FString& Name, const FString& RelativePath) {}
	virtual void ExitAuxBus() {}

	virtual void EnterFolderOrBus(const FString& Name, const FString& RelativePath, EWwiseItemType::Type ItemType) {}
	virtual void ExitFolderOrBus() {}

	virtual void EnterStateGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) {}
	virtual void ExitStateGroup() {}

	virtual void EnterState(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void EnterSwitchGroup(const FGuid& Id, const FString& Name, const FString& RelativePath) {}
	virtual void ExitSwitchGroup() {}

	virtual void EnterSwitch(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void EnterGameParameter(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void EnterTrigger(const FGuid& Id, const FString& Name, const FString& RelativePath) {}

	virtual void ExitChildrenList() {}

	virtual void RemoveWorkUnit(const FString& WorkUnitPath) {}

	virtual void End() {}
};
