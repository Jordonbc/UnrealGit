// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffView.h"

#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"

#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffView"

SUnrealGitWidgetDiffView::SUnrealGitWidgetDiffView() {}

SUnrealGitWidgetDiffView::~SUnrealGitWidgetDiffView()
{
	if (Session.IsValid())
	{
		Session->OnUpdated.Remove(OnUpdatedHandle);
	}
}

void SUnrealGitWidgetDiffView::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;
	CanvasState = MakeShared<FUnrealGitWidgetDiffCanvasState>();
	CanvasNodes = MakeShared<TArray<FUnrealGitWidgetNode>>();

	LeftBrush.DrawAs = ESlateBrushDrawType::Image;
	RightBrush.DrawAs = ESlateBrushDrawType::Image;
	DiffBrush.DrawAs = ESlateBrushDrawType::Image;

	ModeOptions.Add(MakeShared<FString>(TEXT("Side by Side")));
	ModeOptions.Add(MakeShared<FString>(TEXT("Difference")));
	ModeOptions.Add(MakeShared<FString>(TEXT("Onion")));
	CurrentModeOption = ModeOptions[0];

	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Match Designer")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Fill Screen")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Desired")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Custom 800x600")));
	CurrentPreviewModeOption = PreviewModeOptions[0];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SAssignNew(ModeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ModeOptions)
				.InitiallySelectedItem(CurrentModeOption)
				.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnModeChanged)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return CurrentModeOption.IsValid() ? FText::FromString(*CurrentModeOption) : FText::GetEmpty();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SAssignNew(PreviewModeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PreviewModeOptions)
				.InitiallySelectedItem(CurrentPreviewModeOption)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
				{
					if (!NewSelection.IsValid()) return;
					CurrentPreviewModeOption = NewSelection;
					EThumbnailPreviewSizeMode Mode = EThumbnailPreviewSizeMode::MatchDesignerMode;
					if (*NewSelection == TEXT("Fill Screen")) Mode = EThumbnailPreviewSizeMode::FillScreen;
					else if (*NewSelection == TEXT("Desired")) Mode = EThumbnailPreviewSizeMode::Desired;
					else if (*NewSelection == TEXT("Custom 800x600")) Mode = EThumbnailPreviewSizeMode::Custom;
					if (Session.IsValid())
					{
						FUnrealGitWidgetDiffLauncher::Launch(Session->AssetFileName, Session->LeftRevision, Session->RightRevision, Mode);
						FUnrealGitWidgetDiffTab::CloseActive();
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return CurrentPreviewModeOption.IsValid() ? FText::FromString(*CurrentPreviewModeOption) : FText::GetEmpty();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bFitToWindow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged(this, &SUnrealGitWidgetDiffView::OnFitChanged)
				[ SNew(STextBlock).Text(LOCTEXT("FitToWindow", "Fit to Window")) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton).OnClicked(this, &SUnrealGitWidgetDiffView::OnSwapClicked)
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.SortUpDown"))) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetView", "Reset View"))
				.OnClicked_Lambda([this]()
				{
					if (LeftCanvas.IsValid()) LeftCanvas->ResetView();
					if (RightCanvas.IsValid()) RightCanvas->ResetView();
					if (DiffCanvas.IsValid()) DiffCanvas->ResetView();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton).OnClicked(this, &SUnrealGitWidgetDiffView::OnCloseClicked)
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Close"))) ]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return IsOnionMode() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[ SNew(STextBlock).Text(LOCTEXT("BlendLabel", "Blend")) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.MinValue(0.0f).MaxValue(100.0f)
				.Value_Lambda([this]() { return OnionBlendPercent; })
				.OnValueChanged(this, &SUnrealGitWidgetDiffView::OnOnionBlendChanged)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[ SNew(STextBlock).Text_Lambda([this]() { return FText::Format(LOCTEXT("BlendValue", "{0}%"), FText::AsNumber(FMath::RoundToInt(OnionBlendPercent))); }) ]
		]

		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			// Left: read-only hierarchy tree.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(260.0f)
				[
					SNew(SBorder).Padding(4.0f)
					[
						SAssignNew(HierarchyList, STreeView<TSharedPtr<FUnrealGitWidgetNode>>)
						.TreeItemsSource(&HierarchyRoots)
						.OnGenerateRow(this, &SUnrealGitWidgetDiffView::OnGenerateHierarchyRow)
						.OnGetChildren(this, &SUnrealGitWidgetDiffView::OnGetHierarchyChildren)
						.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnHierarchySelectionChanged)
						.SelectionMode(ESelectionMode::Single)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SSeparator).Orientation(Orient_Vertical) ]
			// Center: dual preview canvases.
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
					SAssignNew(Switcher, SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return GetSwitcherIndex(); })

					+ SWidgetSwitcher::Slot()
					[ SNew(STextBlock).Text(LOCTEXT("Loading", "Loading...")).Justification(ETextJustify::Center) ]

					+ SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (Session.IsValid())
							{
								if (!Session->StatusMessage.IsEmpty()) return Session->StatusMessage;
								if (!Session->Result.Error.IsEmpty()) return Session->Result.Error;
							}
							return LOCTEXT("Error", "Error");
						})
						.Justification(ETextJustify::Center)
					]

					+ SWidgetSwitcher::Slot()
					[ SNew(STextBlock).Text(LOCTEXT("Empty", "No diff available.")).Justification(ETextJustify::Center) ]

					+ SWidgetSwitcher::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
							[
								SNew(SBorder)
								.BorderBackgroundColor(FLinearColor(0.35f, 0.08f, 0.08f, 1.0f))
								.Padding(4.0f)
								[ SNew(STextBlock).Text_Lambda([this]() { return FText::Format(LOCTEXT("OldWidget", "Old Widget  {0}"), GetLeftLabel()); }) ]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[ SAssignNew(LeftCanvas, SUnrealGitWidgetDiffCanvas)
								.BaseBrush(this, &SUnrealGitWidgetDiffView::GetLeftBrush)
								.OverlayBrush(this, &SUnrealGitWidgetDiffView::GetHighlightBrush)
								.ShowOverlay(false)
								.FitToWindow_Lambda([this]() { return bFitToWindow; })
								.TransformState(CanvasState)
								.HighlightNodes(CanvasNodes)
								.UseLeftGeometry(true)
								.OnWidgetPicked_Lambda([this](const FString& Name) { SelectWidgetByName(Name); }) ]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[ SNew(SSeparator).Orientation(Orient_Vertical) ]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
							[
								SNew(SBorder)
								.BorderBackgroundColor(FLinearColor(0.08f, 0.3f, 0.08f, 1.0f))
								.Padding(4.0f)
								[ SNew(STextBlock).Text_Lambda([this]() { return FText::Format(LOCTEXT("NewWidget", "New Widget  {0}"), GetRightLabel()); }) ]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[ SAssignNew(RightCanvas, SUnrealGitWidgetDiffCanvas)
								.BaseBrush(this, &SUnrealGitWidgetDiffView::GetRightBrush)
								.OverlayBrush(this, &SUnrealGitWidgetDiffView::GetHighlightBrush)
								.ShowOverlay(false)
								.FitToWindow_Lambda([this]() { return bFitToWindow; })
								.TransformState(CanvasState)
								.HighlightNodes(CanvasNodes)
								.UseLeftGeometry(false)
								.OnWidgetPicked_Lambda([this](const FString& Name) { SelectWidgetByName(Name); }) ]
						]
					]

					+ SWidgetSwitcher::Slot()
					[ SAssignNew(DiffCanvas, SUnrealGitWidgetDiffCanvas)
						.BaseBrush(this, &SUnrealGitWidgetDiffView::GetDiffBrush)
						.ShowOverlay(false)
						.FitToWindow_Lambda([this]() { return bFitToWindow; })
						.TransformState(CanvasState)
						.HighlightNodes(CanvasNodes)
						.UseBothGeometries(true)
						.OnWidgetPicked_Lambda([this](const FString& Name) { SelectWidgetByName(Name); }) ]

					+ SWidgetSwitcher::Slot()
					[ SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ SNew(SImage).Image(this, &SUnrealGitWidgetDiffView::GetRightBrush)
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, OnionBlendPercent / 100.0f)); }) ] ]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SSeparator).Orientation(Orient_Vertical) ]
			// Right: read-only changed-properties list.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(300.0f)
				[
					SNew(SBorder).Padding(4.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[ SAssignNew(DiffSummaryText, STextBlock).Text(LOCTEXT("DiffSummary", "Diff Summary")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
							[ SNew(SCheckBox).IsChecked_Lambda([this]() { return bShowAdded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bShowAdded = (S == ECheckBoxState::Checked); RebuildDiffEntries(); })
								[ SNew(STextBlock).Text(LOCTEXT("FilterAdded", "Added")) ] ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
							[ SNew(SCheckBox).IsChecked_Lambda([this]() { return bShowRemoved ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bShowRemoved = (S == ECheckBoxState::Checked); RebuildDiffEntries(); })
								[ SNew(STextBlock).Text(LOCTEXT("FilterRemoved", "Removed")) ] ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
							[ SNew(SCheckBox).IsChecked_Lambda([this]() { return bShowModified ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bShowModified = (S == ECheckBoxState::Checked); RebuildDiffEntries(); })
								[ SNew(STextBlock).Text(LOCTEXT("FilterModified", "Modified")) ] ]
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SAssignNew(PropertyList, SListView<TSharedPtr<FDiffEntry>>)
							.ListItemsSource(&PropertyItems)
							.OnGenerateRow(this, &SUnrealGitWidgetDiffView::OnGeneratePropertyRow)
							.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnPropertySelectionChanged)
						]
					]
				]
			]
		]

		]
	];

	UpdateBrushes();
	EnsureLivePreviews();
	RebuildHierarchy();
	if (Session.IsValid())
	{
		// Weak binding: a late session broadcast after this view is destroyed is
		// skipped instead of calling into freed memory.
		OnUpdatedHandle = Session->OnUpdated.AddSP(
			StaticCastSharedRef<SUnrealGitWidgetDiffView>(SharedThis(this)),
			&SUnrealGitWidgetDiffView::OnSessionUpdated);
	}
}

