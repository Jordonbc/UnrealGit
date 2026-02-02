// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#include "Async/Async.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/GitModels.h"
#include "UnrealGit/Git/GitRevisionMaterializer.h"
#include "UnrealGit/Git/Parsers/GitVersionParser.h"
#include "UnrealGit/Private/UnrealGit/Git/SystemGitProcessRunner.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"
#include "UnrealGit/Private/UnrealGit/Settings/UnrealGitSettingsResolver.h"
#include "UnrealGit/Settings/UnrealGitProjectSettings.h"
#include "UnrealGit/Settings/UnrealGitUserSettings.h"
#include "UnrealGit/SourceControl/Workers/GitCheckInWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCheckOutWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitGetHistoryWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitLockWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitMarkForAddWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitSyncWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUnlockWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlState.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlRevision.h"
#include "UObject/CoreUObjectDelegates.h"

struct FUnrealGitSourceControlProvider::FCommand final
{
	FCommand(
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation,
		const TArray<FString>& InFiles,
		const FSourceControlOperationComplete& InDelegate,
		TUniquePtr<IGitSourceControlWorker>&& InWorker,
		const FUnrealGitProviderSettings& InProviderSettings,
		bool bInQueryLfsLocks)
		: Operation(InOperation)
		, Files(InFiles)
		, Delegate(InDelegate)
		, Worker(MoveTemp(InWorker))
		, ProviderSettings(InProviderSettings)
		, bQueryLfsLocks(bInQueryLfsLocks)
	{
	}

	TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> Operation;
	TArray<FString> Files;
	FSourceControlOperationComplete Delegate;

	TUniquePtr<IGitSourceControlWorker> Worker;
	TFuture<FUnrealGitWorkerOutput> Future;

	FUnrealGitProviderSettings ProviderSettings;
	bool bQueryLfsLocks = false;
};

static bool IsOperationSynchronousOnGameThread(EConcurrency::Type Concurrency)
{
	return Concurrency == EConcurrency::Synchronous && IsInGameThread();
}

static FString BytesToTextUtf8Lossy(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() == 0)
	{
		return FString();
	}

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	return FString(Converter.Length(), Converter.Get());
}

FUnrealGitSourceControlProvider::FUnrealGitSourceControlProvider() = default;

FUnrealGitSourceControlProvider::~FUnrealGitSourceControlProvider()
{
	Close();
}

