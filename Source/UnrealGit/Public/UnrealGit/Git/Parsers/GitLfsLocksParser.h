// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * @class FGitLfsLocksParser
 * Parses `git lfs locks --json` output.
 *
 * This parser is deterministic and side-effect free.
 */
class UNREALGIT_API FGitLfsLocksParser final
{
public:
	/**
	 * Parses `git lfs locks --json` output into LFS lock entries.
	 *
	 * @param StdOutText Raw stdout text (JSON).
	 * @param OutLocks Parsed LFS locks on success.
	 * @param OutError Error message on parse failure.
	 * @return True on success, false otherwise.
	 */
	static bool ParseJson(const FString& StdOutText, TArray<FGitLfsLock>& OutLocks, FString& OutError);
};
