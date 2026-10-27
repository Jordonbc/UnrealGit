// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffStyle.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/Widget.h"
#include "Designer/SZoomPan.h"
#include "Rendering/DrawElements.h"
#include "Settings/EditorStyleSettings.h"
#include "Settings/WidgetDesignerSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	bool IsFinitePoint(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	bool IsValidOutlineRect(const FVector2D& Min, const FVector2D& Max)
	{
		if (!IsFinitePoint(Min) || !IsFinitePoint(Max))
		{
			return false;
		}

		const FVector2D Size = Max - Min;
		const float MaxDimension = 100000.0f;
		return FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) &&
			Size.X > 0.0f && Size.Y > 0.0f &&
			Size.X <= MaxDimension && Size.Y <= MaxDimension;
	}

	/**
	 * Builds a closed rectangular outline. Fixed point count, so this path can
	 * never produce an unbounded allocation regardless of the geometry it gets.
	 */
	void AddRectOutline(TArray<FVector2D>& OutPoints, const FVector2D& Min, const FVector2D& Max)
	{
		if (!IsValidOutlineRect(Min, Max))
		{
			return;
		}

		OutPoints.Add(Min);
		OutPoints.Add(FVector2D(Max.X, Min.Y));
		OutPoints.Add(Max);
		OutPoints.Add(FVector2D(Min.X, Max.Y));
		OutPoints.Add(Min);
	}

	/**
	 * Adds one dashed edge. The segment count is bounded up front so the loop can
	 * never allocate an unbounded number of points, even for absurd geometry.
	 */
	void AddDashedEdge(TArray<FVector2D>& OutPoints, const FVector2D& Start, const FVector2D& End, const float Dash, const float Gap)
	{
		if (!IsFinitePoint(Start) || !IsFinitePoint(End))
		{
			return;
		}

		const FVector2D Delta = End - Start;
		const float Length = Delta.Size();
		if (!FMath::IsFinite(Length) || Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float SafeDash = FMath::Max(1.0f, Dash);
		const float SafeGap = FMath::Max(1.0f, Gap);
		const float Step = SafeDash + SafeGap;
		const int32 MaxSegments = 4096;
		const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(Length / Step), 0, MaxSegments);
		const FVector2D Dir = Delta / Length;

		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const float SegmentStart = Index * Step;
			const float SegmentEnd = FMath::Min(SegmentStart + SafeDash, Length);
			if (SegmentEnd <= SegmentStart)
			{
				break;
			}
			OutPoints.Add(Start + Dir * SegmentStart);
			OutPoints.Add(Start + Dir * SegmentEnd);
		}
	}

	void AddDashedRect(TArray<FVector2D>& OutPoints, const FVector2D& Min, const FVector2D& Max)
	{
		if (!IsValidOutlineRect(Min, Max))
		{
			return;
		}

		AddDashedEdge(OutPoints, Min, FVector2D(Max.X, Min.Y), 6.0f, 4.0f);
		AddDashedEdge(OutPoints, FVector2D(Max.X, Min.Y), Max, 6.0f, 4.0f);
		AddDashedEdge(OutPoints, Max, FVector2D(Min.X, Max.Y), 6.0f, 4.0f);
		AddDashedEdge(OutPoints, FVector2D(Min.X, Max.Y), Min, 6.0f, 4.0f);
	}

	float GetGridCellSize(const float EffectiveZoom, int32& OutRulePeriod, float& OutNominalGridSize)
	{
		OutRulePeriod = FMath::Max(1, (int32)FAppStyle::GetFloat(TEXT("Graph.Panel.GridRulePeriod")));
		OutNominalGridSize = (float)FMath::Max(1, GetDefault<UWidgetDesignerSettings>()->GridSnapSize);

		float Inflation = 1.0f;
		while (EffectiveZoom * Inflation * OutNominalGridSize <= 8.0f)
		{
			Inflation *= 2.0f;
		}
		return OutNominalGridSize * EffectiveZoom * Inflation;
	}
}

