// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitForceUnlockWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitForceUnlockWorker::GetName() const
{
	return "ForceUnlock";
}

void FGitForceUnlockWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& Settings,
	const FString& WorkingDirectoryHint,
	bool /*bQueryLfsLocks*/,
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*Operation*/,
	const TArray<FString>& Files,
	const FString& CurrentRepoRoot,
	FUnrealGitWorkerOutput& OutOutput)
{
	OutOutput = FUnrealGitWorkerOutput();

	if (!Settings.bEnableLfsLocks)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Git LFS locks are disabled in settings."));
		return;
	}

	if (Files.Num() == 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("No files specified."));
		return;
	}

	FString RepoRoot = CurrentRepoRoot;
	FString RepoRootError;
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot, &RepoRootError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty() ? TEXT("Git repository root could not be determined.") : RepoRootError);
		return;
	}

	TArray<FString> RelativePaths;
	FString ConvertError;
	if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(ConvertError);
		return;
	}

	FString UnlockError;
	if (!UnrealGit::Workers::UnlockLfsLocks(ProcessRunner, RepoRoot, RelativePaths, true, UnlockError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnlockError);
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
