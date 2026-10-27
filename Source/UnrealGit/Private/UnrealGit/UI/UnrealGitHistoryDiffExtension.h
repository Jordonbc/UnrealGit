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
	/** Populates the diff menu with available actions. */
	static void PopulateDiffMenu(UToolMenu* InMenu);

	/** Diffs the selected revision against the workspace version. */
	static void OnDiffAgainstWorkspace(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);

	/** Diffs the two selected revisions. */
	static void OnDiffSelected(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);

	/** Diffs the selected revision against its previous revision. */
	static void OnDiffAgainstPrevious(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);
};
