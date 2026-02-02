// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * Parses `git lfs locks --json` output.
 *
 * This parser is deterministic and side-effect free.
 */
class FGitLfsLocksParser final
{
public:
	static bool ParseJson(const FString& StdOutText, TArray<FGitLfsLock>& OutLocks, FString& OutError);
};

