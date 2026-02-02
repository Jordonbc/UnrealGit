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
class FUnrealGitSourceControlProvider final : public ISourceControlProvider, public TSharedFromThis<FUnrealGitSourceControlProvider, ESPMode::ThreadSafe>
{
public:
	FUnrealGitSourceControlProvider();
	virtual ~FUnrealGitSourceControlProvider() override;

	// ISourceControlProvider
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual const FName& GetName() const override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;

	virtual FSourceControlStatePtr GetState(const FString& Filename, EStateCacheUsage::Type InStateCacheUsage) override;
	virtual FSourceControlStatePtr GetState(const FSourceControlChangelistPtr& InChangelist, const FString& Filename) override;
	virtual TArray<FSourceControlStateRef> GetState(const TArray<FString>& InFiles, EStateCacheUsage::Type InStateCacheUsage) override;

	virtual ECommandResult::Type Execute(
		const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation,
		const TArray<FString>& InFiles,
		EConcurrency::Type InConcurrency = EConcurrency::Asynchronous,
		const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete()) override;

	virtual bool CanCancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation) const override;
	virtual void CancelOperation(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation) override;

	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool Tick(float DeltaTime) override;

private:
	struct FCommand;

	void UpdateStatesFromStatusSnapshot(const struct FGitStatusSnapshot& Snapshot, const TArray<FString>& RequestedFiles);
	void ApplyLfsLocksToStates(const TOptional<struct FGitStatusSnapshot>& Snapshot, const TArray<struct FGitLfsLock>& Locks);

	FSourceControlStateRef GetOrCreateStateInternal(const FString& AbsoluteFilename);
	FString GetWorkingDirectoryHint() const;
	void HandleSettingsObjectChanged(UObject* ObjectBeingModified, struct FPropertyChangedEvent& PropertyChangedEvent);
	void StartEnvironmentValidation();

private:
	mutable FCriticalSection StateCacheLock;
	TMap<FString, FSourceControlStateRef> StateCache;

	mutable FCriticalSection CommandLock;
	TArray<TSharedRef<FCommand, ESPMode::ThreadSafe>> Commands;

	FString RepoRoot;
	FText LastErrorText;
	FUnrealGitProviderSettings Settings;

	TSharedPtr<IGitProcessRunner, ESPMode::ThreadSafe> ProcessRunner;
	TSharedPtr<FGitRevisionMaterializer, ESPMode::ThreadSafe> RevisionMaterializer;

	mutable FCriticalSection EnvironmentLock;
	TOptional<FGitEnvironmentInfo> EnvironmentInfo;
	TFuture<FGitEnvironmentInfo> EnvironmentFuture;

	FDelegateHandle SettingsChangedHandle;

	FDateTime NextLfsLocksRefreshUtc = FDateTime(0);
	bool bHasCachedLfsLocks = false;
	TArray<struct FGitLfsLock> CachedLfsLocks;
};
