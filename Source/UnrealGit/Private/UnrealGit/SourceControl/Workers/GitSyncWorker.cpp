// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitSyncWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitProcessUtils.h"

FName FGitSyncWorker::GetName() const
{
	return "Sync";
}

void FGitSyncWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& Settings,
	const FString& WorkingDirectoryHint,
	bool /*bQueryLfsLocks*/,
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*Operation*/,
	const TArray<FString>& /*Files*/,
	const FString& CurrentRepoRoot,
	FUnrealGitWorkerOutput& OutOutput)
{
	OutOutput = FUnrealGitWorkerOutput();

	FString RepoRoot = CurrentRepoRoot;
	if (!UnrealGit::Workers::EnsureRepoRootOrFail(ProcessRunner, WorkingDirectoryHint, RepoRoot, OutOutput))
	{
		return;
	}

	FGitProcessRequest PullRequest;
	PullRequest.WorkingDirectory = FString();
	PullRequest.RepoRoot = RepoRoot;
	PullRequest.Arguments = Settings.bPullRebase
		? TArray<FString>({ TEXT("pull"), TEXT("--rebase"), TEXT("--no-stat") })
		: TArray<FString>({ TEXT("pull"), TEXT("--no-stat") });

	const FGitProcessResult PullResult = ProcessRunner->Run(PullRequest);
	if (PullResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = UnrealGit::Workers::MakeGitErrorText(PullResult);
		return;
	}

	UnrealGit::Workers::CompleteWorkerSuccess(OutOutput, RepoRoot, TArray<FString>());
}
