// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCheckOutWorker.h"

#include "HAL/PlatformFileManager.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitProcessUtils.h"

FName FGitCheckOutWorker::GetName() const
{
	return "CheckOut";
}

void FGitCheckOutWorker::Execute(
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

	FString RepoRoot = CurrentRepoRoot;
	if (!UnrealGit::Workers::EnsureRepoRootOrFail(ProcessRunner, WorkingDirectoryHint, RepoRoot, OutOutput))
	{
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

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.SetReadOnly(*AbsolutePath, false);

		if (Settings.bEnableLfsLocks)
		{
			// Checkout doubles as the LFS lock acquisition: holding the lock means the file
			// is checked out to this user. Non-LFS files simply fail to lock and stay usable.
			FGitProcessRequest LockRequest;
			LockRequest.WorkingDirectory = FString();
			LockRequest.RepoRoot = RepoRoot;
			LockRequest.Arguments = { TEXT("lfs"), TEXT("lock"), TEXT("--"), Relative };

			const FGitProcessResult LockResult = ProcessRunner->Run(LockRequest);
			if (LockResult.ExitCode != 0)
			{
				UE_LOG(LogUnrealGit, Verbose, TEXT("CheckOut: could not lock '%s' (probably not LFS-tracked): %s"),
					*Relative,
					*UnrealGit::Workers::BytesToTextUtf8Lossy(LockResult.StdErr).TrimStartAndEnd());
			}
		}
	}

	UnrealGit::Workers::CompleteWorkerSuccess(OutOutput, RepoRoot, Files);
}
