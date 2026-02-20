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

	FGitProcessRequest AddRequest;
	AddRequest.WorkingDirectory = FString();
	AddRequest.RepoRoot = RepoRoot;
	AddRequest.Arguments = {
		TEXT("add"),
		TEXT("-A"),
		TEXT("--")
	};

	for (const FString& AbsolutePath : Files)
	{
		FString Relative;
		if (!UnrealGit::Workers::TryMakeRepoRelativePath(RepoRoot, AbsolutePath, Relative))
		{
			OutOutput.bSuccess = false;
			OutOutput.ErrorText = FText::FromString(TEXT("File is outside the Git repository root."));
			return;
		}
		AddRequest.Arguments.Add(Relative);
	}

	UE_LOG(LogUnrealGit, Verbose, TEXT("CheckIn: Running git add with WorkingDirectory='%s', args='%s'"), 
		*AddRequest.WorkingDirectory, *FString::Join(AddRequest.Arguments, TEXT(" ")));

	const FGitProcessResult AddResult = ProcessRunner->Run(AddRequest);
	UE_LOG(LogUnrealGit, Verbose, TEXT("CheckIn: git add result: exit=%d, stdout='%s', stderr='%s'"), 
		AddResult.ExitCode, 
		*UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdOut).Left(200),
		*UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdErr).Left(200));

	if (AddResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(AddResult.StdErr));
		return;
	}

	const FString TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UnrealGit"));
	IFileManager::Get().MakeDirectory(*TempDir, true);
	const FString MessageFile = FPaths::CreateTempFilename(*TempDir, TEXT("CommitMessage_"), TEXT(".txt"));

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

	FGitProcessRequest CommitRequest;
	CommitRequest.WorkingDirectory = FString();
	CommitRequest.RepoRoot = RepoRoot;
	CommitRequest.Arguments = { TEXT("commit"), TEXT("-F"), MessageFile };

	const FGitProcessResult CommitResult = ProcessRunner->Run(CommitRequest);
	if (CommitResult.ExitCode != 0)
	{
		OutOutput.bSuccess = false;
		OutOutput.ErrorText = FText::FromString(UnrealGit::Workers::BytesToTextUtf8Lossy(CommitResult.StdErr));
		return;
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
