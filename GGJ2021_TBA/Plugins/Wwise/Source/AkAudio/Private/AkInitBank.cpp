#include "AkInitBank.h"

UAkInitBank::UAkInitBank()
{
	bDelayLoadAssetMedia = true;
}

UAkAssetData* UAkInitBank::createAssetData(UObject* parent) const
{
	return NewObject<UAkInitBankAssetData>(parent);
}

void UAkInitBank::Load()
{
	Super::Load();
	bDelayLoadAssetMedia = false;
}

#if WITH_EDITOR
void UAkInitBank::Reset()
{
	AvailableAudioCultures.Empty();

	Super::Reset();
}
#endif
