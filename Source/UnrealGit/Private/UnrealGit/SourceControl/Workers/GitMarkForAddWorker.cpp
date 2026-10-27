// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitMarkForAddWorker.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

void FGitMarkForAddWorker::Execute(
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

	TArray<FString> RelativePaths;
	FString ConvertError;
	if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(ConvertError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ConvertError);
		return;
	}

	FString AddError;
	if (!UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, { TEXT("add"), TEXT("-N") }, RelativePaths, AddError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(AddError);
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
