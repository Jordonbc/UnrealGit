// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffRenderer.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Slate/WidgetRenderer.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/SVirtualWindow.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffRenderer"

bool FUnrealGitWidgetDiffRenderer::Render(UWidgetBlueprint* Blueprint, EThumbnailPreviewSizeMode Mode, FImage& OutImage, FText& OutError)
{
	OutImage = FImage();
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

	TSharedPtr<SWidget> SlateWidget = WidgetInstance->TakeWidget();
	if (!SlateWidget.IsValid())
	{
		OutError = LOCTEXT("NoSlateWidget", "Widget produced no Slate content.");
		return false;
	}

	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	TUniquePtr<FHittestGrid> HitTestGrid = MakeUnique<FHittestGrid>();
	Window->SetContent(SlateWidget.ToSharedRef());
	Window->Resize(FVector2D(256.f, 256.f));
	Window->SlatePrepass(1.0f);
	const FVector2D DesiredSize = Window->GetDesiredSize();

	if (DesiredSize.X < SMALL_NUMBER || DesiredSize.Y < SMALL_NUMBER)
	{
		OutError = LOCTEXT("ZeroSize", "Widget has zero size.");
		return false;
	}

	TOptional<FVector2D> CustomSize;
	if (Mode == EThumbnailPreviewSizeMode::Custom)
	{
		CustomSize = FVector2D(800.0f, 600.0f);
	}
	const FVector2D UnscaledSize = FWidgetBlueprintEditorUtils::GetWidgetPreviewUnScaledCustomSize(
		DesiredSize,
		WidgetInstance,
		CustomSize,
		Mode);

	if (UnscaledSize.X < SMALL_NUMBER || UnscaledSize.Y < SMALL_NUMBER)
	{
		OutError = LOCTEXT("ZeroUnscaled", "Widget has no preview size.");
		return false;
	}

	const int32 CanvasW = FMath::CeilToInt(UnscaledSize.X);
	const int32 CanvasH = FMath::CeilToInt(UnscaledSize.Y);

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RenderTarget->Filter = TF_Bilinear;
	RenderTarget->ClearColor = FLinearColor::Transparent;
	RenderTarget->SRGB = true;
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->InitCustomFormat(CanvasW, CanvasH, PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);

	Window->Resize(UnscaledSize);

	FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(true);
	WidgetRenderer->SetIsPrepassNeeded(false);
	WidgetRenderer->SetShouldClearTarget(true);
	WidgetRenderer->ViewOffset = FVector2D::ZeroVector;

	WidgetRenderer->DrawWindow(
		RenderTarget,
		*HitTestGrid,
		Window,
		1.0f,
		UnscaledSize,
		0.0f);

	FlushRenderingCommands();

	const bool bReadOk = FImageUtils::GetRenderTargetImage(RenderTarget, OutImage);

	BeginCleanup(WidgetRenderer);
	WidgetRenderer = nullptr;

	RenderTarget->MarkAsGarbage();

	if (!bReadOk || OutImage.SizeX <= 0 || OutImage.SizeY <= 0)
	{
		OutError = LOCTEXT("ReadFailed", "Failed to read rendered widget.");
		return false;
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
			DiffPixels[i] = FColor(255, 0, 255, 255); // magenta highlight
			const int32 X = i % A.SizeX;
			const int32 Y = i / A.SizeX;
			MinX = FMath::Min(MinX, X);
			MinY = FMath::Min(MinY, Y);
			MaxX = FMath::Max(MaxX, X);
			MaxY = FMath::Max(MaxY, Y);
		}
		else
		{
			// Dimmed newer image for context.
			DiffPixels[i] = FColor(
				(uint8)(CB.R * 0.35f),
				(uint8)(CB.G * 0.35f),
				(uint8)(CB.B * 0.35f),
				CB.A);
		}
	}

	// DiffPixels aliases OutDiffImage.RawData, so writes above already landed.

	if (OutDiffPixelCount > 0)
	{
		OutDiffBounds = FIntRect(MinX, MinY, MaxX + 1, MaxY + 1);
	}
	else
	{
		OutDiffBounds = FIntRect(0, 0, 0, 0);
	}

	OutDiffRatio = (float)OutDiffPixelCount / (float)FMath::Max(1, Total);

	return true;
}

#undef LOCTEXT_NAMESPACE
