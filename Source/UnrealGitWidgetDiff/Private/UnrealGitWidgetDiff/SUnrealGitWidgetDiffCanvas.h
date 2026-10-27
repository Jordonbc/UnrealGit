// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "Widgets/SCompoundWidget.h"

class UUserWidget;

/** Shared zoom/pan state for synchronised diff canvases. */
struct FUnrealGitWidgetDiffCanvasState
{
	float Zoom = 1.0f;
	FVector2D Pan = FVector2D::ZeroVector;
	/** Name of the selected hierarchy node, shared so canvases never capture the view. */
	FString SelectedNodeName;
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
		, _UseBothGeometries(false)
		, _DesignSize(FVector2D::ZeroVector)
	{}
		SLATE_ATTRIBUTE(const FSlateBrush*, BaseBrush)
		SLATE_ATTRIBUTE(const FSlateBrush*, OverlayBrush)
		SLATE_ATTRIBUTE(bool, ShowOverlay)
		SLATE_ATTRIBUTE(bool, FitToWindow)
		SLATE_ARGUMENT(TSharedPtr<FUnrealGitWidgetDiffCanvasState>, TransformState)
		SLATE_ARGUMENT(TSharedPtr<TArray<FUnrealGitWidgetNode>>, HighlightNodes)
		SLATE_ARGUMENT(bool, UseLeftGeometry)
		/** Difference mode: draw both revisions' captured geometry. */
		SLATE_ARGUMENT(bool, UseBothGeometries)
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
	void SetPreviewContent(TSharedPtr<SWidget> InContent, FVector2D InDesignSize, UUserWidget* InPreviewWidget = nullptr);

	/**
	 * Reveals and centres the named widget. Ancestor scroll boxes in the live
	 * preview are scrolled so the widget becomes visible; the shared pan is then
	 * centred on its design-space bounds.
	 * @return true when this canvas found the widget and scheduled the focus.
	 */
	bool FocusNode(const FString& NodeName);
	/** Applies a scheduled focus once the live geometry has settled. */
	void TickPendingFocus();

private:
	FVector2D GetImageSize() const;
	FUnrealGitWidgetDiffCanvasState& GetState() const;
	void PaintGrid(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void CollectLiveGeometry(float EffectiveZoom, TMap<FString, FBox2D>& OutRects) const;
	void PaintOverlays(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, const TMap<FString, FBox2D>& LiveRects, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	TAttribute<const FSlateBrush*> BaseBrush;
	TAttribute<const FSlateBrush*> OverlayBrush;
	TAttribute<bool> bShowOverlay;
	TAttribute<bool> bFitToWindow;

	TSharedPtr<FUnrealGitWidgetDiffCanvasState> TransformState;
	mutable TSharedPtr<FUnrealGitWidgetDiffCanvasState> LocalState;

	TSharedPtr<TArray<FUnrealGitWidgetNode>> HighlightNodes;
	bool bUseLeftGeometry = true;
	bool bUseBothGeometries = false;

	TSharedPtr<SWidget> PreviewContent;
	TSharedPtr<class SZoomPan> PreviewZoomPan;
	TSharedPtr<class SBox> PreviewBox;
	FVector2D DesignSize = FVector2D::ZeroVector;
	/** Live widget whose Slate hierarchy is on screen; geometry source for Designer-space outlines. */
	TWeakObjectPtr<UUserWidget> PreviewWidget;
	/** Last live geometry captured during paint, used for click picking in Designer space. */
	mutable TMap<FString, FBox2D> LiveRectCache;
	/** Pending focus target, applied by TickPendingFocus after the live layout settles. */
	FString PendingFocusNode;
	int32 PendingFocusTicks = 0;
	int32 PendingFocusRetries = 0;

	float ComputeEffectiveZoom() const;
	FVector2D ComputeViewOffset() const;
	void RebuildZoomPan();
	void CenterOn(const FVector2D& ImageSize, const FVector2D& Center);
	/** Reveals the named widget in this live preview's scroll boxes. */
	bool RevealNodeInScrollBoxes(const FString& NodeName);

	FOnWidgetPicked OnWidgetPicked;

	bool bPanning = false;
	bool bMouseDown = false;
	bool bDragged = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	FVector2D MouseDownPosition = FVector2D::ZeroVector;
};
