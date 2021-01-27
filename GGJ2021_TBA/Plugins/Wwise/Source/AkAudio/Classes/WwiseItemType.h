#pragma once

#include "Containers/UnrealString.h"

namespace EWwiseItemType
{
	enum Type
	{
		Event,
		AuxBus,
		AcousticTexture,
		State,
		Switch,
		GameParameter,
		Trigger,
		ActorMixer,
		Bus,
		Project,
		StandaloneWorkUnit,
		NestedWorkUnit,
		PhysicalFolder,
		Folder,
		Sound,
		SwitchContainer,
		RandomSequenceContainer,
		BlendContainer,
		MotionBus,
		StateGroup,
		SwitchGroup,

		LastWwiseDraggable = Trigger,
		LastWaapiDraggable = ActorMixer,

		None = -1,
	};

	static const FString ItemNames[] = {
		TEXT("Event"),
		TEXT("AuxBus"),
		TEXT("AcousticTexture"),
		TEXT("State"),
		TEXT("Switch"),
		TEXT("GameParameter"),
		TEXT("Trigger"),
		TEXT("ActorMixer"),
	};
	static const FString DisplayNames[] = {
		TEXT("Events"),
		TEXT("Busses"),
		TEXT("VirtualAcoustics"),
		TEXT("States"),
		TEXT("Switches"),
		TEXT("GameParameters"),
		TEXT("Triggers"),
		TEXT("ActorMixer"),
	};
	static const FString FolderNames[] = {
		TEXT("Events"),
		TEXT("Master-Mixer Hierarchy"),
		TEXT("Virtual Acoustics"),
		TEXT("States"),
		TEXT("Switches"),
		TEXT("Game Parameters"),
		TEXT("Triggers"),
		TEXT("Actor-Mixer Hierarchy"),
	};
	static const FString PickerLabel[] = {
		TEXT("Events"),
		TEXT("Auxiliary Busses"),
		TEXT("Textures"),
		TEXT("States"),
		TEXT("Switches"),
		TEXT("GameParameters"),
		TEXT("Triggers"),
		TEXT("Actor Mixer"),
	};

	inline Type FromString(const FString& ItemName)
	{
		struct TypePair
		{
			FString Name;
			Type Value;
		};

		static const TypePair ValidTypes[] = {
			{TEXT("AcousticTexture"), Type::AcousticTexture},
			{TEXT("ActorMixer"), Type::ActorMixer},
			{TEXT("AuxBus"), Type::AuxBus},
			{TEXT("BlendContainer"), Type::BlendContainer},
			{TEXT("Bus"), Type::Bus},
			{TEXT("Event"), Type::Event},
			{TEXT("Folder"), Type::Folder},
			{TEXT("GameParameter"), Type::GameParameter},
			{TEXT("MotionBus"), Type::MotionBus},
			{TEXT("PhysicalFolder"), Type::PhysicalFolder},
			{TEXT("Project"), Type::Project},
			{TEXT("RandomSequenceContainer"), Type::RandomSequenceContainer},
			{TEXT("Sound"), Type::Sound},
			{TEXT("State"), Type::State},
			{TEXT("StateGroup"), Type::StateGroup},
			{TEXT("Switch"), Type::Switch},
			{TEXT("SwitchContainer"), Type::SwitchContainer},
			{TEXT("SwitchGroup"), Type::SwitchGroup},
			{TEXT("Trigger"), Type::Trigger},
			{TEXT("WorkUnit"), Type::StandaloneWorkUnit},
		};

		for (const auto& type : ValidTypes)
		{
			if (type.Name == ItemName)
			{
				return type.Value;
			}
		}

		return Type::None;
	}
};
