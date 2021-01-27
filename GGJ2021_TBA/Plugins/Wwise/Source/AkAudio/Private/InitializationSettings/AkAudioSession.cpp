// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved

#include "InitializationSettings/AkAudioSession.h"
#include "InitializationSettings/AkInitializationSettings.h"

void FAkAudioSession::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
#if PLATFORM_IOS
    InitializationStructure.PlatformInitSettings.audioSession.eCategory = (AkAudioSessionCategory)AudioSessionCategory;
    InitializationStructure.PlatformInitSettings.audioSession.eCategoryOptions = (AkAudioSessionCategoryOptions)AudioSessionCategoryOptions;
    InitializationStructure.PlatformInitSettings.audioSession.eMode = (AkAudioSessionMode)AudioSessionMode;
#endif
}

