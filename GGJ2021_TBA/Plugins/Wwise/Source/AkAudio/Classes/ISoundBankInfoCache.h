#pragma once

#include "AkInclude.h"

class AKAUDIO_API ISoundBankInfoCache
{
public:
	virtual ~ISoundBankInfoCache() {}
	virtual bool IsSoundBankUpToUpdate(const FGuid& Id, const FString& Platform, const FString& Language, const uint32 Hash) const = 0;
};