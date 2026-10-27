// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "ISourceControlOperation.h"
#include "ISourceControlModule.h"
#include "Misc/Paths.h"
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
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCopyWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitDeleteWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitResolveWorker.h"
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
#include "UnrealGit/Private/UnrealGit/UI/SUnrealGitSettingsWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/SNullWidget.h"
#endif // SOURCE_CONTROL_WITH_SLATE

static FString FindRepositoryRootPath(const FString& StartDirectory)
{
	FString Current = FPaths::ConvertRelativePathToFull(StartDirectory);
	FPaths::NormalizeDirectoryName(Current);

	for (int32 Depth = 0; Depth < 64 && !Current.IsEmpty(); ++Depth)
	{
		const FString GitPath = FPaths::Combine(Current, TEXT(".git"));
		if (IFileManager::Get().DirectoryExists(*GitPath) || IFileManager::Get().FileExists(*GitPath))
		{
			return Current;
		}

		const FString Parent = FPaths::GetPath(Current);
		if (Parent.IsEmpty() || Parent == Current)
		{
			break;
		}
		Current = Parent;
	}

	return FString();
}

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

	// Filesystem-only repository detection so IsEnabled() is meaningful before any worker runs.
	RepoRoot = FindRepositoryRootPath(GetWorkingDirectoryHint());
	bRepositoryFound = !RepoRoot.IsEmpty();

	const FString SessionDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::DiffDir(), TEXT("UnrealGit"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
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
			bGitAvailable = Info.bGitAvailable;
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

	RepoRoot.Reset();
	bRepositoryFound = false;
	bGitAvailable = true;
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
		const FString BranchName = LocalEnv.BranchName.IsEmpty() ? FString(TEXT("unknown")) : LocalEnv.BranchName;
		const FString RemoteUrl = LocalEnv.RemoteUrl.IsEmpty() ? FString(TEXT("none")) : LocalEnv.RemoteUrl;
		const FString UserName = LocalEnv.UserName.IsEmpty() ? FString(TEXT("unknown")) : LocalEnv.UserName;
		const FString UserEmail = LocalEnv.UserEmail.IsEmpty() ? FString(TEXT("unknown")) : LocalEnv.UserEmail;

		return FText::FromString(FString::Printf(
			TEXT("Local repository: %s / Remote origin: %s / Branch: %s / User: %s / E-mail: %s / git %s, lfs %s"),
			*RepoRoot, *RemoteUrl, *BranchName, *UserName, *UserEmail, *LocalEnv.GitVersion, *LocalEnv.GitLfsVersion));
	}

	return FText::FromString(FString::Printf(TEXT("Local repository: %s"), *RepoRoot));
}

bool FUnrealGitSourceControlProvider::IsEnabled() const
{
	return bRepositoryFound;
}

bool FUnrealGitSourceControlProvider::IsAvailable() const
{
	return ProcessRunner.IsValid() && bGitAvailable;
}

