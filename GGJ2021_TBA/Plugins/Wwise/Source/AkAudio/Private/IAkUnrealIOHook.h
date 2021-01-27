#pragma once

#include "AkInclude.h"

class IAkUnrealIOHook : public AK::StreamMgr::IAkIOHookDeferred,
	public AK::StreamMgr::IAkFileLocationResolver
{
public:
	virtual ~IAkUnrealIOHook() {}

	virtual bool Init(const AkDeviceSettings& in_deviceSettings) = 0;

	virtual AKRESULT LoadFilePackage(
		const AkOSChar* in_pszFilePackageName,	// File package name. Location is resolved using base class' Open().
		AkUInt32& out_uPackageID			// Returned package ID.
		)
	{
		return AK_Success;
	}

	virtual AKRESULT UnloadAllFilePackages() { return AK_Success; }
};