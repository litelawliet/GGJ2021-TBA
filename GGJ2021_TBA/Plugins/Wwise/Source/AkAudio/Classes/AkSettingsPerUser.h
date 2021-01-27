// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "Engine/EngineTypes.h"
#include "AkWaapiClient.h"
#include "AkSettingsPerUser.generated.h"

UCLASS(config = EditorPerProjectUserSettings)
class AKAUDIO_API UAkSettingsPerUser : public UObject
{
	GENERATED_UCLASS_BODY()

	// Wwise Installation Path (Windows Authoring tool)
	UPROPERTY(Config, EditAnywhere, Category = "Installation")
	FDirectoryPath WwiseWindowsInstallationPath;

	// Wwise Installation Path (Mac Authoring tool)
	UPROPERTY(Config, EditAnywhere, Category = "Installation", meta = (FilePathFilter = "app", AbsolutePath))
	FFilePath WwiseMacInstallationPath;

	// IP Address used to connect to WAAPI. Changing this requires Editor restart
	UPROPERTY(Config, EditAnywhere, Category = "WAAPI")
	FString WaapiIPAddress = WAAPI_LOCAL_HOST_IP_STRING;

	// Network Port used to connect to WAAPI. Changing this requires Editor restart
	UPROPERTY(Config, EditAnywhere, Category = "WAAPI")
	uint32 WaapiPort = WAAPI_PORT;

	// Whether to synchronize the selection between the WAAPI picker and the Wwise Project Explorer
	UPROPERTY(Config, EditAnywhere, Category = "WAAPI")
	bool AutoSyncSelection = true;

	UPROPERTY(Config)
	bool SuppressWwiseProjectPathWarnings = false;

	UPROPERTY(Config)
	bool SoundDataGenerationSkipLanguage = false;

#if WITH_EDITOR
protected:
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#if UE_4_25_OR_LATER
	void PreEditChange(FProperty* PropertyAboutToChange) override;
#else
	void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif

private:
	FString PreviousWwiseWindowsInstallationPath;
	FString PreviousWwiseMacInstallationPath;
#endif
};