// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

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
		const float Dash = 5.0f;
		const float Gap = 4.0f;
		AddDashedEdge(OutPoints, Min, FVector2D(Max.X, Min.Y), Dash, Gap);
		AddDashedEdge(OutPoints, FVector2D(Max.X, Min.Y), Max, Dash, Gap);
		AddDashedEdge(OutPoints, Max, FVector2D(Min.X, Max.Y), Dash, Gap);
		AddDashedEdge(OutPoints, FVector2D(Min.X, Max.Y), Min, Dash, Gap);
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

	// Keep each canvas' pan/zoom contained to its own pane.
	SetClipping(EWidgetClipping::ClipToBounds);
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
	const FSlateBrush* Brush = BaseBrush.Get();
	if (!Brush || Brush->GetResourceObject() == nullptr)
	{
		return FVector2D(100.0f, 100.0f);
	}
	return Brush->ImageSize;
}

FVector2D SUnrealGitWidgetDiffCanvas::ComputeDesiredSize(float /*LayoutScaleMultiplier*/) const
{
	return FVector2D(100.0f, 100.0f);
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
	const FSlateBrush* Base = BaseBrush.Get();
	const FVector2D ImageSize = GetImageSize();
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
	const float FitScale = bFitToWindow.Get(false)
		? FMath::Min(AllottedSize.X / FMath::Max(1.0f, ImageSize.X), AllottedSize.Y / FMath::Max(1.0f, ImageSize.Y))
		: 1.0f;
	const float EffectiveZoom = FitScale * State.Zoom;
	const FVector2D DrawSize = ImageSize * EffectiveZoom;
	const FVector2D Position = (AllottedSize - DrawSize) * 0.5f + State.Pan;

	// Designer-style dark viewport background with a subtle grid.
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (WhiteBrush)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.045f, 0.045f, 0.045f, 1.0f));

		const float GridStep = 50.0f;
		TArray<FVector2D> GridLines;
		for (float X = 0.0f; X <= AllottedSize.X; X += GridStep)
		{
			GridLines.Add(FVector2D(X, 0.0f));
			GridLines.Add(FVector2D(X, AllottedSize.Y));
		}
		for (float Y = 0.0f; Y <= AllottedSize.Y; Y += GridStep)
		{
			GridLines.Add(FVector2D(0.0f, Y));
			GridLines.Add(FVector2D(AllottedSize.X, Y));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			GridLines,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.035f),
			true,
			1.0f);
	}

	if (Base && Base->GetResourceObject() != nullptr)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
			Base,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}

	const FSlateBrush* Overlay = OverlayBrush.Get();
	if (bShowOverlay.Get(false) && Overlay && Overlay->GetResourceObject() != nullptr)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
			Overlay,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
	}

	if (HighlightNodes)
	{
		auto GetGeometry = [this](const FUnrealGitWidgetNode& Node, FVector2D& OutPos, FVector2D& OutSize) -> bool
		{
			if (bUseLeftGeometry)
			{
				if (!Node.bHasLeftGeometry) return false;
				OutPos = Node.LeftPosition;
				OutSize = Node.LeftSize;
				return true;
			}
			if (!Node.bHasRightGeometry) return false;
			OutPos = Node.RightPosition;
			OutSize = Node.RightSize;
			return true;
		};

		// Faint designer-style layout bounds for every widget.
		for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
		{
			FVector2D NodePos, NodeSize;
			if (!GetGeometry(Node, NodePos, NodeSize)) continue;

			const FVector2D Min = Position + NodePos * EffectiveZoom;
			const FVector2D Max = Min + NodeSize * EffectiveZoom;
			TArray<FVector2D> Points;
			AddDashedRect(Points, Min, Max);
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId + 4, AllottedGeometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.07f), true, 1.0f);
		}

		// Change outlines: green added, red removed, yellow modified.
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
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId + 5, AllottedGeometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.9f), true, 2.0f);
		}

		// Selection outline + non-interactive corner handles.
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
				FSlateDrawElement::MakeLines(
					OutDrawElements, LayerId + 6, AllottedGeometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, FLinearColor(0.25f, 0.8f, 1.0f, 1.0f), true, 2.0f);

				if (WhiteBrush)
				{
					const float HandleSize = 6.0f;
					const FVector2D Corners[4] = { Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y) };
					for (const FVector2D& Corner : Corners)
					{
						FSlateDrawElement::MakeBox(
							OutDrawElements, LayerId + 7,
							AllottedGeometry.ToPaintGeometry(FVector2D(HandleSize, HandleSize), FSlateLayoutTransform(Corner - FVector2D(HandleSize * 0.5f))),
							WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.25f, 0.8f, 1.0f, 1.0f));
					}
				}
				break;
			}
		}
	}

	return LayerId + 8;
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
