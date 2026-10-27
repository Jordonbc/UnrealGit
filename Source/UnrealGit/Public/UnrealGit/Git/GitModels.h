// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @enum EGitFileState
 * High-level Git file state used by UnrealGit domain model.
 *
 * This enum is independent of editor/source control UI state and is intended
 * to be mapped to EWorkspaceState in the provider layer.
 */
enum class EGitFileState : uint8
{
	Unknown,
	Unchanged,
	Added,
	Modified,
	Deleted,
	Renamed,
	Conflicted,
	Untracked,
	Ignored,
};

/**
 * @enum EGitLockState
 * Lock state derived from Git LFS locks.
 */
enum class EGitLockState : uint8
{
	Unknown,
	NotLocked,
	LockedByMe,
	LockedByOther,
};

/**
 * @struct FGitResolveInfo
 * Merge-conflict details for a single file, derived from `git ls-files --unmerged`.
 *
 * Mirrors ISourceControlState::FResolveInfo so the editor can offer merge tooling.
 */
struct FGitResolveInfo final
{
	FString RemoteFile;
	FString BaseFile;
	FString RemoteRevision;
	FString BaseRevision;

	/**
	 * Reports whether the resolve info contains at least one side of a merge conflict.
	 *
	 * @return True when either BaseFile or RemoteFile is non-empty.
	 */
	bool IsValid() const
	{
		return !BaseFile.IsEmpty() || !RemoteFile.IsEmpty();
	}
};

/**
 * @struct FGitFileStatus
 * Parsed file status from `git status --porcelain=v2 -z`.
 */
struct FGitFileStatus final
{
	FString RelativePath;
	FString OriginalPath;

	EGitFileState State = EGitFileState::Unknown;
	bool bIsStaged = false;
	bool bIsUnstaged = false;

	bool bIsTracked = false;
	bool bIsConflicted = false;

	/** Set only for conflicted files when unmerged index stages are available. */
	TOptional<FGitResolveInfo> ResolveInfo;
};

/**
 * @struct FGitBranchStatus
 * Parsed branch status from porcelain v2 `--branch`.
 */
struct FGitBranchStatus final
{
	FString Head;
	TOptional<int32> Ahead;
	TOptional<int32> Behind;
};

/**
 * @struct FGitStatusSnapshot
 * Parsed Git status snapshot.
 */
struct FGitStatusSnapshot final
{
	FGitBranchStatus Branch;
	TArray<FGitFileStatus> Files;
};

/**
 * @struct FGitLfsLock
 * Parsed Git LFS lock information.
 */
struct FGitLfsLock final
{
	FString Path;
	FString Id;
	FString OwnerName;
	FString OwnerEmail;
	FDateTime LockedAtUtc = FDateTime(0);
};

/**
 * @struct FGitRevisionInfo
 * Parsed Git revision metadata for history and diff selection.
 */
struct FGitRevisionInfo final
{
	FString CommitId;
	FString AuthorName;
	FString AuthorEmail;
	FDateTime AuthorDateUtc = FDateTime(0);
	FString Subject;
	FString Body;

	/** Human-readable action for this revision ("edit", "add", "delete", "branch", ...). */
	FString Action;

	/** 1-based rank within the file history, oldest revision = 1. */
	int32 RevisionNumber = 0;

	/** Size in bytes of the file at this revision, or 0 when unknown. */
	int32 FileSize = 0;
};

/**
 * @struct FGitFileRevision
 * File-specific revision including the path as it existed at the revision.
 */
struct FGitFileRevision final
{
	FGitRevisionInfo Revision;
	FString RepoRelativePathAtRevision;
};
