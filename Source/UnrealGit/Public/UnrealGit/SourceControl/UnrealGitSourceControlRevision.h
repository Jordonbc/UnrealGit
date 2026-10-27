// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "UnrealGit/Git/GitModels.h"

class FGitRevisionMaterializer;
class IGitProcessRunner;

/**
 * @class FUnrealGitSourceControlRevision
 * Source control revision implementation for Git commits.
 *
 * Responsibility:
 * - Exposes revision metadata for Unreal history and diff views.
 * - Materializes file contents via `git cat-file --filters <rev>:<path>` (LFS-smudging), with `git show` as a fallback, to a temp file when requested.
 *
 * Threading:
 * - Get supports EConcurrency::Asynchronous and EConcurrency::Synchronous.
 * - Synchronous materialization on the game thread is supported: it offloads the
 *   work to a worker thread and blocks until it completes.
 */
class FUnrealGitSourceControlRevision final : public ISourceControlRevision
{
public:
	/**
	 * Creates a revision bound to a file and its git commit metadata.
	 *
	 * @param InAbsoluteFilename Absolute path of the file in the working tree.
	 * @param InRepoRelativePath Repository-relative path at this revision.
	 * @param InRevisionInfo Parsed commit metadata for this revision.
	 * @param InRepoRoot Absolute repository root.
	 * @param InProcessRunner Process runner used for materialization.
	 * @param InMaterializer Materializer used to fetch file contents.
	 */
	FUnrealGitSourceControlRevision(
		FString InAbsoluteFilename,
		FString InRepoRelativePath,
		FGitRevisionInfo InRevisionInfo,
		FString InRepoRoot,
		TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> InProcessRunner,
		TSharedRef<FGitRevisionMaterializer, ESPMode::ThreadSafe> InMaterializer);

	// ISourceControlRevision
	virtual bool Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const override;
	virtual const FString& GetFilename() const override;
	virtual const FString& GetRevision() const override;
	virtual int32 GetRevisionNumber() const override;
	virtual const FString& GetDescription() const override;
	virtual const FString& GetUserName() const override;
	virtual const FDateTime& GetDate() const override;
	virtual const FString& GetAction() const override;
	virtual int32 GetCheckInIdentifier() const override;
	virtual int32 GetFileSize() const override;
	virtual const FString& GetClientSpec() const override;
	virtual TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> GetBranchSource() const override;
	virtual bool GetAnnotated(FString& InOutFilename) const override;
	virtual bool GetAnnotated(TArray<FAnnotationLine>& OutLines) const override;

	/**
	 * Links this revision to the revision it was branched/copied from (history metadata).
	 *
	 * @param InBranchSource Revision this one was branched/copied from.
	 */
	void SetBranchSource(TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> InBranchSource);

private:
	/**
	 * @struct FMaterializationTask
	 * Tracks an in-flight materialization operation.
	 */
	struct FMaterializationTask final
	{
		TFuture<bool> Future;
		FString MaterializedFilename;
		FString Error;
	};

	/**
	 * Completes an in-flight materialization if it has finished.
	 *
	 * @param InOutFilename Receives the materialized file path on success.
	 * @return True when materialization completed successfully.
	 */
	bool TryCompleteMaterialization(FString& InOutFilename) const;

	/**
	 * Starts asynchronous materialization of this revision's file contents.
	 *
	 * @return True when materialization was started successfully.
	 */
	bool StartMaterializationAsync() const;

	/**
	 * Materializes this revision's file contents synchronously.
	 *
	 * @param InOutFilename Receives the materialized file path on success.
	 * @return True on success.
	 */
	bool MaterializeSync(FString& InOutFilename) const;

private:
	FString AbsoluteFilename;
	FString RepoRelativePath;
	FGitRevisionInfo RevisionInfo;
	FString RepoRoot;

	TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> ProcessRunner;
	TSharedRef<FGitRevisionMaterializer, ESPMode::ThreadSafe> Materializer;

	mutable FCriticalSection Lock;
	mutable TSharedPtr<FMaterializationTask, ESPMode::ThreadSafe> Task;
	mutable TOptional<FString> CachedMaterializedFilename;

	TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> BranchSource;
};
