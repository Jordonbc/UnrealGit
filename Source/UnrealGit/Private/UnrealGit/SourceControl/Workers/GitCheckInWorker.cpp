// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/Workers/GitCheckInWorker.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "SourceControlOperations.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"

FName FGitCheckInWorker::GetName() const
{
	return "CheckIn";
}

void FGitCheckInWorker::Execute(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FUnrealGitProviderSettings& Settings,
	const FString& WorkingDirectoryHint,
	bool /*bQueryLfsLocks*/,
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
		OutOutput.ErrorText = FText::FromString(RepoRootError.IsEmpty() ? TEXT("Git repository root could not be determined.") : RepoRootError);
		return;
	}

	const TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOp = StaticCastSharedRef<FCheckIn>(Operation);
	const FString Description = CheckInOp->GetDescription().ToString();
	if (Description.TrimStartAndEnd().IsEmpty())
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Commit message is empty."));
		return;
	}

	TArray<FString> RelativePaths;
	FString ConvertError;
	if (Files.Num() > 0)
	{
		if (!UnrealGit::Workers::TryMakeRepoRelativePaths(RepoRoot, Files, RelativePaths, ConvertError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(ConvertError.IsEmpty() ? TEXT("File is outside the Git repository root.") : ConvertError);
			return;
		}

		FString AddError;
		if (!UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, { TEXT("add"), TEXT("-A") }, RelativePaths, AddError))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(AddError);
			return;
		}
	}

	const FString TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UnrealGit"));
	IFileManager::Get().MakeDirectory(*TempDir, true);
	FString MessageFile = FPaths::CreateTempFilename(*TempDir, TEXT("CommitMessage_"), TEXT(".txt"));
	MessageFile = FPaths::ConvertRelativePathToFull(MessageFile);

	TArray<uint8> Utf8Bytes;
	{
		FTCHARToUTF8 Utf8(*Description);
		Utf8Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Utf8Bytes.Add('\n');
	}

	if (!FFileHelper::SaveArrayToFile(Utf8Bytes, *MessageFile))
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(TEXT("Failed to write commit message temporary file."));
		return;
	}

	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*MessageFile, false, true, true);
	};

	if (RelativePaths.Num() == 0)
	{
		FGitProcessRequest CommitRequest;
		CommitRequest.WorkingDirectory = FString();
		CommitRequest.RepoRoot = RepoRoot;
		CommitRequest.Arguments = { TEXT("commit"), TEXT("-F"), MessageFile };

		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("CheckIn: Running git commit with message file: %s"), *MessageFile);

		const FGitProcessResult CommitResult = ProcessRunner->Run(CommitRequest);
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("CheckIn: git commit result: exit=%d, stdout='%s', stderr='%s'"),
			CommitResult.ExitCode,
			*UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdOut).Left(200),
			*UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdErr).Left(200));

		if (CommitResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdErr));
			return;
		}
	}
	else
	{
		bool bFirstBatch = true;
		bool bCommitSucceeded = true;
		FString CommitError;

		UnrealGit::Workers::ForEachPathBatch(RelativePaths, UnrealGit::Workers::MaxFilesPerCommand, [&](const TArray<FString>& Batch)
		{
			if (!bCommitSucceeded)
			{
				return;
			}

			FGitProcessRequest CommitRequest;
			CommitRequest.WorkingDirectory = FString();
			CommitRequest.RepoRoot = RepoRoot;

			if (bFirstBatch)
			{
				CommitRequest.Arguments = { TEXT("commit"), TEXT("-F"), MessageFile, TEXT("--") };
				bFirstBatch = false;
			}
			else
			{
				CommitRequest.Arguments = { TEXT("commit"), TEXT("--amend"), TEXT("-F"), MessageFile, TEXT("--") };
			}
			CommitRequest.Arguments.Append(Batch);

			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("CheckIn: Running git commit with message file: %s"), *MessageFile);

			const FGitProcessResult CommitResult = ProcessRunner->Run(CommitRequest);
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("CheckIn: git commit result: exit=%d, stdout='%s', stderr='%s'"),
				CommitResult.ExitCode,
				*UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdOut).Left(200),
				*UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdErr).Left(200));

			if (CommitResult.ExitCode != 0)
			{
				bCommitSucceeded = false;
				CommitError = UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdErr);
			}
		});

		if (!bCommitSucceeded)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(CommitError);
			return;
		}
	}

	if (Settings.bEnableLfsLocks && RelativePaths.Num() > 0)
	{
		// Submitting releases the LFS lock acquired by checkout (best effort).
		FString UnlockError;
		UnrealGit::Workers::RunGitWithBatchedPaths(ProcessRunner, RepoRoot, { TEXT("lfs"), TEXT("unlock") }, RelativePaths, UnlockError);
	}

	OutOutput.bSuccess = true;
	OutOutput.RepoRoot = RepoRoot;
	OutOutput.bShouldUpdateStatus = true;

	if (Settings.bAutoPushAfterSubmit)
	{
		FGitProcessRequest PushRequest;
		PushRequest.WorkingDirectory = FString();
		PushRequest.RepoRoot = RepoRoot;
		PushRequest.Arguments = { TEXT("push") };
		const FGitProcessResult PushResult = ProcessRunner->Run(PushRequest);
		if (PushResult.ExitCode != 0)
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(PushResult.StdErr));
			return;
		}
	}
}
