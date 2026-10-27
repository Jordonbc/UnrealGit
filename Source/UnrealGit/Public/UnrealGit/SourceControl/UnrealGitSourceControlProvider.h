// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "UnrealGit/Git/GitEnvironmentInfo.h"
#include "UnrealGit/Git/GitModels.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"

class IGitProcessRunner;
class FUnrealGitSourceControlState;
class FGitRevisionMaterializer;

/**
 * @class FUnrealGitSourceControlProvider
 * Unreal Source Control provider backed by the system Git CLI and Git LFS locks.
 *
 * Responsibility:
 * - Implements Unreal's provider interface and dispatches operations to background workers.
 * - Maintains a state cache for fast icon/status queries.
 *
 * Threading:
 * - Process execution is offloaded to a background thread, but synchronous callers
 *   (Execute with EConcurrency::Synchronous, Init, RefreshRepositoryState) block
 *   until the spawned future completes (Init/RefreshRepositoryState wait up to
 *   5 seconds for environment validation).
 * - Provider public API methods are called on the game thread unless noted otherwise.
 */
class FUnrealGitSourceControlProvider final : public ISourceControlProvider
{
public:
	FUnrealGitSourceControlProvider();
	virtual ~FUnrealGitSourceControlProvider() override;

	// ISourceControlProvider
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual const FName& GetName() const override;
	virtual FText GetStatusText() const override;
	virtual TMap<EStatus, FString> GetStatus() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;

	virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) override;
	virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRoot) override;
	virtual int32 GetStateBranchIndex(const FString& BranchName) const override;
	virtual bool GetStateBranchAtIndex(int32 BranchIndex, FString& OutBranchName) const override;

	virtual ECommandResult::Type GetState(const TArray<FString>& InFiles, TArray<FSourceControlStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
	virtual ECommandResult::Type GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;

	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
	virtual void UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle) override;

	virtual ECommandResult::Type Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete()) override;
	virtual bool CanExecuteOperation(const FSourceControlOperationRef& InOperation) const override;
	virtual bool CanCancelOperation(const FSourceControlOperationRef& InOperation) const override;
	virtual void CancelOperation(const FSourceControlOperationRef& InOperation) override;

	virtual TArray<TSharedRef<class ISourceControlLabel>> GetLabels(const FString& InMatchingSpec) const override;
	virtual TArray<FSourceControlChangelistRef> GetChangelists(EStateCacheUsage::Type InStateCacheUsage) override;

	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual bool UsesUncontrolledChangelists() const override;
	virtual bool UsesCheckout() const override;
	virtual bool UsesSoftRevertOnDelete() const override;
	virtual bool UsesFileRevisions() const override;
	virtual bool UsesSnapshots() const override;
	virtual bool AllowsDiffAgainstDepot() const override;
	virtual TOptional<bool> HasChangesToSync() const override;
	virtual TOptional<bool> HasChangesToCheckIn() const override;
	virtual void Tick() override;

#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif // SOURCE_CONTROL_WITH_SLATE

	// Settings widget support

	/**
	 * Resolved git executable (user override or detected install).
	 *
	 * @return Path to the git executable in use.
	 */
	FString GetGitExecutablePath() const;

	/**
	 * Snapshot copy of the last environment validation result.
	 *
	 * @return Copy of the cached environment info.
	 */
	FGitEnvironmentInfo GetEnvironmentInfo() const;

	/**
	 * Absolute repository root, or an empty string when no repository was found.
	 *
	 * @return Repository root path.
	 */
	FString GetRepositoryRoot() const;

	/** Current branch name reported by the last status snapshot.
	 * @return Current branch name. */
	FString GetBranchName() const;

	/** Remote origin URL discovered from the repository.
	 * @return Remote origin URL. */
	FString GetRemoteUrl() const;

	/** Last error text produced by the provider.
	 * @return Last error text. */
	FText GetLastError() const;

	/**
	 * True when git itself was found and validated.
	 *
	 * @return True when git is available.
	 */
	bool IsGitToolAvailable() const;

	/**
	 * True when a .git directory/file was found at or above the repository discovery directory.
	 *
	 * @return True when a repository was found.
	 */
	bool HasGitRepository() const;

	/**
	 * Persists the git executable override to user settings and reinitializes discovery.
	 *
	 * @param InExecutablePath Path to the git executable to use.
	 * @param OutError Error text on failure.
	 * @return True on success, false otherwise.
	 */
	bool SetGitExecutableOverride(const FString& InExecutablePath, FText& OutError);

	/**
	 * Initializes a git repository in the repository discovery directory.
	 *
	 * Blocking: runs git on a worker thread internally. Call from the game thread
	 * (the settings widget does), not from another worker.
	 *
	 * @param OriginUrl Remote origin URL to configure.
	 * @param bWriteGitIgnore Write a standard Unreal .gitignore before the initial commit.
	 * @param bWriteReadme Write a project README.md before the initial commit.
	 * @param bWriteGitAttributes Write a Git LFS .gitattributes before the initial commit.
	 * @param bCreateInitialCommit Whether to create an initial commit.
	 * @param OutError Error text on failure.
	 * @return True on success, false otherwise.
	 */
	bool InitializeRepository(const FString& OriginUrl, bool bWriteGitIgnore, bool bWriteReadme, bool bWriteGitAttributes, bool bCreateInitialCommit, FText& OutError);

	/**
	 * Prints provider/state diagnostics at Warning level (visible with the current log category settings).
	 *
	 * Mutates state: calls Init(true) when the process runner is not initialized, and
	 * forces a synchronous status update. May block. Call from the game thread.
	 */
	void LogDiagnostics();

