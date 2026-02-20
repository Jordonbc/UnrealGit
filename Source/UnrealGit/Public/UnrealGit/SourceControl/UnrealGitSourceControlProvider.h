// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"
#include "UnrealGit/Git/GitEnvironmentInfo.h"

class IGitProcessRunner;
class FUnrealGitSourceControlState;
class FGitRevisionMaterializer;

/**
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
	virtual bool UsesFileRevisions() const override;
	virtual bool UsesSnapshots() const override;
	virtual bool AllowsDiffAgainstDepot() const override;
	virtual TOptional<bool> IsAtLatestRevision() const override;
	virtual TOptional<int> GetNumLocalChanges() const override;
	virtual void Tick() override;

#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif // SOURCE_CONTROL_WITH_SLATE

private:
	struct FCommand;

	void UpdateStatesFromStatusSnapshot(const struct FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles);
	void ApplyLfsLocksToStates(const TOptional<struct FGitStatusSnapshot>& Snapshot, const TArray<struct FGitLfsLock>& Locks);

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

	TSharedPtr<IGitProcessRunner, ESPMode::ThreadSafe> ProcessRunner;
	TSharedPtr<FGitRevisionMaterializer, ESPMode::ThreadSafe> RevisionMaterializer;

	mutable FCriticalSection EnvironmentLock;
	TOptional<FGitEnvironmentInfo> EnvironmentInfo;
	TFuture<FGitEnvironmentInfo> EnvironmentFuture;
	FSourceControlStateChanged SourceControlStateChanged;

	FDateTime NextLfsLocksRefreshUtc = FDateTime(0);
	bool bHasCachedLfsLocks = false;
	TArray<struct FGitLfsLock> CachedLfsLocks;
};
