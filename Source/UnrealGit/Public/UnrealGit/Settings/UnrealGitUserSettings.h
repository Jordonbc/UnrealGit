// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealGitUserSettings.generated.h"

/**
 * User-level configuration for the UnrealGit Source Control provider.
 *
 * Responsibility:
 * - Stores per-user preferences like executable overrides and auto-lock behavior.
 *
 * Threading:
 * - This is a UObject config container. Runtime usage must copy values into validated settings once.
 */
UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="UnrealGit (User)"))
class UNREALGIT_API UUnrealGitUserSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Optional override for the git executable. When empty, UnrealGit uses PATH lookup ("git"). */
	UPROPERTY(EditAnywhere, Config, Category="Executables")
	FFilePath GitExecutableOverride;

	/** Automatically locks Git LFS files when checking out (making writable). */
	UPROPERTY(EditAnywhere, Config, Category="Locks")
	bool bAutoLockOnCheckout = false;

	virtual FName GetCategoryName() const override;
	virtual FName GetContainerName() const override;
};

