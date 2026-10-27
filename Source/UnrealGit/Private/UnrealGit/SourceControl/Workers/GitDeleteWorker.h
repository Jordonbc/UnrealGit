// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

/**
 * @class FGitDeleteWorker
 * Removes the requested files from the index and working tree (`git rm --ignore-unmatch`).
 */
class UNREALGIT_API FGitDeleteWorker final : public IGitSourceControlWorker
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