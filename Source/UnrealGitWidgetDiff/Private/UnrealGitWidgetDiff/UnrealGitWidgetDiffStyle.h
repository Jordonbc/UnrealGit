// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

/**
 * Tree/summary palette used by the view's hierarchy rows.
 * @param Change Change state of the widget.
 * @return Colour for that change in hierarchy/summary contexts.
 */
FLinearColor GetChangeColor(EUnrealGitWidgetChange Change);

/**
 * Summary-badge palette used by the view's changed-properties rows.
 * @param Change Change state of the widget.
 * @return Colour for the badge in the properties summary.
 */
FLinearColor GetChangeBadgeColor(EUnrealGitWidgetChange Change);

/**
 * Outline/badge palette used by the canvas overlays.
 * @param Change Change state of the widget.
 * @return Colour for outlines and badges drawn on canvases.
 */
FLinearColor GetChangeOutlineColor(EUnrealGitWidgetChange Change);

/**
 * `+`/`-`/`~` marker glyph for a change.
 * @param Change Change state of the widget.
 * @return Single-character glyph string.
 */
FString GetChangeGlyph(EUnrealGitWidgetChange Change);