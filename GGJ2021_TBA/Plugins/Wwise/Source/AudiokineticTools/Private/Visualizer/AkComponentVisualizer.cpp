// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkComponentVisualizer.cpp:
=============================================================================*/
#include "AkComponentVisualizer.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"
#include "SceneView.h"
#include "SceneManagement.h"

#include "AkWaapiClient.h"
#include "AkWaapiUtils.h"

namespace FAkComponentVisualizer_Helper
{
	float GetRadius(const UAkComponent* AkComponent)
	{
		if (auto waapiClient = FAkWaapiClient::Get())
		{
			auto AkAudioEventName = GET_AK_EVENT_NAME(AkComponent->AkAudioEvent, AkComponent->EventName);
			if (!AkAudioEventName.IsEmpty())
			{
				TSharedRef<FJsonObject> args = MakeShared<FJsonObject>();
				{
					TSharedPtr<FJsonObject> from = MakeShared<FJsonObject>();
					from->SetArrayField(WwiseWaapiHelper::NAME, TArray<TSharedPtr<FJsonValue>>
					{
						MakeShared<FJsonValueString>(FString::Printf(TEXT("Event:%s"), *AkAudioEventName))
					});
					args->SetObjectField(WwiseWaapiHelper::FROM, from);
				}

				TSharedRef<FJsonObject> options = MakeShared<FJsonObject>();
				options->SetArrayField(WwiseWaapiHelper::RETURN, TArray<TSharedPtr<FJsonValue>>
				{
					MakeShared<FJsonValueString>(WwiseWaapiHelper::MAX_RADIUS_ATTENUATION)
				});

				TSharedPtr<FJsonObject> result;
				if (waapiClient->Call(ak::wwise::core::object::get, args, options, result, 500, true))
				{
					TArray<TSharedPtr<FJsonValue>> ReturnArray = result->GetArrayField(WwiseWaapiHelper::RETURN);
					if (ReturnArray.Num() > 0)
					{
						const TSharedPtr<FJsonObject>* AttenuationObjectPtr = nullptr;
						if (ReturnArray[0]->AsObject()->TryGetObjectField(WwiseWaapiHelper::MAX_RADIUS_ATTENUATION, AttenuationObjectPtr))
						{
							return (*AttenuationObjectPtr)->GetNumberField(WwiseWaapiHelper::RADIUS);
						}
					}
				}
			}
		}

		return AkComponent->GetAttenuationRadius();
	}
}

void FAkComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!View->Family->EngineShowFlags.AudioRadius)
		return;

	const UAkComponent* AkComponent = Cast<const UAkComponent>(Component);
	if (!AkComponent)
		return;

	auto radius = FAkComponentVisualizer_Helper::GetRadius(AkComponent);
	if (radius <= 0.0f)
		return;

	FColor AudioOuterRadiusColor(255, 153, 0);
	DrawWireSphereAutoSides(PDI, AkComponent->GetComponentLocation(), AudioOuterRadiusColor, radius, SDPG_World);
}
