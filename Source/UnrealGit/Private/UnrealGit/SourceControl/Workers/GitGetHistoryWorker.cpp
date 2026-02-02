// Copyright Epic Games, Inc. All Rights Reserved.

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
		FString Relative;
		if (!UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
			return;
		}

		FGitProcessRequest LogRequest;
		LogRequest.WorkingDirectory = RepoRoot;
		LogRequest.Arguments =
		{
			TEXT("--no-pager"),
			TEXT("log"),
			TEXT("--follow"),
			TEXT("--date=iso-strict"),
			TEXT("--name-status"),
			TEXT("-z"),
			TEXT("--pretty=format:%x1e%H%x1f%an%x1f%ae%x1f%ad%x1f%s"),
			TEXT("--"),
			Relative,
		};

		const FGitProcessResult LogResult = ProcessRunner->Run(LogRequest);
		if (LogResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(LogResult.StdErr));
			return;
		}

		TArray<FGitFileRevision> Revisions;
		FString ParseError;
		if (!FGitFileHistoryParser::Parse(LogResult.StdOut, Relative, Revisions, ParseError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ParseError);
			return;
		}

		OutOutput.FileHistoryByAbsoluteFile.Add(FPaths::ConvertRelativePathToFull(AbsolutePath), MoveTemp(Revisions));
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
}
