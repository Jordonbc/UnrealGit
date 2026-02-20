// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#include "Async/Async.h"
#include "ISourceControlOperation.h"
#include "ISourceControlModule.h"
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
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitLockWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitMarkForAddWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitSyncWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUnlockWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlState.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlRevision.h"

#if SOURCE_CONTROL_WITH_SLATE
#include "Styling/AppStyle.h"
#include "Widgets/SNullWidget.h"
#endif // SOURCE_CONTROL_WITH_SLATE

struct FUnrealGitSourceControlProvider::FCommand final
{
	FCommand(
		const FSourceControlOperationRef& InOperation,
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

	FSourceControlOperationRef Operation;
	TArray<FString> Files;
	FSourceControlOperationComplete Delegate;

	TUniquePtr<IGitSourceControlWorker> Worker;
	TFuture<FUnrealGitWorkerOutput> Future;
	
	FUnrealGitProviderSettings ProviderSettings;
	bool bQueryLfsLocks = false;
};

FUnrealGitSourceControlProvider::FUnrealGitSourceControlProvider() = default;

FUnrealGitSourceControlProvider::~FUnrealGitSourceControlProvider()
{
	Close();
}

void FUnrealGitSourceControlProvider::Init(bool bForceConnection)
{
	LastErrorText = FText::GetEmpty();

	UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Init (bForceConnection=%s)"), bForceConnection ? TEXT("true") : TEXT("false"));

	FText SettingsError;
	if (!FUnrealGitSettingsResolver::Build(Settings, SettingsError))
	{
		LastErrorText = SettingsError;
		UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Settings resolver failed: %s"), *SettingsError.ToString());
		Settings = FUnrealGitProviderSettings();
		Settings.GitExecutable = TEXT("git");
		Settings.RepositoryDiscoveryDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: GitExecutable=\"%s\" RepoDiscoveryDir=\"%s\" EnableLfsLocks=%s"),
		*Settings.GitExecutable,
		*Settings.RepositoryDiscoveryDirectory,
		Settings.bEnableLfsLocks ? TEXT("true") : TEXT("false"));

	ProcessRunner = MakeShared<FSystemGitProcessRunner, ESPMode::ThreadSafe>(Settings.GitExecutable);

	const FString SessionDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UnrealGit"), TEXT("Materialized"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	RevisionMaterializer = MakeShared<FGitRevisionMaterializer, ESPMode::ThreadSafe>(SessionDir);

	StartEnvironmentValidation();

