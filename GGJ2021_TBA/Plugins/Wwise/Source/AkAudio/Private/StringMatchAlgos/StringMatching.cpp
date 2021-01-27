// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	StringMatching.cpp:
=============================================================================*/
#include "StringMatching.h"
#include "Array2D.h"

float LCS::GetLCSScore(const FString& a, const FString& b)
{
	StringComparer compare(a, b);
	compare.lcs();

	float score = (float)(compare.a.Len() + compare.b.Len()) / (a.Len() + b.Len());

	float subscore;
	if (compare.a.Len() < compare.b.Len())
		subscore = (float)compare.a.Len() / a.Len();
	else
		subscore = (float)compare.b.Len() / b.Len();

	score = (1.f - score) * (1.f - subscore);

	return score;
}
