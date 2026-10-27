// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprintEditorUtils.h"

/**
 * @class FUnrealGitWidgetDiffLauncher
 * Launches a widget diff session for an asset between two revisions.
 */
class FUnrealGitWidgetDiffLauncher
{
public:
	/**
	 * Launches the diff for the given asset.
	 * @param AssetFileName Asset to diff.
	 * @param LeftRevision Older revision.
	 * @param RightRevision Newer revision.
	 * @param Mode Preview size mode.
	 */
	static void Launch(const FString& AssetFileName, const FString& LeftRevision, const FString& RightRevision, EThumbnailPreviewSizeMode Mode = EThumbnailPreviewSizeMode::MatchDesignerMode);
};