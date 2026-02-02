// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IGitProcessRunner;

/**
 * Materializes Git revisions to temporary files for Unreal's diff and asset load pipeline.
 *
 * Responsibility:
 * - Executes `git show <rev>:<path>` and writes the result as a binary-safe temp file.
 * - Tracks the created files and provides explicit cleanup.
 *
 * Threading:
 * - Materialize may be called from background threads.
 * - Cleanup must be called when no revision objects will use the files anymore.
 */
class FGitRevisionMaterializer final : public TSharedFromThis<FGitRevisionMaterializer, ESPMode::ThreadSafe>
{
public:
	explicit FGitRevisionMaterializer(FString InSessionRootDirectory);
	~FGitRevisionMaterializer();

	/**
	 * Writes a temporary file for a specific revision and relative path.
	 *
	 * @param ProcessRunner Process runner used to execute Git.
	 * @param RepoRoot Repository root used as working directory.
	 * @param CommitId Commit SHA or ref.
	 * @param RelativePath Repository-relative path using forward slashes.
	 * @param OutAbsoluteFilename Absolute temp filename on disk.
	 * @param OutError Error message on failure.
	 */
	bool Materialize(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot,
		const FString& CommitId,
		const FString& RelativePath,
		FString& OutAbsoluteFilename,
		FString& OutError);

	/**
	 * Deletes all materialized files for this session.
	 */
	void Cleanup();

private:
	struct FKey final
	{
		FString CommitId;
		FString RelativePath;
	};

	friend uint32 GetTypeHash(const FKey& Key)
	{
		return HashCombine(GetTypeHash(Key.CommitId), GetTypeHash(Key.RelativePath));
	}

	friend bool operator==(const FKey& A, const FKey& B)
	{
		return A.CommitId == B.CommitId && A.RelativePath == B.RelativePath;
	}

private:
	FString SessionRootDirectory;

	mutable FCriticalSection Lock;
	TMap<FKey, FString> MaterializedFiles;
};

