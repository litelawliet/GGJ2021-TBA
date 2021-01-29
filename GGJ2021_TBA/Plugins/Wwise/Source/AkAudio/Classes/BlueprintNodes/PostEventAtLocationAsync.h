#pragma once

#include "AkAudioDevice.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "PostEventAtLocationAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPostEventAtLocationAsyncOutputPin, int32, PlayingID);

UCLASS()
class AKAUDIO_API UPostEventAtLocationAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FPostEventAtLocationAsyncOutputPin Completed;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UPostEventAtLocationAsync* PostEventAtLocationAsync(const UObject* WorldContextObject, class UAkAudioEvent* AkEvent, FVector Location, FRotator Orientation);

public:
	void Activate() override;

private:
	UFUNCTION()
	void PollPostEventFuture();

private:
	const UObject* WorldContextObject = nullptr;
	class UAkAudioEvent* AkEvent = nullptr;
	FVector Location;
	FRotator Orientation;
	TFuture<AkPlayingID> playingIDFuture;
	FTimerHandle Timer;
};
