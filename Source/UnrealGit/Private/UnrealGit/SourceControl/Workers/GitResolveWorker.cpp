// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitResolveWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitResolveWorker::GetName() const
{
	return "Resolve";
}

void FGitResolveWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& /*Settings*/,
	const FString& WorkingDirectoryHint,
	bool /*bQueryLfsLocks*/,
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*Operation*/,
	const TArray<FString>& Files,
	const FString& CurrentRepoRoot,
	FUnrealGitWorkerOutput& OutOutput)
{
	OutOutput = FUnrealGitWorkerOutput();

	FString RepoRoot = CurrentRepoRoot;
	FString RepoRootError;
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot, &RepoRootError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty() ? TEXT("Git repository root could not be determined.") : RepoRootError);
		return;
	}

	if (Files.Num() == 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("No files specified."));
		return;
	}

	TArray<FString> Relative;
	FString ConversionError;
	if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, Relative, ConversionError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(ConversionError);
		return;
	}

	FString GitError;
	if (!UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, { TEXT("add") }, Relative, GitError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(GitError);
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}