	if (EnvironmentFuture.IsValid())
	{
		const double SyncTimeoutSeconds = FSystemGitProcessRunner::DefaultTimeoutSeconds;
		const bool bEnvironmentReady = EnvironmentFuture.WaitFor(FTimespan::FromSeconds(SyncTimeoutSeconds));
		if (bEnvironmentReady)
		{
			FGitEnvironmentInfo Info = EnvironmentFuture.Get();
			{
				FScopeLock Scope(&EnvironmentLock);
				EnvironmentInfo = Info;
			}
			UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Environment: GitAvailable=%s GitVersion=\"%s\" LfsAvailable=%s LfsVersion=\"%s\" User=\"%s\" Email=\"%s\""),
				Info.bGitAvailable ? TEXT("true") : TEXT("false"),
				*Info.GitVersion,
				Info.bGitLfsAvailable ? TEXT("true") : TEXT("false"),
				*Info.GitLfsVersion,
				*Info.UserName,
				*Info.UserEmail);
		}
		else
		{
			UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: Environment validation timed out after %f seconds"), SyncTimeoutSeconds);
		}
		EnvironmentFuture = TFuture<FGitEnvironmentInfo>();
	}

	if (bForceConnection)
	{
		const FSourceControlOperationRef ConnectOp = ISourceControlOperation::Create<FConnect>();
		Execute(ConnectOp, nullptr, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete());
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

TMap<ISourceControlProvider::EStatus, FString> FUnrealGitSourceControlProvider::GetStatus() const
{
	TMap<EStatus, FString> Status;

	Status.Add(EStatus::Enabled, TEXT("true"));
	Status.Add(EStatus::Connected, RepoRoot.IsEmpty() ? TEXT("false") : TEXT("true"));
	Status.Add(EStatus::Repository, RepoRoot);

	{
		FScopeLock Scope(&EnvironmentLock);
		if (EnvironmentInfo.IsSet())
		{
			Status.Add(EStatus::User, EnvironmentInfo->UserName);
			Status.Add(EStatus::Email, EnvironmentInfo->UserEmail);
			Status.Add(EStatus::ScmVersion, EnvironmentInfo->GitVersion);
		}
	}

	return Status;
}

bool FUnrealGitSourceControlProvider::QueryStateBranchConfig(const FString& /*ConfigSrc*/, const FString& /*ConfigDest*/)
{
	return false;
}

void FUnrealGitSourceControlProvider::RegisterStateBranches(const TArray<FString>& /*BranchNames*/, const FString& /*ContentRoot*/)
{
}

int32 FUnrealGitSourceControlProvider::GetStateBranchIndex(const FString& /*BranchName*/) const
{
	return INDEX_NONE;
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

ECommandResult::Type FUnrealGitSourceControlProvider::GetState(const TArray<FString>& InFiles, TArray<FSourceControlStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	OutState.Reset();
	OutState.Reserve(InFiles.Num());

	TArray<FString> AbsFiles;
	AbsFiles.Reserve(InFiles.Num());

	bool bAnyInvalid = false;
	for (const FString& File : InFiles)
	{
		const FString Abs = FPaths::ConvertRelativePathToFull(File);
		FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
		OutState.Add(StateRef);
		AbsFiles.Add(Abs);

		// Check if this state needs updating - use public IsUnknown() method
		if (StateRef->IsUnknown())
		{
			bAnyInvalid = true;
		}
	}

	// Only trigger async update if:
	// 1. ForceUpdate is requested, OR
	// 2. Some files have unknown states AND no update is already in progress
	bool bUpdateInProgress = false;
	{
		FScopeLock Lock(&CommandLock);
		bUpdateInProgress = Commands.Num() > 0;
	}

	if ((InStateCacheUsage == EStateCacheUsage::ForceUpdate || bAnyInvalid) && !bUpdateInProgress && !RepoRoot.IsEmpty())
	{
		const FSourceControlOperationRef UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
		Execute(UpdateOp, nullptr, AbsFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete());
	}

	return ECommandResult::Succeeded;
}

ECommandResult::Type FUnrealGitSourceControlProvider::GetState(const TArray<FSourceControlChangelistRef>& /*InChangelists*/, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type /*InStateCacheUsage*/)
{
	OutState.Reset();
	return ECommandResult::Succeeded;
}

TArray<FSourceControlStateRef> FUnrealGitSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Results;
	FScopeLock Lock(&StateCacheLock);
	for (const TPair<FString, FSourceControlStateRef>& Pair : StateCache)
	{
		if (Predicate(Pair.Value))
		{
			Results.Add(Pair.Value);
		}
	}
	return Results;
}

FDelegateHandle FUnrealGitSourceControlProvider::RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& Delegate)
{
	return SourceControlStateChanged.Add(Delegate);
}

void FUnrealGitSourceControlProvider::UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle)
{
	SourceControlStateChanged.Remove(Handle);
}

FString FUnrealGitSourceControlProvider::GetWorkingDirectoryHint() const
{
	if (!Settings.RepositoryDiscoveryDirectory.IsEmpty())
	{
		return Settings.RepositoryDiscoveryDirectory;
	}

	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
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
			const FString StdOut = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut);
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
			const FString StdOut = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut);
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
				Info.UserName = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
			}
		}

		{
			FGitProcessRequest EmailRequest;
			EmailRequest.WorkingDirectory = WorkingDir;
			EmailRequest.Arguments = { TEXT("config"), TEXT("user.email") };
			const FGitProcessResult Res = Runner->Run(EmailRequest);
			if (Res.ExitCode == 0)
			{
				Info.UserEmail = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
			}
		}

		return Info;
	});
}

