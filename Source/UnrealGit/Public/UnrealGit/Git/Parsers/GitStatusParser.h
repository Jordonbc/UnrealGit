// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * @class FGitStatusParser
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
	/**
	 * Parses `git status --porcelain=v2 -z --branch` output into a status snapshot.
	 *
	 * @param StdOut Raw bytes from stdout (UTF-8, NUL-delimited records).
	 * @param OutSnapshot Parsed status snapshot on success.
	 * @param OutError Error message on parse failure.
	 * @return True on success, false otherwise.
	 */
	static bool ParsePorcelainV2Z(const TArray<uint8>& StdOut, FGitStatusSnapshot& OutSnapshot, FString& OutError);

	/**
	 * Parses `git ls-files --unmerged -z` output into per-file resolve info.
	 *
	 * @param StdOut Raw bytes from stdout (UTF-8, NUL-delimited records).
	 * @param OutResolveInfo Map from relative path to resolve info on success.
	 * @param OutError Error message on parse failure.
	 * @return True on success, false otherwise.
	 */
	static bool ParseUnmergedZ(const TArray<uint8>& StdOut, TMap<FString, FGitResolveInfo>& OutResolveInfo, FString& OutError);
};
