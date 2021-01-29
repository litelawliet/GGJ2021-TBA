#pragma once

#include "AkAssetBase.h"
#include "AkInitBank.generated.h"

USTRUCT()
struct AKAUDIO_API FAkPluginInfo
{
	GENERATED_BODY()

public:
	FAkPluginInfo() = default;

	FAkPluginInfo(const FString& InName, uint32 InPluginID, const FString& InDLL)
	: Name(InName)
	, PluginID(InPluginID)
	, DLL(InDLL)
	{
	}

	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	FString Name;

	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	uint32 PluginID;

	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	FString DLL;
};

UCLASS()
class AKAUDIO_API UAkInitBankAssetData : public UAkAssetDataWithMedia
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	TArray<FAkPluginInfo> PluginInfos;
};

UCLASS()
class AKAUDIO_API UAkInitBank : public UAkAssetBase
{
	GENERATED_BODY()
	UAkInitBank();
public:
	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	TArray<FString> AvailableAudioCultures;

	UPROPERTY(VisibleAnywhere, Category = "AkInitBank")
	FString DefaultLanguage;

	virtual void Load() override;

#if WITH_EDITOR
	void Reset() override;
#endif

protected:
	UAkAssetData* createAssetData(UObject* parent) const override;
};
