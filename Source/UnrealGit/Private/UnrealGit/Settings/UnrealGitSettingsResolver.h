// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"

/**
 * Builds a validated provider settings snapshot from Unreal config UObjects.
 */
class FUnrealGitSettingsResolver final
{
public:
	static bool Build(FUnrealGitProviderSettings& OutSettings, FText& OutErrorText);
};

