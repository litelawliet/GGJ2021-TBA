// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	StringMatching.h:
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Array2D.h"

// Longest Common Substring
namespace LCS
{
	struct StringComparer
	{
		StringComparer(FString in_a, FString in_b)
		{
			a = in_a;
			b = in_b;
		}

		void lcs()
		{
			int32 m = a.Len();
			int32 n = b.Len();

			Array2D<int32> suffixLengths(m + 1, n + 1, 0);
			int32 result = 0;
			int32 aIdx = 0;
			int32 bIdx = 0;

			for (int32 i = 0; i <= m; i++)
			{
				for (int32 j = 0; j <= n; j++)
				{
					if (i == 0 || j == 0)
						suffixLengths(i, j) = 0;
					else if (a[i - 1] == b[j - 1])
					{
						suffixLengths(i, j) = suffixLengths(i - 1, j - 1) + 1;
						if (suffixLengths(i, j) > result)
						{
							result = suffixLengths(i, j);
							aIdx = i;
							bIdx = j;
						}
					}
					else
						suffixLengths(i, j) = 0;
				}
			}

			if (result > 2)
			{
				a.RemoveAt(aIdx - result, result);
				b.RemoveAt(bIdx - result, result);

				lcs();
			}
		}

		FString a;
		FString b;
	};

	// Obtain the length of the longest common sub-string between two strings
	// https://en.wikipedia.org/wiki/Longest_common_substring_problem
	float GetLCSScore(const FString& a, const FString& b);
};
