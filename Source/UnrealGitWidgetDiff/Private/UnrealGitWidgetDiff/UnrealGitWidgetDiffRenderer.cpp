// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffRenderer.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/SVirtualWindow.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffRenderer"

namespace
{
	void CollectWidgetGeometry(UWidget* Widget, const float Scale, const FVector2D& Offset, TMap<FString, FBox2D>& OutRects)
	{
		if (!Widget)
		{
			return;
		}

		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		const FVector2D AbsolutePosition = Geometry.GetAbsolutePosition();
		const bool bValidSize = FMath::IsFinite(LocalSize.X) && FMath::IsFinite(LocalSize.Y) &&
			LocalSize.X > 0.0f && LocalSize.Y > 0.0f && LocalSize.X < 16384.0f && LocalSize.Y < 16384.0f;
		const bool bValidPosition = FMath::IsFinite(AbsolutePosition.X) && FMath::IsFinite(AbsolutePosition.Y) &&
			FMath::Abs(AbsolutePosition.X) < 100000.0f && FMath::Abs(AbsolutePosition.Y) < 100000.0f;
		if (bValidSize && bValidPosition && FMath::IsFinite(Scale) && Scale > 0.0f && Scale < 100.0f)
		{
			const FVector2D Position = AbsolutePosition * Scale + Offset;
			if (FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y))
			{
				OutRects.Add(Widget->GetName(), FBox2D(Position, Position + LocalSize * Scale));
			}
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				CollectWidgetGeometry(Panel->GetChildAt(ChildIndex), Scale, Offset, OutRects);
			}
		}
	}
}

bool FUnrealGitWidgetDiffRenderer::Render(UWidgetBlueprint* Blueprint, EThumbnailPreviewSizeMode Mode, FImage& OutImage, TMap<FString, FBox2D>& OutWidgetRects, FText& OutError)
{
	OutImage = FImage();
	OutWidgetRects.Reset();
	OutError = FText::GetEmpty();

	if (!Blueprint)
	{
		OutError = LOCTEXT("NoBlueprint", "No widget blueprint provided.");
		return false;
	}
	if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		OutError = LOCTEXT("NoGeneratedClass", "Widget blueprint has no compiled preview.");
		return false;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		OutError = LOCTEXT("NoWorld", "Editor world is unavailable.");
		return false;
	}

	UUserWidget* WidgetInstance = NewObject<UUserWidget>(World, Blueprint->GeneratedClass, NAME_None, RF_Transient);
	if (!WidgetInstance)
	{
		OutError = LOCTEXT("CreateFailed", "Failed to create widget instance.");
		return false;
	}

	WidgetInstance->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
	if (!WidgetInstance->Initialize())
	{
		OutError = LOCTEXT("InitFailed", "Failed to initialize widget instance.");
		return false;
	}

	// Let the engine's designer-preview code size and render the widget exactly
	// as the UMG designer thumbnail does (design size mode, DPI, gamma, clipping).
	const FVector2D ThumbnailSize(1024.0f, 1024.0f);
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());

	TOptional<FWidgetBlueprintEditorUtils::FWidgetThumbnailProperties> Properties =
		FWidgetBlueprintEditorUtils::DrawSWidgetInRenderTargetForThumbnail(
			WidgetInstance,
			RenderTarget,
			ThumbnailSize,
			TOptional<FVector2D>(),
			Mode);

	if (!Properties.IsSet())
	{
		OutError = LOCTEXT("RenderFailed", "Widget produced no preview content.");
		return false;
	}

	FlushRenderingCommands();

	if (!FImageUtils::GetRenderTargetImage(RenderTarget, OutImage) || OutImage.SizeX <= 0 || OutImage.SizeY <= 0)
	{
		OutError = LOCTEXT("ReadFailed", "Failed to read rendered widget.");
		return false;
	}

	// Collect each widget's preview rectangle so the viewer can draw designer-style
	// change outlines without a pixel overlay.
	const FVector2D ScaledSize = Properties->ScaledSize;
	if (ScaledSize.X > 0.0f)
	{
		TSharedPtr<SWidget> SlateWidget = WidgetInstance->TakeWidget();
		if (SlateWidget.IsValid())
		{
			TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
			Window->SetContent(SlateWidget.ToSharedRef());
			Window->Resize(ThumbnailSize);
			Window->SlatePrepass(1.0f);

			const FVector2D UnscaledSize = FWidgetBlueprintEditorUtils::GetWidgetPreviewUnScaledCustomSize(
				Window->GetDesiredSize(),
				WidgetInstance,
				TOptional<FVector2D>(),
				Mode);

			if (UnscaledSize.X > 0.0f)
			{
				const float Scale = ScaledSize.X / UnscaledSize.X;
				// The render-target path sizes the target to the fitted widget, so
				// the image origin is (0,0); the returned Offset only applies when
				// drawing into a larger canvas.
				CollectWidgetGeometry(Blueprint->WidgetTree ? Blueprint->WidgetTree->RootWidget : nullptr, Scale, FVector2D::ZeroVector, OutWidgetRects);
			}
		}
	}

	return true;
}

