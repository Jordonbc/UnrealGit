// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * @class FGitFileHistoryParser
 * Parses `git log --follow --name-status -z` output with RS/US delimiters.
 *
 * This parser is deterministic and side-effect free.
 */
class UNREALGIT_API FGitFileHistoryParser final
{
public:
	/**
	 * Parses a file history stream and reconstructs the repo-relative path at each revision.
	 *
	 * Input format expectations:
	 * - Commit records are prefixed by record separator (0x1E).
	 * - Metadata fields are separated by unit separator (0x1F).
	 * - Name-status section uses NUL separators (-z).
	 *
	 * @param StdOut Raw bytes from stdout (UTF-8 and NUL safe).
	 * @param InitialRepoRelativePath The newest repo-relative path (from the working tree).
	 * @param OutHistory Parsed revisions in log order (newest to oldest).
	 * @param OutError Error message on parse failure.
	 */
	static bool Parse(const TArray<uint8>& StdOut, const FString& InitialRepoRelativePath, TArray<FGitFileRevision>& OutHistory, FString& OutError);

	/**
	 * Parses the stdout of `git cat-file --batch-check` and fills FileSize for each
	 * revision in OutRevisions, in order. Missing/ambiguous/empty responses leave 0.
	 *
	 * @param StdOut Raw stdout text from `git cat-file --batch-check`.
	 * @param InOutRevisions Revisions whose FileSize fields are populated in place.
	 */
	static void ParseCatFileBatchCheckOutput(const FString& StdOut, TArray<FGitFileRevision>& InOutRevisions);

	/**
	 * Byte-safe overload of ParseCatFileBatchCheckOutput.
	 *
	 * @param StdOut Raw stdout bytes from `git cat-file --batch-check`.
	 * @param InOutRevisions Revisions whose FileSize fields are populated in place.
	 */
	static void ParseCatFileBatchCheckOutput(const TArray<uint8>& StdOut, TArray<FGitFileRevision>& InOutRevisions);

	/**
	 * Alias for ParseCatFileBatchCheckOutput (unit-test friendly).
	 *
	 * @param StdOut Raw stdout text from `git cat-file --batch-check`.
	 * @param InOutRevisions Revisions whose FileSize fields are populated in place.
	 */
	static void ParseCatFileBatchCheck(const FString& StdOut, TArray<FGitFileRevision>& InOutRevisions);

	/**
	 * Alias that fills a sizes array (one entry per response line).
	 *
	 * @param StdOut Raw stdout text from `git cat-file --batch-check`.
	 * @param OutSizes Parsed file sizes, one entry per response line.
	 */
	static void ParseCatFileBatchCheck(const FString& StdOut, TArray<int32>& OutSizes);

	/**
	 * Alias returning the parsed sizes.
	 *
	 * @param StdOut Raw stdout text from `git cat-file --batch-check`.
	 * @return Parsed file sizes, one entry per response line.
	 */
	static TArray<int32> ParseCatFileBatchCheckSizes(const FString& StdOut);
};
