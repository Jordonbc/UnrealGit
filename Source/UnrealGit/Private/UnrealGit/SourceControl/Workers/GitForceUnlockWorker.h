// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

/**
 * @class FGitForceUnlockWorker
 * Force-unlocks Git LFS locks for the requested files, regardless of owner.
 */
class UNREALGIT_API FGitForceUnlockWorker final : public IGitSourceControlWorker
{
public:
	virtual FName GetName() const override;

	virtual void Execute(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FUnrealGitProviderSettings& Settings,
		const FString& WorkingDirectoryHint,
		bool bQueryLfsLocks,
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& Operation,
		const TArray<FString>& Files,
		const FString& CurrentRepoRoot,
		FUnrealGitWorkerOutput& OutOutput) override;
};
