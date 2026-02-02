// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * Parses `git status --porcelain=v2 -z --branch` output.
 *
 * This parser is deterministic and side-effect free.
 *
 * Input:
 * - Raw bytes from stdout (UTF-8, NUL-delimited records).
 *
 * Output:
 * - A status snapshot describing branch state and per-file state.
 */
class UNREALGIT_API FGitStatusParser final
{
public:
	static bool ParsePorcelainV2Z(const TArray<uint8>& StdOut, FGitStatusSnapshot& OutSnapshot, FString& OutError);
};
