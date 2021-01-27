
#include "AkAudioStyle.h"
#include "AkAudioDevice.h"
#include "WaapiPicker/WwiseTreeItem.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Engine/Texture2D.h"

TSharedPtr< FSlateStyleSet > FAkAudioStyle::StyleInstance = nullptr;

namespace AkAudioStyle_Helpers
{
	template<typename T1, typename T2>
	auto LoadAkTexture(T1&& RelativePath, T2&& TextureName)
	{
		return LoadObject<UTexture2D>(nullptr, *(FString("/Wwise/") / FString(Forward<T1>(RelativePath)) / FString(Forward<T2>(TextureName)) + FString(".") + FString(Forward<T2>(TextureName))));
	}

	auto CreateAkImageBrush(UTexture2D* Texture, const FVector2D& TextureSize)
	{
		return new FSlateImageBrush(Texture, TextureSize);
	}

	template<typename StringType, typename...Args>
	auto CreateEngineBoxBrush(FSlateStyleSet& Style, StringType& RelativePath, Args&&... args)
	{
		return new FSlateBoxBrush(Style.RootToContentDir(Forward<StringType>(RelativePath), TEXT(".png")), Forward<Args>(args)...);
	}

	template<typename StringType, typename...Args>
	auto EngineBoxBrush(FSlateStyleSet& Style, StringType&& RelativePath, Args&&... args)
	{
		return FSlateBoxBrush(Style.RootToContentDir(Forward<StringType>(RelativePath), TEXT(".png")), Forward<Args>(args)...);
	}

	template<typename StringType, typename...Args>
	auto EngineTTTFCoreFont(FSlateStyleSet& Style,StringType&& RelativePath, Args&&... args)
	{
		return FSlateFontInfo(Style.RootToCoreContentDir(Forward<StringType>(RelativePath), TEXT(".ttf")), Forward<Args>(args)...);
	}

	template<typename StringType, typename...Args>
	auto CreateEngineImageBrush(FSlateStyleSet& Style, StringType& RelativePath, Args&&... args)
	{
		return new FSlateImageBrush(Style.RootToContentDir(Forward<StringType>(RelativePath), TEXT(".png")), Forward<Args>(args)...);
	}
}

void SetAkBrush(FSlateStyleSet& Style, const char* BrushName, const char* TextureName)
{
	using namespace AkAudioStyle_Helpers;

	const FVector2D Icon15x15(15.0f, 15.0f);

	auto Texture = LoadAkTexture("WwiseTree/Icons", TextureName);
	if (Texture != nullptr)
	{
		Texture->AddToRoot();
		Style.Set(BrushName, CreateAkImageBrush(Texture, Icon15x15));
	}
}

void SetClassThumbnail(FSlateStyleSet& Style, const char* BrushName, const char* TextureName)
{
	using namespace AkAudioStyle_Helpers;

	const FVector2D ThumbnailSize(256.0f, 256.0f);

	auto Texture = LoadAkTexture("WwiseTypes", TextureName);
	if (Texture != nullptr)
	{
		Texture->AddToRoot();
		Style.Set(BrushName, CreateAkImageBrush(Texture, ThumbnailSize));
	}
}

