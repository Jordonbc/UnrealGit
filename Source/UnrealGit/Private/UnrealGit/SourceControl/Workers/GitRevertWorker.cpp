// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"

#include "UnrealGit.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitRevertWorker::GetName() const
{
	return "Revert";
}

void FGitRevertWorker::Execute(
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

	if (Files.Num() == 0)
	{
		FGitProcessRequest RestoreRequest;
		RestoreRequest.WorkingDirectory = FString();
		RestoreRequest.RepoRoot = RepoRoot;
		RestoreRequest.Arguments = {
			TEXT("restore"),
			TEXT("--source=HEAD"),
			TEXT("--staged"),
			TEXT("--worktree"),
			TEXT("--"),
			TEXT(".")
		};

		const FGitProcessResult RestoreResult = ProcessRunner->Run(RestoreRequest);
		if (RestoreResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = UnrealGit::Workers::MakeGitErrorText(RestoreResult);
			return;
		}
	}
	else
	{
		TArray<FString> RelativePaths;
		FString ConvertError;
		if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ConvertError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ConvertError);
			return;
		}

		const TArray<FString> RestorePrefix = { TEXT("restore"), TEXT("--source=HEAD"), TEXT("--staged"), TEXT("--worktree") };
		FString RestoreError;
		if (!UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, RestorePrefix, RelativePaths, RestoreError))
		{
			// Added/untracked paths are not in HEAD, so restore cannot match them; unstage those instead.
			FString FirstError;
			for (const FString& RelativePath : RelativePaths)
			{
				FString SingleError;
				if (UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, RestorePrefix, { RelativePath }, SingleError))
				{
					continue;
				}

				FGitProcessRequest ResetRequest;
				ResetRequest.WorkingDirectory = FString();
				ResetRequest.RepoRoot = RepoRoot;
				ResetRequest.Arguments = { TEXT("reset"), TEXT("-q"), TEXT("HEAD"), TEXT("--"), RelativePath };

				const FGitProcessResult ResetResult = ProcessRunner->Run(ResetRequest);
				if (ResetResult.ExitCode != 0 && FirstError.IsEmpty())
				{
					FirstError = SingleError.IsEmpty()
						? UnrealGit::Workers::BytesToTextUtf8Lossy(ResetResult.StdErr)
						: SingleError;
				}
			}

			if (!FirstError.IsEmpty())
			{
				OutOutput.bSuccess = false;
				OutOutput.ErrorText = FText::FromString(FirstError);
				return;
			}
		}
	}

	if (Settings.bEnableLfsLocks && Files.Num() > 0)
	{
		// Reverting discards the changes, so release the LFS lock acquired by checkout.
		TArray<FString> RelativePaths;
		FString ConvertError;
		if (UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
		{
			FString UnlockError;
			UnrealGit::Workers::UnlockLfsLocks(ProcessRunner, RepoRoot, RelativePaths, false, UnlockError);
			if (!UnlockError.IsEmpty())
			{
				UE_LOG(LogUnrealGit, Verbose, TEXT("Revert: UnlockLfsLocks failed: %s"), *UnlockError);
			}
		}
	}

	UnrealGit::Workers::CompleteWorkerSuccess(OutOutput, RepoRoot, Files);
}
