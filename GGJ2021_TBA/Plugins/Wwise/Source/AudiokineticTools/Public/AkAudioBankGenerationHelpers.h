// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

#pragma once

#include "Misc/EnumClassFlags.h"
#include "Logging/LogMacros.h"
#include "Core/Public/UObject/WeakObjectPtrTemplates.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAkSoundData, Log, All);

class UAkAudioBank;

namespace AkAudioBankGenerationHelper
{
	/**
	 * Get path to the WwiseConsole application
	 */
	FString GetWwiseConsoleApplicationPath();

	/**
	 * Function to create the Generate SoundBanks window
	 *
	 * @param pSoundBanks				List of SoundBanks to be pre-selected
	 * @paramin_bShouldSaveWwiseProject	Whether the Wwise project should be saved or not
	 */
	void CreateGenerateSoundDataWindow(TArray<TWeakObjectPtr<UAkAudioBank>>* SoundBanks = nullptr, bool ProjectSave = false);

	void CreateClearSoundDataWindow();

	enum class AkSoundDataClearFlags
	{
		None = 0,
		AssetData = 1 << 0,
		SoundBankInfoCache = 1 << 1,
		MediaCache = 1 << 2,
		OrphanMedia = 1 << 3,
		ExternalSource = 1 << 4
	};

	ENUM_CLASS_FLAGS(AkSoundDataClearFlags)

	void DoClearSoundData(AkSoundDataClearFlags ClearFlags);
}
