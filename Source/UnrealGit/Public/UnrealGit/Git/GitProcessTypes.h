// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A Git process request executed via the system Git CLI.
 *
 * This is a value type intended to be constructed by workers and executed by an
 * IGitProcessRunner implementation on a background thread.
 *
 * Threading:
 * - This struct is safe to construct and move across threads.
 * - The process execution itself must not run on the game thread.
 */
struct FGitProcessRequest final
{
	/** Git subcommand and arguments, excluding the git executable itself. Example: { "status", "--porcelain=v2" }. */
	TArray<FString> Arguments;

	/** Repository root - if set, automatically prepends "-C <RepoRoot>" to Arguments. */
	FString RepoRoot;

	/** Working directory for the git process (typically the repository root). */
	FString WorkingDirectory;

	/** Optional stdin payload to provide to the process. */
	TArray<uint8> StdIn;

	/** Optional timeout. If unset, the process is allowed to run until completion. */
	TOptional<FTimespan> Timeout;
};

/**
 * Result of a Git CLI process execution.
 *
 * StdOut/StdErr are captured as raw bytes to preserve NUL-delimited (-z) and
 * binary output (uasset/uasset diffs via git show).
 */
struct FGitProcessResult final
{
	int32 ExitCode = INDEX_NONE;
	bool bWasCanceled = false;
	FTimespan Duration = FTimespan::Zero();

	TArray<uint8> StdOut;
	TArray<uint8> StdErr;
};

