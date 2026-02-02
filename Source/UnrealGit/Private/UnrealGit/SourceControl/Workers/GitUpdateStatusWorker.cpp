// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"

#include "ISourceControlOperation.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"
#include "UnrealGit/Git/Parsers/GitRevParseParser.h"
#include "UnrealGit/Git/Parsers/GitStatusParser.h"

static FString BytesToTextUtf8Lossy(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() == 0)
	{
		return FString();
	}

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	return FString(Converter.Length(), Converter.Get());
}

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
	if (RepoRoot.IsEmpty())
	{
		FGitProcessRequest RootRequest;
		RootRequest.WorkingDirectory = WorkingDirectoryHint;
		RootRequest.Arguments = { TEXT("rev-parse"), TEXT("--show-toplevel") };

		const FGitProcessResult RootResult = ProcessRunner->Run(RootRequest);
		const FString RootStdOut = BytesToTextUtf8Lossy(RootResult.StdOut);

		if (RootResult.ExitCode != 0 || !FGitRevParseParser::ParseShowToplevel(RootStdOut, RepoRoot))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("Git repository root could not be determined. Ensure the project is inside a Git worktree."));
			return;
		}
	}

	FGitProcessRequest StatusRequest;
	StatusRequest.WorkingDirectory = RepoRoot;
	StatusRequest.Arguments = {
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
		StatusRequest.Arguments.Add(TEXT("--"));
		for (const FString& AbsolutePath : Files)
		{
			FString Relative = AbsolutePath;
			if (!FPaths::MakePathRelativeTo(Relative, *RepoRoot))
			{
				OutOutput.bSuccess = false;
				OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
				return;
			}
			FPaths::MakeStandardFilename(Relative);
			StatusRequest.Arguments.Add(Relative);
		}
	}

	const FGitProcessResult StatusResult = ProcessRunner->Run(StatusRequest);
	if (StatusResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(BytesToTextUtf8Lossy(StatusResult.StdErr));
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
		LocksRequest.WorkingDirectory = RepoRoot;
		LocksRequest.Arguments = { TEXT("lfs"), TEXT("locks"), TEXT("--json") };

		const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
		if (LocksResult.ExitCode == 0)
		{
			TArray<FGitLfsLock> Locks;
			FString LocksError;
			const FString LocksText = BytesToTextUtf8Lossy(LocksResult.StdOut);
			if (FGitLfsLocksParser::ParseJson(LocksText, Locks, LocksError))
			{
				OutOutput.bHasLfsLocks = true;
				OutOutput.LfsLocks = MoveTemp(Locks);
			}
		}
	}
}
