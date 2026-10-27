// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCopyWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitProcessUtils.h"

FName FGitCopyWorker::GetName() const
{
	return "Copy";
}

void FGitCopyWorker::Execute(
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
	if (!UnrealGit::Workers::EnsureRepoRootOrFail(ProcessRunner, WorkingDirectoryHint, RepoRoot, OutOutput))
	{
		return;
	}

	if (Files.Num() == 0)
	{
		UnrealGit::Workers::CompleteWorkerSuccess(OutOutput, RepoRoot, Files, false);
		return;
	}

	if (!UnrealGit::Workers::RunGitForFiles(ProcessRunner, RepoRoot, { TEXT("add") }, Files, OutOutput))
	{
		return;
	}

	UnrealGit::Workers::CompleteWorkerSuccess(OutOutput, RepoRoot, Files);
}