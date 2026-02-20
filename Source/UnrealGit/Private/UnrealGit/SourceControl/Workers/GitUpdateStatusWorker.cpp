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
	UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Received Files.Num()=%d"), Files.Num());
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
		}

		UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Converted to %d relative paths"), RepoRelativePaths.Num());

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

	TSet<FString> FoundRelativePaths;
	for (const FGitFileStatus& FileStatus : Snapshot.Files)
	{
		FoundRelativePaths.Add(FileStatus.RelativePath);
	}

	if (Files.Num() == 0)
	{
		UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Full repo query - fetching all tracked files with git ls-files"));
		FGitProcessRequest LsFilesRequest;
		LsFilesRequest.WorkingDirectory = FString();
		LsFilesRequest.Arguments = {
			TEXT("-C"), *RepoRoot,
			TEXT("ls-files"),
			TEXT("-z"),
		};

		const FGitProcessResult LsFilesResult = ProcessRunner->Run(LsFilesRequest);
		if (LsFilesResult.ExitCode == 0)
		{
			TArray<FString> AllTrackedPaths = UnrealGit::Workers::ParseNullDelimitedList(LsFilesResult.StdOut);
			UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: git ls-files found %d tracked files"), AllTrackedPaths.Num());

			TSet<FString> TrackedSet;
			for (const FString& Path : AllTrackedPaths)
			{
				TrackedSet.Add(Path);
			}

			int32 AddedCount = 0;
			for (const FString& TrackedPath : AllTrackedPaths)
			{
				if (!FoundRelativePaths.Contains(TrackedPath))
				{
					FGitFileStatus UnchangedStatus;
					UnchangedStatus.RelativePath = TrackedPath;
					UnchangedStatus.State = EGitFileState::Unchanged;
					UnchangedStatus.bIsTracked = true;
					UnchangedStatus.bIsStaged = false;
					UnchangedStatus.bIsUnstaged = false;
					UnchangedStatus.bIsConflicted = false;
					Snapshot.Files.Add(MoveTemp(UnchangedStatus));
					AddedCount++;
				}
			}
			UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Added %d Unchanged entries from ls-files"), AddedCount);
		}
	}

	UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Parsed %d files from git status"), Snapshot.Files.Num());

	TArray<FString> RepoRelativePathsNotInStatus;
	if (Files.Num() > 0)
	{
		RepoRelativePathsNotInStatus.Reserve(Files.Num());
		for (const FString& FilePath : Files)
		{
			FString Relative;
			if (UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, FilePath, Relative))
			{
				if (!FoundRelativePaths.Contains(Relative))
				{
					RepoRelativePathsNotInStatus.Add(Relative);
				}
			}
		}
	}
	UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: %d files not in status, checking if tracked"), RepoRelativePathsNotInStatus.Num());

	if (RepoRelativePathsNotInStatus.Num() > 0)
	{
		UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Running git ls-files for %d files"), RepoRelativePathsNotInStatus.Num());
		FGitProcessRequest LsFilesRequest;
		LsFilesRequest.WorkingDirectory = FString();
		LsFilesRequest.Arguments = {
			TEXT("-C"), *RepoRoot,
			TEXT("ls-files"),
			TEXT("-z"),
		};
		LsFilesRequest.Arguments.Add(TEXT("--"));
		for (const FString& Relative : RepoRelativePathsNotInStatus)
		{
			LsFilesRequest.Arguments.Add(Relative);
		}

		const FGitProcessResult LsFilesResult = ProcessRunner->Run(LsFilesRequest);
		UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: git ls-files result: exit=%d, stdout_len=%d"), LsFilesResult.ExitCode, LsFilesResult.StdOut.Num());
		if (LsFilesResult.ExitCode == 0)
		{
			TArray<FString> TrackedPaths = UnrealGit::Workers::ParseNullDelimitedList(LsFilesResult.StdOut);

			TSet<FString> TrackedSet;
			for (const FString& Path : TrackedPaths)
			{
				TrackedSet.Add(Path);
			}
			UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: ls-files found %d tracked paths"), TrackedSet.Num());

			int32 UnchangedCount = 0;
			for (const FString& Relative : RepoRelativePathsNotInStatus)
			{
				if (TrackedSet.Contains(Relative))
				{
					FGitFileStatus UnchangedStatus;
					UnchangedStatus.RelativePath = Relative;
					UnchangedStatus.State = EGitFileState::Unchanged;
					UnchangedStatus.bIsTracked = true;
					UnchangedStatus.bIsStaged = false;
					UnchangedStatus.bIsUnstaged = false;
					UnchangedStatus.bIsConflicted = false;
					Snapshot.Files.Add(MoveTemp(UnchangedStatus));
					UnchangedCount++;
				}
			}
			UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Added %d Unchanged entries to snapshot"), UnchangedCount);
		}
		else
		{
			UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: git ls-files failed with exit code %d"), LsFilesResult.ExitCode);
		}
	}

	UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: Final snapshot has %d files"), Snapshot.Files.Num());
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
