// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

/**
 * System Git CLI runner using Unreal's process APIs.
 *
 * Threading:
 * - Instances are thread-safe.
 * - Run must be called off the game thread.
 */
class FSystemGitProcessRunner final : public IGitProcessRunner
{
public:
	explicit FSystemGitProcessRunner(FString InGitExecutablePath);

	virtual FGitProcessResult Run(const FGitProcessRequest& Request) override;

private:
	FString GitExecutablePath;

	static FString BuildCommandLine(const TArray<FString>& Arguments);
};