void SUnrealGitWidgetDiffView::OnSessionUpdated()
{
	RequestRefresh();
}

void SUnrealGitWidgetDiffView::EnsureLivePreviews()
{
	if (bLivePreviewsBuilt || !Session.IsValid() || Session->State != EUnrealGitWidgetDiffState::Ready || !Session->Images.IsValid())
	{
		return;
	}

	auto Build = [](UWidgetBlueprint* Blueprint, TStrongObjectPtr<UUserWidget>& OutWidget, TSharedPtr<SWidget>& OutSlate, FVector2D& OutSize)
	{
		if (!Blueprint || !Blueprint->GeneratedClass)
		{
			return;
		}
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return;
		}

		UUserWidget* Widget = NewObject<UUserWidget>(World, Blueprint->GeneratedClass, NAME_None, RF_Transient);
		if (!Widget)
		{
			return;
		}
		Widget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!Widget->Initialize())
		{
			return;
		}

		TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
		if (!SlateWidget.IsValid())
		{
			return;
		}

		TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
		Window->SetContent(SlateWidget.ToSharedRef());
		Window->Resize(FVector2D(1024.0f, 1024.0f));
		Window->SlatePrepass(1.0f);
		const FVector2D UnscaledSize = FWidgetBlueprintEditorUtils::GetWidgetPreviewUnScaledCustomSize(
			Window->GetDesiredSize(),
			Widget,
			TOptional<FVector2D>(),
			EThumbnailPreviewSizeMode::MatchDesignerMode);

		OutWidget = TStrongObjectPtr<UUserWidget>(Widget);
		OutSlate = SlateWidget;
		OutSize = (FMath::IsFinite(UnscaledSize.X) && FMath::IsFinite(UnscaledSize.Y) &&
			UnscaledSize.X > 0.0f && UnscaledSize.Y > 0.0f &&
			UnscaledSize.X < 16384.0f && UnscaledSize.Y < 16384.0f)
			? UnscaledSize
			: FVector2D(1280.0f, 720.0f);
	};

	Build(Session->Images->LeftBlueprint, LeftLiveWidget, LeftPreviewWidget, LeftDesignSize);
	Build(Session->Images->RightBlueprint, RightLiveWidget, RightPreviewWidget, RightDesignSize);
	bLivePreviewsBuilt = true;

	if (LeftCanvas.IsValid())
	{
		LeftCanvas->SetPreviewContent(LeftPreviewWidget, LeftDesignSize, LeftLiveWidget.Get());
	}
	if (RightCanvas.IsValid())
	{
		RightCanvas->SetPreviewContent(RightPreviewWidget, RightDesignSize, RightLiveWidget.Get());
	}
}

