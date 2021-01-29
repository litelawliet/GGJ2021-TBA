#pragma once

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"

#include "AssetData.h"

class SWindow;

class SAkAudioBankPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAkAudioBankPicker)
	{}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

	SAkAudioBankPicker(void);

	AUDIOKINETICTOOLS_API void Construct(const FArguments& InArgs);

	FAssetData SelectedAkEventGroup;

private:
	void OnCreateNewAssetSelected();
	void OnAssetSelected(const FAssetData& AssetData);
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	void OnAssetEnterPressed(const TArray<FAssetData>& AssetData);
	bool CanSelect() const;

	FReply CloseWindow();
	FReply OnCancel();

private:
	TWeakPtr<SWindow> WidgetWindow;
};