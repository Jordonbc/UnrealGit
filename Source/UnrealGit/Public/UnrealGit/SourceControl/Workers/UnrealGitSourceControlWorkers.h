// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"

class IGitProcessRunner;
class ISourceControlOperation;

struct FGitStatusSnapshot;

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

class IGitSourceControlWorker
{
public:
	virtual ~IGitSourceControlWorker() = default;

	virtual FName GetName() const = 0;

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

