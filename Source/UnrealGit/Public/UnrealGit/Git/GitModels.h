// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
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
};

/**
 * Parsed branch status from porcelain v2 `--branch`.
 */
struct FGitBranchStatus final
{
	FString Head;
	TOptional<int32> Ahead;
	TOptional<int32> Behind;
};

/**
 * Parsed Git status snapshot.
 */
struct FGitStatusSnapshot final
{
	FGitBranchStatus Branch;
	TArray<FGitFileStatus> Files;
};

/**
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
};

/**
 * File-specific revision including the path as it existed at the revision.
 */
struct FGitFileRevision final
{
	FGitRevisionInfo Revision;
	FString RepoRelativePathAtRevision;
};