void SetAkResourceBrushes(FSlateStyleSet& Style)
{
	SetAkBrush(Style, "AudiokineticTools.ActorMixerIcon", "actor_mixer_nor");
	SetAkBrush(Style, "AudiokineticTools.SoundIcon", "sound_fx_nor");
	SetAkBrush(Style, "AudiokineticTools.SwitchContainerIcon", "container_switch_nor");
	SetAkBrush(Style, "AudiokineticTools.RandomSequenceContainerIcon", "container_random_sequence_nor");
	SetAkBrush(Style, "AudiokineticTools.BlendContainerIcon", "layer_container_nor");
	SetAkBrush(Style, "AudiokineticTools.MotionBusIcon", "motion_bus_nor");
	SetAkBrush(Style, "AudiokineticTools.AkPickerTabIcon", "wwise_logo_32");
	SetAkBrush(Style, "AudiokineticTools.EventIcon", "event_nor");
	SetAkBrush(Style, "AudiokineticTools.AcousticTextureIcon", "acoutex_nor");
	SetAkBrush(Style, "AudiokineticTools.AuxBusIcon", "auxbus_nor");
	SetAkBrush(Style, "AudiokineticTools.BusIcon", "bus_nor");
	SetAkBrush(Style, "AudiokineticTools.FolderIcon", "folder_nor");
	SetAkBrush(Style, "AudiokineticTools.PhysicalFolderIcon", "physical_folder_nor");
	SetAkBrush(Style, "AudiokineticTools.WorkUnitIcon", "workunit_nor");
	SetAkBrush(Style, "AudiokineticTools.ProjectIcon", "wproj");
	SetAkBrush(Style, "AudiokineticTools.RTPCIcon", "gameparameter_nor");
	SetAkBrush(Style, "AudiokineticTools.StateIcon", "state_nor");
	SetAkBrush(Style, "AudiokineticTools.StateGroupIcon", "stategroup_nor");
	SetAkBrush(Style, "AudiokineticTools.SwitchIcon", "switch_nor");
	SetAkBrush(Style, "AudiokineticTools.SwitchGroupIcon", "switchgroup_nor");
	SetAkBrush(Style, "AudiokineticTools.TriggerIcon", "trigger_nor");

	SetClassThumbnail(Style, "ClassThumbnail.AkAcousticTexture", "AkAcousticTexture");
	SetClassThumbnail(Style, "ClassThumbnail.AkAudioEvent", "AkAudioEvent");
	SetClassThumbnail(Style, "ClassThumbnail.AkAuxBus", "AkAuxBus");
	SetClassThumbnail(Style, "ClassThumbnail.AkAudioBank", "AkAudioBank");
	SetClassThumbnail(Style, "ClassThumbnail.AkInitBank", "AkAudioBank");
	SetClassThumbnail(Style, "ClassThumbnail.AkLocalizedMediaAsset", "AkLocalizedMediaAsset");
	SetClassThumbnail(Style, "ClassThumbnail.AkMediaAsset", "AkMediaAsset");
	SetClassThumbnail(Style, "ClassThumbnail.AkExternalMediaAsset", "AkExternalMediaAsset");
	SetClassThumbnail(Style, "ClassThumbnail.AkRtpc", "AkRtpc");
	SetClassThumbnail(Style, "ClassThumbnail.AkStateValue", "AkStateValue");
	SetClassThumbnail(Style, "ClassThumbnail.AkSwitchValue", "AkSwitchValue");
	SetClassThumbnail(Style, "ClassThumbnail.AkTrigger", "AkTrigger");
}

void FAkAudioStyle::Initialize()
{
	using namespace AkAudioStyle_Helpers;

	if (!StyleInstance.IsValid())
	{
		StyleInstance = MakeShareable(new FSlateStyleSet(FAkAudioStyle::GetStyleSetName()));
		auto ContentRoot = FPaths::EngineContentDir() / TEXT("Slate");
		StyleInstance->SetContentRoot(ContentRoot);
		StyleInstance->SetCoreContentRoot(ContentRoot);

		FSlateStyleSet& Style = *StyleInstance.Get();
		{
			SetAkResourceBrushes(Style);

			Style.Set("AudiokineticTools.GroupBorder", CreateEngineBoxBrush(Style, "Common/GroupBorder", FMargin(4.0f / 16.0f)));
			Style.Set("AudiokineticTools.AssetDragDropTooltipBackground", CreateEngineBoxBrush(Style, "Old/Menu_Background", FMargin(8.0f / 64.0f)));

			FButtonStyle HoverHintOnly = FButtonStyle()
				.SetNormal(FSlateNoResource())
				.SetHovered(EngineBoxBrush(Style, "Common/Button_Hovered", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.15f)))
				.SetPressed(EngineBoxBrush(Style, "Common/Button_Pressed", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.25f)))
				.SetNormalPadding(FMargin(0, 0, 0, 1))
				.SetPressedPadding(FMargin(0, 1, 0, 0));
			Style.Set("AudiokineticTools.HoverHintOnly", HoverHintOnly);

			Style.Set("AudiokineticTools.SourceTitleFont", EngineTTTFCoreFont(Style, "Fonts/Roboto-Regular", 12));
			Style.Set("AudiokineticTools.SourceTreeItemFont", EngineTTTFCoreFont(Style, "Fonts/Roboto-Regular", 10));
			Style.Set("AudiokineticTools.SourceTreeRootItemFont", EngineTTTFCoreFont(Style, "Fonts/Roboto-Regular", 12));

			const FVector2D Icon12x12(12.0f, 12.0f);
			Style.Set("AudiokineticTools.Button_EllipsisIcon", CreateEngineImageBrush(Style, "Icons/ellipsis_12x", Icon12x12));
		}

		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}
