// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FGitRevParseParser
 * Parses simple git plumbing outputs that are line oriented.
 *
 * This parser is deterministic and side-effect free.
 */
class UNREALGIT_API FGitRevParseParser final
{
public:
	/**
	 * Parses `git rev-parse --show-toplevel` stdout.
	 *
	 * @param StdOutText Stdout as text (expected UTF-8 converted to FString).
	 * @param OutRepoRoot Absolute repository root path.
	 * @return True on success, false otherwise.
	 */
	static bool ParseShowToplevel(const FString& StdOutText, FString& OutRepoRoot);
};

