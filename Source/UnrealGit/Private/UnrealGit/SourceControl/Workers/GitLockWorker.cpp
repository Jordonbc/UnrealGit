// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitLockWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitLockWorker::GetName() const
{
	return "Lock";
}

void FGitLockWorker::Execute(
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

	FString RepoRoot = CurrentRepoRoot;
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Git repository root could not be determined."));
		return;
	}

	for (const FString& AbsolutePath : Files)
	{
		FString Relative;
		if (!UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
			return;
		}

		FGitProcessRequest LockRequest;
		LockRequest.WorkingDirectory = RepoRoot;
		LockRequest.Arguments = { TEXT("lfs"), TEXT("lock"), TEXT("--"), Relative };

		const FGitProcessResult LockResult = ProcessRunner->Run(LockRequest);
		if (LockResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(LockResult.StdErr));
			return;
		}
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
