#include "AkIntegrationBehavior.h"

#include "AkUnrealHelper.h"
#include "AkEventBasedIntegrationBehavior.h"
#include "AkLegacyIntegrationBehavior.h"
#include "AkSettings.h"

AkIntegrationBehavior* AkIntegrationBehavior::s_Instance = nullptr;

AkIntegrationBehavior* AkIntegrationBehavior::Get()
{
	if (!s_Instance)
	{
		if (AkUnrealHelper::IsUsingEventBased())
		{
			s_Instance = new AkEventBasedIntegrationBehavior;
		}
		else
		{
			s_Instance = new AkLegacyIntegrationBehavior;
		}
	}

	return s_Instance;
}
