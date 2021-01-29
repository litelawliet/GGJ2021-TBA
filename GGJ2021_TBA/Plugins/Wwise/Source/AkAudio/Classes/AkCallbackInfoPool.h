// Copyright (c) 2019 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "Engine/EngineTypes.h"
#include "CoreUObject/Public/UObject/StrongObjectPtr.h"

class UAkCallbackInfo;

class AkCallbackInfoPool final
{
public:
	template<typename CallbackType>
	CallbackType* Acquire()
	{
		return static_cast<CallbackType*>(internalAcquire(CallbackType::StaticClass()));
	}

	void Release(UAkCallbackInfo* instance);

private:
	UAkCallbackInfo* internalAcquire(UClass* type);

private:
	TMap<UClass*, TArray<UAkCallbackInfo*>> Pool;
	TArray<TStrongObjectPtr<UAkCallbackInfo>> gcStorage;
};
