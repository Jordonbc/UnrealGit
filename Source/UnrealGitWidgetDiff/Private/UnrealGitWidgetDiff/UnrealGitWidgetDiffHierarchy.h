// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

class UWidgetBlueprint;

/**
 * Builds a flattened, read-only hierarchy of a widget tree and compares two
 * revisions to mark added/removed/modified widgets.
 */
class FUnrealGitWidgetDiffHierarchy
{
public:
	static TArray<FUnrealGitWidgetNode> Build(UWidgetBlueprint* Left, UWidgetBlueprint* Right);
};
