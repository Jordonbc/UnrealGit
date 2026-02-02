// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCheckOutWorker.h"

#include "HAL/PlatformFileManager.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

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
	if (!UnrealGit::Workers::EnsureRepoRoot(ProcessRunner, WorkingDirectoryHint, RepoRoot))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Git repository root could not be determined."));
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

		if (Settings.bEnableLfsLocks && Settings.bAutoLockOnCheckout)
		{
			FGitProcessRequest LockRequest;
			LockRequest.WorkingDirectory = RepoRoot;
			LockRequest.Arguments = { TEXT("lfs"), TEXT("lock"), TEXT("--"), Relative };
			ProcessRunner->Run(LockRequest);
		}
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;
	OutOutput.FilesToUpdateStatus = Files;
}
