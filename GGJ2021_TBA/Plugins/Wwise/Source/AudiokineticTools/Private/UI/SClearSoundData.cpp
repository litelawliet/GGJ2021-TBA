// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	SClearSoundData.cpp
------------------------------------------------------------------------------------*/

#include "SClearSoundData.h"

#include "AkAudioBankGenerationHelpers.h"
#include "Dialogs/Dialogs.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AkAudio"

SClearSoundData::SClearSoundData()
{
}

void SClearSoundData::Construct(const FArguments& InArgs)
{
	// Build the form
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			.HAlign(HAlign_Left)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("AkClearWwiseProject", "Clear Wwise Project"))
				.BodyContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ClearSoundBankInfoCache, SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkClearSundBankInfoCache", "Clear SoundBanks incremental build cache"))
							.ToolTipText(LOCTEXT("AkClearSundBankInfoCacheToolTip", "Delete the SoundBankInfoCache.dat in the Wwise project cache folder. This file is used in incremental sound bank generation to determine if we need to regenerate the bank or not."))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ClearMediaCache, SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkClearMediaCache", "Clear the media files inside the cache"))
							.ToolTipText(LOCTEXT("AkClearMediaCacheTip", "Delete all the cached .wem files in the Wwise project cache folder."))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ClearExternalSource, SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkClearExternalSource", "Clear the external sources cache."))
							.ToolTipText(LOCTEXT("AkClearExternalSourceTip", "Delete all the .wem files in the Unreal External Source folder."))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			.HAlign(HAlign_Left)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("AkClearUnrealData", "Clear Unreal Assets"))
				.BodyContent()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ClearAssetData, SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkClearAsset", "Clear data in assets"))
							.ToolTipText(LOCTEXT("AkClearAssetTooltip", "Inside the Wwise Unreal assets, we store bank data, media data and more. This allows to clean all those for a fresh restart. You should also clear all wwise project data before regenerating the sound data."))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ClearOrphanMedia, SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AkDeleteOrphanMedia", "Clear orphaned Unreal Wwise Media assets."))
							.ToolTipText(LOCTEXT("AkDeleteOrphanMediaTooltip", "Delete all Unreal Wwise Media assets that doesn't reference to anything anymore"))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("AkClear", "Clear"))
				.OnClicked(this, &SClearSoundData::OnClearButtonClicked)
			]
		]
	];
}

FReply SClearSoundData::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	if( InKeyboardEvent.GetKey() == EKeys::Enter )
	{
		return OnClearButtonClicked();
	}
	else if( InKeyboardEvent.GetKey() == EKeys::Escape )
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		ParentWindow->RequestDestroyWindow();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyboardEvent);
}

FReply SClearSoundData::OnClearButtonClicked()
{
	AkAudioBankGenerationHelper::AkSoundDataClearFlags clearFlags = AkAudioBankGenerationHelper::AkSoundDataClearFlags::None;

	if (ClearAssetData && ClearAssetData->IsChecked())
	{
		clearFlags |= AkAudioBankGenerationHelper::AkSoundDataClearFlags::AssetData;
	}
	if (ClearSoundBankInfoCache && ClearSoundBankInfoCache->IsChecked())
	{
		clearFlags |= AkAudioBankGenerationHelper::AkSoundDataClearFlags::SoundBankInfoCache;
	}
	if (ClearMediaCache && ClearMediaCache->IsChecked())
	{
		clearFlags |= AkAudioBankGenerationHelper::AkSoundDataClearFlags::MediaCache;
	}
	if (ClearOrphanMedia && ClearOrphanMedia->IsChecked())
	{
		clearFlags |= AkAudioBankGenerationHelper::AkSoundDataClearFlags::OrphanMedia;
	}
	if (ClearExternalSource && ClearExternalSource->IsChecked())
	{
		clearFlags |= AkAudioBankGenerationHelper::AkSoundDataClearFlags::ExternalSource;
	}

	if (clearFlags != AkAudioBankGenerationHelper::AkSoundDataClearFlags::None)
	{
		AkAudioBankGenerationHelper::DoClearSoundData(clearFlags);
	}

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	ParentWindow->RequestDestroyWindow();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
