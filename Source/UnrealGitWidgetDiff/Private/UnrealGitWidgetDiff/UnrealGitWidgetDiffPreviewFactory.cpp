// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffPreviewFactory.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/SVirtualWindow.h"

bool CreatePreviewWidget(
	UWidgetBlueprint* Blueprint,
	EThumbnailPreviewSizeMode Mode,
	TStrongObjectPtr<UUserWidget>& OutWidget,
	TSharedPtr<SWidget>& OutSlate,
	FVector2D& OutDesignSize,
	bool bBuildSlatePreview,
	const FVector2D& PreviewWindowSize)
{
	OutWidget.Reset();
	OutSlate.Reset();
	OutDesignSize = FVector2D::ZeroVector;

	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return false;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return false;
	}

	UUserWidget* Widget = NewObject<UUserWidget>(World, Blueprint->GeneratedClass, NAME_None, RF_Transient);
	if (!Widget)
	{
		return false;
	}
	Widget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!Widget->Initialize())
	{
		return false;
	}

	if (!bBuildSlatePreview)
	{
		OutWidget = TStrongObjectPtr<UUserWidget>(Widget);
		return true;
	}

	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!SlateWidget.IsValid())
	{
		return false;
	}

	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	Window->SetContent(SlateWidget.ToSharedRef());
	Window->Resize(PreviewWindowSize);
	Window->SlatePrepass(1.0f);
	const FVector2D UnscaledSize = FWidgetBlueprintEditorUtils::GetWidgetPreviewUnScaledCustomSize(
		Window->GetDesiredSize(),
		Widget,
		TOptional<FVector2D>(),
		Mode);

	OutWidget = TStrongObjectPtr<UUserWidget>(Widget);
	OutSlate = SlateWidget;
	OutDesignSize = (FMath::IsFinite(UnscaledSize.X) && FMath::IsFinite(UnscaledSize.Y) &&
		UnscaledSize.X > 0.0f && UnscaledSize.Y > 0.0f &&
		UnscaledSize.X < 16384.0f && UnscaledSize.Y < 16384.0f)
		? UnscaledSize
		: FVector2D(1280.0f, 720.0f);
	return true;
}