// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Parses `git --version` and `git lfs version` output.
 *
 * This parser is deterministic and side-effect free.
 */
class FGitVersionParser final
{
public:
	/**
	 * Parses `git --version` stdout (e.g. "git version 2.44.0").
	 */
	static bool ParseGitVersion(const FString& StdOutText, FString& OutVersionText);

	/**
	 * Parses `git lfs version` stdout (e.g. "git-lfs/3.4.1 (GitHub; ...)" or "git-lfs/3.4.1").
	 */
	static bool ParseGitLfsVersion(const FString& StdOutText, FString& OutVersionText);
};

