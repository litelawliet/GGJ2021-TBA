// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	WwiseEventDragDropOp.h
------------------------------------------------------------------------------------*/
#pragma once

#include "Containers/Map.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ContentBrowserDelegates.h"

class FWwiseAssetDragDropOp : public FAssetDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWwiseEventDragDropOp, FAssetDragDropOp)

	static TSharedRef<FAssetDragDropOp> New(const FAssetData& InAssetData, UActorFactory* ActorFactory = nullptr);

	static TSharedRef<FAssetDragDropOp> New(TArray<FAssetData> InAssetData, UActorFactory* ActorFactory = nullptr);

	static TSharedRef<FAssetDragDropOp> New(FString InAssetPath);

	static TSharedRef<FAssetDragDropOp> New(TArray<FString> InAssetPaths);

	static TSharedRef<FAssetDragDropOp> New(TArray<FAssetData> InAssetData, TArray<FString> InAssetPaths, UActorFactory* ActorFactory = nullptr);

	bool OnAssetViewDrop(const FAssetViewDragAndDropExtender::FPayload& Payload);
	bool OnAssetViewDragOver(const FAssetViewDragAndDropExtender::FPayload& Payload);
	bool OnAssetViewDragLeave(const FAssetViewDragAndDropExtender::FPayload& Payload);

	void SetCanDrop(const bool InCanDrop);

	void SetTooltipText();
	FText GetTooltipText() const;

	~FWwiseAssetDragDropOp();

public:
	FText CurrentHoverText;
	bool CanDrop = false;
	FAssetViewDragAndDropExtender* Extender = nullptr;
};