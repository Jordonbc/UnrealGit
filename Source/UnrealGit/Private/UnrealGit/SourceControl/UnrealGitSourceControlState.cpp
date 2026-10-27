// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

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
	ResolveInfo = InStatus.ResolveInfo;
}

void FUnrealGitSourceControlState::SetLockState(EGitLockState InLockState, FString InLockOwner)
{
	LockState = InLockState;
	LockOwner = MoveTemp(InLockOwner);
}

void FUnrealGitSourceControlState::SetCheckedOut(bool bInCheckedOut)
{
	bIsCheckedOutByMe = bInCheckedOut;
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

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlState::FindHistoryRevision(int32 RevisionNumber) const
{
	if (RevisionNumber == INVALID_REVISION)
	{
		return TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
	}

	for (const TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe>& Rev : History)
	{
		if (Rev->GetRevisionNumber() == RevisionNumber)
		{
			return Rev;
		}
	}
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

ISourceControlState::FResolveInfo FUnrealGitSourceControlState::GetResolveInfo() const
{
	FResolveInfo Info;
	if (ResolveInfo.IsSet())
	{
		Info.RemoteFile = ResolveInfo->RemoteFile;
		Info.BaseFile = ResolveInfo->BaseFile;
		Info.RemoteRevision = ResolveInfo->RemoteRevision;
		Info.BaseRevision = ResolveInfo->BaseRevision;
	}
	return Info;
}

#if SOURCE_CONTROL_WITH_SLATE
FSlateIcon FUnrealGitSourceControlState::GetIcon() const
{
	const FName StyleSet = FAppStyle::GetAppStyleSetName();

	if (bIsConflicted || FileState == EGitFileState::Conflicted)
	{
		return FSlateIcon(StyleSet, "Plastic.Conflicted");
	}

	if (LockState == EGitLockState::LockedByOther)
	{
		return FSlateIcon(StyleSet, "Perforce.CheckedOutByOtherUser");
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

	if (LockState == EGitLockState::LockedByMe)
	{
		return FSlateIcon(StyleSet, "Perforce.CheckedOut");
	}

	if (FileState == EGitFileState::Unchanged)
	{
		return FSlateIcon();
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
	FText StateText;
	switch (FileState)
	{
	case EGitFileState::Added:
		StateText = FText::FromString(TEXT("Added"));
		break;
	case EGitFileState::Modified:
		StateText = FText::FromString(TEXT("Modified"));
		break;
	case EGitFileState::Deleted:
		StateText = FText::FromString(TEXT("Deleted"));
		break;
	case EGitFileState::Renamed:
		StateText = FText::FromString(TEXT("Renamed"));
		break;
	case EGitFileState::Conflicted:
		StateText = FText::FromString(TEXT("Conflicted"));
		break;
	case EGitFileState::Untracked:
		StateText = FText::FromString(TEXT("Untracked"));
		break;
	case EGitFileState::Ignored:
		StateText = FText::FromString(TEXT("Ignored"));
		break;
	case EGitFileState::Unchanged:
		StateText = FText::FromString(TEXT("Unchanged"));
		break;
	default:
		StateText = FText::FromString(TEXT("Unknown"));
		break;
	}

	switch (LockState)
	{
	case EGitLockState::LockedByMe:
		return FText::Format(INVTEXT("{0}\nChecked out by you"), StateText);
	case EGitLockState::LockedByOther:
		return LockOwner.IsEmpty()
			? FText::Format(INVTEXT("{0}\nChecked out by another user"), StateText)
			: FText::Format(INVTEXT("{0}\nChecked out by {1}"), StateText, FText::FromString(LockOwner));
	default:
		break;
	}

	if (bIsCheckedOutByMe && FileState == EGitFileState::Unchanged)
	{
		return FText::FromString(TEXT("Checked Out"));
	}

	return StateText;
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
	return bIsTracked && !IsCheckedOut() && LockState != EGitLockState::LockedByOther;
}

bool FUnrealGitSourceControlState::IsCheckedOut() const
{
	// Checked out means the user accepted a checkout for this file in this session,
	// holds an LFS lock on it, or the file has local changes. Clean tracked files are
	// NOT checked out, so the editor still prompts for checkout when they are edited.
	return bIsCheckedOutByMe || bIsStaged || bIsUnstaged || LockState == EGitLockState::LockedByMe;
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

TOptional<FText> FUnrealGitSourceControlState::GetStatusText() const
{
	if (LockState == EGitLockState::LockedByOther)
	{
		return LockOwner.IsEmpty()
			? FText::FromString(TEXT("Checked out by another user"))
			: FText::Format(INVTEXT("Checked out by {0}"), FText::FromString(LockOwner));
	}

	if (IsConflicted())
	{
		return FText::FromString(TEXT("Conflicted"));
	}
	if (IsAdded())
	{
		return FText::FromString(TEXT("Added"));
	}
	if (IsDeleted())
	{
		return FText::FromString(TEXT("Deleted"));
	}
	if (bIsCheckedOutByMe || LockState == EGitLockState::LockedByMe)
	{
		return FText::FromString(TEXT("Checked Out"));
	}
	if (IsModified())
	{
		return FText::FromString(TEXT("Modified"));
	}
	if (FileState == EGitFileState::Renamed)
	{
		return FText::FromString(TEXT("Renamed"));
	}
	if (FileState == EGitFileState::Untracked)
	{
		return FText::FromString(TEXT("Untracked"));
	}

	return TOptional<FText>();
}