void SUnrealGitWidgetDiffView::SelectWidgetByName(const FString& Name)
{
	if (!HierarchyList.IsValid())
	{
		return;
	}
	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (Node.IsValid() && Node->Name == Name)
		{
			HierarchyList->SetSelection(Node);
			HierarchyList->RequestScrollIntoView(Node);
			break;
		}
	}
}

void SUnrealGitWidgetDiffView::RequestRefresh()
{
	EnsureLivePreviews();
	UpdateBrushes();
	RebuildHierarchy();
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(GetSwitcherIndex());
	}
	Invalidate(EInvalidateWidget::Layout);
}

void SUnrealGitWidgetDiffView::UpdateBrushes()
{
	UTexture2D* LeftTex = nullptr;
	UTexture2D* RightTex = nullptr;
	UTexture2D* DiffTex = nullptr;
	UTexture2D* HighlightTex = nullptr;

	if (Session.IsValid() && Session->Images.IsValid())
	{
		LeftTex = Session->Images->LeftTexture;
		RightTex = Session->Images->RightTexture;
		DiffTex = Session->Images->DiffTexture;
		HighlightTex = Session->Images->HighlightTexture;
	}

	if (bSwapped)
	{
		UTexture2D* Tmp = LeftTex;
		LeftTex = RightTex;
		RightTex = Tmp;
	}

	UpdateBrush(LeftBrush, LeftTex);
	UpdateBrush(RightBrush, RightTex);
	UpdateBrush(DiffBrush, DiffTex);
	UpdateBrush(HighlightBrush, HighlightTex);
}

