// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SUnrealGitWidgetDiffCanvas::Construct(const FArguments& InArgs)
{
	BaseBrush = InArgs._BaseBrush;
	OverlayBrush = InArgs._OverlayBrush;
	bShowOverlay = InArgs._ShowOverlay;
	TransformState = InArgs._TransformState;
	HighlightNodes = InArgs._HighlightNodes;
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
	const FVector2D DrawSize = ImageSize * State.Zoom;
	const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
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
		for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
		{
			if (!Node.bHasGeometry || Node.Change == EUnrealGitWidgetChange::Unchanged)
			{
				continue;
			}

			FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
			switch (Node.Change)
			{
			case EUnrealGitWidgetChange::Added: Color = FLinearColor(0.2f, 0.9f, 0.2f, 1.0f); break;
			case EUnrealGitWidgetChange::Removed: Color = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f); break;
			case EUnrealGitWidgetChange::Modified: Color = FLinearColor(1.0f, 0.8f, 0.1f, 1.0f); break;
			default: break;
			}

			const FVector2D Min = Position + Node.Position * State.Zoom;
			const FVector2D Size = Node.Size * State.Zoom;
			const FVector2D Max = Min + Size;

			TArray<FVector2D> Points;
			Points.Add(Min);
			Points.Add(FVector2D(Max.X, Min.Y));
			Points.Add(Max);
			Points.Add(FVector2D(Min.X, Max.Y));
			Points.Add(Min);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 4,
				AllottedGeometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				Color.CopyWithNewOpacity(0.85f),
				true,
				1.5f);
		}

		// Selection outline (navigated from the Diff Summary or hierarchy).
		const FString Selected = SelectedNode.Get();
		if (!Selected.IsEmpty())
		{
			for (const FUnrealGitWidgetNode& Node : *HighlightNodes)
			{
				if (!Node.bHasGeometry || Node.Name != Selected)
				{
					continue;
				}

				const FVector2D Min = Position + Node.Position * State.Zoom;
				const FVector2D Max = Min + Node.Size * State.Zoom;
				TArray<FVector2D> Points;
				Points.Add(Min);
				Points.Add(FVector2D(Max.X, Min.Y));
				Points.Add(Max);
				Points.Add(FVector2D(Min.X, Max.Y));
				Points.Add(Min);

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 5,
					AllottedGeometry.ToPaintGeometry(),
					Points,
					ESlateDrawEffect::None,
					FLinearColor(0.25f, 0.8f, 1.0f, 1.0f),
					true,
					2.5f);
				break;
			}
		}
	}

	return LayerId + 6;
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
