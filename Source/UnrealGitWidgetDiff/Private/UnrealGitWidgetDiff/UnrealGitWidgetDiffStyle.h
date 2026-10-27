// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

/** Tree/summary palette used by the view's hierarchy rows. */
FLinearColor GetChangeColor(EUnrealGitWidgetChange Change);

/** Summary-badge palette used by the view's changed-properties rows. */
FLinearColor GetChangeBadgeColor(EUnrealGitWidgetChange Change);

/** Outline/badge palette used by the canvas overlays. */
FLinearColor GetChangeOutlineColor(EUnrealGitWidgetChange Change);

/** `+`/`-`/`~` marker glyph for a change. */
FString GetChangeGlyph(EUnrealGitWidgetChange Change);