ECommandResult::Type FUnrealGitSourceControlProvider::Execute(
	const FSourceControlOperationRef& InOperation,
	FSourceControlChangelistPtr /*InChangelist*/,
	const TArray<FString>& InFiles,
	EConcurrency::Type InConcurrency,
	const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	const FName OpName = InOperation->GetName();

	if (!ProcessRunner.IsValid())
	{
		LastErrorText = FText::FromString(TEXT("UnrealGit process runner not initialized."));
		UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Execute(%s) failed: %s"), *OpName.ToString(), *LastErrorText.ToString());
		return ECommandResult::Failed;
	}

	TUniquePtr<IGitSourceControlWorker> Worker;
	if (OpName == "UpdateStatus" || OpName == "Connect")
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
		UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: %s"), *LastErrorText.ToString());
		return ECommandResult::Failed;
	}

	if (!CanExecuteOperation(InOperation))
	{
		LastErrorText = FText::FromString(FString::Printf(TEXT("Operation not supported: %s"), *OpName.ToString()));
		UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: %s"), *LastErrorText.ToString());
		return ECommandResult::Failed;
	}

	bool bQueryLfsLocks = false;
	if ((OpName == "UpdateStatus" || OpName == "Connect") && Settings.bEnableLfsLocks)
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

	UE_LOG(LogSourceControl, VeryVerbose, TEXT("UnrealGit: Queued operation %s (files=%d, concurrency=%s)"),
		*OpName.ToString(),
		InFiles.Num(),
		InConcurrency == EConcurrency::Synchronous ? TEXT("Sync") : TEXT("Async"));

	return ECommandResult::Succeeded;
}

bool FUnrealGitSourceControlProvider::CanExecuteOperation(const FSourceControlOperationRef& InOperation) const
{
	const FName OpName = InOperation->GetName();
	return OpName == "UpdateStatus"
		|| OpName == "Connect"
		|| OpName == "CheckOut"
		|| OpName == "MarkForAdd"
		|| OpName == "Revert"
		|| OpName == "CheckIn"
		|| OpName == "Sync"
		|| OpName == "GetHistory"
		|| OpName == "Lock"
		|| OpName == "Unlock";
}

bool FUnrealGitSourceControlProvider::CanCancelOperation(const FSourceControlOperationRef& /*InOperation*/) const
{
	// UnrealGit operations run via an async worker/future without cooperative cancellation today.
	return false;
}

void FUnrealGitSourceControlProvider::CancelOperation(const FSourceControlOperationRef& /*InOperation*/)
{
	// Not supported (see CanCancelOperation).
}

TArray<TSharedRef<ISourceControlLabel>> FUnrealGitSourceControlProvider::GetLabels(const FString& /*InMatchingSpec*/) const
{
	return {};
}

bool FUnrealGitSourceControlProvider::UsesLocalReadOnlyState() const
{
	return true;
}

TArray<FSourceControlChangelistRef> FUnrealGitSourceControlProvider::GetChangelists(EStateCacheUsage::Type /*InStateCacheUsage*/)
{
	return {};
}

bool FUnrealGitSourceControlProvider::UsesChangelists() const
{
	return false;
}

bool FUnrealGitSourceControlProvider::UsesUncontrolledChangelists() const
{
	return false;
}

bool FUnrealGitSourceControlProvider::UsesCheckout() const
{
	return true;
}

bool FUnrealGitSourceControlProvider::UsesFileRevisions() const
{
	return true;
}

bool FUnrealGitSourceControlProvider::UsesSnapshots() const
{
	return false;
}

bool FUnrealGitSourceControlProvider::AllowsDiffAgainstDepot() const
{
	return true;
}

TOptional<bool> FUnrealGitSourceControlProvider::IsAtLatestRevision() const
{
	return TOptional<bool>();
}

TOptional<int> FUnrealGitSourceControlProvider::GetNumLocalChanges() const
{
	return TOptional<int>();
}

