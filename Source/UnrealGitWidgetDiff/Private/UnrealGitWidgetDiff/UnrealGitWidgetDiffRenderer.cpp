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
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffPreviewFactory.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/SVirtualWindow.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffRenderer"

namespace
{
	void CollectWidgetGeometry(UWidget* Widget, TMap<FString, FBox2D>& OutRects)
	{
		if (!Widget)
		{
			return;
		}

		// Positions are paint-space geometry, which already includes the thumbnail
		// renderer's fit scale, so they map 1:1 onto the rendered image pixels.
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D AbsolutePosition = Geometry.GetAbsolutePosition();
		const FVector2D AbsoluteSize = Geometry.GetAbsoluteSize();
		const bool bValidSize = FMath::IsFinite(AbsoluteSize.X) && FMath::IsFinite(AbsoluteSize.Y) &&
			AbsoluteSize.X > 0.0f && AbsoluteSize.Y > 0.0f && AbsoluteSize.X < 100000.0f && AbsoluteSize.Y < 100000.0f;
		const bool bValidPosition = FMath::IsFinite(AbsolutePosition.X) && FMath::IsFinite(AbsolutePosition.Y) &&
			FMath::Abs(AbsolutePosition.X) < 100000.0f && FMath::Abs(AbsolutePosition.Y) < 100000.0f;
		if (bValidSize && bValidPosition)
		{
			OutRects.Add(Widget->GetName(), FBox2D(AbsolutePosition, AbsolutePosition + AbsoluteSize));
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				CollectWidgetGeometry(Panel->GetChildAt(ChildIndex), OutRects);
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

	TStrongObjectPtr<UUserWidget> WidgetStrong;
	TSharedPtr<SWidget> UnusedSlate;
	FVector2D UnusedDesignSize;
	if (!CreatePreviewWidget(Blueprint, Mode, WidgetStrong, UnusedSlate, UnusedDesignSize, false))
	{
		OutError = LOCTEXT("CreateFailed", "Failed to create widget instance.");
		return false;
	}
	UUserWidget* WidgetInstance = WidgetStrong.Get();
	if (!WidgetInstance)
	{
		OutError = LOCTEXT("CreateFailed", "Failed to create widget instance.");
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
	// change outlines without a pixel overlay. The widget instance is the tree that
	// was actually rendered above; the blueprint's template tree has no Slate
	// widgets and therefore no cached geometry.
	CollectWidgetGeometry(WidgetInstance->WidgetTree ? WidgetInstance->WidgetTree->RootWidget : nullptr, OutWidgetRects);

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