void FUnrealGitSourceControlProvider::Init(bool /*bForceConnection*/)
{
	LastErrorText = FText::GetEmpty();

	FText SettingsError;
	if (!FUnrealGitSettingsResolver::Build(Settings, SettingsError))
	{
		LastErrorText = SettingsError;
		Settings = FUnrealGitProviderSettings();
		Settings.GitExecutable = TEXT("git");
		Settings.RepositoryDiscoveryDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	ProcessRunner = MakeShared<FSystemGitProcessRunner, ESPMode::ThreadSafe>(Settings.GitExecutable);

	const FString SessionDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UnrealGit"), TEXT("Materialized"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	RevisionMaterializer = MakeShared<FGitRevisionMaterializer, ESPMode::ThreadSafe>(SessionDir);

	StartEnvironmentValidation();

	if (!SettingsChangedHandle.IsValid())
	{
		SettingsChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(AsShared(), &FUnrealGitSourceControlProvider::HandleSettingsObjectChanged);
	}
}

void FUnrealGitSourceControlProvider::Close()
{
	FScopeLock Lock(&CommandLock);
	Commands.Reset();

	if (RevisionMaterializer.IsValid())
	{
		RevisionMaterializer->Cleanup();
		RevisionMaterializer.Reset();
	}

	EnvironmentFuture = TFuture<FGitEnvironmentInfo>();
	{
		FScopeLock Scope(&EnvironmentLock);
		EnvironmentInfo.Reset();
	}

	if (SettingsChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
}

const FName& FUnrealGitSourceControlProvider::GetName() const
{
	static const FName ProviderName("UnrealGit");
	return ProviderName;
}

FText FUnrealGitSourceControlProvider::GetStatusText() const
{
	if (!LastErrorText.IsEmpty())
	{
		return LastErrorText;
	}

	FGitEnvironmentInfo LocalEnv;
	bool bHasEnv = false;
	{
		FScopeLock Scope(&EnvironmentLock);
		if (EnvironmentInfo.IsSet())
		{
			LocalEnv = EnvironmentInfo.GetValue();
			bHasEnv = true;
		}
	}

	if (bHasEnv && !LocalEnv.bGitAvailable)
	{
		return FText::FromString(TEXT("UnrealGit: Git not available (check git executable and PATH)"));
	}
	if (bHasEnv && Settings.bEnableLfsLocks && !LocalEnv.bGitLfsAvailable)
	{
		return FText::FromString(TEXT("UnrealGit: Git LFS not available (locks disabled)"));
	}

	if (RepoRoot.IsEmpty())
	{
		return FText::FromString(TEXT("UnrealGit: Not connected"));
	}

	if (bHasEnv)
	{
		return FText::FromString(FString::Printf(TEXT("UnrealGit: Connected (git %s, lfs %s)"), *LocalEnv.GitVersion, *LocalEnv.GitLfsVersion));
	}

	return FText::FromString(TEXT("UnrealGit: Connected"));
}

bool FUnrealGitSourceControlProvider::IsEnabled() const
{
	return true;
}

bool FUnrealGitSourceControlProvider::IsAvailable() const
{
	return ProcessRunner.IsValid();
}

FSourceControlStateRef FUnrealGitSourceControlProvider::GetOrCreateStateInternal(const FString& AbsoluteFilename)
{
	FScopeLock Lock(&StateCacheLock);
	if (const FSourceControlStateRef* Existing = StateCache.Find(AbsoluteFilename))
	{
		return *Existing;
	}

	FSourceControlStateRef NewState = MakeShared<FUnrealGitSourceControlState, ESPMode::ThreadSafe>(AbsoluteFilename);
	StateCache.Add(AbsoluteFilename, NewState);
	return NewState;
}

FSourceControlStatePtr FUnrealGitSourceControlProvider::GetState(const FString& Filename, EStateCacheUsage::Type InStateCacheUsage)
{
	const FString Abs = FPaths::ConvertRelativePathToFull(Filename);
	const FSourceControlStatePtr State = GetOrCreateStateInternal(Abs);

	// A request for fresh state must never block; schedule an async status update for the file.
	// This keeps icon/UI queries responsive while allowing callers to force refresh.
	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
		Execute(UpdateOp, { Abs }, EConcurrency::Asynchronous, FSourceControlOperationComplete());
	}

	return State;
}

FSourceControlStatePtr FUnrealGitSourceControlProvider::GetState(const FSourceControlChangelistPtr& /*InChangelist*/, const FString& Filename)
{
	return GetOrCreateStateInternal(FPaths::ConvertRelativePathToFull(Filename));
}

TArray<FSourceControlStateRef> FUnrealGitSourceControlProvider::GetState(const TArray<FString>& InFiles, EStateCacheUsage::Type InStateCacheUsage)
{
	TArray<FSourceControlStateRef> OutStates;
	OutStates.Reserve(InFiles.Num());
	TArray<FString> AbsFiles;
	AbsFiles.Reserve(InFiles.Num());
	for (const FString& File : InFiles)
	{
		const FString Abs = FPaths::ConvertRelativePathToFull(File);
		OutStates.Add(GetOrCreateStateInternal(Abs));
		AbsFiles.Add(Abs);
	}

	if (InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
		Execute(UpdateOp, AbsFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete());
	}

	return OutStates;
}

FString FUnrealGitSourceControlProvider::GetWorkingDirectoryHint() const
{
	return Settings.RepositoryDiscoveryDirectory.IsEmpty()
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())
		: Settings.RepositoryDiscoveryDirectory;
}

