// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

class FGitSyncWorker final : public IGitSourceControlWorker
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
