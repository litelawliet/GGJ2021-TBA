#pragma once

#include "Async/AsyncWork.h"
#include "Templates/SharedPointer.h"
#include "WwiseProject/WwiseProjectInfo.h"
#include "AssetManagement/AkSoundDataBuilder.h"

class CookAkSoundDataTask : public FNonAbandonableTask
{
public:
	CookAkSoundDataTask(const AkSoundDataBuilder::InitParameters& InitParameters);
	~CookAkSoundDataTask();

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(CookAkSoundDataTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	static void ExecuteForEditorPlatform();
	static void ExecuteTask(const AkSoundDataBuilder::InitParameters& InitParameters);

private:
	TSharedPtr<AkSoundDataBuilder, ESPMode::ThreadSafe> _dataSource;
};