void FUnrealGitSourceControlProvider::HandleSettingsObjectChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& /*PropertyChangedEvent*/)
{
	if (ObjectBeingModified == nullptr)
	{
		return;
	}

	if (!ObjectBeingModified->IsA<UUnrealGitProjectSettings>() && !ObjectBeingModified->IsA<UUnrealGitUserSettings>())
	{
		return;
	}

	FText SettingsError;
	FUnrealGitProviderSettings NewSettings;
	if (!FUnrealGitSettingsResolver::Build(NewSettings, SettingsError))
	{
		LastErrorText = SettingsError;
		return;
	}

	const bool bGitExecutableChanged = NewSettings.GitExecutable != Settings.GitExecutable;
	Settings = MoveTemp(NewSettings);

	if (bGitExecutableChanged)
	{
		ProcessRunner = MakeShared<FSystemGitProcessRunner, ESPMode::ThreadSafe>(Settings.GitExecutable);
	}

	StartEnvironmentValidation();
}

void FUnrealGitSourceControlProvider::StartEnvironmentValidation()
{
	if (!ProcessRunner.IsValid())
	{
		return;
	}

	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> Runner = ProcessRunner.ToSharedRef();
	const FString WorkingDir = GetWorkingDirectoryHint();

	EnvironmentFuture = Async(EAsyncExecution::ThreadPool, [Runner, WorkingDir]()
	{
		FGitEnvironmentInfo Info;

		{
			FGitProcessRequest VersionRequest;
			VersionRequest.WorkingDirectory = WorkingDir;
			VersionRequest.Arguments = { TEXT("--version") };
			const FGitProcessResult Res = Runner->Run(VersionRequest);
			const FString StdOut = BytesToTextUtf8Lossy(Res.StdOut);
			FString Parsed;
			if (Res.ExitCode == 0 && FGitVersionParser::ParseGitVersion(StdOut, Parsed))
			{
				Info.bGitAvailable = true;
				Info.GitVersion = MoveTemp(Parsed);
			}
		}

		{
			FGitProcessRequest LfsRequest;
			LfsRequest.WorkingDirectory = WorkingDir;
			LfsRequest.Arguments = { TEXT("lfs"), TEXT("version") };
			const FGitProcessResult Res = Runner->Run(LfsRequest);
			const FString StdOut = BytesToTextUtf8Lossy(Res.StdOut);
			FString Parsed;
			if (Res.ExitCode == 0 && FGitVersionParser::ParseGitLfsVersion(StdOut, Parsed))
			{
				Info.bGitLfsAvailable = true;
				Info.GitLfsVersion = MoveTemp(Parsed);
			}
		}

		{
			FGitProcessRequest NameRequest;
			NameRequest.WorkingDirectory = WorkingDir;
			NameRequest.Arguments = { TEXT("config"), TEXT("user.name") };
			const FGitProcessResult Res = Runner->Run(NameRequest);
			if (Res.ExitCode == 0)
			{
				Info.UserName = BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
			}
		}

		{
			FGitProcessRequest EmailRequest;
			EmailRequest.WorkingDirectory = WorkingDir;
			EmailRequest.Arguments = { TEXT("config"), TEXT("user.email") };
			const FGitProcessResult Res = Runner->Run(EmailRequest);
			if (Res.ExitCode == 0)
			{
				Info.UserEmail = BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
			}
		}

		return Info;
	});
}

