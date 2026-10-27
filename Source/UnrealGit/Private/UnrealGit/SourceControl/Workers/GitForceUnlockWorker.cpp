// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitForceUnlockWorker.h"

#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

static FString NormalizeLockPath(const FString& InPath)
{
	FString Result = InPath;
	Result.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Result;
}

FName FGitForceUnlockWorker::GetName() const
{
	return "ForceUnlock";
}

void FGitForceUnlockWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& Settings,
	const FString& WorkingDirectoryHint,
	bool /*bQueryLfsLocks*/,
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*Operation*/,
	const TArray<FString>& Files,
	const FString& CurrentRepoRoot,
	FUnrealGitWorkerOutput& OutOutput)
{
	OutOutput = FUnrealGitWorkerOutput();

	if (!Settings.bEnableLfsLocks)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Git LFS locks are disabled in settings."));
		return;
	}

	if (Files.Num() == 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("No files specified."));
		return;
	}

	FString RepoRoot = CurrentRepoRoot;
	FString RepoRootError;
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot, &RepoRootError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty() ? TEXT("Git repository root could not be determined.") : RepoRootError);
		return;
	}

	TArray<FString> RelativePaths;
	FString ConvertError;
	if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(ConvertError);
		return;
	}

	// Force unlock by lock ID: `git lfs unlock --force <path>` re-resolves the path itself
	// and fails when that lookup does not match (case, cwd, server path normalization).
	FGitProcessRequest LocksRequest;
	LocksRequest.WorkingDirectory = FString();
	LocksRequest.RepoRoot = RepoRoot;
	LocksRequest.Arguments = { TEXT("lfs"), TEXT("locks"), TEXT("--json") };

	const FGitProcessResult LocksResult = ProcessRunner->Run(LocksRequest);
	if (LocksResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(LocksResult.StdErr));
		return;
	}

	TArray<FGitLfsLock> Locks;
	FString LocksError;
	if (!FGitLfsLocksParser::ParseJson(UnrealGit::Workers::BytesToTextUtf8Lossy(LocksResult.StdOut), Locks, LocksError))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(LocksError);
		return;
	}

	TMap<FString, FString> LockIdByPath;
	for (const FGitLfsLock& Lock : Locks)
	{
		LockIdByPath.Add(NormalizeLockPath(Lock.Path), Lock.Id);
	}

	for (const FString& RelativePath : RelativePaths)
	{
		const FString* LockId = LockIdByPath.Find(NormalizeLockPath(RelativePath));
		if (!LockId || LockId->IsEmpty())
		{
			// Nothing locked on this path: force unlock is a no-op.
			continue;
		}

		FGitProcessRequest UnlockRequest;
		UnlockRequest.WorkingDirectory = FString();
		UnlockRequest.RepoRoot = RepoRoot;
		UnlockRequest.Arguments = { TEXT("lfs"), TEXT("unlock"), TEXT("--force"), TEXT("--id"), *LockId };

		const FGitProcessResult UnlockResult = ProcessRunner->Run(UnlockRequest);
		if (UnlockResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(UnlockResult.StdErr));
			return;
		}
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
