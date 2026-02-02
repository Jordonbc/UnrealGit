// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitRevertWorker::GetName() const
{
	return "Revert";
}

void FGitRevertWorker::Execute(
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
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Git repository root could not be determined."));
		return;
	}

	FGitProcessRequest RestoreRequest;
	RestoreRequest.WorkingDirectory = RepoRoot;
	RestoreRequest.Arguments = { TEXT("restore"), TEXT("--staged"), TEXT("--worktree"), TEXT("--") };

	for (const FString& AbsolutePath : Files)
	{
		FString Relative;
		if (!UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
			return;
		}
		RestoreRequest.Arguments.Add(Relative);
	}

	const FGitProcessResult RestoreResult = ProcessRunner->Run(RestoreRequest);
	if (RestoreResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(RestoreResult.StdErr));
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