void SUnrealGitWidgetDiffCanvas::Construct(const FArguments& InArgs)
{
	BaseBrush = InArgs._BaseBrush;
	OverlayBrush = InArgs._OverlayBrush;
	bShowOverlay = InArgs._ShowOverlay;
	bFitToWindow = InArgs._FitToWindow;
	TransformState = InArgs._TransformState;
	HighlightNodes = InArgs._HighlightNodes;
	bUseLeftGeometry = InArgs._UseLeftGeometry;
	bUseBothGeometries = InArgs._UseBothGeometries;
	PreviewContent = InArgs._PreviewContent;
	DesignSize = InArgs._DesignSize;
	OnWidgetPicked = InArgs._OnWidgetPicked;

	if (PreviewContent.IsValid())
	{
		// Read-only Designer mode: the live hierarchy renders but never receives input.
		PreviewContent->SetVisibility(EVisibility::HitTestInvisible);
	}

	SetClipping(EWidgetClipping::ClipToBounds);

	ChildSlot
	[
		SAssignNew(PreviewZoomPan, SZoomPan)
		.ZoomAmount_Lambda([this]() { return ComputeEffectiveZoom(); })
		.ViewOffset_Lambda([this]() { return ComputeViewOffset(); })
		[
			SAssignNew(PreviewBox, SBox)
			.WidthOverride(100.0f)
			.HeightOverride(100.0f)
			[
				SNullWidget::NullWidget
			]
		]
	];
}

FUnrealGitWidgetDiffCanvasTransform SUnrealGitWidgetDiffCanvas::ComputeTransform(const FVector2D& AllottedSize) const
{
	const FUnrealGitWidgetDiffCanvasState& State = GetState();
	const FVector2D ImageSize = GetImageSize();
	const float FitScale = bFitToWindow.Get(false)
		? FMath::Min(AllottedSize.X / FMath::Max(1.0f, ImageSize.X), AllottedSize.Y / FMath::Max(1.0f, ImageSize.Y))
		: 1.0f;
	const float EffectiveZoom = FitScale * State.Zoom;
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FVector2D Position = (AllottedSize - DrawSize) * 0.5f + State.Pan;

	FUnrealGitWidgetDiffCanvasTransform Result;
	Result.Zoom = EffectiveZoom;
	Result.DrawSize = DrawSize;
	Result.Position = Position;
	return Result;
}

float SUnrealGitWidgetDiffCanvas::ComputeEffectiveZoom() const
{
	const FVector2D AllottedSize = GetCachedGeometry().GetLocalSize();
	return ComputeTransform(AllottedSize).Zoom;
}

FVector2D SUnrealGitWidgetDiffCanvas::ComputeViewOffset() const
{
	const FVector2D AllottedSize = GetCachedGeometry().GetLocalSize();
	const FUnrealGitWidgetDiffCanvasTransform Transform = ComputeTransform(AllottedSize);
	return -Transform.Position / FMath::Max(0.001f, Transform.Zoom);
}

FUnrealGitWidgetDiffCanvasState& SUnrealGitWidgetDiffCanvas::GetState() const
{
	if (TransformState.IsValid())
	{
		return *TransformState;
	}
	if (!LocalState.IsValid())
	{
		LocalState = MakeShared<FUnrealGitWidgetDiffCanvasState>();
	}
	return *LocalState;
}

FVector2D SUnrealGitWidgetDiffCanvas::GetImageSize() const
{
	FVector2D Size = FVector2D(100.0f, 100.0f);
	if (DesignSize.X > 0.0f && DesignSize.Y > 0.0f)
	{
		Size = DesignSize;
	}
	else
	{
		const FSlateBrush* Brush = BaseBrush.Get();
		if (Brush && Brush->GetResourceObject())
		{
			Size = Brush->ImageSize;
		}
	}

	const float MaxDimension = 16384.0f;
	if (!FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y) ||
		Size.X <= 0.0f || Size.Y <= 0.0f ||
		Size.X > MaxDimension || Size.Y > MaxDimension)
	{
		return FVector2D(100.0f, 100.0f);
	}
	return Size;
}

FVector2D SUnrealGitWidgetDiffCanvas::ComputeDesiredSize(float /*LayoutScaleMultiplier*/) const
{
	return FVector2D(100.0f, 100.0f);
}

void SUnrealGitWidgetDiffCanvas::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SUnrealGitWidgetDiffCanvas::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const FUnrealGitWidgetDiffCanvasTransform Transform = ComputeTransform(AllottedSize);
	const float EffectiveZoom = Transform.Zoom;
	const FVector2D DrawSize = Transform.DrawSize;
	const FVector2D Position = Transform.Position;

	if (!FMath::IsFinite(EffectiveZoom) || EffectiveZoom <= 0.0f || !IsFinitePoint(Position) || !IsFinitePoint(DrawSize))
	{
		return LayerId;
	}

	PaintGrid(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, LayerId);

	int32 MaxLayer = LayerId + 8;
	TMap<FString, FBox2D> LiveRects;
	if (PreviewContent.IsValid())
	{
		MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 8, InWidgetStyle, bParentEnabled);
		// Capture after the child hierarchy has painted so the cached geometry is
		// this frame's arranged geometry, in the live preview's own coordinates.
		CollectLiveGeometry(EffectiveZoom, LiveRects);
	}
	else
	{
		const FSlateBrush* Base = BaseBrush.Get();
		if (Base && Base->GetResourceObject())
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements, LayerId + 8,
				AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
				Base, ESlateDrawEffect::None, FLinearColor::White);
			MaxLayer = LayerId + 8;
		}
	}

	PaintOverlays(AllottedGeometry, Position, EffectiveZoom, LiveRects, OutDrawElements, MaxLayer + 1);
	return MaxLayer + 2;
}

