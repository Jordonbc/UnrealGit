// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitRevParseParser.h"
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
}