#undef IMAGE_BRUSH

void FAkAudioStyle::Shutdown()
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        ensure(StyleInstance.IsUnique());
        StyleInstance.Reset();
    }
}

FName FAkAudioStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AudiokineticToolsStyle"));
	return StyleSetName;
}

const ISlateStyle& FAkAudioStyle::Get()
{
	Initialize();
	return *StyleInstance;
}

const FSlateBrush* FAkAudioStyle::GetBrush(EWwiseItemType::Type ItemType)
{
	auto& Style = Get();
	switch (ItemType)
	{
	case EWwiseItemType::Event: return Style.GetBrush("AudiokineticTools.EventIcon");
	case EWwiseItemType::AcousticTexture: return Style.GetBrush("AudiokineticTools.AcousticTextureIcon");
	case EWwiseItemType::AuxBus: return Style.GetBrush("AudiokineticTools.AuxBusIcon");
	case EWwiseItemType::Bus: return Style.GetBrush("AudiokineticTools.BusIcon");
	case EWwiseItemType::Folder: return Style.GetBrush("AudiokineticTools.FolderIcon");
	case EWwiseItemType::Project: return Style.GetBrush("AudiokineticTools.ProjectIcon");
	case EWwiseItemType::PhysicalFolder: return Style.GetBrush("AudiokineticTools.PhysicalFolderIcon");
	case EWwiseItemType::StandaloneWorkUnit:
	case EWwiseItemType::NestedWorkUnit: return Style.GetBrush("AudiokineticTools.WorkUnitIcon");
	case EWwiseItemType::ActorMixer: return Style.GetBrush("AudiokineticTools.ActorMixerIcon");
	case EWwiseItemType::Sound: return Style.GetBrush("AudiokineticTools.SoundIcon");
	case EWwiseItemType::SwitchContainer: return Style.GetBrush("AudiokineticTools.SwitchContainerIcon");
	case EWwiseItemType::RandomSequenceContainer: return Style.GetBrush("AudiokineticTools.RandomSequenceContainerIcon");
	case EWwiseItemType::BlendContainer: return Style.GetBrush("AudiokineticTools.BlendContainerIcon");
	case EWwiseItemType::MotionBus: return Style.GetBrush("AudiokineticTools.MotionBusIcon");
	case EWwiseItemType::GameParameter: return Style.GetBrush("AudiokineticTools.RTPCIcon");
	case EWwiseItemType::State: return Style.GetBrush("AudiokineticTools.StateIcon");
	case EWwiseItemType::StateGroup: return Style.GetBrush("AudiokineticTools.StateGroupIcon");
	case EWwiseItemType::Switch: return Style.GetBrush("AudiokineticTools.SwitchIcon");
	case EWwiseItemType::SwitchGroup: return Style.GetBrush("AudiokineticTools.SwitchGroupIcon");
	case EWwiseItemType::Trigger: return Style.GetBrush("AudiokineticTools.TriggerIcon");
	default:
		return nullptr;
	}
}

const FSlateBrush* FAkAudioStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return Get().GetBrush(PropertyName, Specifier);
}

const FSlateFontInfo FAkAudioStyle::GetFontStyle(FName PropertyName, const ANSICHAR* Specifier)
{
	return Get().GetFontStyle(PropertyName, Specifier);
}