void SUnrealGitWidgetDiffCanvas::CollectLiveGeometry(
	float EffectiveZoom,
	TMap<FString, FBox2D>& OutRects) const
{
	OutRects.Reset();
	LiveRectCache.Reset();

	UUserWidget* LiveWidget = PreviewWidget.Get();
	if (!LiveWidget || !LiveWidget->WidgetTree || !PreviewContent.IsValid() ||
		!FMath::IsFinite(EffectiveZoom) || EffectiveZoom <= 0.0f ||
		DesignSize.X <= 0.0f || DesignSize.Y <= 0.0f)
	{
		return;
	}

	// The live preview root is forced to the design size, so its absolute size
	// gives desktop-pixels-per-design-unit on each axis. Mapping through the root
	// keeps outlines aligned with the preview at any zoom, pan, resize or DPI.
	const FGeometry& RootGeometry = PreviewContent->GetTickSpaceGeometry();
	const FVector2D RootAbsolutePosition = RootGeometry.GetAbsolutePosition();
	const FVector2D RootAbsoluteSize = RootGeometry.GetAbsoluteSize();
	const FVector2D PixelsPerDesignUnit(RootAbsoluteSize.X / DesignSize.X, RootAbsoluteSize.Y / DesignSize.Y);
	if (!FMath::IsFinite(RootAbsolutePosition.X) || !FMath::IsFinite(RootAbsolutePosition.Y) ||
		!FMath::IsFinite(PixelsPerDesignUnit.X) || !FMath::IsFinite(PixelsPerDesignUnit.Y) ||
		PixelsPerDesignUnit.X <= 0.0f || PixelsPerDesignUnit.Y <= 0.0f)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	LiveWidget->WidgetTree->GetAllWidgets(Widgets);
	OutRects.Reserve(Widgets.Num());

	for (UWidget* Widget : Widgets)
	{
		if (!Widget)
		{
			continue;
		}

		const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
		const FVector2D AbsolutePosition = WidgetGeometry.GetAbsolutePosition();
		const FVector2D AbsoluteSize = WidgetGeometry.GetAbsoluteSize();
		const bool bValidSize = FMath::IsFinite(AbsoluteSize.X) && FMath::IsFinite(AbsoluteSize.Y) &&
			AbsoluteSize.X > 0.0f && AbsoluteSize.Y > 0.0f &&
			AbsoluteSize.X < 100000.0f && AbsoluteSize.Y < 100000.0f;
		if (!bValidSize || !IsFinitePoint(AbsolutePosition))
		{
			continue;
		}

		const FVector2D DesignTopLeft = (AbsolutePosition - RootAbsolutePosition) / PixelsPerDesignUnit;
		const FVector2D DesignWidgetSize = AbsoluteSize / PixelsPerDesignUnit;
		const FVector2D DesignBottomRight = DesignTopLeft + DesignWidgetSize;
		if (!IsFinitePoint(DesignTopLeft) || !IsFinitePoint(DesignBottomRight))
		{
			continue;
		}

		if (!IsValidOutlineRect(DesignTopLeft, DesignBottomRight))
		{
			continue;
		}

		OutRects.Add(Widget->GetName(), FBox2D(DesignTopLeft, DesignBottomRight));
	}

	LiveRectCache = OutRects;
}

