// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "Widgets/SCompoundWidget.h"

/** Shared zoom/pan state for synchronised diff canvases. */
struct FUnrealGitWidgetDiffCanvasState
{
	float Zoom = 1.0f;
	FVector2D Pan = FVector2D::ZeroVector;
};

/**
 * Read-only UMG preview viewport. Hosts the live Slate widget hierarchy so
 * text and geometry stay sharp at any zoom, and draws the UMG Designer grid,
 * rulers, design boundary and diff overlays in the same coordinate system.
 */
class SUnrealGitWidgetDiffCanvas : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnWidgetPicked, const FString&);

	SLATE_BEGIN_ARGS(SUnrealGitWidgetDiffCanvas)
		: _HighlightNodes(nullptr)
		, _UseLeftGeometry(true)
		, _DesignSize(FVector2D::ZeroVector)
	{}
		SLATE_ATTRIBUTE(const FSlateBrush*, BaseBrush)
		SLATE_ATTRIBUTE(const FSlateBrush*, OverlayBrush)
		SLATE_ATTRIBUTE(bool, ShowOverlay)
		SLATE_ATTRIBUTE(bool, FitToWindow)
		SLATE_ARGUMENT(TSharedPtr<FUnrealGitWidgetDiffCanvasState>, TransformState)
		SLATE_ARGUMENT(TSharedPtr<TArray<FUnrealGitWidgetNode>>, HighlightNodes)
		SLATE_ARGUMENT(bool, UseLeftGeometry)
		SLATE_ATTRIBUTE(FString, SelectedNode)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, PreviewContent)
		SLATE_ARGUMENT(FVector2D, DesignSize)
		SLATE_EVENT(FOnWidgetPicked, OnWidgetPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void ResetView();
	float GetZoom() const;
	void SetPreviewContent(TSharedPtr<SWidget> InContent, FVector2D InDesignSize);

private:
	FVector2D GetImageSize() const;
	FUnrealGitWidgetDiffCanvasState& GetState() const;
	void PaintGrid(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintOverlays(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	TAttribute<const FSlateBrush*> BaseBrush;
	TAttribute<const FSlateBrush*> OverlayBrush;
	TAttribute<bool> bShowOverlay;
	TAttribute<bool> bFitToWindow;

	TSharedPtr<FUnrealGitWidgetDiffCanvasState> TransformState;
	mutable TSharedPtr<FUnrealGitWidgetDiffCanvasState> LocalState;

	TSharedPtr<TArray<FUnrealGitWidgetNode>> HighlightNodes;
	bool bUseLeftGeometry = true;
	TAttribute<FString> SelectedNode;

	TSharedPtr<SWidget> PreviewContent;
	TSharedPtr<class SZoomPan> PreviewZoomPan;
	TSharedPtr<class SBox> PreviewBox;
	FVector2D DesignSize = FVector2D::ZeroVector;

	float ComputeEffectiveZoom() const;
	FVector2D ComputeViewOffset() const;
	void RebuildZoomPan();

	FOnWidgetPicked OnWidgetPicked;

	bool bPanning = false;
	bool bMouseDown = false;
	bool bDragged = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	FVector2D MouseDownPosition = FVector2D::ZeroVector;
};
