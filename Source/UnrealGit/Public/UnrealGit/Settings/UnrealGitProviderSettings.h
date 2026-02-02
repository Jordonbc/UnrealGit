// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Validated configuration snapshot used by the UnrealGit provider at runtime.
 *
 * Responsibility:
 * - Centralizes all config access into a single immutable value type.
 * - Workers and provider code must consume this snapshot instead of reading UObjects directly.
 *
 * Threading:
 * - This is safe to copy and pass across threads.
 */
struct FUnrealGitProviderSettings final
{
	FString GitExecutable;

	/** Directory used for initial repo root discovery via `git rev-parse --show-toplevel`. */
	FString RepositoryDiscoveryDirectory;

	bool bPullRebase = false;
	bool bAutoPushAfterSubmit = false;

	bool bEnableLfsLocks = true;
	int32 LfsLocksRefreshIntervalSeconds = 15;

	bool bAutoLockOnCheckout = false;
};

