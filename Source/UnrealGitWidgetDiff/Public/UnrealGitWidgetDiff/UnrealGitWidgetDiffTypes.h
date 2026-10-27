// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"

/**
 * @struct FUnrealGitWidgetDiffSide
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

/**
 * @enum EUnrealGitWidgetChange
 * Change state of a widget node between the two revisions.
 */
enum class EUnrealGitWidgetChange : uint8
{
	Unchanged,
	Added,
	Removed,
	Modified,
};

/**
 * @struct FUnrealGitPropertyDiff
 * One differing property leaf between the two widget revisions.
 */
struct FUnrealGitPropertyDiff
{
	/** Dotted path relative to the widget, e.g. "Slot.Padding.Left". */
	FString Path;
	/** Grouping label: "Slot", the property's Category metadata, or the owning class name. */
	FString Category;
	/** Removed/Added when the property exists on one side only; Modified when its value differs. */
	EUnrealGitWidgetChange Change = EUnrealGitWidgetChange::Modified;
	/** Value exported from the historical (left) revision; empty for added properties. */
	FString Before;
	/** Value exported from the current (right) revision; empty for removed properties. */
	FString After;
};

/**
 * @struct FUnrealGitWidgetNode
 * One row in the read-only widget hierarchy tree.
 */
struct FUnrealGitWidgetNode
{
	FString Name;
	FString ParentName;
	FString Path;
	FString TypeName;
	int32 Depth = 0;
	bool bIsChildOfChange = false;
	FString ChangedAncestor;
	int32 ParentIndex = INDEX_NONE;
	EUnrealGitWidgetChange Change = EUnrealGitWidgetChange::Unchanged;
	TArray<FString> ChangedProperties;
	/** Value-level property differences, empty for added/removed or unchanged widgets. */
	TArray<FUnrealGitPropertyDiff> PropertyDiffs;
	/** Index into the flattened pre-order array of the left tree, or INDEX_NONE if added. */
	int32 LeftIndex = INDEX_NONE;
	/** Index into the flattened pre-order array of the right tree, or INDEX_NONE if removed. */
	int32 RightIndex = INDEX_NONE;

	/** Widget rectangles in each rendered canvas. */
	FVector2D LeftPosition = FVector2D::ZeroVector;
	FVector2D LeftSize = FVector2D::ZeroVector;
	bool bHasLeftGeometry = false;

	FVector2D RightPosition = FVector2D::ZeroVector;
	FVector2D RightSize = FVector2D::ZeroVector;
	bool bHasRightGeometry = false;
};

/**
 * @struct FUnrealGitWidgetDiffResult
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

	/** Transparent image with only the changed pixels highlighted. */
	FImage HighlightImage;
	TObjectPtr<class UTexture2D> HighlightTexture = nullptr;

	/** Flattened pre-order widget hierarchy with change markers. */
	TArray<FUnrealGitWidgetNode> Hierarchy;
};
