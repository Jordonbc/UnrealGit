// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitGetHistoryWorker.h"

#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitGetHistoryWorker::GetName() const
{
	return "GetHistory";
}

void FGitGetHistoryWorker::Execute(
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

	for (const FString& AbsolutePath : Files)
	{
		TArray<FGitFileRevision> Revisions;
		FString ParseError;
		if (!UnrealGit::Workers::FetchFileHistory(ProcessRunner, RepoRoot, AbsolutePath, Revisions, ParseError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ParseError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ParseError);
			return;
		}

		OutOutput.FileHistoryByAbsoluteFile.Add(FPaths::ConvertRelativePathToFull(AbsolutePath), MoveTemp(Revisions));
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
}
