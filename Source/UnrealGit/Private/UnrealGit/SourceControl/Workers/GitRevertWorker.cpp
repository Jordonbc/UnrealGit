// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitRevertWorker::GetName() const
{
	return "Revert";
}

void FGitRevertWorker::Execute(
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
		FGitProcessRequest RestoreRequest;
		RestoreRequest.WorkingDirectory = FString();
		RestoreRequest.RepoRoot = RepoRoot;
		RestoreRequest.Arguments = {
			TEXT("restore"),
			TEXT("--source=HEAD"),
			TEXT("--staged"),
			TEXT("--worktree"),
			TEXT("--ignore-unmatch"),
			TEXT("--"),
			TEXT(".")
		};

		const FGitProcessResult RestoreResult = ProcessRunner->Run(RestoreRequest);
		if (RestoreResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(RestoreResult.StdErr));
			return;
		}
	}
	else
	{
		TArray<FString> RelativePaths;
		FString ConvertError;
		if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ConvertError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ConvertError);
			return;
		}

		FString RestoreError;
		if (!UnrealGit::Workers::RunGitWithBatchedPaths(
			ProcessRunner,
			RepoRoot,
			{ TEXT("restore"), TEXT("--source=HEAD"), TEXT("--staged"), TEXT("--worktree"), TEXT("--ignore-unmatch") },
			RelativePaths,
			RestoreError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(RestoreError);
			return;
		}
	}

	if (Settings.bEnableLfsLocks && Files.Num() > 0)
	{
		// Reverting discards the changes, so release the LFS lock acquired by checkout.
		TArray<FString> RelativePaths;
		FString ConvertError;
		if (UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
		{
			FString UnlockError;
			UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, { TEXT("lfs"), TEXT("unlock") }, RelativePaths, UnlockError);
		}
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
