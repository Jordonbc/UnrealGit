// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

/**
 * @class FSystemGitProcessRunner
 * System Git CLI runner using Unreal's process APIs.
 *
 * Threading:
 * - Instances are thread-safe.
 * - Run must be called off the game thread.
 */
class FSystemGitProcessRunner final : public IGitProcessRunner
{
public:
	/** Constructs a runner bound to the given Git executable path. */
	explicit FSystemGitProcessRunner(FString InGitExecutablePath);

	/**
	 * Executes a Git request via the system Git binary.
	 *
	 * @param Request Git request to execute.
	 * @return Process execution result.
	 */
	virtual FGitProcessResult Run(const FGitProcessRequest& Request) override;

	/** Default per-invocation timeout in seconds. */
	static constexpr double DefaultTimeoutSeconds = 5.0;

private:
	/** Resolved path to the Git executable used by this runner. */
	FString GitExecutablePath;

	/** Joins the argument list into a single command-line string. */
	static FString BuildCommandLine(const TArray<FString>& Arguments);

	/**
	 * Spawns the Git process once with the given timeout.
	 *
	 * @param Params Command-line parameters.
	 * @param Request Original Git request.
	 * @param TimeoutSeconds Timeout in seconds.
	 * @param GitExecutablePath Path to the Git executable.
	 * @return Process execution result.
	 */
	static FGitProcessResult RunProcessOnce(const FString& Params, const FGitProcessRequest& Request, double TimeoutSeconds, const FString& GitExecutablePath);

	/** Removes a stale `.git/index.lock` left by a crashed Git process. */
	static void EnsureStaleIndexLockRemoved(const FString& WorkingDirectory);
};