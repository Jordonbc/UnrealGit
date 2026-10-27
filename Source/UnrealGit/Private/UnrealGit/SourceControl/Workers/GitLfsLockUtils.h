// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

namespace UnrealGit::Workers
{
	/**
	 * Releases LFS locks for the given repo-relative paths by resolving lock IDs first.
	 *
	 * Path-based `git lfs unlock` re-resolves the lock itself and fails when that lookup
	 * does not match; passing the ID is reliable. Paths without a lock are skipped.
	 *
	 * @param ProcessRunner Process runner used to invoke Git.
	 * @param RepoRoot Repository root path.
	 * @param RelativePaths Repo-relative paths to unlock.
	 * @param bForce Whether to force-unlock.
	 * @param OutError Receives an error message on failure.
	 * @return True when all present locks were released.
	 */
	inline bool UnlockLfsLocks(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const TArray<FString>& RelativePaths,
		bool bForce,
		FString& OutError)
	{
		OutError.Reset();
		if (RelativePaths.Num() == 0)
		{
			return true;
		}

		FGitProcessRequest LocksRequest;
		LocksRequest.WorkingDirectory = FString();
		LocksRequest.RepoRoot = RepoRoot;
		LocksRequest.Arguments = { TEXT("lfs"), TEXT("locks"), TEXT("--json") };

		const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
		if (LocksResult.ExitCode != 0)
		{
			OutError = BytesToTextUtf8Lossy(LocksResult.StdErr).TrimStartAndEnd();
			return false;
		}

		TArray<FGitLfsLock> Locks;
		FString ParseError;
		if (!FGitLfsLocksParser::ParseJson(BytesToTextUtf8Lossy(LocksResult.StdOut), Locks, ParseError))
		{
			OutError = ParseError;
			return false;
		}

		TMap<FString, FString> LockIdByPath;
		for (const FGitLfsLock& Lock : Locks)
		{
			LockIdByPath.Add(NormalizePathSeparators(Lock.Path), Lock.Id);
		}

		for (const FString& RelativePath : RelativePaths)
		{
			const FString NormalizedPath = NormalizePathSeparators(RelativePath);

			const FString* LockId = LockIdByPath.Find(NormalizedPath);
			if (!LockId || LockId->IsEmpty())
			{
				continue;
			}

			FGitProcessRequest UnlockRequest;
			UnlockRequest.WorkingDirectory = FString();
			UnlockRequest.RepoRoot = RepoRoot;
			UnlockRequest.Arguments = { TEXT("lfs"), TEXT("unlock") };
			if (bForce)
			{
				UnlockRequest.Arguments.Add(TEXT("--force"));
			}
			UnlockRequest.Arguments.Add(TEXT("--id"));
			UnlockRequest.Arguments.Add(*LockId);

			const FGitProcessResult UnlockResult = ProcessRunner->Run(UnlockRequest);
			if (UnlockResult.ExitCode != 0)
			{
				OutError = BytesToTextUtf8Lossy(UnlockResult.StdErr).TrimStartAndEnd();
				return false;
			}
		}

		return true;
	}
}
