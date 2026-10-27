// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
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
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitForceUnlockWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitResolveWorker.h"
#include "UnrealGit/SourceControl/Workers/GitCheckInWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCheckOutWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitGetHistoryWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitMarkForAddWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitRevertWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitSyncWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitUpdateStatusWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitWorkerUtils.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlState.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlRevision.h"

#if SOURCE_CONTROL_WITH_SLATE
#include "UnrealGit/Private/UnrealGit/UI/SUnrealGitSettingsWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/SNullWidget.h"
#endif // SOURCE_CONTROL_WITH_SLATE

static void ApplyLocalReadOnlyState(const FString& AbsoluteFilename, bool bShouldBeReadOnly)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*AbsoluteFilename))
	{
		return;
	}

	if (PlatformFile.IsReadOnly(*AbsoluteFilename) != bShouldBeReadOnly)
	{
		PlatformFile.SetReadOnly(*AbsoluteFilename, bShouldBeReadOnly);
	}
}

static bool DoesLockOwnerMatchCurrentUser(const FString& Owner, const FString& UserName, const FString& UserEmail)
{
	if (Owner.IsEmpty())
	{
		return false;
	}

	if (Owner.Equals(UserName, ESearchCase::IgnoreCase) || Owner.Equals(UserEmail, ESearchCase::IgnoreCase))
	{
		return true;
	}

	FString EmailLocalPart = UserEmail;
	int32 AtIndex = INDEX_NONE;
	if (EmailLocalPart.FindChar(TEXT('@'), AtIndex))
	{
		EmailLocalPart.LeftInline(AtIndex);
	}

	// LFS server accounts often differ slightly from the git identity (e.g. "Jordon" vs "jordonbc"),
	// so accept a unique-enough prefix match in either direction.
	const bool bEmailPrefixMatches = EmailLocalPart.Len() >= 3
		&& (Owner.StartsWith(EmailLocalPart, ESearchCase::IgnoreCase) || EmailLocalPart.StartsWith(Owner, ESearchCase::IgnoreCase));
	const bool bUserNamePrefixMatches = UserName.Len() >= 3
		&& (Owner.StartsWith(UserName, ESearchCase::IgnoreCase) || UserName.StartsWith(Owner, ESearchCase::IgnoreCase));

	return bEmailPrefixMatches || bUserNamePrefixMatches;
}

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

void FUnrealGitSourceControlProvider::ResolveRuntimeSettings()
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

	// Filesystem-only repository detection so IsEnabled() is meaningful before any worker runs.
	RepoRoot = FindRepositoryRootPath(GetWorkingDirectoryHint());
	bRepositoryFound = !RepoRoot.IsEmpty();
	NextLfsLocksRefreshUtc = FDateTime(0);
	NextPeriodicRefreshUtc = FDateTime(0);
}

void FUnrealGitSourceControlProvider::ApplyEnvironmentInfo(const FGitEnvironmentInfo& Info)
{
	{
		FScopeLock Scope(&EnvironmentLock);
		EnvironmentInfo = Info;
	}

	bGitAvailable = Info.bGitAvailable;
	Settings.UserName = Info.UserName;
	Settings.UserEmail = Info.UserEmail;
}

void FUnrealGitSourceControlProvider::Init(bool bForceConnection)
{
	UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Init (bForceConnection=%s)"), bForceConnection ? TEXT("true") : TEXT("false"));

	ResolveRuntimeSettings();

	UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: GitExecutable=\"%s\" RepoDiscoveryDir=\"%s\" EnableLfsLocks=%s"),
		*Settings.GitExecutable,
		*Settings.RepositoryDiscoveryDirectory,
		Settings.bEnableLfsLocks ? TEXT("true") : TEXT("false"));

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
			ApplyEnvironmentInfo(Info);
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
			TEXT("Local repository: %s / Remote origin: %s / Branch: %s / User: %s / E-mail: %s / git %s, lfs %s / LFS locks: %d"),
			*RepoRoot, *RemoteUrl, *BranchName, *UserName, *UserEmail, *LocalEnv.GitVersion, *LocalEnv.GitLfsVersion,
			bHasCachedLfsLocks ? CachedLfsLocks.Num() : 0));
	}

	return FText::FromString(FString::Printf(TEXT("Local repository: %s / LFS locks: %d"), *RepoRoot, bHasCachedLfsLocks ? CachedLfsLocks.Num() : 0));
}