void SUnrealGitWidgetDiffCanvas::PaintGrid(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const FSlateBrush* BackgroundImage = FAppStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	if (!BackgroundImage)
	{
		return;
	}

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), BackgroundImage);

	const bool bAntialias = GetDefault<UEditorStyleSettings>()->bAntiAliasGrid;
	int32 RulePeriod = 1;
	float NominalGridSize = 1.0f;
	const float GridCellSize = GetGridCellSize(EffectiveZoom, RulePeriod, NominalGridSize);

	const FLinearColor RegularColor(FAppStyle::GetColor(TEXT("Graph.Panel.GridLineColor")));
	const FLinearColor RuleColor(FAppStyle::GetColor(TEXT("Graph.Panel.GridRuleColor")));
	const FLinearColor CenterColor(FAppStyle::GetColor(TEXT("Graph.Panel.GridCenterColor")));
	const FVector2D LocalGridOrigin = Position;
	const float RuleSize = GridCellSize * RulePeriod;

	float ImageOffsetX = LocalGridOrigin.X - (RuleSize * FMath::Max(FMath::CeilToInt(LocalGridOrigin.X / RuleSize), 0));
	float ImageOffsetY = LocalGridOrigin.Y - (RuleSize * FMath::Max(FMath::CeilToInt(LocalGridOrigin.Y / RuleSize), 0));

	for (int32 GridIndex = 0; ImageOffsetY < AllottedSize.Y; ImageOffsetY += GridCellSize, ++GridIndex)
	{
		if (ImageOffsetY < 0.0f)
		{
			continue;
		}
		const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
		const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
		if (FMath::IsNearlyEqual(LocalGridOrigin.Y, ImageOffsetY, 1.0f)) Color = &CenterColor;
		TArray<FVector2D> Points;
		Points.Add(FVector2D(0.0f, ImageOffsetY));
		Points.Add(FVector2D(AllottedSize.X, ImageOffsetY));
		FSlateDrawElement::MakeLines(
			OutDrawElements, bIsRuleLine ? (LayerId + 2) : (LayerId + 1),
			AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, *Color, bAntialias);
	}

	for (int32 GridIndex = 0; ImageOffsetX < AllottedSize.X; ImageOffsetX += GridCellSize, ++GridIndex)
	{
		if (ImageOffsetX < 0.0f)
		{
			continue;
		}
		const bool bIsRuleLine = (GridIndex % RulePeriod) == 0;
		const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
		if (FMath::IsNearlyEqual(LocalGridOrigin.X, ImageOffsetX, 1.0f)) Color = &CenterColor;
		TArray<FVector2D> Points;
		Points.Add(FVector2D(ImageOffsetX, 0.0f));
		Points.Add(FVector2D(ImageOffsetX, AllottedSize.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements, bIsRuleLine ? (LayerId + 2) : (LayerId + 1),
			AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, *Color, bAntialias);
	}
}

void SUnrealGitWidgetDiffCanvas::PaintOverlays(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	const TMap<FString, FBox2D>& LiveRects,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	PaintRulers(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, LayerId);

	PaintDesignBoundary(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, LayerId);

	PaintBitmapOverlay(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, LayerId);

	if (!HighlightNodes.IsValid())
	{
		return;
	}

	PaintChangeOverlays(AllottedGeometry, Position, EffectiveZoom, LiveRects, OutDrawElements, LayerId);
	PaintSelection(AllottedGeometry, Position, EffectiveZoom, LiveRects, OutDrawElements, LayerId);
}

void SUnrealGitWidgetDiffCanvas::PaintRulers(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	// Rulers, drawn over the preview so they remain visible.
	int32 RulePeriod = 1;
	float NominalGridSize = 1.0f;
	const float GridCellSize = GetGridCellSize(EffectiveZoom, RulePeriod, NominalGridSize);
	const float RulerStep = GridCellSize * RulePeriod;
	const float RulerSize = 20.0f;

	if (WhiteBrush)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(AllottedSize.X, RulerSize), FSlateLayoutTransform(FVector2D::ZeroVector)),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(RulerSize, AllottedSize.Y), FSlateLayoutTransform(FVector2D::ZeroVector)),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
	}

	if (RulerStep > 8.0f)
	{
		const FSlateFontInfo RulerFont = FAppStyle::GetFontStyle(TEXT("SmallFont"));
		const float StartX = Position.X - RulerStep * FMath::Max(FMath::CeilToInt(Position.X / RulerStep), 0);
		for (float X = StartX; X < AllottedSize.X; X += RulerStep)
		{
			if (X < RulerSize)
			{
				continue;
			}
			TArray<FVector2D> Tick;
			Tick.Add(FVector2D(X, 0.0f));
			Tick.Add(FVector2D(X, 6.0f));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Tick, ESlateDrawEffect::None, FLinearColor(0.6f, 0.6f, 0.6f, 0.8f), true);
			const int32 Value = FMath::RoundToInt((X - Position.X) / FMath::Max(0.001f, EffectiveZoom));
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2D(80.0f, RulerSize), FSlateLayoutTransform(FVector2D(X + 2.0f, 1.0f))),
				FText::AsNumber(Value), RulerFont, ESlateDrawEffect::None, FLinearColor(0.6f, 0.6f, 0.6f, 0.9f));
		}
		const float StartY = Position.Y - RulerStep * FMath::Max(FMath::CeilToInt(Position.Y / RulerStep), 0);
		for (float Y = StartY; Y < AllottedSize.Y; Y += RulerStep)
		{
			if (Y < RulerSize)
			{
				continue;
			}
			TArray<FVector2D> Tick;
			Tick.Add(FVector2D(0.0f, Y));
			Tick.Add(FVector2D(6.0f, Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Tick, ESlateDrawEffect::None, FLinearColor(0.6f, 0.6f, 0.6f, 0.8f), true);
			const int32 Value = FMath::RoundToInt((Y - Position.Y) / FMath::Max(0.001f, EffectiveZoom));
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2D(80.0f, RulerSize), FSlateLayoutTransform(FVector2D(1.0f, Y + 2.0f))),
				FText::AsNumber(Value), RulerFont, ESlateDrawEffect::None, FLinearColor(0.6f, 0.6f, 0.6f, 0.9f));
		}
	}
}

