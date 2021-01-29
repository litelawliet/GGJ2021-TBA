// Copyright (c) 2006-2016 Audiokinetic Inc. / All Rights Reserved

#pragma once

#include "MovieSceneAkTrack.h"
#include "AkInclude.h"
#include "AkAudioEvent.h"
#if UE_4_26_OR_LATER
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#else
#include "MovieSceneBackwardsCompatibility.h"
#endif
#include "MovieSceneAkAudioRTPCTrack.generated.h"

/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneAkAudioRTPCTrack
	: public UMovieSceneAkTrack
	, public IMovieSceneTrackTemplateProducer
{
	GENERATED_BODY()

public:

	UMovieSceneAkAudioRTPCTrack()
	{
#if WITH_EDITORONLY_DATA
		SetColorTint(FColor(58, 111, 143, 65));
#endif
	}

	AKAUDIO_API virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

	AKAUDIO_API virtual UMovieSceneSection* CreateNewSection() override;

	AKAUDIO_API virtual FName GetTrackName() const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;

#if WITH_EDITORONLY_DATA
	AKAUDIO_API virtual FText GetDisplayName() const override;
#endif
};
