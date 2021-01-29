// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
/*------------------------------------------------------------------------------------
	SClearSoundData.h
------------------------------------------------------------------------------------*/
#pragma once

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"

class SCheckBox;

class SClearSoundData : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SClearSoundData )
	{}
	SLATE_END_ARGS( )

	SClearSoundData(void);

	AUDIOKINETICTOOLS_API void Construct(const FArguments& InArgs);
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;

	/** override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

private:
	FReply OnClearButtonClicked();

private:
	TSharedPtr<SCheckBox> ClearAssetData;
	TSharedPtr<SCheckBox> ClearSoundBankInfoCache;
	TSharedPtr<SCheckBox> ClearMediaCache;
	TSharedPtr<SCheckBox> ClearExternalSource;
	TSharedPtr<SCheckBox> ClearOrphanMedia;
};