// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"

/**
 * One side of a widget diff (either an older revision or the working copy).
 */
struct FUnrealGitWidgetDiffSide
{
	/** Human-readable label, e.g. "HEAD (a1b2c3d)". */
	FText Label;
	/** Commit SHA or empty for the working copy. */
	FString Revision;
	/** When this revision was committed. */
	FDateTime Date = FDateTime();
	/** Rendered pixels of the widget, BGRA8. */
	FImage Image;
	/** Rendered texture created from Image. */
	TObjectPtr<class UTexture2D> Texture = nullptr;
	/** Error message if this side could not be rendered. */
	FText Error;
};

/** Change state of a widget node between the two revisions. */
enum class EUnrealGitWidgetChange : uint8
{
	Unchanged,
	Added,
	Removed,
	Modified,
};

/** One row in the read-only widget hierarchy tree. */
struct FUnrealGitWidgetNode
{
	FString Name;
	FString TypeName;
	int32 Depth = 0;
	int32 ParentIndex = INDEX_NONE;
	EUnrealGitWidgetChange Change = EUnrealGitWidgetChange::Unchanged;
	TArray<FString> ChangedProperties;
	/** Index into the flattened pre-order array of the left tree, or INDEX_NONE if added. */
	int32 LeftIndex = INDEX_NONE;
	/** Index into the flattened pre-order array of the right tree, or INDEX_NONE if removed. */
	int32 RightIndex = INDEX_NONE;
};

/**
 * Result of comparing two widget blueprint revisions.
 */
struct FUnrealGitWidgetDiffResult
{
	bool bSuccess = false;
	FText Error;

	FUnrealGitWidgetDiffSide Left;
	FUnrealGitWidgetDiffSide Right;

	/** Number of pixels that differ above the tolerance. */
	int32 DiffPixelCount = 0;
	/** Ratio of differing pixels to the total canvas area. */
	float DiffRatio = 0.0f;
	/** Bounding rectangle of all differing pixels (in canvas space). */
	FIntRect DiffBounds = FIntRect(0, 0, 0, 0);
	/** Common canvas size used for both sides. */
	FIntPoint CanvasSize = FIntPoint(0, 0);

	/** Difference image with changed pixels highlighted. */
	FImage DiffImage;
	/** Texture created from DiffImage. */
	TObjectPtr<class UTexture2D> DiffTexture = nullptr;

	/** Flattened pre-order widget hierarchy with change markers. */
	TArray<FUnrealGitWidgetNode> Hierarchy;
};
