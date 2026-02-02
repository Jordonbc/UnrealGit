// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealGitProjectSettings.generated.h"

/**
 * Project-level configuration for the UnrealGit Source Control provider.
 *
 * Responsibility:
 * - Stores project-specific behavior such as pull/submit strategy and repo root override.
 *
 * Threading:
 * - This is a UObject config container. Runtime usage must copy values into validated settings once.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="UnrealGit"))
class UNREALGIT_API UUnrealGitProjectSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Optional explicit repository root. When empty, UnrealGit discovers the repo root from the project directory. */
	UPROPERTY(EditAnywhere, Config, Category="Repository")
	FDirectoryPath RepositoryRootOverride;

	/** Uses `git pull --rebase` instead of `git pull`. */
	UPROPERTY(EditAnywhere, Config, Category="Sync")
	bool bPullRebase = false;

	/** Pushes after a successful submit (commit). */
	UPROPERTY(EditAnywhere, Config, Category="Submit")
	bool bAutoPushAfterSubmit = false;

	/** Enables Git LFS lock integration (locks/lock state). */
	UPROPERTY(EditAnywhere, Config, Category="Locks")
	bool bEnableLfsLocks = true;

	/** Refresh interval (seconds) for querying `git lfs locks`. */
	UPROPERTY(EditAnywhere, Config, Category="Locks", meta=(ClampMin="1", UIMin="1"))
	int32 LfsLocksRefreshIntervalSeconds = 15;

	virtual FName GetCategoryName() const override;
	virtual FName GetContainerName() const override;
};

