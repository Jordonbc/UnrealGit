// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "UnrealGit/Git/GitModels.h"

class FGitRevisionMaterializer;
class IGitProcessRunner;

/**
 * Source control revision implementation for Git commits.
 *
 * Responsibility:
 * - Exposes revision metadata for Unreal history and diff views.
 * - Materializes file contents via `git show` to a temp file when requested.
 *
 * Threading:
 * - Get supports EConcurrency::Asynchronous and EConcurrency::Synchronous.
 * - Synchronous materialization must not be executed on the game thread.
 */
class FUnrealGitSourceControlRevision final : public ISourceControlRevision
{
public:
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

private:
	struct FMaterializationTask final
	{
		TFuture<bool> Future;
		FString MaterializedFilename;
		FString Error;
	};

	bool TryCompleteMaterialization(FString& InOutFilename) const;
	bool StartMaterializationAsync() const;
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

	mutable FString CachedAction;
	mutable FString CachedClientSpec;
};
