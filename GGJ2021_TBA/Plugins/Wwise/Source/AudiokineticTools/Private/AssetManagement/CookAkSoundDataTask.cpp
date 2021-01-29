#include "CookAkSoundDataTask.h"

#include "ToolBehavior/AkToolBehavior.h"

CookAkSoundDataTask::CookAkSoundDataTask(const AkSoundDataBuilder::InitParameters& InitParameters)
{
	_dataSource = AkToolBehavior::Get()->CookAkSoundDataTask_CreateBuilder(InitParameters);
	_dataSource->Init();
}

CookAkSoundDataTask::~CookAkSoundDataTask()
{
}

void CookAkSoundDataTask::ExecuteForEditorPlatform()
{
	AkSoundDataBuilder::InitParameters initParameters;
	initParameters.Platforms = { FPlatformProperties::IniPlatformName() };

	ExecuteTask(initParameters);
}

void CookAkSoundDataTask::ExecuteTask(const AkSoundDataBuilder::InitParameters& InitParameters)
{
	(new FAutoDeleteAsyncTask<CookAkSoundDataTask>(InitParameters))->StartBackgroundTask();
}

void CookAkSoundDataTask::DoWork()
{
	_dataSource->DoWork();
}
