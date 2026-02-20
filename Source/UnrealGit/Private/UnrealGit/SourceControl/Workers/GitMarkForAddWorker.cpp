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

	FGitProcessRequest AddRequest;
	AddRequest.WorkingDirectory = FString();
	AddRequest.RepoRoot = RepoRoot;
	AddRequest.Arguments = {
		TEXT("add"),
		TEXT("-N"),
		TEXT("--")
	};

	for (const FString& AbsolutePath : Files)
	{
		FString Relative;
		if (!UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
			return;
		}
		AddRequest.Arguments.Add(Relative);
	}

	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("MarkForAdd: Running git add with WorkingDirectory='%s', args='%s'"), 
		*AddRequest.WorkingDirectory, *FString::Join(AddRequest.Arguments, TEXT(" ")));

	const FGitProcessResult AddResult = ProcessRunner->Run(AddRequest);
	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("MarkForAdd: git add result: exit=%d, stdout='%s', stderr='%s'"), 
		AddResult.ExitCode, 
		*UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdOut).Left(200),
		*UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdErr).Left(200));

	if (AddResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdErr));
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