TMap<ISourceControlProvider::EStatus, FString> FUnrealGitSourceControlProvider::GetStatus() const
{
	TMap<EStatus, FString> Status;

	Status.Add(EStatus::Enabled, IsEnabled() ? TEXT("true") : TEXT("false"));
	Status.Add(EStatus::Connected, (IsAvailable() && !RepoRoot.IsEmpty()) ? TEXT("true") : TEXT("false"));
	Status.Add(EStatus::Repository, RepoRoot);

	{
		FScopeLock Scope(&EnvironmentLock);
		if (EnvironmentInfo.IsSet())
		{
			Status.Add(EStatus::User, EnvironmentInfo->UserName);
			Status.Add(EStatus::Email, EnvironmentInfo->UserEmail);
			Status.Add(EStatus::ScmVersion, EnvironmentInfo->GitVersion);
			Status.Add(EStatus::Branch, EnvironmentInfo->BranchName);
			Status.Add(EStatus::Remote, EnvironmentInfo->RemoteUrl);
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

bool FUnrealGitSourceControlProvider::GetStateBranchAtIndex(const int32 /*BranchIndex*/, FString& /*OutBranchName*/) const
{
	return false;
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
	if (!IsEnabled())
	{
		return ECommandResult::Failed;
	}

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

		{
			FGitProcessRequest BranchRequest;
			BranchRequest.WorkingDirectory = WorkingDir;
			BranchRequest.Arguments = { TEXT("symbolic-ref"), TEXT("--short"), TEXT("--quiet"), TEXT("HEAD") };
			const FGitProcessResult Res = Runner->Run(BranchRequest);
			if (Res.ExitCode == 0)
			{
				Info.BranchName = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
			}
			else
			{
				FGitProcessRequest DetachedRequest;
				DetachedRequest.WorkingDirectory = WorkingDir;
				DetachedRequest.Arguments = { TEXT("log"), TEXT("-1"), TEXT("--format=%h") };
				const FGitProcessResult DetachedRes = Runner->Run(DetachedRequest);
				if (DetachedRes.ExitCode == 0)
				{
					const FString ShortHash = UnrealGit::Workers::BytesToTextUtf8Lossy(DetachedRes.StdOut).TrimStartAndEnd();
					if (!ShortHash.IsEmpty())
					{
						Info.BranchName = FString::Printf(TEXT("HEAD detached at %s"), *ShortHash);
					}
				}
			}
		}

		{
			FGitProcessRequest RemoteRequest;
			RemoteRequest.WorkingDirectory = WorkingDir;
			RemoteRequest.Arguments = { TEXT("remote"), TEXT("get-url"), TEXT("origin") };
			const FGitProcessResult Res = Runner->Run(RemoteRequest);
			if (Res.ExitCode == 0)
			{
				Info.RemoteUrl = UnrealGit::Workers::BytesToTextUtf8Lossy(Res.StdOut).TrimStartAndEnd();
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

	if (!IsEnabled() && OpName != "Connect")
	{
		LastErrorText = FText::FromString(TEXT("UnrealGit: not connected to a Git repository."));
		UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: Execute(%s) rejected: %s"), *OpName.ToString(), *LastErrorText.ToString());
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
	else if (OpName == "Delete")
	{
		Worker = MakeUnique<FGitDeleteWorker>();
	}
	else if (OpName == "Copy")
	{
		Worker = MakeUnique<FGitCopyWorker>();
	}
	else if (OpName == "Resolve")
	{
		Worker = MakeUnique<FGitResolveWorker>();
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

	if (InConcurrency == EConcurrency::Synchronous)
	{
		TFuture<FUnrealGitWorkerOutput> Future = Async(EAsyncExecution::ThreadPool, [Runner, WorkingDirHint, ExistingRepoRoot, Command]()
		{
			FUnrealGitWorkerOutput Output;
			Command->Worker->Execute(Runner, Command->ProviderSettings, WorkingDirHint, Command->bQueryLfsLocks, Command->Operation, Command->Files, ExistingRepoRoot, Output);
			return Output;
		});
		const FUnrealGitWorkerOutput Output = Future.Get();
		ReconcileWorkerOutput(Command->Operation, Command->Files, Command->Delegate, Output);
		UE_LOG(LogSourceControl, VeryVerbose, TEXT("UnrealGit: Executed operation %s synchronously (files=%d, success=%d)"),
			*OpName.ToString(), InFiles.Num(), Output.bSuccess ? 1 : 0);
		return Output.bSuccess ? ECommandResult::Succeeded : ECommandResult::Failed;
	}

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
		|| OpName == "Delete"
		|| OpName == "Copy"
		|| OpName == "Resolve"
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
	// Git does not use OS read-only flags; CheckOut only clears them defensively for LFS workflows.
	return false;
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
	// Untracked assets must be visible in the Submit Content dialog.
	return true;
}

bool FUnrealGitSourceControlProvider::UsesCheckout() const
{
	return true;
}

bool FUnrealGitSourceControlProvider::UsesSoftRevertOnDelete() const
{
	return false;
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

TOptional<bool> FUnrealGitSourceControlProvider::HasChangesToSync() const
{
	return TOptional<bool>();
}

TOptional<bool> FUnrealGitSourceControlProvider::HasChangesToCheckIn() const
{
	return TOptional<bool>();
}

FString FUnrealGitSourceControlProvider::GetGitExecutablePath() const
{
	return Settings.GitExecutable;
}

FGitEnvironmentInfo FUnrealGitSourceControlProvider::GetEnvironmentInfo() const
{
	FScopeLock Scope(&EnvironmentLock);
	return EnvironmentInfo.IsSet() ? EnvironmentInfo.GetValue() : FGitEnvironmentInfo();
}

FString FUnrealGitSourceControlProvider::GetRepositoryRoot() const
{
	return RepoRoot;
}

FString FUnrealGitSourceControlProvider::GetBranchName() const
{
	FScopeLock Scope(&EnvironmentLock);
	return EnvironmentInfo.IsSet() ? EnvironmentInfo->BranchName : FString();
}

FString FUnrealGitSourceControlProvider::GetRemoteUrl() const
{
	FScopeLock Scope(&EnvironmentLock);
	return EnvironmentInfo.IsSet() ? EnvironmentInfo->RemoteUrl : FString();
}

FText FUnrealGitSourceControlProvider::GetLastError() const
{
	return LastErrorText;
}

bool FUnrealGitSourceControlProvider::IsGitToolAvailable() const
{
	return bGitAvailable;
}

bool FUnrealGitSourceControlProvider::HasGitRepository() const
{
	return bRepositoryFound;
}

bool FUnrealGitSourceControlProvider::SetGitExecutableOverride(const FString& InExecutablePath, FText& OutError)
{
	OutError = FText::GetEmpty();

	const FString TrimmedPath = InExecutablePath.TrimStartAndEnd();
	if (!TrimmedPath.IsEmpty() && !FPaths::IsRelative(TrimmedPath) && !IFileManager::Get().FileExists(*TrimmedPath))
	{
		OutError = FText::FromString(FString::Printf(TEXT("Git executable not found: %s"), *TrimmedPath));
		return false;
	}

	UUnrealGitUserSettings* UserSettings = GetMutableDefault<UUnrealGitUserSettings>();
	if (!UserSettings)
	{
		OutError = FText::FromString(TEXT("UnrealGit user settings are unavailable."));
		return false;
	}

	UserSettings->GitExecutableOverride.FilePath = TrimmedPath;
	UserSettings->SaveConfig();

	RefreshRepositoryState();
	return true;
}

void FUnrealGitSourceControlProvider::RefreshRepositoryState()
{
	LastErrorText = FText::GetEmpty();

	FText SettingsError;
	if (!FUnrealGitSettingsResolver::Build(Settings, SettingsError))
	{
		LastErrorText = SettingsError;
		UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Settings resolver failed: %s"), *SettingsError.ToString());
		Settings = FUnrealGitProviderSettings();
		Settings.GitExecutable = TEXT("git");
		Settings.RepositoryDiscoveryDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	ProcessRunner = MakeShared<FSystemGitProcessRunner, ESPMode::ThreadSafe>(Settings.GitExecutable);

	RepoRoot = FindRepositoryRootPath(GetWorkingDirectoryHint());
	bRepositoryFound = !RepoRoot.IsEmpty();
	NextLfsLocksRefreshUtc = FDateTime(0);

	StartEnvironmentValidation();
	if (EnvironmentFuture.IsValid())
	{
		const double SyncTimeoutSeconds = FSystemGitProcessRunner::DefaultTimeoutSeconds;
		if (EnvironmentFuture.WaitFor(FTimespan::FromSeconds(SyncTimeoutSeconds)))
		{
			const FGitEnvironmentInfo Info = EnvironmentFuture.Get();
			{
				FScopeLock Scope(&EnvironmentLock);
				EnvironmentInfo = Info;
			}
			bGitAvailable = Info.bGitAvailable;
		}
		else
		{
			UE_LOG(LogSourceControl, Warning, TEXT("UnrealGit: Environment validation timed out after %f seconds"), SyncTimeoutSeconds);
		}
		EnvironmentFuture = TFuture<FGitEnvironmentInfo>();
	}

	if (bRepositoryFound)
	{
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), nullptr, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete());
	}
	else
	{
		FScopeLock Lock(&StateCacheLock);
		StateCache.Reset();
	}
}

bool FUnrealGitSourceControlProvider::InitializeRepository(const FString& OriginUrl, bool /*bWriteGitIgnore*/, bool /*bWriteReadme*/, bool /*bWriteGitAttributes*/, bool bCreateInitialCommit, FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!ProcessRunner.IsValid())
	{
		OutError = FText::FromString(TEXT("UnrealGit process runner not initialized."));
		return false;
	}

	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> Runner = ProcessRunner.ToSharedRef();
	const FString WorkingDir = GetWorkingDirectoryHint();
	const FString TrimmedOriginUrl = OriginUrl.TrimStartAndEnd();

	TFuture<FString> Future = Async(EAsyncExecution::ThreadPool, [Runner, WorkingDir, TrimmedOriginUrl, bCreateInitialCommit]()
	{
		auto RunGit = [&Runner, &WorkingDir](const TArray<FString>& Arguments) -> FGitProcessResult
		{
			FGitProcessRequest Request;
			Request.WorkingDirectory = WorkingDir;
			Request.Arguments = Arguments;
			return Runner->Run(Request);
		};

		auto ToError = [](const FGitProcessResult& Result)
		{
			const FString StdErr = UnrealGit::Workers::BytesToTextUtf8Lossy(Result.StdErr).TrimStartAndEnd();
			return StdErr.IsEmpty()
				? FString::Printf(TEXT("git exited with code %d."), Result.ExitCode)
				: StdErr;
		};

		const FGitProcessResult InitResult = RunGit({ TEXT("init") });
		if (InitResult.ExitCode != 0)
		{
			return ToError(InitResult);
		}

		if (!TrimmedOriginUrl.IsEmpty())
		{
			const FGitProcessResult RemoteResult = RunGit({ TEXT("remote"), TEXT("add"), TEXT("origin"), TrimmedOriginUrl });
			if (RemoteResult.ExitCode != 0)
			{
				const FString RemoteError = ToError(RemoteResult);
				if (!RemoteError.Contains(TEXT("already exists")))
				{
					return RemoteError;
				}
			}
		}

		if (bCreateInitialCommit)
		{
			const FGitProcessResult AddResult = RunGit({ TEXT("add"), TEXT("-A") });
			if (AddResult.ExitCode != 0)
			{
				return ToError(AddResult);
			}

			const FGitProcessResult CommitResult = RunGit({ TEXT("commit"), TEXT("-m"), TEXT("Initial commit") });
			if (CommitResult.ExitCode != 0)
			{
				return ToError(CommitResult);
			}
		}

		return FString();
	});

	const FString Error = Future.Get();
	if (!Error.IsEmpty())
	{
		LastErrorText = FText::FromString(Error);
		OutError = LastErrorText;
		UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Repository initialization failed: %s"), *Error);
		return false;
	}

	RefreshRepositoryState();
	return true;
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

void FUnrealGitSourceControlProvider::ReconcileWorkerOutput(
	const FSourceControlOperationRef& Operation,
	const TArray<FString>& Files,
	const FSourceControlOperationComplete& Delegate,
	const FUnrealGitWorkerOutput& Output)
{
	bool bAnyStateChanged = false;
	const FString OperationName = Operation->GetName().ToString();

	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("Reconcile: Processing operation '%s', bSuccess=%d, HasStatusSnapshot=%d, Files=%d"),
		*OperationName, Output.bSuccess ? 1 : 0, Output.StatusSnapshot.IsSet() ? 1 : 0, Files.Num());

	if (Output.RepoRoot.IsSet())
	{
		RepoRoot = Output.RepoRoot.GetValue();
		bRepositoryFound = !RepoRoot.IsEmpty();
	}

	if (Output.bSuccess && Output.StatusSnapshot.IsSet())
	{
		const FGitBranchStatus& BranchStatus = Output.StatusSnapshot.GetValue().Branch;
		if (!BranchStatus.Head.IsEmpty())
		{
			FScopeLock Scope(&EnvironmentLock);
			if (EnvironmentInfo.IsSet())
			{
				EnvironmentInfo->BranchName = BranchStatus.Head;
			}
		}
	}

	if (Output.bSuccess && Output.StatusSnapshot.IsSet())
	{
		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("Reconcile: Calling UpdateStatesFromStatusSnapshot with %d files"), Output.StatusSnapshot.GetValue().Files.Num());
		UpdateStatesFromStatusSnapshot(Output.StatusSnapshot.GetValue(), Files);
		LastErrorText = FText::GetEmpty();
		bAnyStateChanged = true;
	}
	else if (!Output.bSuccess)
	{
		LastErrorText = Output.ErrorText;
		UE_LOG(LogSourceControl, Error, TEXT("UnrealGit: Operation %s failed: %s"), *OperationName, *LastErrorText.ToString());

		if (OperationName == TEXT("Connect"))
		{
			StaticCastSharedRef<FConnect>(Operation)->SetErrorText(LastErrorText);
		}

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

			// Link rename/copy revisions to the revision they branched from.
			for (int32 Index = 0; Index < Revisions.Num(); ++Index)
			{
				const TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>& Revision = Revisions[Index];
				if (Revision->GetAction() == TEXT("branch") && Revisions.IsValidIndex(Index + 1))
				{
					StaticCastSharedRef<FUnrealGitSourceControlRevision>(Revision)->SetBranchSource(Revisions[Index + 1]);
				}
			}

			bAnyStateChanged = true;
		}
	}

	if (Delegate.IsBound())
	{
		Delegate.ExecuteIfBound(Operation, Output.bSuccess ? ECommandResult::Succeeded : ECommandResult::Failed);
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

void FUnrealGitSourceControlProvider::Tick()
{
	if (EnvironmentFuture.IsValid() && EnvironmentFuture.IsReady())
	{
		const FGitEnvironmentInfo Info = EnvironmentFuture.Get();
		{
			FScopeLock Scope(&EnvironmentLock);
			EnvironmentInfo = Info;
		}
		bGitAvailable = Info.bGitAvailable;
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
		ReconcileWorkerOutput(Command->Operation, Command->Files, Command->Delegate, Output);
	}
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<SWidget> FUnrealGitSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SUnrealGitSettingsWidget).Provider(const_cast<FUnrealGitSourceControlProvider*>(this));
}
#endif // SOURCE_CONTROL_WITH_SLATE
