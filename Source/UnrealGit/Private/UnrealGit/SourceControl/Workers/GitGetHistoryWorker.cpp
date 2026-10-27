// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitGetHistoryWorker.h"

#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitHistoryUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"

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
	if (!UnrealGit::Workers::EnsureRepoRootOrFail(ProcessRunner, WorkingDirectoryHint, RepoRoot, OutOutput))
	{
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
