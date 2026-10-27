// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"
#include "UnrealGit/Git/GitEnvironmentInfo.h"
#include "UnrealGit/Git/GitModels.h"

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
 * - All Git operations run asynchronously on a background thread.
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

	FString GetBranchName() const;
	FString GetRemoteUrl() const;
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
	 * @param bWriteGitIgnore Whether to write a .gitignore file.
	 * @param bWriteReadme Whether to write a README file.
	 * @param bWriteGitAttributes Whether to write a .gitattributes file.
	 * @param bCreateInitialCommit Whether to create an initial commit.
	 * @param OutError Error text on failure.
	 * @return True on success, false otherwise.
	 */
	bool InitializeRepository(const FString& OriginUrl, bool bWriteGitIgnore, bool bWriteReadme, bool bWriteGitAttributes, bool bCreateInitialCommit, FText& OutError);

	/**
	 * Prints provider/state diagnostics at Warning level (visible with the current log category settings).
	 */
	void LogDiagnostics();

private:
	/** @struct FCommand Internal queued command entry. */
	struct FCommand;

	void RefreshRepositoryState();
	void ResolveRuntimeSettings();
	void ApplyEnvironmentInfo(const FGitEnvironmentInfo& Info);
	void UpdateStatesFromStatusSnapshot(const struct FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles);
	void ApplyLfsLocksToStates(const TOptional<struct FGitStatusSnapshot>& Snapshot, const TArray<struct FGitLfsLock>& Locks);
	void ReconcileWorkerOutput(const FSourceControlOperationRef& Operation, const TArray<FString>& Files, const FSourceControlOperationComplete& Delegate, const struct FUnrealGitWorkerOutput& Output);

	/** Applies checkout/lock state to the given files after a successful operation. */
	void ApplyOperationStateToFiles(const TArray<FString>& Files, bool bCheckedOut, EGitLockState LockState, bool& bAnyStateChanged);

	FSourceControlStateRef GetOrCreateStateInternal(const FString& AbsoluteFilename);
	FString GetWorkingDirectoryHint() const;
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
