// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlState.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * Source control state for a single file as reported by Git.
 *
 * Responsibility:
 * - Stores state used for icons, context menus, and history.
 * - Acts as a UI-friendly view over the domain-layer Git status.
 *
 * Threading:
 * - Instances are updated on the game thread when background operations complete.
 */
class FUnrealGitSourceControlState final : public ISourceControlState
{
public:
	explicit FUnrealGitSourceControlState(FString InAbsoluteFilename);

	void UpdateFromStatus(const FGitFileStatus& InStatus);
	void SetLockState(EGitLockState InLockState, FString InLockOwner);
	void SetHistory(const TArray<TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe>>& InHistory);

	// ISourceControlState
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem(int32 HistoryIndex) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(int32 RevisionNumber) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(const FString& InRevision) const override;

	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetCurrentRevision() const override;

#if SOURCE_CONTROL_WITH_SLATE
	virtual FSlateIcon GetIcon() const override;
#endif // SOURCE_CONTROL_WITH_SLATE

	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;

	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = nullptr) const override;
	virtual bool IsCheckedOutInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual bool IsModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual bool IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual TArray<FString> GetCheckedOutBranches() const override;
	virtual FString GetOtherUserBranchCheckedOuts() const override;
	virtual bool GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const override;
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool CanDelete() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool IsConflicted() const override;
	virtual bool CanRevert() const override;

private:
	FString AbsoluteFilename;
	FDateTime TimeStamp;

	EGitFileState FileState = EGitFileState::Unknown;
	bool bIsStaged = false;
	bool bIsUnstaged = false;
	bool bIsTracked = false;
	bool bIsConflicted = false;

	EGitLockState LockState = EGitLockState::Unknown;
	FString LockOwner;

	TArray<TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe>> History;
};
