// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"

#include "ISourceControlOperation.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"
#include "UnrealGit/Git/Parsers/GitStatusParser.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

void FGitUpdateStatusWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& Settings,
	const FString& WorkingDirectoryHint,
	bool bQueryLfsLocks,
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
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty()
			? TEXT("Git repository root could not be determined. Ensure the project is inside a Git worktree.")
			: RepoRootError);
		return;
	}

	FGitProcessRequest StatusRequest;
	StatusRequest.WorkingDirectory = FString();
	StatusRequest.Arguments = {
		TEXT("-C"), *RepoRoot,
		TEXT("status"),
		TEXT("--porcelain=v2"),
		TEXT("-z"),
		TEXT("--branch"),
		TEXT("--untracked-files=all"),
		TEXT("--ignored=matching"),
	};

	// Use pathspecs when specific files are requested to reduce work.
	if (Files.Num() > 0)
	{
		TArray<FString> RepoRelativePaths;
		RepoRelativePaths.Reserve(Files.Num());

		for (const FString& FilePath : Files)
		{
			FString Relative;
			if (UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, FilePath, Relative))
			{
				RepoRelativePaths.Add(Relative);
			}
			else
			{
				UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: Skipping file outside repository root: %s"), *FPaths::ConvertRelativePathToFull(FilePath));
			}
		}

		if (RepoRelativePaths.Num() > 0)
		{
			StatusRequest.Arguments.Add(TEXT("--"));
			for (const FString& Relative : RepoRelativePaths)
			{
				StatusRequest.Arguments.Add(Relative);
			}
		}
	}

	const FGitProcessResult StatusResult = ProcessRunner->Run(StatusRequest);
	UE_LOG(LogUnrealGit, Log, TEXT("UpdateStatus: git status result: exit=%d, stdout_len=%d, stdout='%s', stderr='%s'"), 
		StatusResult.ExitCode, StatusResult.StdOut.Num(), 
		*UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdOut).Left(500),
		*UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdErr));
	if (StatusResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		const FString StdErr = UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdErr).TrimStartAndEnd();
		const FString StdOut = UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdOut).TrimStartAndEnd();
		OutOutput.ErrorText = FText::FromString(!StdErr.IsEmpty() ? StdErr : StdOut);
		return;
	}

	FGitStatusSnapshot Snapshot;
	FString ParseError;
	if (!FGitStatusParser::ParsePorcelainV2Z(StatusResult.StdOut, Snapshot, ParseError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(ParseError);
		return;
	}

	OutOutput.bSuccess = true;
	OutOutput.StatusSnapshot = MoveTemp(Snapshot);
	OutOutput.RepoRoot = RepoRoot;

	if (Settings.bEnableLfsLocks && bQueryLfsLocks)
	{
		FGitProcessRequest LocksRequest;
		LocksRequest.WorkingDirectory = FString();
		LocksRequest.Arguments = { TEXT("-C"), *RepoRoot, TEXT("lfs"), TEXT("locks"), TEXT("--json") };

		const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
		if (LocksResult.ExitCode == 0)
		{
			TArray<FGitLfsLock> Locks;
			FString LocksError;
			const FString LocksText = UnrealGit::Workers::BytesToTextUtf8Lossy(LocksResult.StdOut);
			if (FGitLfsLocksParser::ParseJson(LocksText, Locks, LocksError))
			{
				OutOutput.bHasLfsLocks = true;
				OutOutput.LfsLocks = MoveTemp(Locks);
			}
		}
	}
}