void SUnrealGitWidgetDiffView::UpdateBrush(FSlateBrush& Brush, UTexture2D* Texture)
{
	Brush.SetResourceObject(Texture);
	if (!Texture)
	{
		Brush.ImageSize = FVector2D(0.0f, 0.0f);
		return;
	}

	Brush.ImageSize = FVector2D((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
}

void SUnrealGitWidgetDiffView::RebuildHierarchy()
{
	if (!Session.IsValid() || Session->State != EUnrealGitWidgetDiffState::Ready)
	{
		return;
	}

	if (!CanvasNodes.IsValid())
	{
		CanvasNodes = MakeShared<TArray<FUnrealGitWidgetNode>>();
	}
	*CanvasNodes = Session->Result.Hierarchy;

	int32 AddedCount = 0;
	int32 RemovedCount = 0;
	int32 ModifiedCount = 0;
	for (const FUnrealGitWidgetNode& Node : *CanvasNodes)
	{
		switch (Node.Change)
		{
		case EUnrealGitWidgetChange::Added: ++AddedCount; break;
		case EUnrealGitWidgetChange::Removed: ++RemovedCount; break;
		case EUnrealGitWidgetChange::Modified: ++ModifiedCount; break;
		default: break;
		}
	}
	if (DiffSummaryText.IsValid())
	{
		DiffSummaryText->SetText(FText::Format(
			LOCTEXT("DiffSummaryFmt", "Widget Diff Summary\n+{0} Added   -{1} Removed   ~{2} Modified"),
			FText::AsNumber(AddedCount),
			FText::AsNumber(RemovedCount),
			FText::AsNumber(ModifiedCount)));
	}

	RebuildDiffEntries();

	HierarchyItems.Reset();
	HierarchyRoots.Reset();

	TSet<FString> Names;
	for (const FUnrealGitWidgetNode& Node : Session->Result.Hierarchy)
	{
		Names.Add(Node.Name);
	}

	for (const FUnrealGitWidgetNode& Node : Session->Result.Hierarchy)
	{
		TSharedPtr<FUnrealGitWidgetNode> Item = MakeShared<FUnrealGitWidgetNode>(Node);
		HierarchyItems.Add(Item);
		if (Node.ParentName.IsEmpty() || !Names.Contains(Node.ParentName))
		{
			HierarchyRoots.Add(Item);
		}
	}

	if (HierarchyList.IsValid())
	{
		HierarchyList->RequestTreeRefresh();
		for (const TSharedPtr<FUnrealGitWidgetNode>& Item : HierarchyItems)
		{
			HierarchyList->SetItemExpansion(Item, true);
		}
	}
	if (PropertyList.IsValid())
	{
		PropertyList->RequestListRefresh();
	}
	if (LeftCanvas.IsValid()) LeftCanvas->Invalidate(EInvalidateWidget::Paint);
	if (RightCanvas.IsValid()) RightCanvas->Invalidate(EInvalidateWidget::Paint);
	if (DiffCanvas.IsValid()) DiffCanvas->Invalidate(EInvalidateWidget::Paint);
}

void SUnrealGitWidgetDiffView::OnGetHierarchyChildren(TSharedPtr<FUnrealGitWidgetNode> Item, TArray<TSharedPtr<FUnrealGitWidgetNode>>& OutChildren) const
{
	if (!Item.IsValid())
	{
		return;
	}

	for (const TSharedPtr<FUnrealGitWidgetNode>& Candidate : HierarchyItems)
	{
		if (Candidate.IsValid() && Candidate->ParentName == Item->Name)
		{
			OutChildren.Add(Candidate);
		}
	}
}

TSharedRef<ITableRow> SUnrealGitWidgetDiffView::OnGenerateHierarchyRow(TSharedPtr<FUnrealGitWidgetNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FLinearColor Color(0.7f, 0.7f, 0.7f, 1.0f);
	if (Item.IsValid())
	{
		switch (Item->Change)
		{
		case EUnrealGitWidgetChange::Added: Color = FLinearColor(0.3f, 0.9f, 0.3f, 1.0f); break;
		case EUnrealGitWidgetChange::Removed: Color = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); break;
		case EUnrealGitWidgetChange::Modified: Color = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f); break;
		default: Color = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f); break;
		}
	}

	const float Indent = Item.IsValid() ? Item->Depth * 12.0f : 0.0f;
	return SNew(STableRow<TSharedPtr<FUnrealGitWidgetNode>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SSpacer).Size(FVector2D(Indent, 1.0f)) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString())).ColorAndOpacity(FSlateColor(Color)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[ SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->TypeName : FString())).ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f))) ]
	];
}

