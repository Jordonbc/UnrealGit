// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

class UWidget;

/**
 * @class FUnrealGitWidgetDiffPropertyComparer
 * Computes value-level property differences between two widget template objects.
 *
 * Comparison walks reflected properties (including widget slot properties) and
 * recurses through structs and containers, emitting one entry per differing leaf.
 * Values are exported through the reflection system so object references appear
 * as stable asset paths rather than memory addresses.
 */
class FUnrealGitWidgetDiffPropertyComparer
{
public:
	/**
	 * Compares every comparable reflected property of two widget templates.
	 * @param Left Historical widget template.
	 * @param Right Current widget template.
	 * @param OutDiffs Receives one entry per differing property leaf, in declaration order.
	 */
	static void CompareWidgets(const UWidget* Left, const UWidget* Right, TArray<FUnrealGitPropertyDiff>& OutDiffs);
};