ECommandResult::Type FUnrealGitSourceControlProvider::Execute(
	const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation,
	const TArray<FString>& InFiles,
	EConcurrency::Type InConcurrency,
	const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	if (IsOperationSynchronousOnGameThread(InConcurrency))
	{
		LastErrorText = FText::FromString(TEXT("UnrealGit does not execute synchronous operations on the game thread."));
		return ECommandResult::Failed;
	}

	if (!ProcessRunner.IsValid())
	{
		LastErrorText = FText::FromString(TEXT("UnrealGit process runner not initialized."));
		return ECommandResult::Failed;
	}

	const FName OpName = InOperation->GetName();

	TUniquePtr<IGitSourceControlWorker> Worker;
	if (OpName == "UpdateStatus")
	{
		Worker = MakeUnique<FGitUpdateStatusWorker>();
	}
	else if (OpName == "CheckOut")
	{
		Worker = MakeUnique<FGitCheckOutWorker>();
	}
	else if (OpName == "MarkForAdd")
	{
		Worker = MakeUnique<FGitMarkForAddWorker>();
	}
	else if (OpName == "Revert")
	{
		Worker = MakeUnique<FGitRevertWorker>();
	}
	else if (OpName == "CheckIn")
	{
		Worker = MakeUnique<FGitCheckInWorker>();
	}
	else if (OpName == "Sync")
	{
		Worker = MakeUnique<FGitSyncWorker>();
	}
	else if (OpName == "GetHistory")
	{
		Worker = MakeUnique<FGitGetHistoryWorker>();
	}
	else if (OpName == "Lock")
	{
		Worker = MakeUnique<FGitLockWorker>();
	}
	else if (OpName == "Unlock")
	{
		Worker = MakeUnique<FGitUnlockWorker>();
	}

	if (!Worker)
	{
		LastErrorText = FText::FromString(FString::Printf(TEXT("Operation not supported: %s"), *OpName.ToString()));
		return ECommandResult::Failed;
	}

	bool bQueryLfsLocks = false;
	if (OpName == "UpdateStatus" && Settings.bEnableLfsLocks)
	{
		const FDateTime NowUtc = FDateTime::UtcNow();
		if (NextLfsLocksRefreshUtc <= NowUtc)
		{
			bQueryLfsLocks = true;
			NextLfsLocksRefreshUtc = NowUtc + FTimespan::FromSeconds(FMath::Max(1, Settings.LfsLocksRefreshIntervalSeconds));
		}
	}

	TSharedRef<FCommand, ESPMode::ThreadSafe> Command = MakeShared<FCommand, ESPMode::ThreadSafe>(
		InOperation,
		InFiles,
		InOperationCompleteDelegate,
		MoveTemp(Worker),
		Settings,
		bQueryLfsLocks);
	const FString WorkingDirHint = GetWorkingDirectoryHint();
	const FString ExistingRepoRoot = RepoRoot;
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> Runner = ProcessRunner.ToSharedRef();

	Command->Future = Async(EAsyncExecution::ThreadPool, [Runner, WorkingDirHint, ExistingRepoRoot, Command]()
	{
		FUnrealGitWorkerOutput Output;
		Command->Worker->Execute(Runner, Command->ProviderSettings, WorkingDirHint, Command->bQueryLfsLocks, Command->Operation, Command->Files, ExistingRepoRoot, Output);
		return Output;
	});

	{
		FScopeLock Lock(&CommandLock);
		Commands.Add(Command);
	}

	return ECommandResult::Succeeded;
}

bool FUnrealGitSourceControlProvider::CanCancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*InOperation*/) const
{
	return false;
}

void FUnrealGitSourceControlProvider::CancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& /*InOperation*/)
{
}

bool FUnrealGitSourceControlProvider::UsesLocalReadOnlyState() const
{
	return true;
}

void FUnrealGitSourceControlProvider::UpdateStatesFromStatusSnapshot(const FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles)
{
	TSet<FString> RequestedSet;
	for (const FString& File : RequestedFiles)
	{
		RequestedSet.Add(FPaths::ConvertRelativePathToFull(File));
	}

	for (const FGitFileStatus& FileStatus : Snapshot.Files)
	{
		const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / FileStatus.RelativePath);
		FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
		StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->UpdateFromStatus(FileStatus);
	}

	// Ensure explicitly requested files have state objects even if not returned by the status snapshot.
	for (const FString& Requested : RequestedSet)
	{
		GetOrCreateStateInternal(Requested);
	}
}