void SUnrealGitWidgetDiffView::RebuildDiffEntries()
{
	AllDiffEntries.Reset();
	if (Session.IsValid() && Session->State == EUnrealGitWidgetDiffState::Ready)
	{
		for (const FUnrealGitWidgetNode& Node : Session->Result.Hierarchy)
		{
			if (Node.Change == EUnrealGitWidgetChange::Unchanged)
			{
				continue;
			}
			TSharedPtr<FDiffEntry> Entry = MakeShared<FDiffEntry>();
			Entry->Name = Node.Name;
			Entry->TypeName = Node.TypeName;
			Entry->Path = Node.Path;
			Entry->Change = Node.Change;
			Entry->Properties = Node.ChangedProperties;
			if (Node.bIsChildOfChange)
			{
				Entry->Properties.AddUnique(FString::Printf(TEXT("Child of added container: %s"), *Node.ChangedAncestor));
			}
			AllDiffEntries.Add(Entry);
		}
	}

	PropertyItems.Reset();
	for (const TSharedPtr<FDiffEntry>& Entry : AllDiffEntries)
	{
		if (!Entry.IsValid()) continue;
		if (Entry->Change == EUnrealGitWidgetChange::Added && !bShowAdded) continue;
		if (Entry->Change == EUnrealGitWidgetChange::Removed && !bShowRemoved) continue;
		if (Entry->Change == EUnrealGitWidgetChange::Modified && !bShowModified) continue;
		PropertyItems.Add(Entry);
	}
	if (PropertyList.IsValid())
	{
		PropertyList->RequestListRefresh();
	}
}

void SUnrealGitWidgetDiffView::OnHierarchySelectionChanged(TSharedPtr<FUnrealGitWidgetNode> Item, ESelectInfo::Type /*SelectInfo*/)
{
	if (CanvasState.IsValid())
	{
		CanvasState->SelectedNodeName = Item.IsValid() ? Item->Name : FString();
	}
	if (LeftCanvas.IsValid()) LeftCanvas->Invalidate(EInvalidateWidget::Paint);
	if (RightCanvas.IsValid()) RightCanvas->Invalidate(EInvalidateWidget::Paint);
	if (DiffCanvas.IsValid()) DiffCanvas->Invalidate(EInvalidateWidget::Paint);

	if (bSyncingSelection)
	{
		return;
	}
	TGuardValue<bool> SelectionGuard(bSyncingSelection, true);

	if (Item.IsValid() && PropertyList.IsValid())
	{
		for (const TSharedPtr<FDiffEntry>& Entry : PropertyItems)
		{
			if (Entry.IsValid() && Entry->Name == Item->Name)
			{
				PropertyList->SetSelection(Entry);
				PropertyList->RequestScrollIntoView(Entry);
				break;
			}
		}
	}
}

