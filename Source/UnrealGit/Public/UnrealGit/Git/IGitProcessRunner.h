// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitProcessTypes.h"

/**
 * Executes Git commands using the system Git binary.
 *
 * This interface forms the boundary between Unreal code and external process execution.
 * Implementations must be thread-safe and must never execute processes on the game thread.
 *
 * Error behavior:
 * - Implementations return an ExitCode and captured StdOut/StdErr.
 * - Implementations must not log or present UI; callers surface errors through the provider layer.
 */
class IGitProcessRunner
{
public:
	virtual ~IGitProcessRunner() = default;

	/**
	 * Executes a Git CLI request.
	 *
	 * Threading:
	 * - Must be called from a background thread.
	 *
	 * @param Request Git request to execute.
	 * @return Process execution result.
	 */
	virtual FGitProcessResult Run(const FGitProcessRequest& Request) = 0;
};

