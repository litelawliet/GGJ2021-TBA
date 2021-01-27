// Defines which features of the Wwise-Unreal integration are supported in which version of UE.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#define UE_4_21_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 21)
#define UE_4_22_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 22)
#define UE_4_23_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 23)
#define UE_4_24_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 24)
#define UE_4_25_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
#define UE_4_26_OR_LATER (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26)

#if UE_4_22_OR_LATER
#define AK_DEPRECATED UE_DEPRECATED
#else
#define AK_DEPRECATED DEPRECATED
#endif