void SUnrealGitWidgetDiffView::OnPropertySelectionChanged(TSharedPtr<FDiffEntry> Item, ESelectInfo::Type /*SelectInfo*/)
{
	if (bSyncingSelection)
	{
		return;
	}
	if (!Item.IsValid() || !HierarchyList.IsValid())
	{
		return;
	}
	TGuardValue<bool> SelectionGuard(bSyncingSelection, true);

	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (Node.IsValid() && Node->Name == Item->Name)
		{
			HierarchyList->SetSelection(Node);
			HierarchyList->RequestScrollIntoView(Node);
			break;
		}
	}
}

TSharedRef<ITableRow> SUnrealGitWidgetDiffView::OnGeneratePropertyRow(TSharedPtr<FDiffEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FLinearColor Color(0.6f, 0.6f, 0.6f, 1.0f);
	FString Glyph = TEXT("~");
	if (Item.IsValid())
	{
		switch (Item->Change)
		{
		case EUnrealGitWidgetChange::Added: Color = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); Glyph = TEXT("+"); break;
		case EUnrealGitWidgetChange::Removed: Color = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f); Glyph = TEXT("-"); break;
		case EUnrealGitWidgetChange::Modified: Color = FLinearColor(0.95f, 0.75f, 0.1f, 1.0f); Glyph = TEXT("~"); break;
		default: break;
		}
	}

	return SNew(STableRow<TSharedPtr<FDiffEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(2.0f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(Color)
			.Padding(4.0f)
			[ SNew(STextBlock).Text(FText::FromString(Glyph)) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 2.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString())) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? Item->TypeName : FString()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? Item->Path : FString()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f)))
				.AutoWrapText(true) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() && Item->Properties.Num() > 0 ? FString::Join(Item->Properties, TEXT(", ")) : FString()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)))
				.AutoWrapText(true) ]
		]
	];
}

int32 SUnrealGitWidgetDiffView::GetSwitcherIndex() const
{
	if (Session.IsValid())
	{
		switch (Session->State)
		{
		case EUnrealGitWidgetDiffState::Loading: return 0;
		case EUnrealGitWidgetDiffState::Error: return 1;
		case EUnrealGitWidgetDiffState::Empty: return 2;
		case EUnrealGitWidgetDiffState::Ready:
			switch (ViewMode)
			{
			case EViewMode::SideBySide: return 3;
			case EViewMode::Difference: return 4;
			case EViewMode::Onion: return 5;
			}
			break;
		}
	}
	return 0;
}

void SUnrealGitWidgetDiffView::OnModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewSelection.IsValid()) return;
	CurrentModeOption = NewSelection;
	if (*NewSelection == TEXT("Side by Side")) ViewMode = EViewMode::SideBySide;
	else if (*NewSelection == TEXT("Difference")) ViewMode = EViewMode::Difference;
	else if (*NewSelection == TEXT("Onion")) ViewMode = EViewMode::Onion;
	RequestRefresh();
}

void SUnrealGitWidgetDiffView::OnFitChanged(ECheckBoxState NewState)
{
	bFitToWindow = (NewState == ECheckBoxState::Checked);
	RequestRefresh();
}

FReply SUnrealGitWidgetDiffView::OnSwapClicked()
{
	bSwapped = !bSwapped;
	RequestRefresh();
	return FReply::Handled();
}

FReply SUnrealGitWidgetDiffView::OnCloseClicked()
{
	FUnrealGitWidgetDiffTab::CloseActive();
	return FReply::Handled();
}

void SUnrealGitWidgetDiffView::OnOnionBlendChanged(float NewValue)
{
	OnionBlendPercent = NewValue;
	RequestRefresh();
}

FText SUnrealGitWidgetDiffView::GetLeftLabel() const
{
	if (Session.IsValid())
	{
		return bSwapped ? Session->Result.Right.Label : Session->Result.Left.Label;
	}
	return FText::GetEmpty();
}

FText SUnrealGitWidgetDiffView::GetRightLabel() const
{
	if (Session.IsValid())
	{
		return bSwapped ? Session->Result.Left.Label : Session->Result.Right.Label;
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