FImage FUnrealGitWidgetDiffRenderer::PadToCanvas(const FImage& Src, const FIntPoint& CanvasSize)
{
	FImage Out;
	if (Src.SizeX <= 0 || Src.SizeY <= 0 || CanvasSize.X <= 0 || CanvasSize.Y <= 0)
	{
		return Out;
	}

	Out.Init(CanvasSize.X, CanvasSize.Y, Src.Format, Src.GammaSpace);
	if (Out.RawData.Num() > 0)
	{
		FMemory::Memset(Out.RawData.GetData(), 0, Out.RawData.Num());
	}

	const int32 BytesPerPixel = Src.GetBytesPerPixel();
	const int32 SrcStride = Src.SizeX * BytesPerPixel;
	const int32 DstStride = CanvasSize.X * BytesPerPixel;
	const int32 OffX = (CanvasSize.X - Src.SizeX) / 2;
	const int32 OffY = (CanvasSize.Y - Src.SizeY) / 2;

	for (int32 y = 0; y < Src.SizeY; ++y)
	{
		if (OffY + y < 0 || OffY + y >= CanvasSize.Y)
		{
			continue;
		}
		uint8* DstRow = Out.RawData.GetData() + (OffY + y) * DstStride + OffX * BytesPerPixel;
		const uint8* SrcRow = Src.RawData.GetData() + y * SrcStride;
		FMemory::Memcpy(DstRow, SrcRow, SrcStride);
	}

	return Out;
}

bool FUnrealGitWidgetDiffRenderer::MakeHighlight(const FImage& A, const FImage& B, FImage& OutHighlightImage)
{
	OutHighlightImage = FImage();
	if (A.SizeX != B.SizeX || A.SizeY != B.SizeY || A.SizeX <= 0 || A.SizeY <= 0)
	{
		return false;
	}

	const int32 Total = A.SizeX * A.SizeY;
	TArrayView64<const FColor> PixelsA = A.AsBGRA8();
	TArrayView64<const FColor> PixelsB = B.AsBGRA8();
	if (PixelsA.Num() != Total || PixelsB.Num() != Total)
	{
		return false;
	}

	OutHighlightImage.Init(A.SizeX, A.SizeY, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	TArrayView64<FColor> Highlight = OutHighlightImage.AsBGRA8();
	if (Highlight.Num() != Total)
	{
		return false;
	}

	for (int32 i = 0; i < Total; ++i)
	{
		const FColor& CA = PixelsA[i];
		const FColor& CB = PixelsB[i];
		const int32 Delta = FMath::Max3(
			FMath::Abs((int32)CA.R - (int32)CB.R),
			FMath::Abs((int32)CA.G - (int32)CB.G),
			FMath::Abs((int32)CA.B - (int32)CB.B));

		Highlight[i] = (Delta > 8)
			? FColor(255, 0, 255, 180)
			: FColor(0, 0, 0, 0);
	}

	return true;
}

bool FUnrealGitWidgetDiffRenderer::ComputeDiff(
	const FImage& A,
	const FImage& B,
	FImage& OutDiffImage,
	int32& OutDiffPixelCount,
	FIntRect& OutDiffBounds,
	float& OutDiffRatio)
{
	OutDiffImage = FImage();
	OutDiffPixelCount = 0;
	OutDiffBounds = FIntRect(0, 0, 0, 0);
	OutDiffRatio = 0.0f;

	if (A.SizeX != B.SizeX || A.SizeY != B.SizeY || A.SizeX <= 0 || A.SizeY <= 0)
	{
		return false;
	}

	const int32 Total = A.SizeX * A.SizeY;
	TArrayView64<const FColor> PixelsA = A.AsBGRA8();
	TArrayView64<const FColor> PixelsB = B.AsBGRA8();
	if (PixelsA.Num() != Total || PixelsB.Num() != Total)
	{
		return false;
	}

	OutDiffImage.Init(A.SizeX, A.SizeY, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	if (OutDiffImage.RawData.Num() <= 0)
	{
		return false;
	}
	TArrayView64<FColor> DiffPixels = OutDiffImage.AsBGRA8();
	if (DiffPixels.Num() != Total)
	{
		return false;
	}

	int32 MinX = A.SizeX, MinY = A.SizeY, MaxX = -1, MaxY = -1;
	const int32 Threshold = 8;

	for (int32 i = 0; i < Total; ++i)
	{
		const FColor& CA = PixelsA[i];
		const FColor& CB = PixelsB[i];

		const int32 Delta = FMath::Max3(
			FMath::Abs((int32)CA.R - (int32)CB.R),
			FMath::Abs((int32)CA.G - (int32)CB.G),
			FMath::Abs((int32)CA.B - (int32)CB.B));

		if (Delta > Threshold)
		{
			++OutDiffPixelCount;
			DiffPixels[i] = FColor(255, 0, 255, 255);
			const int32 X = i % A.SizeX;
			const int32 Y = i / A.SizeX;
			MinX = FMath::Min(MinX, X);
			MinY = FMath::Min(MinY, Y);
			MaxX = FMath::Max(MaxX, X);
			MaxY = FMath::Max(MaxY, Y);
		}
		else
		{
			DiffPixels[i] = FColor(
				(uint8)(CB.R * 0.35f),
				(uint8)(CB.G * 0.35f),
				(uint8)(CB.B * 0.35f),
				CB.A);
		}
	}

	if (OutDiffPixelCount > 0)
	{
		OutDiffBounds = FIntRect(MinX, MinY, MaxX + 1, MaxY + 1);
	}

	OutDiffRatio = (float)OutDiffPixelCount / (float)FMath::Max(1, Total);
	return true;
}

#undef LOCTEXT_NAMESPACE
