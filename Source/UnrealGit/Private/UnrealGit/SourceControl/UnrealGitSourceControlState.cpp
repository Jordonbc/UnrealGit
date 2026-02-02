// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlState.h"

#if SOURCE_CONTROL_WITH_SLATE
#include "Styling/AppStyle.h"
#endif // SOURCE_CONTROL_WITH_SLATE

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

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlState::GetCurrentRevision() const
{
	return History.Num() > 0 ? History[0] : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

#if SOURCE_CONTROL_WITH_SLATE
FSlateIcon FUnrealGitSourceControlState::GetIcon() const
{
	const FName StyleSet = FAppStyle::GetAppStyleSetName();

	if (bIsConflicted || FileState == EGitFileState::Conflicted)
	{
		return FSlateIcon(StyleSet, "Plastic.Conflicted");
	}

	if (FileState == EGitFileState::Added)
	{
		return FSlateIcon(StyleSet, "Perforce.OpenForAdd");
	}

	if (FileState == EGitFileState::Deleted)
	{
		return FSlateIcon(StyleSet, "Perforce.MarkedForDelete");
	}

	if (FileState == EGitFileState::Modified || bIsStaged || bIsUnstaged)
	{
		return FSlateIcon(StyleSet, "Perforce.CheckedOut");
	}

	if (FileState == EGitFileState::Untracked)
	{
		return FSlateIcon(StyleSet, "Perforce.NotInDepot");
	}

	if (FileState == EGitFileState::Ignored)
	{
		return FSlateIcon(StyleSet, "Plastic.Ignored");
	}

	return FSlateIcon(StyleSet, "SourceControl.StatusIcon.Unknown");
}
#endif // SOURCE_CONTROL_WITH_SLATE

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
	return bIsTracked && LockState != EGitLockState::LockedByOther;
}

bool FUnrealGitSourceControlState::IsCheckedOut() const
{
	return bIsStaged || bIsUnstaged;
}

bool FUnrealGitSourceControlState::IsCheckedOutOther(FString* Who) const
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

bool FUnrealGitSourceControlState::IsCheckedOutInOtherBranch(const FString& /*CurrentBranch*/) const
{
	return false;
}

bool FUnrealGitSourceControlState::IsModifiedInOtherBranch(const FString& /*CurrentBranch*/) const
{
	return false;
}

bool FUnrealGitSourceControlState::IsCheckedOutOrModifiedInOtherBranch(const FString& /*CurrentBranch*/) const
{
	return false;
}

TArray<FString> FUnrealGitSourceControlState::GetCheckedOutBranches() const
{
	return {};
}

FString FUnrealGitSourceControlState::GetOtherUserBranchCheckedOuts() const
{
	return FString();
}

bool FUnrealGitSourceControlState::GetOtherBranchHeadModification(FString& /*HeadBranchOut*/, FString& /*ActionOut*/, int32& /*HeadChangeListOut*/) const
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
	return !IsCheckedOutOther();
}

bool FUnrealGitSourceControlState::CanDelete() const
{
	return bIsTracked;
}

bool FUnrealGitSourceControlState::IsUnknown() const
{
	return FileState == EGitFileState::Unknown;
}

bool FUnrealGitSourceControlState::IsModified() const
{
	return FileState == EGitFileState::Modified || bIsStaged || bIsUnstaged;
}

bool FUnrealGitSourceControlState::CanAdd() const
{
	return FileState == EGitFileState::Untracked;
}

bool FUnrealGitSourceControlState::IsConflicted() const
{
	return bIsConflicted || FileState == EGitFileState::Conflicted;
}

bool FUnrealGitSourceControlState::CanRevert() const
{
	return IsModified() || IsAdded() || IsDeleted() || IsConflicted();
}
