#pragma once

#include "UObject/Object.h"
#include "AkAudioType.generated.h"

UCLASS()
class AKAUDIO_API UAkAudioType : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "AkAudioType")
	FGuid ID;
#endif

#if WITH_EDITOR
	virtual void Reset();
#endif

	UPROPERTY(VisibleAnywhere, Category="AkAudioType")
	uint32 ShortID;

	UPROPERTY(EditAnywhere, Category = "AkAudioType")
	TArray<UObject*> UserData;

	virtual void PostLoad() override;
	virtual void Load();
public:
	template<typename T>
	T* GetUserData()
	{
		for (auto entry : UserData)
		{
			if (entry && entry->GetClass()->IsChildOf(T::StaticClass()))
			{
				return entry;
			}
		}

		return nullptr;
	}
};