void FUnrealGitSourceControlProvider::UpdateStatesFromStatusSnapshot(const FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles)
{
	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatesFromStatusSnapshot: Snapshot has %d files, RequestedFiles has %d"), Snapshot.Files.Num(), RequestedFiles.Num());

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
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UpdateStatesFromStatusSnapshot: Updated state for '%s' to state %d"), *Abs, (int32)FileStatus.State);
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

void FUnrealGitSourceControlProvider::Tick()
{
	if (EnvironmentFuture.IsValid() && EnvironmentFuture.IsReady())
	{
		const FGitEnvironmentInfo Info = EnvironmentFuture.Get();
		{
			FScopeLock Scope(&EnvironmentLock);
			EnvironmentInfo = Info;
		}
		UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Environment: GitAvailable=%s GitVersion=\"%s\" LfsAvailable=%s LfsVersion=\"%s\" User=\"%s\" Email=\"%s\""),
			Info.bGitAvailable ? TEXT("true") : TEXT("false"),
			*Info.GitVersion,
			Info.bGitLfsAvailable ? TEXT("true") : TEXT("false"),
			*Info.GitLfsVersion,
			*Info.UserName,
			*Info.UserEmail);
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
		bool bAnyStateChanged = false;
		const FString OperationName = Command->Operation->GetName().ToString();

		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("Tick: Processing completed operation '%s', bSuccess=%d, HasStatusSnapshot=%d, Files=%d"), 
			*OperationName, Output.bSuccess ? 1 : 0, Output.StatusSnapshot.IsSet() ? 1 : 0, Command->Files.Num());

		if (Output.RepoRoot.IsSet())
		{
			RepoRoot = Output.RepoRoot.GetValue();
		}

		if (Output.bSuccess && Output.StatusSnapshot.IsSet())
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("Tick: Calling UpdateStatesFromStatusSnapshot with %d files"), Output.StatusSnapshot.GetValue().Files.Num());
			UpdateStatesFromStatusSnapshot(Output.StatusSnapshot.GetValue(), Command->Files);
			LastErrorText = FText::GetEmpty();
			bAnyStateChanged = true;
		}
		else if (!Output.bSuccess)
		{
			LastErrorText = Output.ErrorText;
			UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Operation %s failed: %s"), *OperationName, *LastErrorText.ToString());

			const FString ErrorStr = LastErrorText.ToString();
			if (ErrorStr.Contains(TEXT("index file smaller than expected")) || ErrorStr.Contains(TEXT("index file corrupt")))
			{
				UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: Detected corrupted index, attempting auto-recovery..."));
				if (UnrealGit::Workers::TryFixCorruptedIndex(ProcessRunner, RepoRoot))
				{
					LastErrorText = FText::FromString(TEXT("Corrupted index was rebuilt. Please retry the operation."));
					UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Successfully recovered from corrupted index. User should retry the operation."));
				}
			}
		}

		if (Output.bSuccess && Output.bHasLfsLocks)
		{
			CachedLfsLocks = Output.LfsLocks;
			bHasCachedLfsLocks = true;
			bAnyStateChanged = true;
		}

		if (Output.bSuccess && Output.StatusSnapshot.IsSet() && bHasCachedLfsLocks)
		{
			ApplyLfsLocksToStates(Output.StatusSnapshot, CachedLfsLocks);
			bAnyStateChanged = true;
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
				bAnyStateChanged = true;
			}
		}

		if (Command->Delegate.IsBound())
		{
			Command->Delegate.ExecuteIfBound(Command->Operation, Output.bSuccess ? ECommandResult::Succeeded : ECommandResult::Failed);
		}

		if (Output.bSuccess && Output.bShouldUpdateStatus)
		{
			const FSourceControlOperationRef UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
			Execute(UpdateOp, nullptr, Output.FilesToUpdateStatus, EConcurrency::Asynchronous, FSourceControlOperationComplete());
		}

		if (bAnyStateChanged)
		{
			SourceControlStateChanged.Broadcast();
		}
	}
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<SWidget> FUnrealGitSourceControlProvider::MakeSettingsWidget() const
{
	return SNullWidget::NullWidget;
}
#endif // SOURCE_CONTROL_WITH_SLATE
