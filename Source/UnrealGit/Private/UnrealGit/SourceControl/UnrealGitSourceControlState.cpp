// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlState.h"

#include "HAL/FileManager.h"

FUnrealGitSourceControlState::FUnrealGitSourceControlState(FString InAbsoluteFilename)
	: AbsoluteFilename(MoveTemp(InAbsoluteFilename))
	, TimeStamp(FDateTime::UtcNow())
{
}

void FUnrealGitSourceControlState::UpdateFromStatus(const FGitFileStatus& InStatus)
{
	TimeStamp = FDateTime::UtcNow();

	FileState = InStatus.State;
	bIsStaged = InStatus.bIsStaged;
	bIsUnstaged = InStatus.bIsUnstaged;
	bIsTracked = InStatus.bIsTracked;
	bIsConflicted = InStatus.bIsConflicted;
}

void FUnrealGitSourceControlState::SetLockState(EGitLockState InLockState, FString InLockOwner)
{
	LockState = InLockState;
	LockOwner = MoveTemp(InLockOwner);
}

void FUnrealGitSourceControlState::SetHistory(const TArray<TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>>& InHistory)
{
	History = InHistory;
}

int32 FUnrealGitSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlState::GetHistoryItem(int32 HistoryIndex) const
{
	return History.IsValidIndex(HistoryIndex) ? History[HistoryIndex] : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlState::FindHistoryRevision(int32 /*RevisionNumber*/) const
{
	return TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for (const TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>& Rev : History)
	{
		if (Rev->GetRevision() == InRevision)
		{
			return Rev;
		}
	}
	return TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

FName FUnrealGitSourceControlState::GetIconName() const
{
	if (bIsConflicted || FileState == EGitFileState::Conflicted)
	{
		return "SourceControl.StatusIcon.Conflicted";
	}

	if (FileState == EGitFileState::Added)
	{
		return "SourceControl.StatusIcon.OpenForAdd";
	}

	if (FileState == EGitFileState::Deleted)
	{
		return "SourceControl.StatusIcon.MarkedForDelete";
	}

	if (FileState == EGitFileState::Modified || bIsStaged || bIsUnstaged)
	{
		return "SourceControl.StatusIcon.CheckedOut";
	}

	if (FileState == EGitFileState::Untracked)
	{
		return "SourceControl.StatusIcon.NotInDepot";
	}

	if (FileState == EGitFileState::Ignored)
	{
		return "SourceControl.StatusIcon.NotInDepot";
	}

	if (FileState == EGitFileState::Unchanged && bIsTracked)
	{
		return "SourceControl.StatusIcon.Controlled";
	}

	return "SourceControl.StatusIcon.Unknown";
}

FName FUnrealGitSourceControlState::GetSmallIconName() const
{
	return GetIconName();
}

FText FUnrealGitSourceControlState::GetDisplayName() const
{
	return FText::FromString(FPaths::GetCleanFilename(AbsoluteFilename));
}

FText FUnrealGitSourceControlState::GetDisplayTooltip() const
{
	switch (FileState)
	{
	case EGitFileState::Added:
		return FText::FromString(TEXT("Added"));
	case EGitFileState::Modified:
		return FText::FromString(TEXT("Modified"));
	case EGitFileState::Deleted:
		return FText::FromString(TEXT("Deleted"));
	case EGitFileState::Renamed:
		return FText::FromString(TEXT("Renamed"));
	case EGitFileState::Conflicted:
		return FText::FromString(TEXT("Conflicted"));
	case EGitFileState::Untracked:
		return FText::FromString(TEXT("Untracked"));
	case EGitFileState::Ignored:
		return FText::FromString(TEXT("Ignored"));
	case EGitFileState::Unchanged:
		return FText::FromString(TEXT("Unchanged"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

const FString& FUnrealGitSourceControlState::GetFilename() const
{
	return AbsoluteFilename;
}

const FDateTime& FUnrealGitSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

bool FUnrealGitSourceControlState::CanCheckIn() const
{
	return CanRevert();
}

bool FUnrealGitSourceControlState::CanCheckout() const
{
	return bIsTracked && !IsLockedOther();
}

bool FUnrealGitSourceControlState::IsCheckedOut() const
{
	return bIsStaged || bIsUnstaged;
}

bool FUnrealGitSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (IsLockedOther())
	{
		if (Who)
		{
			*Who = LockOwner;
		}
		return true;
	}
	return false;
}

bool FUnrealGitSourceControlState::IsCheckedOutInOtherBranch(const FString& /*CurrentBranch*/) const
{
	return false;
}

bool FUnrealGitSourceControlState::IsCurrent() const
{
	return true;
}

bool FUnrealGitSourceControlState::IsSourceControlled() const
{
	return bIsTracked;
}

bool FUnrealGitSourceControlState::IsAdded() const
{
	return FileState == EGitFileState::Added;
}

bool FUnrealGitSourceControlState::IsDeleted() const
{
	return FileState == EGitFileState::Deleted;
}

bool FUnrealGitSourceControlState::IsIgnored() const
{
	return FileState == EGitFileState::Ignored;
}

bool FUnrealGitSourceControlState::CanEdit() const
{
	return !IsReadOnly();
}

bool FUnrealGitSourceControlState::IsModified() const
{
	return FileState == EGitFileState::Modified || bIsStaged || bIsUnstaged;
}

bool FUnrealGitSourceControlState::CanAdd() const
{
	return FileState == EGitFileState::Untracked;
}

bool FUnrealGitSourceControlState::CanDelete() const
{
	return bIsTracked;
}

bool FUnrealGitSourceControlState::IsUnknown() const
{
	return FileState == EGitFileState::Unknown;
}

bool FUnrealGitSourceControlState::IsConflicted() const
{
	return bIsConflicted || FileState == EGitFileState::Conflicted;
}

bool FUnrealGitSourceControlState::IsReadOnly() const
{
	return IFileManager::Get().IsReadOnly(*AbsoluteFilename);
}

bool FUnrealGitSourceControlState::CanRevert() const
{
	return IsModified() || IsAdded() || IsDeleted() || IsConflicted();
}

bool FUnrealGitSourceControlState::CanLock() const
{
	return LockState == EGitLockState::NotLocked;
}

bool FUnrealGitSourceControlState::CanUnlock() const
{
	return LockState == EGitLockState::LockedByMe;
}

bool FUnrealGitSourceControlState::IsLocked() const
{
	return LockState == EGitLockState::LockedByMe || LockState == EGitLockState::LockedByOther;
}

bool FUnrealGitSourceControlState::IsLockedOther(FString* Who) const
{
	if (LockState == EGitLockState::LockedByOther)
	{
		if (Who)
		{
			*Who = LockOwner;
		}
		return true;
	}
	return false;
}

bool FUnrealGitSourceControlState::IsLockedLocal() const
{
	return LockState == EGitLockState::LockedByMe;
}

