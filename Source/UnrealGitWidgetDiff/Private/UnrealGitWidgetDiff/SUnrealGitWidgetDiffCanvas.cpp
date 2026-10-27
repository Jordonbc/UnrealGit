// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"

#include "Designer/SZoomPan.h"
#include "Rendering/DrawElements.h"
#include "Settings/EditorStyleSettings.h"
#include "Settings/WidgetDesignerSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	void AddDashedEdge(TArray<FVector2D>& OutPoints, const FVector2D& Start, const FVector2D& End, const float Dash, const float Gap)
	{
		const FVector2D Delta = End - Start;
		const float Length = Delta.Size();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector2D Dir = Delta / Length;
		float Cursor = 0.0f;
		while (Cursor < Length)
		{
			const float SegmentEnd = FMath::Min(Cursor + Dash, Length);
			OutPoints.Add(Start + Dir * Cursor);
			OutPoints.Add(Start + Dir * SegmentEnd);
			Cursor = SegmentEnd + Gap;
		}
	}

	void AddDashedRect(TArray<FVector2D>& OutPoints, const FVector2D& Min, const FVector2D& Max)
	{
		AddDashedEdge(OutPoints, Min, FVector2D(Max.X, Min.Y), 5.0f, 4.0f);
		AddDashedEdge(OutPoints, FVector2D(Max.X, Min.Y), Max, 5.0f, 4.0f);
		AddDashedEdge(OutPoints, Max, FVector2D(Min.X, Max.Y), 5.0f, 4.0f);
		AddDashedEdge(OutPoints, FVector2D(Min.X, Max.Y), Min, 5.0f, 4.0f);
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
	SelectedNode = InArgs._SelectedNode;
	PreviewContent = InArgs._PreviewContent;
	DesignSize = InArgs._DesignSize;

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

float SUnrealGitWidgetDiffCanvas::ComputeEffectiveZoom() const
{
	const FUnrealGitWidgetDiffCanvasState& State = GetState();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D AllottedSize = GetCachedGeometry().GetLocalSize();
	const float FitScale = bFitToWindow.Get(false)
		? FMath::Min(AllottedSize.X / FMath::Max(1.0f, ImageSize.X), AllottedSize.Y / FMath::Max(1.0f, ImageSize.Y))
		: 1.0f;
	return FitScale * State.Zoom;
}

FVector2D SUnrealGitWidgetDiffCanvas::ComputeViewOffset() const
{
	const float Zoom = ComputeEffectiveZoom();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D AllottedSize = GetCachedGeometry().GetLocalSize();
	const FVector2D Position = (AllottedSize - ImageSize * Zoom) * 0.5f + GetState().Pan;
	return -Position / FMath::Max(0.001f, Zoom);
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
	if (DesignSize.X > 0.0f && DesignSize.Y > 0.0f)
	{
		return DesignSize;
	}
	const FSlateBrush* Brush = BaseBrush.Get();
	return (Brush && Brush->GetResourceObject()) ? Brush->ImageSize : FVector2D(100.0f, 100.0f);
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
	const FUnrealGitWidgetDiffCanvasState& State = GetState();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const float FitScale = bFitToWindow.Get(false)
		? FMath::Min(AllottedSize.X / FMath::Max(1.0f, ImageSize.X), AllottedSize.Y / FMath::Max(1.0f, ImageSize.Y))
		: 1.0f;
	const float EffectiveZoom = FitScale * State.Zoom;
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FVector2D Position = (AllottedSize - DrawSize) * 0.5f + State.Pan;

	PaintGrid(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, LayerId);

	int32 MaxLayer = LayerId + 8;
	if (PreviewContent.IsValid())
	{
		MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 8, InWidgetStyle, bParentEnabled);
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

	PaintOverlays(AllottedGeometry, Position, EffectiveZoom, OutDrawElements, MaxLayer + 1);
	return MaxLayer + 2;
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
		if (ImageOffsetY < 0.0f) continue;
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
		if (ImageOffsetX < 0.0f) continue;
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
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	const FUnrealGitWidgetDiffCanvasState& State = GetState();
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
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
			if (X < RulerSize) continue;
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
			if (Y < RulerSize) continue;
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

	// Design-area boundary.
	if (WhiteBrush)
	{
		TArray<FVector2D> Border;
		Border.Add(Position);
		Border.Add(FVector2D(Position.X + DrawSize.X, Position.Y));
		Border.Add(Position + DrawSize);
		Border.Add(FVector2D(Position.X, Position.Y + DrawSize.Y));
		Border.Add(Position);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Border, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.22f), true, 1.0f);
	}

	// Optional pixel-diff overlay.
	const FSlateBrush* Overlay = OverlayBrush.Get();
	if (bShowOverlay.Get(false) && Overlay && Overlay->GetResourceObject())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
			Overlay, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
	}

	if (!HighlightNodes)
	{
		return;
	}

	auto GetGeometry = [this](const FUnrealGitWidgetNode& Node, FVector2D& OutPos, FVector2D& OutSize) -> bool
	{
		if (bUseLeftGeometry)
		{
			if (!Node.bHasLeftGeometry) return false;
			OutPos = Node.LeftPosition; OutSize = Node.LeftSize; return true;
		}
		if (!Node.bHasRightGeometry) return false;
		OutPos = Node.RightPosition; OutSize = Node.RightSize; return true;
	};

	for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
	{
		FVector2D NodePos, NodeSize;
		if (!GetGeometry(Node, NodePos, NodeSize)) continue;
		const FVector2D Min = Position + NodePos * EffectiveZoom;
		const FVector2D Max = Min + NodeSize * EffectiveZoom;
		TArray<FVector2D> Points;
		AddDashedRect(Points, Min, Max);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.07f), true, 1.0f);
	}

	for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
	{
		FVector2D NodePos, NodeSize;
		if (!GetGeometry(Node, NodePos, NodeSize)) continue;
		if (Node.Change == EUnrealGitWidgetChange::Unchanged) continue;
		if (bUseLeftGeometry && Node.Change == EUnrealGitWidgetChange::Added) continue;
		if (!bUseLeftGeometry && Node.Change == EUnrealGitWidgetChange::Removed) continue;

		FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
		switch (Node.Change)
		{
		case EUnrealGitWidgetChange::Added: Color = FLinearColor(0.2f, 0.9f, 0.2f, 1.0f); break;
		case EUnrealGitWidgetChange::Removed: Color = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f); break;
		case EUnrealGitWidgetChange::Modified: Color = FLinearColor(1.0f, 0.8f, 0.1f, 1.0f); break;
		default: break;
		}
		const FVector2D Min = Position + NodePos * EffectiveZoom;
		const FVector2D Max = Min + NodeSize * EffectiveZoom;
		TArray<FVector2D> Points;
		AddDashedRect(Points, Min, Max);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.9f), true, 2.0f);
	}

	const FString Selected = SelectedNode.Get();
	if (!Selected.IsEmpty())
	{
		for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
		{
			if (Node.Name != Selected) continue;
			FVector2D NodePos, NodeSize;
			if (!GetGeometry(Node, NodePos, NodeSize)) continue;
			const FVector2D Min = Position + NodePos * EffectiveZoom;
			const FVector2D Max = Min + NodeSize * EffectiveZoom;
			TArray<FVector2D> Points;
			Points.Add(Min);
			Points.Add(FVector2D(Max.X, Min.Y));
			Points.Add(Max);
			Points.Add(FVector2D(Min.X, Max.Y));
			Points.Add(Min);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, FLinearColor(0.25f, 0.8f, 1.0f, 1.0f), true, 2.0f);

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
		LastMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
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
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SUnrealGitWidgetDiffCanvas::SetPreviewContent(TSharedPtr<SWidget> InContent, FVector2D InDesignSize)
{
	PreviewContent = InContent;
	DesignSize = InDesignSize;
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
	Invalidate(EInvalidateWidget::Paint);
}

float SUnrealGitWidgetDiffCanvas::GetZoom() const
{
	return GetState().Zoom;
}
