// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitRevParseParser.h"
#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

namespace UnrealGit::Workers
{
	inline FString BytesToTextUtf8Lossy(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() == 0)
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converter.Length(), Converter.Get());
	}

	inline TArray<FString> ParseNullDelimitedList(const TArray<uint8>& Bytes)
	{
		TArray<FString> Results;

		if (Bytes.Num() == 0)
		{
			return Results;
		}

		int32 SegmentStart = 0;
		for (int32 i = 0; i < Bytes.Num(); ++i)
		{
			if (Bytes[i] == 0)
			{
				const int32 SegmentLength = i - SegmentStart;
				if (SegmentLength > 0)
				{
					FUTF8ToTCHAR Converter(
						reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + SegmentStart),
						SegmentLength);
					Results.Add(FString(Converter.Length(), Converter.Get()));
				}
				SegmentStart = i + 1;
			}
		}

		const int32 RemainingLength = Bytes.Num() - SegmentStart;
		if (RemainingLength > 0)
		{
			FUTF8ToTCHAR Converter(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + SegmentStart),
				RemainingLength);
			Results.Add(FString(Converter.Length(), Converter.Get()));
		}

		return Results;
	}

	/** Queries `git ls-files` and returns the tracked paths as a set. */
	inline bool QueryTrackedPaths(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const TArray<FString>& RelativePaths,
		TSet<FString>& OutTrackedPaths)
	{
		FGitProcessRequest Request;
		Request.WorkingDirectory = FString();
		Request.RepoRoot = RepoRoot;
		Request.Arguments = { TEXT("ls-files"), TEXT("-z") };

		if (RelativePaths.Num() > 0)
		{
			Request.Arguments.Add(TEXT("--"));
			Request.Arguments.Append(RelativePaths);
		}

		const FGitProcessResult Result = ProcessRunner->Run(Request);
		if (Result.ExitCode != 0)
		{
			return false;
		}

		for (const FString& Path : ParseNullDelimitedList(Result.StdOut))
		{
			OutTrackedPaths.Add(Path);
		}

		return true;
	}

	inline FGitProcessRequest MakeGitRequest(const FString& RepoRoot, const TArray<FString>& Arguments)
	{
		FGitProcessRequest Request;
		Request.RepoRoot = RepoRoot;
		Request.WorkingDirectory = FString();
		Request.Arguments = Arguments;
		return Request;
	}

	inline bool EnsureRepoRoot(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& WorkingDirectoryHint,
		FString& InOutRepoRoot,
		FString* OutError = nullptr)
	{
		if (!InOutRepoRoot.IsEmpty())
		{
			return true;
		}

		FString CurrentDir = WorkingDirectoryHint;
		if (CurrentDir.IsEmpty())
		{
			CurrentDir = FPaths::ProjectDir();
		}
		UE_LOG(LogUnrealGit, Verbose, TEXT("EnsureRepoRoot: Searching from directory: '%s'"), *CurrentDir);
		CurrentDir = FPaths::ConvertRelativePathToFull(CurrentDir);
		FPaths::NormalizeDirectoryName(CurrentDir);

		FGitProcessRequest RootRequest;
		RootRequest.WorkingDirectory = FString();
		RootRequest.Arguments = { TEXT("-C"), *CurrentDir, TEXT("rev-parse"), TEXT("--show-toplevel") };

		const FGitProcessResult RootResult = ProcessRunner->Run(RootRequest);
		const FString StdErr = BytesToTextUtf8Lossy(RootResult.StdErr).TrimStartAndEnd();
		const FString StdOut = BytesToTextUtf8Lossy(RootResult.StdOut);

		UE_LOG(LogUnrealGit, Verbose, TEXT("EnsureRepoRoot: git -C rev-parse result: exit=%d, stdout='%s', stderr='%s'"),
			RootResult.ExitCode, *StdOut.TrimStartAndEnd(), *StdErr);

		if (RootResult.ExitCode == 0 && FGitRevParseParser::ParseShowToplevel(StdOut, InOutRepoRoot))
		{
			return true;
		}

		if (OutError)
		{
			const FString Err = StdErr.IsEmpty()
				? FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (exit code: %d)."),
					*CurrentDir, RootResult.ExitCode)
				: FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (exit code: %d, stderr: %s)."),
					*CurrentDir, RootResult.ExitCode, *StdErr);
			*OutError = Err;
		}

		return false;
	}

	/** Normalizes path separators to forward slashes for git and lock-path comparisons. */
	inline FString NormalizePathSeparators(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
	}

	/** Ensures a repo root is resolved, writing the standard error message into the worker output on failure. */
	inline bool EnsureRepoRootOrFail(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& WorkingDirectoryHint,
		FString& InOutRepoRoot,
		FUnrealGitWorkerOutput& OutOutput,
		const TCHAR* DefaultErrorMessage = TEXT("Git repository root could not be determined."))
	{
		FString RepoRootError;
		if (EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, InOutRepoRoot, &RepoRootError))
		{
			return true;
		}

		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty() ? FString(DefaultErrorMessage) : RepoRootError);

		return false;
	}

	inline bool TryMakeRepoRelativePath(const FString& RepoRoot, const FString& AbsolutePath, FString& OutRelative)
	{
		if (RepoRoot.IsEmpty())
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - RepoRoot is empty. Absolute='%s'"), *AbsolutePath);
			return false;
		}

		const FString Abs = FPaths::ConvertRelativePathToFull(AbsolutePath);
		FString Rel = Abs;
		if (!FPaths::MakePathRelativeTo(Rel, *RepoRoot))
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - MakePathRelativeTo failed. Abs='%s', RepoRoot='%s'"), *Abs, *RepoRoot);
			return false;
		}

		FString RepoRootDirName = FPaths::GetCleanFilename(RepoRoot);
		if (!RepoRootDirName.IsEmpty() && Rel.StartsWith(RepoRootDirName + TEXT("/")))
		{
			Rel = Rel.RightChop(RepoRootDirName.Len() + 1);
		}

		if (Rel.IsEmpty() || Rel == TEXT("..") || Rel.StartsWith(TEXT("../")) || Rel.Contains(TEXT("/../")))
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - Validation failed. Rel='%s'"), *Rel);
			return false;
		}

		OutRelative = MoveTemp(Rel);
		return true;
	}

	/**
	 * Maximum number of paths passed to a single git invocation.
	 *
	 * Matches the engine's batching limit and keeps command lines below the
	 * Windows CreateProcess limit for large submits and status queries.
	 */
	static constexpr int32 MaxFilesPerCommand = 50;

	/** Converts absolute paths to repo-relative paths, failing on the first path outside the repository. */
	inline bool TryMakeRepoRelativePaths(
		const FString& RepoRoot,
		const TArray<FString>& AbsolutePaths,
		TArray<FString>& OutRelativePaths,
		FString& OutError)
	{
		OutRelativePaths.Reset();
		OutRelativePaths.Reserve(AbsolutePaths.Num());
		OutError.Reset();

		for (const FString& AbsolutePath : AbsolutePaths)
		{
			FString Relative;
			if (!TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
			{
				OutError = FString::Printf(TEXT("File is outside the Git repository root: %s"), *AbsolutePath);
				return false;
			}
			OutRelativePaths.Add(MoveTemp(Relative));
		}

		return true;
	}

	/** Invokes Func once per chunk of at most BatchSize paths. */
	template <typename FuncType>
	void ForEachPathBatch(const TArray<FString>& Paths, int32 BatchSize, FuncType&& Func)
	{
		const int32 EffectiveBatchSize = FMath::Max(1, BatchSize);
		for (int32 Start = 0; Start < Paths.Num(); Start += EffectiveBatchSize)
		{
			const int32 End = FMath::Min(Start + EffectiveBatchSize, Paths.Num());

			TArray<FString> Batch;
			Batch.Reserve(End - Start);
			for (int32 Index = Start; Index < End; ++Index)
			{
				Batch.Add(Paths[Index]);
			}

			Func(Batch);
		}
	}

	/**
	 * Runs a git command whose trailing arguments are a list of repo-relative paths,
	 * automatically batching them and separating them with "--".
	 *
	 * PrefixArguments must not include the "--" separator.
	 */
	inline bool RunGitWithBatchedPaths(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const TArray<FString>& PrefixArguments,
		const TArray<FString>& RelativePaths,
		FString& OutError)
	{
		OutError.Reset();

		if (RelativePaths.Num() == 0)
		{
			return true;
		}

		bool bSucceeded = true;
		ForEachPathBatch(RelativePaths, MaxFilesPerCommand, [&](const TArray<FString>& Batch)
		{
			if (!bSucceeded)
			{
				return;
			}

			FGitProcessRequest Request;
			Request.RepoRoot = RepoRoot;
			Request.WorkingDirectory = FString();
			Request.Arguments = PrefixArguments;
			Request.Arguments.Add(TEXT("--"));
			Request.Arguments.Append(Batch);

			const FGitProcessResult Result = ProcessRunner->Run(Request);
			if (Result.ExitCode != 0)
			{
				bSucceeded = false;
				OutError = BytesToTextUtf8Lossy(Result.StdErr).TrimStartAndEnd();
				if (OutError.IsEmpty())
				{
					OutError = FString::Printf(TEXT("git %s failed with exit code %d."),
						*FString::Join(PrefixArguments, TEXT(" ")), Result.ExitCode);
				}
			}
		});

		return bSucceeded;
	}

	/** Builds a worker error message from git's stderr. */
	inline FText MakeGitErrorText(const FGitProcessResult& Result)
	{
		return FText::FromString(BytesToTextUtf8Lossy(Result.StdErr).TrimStartAndEnd());
	}

	/** Fills the standard successful worker output (repo root + status refresh). */
	inline void CompleteWorkerSuccess(
		FUnrealGitWorkerOutput& OutOutput,
		const FString& RepoRoot,
		const TArray<FString>& Files,
		bool bShouldUpdateStatus = true)
	{
		OutOutput.bSuccess = true;
		OutOutput.RepoRoot = RepoRoot;
		OutOutput.bShouldUpdateStatus = bShouldUpdateStatus;
		OutOutput.FilesToUpdateStatus = Files;
	}

	/**
	 * Releases LFS locks for the given repo-relative paths by resolving lock IDs first.
	 *
	 * Path-based `git lfs unlock` re-resolves the lock itself and fails when that lookup
	 * does not match; passing the ID is reliable. Paths without a lock are skipped.
	 */
	inline bool UnlockLfsLocks(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const TArray<FString>& RelativePaths,
		bool bForce,
		FString& OutError)
	{
		OutError.Reset();
		if (RelativePaths.Num() == 0)
		{
			return true;
		}

		FGitProcessRequest LocksRequest;
		LocksRequest.WorkingDirectory = FString();
		LocksRequest.RepoRoot = RepoRoot;
		LocksRequest.Arguments = { TEXT("lfs"), TEXT("locks"), TEXT("--json") };

		const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
		if (LocksResult.ExitCode != 0)
		{
			OutError = BytesToTextUtf8Lossy(LocksResult.StdErr).TrimStartAndEnd();
			return false;
		}

		TArray<FGitLfsLock> Locks;
		FString ParseError;
		if (!FGitLfsLocksParser::ParseJson(BytesToTextUtf8Lossy(LocksResult.StdOut), Locks, ParseError))
		{
			OutError = ParseError;
			return false;
		}

		TMap<FString, FString> LockIdByPath;
		for (const FGitLfsLock& Lock : Locks)
		{
			LockIdByPath.Add(NormalizePathSeparators(Lock.Path), Lock.Id);
		}

		for (const FString& RelativePath : RelativePaths)
		{
			const FString NormalizedPath = NormalizePathSeparators(RelativePath);

			const FString* LockId = LockIdByPath.Find(NormalizedPath);
			if (!LockId || LockId->IsEmpty())
			{
				continue;
			}

			FGitProcessRequest UnlockRequest;
			UnlockRequest.WorkingDirectory = FString();
			UnlockRequest.RepoRoot = RepoRoot;
			UnlockRequest.Arguments = { TEXT("lfs"), TEXT("unlock") };
			if (bForce)
			{
				UnlockRequest.Arguments.Add(TEXT("--force"));
			}
			UnlockRequest.Arguments.Add(TEXT("--id"));
			UnlockRequest.Arguments.Add(*LockId);

			const FGitProcessResult UnlockResult = ProcessRunner->Run(UnlockRequest);
			if (UnlockResult.ExitCode != 0)
			{
				OutError = BytesToTextUtf8Lossy(UnlockResult.StdErr).TrimStartAndEnd();
				return false;
			}
		}

		return true;
	}

	inline bool TryFixCorruptedIndex(
		const TSharedPtr<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot)
	{
		const FString IndexPath = FPaths::Combine(RepoRoot, TEXT(".git"), TEXT("index"));
		if (!IFileManager::Get().FileExists(*IndexPath))
		{
			return false;
		}

		UE_LOG(LogUnrealGit, Warning, TEXT("Detected corrupted git index, attempting to rebuild..."));

		IFileManager::Get().Delete(*IndexPath, false, true, true);

		FGitProcessRequest Request;
		Request.RepoRoot = RepoRoot;
		Request.Arguments = { TEXT("reset") };

		const FGitProcessResult Result = ProcessRunner->Run(Request);
		if (Result.ExitCode == 0)
		{
			UE_LOG(LogUnrealGit, Log, TEXT("Successfully rebuilt git index"));
			return true;
		}

		UE_LOG(LogUnrealGit, Error, TEXT("Failed to rebuild git index: %s"), *BytesToTextUtf8Lossy(Result.StdErr));
		return false;
	}

	inline bool FetchFileHistory(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const FString& AbsolutePath,
		TArray<FGitFileRevision>& OutRevisions,
		FString& OutError)
	{
		OutRevisions.Reset();
		OutError.Reset();

		FString Relative;
		if (!TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutError = TEXT("File is outside the Git repository root.");
			return false;
		}

		FGitProcessRequest LogRequest;
		LogRequest.WorkingDirectory = FString();
		LogRequest.RepoRoot = RepoRoot;
		LogRequest.Arguments =
		{
			TEXT("--no-pager"),
			TEXT("log"),
			TEXT("--follow"),
			TEXT("--date=iso-strict"),
			TEXT("--name-status"),
			TEXT("-z"),
			TEXT("--max-count=1000"),
			TEXT("--pretty=format:%x1e%H%x1f%an%x1f%ae%x1f%ad%x1f%s%x0a"),
			TEXT("--"),
			Relative,
		};

		const FGitProcessResult LogResult = ProcessRunner->Run(LogRequest);
		if (LogResult.ExitCode != 0)
		{
			OutError = BytesToTextUtf8Lossy(LogResult.StdErr);
			return false;
		}

		if (!FGitFileHistoryParser::Parse(LogResult.StdOut, Relative, OutRevisions, OutError))
		{
			return false;
		}

		if (OutRevisions.Num() > 0)
		{
			FString StdInStr;
			for (const FGitFileRevision& Revision : OutRevisions)
			{
				StdInStr += FString::Printf(TEXT("%s:%s\n"), *Revision.Revision.CommitId, *Revision.RepoRelativePathAtRevision);
			}

			FTCHARToUTF8 Utf8(*StdInStr);
			FGitProcessRequest SizeRequest;
			SizeRequest.RepoRoot = RepoRoot;
			SizeRequest.WorkingDirectory = FString();
			SizeRequest.Arguments = { TEXT("cat-file"), TEXT("--batch-check") };
			SizeRequest.StdIn.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

			const FGitProcessResult SizeResult = ProcessRunner->Run(SizeRequest);
			if (SizeResult.ExitCode == 0)
			{
				const FString SizeStdOut = BytesToTextUtf8Lossy(SizeResult.StdOut);
				FGitFileHistoryParser::ParseCatFileBatchCheckOutput(SizeStdOut, OutRevisions);
			}
		}

		return true;
	}
}