void SUnrealGitWidgetDiffCanvas::PaintDesignBoundary(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D ImageSize = GetImageSize();
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	// Design-area boundary.
	if (WhiteBrush && DrawSize.X > 0.0f && DrawSize.Y > 0.0f && DrawSize.X < 100000.0f && DrawSize.Y < 100000.0f)
	{
		TArray<FVector2D> Border;
		Border.Add(Position);
		Border.Add(FVector2D(Position.X + DrawSize.X, Position.Y));
		Border.Add(Position + DrawSize);
		Border.Add(FVector2D(Position.X, Position.Y + DrawSize.Y));
		Border.Add(Position);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Border, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.22f), true, 1.0f);
	}
}

void SUnrealGitWidgetDiffCanvas::PaintBitmapOverlay(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D ImageSize = GetImageSize();
	const FVector2D DrawSize = ImageSize * EffectiveZoom;

	// Optional pixel-diff overlay.
	const FSlateBrush* Overlay = OverlayBrush.Get();
	if (bShowOverlay.Get(false) && Overlay && Overlay->GetResourceObject())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
			Overlay, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
	}
}

void SUnrealGitWidgetDiffCanvas::PaintChangeOverlays(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	const TMap<FString, FBox2D>& LiveRects,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	auto DrawChangeBadge = [&](const FVector2D& RectMin, const FVector2D& RectMax, const FLinearColor& Color, const EUnrealGitWidgetChange Change)
	{
		const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
		const float Radius = 6.5f;
		const FVector2D Center(
			FMath::Clamp(RectMax.X + Radius + 3.0f, Radius + 2.0f, FMath::Max(Radius + 2.0f, AllottedSize.X - Radius - 2.0f)),
			FMath::Clamp((RectMin.Y + RectMax.Y) * 0.5f, Radius + 2.0f, FMath::Max(Radius + 2.0f, AllottedSize.Y - Radius - 2.0f)));
		const float Diameter = Radius * 2.0f;

		const FSlateBrush* CircleBrush = FCoreStyle::Get().GetBrush(TEXT("Icons.FilledCircle"));
		const FSlateBrush* BadgeBrush = (CircleBrush && CircleBrush->GetResourceObject()) ? CircleBrush : WhiteBrush;
		if (BadgeBrush)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2D(Diameter, Diameter), FSlateLayoutTransform(Center - FVector2D(Radius))),
				BadgeBrush, ESlateDrawEffect::None, Color);
		}

		auto DrawStroke = [&](const FVector2D& Start, const FVector2D& End)
		{
			TArray<FVector2D> Stroke;
			Stroke.Add(Start);
			Stroke.Add(End);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Stroke, ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);
		};

		const float Glyph = Radius * 0.5f;
		if (Change == EUnrealGitWidgetChange::Added)
		{
			DrawStroke(Center - FVector2D(Glyph, 0.0f), Center + FVector2D(Glyph, 0.0f));
			DrawStroke(Center - FVector2D(0.0f, Glyph), Center + FVector2D(0.0f, Glyph));
		}
		else if (Change == EUnrealGitWidgetChange::Removed)
		{
			DrawStroke(Center - FVector2D(Glyph, Glyph), Center + FVector2D(Glyph, Glyph));
			DrawStroke(Center + FVector2D(Glyph, -Glyph), Center + FVector2D(-Glyph, Glyph));
		}
		else if (Change == EUnrealGitWidgetChange::Modified)
		{
			DrawStroke(Center - FVector2D(Glyph, 0.0f), Center + FVector2D(Glyph, 0.0f));
		}
	};

	auto DrawChangeRect = [&](const FVector2D& RectPosition, const FVector2D& RectSize, const FLinearColor& Color, const EUnrealGitWidgetChange Change)
	{
		const FVector2D Min = Position + RectPosition * EffectiveZoom;
		const FVector2D Max = Min + RectSize * EffectiveZoom;
		if (!IsValidOutlineRect(Min, Max))
		{
			return;
		}

		// Subtle translucent fill keeps nested changed widgets distinguishable
		// without obscuring text, matching the designer diff mockup. Very large
		// rects (root panels) skip the fill so they do not tint the whole preview.
		const float RectArea = (Max.X - Min.X) * (Max.Y - Min.Y);
		const float ViewArea = FMath::Max(1.0f, DrawSize.X * DrawSize.Y);
		if (WhiteBrush && RectArea < ViewArea * 0.5f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(Max - Min, FSlateLayoutTransform(Min)),
				WhiteBrush, ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.12f));
		}

		TArray<FVector2D> Points;
		AddDashedRect(Points, Min, Max);
		if (Points.Num() > 0)
		{
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Color, true, 2.0f);
		}

		DrawChangeBadge(Min, Max, Color, Change);
	};

	auto DrawStoredGeometry = [&](const FUnrealGitWidgetNode& Node, bool bLeft)
	{
		const bool bHasGeometry = bLeft ? Node.bHasLeftGeometry : Node.bHasRightGeometry;
		if (!bHasGeometry)
		{
			return;
		}
		const FVector2D NodePosition = bLeft ? Node.LeftPosition : Node.RightPosition;
		const FVector2D NodeSize = bLeft ? Node.LeftSize : Node.RightSize;
		DrawChangeRect(NodePosition, NodeSize, GetChangeOutlineColor(Node.Change), Node.Change);
	};

	const bool bUseLiveGeometry = PreviewWidget.IsValid();
	for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
	{
		if (Node.Change == EUnrealGitWidgetChange::Unchanged)
		{
			continue;
		}

		if (bUseLiveGeometry)
		{
			// Designer space: outlines come from the live Slate hierarchy on screen,
			// so they track zoom, pan, resize and DPI exactly.
			if (const FBox2D* LiveRect = LiveRects.Find(Node.Name))
			{
				DrawChangeRect(LiveRect->Min, LiveRect->GetSize(), GetChangeOutlineColor(Node.Change), Node.Change);
			}
			continue;
		}

		// Difference mode: use the geometry captured from each rendered revision.
		if (bUseBothGeometries)
		{
			if (Node.Change == EUnrealGitWidgetChange::Removed || Node.Change == EUnrealGitWidgetChange::Modified)
			{
				DrawStoredGeometry(Node, true);
			}
			if (Node.Change == EUnrealGitWidgetChange::Added || Node.Change == EUnrealGitWidgetChange::Modified)
			{
				DrawStoredGeometry(Node, false);
			}
			continue;
		}

		if (bUseLeftGeometry && Node.Change == EUnrealGitWidgetChange::Added)
		{
			continue;
		}
		if (!bUseLeftGeometry && Node.Change == EUnrealGitWidgetChange::Removed)
		{
			continue;
		}
		DrawStoredGeometry(Node, bUseLeftGeometry);
	}
}