bool FUnrealGitSourceControlProvider::IsEnabled() const
{
	// Keep the provider selectable whenever it initialized: repository detection is
	// filesystem-based and must never disable the editor's Source Control menu.
	return ProcessRunner.IsValid();
}

bool FUnrealGitSourceControlProvider::IsAvailable() const
{
	return ProcessRunner.IsValid();
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

void FUnrealGitSourceControlProvider::ApplyOperationStateToFiles(const TArray<FString>& Files, bool bCheckedOut, EGitLockState InLockState, bool& bAnyStateChanged)
{
	for (const FString& File : Files)
	{
		const FString Abs = FPaths::ConvertRelativePathToFull(File);
		const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
		const TSharedRef<FUnrealGitSourceControlState> State = StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef);
		State->SetCheckedOut(bCheckedOut);
		State->SetLockState(InLockState, FString());
		bAnyStateChanged = true;
	}
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

	if ((InStateCacheUsage == EStateCacheUsage::ForceUpdate || bAnyInvalid) && !bUpdateInProgress)
	{
		// A full-repo status in one command beats trickling one request per asset.
		const bool bFullRepoSweep = (InStateCacheUsage != EStateCacheUsage::ForceUpdate);
		const FSourceControlOperationRef UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
		Execute(UpdateOp, nullptr, bFullRepoSweep ? TArray<FString>() : AbsFiles, EConcurrency::Asynchronous, FSourceControlOperationComplete());
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
	else if (OpName == "ForceUnlock")
	{
		Worker = MakeUnique<FGitForceUnlockWorker>();
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

	// CheckOut is network-bound (one `git lfs lock` per file). Never block the game thread for it:
	// return immediately with optimistic state and let the worker finish in the background.
	const bool bAsyncCheckOut = (InConcurrency == EConcurrency::Synchronous) && OpName == "CheckOut" && InFiles.Num() > 0;
	if (bAsyncCheckOut)
	{
		for (const FString& File : InFiles)
		{
			const FString Abs = FPaths::ConvertRelativePathToFull(File);
			const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
			StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->SetCheckedOut(true);

			// Clear read-only now so an immediate save cannot race the background worker.
			ApplyLocalReadOnlyState(Abs, false);
		}

		SourceControlStateChanged.Broadcast();
	}

	if (InConcurrency == EConcurrency::Synchronous && !bAsyncCheckOut)
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
		|| OpName == "ForceUnlock";
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
	// The editor uses this to decide whether to drive the check-out/read-only flow.
	// CheckOut is our hook for LFS lock-on-checkout, so it must remain enabled.
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
	ResolveRuntimeSettings();

	StartEnvironmentValidation();
	if (EnvironmentFuture.IsValid())
	{
		const double SyncTimeoutSeconds = FSystemGitProcessRunner::DefaultTimeoutSeconds;
		if (EnvironmentFuture.WaitFor(FTimespan::FromSeconds(SyncTimeoutSeconds)))
		{
			const FGitEnvironmentInfo Info = EnvironmentFuture.Get();
			ApplyEnvironmentInfo(Info);
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

void FUnrealGitSourceControlProvider::LogDiagnostics()
{
	if (!ProcessRunner.IsValid())
	{
		Init(true);
	}

	if (!ProcessRunner.IsValid())
	{
		UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: diagnostics: provider not initialized."));
		return;
	}

	NextLfsLocksRefreshUtc = FDateTime(0);
	const FSourceControlOperationRef UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
	Execute(UpdateOp, nullptr, TArray<FString>(), EConcurrency::Synchronous, FSourceControlOperationComplete());

	UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: diagnostics: enabled=%d available=%d repo=\"%s\""),
		IsEnabled() ? 1 : 0,
		IsAvailable() ? 1 : 0,
		*RepoRoot);
	UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: diagnostics: status=\"%s\""), *GetStatusText().ToString());

	int32 TotalStates = 0;
	int32 CheckedOutStates = 0;
	int32 CheckedOutOtherStates = 0;
	{
		FScopeLock Lock(&StateCacheLock);
		TotalStates = StateCache.Num();
		for (const TPair<FString, FSourceControlStateRef>& Pair : StateCache)
		{
			if (Pair.Value->IsCheckedOut())
			{
				++CheckedOutStates;
			}

			FString Who;
			if (Pair.Value->IsCheckedOutOther(&Who))
			{
				++CheckedOutOtherStates;
			}
		}
	}

	UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: diagnostics: states=%d checkedOut=%d checkedOutOther=%d cachedLocks=%d"),
		TotalStates,
		CheckedOutStates,
		CheckedOutOtherStates,
		bHasCachedLfsLocks ? CachedLfsLocks.Num() : -1);

	for (const FGitLfsLock& LockInfo : CachedLfsLocks)
	{
		const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / LockInfo.Path);
		const FSourceControlStatePtr State = ISourceControlProvider::GetState(Abs, EStateCacheUsage::Use);

		FString Who;
		const bool bCheckedOutOther = State.IsValid() && State->IsCheckedOutOther(&Who);
		UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: diagnostics: lock=\"%s\" key=\"%s\" tracked=%d checkedOut=%d other=%d owner=\"%s\" status=\"%s\""),
			*LockInfo.Path,
			*Abs,
			(State.IsValid() && State->IsSourceControlled()) ? 1 : 0,
			(State.IsValid() && State->IsCheckedOut()) ? 1 : 0,
			bCheckedOutOther ? 1 : 0,
			*Who,
			State.IsValid() ? *State->GetStatusText().Get(FText::GetEmpty()).ToString() : TEXT("<none>"));
	}
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

	UE_LOG(LogUnrealGit, Verbose, TEXT("UpdateStatesFromStatusSnapshot: refreshed %d file state(s) (requested %d)"),
		Snapshot.Files.Num(),
		RequestedFiles.Num());
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
		FString Rel = UnrealGit::Workers::NormalizePathSeparators(Lock.Path);
		const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / Rel);

		const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);

		EGitLockState LockState = EGitLockState::LockedByOther;
		const bool bOwnerMatches = DoesLockOwnerMatchCurrentUser(Lock.OwnerName, MyName, MyEmail)
			|| DoesLockOwnerMatchCurrentUser(Lock.OwnerEmail, MyName, MyEmail);
		const bool bCheckedOutByMe = StaticCastSharedRef<FUnrealGitSourceControlState>(StateRef)->IsCheckedOutByMe();
		if (bOwnerMatches || bCheckedOutByMe)
		{
			LockState = EGitLockState::LockedByMe;
		}

		FString Owner = !Lock.OwnerName.IsEmpty() ? Lock.OwnerName : Lock.OwnerEmail;
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

		if (OperationName == TEXT("CheckOut"))
		{
			// Roll back the optimistic checkout state.
			ApplyOperationStateToFiles(Files, false, EGitLockState::NotLocked, bAnyStateChanged);
		}

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

	if (Output.bSuccess && OperationName == TEXT("CheckOut"))
	{
		ApplyOperationStateToFiles(Files, true, EGitLockState::LockedByMe, bAnyStateChanged);
	}
	else if (Output.bSuccess && (OperationName == TEXT("CheckIn") || OperationName == TEXT("Revert") || OperationName == TEXT("Delete") || OperationName == TEXT("ForceUnlock")))
	{
		ApplyOperationStateToFiles(Files, false, EGitLockState::NotLocked, bAnyStateChanged);
	}

	if (Output.bSuccess && (OperationName == TEXT("CheckOut") || OperationName == TEXT("CheckIn") || OperationName == TEXT("Revert") || OperationName == TEXT("ForceUnlock")))
	{
		// Checkout/checkin/unlock change LFS lock ownership, so refresh locks with the next status query.
		NextLfsLocksRefreshUtc = FDateTime(0);
	}

	if (Output.bSuccess && Output.bHasLfsLocks)
	{
		CachedLfsLocks = Output.LfsLocks;
		bHasCachedLfsLocks = true;
		bAnyStateChanged = true;
	}

	if (Output.bSuccess && bHasCachedLfsLocks && (Output.bHasLfsLocks || Output.StatusSnapshot.IsSet()))
	{
		ApplyLfsLocksToStates(Output.StatusSnapshot, CachedLfsLocks);
		bAnyStateChanged = true;
	}

	if (Output.bSuccess && Output.bHasLfsLocks)
	{
		// One-shot diagnostic, only when the lock set changes.
		const int32 LockCount = CachedLfsLocks.Num();
		if (LockCount != LastReportedLfsLockCount)
		{
			LastReportedLfsLockCount = LockCount;

			int32 CachedStateCount = 0;
			{
				FScopeLock Lock(&StateCacheLock);
				CachedStateCount = StateCache.Num();
			}

			UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: locks=%d cachedStates=%d repo=\"%s\""), LockCount, CachedStateCount, *RepoRoot);

			if (LockCount > 0)
			{
				const FGitLfsLock& FirstLock = CachedLfsLocks[0];
				const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / FirstLock.Path);
				const FSourceControlStatePtr State = ISourceControlProvider::GetState(Abs, EStateCacheUsage::Use);
				FString Who;
				const bool bOther = State.IsValid() && State->IsCheckedOutOther(&Who);

				UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: first lock=\"%s\" key=\"%s\" tracked=%d checkedOut=%d other=%d owner=\"%s\" status=\"%s\""),
					*FirstLock.Path,
					*Abs,
					(State.IsValid() && State->IsSourceControlled()) ? 1 : 0,
					(State.IsValid() && State->IsCheckedOut()) ? 1 : 0,
					bOther ? 1 : 0,
					*Who,
					State.IsValid() ? *State->GetStatusText().Get(FText::GetEmpty()).ToString() : TEXT("<none>"));
			}
		}
	}

	if (Output.bSuccess && Output.StatusSnapshot.IsSet())
	{
		// Apply read-only state only after lock state is final: a checked-out or locked file
		// must stay writable, and this ordering avoids briefly marking it read-only.
		for (const FGitFileStatus& FileStatus : Output.StatusSnapshot.GetValue().Files)
		{
			if (!FileStatus.bIsTracked || !FileStatus.RelativePath.StartsWith(TEXT("Content/"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FString Abs = FPaths::ConvertRelativePathToFull(RepoRoot / FileStatus.RelativePath);
			const FSourceControlStateRef StateRef = GetOrCreateStateInternal(Abs);
			ApplyLocalReadOnlyState(Abs, !StateRef->IsCheckedOut() && !StateRef->IsConflicted());
		}
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
		ApplyEnvironmentInfo(Info);
		UE_LOG(LogSourceControl, Log, TEXT("UnrealGit: Environment: GitAvailable=%s GitVersion=\"%s\" LfsAvailable=%s LfsVersion=\"%s\" User=\"%s\" Email=\"%s\""),
			Info.bGitAvailable ? TEXT("true") : TEXT("false"),
			*Info.GitVersion,
			Info.bGitLfsAvailable ? TEXT("true") : TEXT("false"),
			*Info.GitLfsVersion,
			*Info.UserName,
			*Info.UserEmail);
		EnvironmentFuture = TFuture<FGitEnvironmentInfo>();
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	if (bRepositoryFound && NowUtc >= NextPeriodicRefreshUtc)
	{
		bool bCommandsIdle = false;
		{
			FScopeLock Lock(&CommandLock);
			bCommandsIdle = Commands.Num() == 0;
		}

		if (bCommandsIdle)
		{
			// Periodically push status (and lock ownership) so the editor's SCC UI reflects
			// changes even when it does not explicitly request a refresh.
			NextPeriodicRefreshUtc = NowUtc + FTimespan::FromSeconds(30);
			Execute(ISourceControlOperation::Create<FUpdateStatus>(), nullptr, TArray<FString>(), EConcurrency::Asynchronous, FSourceControlOperationComplete());
		}
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
