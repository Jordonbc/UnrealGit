// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Parses simple git plumbing outputs that are line oriented.
 *
 * This parser is deterministic and side-effect free.
 */
class FGitRevParseParser final
{
public:
	/**
	 * Parses `git rev-parse --show-toplevel` stdout.
	 *
	 * @param StdOutText Stdout as text (expected UTF-8 converted to FString).
	 * @param OutRepoRoot Absolute repository root path.
	 */
	static bool ParseShowToplevel(const FString& StdOutText, FString& OutRepoRoot);
};

