// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "Widgets/SLeafWidget.h"

/** Shared zoom/pan state for synchronised diff canvases. */
struct FUnrealGitWidgetDiffCanvasState
{
	float Zoom = 1.0f;
	FVector2D Pan = FVector2D::ZeroVector;
};

/**
 * Read-only zoom/pan canvas that draws a base image plus an optional
 * highlight overlay (changed pixels) on top. When several canvases share a
 * state object their zoom/pan stay in sync.
 */
class SUnrealGitWidgetDiffCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealGitWidgetDiffCanvas) {}
		SLATE_ATTRIBUTE(const FSlateBrush*, BaseBrush)
		SLATE_ATTRIBUTE(const FSlateBrush*, OverlayBrush)
		SLATE_ATTRIBUTE(bool, ShowOverlay)
		SLATE_ARGUMENT(TSharedPtr<FUnrealGitWidgetDiffCanvasState>, TransformState)
		SLATE_ARGUMENT(TArray<FUnrealGitWidgetNode>*, HighlightNodes)
		SLATE_ATTRIBUTE(FString, SelectedNode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void ResetView();
	float GetZoom() const;

private:
	FVector2D GetImageSize() const;
	FUnrealGitWidgetDiffCanvasState& GetState() const;

	TAttribute<const FSlateBrush*> BaseBrush;
	TAttribute<const FSlateBrush*> OverlayBrush;
	TAttribute<bool> bShowOverlay;

	TSharedPtr<FUnrealGitWidgetDiffCanvasState> TransformState;
	mutable TSharedPtr<FUnrealGitWidgetDiffCanvasState> LocalState;
	TArray<FUnrealGitWidgetNode>* HighlightNodes = nullptr;
	TAttribute<FString> SelectedNode;

	bool bPanning = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
};
