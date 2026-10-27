// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @struct FGitEnvironmentInfo
 * Toolchain information for the system Git and Git LFS integration.
 *
 * Responsibility:
 * - Captures availability and parsed version info.
 * - Used to surface clear status text in the provider layer.
 */
struct FGitEnvironmentInfo final
{
	bool bGitAvailable = false;
	FString GitVersion;

	bool bGitLfsAvailable = false;
	FString GitLfsVersion;

	FString UserName;
	FString UserEmail;

	FString BranchName;
	FString RemoteUrl;
};

