// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "Widgets/SCompoundWidget.h"

class UUserWidget;

/**
 * @struct FUnrealGitWidgetDiffCanvasState
 * Shared zoom/pan state for synchronised diff canvases.
 */
struct FUnrealGitWidgetDiffCanvasState
{
	float Zoom = 1.0f;
	FVector2D Pan = FVector2D::ZeroVector;
	/** Name of the selected hierarchy node, shared so canvases never capture the view. */
	FString SelectedNodeName;
};

/**
 * @struct FUnrealGitWidgetDiffCanvasTransform
 * Effective transform computed for a given allotted size.
 */
struct FUnrealGitWidgetDiffCanvasTransform
{
	float Zoom = 1.0f;
	FVector2D DrawSize = FVector2D::ZeroVector;
	FVector2D Position = FVector2D::ZeroVector;
};

/**
 * @class SUnrealGitWidgetDiffCanvas
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

	/** Builds the widget from the supplied arguments. @param InArgs Slate construction arguments. */
	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	/**
	 * Handles mouse-wheel zoom.
	 * @param MyGeometry Geometry of this canvas.
	 * @param MouseEvent Mouse event.
	 * @return Handled reply.
	 */
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	/**
	 * Handles mouse-move panning.
	 * @param MyGeometry Geometry of this canvas.
	 * @param MouseEvent Mouse event.
	 * @return Handled reply.
	 */
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Resets zoom, pan and any pending focus target. */
	void ResetView();
	/** @return Current zoom factor of the shared canvas state. */
	float GetZoom() const;
	/**
	 * Installs the live preview widget and its design size into the canvas.
	 * @param InContent Live preview Slate widget to host, or null to clear.
	 * @param InDesignSize Design size of the preview in design units.
	 * @param InPreviewWidget Live user widget whose geometry is captured, or null.
	 */
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
	/** @return Resolved design/image size used for layout and painting. */
	FVector2D GetImageSize() const;
	/** @return Mutable reference to the shared or local canvas state. */
	FUnrealGitWidgetDiffCanvasState& GetState() const;
	/**
	 * Draws the designer grid behind the preview.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintGrid(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Captures each live widget's rectangle into design space.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param OutRects Map from widget name to design-space rectangle.
	 */
	void CollectLiveGeometry(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, TMap<FString, FBox2D>& OutRects) const;
	/**
	 * Paints rulers, design boundary, optional bitmap overlay and change overlays.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param LiveRects Live widget rectangles in design space.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintOverlays(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, const TMap<FString, FBox2D>& LiveRects, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Draws the horizontal and vertical rulers.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintRulers(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Draws the dashed outline of the design area.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintDesignBoundary(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Draws the optional pixel-diff overlay bitmap.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintBitmapOverlay(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Draws dashed change rectangles and badges for changed widgets.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param LiveRects Live widget rectangles in design space.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintChangeOverlays(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, const TMap<FString, FBox2D>& LiveRects, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	/**
	 * Draws the selection outline for the currently selected node.
	 * @param AllottedGeometry Geometry of this canvas.
	 * @param Position Top-left of the image in canvas space.
	 * @param EffectiveZoom Effective zoom factor.
	 * @param LiveRects Live widget rectangles in design space.
	 * @param OutDrawElements Slate draw list to append to.
	 * @param LayerId Base layer id.
	 */
	void PaintSelection(const FGeometry& AllottedGeometry, const FVector2D& Position, float EffectiveZoom, const TMap<FString, FBox2D>& LiveRects, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

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

	/**
	 * Computes the effective transform for the given allotted size.
	 * @param AllottedSize Size available to the canvas.
	 * @return Zoom, draw size and position for that size.
	 */
	FUnrealGitWidgetDiffCanvasTransform ComputeTransform(const FVector2D& AllottedSize) const;
	/** @return Effective zoom including fit-to-window scaling. */
	float ComputeEffectiveZoom() const;
	/** @return View offset passed to the zoom/pan widget. */
	FVector2D ComputeViewOffset() const;
	/** Rebuilds the preview zoom/pan widget after content changes. */
	void RebuildZoomPan();
	/**
	 * Centres the view on the given point in design space.
	 * @param ImageSize Design size of the image.
	 * @param Center Point in design space to centre on.
	 */
	void CenterOn(const FVector2D& ImageSize, const FVector2D& Center);
	/**
	 * Reveals the named widget in this live preview's scroll boxes.
	 * @param NodeName Name of the widget to reveal.
	 * @return true when the widget was found.
	 */
	bool RevealNodeInScrollBoxes(const FString& NodeName);

	FOnWidgetPicked OnWidgetPicked;

	bool bPanning = false;
	bool bMouseDown = false;
	bool bDragged = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	FVector2D MouseDownPosition = FVector2D::ZeroVector;
};
