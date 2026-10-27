// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SourceControlWindows.h"

class UToolMenu;

/**
 * @class FUnrealGitHistoryDiffExtension
 * Adds diff actions to the source control history widget for UnrealGit revisions.
 */
class FUnrealGitHistoryDiffExtension
{
public:
	/** Registers the history diff extension. */
	static void Register();

	/** Unregisters the history diff extension. */
	static void Unregister();

private:
	/** Populates the diff menu with available actions.
	 * @param InMenu Menu to populate. */
	static void PopulateDiffMenu(UToolMenu* InMenu);

	/** Diffs the selected revision against the workspace version.
	 * @param Selected Selected history items. */
	static void OnDiffAgainstWorkspace(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);

	/** Diffs the two selected revisions.
	 * @param Selected Selected history items. */
	static void OnDiffSelected(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);

	/** Diffs the selected revision against its previous revision.
	 * @param Selected Selected history items. */
	static void OnDiffAgainstPrevious(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);
};
