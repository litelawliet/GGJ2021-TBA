// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AkUEFeatures.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneAkAudioRTPCTemplate.generated.h"


class UMovieSceneAkAudioRTPCSection;

struct FMovieSceneAkAudioRTPCSectionData
{
	FMovieSceneAkAudioRTPCSectionData() {}

	FMovieSceneAkAudioRTPCSectionData(const UMovieSceneAkAudioRTPCSection& Section);

	FString RTPCName;

	FMovieSceneFloatChannel RTPCChannel;
};


USTRUCT()
struct AKAUDIO_API FMovieSceneAkAudioRTPCTemplate
	: public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneAkAudioRTPCTemplate() {}

	FMovieSceneAkAudioRTPCTemplate(const UMovieSceneAkAudioRTPCSection& InSection);

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void SetupOverrides() override { EnableOverrides(RequiresSetupFlag); }

	UPROPERTY()
	const UMovieSceneAkAudioRTPCSection* Section = nullptr;
};
