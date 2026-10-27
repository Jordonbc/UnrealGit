// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

namespace UnrealGit::Workers
{
	inline FGitProcessRequest MakeGitRequest(const FString& RepoRoot, const TArray<FString>& Arguments)
	{
		FGitProcessRequest Request;
		Request.RepoRoot = RepoRoot;
		Request.WorkingDirectory = FString();
		Request.Arguments = Arguments;
		return Request;
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
	 * Converts absolute paths to repo-relative, runs the given git subcommand in
	 * batched invocations, and maps any conversion or git failure into OutOutput.
	 */
	inline bool RunGitForFiles(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const TArray<FString>& PrefixArgs,
		const TArray<FString>& Files,
		FUnrealGitWorkerOutput& OutOutput)
	{
		TArray<FString> Relative;
		FString ConversionError;
		if (!TryMakeRepoRelativePaths(RepoRoot, Files, Relative, ConversionError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ConversionError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ConversionError);
			return false;
		}

		FString GitError;
		if (!RunGitWithBatchedPaths(ProcessRunner, RepoRoot, PrefixArgs, Relative, GitError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(GitError);
			return false;
		}

		return true;
	}
}
