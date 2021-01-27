// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AkAudioType.h"
#include "AkAcousticTexture.generated.h"

UCLASS(BlueprintType)
class AKAUDIO_API UAkAcousticTexture : public UAkAudioType
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category="AkTexture")
	FLinearColor	EditColor;
#endif
};
