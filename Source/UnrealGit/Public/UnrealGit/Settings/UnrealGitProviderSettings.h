// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @struct FUnrealGitProviderSettings
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
	/** Absolute path to the git executable used for all process invocations. */
	FString GitExecutable;

	/** Directory used for initial repo root discovery via `git rev-parse --show-toplevel`. */
	FString RepositoryDiscoveryDirectory;

	/** Uses `git pull --rebase` instead of `git pull`. */
	bool bPullRebase = false;

	/** Pushes after a successful submit (commit). */
	bool bAutoPushAfterSubmit = false;

	/** Enables Git LFS lock integration (locks/lock state). */
	bool bEnableLfsLocks = true;

	/** Refresh interval (seconds) for querying `git lfs locks`. */
	int32 LfsLocksRefreshIntervalSeconds = 15;

	/** Current git user identity, used to recognise locks owned by this user. */
	FString UserName;
	FString UserEmail;
};

