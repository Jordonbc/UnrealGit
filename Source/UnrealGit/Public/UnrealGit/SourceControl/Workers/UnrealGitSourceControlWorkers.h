// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"

class IGitProcessRunner;
class ISourceControlOperation;

struct FGitStatusSnapshot;

/**
 * @struct FUnrealGitWorkerOutput
 * Aggregated output produced by a background source control worker.
 */
struct FUnrealGitWorkerOutput final
{
	bool bSuccess = false;
	FText ErrorText;

	TOptional<FGitStatusSnapshot> StatusSnapshot;
	TOptional<FString> RepoRoot;

	bool bShouldUpdateStatus = false;
	TArray<FString> FilesToUpdateStatus;

	TMap<FString, TArray<FGitFileRevision>> FileHistoryByAbsoluteFile;

	bool bHasLfsLocks = false;
	TArray<FGitLfsLock> LfsLocks;
};

/**
 * @class IGitSourceControlWorker
 * Interface implemented by background workers that execute source control operations.
 */
class IGitSourceControlWorker
{
public:
	virtual ~IGitSourceControlWorker() = default;

	/** Returns the operation name identifying this worker. */
	virtual FName GetName() const = 0;

	/**
	 * Executes the worker on a background thread.
	 *
	 * @param ProcessRunner Process runner used to invoke Git.
	 * @param Settings Validated provider settings snapshot.
	 * @param WorkingDirectoryHint Hint used to locate the repository.
	 * @param bQueryLfsLocks Whether to query Git LFS locks.
	 * @param Operation Source control operation being executed.
	 * @param Files Files affected by the operation.
	 * @param CurrentRepoRoot Current repository root path.
	 * @param OutOutput Aggregated output populated by the worker.
	 */
	virtual void Execute(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FUnrealGitProviderSettings& Settings,
		const FString& WorkingDirectoryHint,
		bool bQueryLfsLocks,
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& Operation,
		const TArray<FString>& Files,
		const FString& CurrentRepoRoot,
		FUnrealGitWorkerOutput& OutOutput) = 0;
};