private:
	/** @struct FCommand Internal queued command entry. */
	struct FCommand;

	/**
	 * Re-resolves runtime settings, validates the environment, and triggers a status refresh.
	 */
	void RefreshRepositoryState();

	/**
	 * Builds provider settings and the process runner from user/project settings.
	 */
	void ResolveRuntimeSettings();

	/**
	 * Stores validated environment info and updates git availability and user identity.
	 *
	 * @param Info Environment info produced by validation.
	 */
	void ApplyEnvironmentInfo(const FGitEnvironmentInfo& Info);

	/**
	 * Updates cached source control states from a parsed status snapshot.
	 *
	 * @param Snapshot Parsed git status snapshot.
	 * @param RequestedFiles Files whose state should be updated.
	 */
	void UpdateStatesFromStatusSnapshot(const struct FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles);

	/**
	 * Applies Git LFS lock ownership to cached states.
	 *
	 * @param Snapshot Optional status snapshot used for context.
	 * @param Locks Parsed LFS locks.
	 */
	void ApplyLfsLocksToStates(const TOptional<struct FGitStatusSnapshot>& Snapshot, const TArray<struct FGitLfsLock>& Locks);

	/**
	 * Applies worker output to cached state and notifies the completion delegate.
	 *
	 * @param Operation Source control operation being reconciled.
	 * @param Files Files affected by the operation.
	 * @param Delegate Completion delegate to invoke.
	 * @param Output Worker output to apply.
	 */
	void ReconcileWorkerOutput(const FSourceControlOperationRef& Operation, const TArray<FString>& Files, const FSourceControlOperationComplete& Delegate, const struct FUnrealGitWorkerOutput& Output);

	/**
	 * Applies a status snapshot from worker output to cached state.
	 *
	 * @param Operation Source control operation being applied.
	 * @param Files Files affected by the operation.
	 * @param Output Worker output containing the snapshot.
	 * @param bAnyStateChanged Set to true when any cached state changed.
	 */
	void ApplyStatusSnapshot(const FSourceControlOperationRef& Operation, const TArray<FString>& Files, const FUnrealGitWorkerOutput& Output, bool& bAnyStateChanged);

	/**
	 * Applies lock state from worker output to cached state.
	 *
	 * @param Operation Source control operation being applied.
	 * @param Files Files affected by the operation.
	 * @param Output Worker output containing lock state.
	 * @param bAnyStateChanged Set to true when any cached state changed.
	 */
	void ApplyLockState(const FSourceControlOperationRef& Operation, const TArray<FString>& Files, const FUnrealGitWorkerOutput& Output, bool& bAnyStateChanged);

	/**
	 * Applies read-only file policy based on worker output.
	 *
	 * @param Output Worker output containing policy-relevant data.
	 */
	void ApplyReadOnlyPolicy(const FUnrealGitWorkerOutput& Output);

	/**
	 * Builds revision history from worker output.
	 *
	 * @param Output Worker output containing history data.
	 * @param bAnyStateChanged Set to true when any cached state changed.
	 */
	void BuildHistory(const FUnrealGitWorkerOutput& Output, bool& bAnyStateChanged);

	/**
	 * Notifies the completion delegate after reconciling worker output.
	 *
	 * @param Operation Source control operation being completed.
	 * @param Delegate Completion delegate to invoke.
	 * @param Output Worker output to report.
	 * @param bAnyStateChanged Whether any cached state changed.
	 */
	void NotifyDelegate(const FSourceControlOperationRef& Operation, const FSourceControlOperationComplete& Delegate, const FUnrealGitWorkerOutput& Output, bool& bAnyStateChanged);

	/**
	 * Applies checkout/lock state to the given files after a successful operation.
	 *
	 * @param Files Files to update.
	 * @param bCheckedOut Whether the files are checked out.
	 * @param LockState Lock state to apply.
	 * @param bAnyStateChanged Set to true when any cached state changed.
	 */
	void ApplyOperationStateToFiles(const TArray<FString>& Files, bool bCheckedOut, EGitLockState LockState, bool& bAnyStateChanged);

	/**
	 * Returns an existing cached state for a file or creates one.
	 *
	 * @param AbsoluteFilename Absolute path of the file.
	 * @return Cached source control state reference.
	 */
	FSourceControlStateRef GetOrCreateStateInternal(const FString& AbsoluteFilename);

	/**
	 * Returns the working directory hint used for repository discovery.
	 *
	 * @return Working directory path.
	 */
	FString GetWorkingDirectoryHint() const;

	/**
	 * Starts asynchronous environment validation.
	 */
	void StartEnvironmentValidation();

private:
	mutable FCriticalSection StateCacheLock;
	TMap<FString, FSourceControlStateRef> StateCache;

	mutable FCriticalSection CommandLock;
	TArray<TSharedRef<FCommand, ESPMode::ThreadSafe>> Commands;

	mutable FCriticalSection SyncOperationLock;

	FString RepoRoot;
	FText LastErrorText;
	FUnrealGitProviderSettings Settings;

	bool bGitAvailable = true;
	bool bRepositoryFound = false;

	TSharedPtr<IGitProcessRunner, ESPMode::ThreadSafe> ProcessRunner;
	TSharedPtr<FGitRevisionMaterializer, ESPMode::ThreadSafe> RevisionMaterializer;

	mutable FCriticalSection EnvironmentLock;
	TOptional<FGitEnvironmentInfo> EnvironmentInfo;
	TFuture<FGitEnvironmentInfo> EnvironmentFuture;
	FSourceControlStateChanged SourceControlStateChanged;

	FDateTime NextLfsLocksRefreshUtc = FDateTime(0);
	FDateTime NextPeriodicRefreshUtc = FDateTime(0);
	int32 LastReportedLfsLockCount = -1;
	bool bHasCachedLfsLocks = false;
	TArray<struct FGitLfsLock> CachedLfsLocks;
};
