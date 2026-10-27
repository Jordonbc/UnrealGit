// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"

#include "Async/Async.h"
#include "DiffUtils.h"
#include "Editor.h"
#include "ImageUtils.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SourceControlHelpers.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffRenderer.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffHierarchy.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffLauncher"

void FUnrealGitWidgetDiffLauncher::Launch(const FString& AssetFileName, const FString& LeftRevision, const FString& RightRevision, EThumbnailPreviewSizeMode Mode)
{
	TSharedPtr<FUnrealGitWidgetDiffSession> Session = MakeShared<FUnrealGitWidgetDiffSession>();
	Session->AssetFileName = AssetFileName;
	Session->LeftRevision = LeftRevision;
	Session->RightRevision = RightRevision;

	FUnrealGitWidgetDiffTab::OpenSession(Session);

	const bool bLeftIsRevision = !LeftRevision.IsEmpty();
	const bool bRightIsRevision = !RightRevision.IsEmpty();

	AsyncTask(ENamedThreads::AnyThread, [Session, AssetFileName, LeftRevision, RightRevision, bLeftIsRevision, bRightIsRevision, Mode]()
	{
		// Materialize revisions off the game thread.
		TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> LeftRevisionPtr;
		FString LeftTempFile;
		FDateTime LeftDate = FDateTime::Now();
		FText LeftMatError;
		bool bLeftMatOk = true;

		TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> RightRevisionPtr;
		FString RightTempFile;
		FDateTime RightDate = FDateTime::Now();
		FText RightMatError;
		bool bRightMatOk = true;

		if (bLeftIsRevision)
		{
			ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
			FSourceControlStatePtr State = Provider.GetState(AssetFileName, EStateCacheUsage::Use);
			if (!State.IsValid())
			{
				bLeftMatOk = false;
				LeftMatError = LOCTEXT("NoStateLeft", "Could not query source control state for the left revision.");
			}
			else
			{
				LeftRevisionPtr = State->FindHistoryRevision(LeftRevision);
				if (!LeftRevisionPtr.IsValid())
				{
					bLeftMatOk = false;
					LeftMatError = LOCTEXT("NoRevisionLeft", "Could not find the left revision in history.");
				}
				else if (!LeftRevisionPtr->Get(LeftTempFile))
				{
					bLeftMatOk = false;
					LeftMatError = LOCTEXT("GetLeftFailed", "Failed to materialize the left revision.");
				}
				else
				{
					LeftDate = LeftRevisionPtr->GetDate();
				}
			}
		}

		if (bRightIsRevision)
		{
			ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
			FSourceControlStatePtr State = Provider.GetState(AssetFileName, EStateCacheUsage::Use);
			if (!State.IsValid())
			{
				bRightMatOk = false;
				RightMatError = LOCTEXT("NoStateRight", "Could not query source control state for the right revision.");
			}
			else
			{
				RightRevisionPtr = State->FindHistoryRevision(RightRevision);
				if (!RightRevisionPtr.IsValid())
				{
					bRightMatOk = false;
					RightMatError = LOCTEXT("NoRevisionRight", "Could not find the right revision in history.");
				}
				else if (!RightRevisionPtr->Get(RightTempFile))
				{
					bRightMatOk = false;
					RightMatError = LOCTEXT("GetRightFailed", "Failed to materialize the right revision.");
				}
				else
				{
					RightDate = RightRevisionPtr->GetDate();
				}
			}
		}

		AsyncTask(ENamedThreads::GameThread, [Session, AssetFileName, LeftRevision, RightRevision, bLeftIsRevision, bRightIsRevision, Mode,
			LeftRevisionPtr, LeftDate, LeftMatError, bLeftMatOk,
			RightRevisionPtr, RightDate, RightMatError, bRightMatOk]()
		{
			if (!bLeftMatOk)
			{
				Session->SetError(LeftMatError);
				return;
			}
			if (!bRightMatOk)
			{
				Session->SetError(RightMatError);
				return;
			}

			UWidgetBlueprint* LeftBlueprint = nullptr;
			if (bLeftIsRevision)
			{
				UPackage* Package = DiffUtils::LoadPackageForDiff(LeftRevisionPtr);
				if (!Package)
				{
					Session->SetError(LOCTEXT("LoadLeftPackageFailed", "Failed to load the left revision package."));
					return;
				}
				const FString AssetName = FPaths::GetBaseFilename(LeftRevisionPtr->GetFilename());
				LeftBlueprint = Cast<UWidgetBlueprint>(FindObject<UObject>(Package, *AssetName));
				if (!LeftBlueprint)
				{
					LeftBlueprint = Cast<UWidgetBlueprint>(Package->FindAssetInPackage());
				}
				if (!LeftBlueprint)
				{
					Session->SetError(LOCTEXT("FindLeftAssetFailed", "Could not find a widget blueprint in the left revision."));
					return;
				}
			}
			else
			{
				FString PackageName;
				if (!FPackageName::TryConvertFilenameToLongPackageName(AssetFileName, PackageName))
				{
					Session->SetError(LOCTEXT("PackageNameLeftFailed", "Could not resolve the left package name."));
					return;
				}
				UPackage* Package = FindObject<UPackage>(nullptr, *PackageName);
				if (!Package)
				{
					Package = LoadPackage(nullptr, *PackageName, LOAD_None);
				}
				if (!Package)
				{
					Session->SetError(LOCTEXT("LoadLeftWorkspaceFailed", "Failed to load the left (working copy) package."));
					return;
				}
				const FString AssetName = FPaths::GetBaseFilename(AssetFileName);
				UObject* Found = FindObject<UObject>(Package, *AssetName);
				if (!Found)
				{
					Found = Package->FindAssetInPackage();
				}
				LeftBlueprint = Cast<UWidgetBlueprint>(Found);
				if (!LeftBlueprint)
				{
					Session->SetError(LOCTEXT("FindLeftWorkspaceAssetFailed", "Could not find a widget blueprint in the working copy."));
					return;
				}
			}

			UWidgetBlueprint* RightBlueprint = nullptr;
			if (bRightIsRevision)
			{
				UPackage* Package = DiffUtils::LoadPackageForDiff(RightRevisionPtr);
				if (!Package)
				{
					Session->SetError(LOCTEXT("LoadRightPackageFailed", "Failed to load the right revision package."));
					return;
				}
				const FString AssetName = FPaths::GetBaseFilename(RightRevisionPtr->GetFilename());
				RightBlueprint = Cast<UWidgetBlueprint>(FindObject<UObject>(Package, *AssetName));
				if (!RightBlueprint)
				{
					RightBlueprint = Cast<UWidgetBlueprint>(Package->FindAssetInPackage());
				}
				if (!RightBlueprint)
				{
					Session->SetError(LOCTEXT("FindRightAssetFailed", "Could not find a widget blueprint in the right revision."));
					return;
				}
			}
			else
			{
				FString PackageName;
				if (!FPackageName::TryConvertFilenameToLongPackageName(AssetFileName, PackageName))
				{
					Session->SetError(LOCTEXT("PackageNameRightFailed", "Could not resolve the right package name."));
					return;
				}
				UPackage* Package = FindObject<UPackage>(nullptr, *PackageName);
				if (!Package)
				{
					Package = LoadPackage(nullptr, *PackageName, LOAD_None);
				}
				if (!Package)
				{
					Session->SetError(LOCTEXT("LoadRightWorkspaceFailed", "Failed to load the right (working copy) package."));
					return;
				}
				const FString AssetName = FPaths::GetBaseFilename(AssetFileName);
				UObject* Found = FindObject<UObject>(Package, *AssetName);
				if (!Found)
				{
					Found = Package->FindAssetInPackage();
				}
				RightBlueprint = Cast<UWidgetBlueprint>(Found);
				if (!RightBlueprint)
				{
					Session->SetError(LOCTEXT("FindRightWorkspaceAssetFailed", "Could not find a widget blueprint in the working copy."));
					return;
				}
			}

			TArray<FUnrealGitWidgetNode> Hierarchy = FUnrealGitWidgetDiffHierarchy::Build(LeftBlueprint, RightBlueprint);

			FImage LeftImage;
			TMap<FString, FBox2D> LeftRects;
			FText RenderError;
			if (!FUnrealGitWidgetDiffRenderer::Render(LeftBlueprint, Mode, LeftImage, LeftRects, RenderError))
			{
				Session->SetError(FText::Format(LOCTEXT("RenderLeftFailed", "Failed to render left side: {0}"), RenderError));
				return;
			}

			FImage RightImage;
			TMap<FString, FBox2D> RightRects;
			FText RenderErrorRight;
			if (!FUnrealGitWidgetDiffRenderer::Render(RightBlueprint, Mode, RightImage, RightRects, RenderErrorRight))
			{
				Session->SetError(FText::Format(LOCTEXT("RenderRightFailed", "Failed to render right side: {0}"), RenderErrorRight));
				return;
			}

			const FIntPoint CanvasSize(
				FMath::Max(LeftImage.SizeX, RightImage.SizeX),
				FMath::Max(LeftImage.SizeY, RightImage.SizeY));
			if (CanvasSize.X <= 0 || CanvasSize.Y <= 0)
			{
				Session->SetError(LOCTEXT("BadCanvas", "Rendered images have invalid dimensions."));
				return;
			}

			// The diff canvas shows the images padded to the common canvas, so the
			// captured geometry must be shifted by the same padding offset.
			const FVector2D LeftPad(
				(float)((CanvasSize.X - LeftImage.SizeX) / 2),
				(float)((CanvasSize.Y - LeftImage.SizeY) / 2));
			const FVector2D RightPad(
				(float)((CanvasSize.X - RightImage.SizeX) / 2),
				(float)((CanvasSize.Y - RightImage.SizeY) / 2));

			for (FUnrealGitWidgetNode& Node : Hierarchy)
			{
				if (const FBox2D* LeftRect = LeftRects.Find(Node.Name))
				{
					Node.LeftPosition = LeftRect->Min + LeftPad;
					Node.LeftSize = LeftRect->GetSize();
					Node.bHasLeftGeometry = true;
				}
				if (const FBox2D* RightRect = RightRects.Find(Node.Name))
				{
					Node.RightPosition = RightRect->Min + RightPad;
					Node.RightSize = RightRect->GetSize();
					Node.bHasRightGeometry = true;
				}
			}

			const FImage PaddedLeft = FUnrealGitWidgetDiffRenderer::PadToCanvas(LeftImage, CanvasSize);
			const FImage PaddedRight = FUnrealGitWidgetDiffRenderer::PadToCanvas(RightImage, CanvasSize);

			FImage DiffImage;
			int32 DiffPixelCount = 0;
			FIntRect DiffBounds(0, 0, 0, 0);
			float DiffRatio = 0.0f;
			if (!FUnrealGitWidgetDiffRenderer::ComputeDiff(PaddedLeft, PaddedRight, DiffImage, DiffPixelCount, DiffBounds, DiffRatio))
			{
				Session->SetError(LOCTEXT("ComputeDiffFailed", "Failed to compute the difference image."));
				return;
			}

			UUnrealGitWidgetDiffImages* ImagesObj = NewObject<UUnrealGitWidgetDiffImages>(GetTransientPackage());
			ImagesObj->AddToRoot();

			FImage HighlightImage;
			FUnrealGitWidgetDiffRenderer::MakeHighlight(PaddedLeft, PaddedRight, HighlightImage);

			UTexture2D* LeftTexture = FImageUtils::CreateTexture2DFromImage(PaddedLeft);
			UTexture2D* RightTexture = FImageUtils::CreateTexture2DFromImage(PaddedRight);
			UTexture2D* DiffTexture = FImageUtils::CreateTexture2DFromImage(DiffImage);
			UTexture2D* HighlightTexture = FImageUtils::CreateTexture2DFromImage(HighlightImage);

			ImagesObj->LeftTexture = LeftTexture;
			ImagesObj->RightTexture = RightTexture;
			ImagesObj->DiffTexture = DiffTexture;
			ImagesObj->HighlightTexture = HighlightTexture;
			ImagesObj->LeftBlueprint = LeftBlueprint;
			ImagesObj->RightBlueprint = RightBlueprint;

			FUnrealGitWidgetDiffResult Result;
			Result.bSuccess = true;
			Result.CanvasSize = CanvasSize;
			Result.DiffPixelCount = DiffPixelCount;
			Result.DiffRatio = DiffRatio;
			Result.DiffBounds = DiffBounds;
			Result.DiffImage = DiffImage;
			Result.DiffTexture = DiffTexture;
			Result.HighlightImage = HighlightImage;
			Result.HighlightTexture = HighlightTexture;

			const FString LeftLabelString = LeftRevision.IsEmpty() ? FString(TEXT("Working Copy")) : LeftRevision.Left(8);
			const FString RightLabelString = RightRevision.IsEmpty() ? FString(TEXT("Working Copy")) : RightRevision.Left(8);

			Result.Left.Label = FText::FromString(LeftLabelString);
			Result.Left.Revision = LeftRevision;
			Result.Left.Date = bLeftIsRevision ? LeftDate : FDateTime::Now();
			Result.Left.Image = PaddedLeft;
			Result.Left.Texture = LeftTexture;
			Result.Left.Error = FText::GetEmpty();

			Result.Right.Label = FText::FromString(RightLabelString);
			Result.Right.Revision = RightRevision;
			Result.Right.Date = bRightIsRevision ? RightDate : FDateTime::Now();
			Result.Right.Image = PaddedRight;
			Result.Right.Texture = RightTexture;
			Result.Right.Error = FText::GetEmpty();
			Result.Hierarchy = Hierarchy;

			TStrongObjectPtr<UUnrealGitWidgetDiffImages> ImagesPtr(ImagesObj);
			Session->SetReady(Result, ImagesPtr);
		});
	});
}

#undef LOCTEXT_NAMESPACE