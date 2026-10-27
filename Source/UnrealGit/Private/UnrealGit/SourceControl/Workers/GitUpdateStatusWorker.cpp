// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"

#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
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
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& Operation,
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

	// Use pathspecs when specific files are requested to reduce work.
	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Received Files.Num()=%d"), Files.Num());

	TArray<FString> RepoRelativePaths;
	if (Files.Num() > 0)
	{
		RepoRelativePaths.Reserve(Files.Num());

		for (const FString& FilePath : Files)
		{
			FString Relative;
			if (UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, FilePath, Relative))
			{
				RepoRelativePaths.Add(Relative);
			}
		}

		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Converted to %d relative paths"), RepoRelativePaths.Num());
	}

	FGitStatusSnapshot Snapshot;
	FString ParseError;

	const auto BaseStatusArguments = []() {
		return TArray<FString>{
			TEXT("status"),
			TEXT("--porcelain=v2"),
			TEXT("-z"),
			TEXT("--branch"),
			TEXT("--untracked-files=all"),
			TEXT("--ignored=matching"),
		};
	};

	bool bStatusOk = true;
	FString StatusErrorText;
	bool bBranchSet = false;

	auto RunStatusBatch = [&](const TArray<FString>& BatchPaths)
	{
		if (!bStatusOk)
		{
			return;
		}

		FGitProcessRequest StatusRequest;
		StatusRequest.WorkingDirectory = FString();
		StatusRequest.RepoRoot = RepoRoot;
		StatusRequest.Arguments = BaseStatusArguments();

		if (BatchPaths.Num() > 0)
		{
			StatusRequest.Arguments.Add(TEXT("--"));
			for (const FString& Relative : BatchPaths)
			{
				StatusRequest.Arguments.Add(Relative);
			}
		}

		const FGitProcessResult StatusResult = ProcessRunner->Run(StatusRequest);
		UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatus: git status batch finished: exit=%d, bytes=%d"),
			StatusResult.ExitCode, StatusResult.StdOut.Num());
		if (StatusResult.ExitCode != 0)
		{
			bStatusOk = false;
			const FString StdErr = UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdErr).TrimStartAndEnd();
			const FString StdOut = UnrealGit::Workers::BytesToTextUtf8Lossy(StatusResult.StdOut).TrimStartAndEnd();
			StatusErrorText = !StdErr.IsEmpty() ? StdErr : StdOut;
			return;
		}

		FGitStatusSnapshot BatchSnapshot;
		FString BatchParseError;
		if (!FGitStatusParser::ParsePorcelainV2Z(StatusResult.StdOut, BatchSnapshot, BatchParseError))
		{
			bStatusOk = false;
			StatusErrorText = BatchParseError;
			return;
		}

		if (!bBranchSet)
		{
			Snapshot.Branch = BatchSnapshot.Branch;
			bBranchSet = true;
		}

		for (FGitFileStatus& FileStatus : BatchSnapshot.Files)
		{
			Snapshot.Files.Add(MoveTemp(FileStatus));
		}
	};

	if (RepoRelativePaths.Num() > 0)
	{
		UnrealGit::Workers::ForEachPathBatch(RepoRelativePaths, UnrealGit::Workers::MaxFilesPerCommand, [&](const TArray<FString>& Batch)
		{
			RunStatusBatch(Batch);
		});
	}
	else
	{
		TArray<FString> EmptyBatch;
		RunStatusBatch(EmptyBatch);
	}

	if (!bStatusOk)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(StatusErrorText);
		return;
	}

	TSet<FString> FoundRelativePaths;
	for (const FGitFileStatus& FileStatus : Snapshot.Files)
	{
		FoundRelativePaths.Add(FileStatus.RelativePath);
	}

	if (Files.Num() == 0)
	{
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Full repo query - fetching all tracked files with git ls-files"));
		FGitProcessRequest LsFilesRequest;
		LsFilesRequest.WorkingDirectory = FString();
		LsFilesRequest.RepoRoot = RepoRoot;
		LsFilesRequest.Arguments = {
			TEXT("ls-files"),
			TEXT("-z"),
		};

		const FGitProcessResult LsFilesResult = ProcessRunner->Run(LsFilesRequest);
		if (LsFilesResult.ExitCode == 0)
		{
			TArray<FString> AllTrackedPaths = UnrealGit::Workers::ParseNullDelimitedList(LsFilesResult.StdOut);
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: git ls-files found %d tracked files"), AllTrackedPaths.Num());

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
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Added %d Unchanged entries from ls-files"), AddedCount);
		}
	}

	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Parsed %d files from git status"), Snapshot.Files.Num());

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
	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: %d files not in status, checking if tracked"), RepoRelativePathsNotInStatus.Num());

	if (RepoRelativePathsNotInStatus.Num() > 0)
	{
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Running git ls-files for %d files"), RepoRelativePathsNotInStatus.Num());
		FGitProcessRequest LsFilesRequest;
		LsFilesRequest.WorkingDirectory = FString();
		LsFilesRequest.RepoRoot = RepoRoot;
		LsFilesRequest.Arguments = {
			TEXT("ls-files"),
			TEXT("-z"),
		};
		LsFilesRequest.Arguments.Add(TEXT("--"));
		for (const FString& Relative : RepoRelativePathsNotInStatus)
		{
			LsFilesRequest.Arguments.Add(Relative);
		}

		const FGitProcessResult LsFilesResult = ProcessRunner->Run(LsFilesRequest);
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: git ls-files result: exit=%d, stdout_len=%d"), LsFilesResult.ExitCode, LsFilesResult.StdOut.Num());
		if (LsFilesResult.ExitCode == 0)
		{
			TArray<FString> TrackedPaths = UnrealGit::Workers::ParseNullDelimitedList(LsFilesResult.StdOut);

			TSet<FString> TrackedSet;
			for (const FString& Path : TrackedPaths)
			{
				TrackedSet.Add(Path);
			}
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: ls-files found %d tracked paths"), TrackedSet.Num());

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
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Added %d Unchanged entries to snapshot"), UnchangedCount);
		}
		else
		{
			UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: git ls-files failed with exit code %d"), LsFilesResult.ExitCode);
		}
	}

	// Attach merge (resolve) data to conflicted files using `git ls-files --unmerged`.
	TSet<FString> ConflictedPathSet;
	for (const FGitFileStatus& FileStatus : Snapshot.Files)
	{
		if (FileStatus.bIsConflicted || FileStatus.State == EGitFileState::Conflicted)
		{
			ConflictedPathSet.Add(FileStatus.RelativePath);
		}
	}

	TArray<FString> ConflictedPaths = ConflictedPathSet.Array();
	if (ConflictedPaths.Num() > 0)
	{
		UnrealGit::Workers::ForEachPathBatch(ConflictedPaths, 50, [&](const TArray<FString>& Batch)
		{
			FGitProcessRequest UnmergedRequest;
			UnmergedRequest.WorkingDirectory = FString();
			UnmergedRequest.RepoRoot = RepoRoot;
			UnmergedRequest.Arguments = {
				TEXT("ls-files"),
				TEXT("--unmerged"),
				TEXT("-z"),
			};
			UnmergedRequest.Arguments.Add(TEXT("--"));
			for (const FString& Relative : Batch)
			{
				UnmergedRequest.Arguments.Add(Relative);
			}

			const FGitProcessResult UnmergedResult = ProcessRunner->Run(UnmergedRequest);
			if (UnmergedResult.ExitCode != 0)
			{
				UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: git ls-files --unmerged failed with exit code %d"), UnmergedResult.ExitCode);
				return;
			}

			TMap<FString, FGitResolveInfo> ResolveInfoMap;
			FString UnmergedError;
			if (!FGitStatusParser::ParseUnmergedZ(UnmergedResult.StdOut, ResolveInfoMap, UnmergedError))
			{
				UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Failed to parse unmerged output: %s"), *UnmergedError);
				return;
			}

			for (const auto& Pair : ResolveInfoMap)
			{
				for (FGitFileStatus& Status : Snapshot.Files)
				{
					if (Status.RelativePath == Pair.Key)
					{
						Status.ResolveInfo = Pair.Value;
						break;
					}
				}
			}
		});
	}

	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: Final snapshot has %d files"), Snapshot.Files.Num());
	OutOutput.bSuccess = true;
	OutOutput.StatusSnapshot = MoveTemp(Snapshot);
	OutOutput.RepoRoot = RepoRoot;

	const bool bUpdateHistory = (Operation->GetName() == "UpdateStatus")
		&& static_cast<const FUpdateStatus*>(&Operation.Get())->ShouldUpdateHistory();

	if (bUpdateHistory)
	{
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatus: UpdateHistory requested, fetching file history for %d files"), Files.Num());
		for (const FString& AbsolutePath : Files)
		{
			TArray<FGitFileRevision> Revisions;
			FString HistoryParseError;
			if (UnrealGit::Workers::FetchFileHistory(ProcessRunner, RepoRoot, AbsolutePath, Revisions, HistoryParseError))
			{
				OutOutput.FileHistoryByAbsoluteFile.Add(FPaths::ConvertRelativePathToFull(AbsolutePath), MoveTemp(Revisions));
			}
			else
			{
				UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: Failed to fetch history for '%s': %s"), *AbsolutePath, *HistoryParseError);
			}
		}
	}

	if (Settings.bEnableLfsLocks && bQueryLfsLocks)
	{
		FGitProcessRequest LocksRequest;
		LocksRequest.WorkingDirectory = FString();
		LocksRequest.RepoRoot = RepoRoot;
		LocksRequest.Arguments = { TEXT("lfs"), TEXT("locks"), TEXT("--json") };

		const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
		if (LocksResult.ExitCode != 0)
		{
			UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: git lfs locks failed (exit %d): %s"),
				LocksResult.ExitCode,
				*UnrealGit::Workers::BytesToTextUtf8Lossy(LocksResult.StdErr).TrimStartAndEnd());
		}
		else if (LocksResult.ExitCode == 0)
		{
			TArray<FGitLfsLock> Locks;
			FString LocksError;
			const FString LocksText = UnrealGit::Workers::BytesToTextUtf8Lossy(LocksResult.StdOut);
			if (FGitLfsLocksParser::ParseJson(LocksText, Locks, LocksError))
			{
				UE_LOG(LogUnrealGit, Log, TEXT("UpdateStatus: git lfs locks returned %d lock(s)"), Locks.Num());
				OutOutput.bHasLfsLocks = true;
				OutOutput.LfsLocks = MoveTemp(Locks);
			}
			else
			{
				UE_LOG(LogUnrealGit, Warning, TEXT("UpdateStatus: failed to parse git lfs locks: %s"), *LocksError);
			}
		}
	}
}