void FUnrealGitSourceControlProvider::ApplyLfsLocksToStates(const TOptional<FGitStatusSnapshot>& Snapshot, const TArray<FGitLfsLock>& Locks)
{
	FString MyName;
	FString MyEmail;
	{
		FScopeLock Scope(&EnvironmentLock);
		if (EnvironmentInfo.IsSet())
		{
			MyName = EnvironmentInfo->UserName;
			MyEmail = EnvironmentInfo->UserEmail;
		}
	}

	if (Snapshot.IsSet())
	{
		for (const FGitFileStatus& FileStatus : Snapshot->Files)
		{
			const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / FileStatus.RelativePath);
			const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
			StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->SetLockState(EGitLockState::NotLocked, FString());
		}
	}

	for (const FGitLfsLock& Lock : Locks)
	{
		FString Rel = Lock.Path;
		FPaths::MakeStandardFilename(Rel);
		const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / Rel);

		EGitLockState LockState = EGitLockState::LockedByOther;
		if (!MyEmail.IsEmpty() && Lock.OwnerEmail == MyEmail)
		{
			LockState = EGitLockState::LockedByMe;
		}
		else if (!MyName.IsEmpty() && Lock.OwnerName == MyName)
		{
			LockState = EGitLockState::LockedByMe;
		}

		FString Owner = !Lock.OwnerName.IsEmpty() ? Lock.OwnerName : Lock.OwnerEmail;
		const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
		StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->SetLockState(LockState, MoveTemp(Owner));
	}
}

bool FUnrealGitSourceControlProvider::Tick(float /*DeltaTime*/)
{
	if (EnvironmentFuture.IsValid() && EnvironmentFuture.IsReady())
	{
		const FGitEnvironmentInfo Info = EnvironmentFuture.Get();
		{
			FScopeLock Scope(&EnvironmentLock);
			EnvironmentInfo = Info;
		}
		EnvironmentFuture = TFuture<FGitEnvironmentInfo>();
	}

	TArray<TSharedRef<FCommand, ESPMode::ThreadSafe>> Completed;
	{
		FScopeLock Lock(&CommandLock);
		for (int32 Index = Commands.Num() - 1; Index >= 0; --Index)
		{
			const TSharedRef<FCommand, ESPMode::ThreadSafe>& Command = Commands[Index];
			if (Command->Future.IsReady())
			{
				Completed.Add(Command);
				Commands.RemoveAtSwap(Index);
			}
		}
	}

	for (const TSharedRef<FCommand, ESPMode::ThreadSafe>& Command : Completed)
	{
		const FUnrealGitWorkerOutput Output = Command->Future.Get();

		if (Output.RepoRoot.IsSet())
		{
			RepoRoot = Output.RepoRoot.GetValue();
		}

		if (Output.bSuccess && Output.StatusSnapshot.IsSet())
		{
			UpdateStatesFromStatusSnapshot(Output.StatusSnapshot.GetValue(), Command->Files);
			LastErrorText = FText::GetEmpty();
		}
		else if (!Output.bSuccess)
		{
			LastErrorText = Output.ErrorText;
		}

		if (Output.bSuccess && Output.bHasLfsLocks)
		{
			CachedLfsLocks = Output.LfsLocks;
			bHasCachedLfsLocks = true;
		}

		if (Output.bSuccess && Output.StatusSnapshot.IsSet() && bHasCachedLfsLocks)
		{
			ApplyLfsLocksToStates(Output.StatusSnapshot, CachedLfsLocks);
		}

		if (Output.bSuccess && Output.FileHistoryByAbsoluteFile.Num() > 0 && RevisionMaterializer.IsValid())
		{
			for (const TPair<FString, TArray<FGitFileRevision>>& Pair : Output.FileHistoryByAbsoluteFile)
			{
				const FString Abs = Pair.Key;

				TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>> Revisions;
				Revisions.Reserve(Pair.Value.Num());
				for (const FGitFileRevision& FileRev : Pair.Value)
				{
					Revisions.Add(MakeShared<FUnrealGitSourceControlRevision, ESPMode::ThreadSafe>(
						Abs,
						FileRev.RepoRelativePathAtRevision,
						FileRev.Revision,
						RepoRoot,
						ProcessRunner.ToSharedRef(),
						RevisionMaterializer.ToSharedRef()));
				}

				const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
				StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->SetHistory(Revisions);
			}
		}

		if (Command->Delegate.IsBound())
		{
			Command->Delegate.ExecuteIfBound(Command->Operation, Output.bSuccess ? ECommandResult::Succeeded : ECommandResult::Failed);
		}

		if (Output.bSuccess && Output.bShouldUpdateStatus)
		{
			const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe> UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
			Execute(UpdateOp, Output.FilesToUpdateStatus, EConcurrency::Asynchronous, FSourceControlOperationComplete());
		}
	}

	return true;
}
