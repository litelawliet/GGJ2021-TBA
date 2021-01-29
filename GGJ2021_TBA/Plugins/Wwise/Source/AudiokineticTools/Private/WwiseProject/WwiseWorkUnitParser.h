#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "WwiseItemType.h"
#include "Misc/DateTime.h"

class WorkUnitXmlVisitor;
class FXmlFile;
class FXmlNode;

class WwiseWorkUnitParser
{
public:
	void SetVisitor(WorkUnitXmlVisitor* Visitor) { visitor = Visitor; }

	bool Parse();

	bool ForceParse();

private:
	void parseFolders(const FString& FolderName, EWwiseItemType::Type ItemType);
	void parseWorkUnitFile(const FString& WwuPath, const FString& RelativePath, EWwiseItemType::Type ItemType, bool ForceRefresh, bool ForceParse);
	void parseWorkUnitXml(const FXmlFile& WorkUnitXml, const FString& WorkUnitPath, const FString& RelativePath, EWwiseItemType::Type ItemType);
	void parseWorkUnitChildren(const FXmlNode* NodeToParse, const FString& WorkUnitPath, const FString& RelativePath, EWwiseItemType::Type ItemType);
	void recurse(const FXmlNode* CurrentNode, const FString& WorkUnitPath, const FString& CurrentPath, EWwiseItemType::Type ItemType);
	bool isStandAloneWwu(const FXmlFile& Wwu, EWwiseItemType::Type ItemType);

	TMap<EWwiseItemType::Type, TMap<FString, FDateTime>> wwuLastPopulateTime;
	FString projectRootFolder;
	WorkUnitXmlVisitor* visitor = nullptr;
};