void SUnrealGitWidgetDiffCanvas::PaintSelection(
	const FGeometry& AllottedGeometry,
	const FVector2D& Position,
	float EffectiveZoom,
	const TMap<FString, FBox2D>& LiveRects,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FVector2D ImageSize = GetImageSize();
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	const FString& Selected = GetState().SelectedNodeName;
	if (!Selected.IsEmpty())
	{
		for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
		{
			if (Node.Name != Selected)
			{
				continue;
			}

			FVector2D NodePosition = FVector2D::ZeroVector;
			FVector2D NodeSize = FVector2D::ZeroVector;
			bool bHasGeometry = false;
			if (const FBox2D* LiveRect = LiveRects.Find(Node.Name))
			{
				NodePosition = LiveRect->Min;
				NodeSize = LiveRect->GetSize();
				bHasGeometry = true;
			}
			else if (Node.bHasLeftGeometry)
			{
				NodePosition = Node.LeftPosition;
				NodeSize = Node.LeftSize;
				bHasGeometry = true;
			}
			else if (Node.bHasRightGeometry)
			{
				NodePosition = Node.RightPosition;
				NodeSize = Node.RightSize;
				bHasGeometry = true;
			}
			if (!bHasGeometry)
			{
				break;
			}

			const FVector2D Min = Position + NodePosition * EffectiveZoom;
			const FVector2D Max = Min + NodeSize * EffectiveZoom;
			if (!IsValidOutlineRect(Min, Max))
			{
				break;
			}

			TArray<FVector2D> Points;
			AddRectOutline(Points, Min, Max);
			if (Points.Num() > 0)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, FLinearColor(0.25f, 0.8f, 1.0f, 1.0f), true, 2.0f);
			}

			if (WhiteBrush)
			{
				const float HandleSize = 6.0f;
				const FVector2D Corners[4] = { Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y) };
				for (const FVector2D& Corner : Corners)
				{
					FSlateDrawElement::MakeBox(
						OutDrawElements, LayerId,
						AllottedGeometry.ToPaintGeometry(FVector2D(HandleSize, HandleSize), FSlateLayoutTransform(Corner - FVector2D(HandleSize * 0.5f))),
						WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.25f, 0.8f, 1.0f, 1.0f));
				}
			}
			break;
		}
	}
}

FReply SUnrealGitWidgetDiffCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float Delta = MouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Delta))
	{
		return FReply::Unhandled();
	}

	FUnrealGitWidgetDiffCanvasState& State = GetState();
	const float OldZoom = State.Zoom;
	State.Zoom = FMath::Clamp(State.Zoom * FMath::Pow(1.15f, Delta), 0.1f, 10.0f);
	const FVector2D LocalCursor = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D Pivot = (MyGeometry.GetLocalSize() * 0.5f) + State.Pan;
	const FVector2D Offset = LocalCursor - Pivot;
	State.Pan -= Offset * ((State.Zoom / OldZoom) - 1.0f);
	Invalidate(EInvalidateWidget::Paint);
	return FReply::Handled();
}

FReply SUnrealGitWidgetDiffCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPanning = true;
		bMouseDown = true;
		bDragged = false;
		LastMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		MouseDownPosition = LastMousePosition;
		// Capture immediately so the live hierarchy never receives the drag.
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SUnrealGitWidgetDiffCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bPanning)
	{
		FUnrealGitWidgetDiffCanvasState& State = GetState();
		const FVector2D Current = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (FVector2D::Distance(Current, MouseDownPosition) > 4.0f)
		{
			bDragged = true;
		}
		State.Pan += Current - LastMousePosition;
		LastMousePosition = Current;
		Invalidate(EInvalidateWidget::Paint);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SUnrealGitWidgetDiffCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPanning)
	{
		const bool bWasClick = bMouseDown && !bDragged;
		bPanning = false;
		bMouseDown = false;

		if (bWasClick && HighlightNodes.IsValid() && OnWidgetPicked.IsBound())
		{
			const FVector2D LocalCursor = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			const FVector2D AllottedSize = MyGeometry.GetLocalSize();
			const FUnrealGitWidgetDiffCanvasTransform Transform = ComputeTransform(AllottedSize);
			const float EffectiveZoom = Transform.Zoom;
			const FVector2D Position = Transform.Position;
			const FVector2D DesignPos = (LocalCursor - Position) / FMath::Max(0.001f, EffectiveZoom);

			auto Contains = [&DesignPos](const FVector2D& NodePosition, const FVector2D& NodeSize)
			{
				return DesignPos.X >= NodePosition.X && DesignPos.Y >= NodePosition.Y &&
					DesignPos.X <= NodePosition.X + NodeSize.X && DesignPos.Y <= NodePosition.Y + NodeSize.Y;
			};

			const FUnrealGitWidgetNode* Best = nullptr;
			for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
			{
				bool bHit = false;
				if (PreviewWidget.IsValid())
				{
					// Designer space: pick against the live geometry captured during paint.
					if (const FBox2D* LiveRect = LiveRectCache.Find(Node.Name))
					{
						bHit = Contains(LiveRect->Min, LiveRect->GetSize());
					}
				}
				else
				{
					const bool bTestLeft = bUseBothGeometries || bUseLeftGeometry;
					const bool bTestRight = bUseBothGeometries || !bUseLeftGeometry;
					bHit = (bTestLeft && Node.bHasLeftGeometry && Contains(Node.LeftPosition, Node.LeftSize)) ||
						(bTestRight && Node.bHasRightGeometry && Contains(Node.RightPosition, Node.RightSize));
				}

				if (bHit && (!Best || Node.Depth > Best->Depth))
				{
					Best = &Node;
				}
			}

			if (Best)
			{
				OnWidgetPicked.Execute(Best->Name);
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SUnrealGitWidgetDiffCanvas::SetPreviewContent(TSharedPtr<SWidget> InContent, FVector2D InDesignSize, UUserWidget* InPreviewWidget)
{
	PreviewContent = InContent;
	DesignSize = InDesignSize;
	PreviewWidget = InPreviewWidget;
	LiveRectCache.Reset();
	if (PreviewContent.IsValid())
	{
		PreviewContent->SetVisibility(EVisibility::HitTestInvisible);
	}
	if (PreviewBox.IsValid())
	{
		PreviewBox->SetWidthOverride(DesignSize.X > 0.0f ? DesignSize.X : 100.0f);
		PreviewBox->SetHeightOverride(DesignSize.Y > 0.0f ? DesignSize.Y : 100.0f);
		PreviewBox->SetContent(PreviewContent.IsValid() ? PreviewContent.ToSharedRef() : SNullWidget::NullWidget);
	}
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SUnrealGitWidgetDiffCanvas::ResetView()
{
	FUnrealGitWidgetDiffCanvasState& State = GetState();
	State.Zoom = 1.0f;
	State.Pan = FVector2D::ZeroVector;
	PendingFocusNode.Reset();
	PendingFocusTicks = 0;
	PendingFocusRetries = 0;
	Invalidate(EInvalidateWidget::Paint);
}

bool SUnrealGitWidgetDiffCanvas::RevealNodeInScrollBoxes(const FString& NodeName)
{
	UUserWidget* LiveWidget = PreviewWidget.Get();
	if (!LiveWidget || !LiveWidget->WidgetTree)
	{
		return false;
	}

	UWidget* Target = LiveWidget->WidgetTree->FindWidget(FName(*NodeName));
	if (!Target)
	{
		return false;
	}

	// Scroll every ancestor scroll box so the widget is centred inside its
	// viewport; a scroll-box child can otherwise sit outside the visible area.
	for (UWidget* Parent = Target->GetParent(); Parent; Parent = Parent->GetParent())
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(Parent))
		{
			ScrollBox->ScrollWidgetIntoView(Target, /*AnimateScroll*/ false, EDescendantScrollDestination::Center);
		}
	}

	return true;
}

bool SUnrealGitWidgetDiffCanvas::FocusNode(const FString& NodeName)
{
	if (NodeName.IsEmpty())
	{
		return false;
	}

	if (PreviewWidget.IsValid())
	{
		if (!RevealNodeInScrollBoxes(NodeName))
		{
			return false;
		}

		// The scroll offset is applied during the next layout/paint; centre once
		// the live geometry reflects the revealed position.
		PendingFocusNode = NodeName;
		PendingFocusTicks = 3;
		PendingFocusRetries = 10;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
		return true;
	}

	if (!HighlightNodes.IsValid())
	{
		return false;
	}

	// Difference/preview-texture canvas: stored geometry is already in image pixels.
	for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
	{
		if (Node.Name != NodeName)
		{
			continue;
		}

		bool bLeft = bUseLeftGeometry;
		if (bUseBothGeometries)
		{
			bLeft = Node.Change != EUnrealGitWidgetChange::Added;
		}
		const bool bHasGeometry = bLeft ? Node.bHasLeftGeometry : Node.bHasRightGeometry;
		if (!bHasGeometry)
		{
			return false;
		}

		const FVector2D NodePosition = bLeft ? Node.LeftPosition : Node.RightPosition;
		const FVector2D NodeSize = bLeft ? Node.LeftSize : Node.RightSize;
		CenterOn(GetImageSize(), NodePosition + NodeSize * 0.5f);
		return true;
	}

	return false;
}

void SUnrealGitWidgetDiffCanvas::TickPendingFocus()
{
	if (PendingFocusNode.IsEmpty())
	{
		return;
	}

	if (PendingFocusTicks > 0)
	{
		--PendingFocusTicks;
		return;
	}

	const FBox2D* LiveRect = LiveRectCache.Find(PendingFocusNode);
	if (!LiveRect)
	{
		if (--PendingFocusRetries <= 0)
		{
			PendingFocusNode.Reset();
		}
		return;
	}

	CenterOn(GetImageSize(), LiveRect->GetCenter());
	PendingFocusNode.Reset();
}

void SUnrealGitWidgetDiffCanvas::CenterOn(const FVector2D& ImageSize, const FVector2D& Center)
{
	const float Zoom = ComputeEffectiveZoom();
	if (!IsFinitePoint(Center) || !FMath::IsFinite(Zoom) || Zoom <= 0.0f)
	{
		return;
	}

	FUnrealGitWidgetDiffCanvasState& State = GetState();
	State.Pan = (ImageSize * 0.5f - Center) * Zoom;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

float SUnrealGitWidgetDiffCanvas::GetZoom() const
{
	return GetState().Zoom;
}
