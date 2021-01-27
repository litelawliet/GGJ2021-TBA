// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reservedn
#include "AkAuxBus.h"

UAkAssetData* UAkAuxBus::createAssetData(UObject* parent) const
{
	return NewObject<UAkAssetDataWithMedia>(parent);
}
