// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

namespace UnrealGit::Workers
{
	/**
	 * Fetches the revision history for a single file.
	 *
	 * @param ProcessRunner Process runner used to invoke Git.
	 * @param RepoRoot Repository root path.
	 * @param AbsolutePath Absolute path of the file.
	 * @param OutRevisions Receives parsed revisions (newest to oldest) with sizes filled.
	 * @param OutError Receives an error message on failure.
	 * @return True when history was fetched and parsed.
	 */
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
