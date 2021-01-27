// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "AkAssetBase.h"
#include "AkAuxBus.generated.h"

class UAkAudioBank;

UCLASS(hidecategories=(Advanced, Attachment, Volume), BlueprintType)
class AKAUDIO_API UAkAuxBus : public UAkAssetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "AkAuxBus")
	UAkAudioBank* RequiredBank = nullptr;

protected:
	UAkAssetData* createAssetData(UObject* parent) const override;
};
