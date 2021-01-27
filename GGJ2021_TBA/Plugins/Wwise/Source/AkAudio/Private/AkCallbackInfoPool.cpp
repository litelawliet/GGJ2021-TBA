// Copyright (c) 2019 Audiokinetic Inc. / All Rights Reserved
#include "AkCallbackInfoPool.h"
#include "AkGameplayTypes.h"

UAkCallbackInfo* AkCallbackInfoPool::internalAcquire(UClass* type)
{
	ensure(IsInGameThread());
	auto& poolArray = Pool.FindOrAdd(type);
	if (poolArray.Num() > 0)
	{
		return poolArray.Pop();
	}

	auto instance = NewObject<UAkCallbackInfo>(GetTransientPackage(), type, NAME_None, RF_Public | RF_Standalone);
	gcStorage.Emplace(instance);
	return instance;
}

void AkCallbackInfoPool::Release(UAkCallbackInfo* instance)
{
	ensure(IsInGameThread());
	if (Pool.Contains(instance->GetClass()))
	{
		instance->Reset();
		Pool[instance->GetClass()].Push(instance);
	}
}
