// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"

/**
 * @class FUnrealGitSettingsResolver
 * Builds a validated provider settings snapshot from Unreal config UObjects.
 */
class FUnrealGitSettingsResolver final
{
public:
	/**
	 * Builds a validated settings snapshot.
	 *
	 * @param OutSettings Receives the resolved provider settings on success.
	 * @param OutErrorText Receives a human-readable error on failure.
	 * @return True when settings were resolved successfully.
	 */
	static bool Build(FUnrealGitProviderSettings& OutSettings, FText& OutErrorText);
};

