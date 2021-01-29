#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SanitizeWwiseObjectPathCommandlet.generated.h"

UCLASS()
class AUDIOKINETICTOOLS_API USanitizeWwiseObjectPathCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:

	USanitizeWwiseObjectPathCommandlet();

	// UCommandlet interface
	virtual int32 Main(const FString& Params) override;
};
