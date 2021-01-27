#pragma once

#include "AkSoundDataBuilder.h"

#include "Async/Future.h"
#include "Containers/UnrealString.h"

class FEvent;
class FJsonObject;
class UAkAudioEvent;
class UAkAuxBus;
class UAkInitBank;

class WaapiAkSoundDataBuilder : public AkSoundDataBuilder
{
public:
	WaapiAkSoundDataBuilder(const InitParameters& InitParameter);
	~WaapiAkSoundDataBuilder();

	void Init() override;

	void DoWork() override;

private:
	void onSoundBankGenerated(uint64_t id, TSharedPtr<FJsonObject> responseJson);
	void onSoundBankGenerationDone(uint64_t id, TSharedPtr<FJsonObject> responseJson);

	bool parseBankData(UAkAssetData* AssetData, TSharedPtr<FJsonObject> ResponseJson, FCriticalSection* DataLock);

private:
	uint64 _generatedSubscriptionId = 0;
	uint64 _generatedDoneSubscriptionId = 0;
	FDelegateHandle _connectionLostHandle;

	FCriticalSection parseTasksLock;
	FGraphEventArray allParseTask;
	FEvent* waitForGenerationDoneEvent = nullptr;
	FThreadSafeBool _generationSuccess = true;
};
