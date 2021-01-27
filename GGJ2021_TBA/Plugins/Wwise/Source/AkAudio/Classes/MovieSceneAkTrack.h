// Copyright (c) 2006-2016 Audiokinetic Inc. / All Rights Reserved

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "AkUEFeatures.h"

#if !UE_4_25_OR_LATER
#include "IMovieSceneTrackInstance.h"
#endif
#include "MovieSceneAkTrack.generated.h"


/**
 * Handles manipulation of an Ak track in a movie scene
 */
UCLASS(abstract, MinimalAPI)
class UMovieSceneAkTrack 
	: public UMovieSceneTrack
{
	GENERATED_BODY()

public:

	/** begin UMovieSceneTrack interface */
	
	virtual void RemoveAllAnimationData() override { Sections.Empty(); }
	virtual bool HasSection(const UMovieSceneSection& Section) const override { return Sections.Contains(&Section); }
	virtual void AddSection(UMovieSceneSection& Section) override { Sections.Add(&Section); }
	virtual void RemoveSection(UMovieSceneSection& Section) override { Sections.Remove(&Section); }
	virtual bool IsEmpty() const override { return Sections.Num() == 0; }
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override { return Sections; }

	/** end UMovieSceneTrack interface */

	void SetIsAMasterTrack(bool AMasterTrack) { bIsAMasterTrack = AMasterTrack; }
	bool IsAMasterTrack() const { return bIsAMasterTrack; }

protected:

	/** All the sections in this track */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;

	UPROPERTY()
	uint32 bIsAMasterTrack : 1;
};
