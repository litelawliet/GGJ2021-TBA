// Copyright (c) 2006-2018 Audiokinetic Inc. / All Rights Reserved
#pragma once

#include "Engine/EngineTypes.h"
#include "Input/Reply.h"
#include "AkUEFeatures.h"

#if WITH_EDITOR
DECLARE_DELEGATE_RetVal(FReply, FOnButtonClickedMigration);
#endif

namespace AkUnrealHelper
{
	template <typename T>
	struct TMallocDelete
	{
#if UE_4_25_OR_LATER
		DECLARE_INLINE_TYPE_LAYOUT(TMallocDelete, NonVirtual);
#endif

		TMallocDelete() = default;
		TMallocDelete(const TMallocDelete&) = default;
		TMallocDelete& operator=(const TMallocDelete&) = default;
		~TMallocDelete() = default;

		template <
			typename U,
			typename = decltype(ImplicitConv<T*>((U*)nullptr))
		>
			TMallocDelete(const TMallocDelete<U>&)
		{
		}

		template <
			typename U,
			typename = decltype(ImplicitConv<T*>((U*)nullptr))
		>
			TMallocDelete& operator=(const TMallocDelete<U>&)
		{
			return *this;
		}

		void operator()(T* Ptr) const
		{
			FMemory::Free(Ptr);
		}
	};

	AKAUDIO_API void TrimPath(FString& Path);

	AKAUDIO_API FString GetProjectDirectory();
	AKAUDIO_API FString GetWwisePluginDirectory();
	AKAUDIO_API FString GetThirdPartyDirectory();
	AKAUDIO_API FString GetContentDirectory();
	AKAUDIO_API FString GetSoundBankDirectory();
	AKAUDIO_API FString GetExternalSourceDirectory();

	AKAUDIO_API FString GetBaseAssetPackagePath();
	AKAUDIO_API FString GetLocalizedAssetPackagePath();
	AKAUDIO_API FString GetExternalSourceAssetPackagePath();

	AKAUDIO_API FString GetWwiseProjectPath();
	AKAUDIO_API FString GetWwiseSoundBankInfoCachePath();

	AKAUDIO_API FString GetGeneratedSoundBanksFolder();

	AKAUDIO_API bool IsUsingEventBased();

	extern AKAUDIO_API const TCHAR* MediaFolderName;

	extern AKAUDIO_API const FGuid InitBankID;

#if WITH_EDITOR
	AKAUDIO_API void SanitizePath(FString& Path, const FString& PreviousPath, const FText& DialogMessage);
	AKAUDIO_API void SanitizeProjectPath(FString& Path, const FString& PreviousPath, const FText& DialogMessage, bool &bRequestRefresh);
	AKAUDIO_API void SaveConfigFile(UObject* ConfigObject);

	AKAUDIO_API FString GuidToBankName(const FGuid& Guid);
	AKAUDIO_API FGuid BankNameToGuid(const FString& BankName);

	AKAUDIO_API void ShowEventBasedPackagingMigrationDialog(FOnButtonClickedMigration in_OnclickedYes, FOnButtonClickedMigration in_OnclickedNo);
	AKAUDIO_API void DeleteOldSoundBanks();
#endif
}
