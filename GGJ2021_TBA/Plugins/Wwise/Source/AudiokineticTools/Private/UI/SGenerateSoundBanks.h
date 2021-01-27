// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
/*------------------------------------------------------------------------------------
	SGenerateSoundBanks.h
------------------------------------------------------------------------------------*/
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "WwiseProject/WwiseProjectInfo.h"

class SCheckBox;

class SGenerateSoundBanks : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGenerateSoundBanks )
	{}
	SLATE_END_ARGS( )

	SGenerateSoundBanks(void);

	AUDIOKINETICTOOLS_API void Construct(const FArguments& InArgs);
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;

	/** override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	bool ShouldDisplayWindow() { return PlatformNames.Num() != 0; }

private:
	void PopulateList();

private:
	FReply OnGenerateButtonClicked();
	TSharedRef<ITableRow> MakePlatformListItemWidget(TSharedPtr<FString> Platform, const TSharedRef<STableViewBase>& OwnerTable);

private:
	TSharedPtr<SListView<TSharedPtr<FString>>> PlatformList;
	TSharedPtr<SListView<TSharedPtr<FString>>> LanguageList;
	TSharedPtr<SCheckBox> SkipLanguagesCheckBox;

	TArray<TSharedPtr<FString>> PlatformNames;
	TArray<TSharedPtr<FString>> LanguagesNames;

	WwiseProjectInfo wwiseProjectInfo;
};