// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

class UWidgetBlueprint;

/**
 * @class FUnrealGitWidgetDiffHierarchy
 * Builds a flattened, read-only hierarchy of a widget tree and compares two
 * revisions to mark added/removed/modified widgets.
 */
class FUnrealGitWidgetDiffHierarchy
{
public:
	/**
	 * Builds the flattened hierarchy comparing both revisions.
	 * @param Left Older widget blueprint.
	 * @param Right Newer widget blueprint.
	 * @return Flattened node list with change markers.
	 */
	static TArray<FUnrealGitWidgetNode> Build(UWidgetBlueprint* Left, UWidgetBlueprint* Right);
};
