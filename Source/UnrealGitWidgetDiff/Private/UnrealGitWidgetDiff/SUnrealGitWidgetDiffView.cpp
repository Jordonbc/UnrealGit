// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffView.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailPropertyRow.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffCanvas.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffPreviewFactory.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffStyle.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffView"

SUnrealGitWidgetDiffView::SUnrealGitWidgetDiffView()
{
}

SUnrealGitWidgetDiffView::~SUnrealGitWidgetDiffView()
{
	if (DockManager.IsValid())
	{
		DockManager->UnregisterTabSpawner(TEXT("UnrealGitWidgetDiff.Hierarchy"));
		DockManager->UnregisterTabSpawner(TEXT("UnrealGitWidgetDiff.Viewer"));
		DockManager->UnregisterTabSpawner(TEXT("UnrealGitWidgetDiff.Changes"));
		DockManager->UnregisterTabSpawner(TEXT("UnrealGitWidgetDiff.Details"));
		DockManager.Reset();
	}

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

	if (InArgs._OwnerTab.IsValid())
	{
		DockManager = FGlobalTabmanager::Get()->NewTabManager(InArgs._OwnerTab.Pin().ToSharedRef());
		CreateDetailsViews();
		RegisterDockSpawners();
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildMenuBar()
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			BuildOnionBlendRow()
		]

		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(DockHost, SBox)
			[
				DockManager.IsValid()
					? DockManager->RestoreFrom(MakeDefaultLayout(), nullptr).ToSharedRef()
					: TSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("DockUnavailable", "Dock layout unavailable.")))
			]
		]

		]
	];

	WireSession();
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildMenuBar()
{
	FMenuBarBuilder MenuBarBuilder(nullptr);
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenu", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SUnrealGitWidgetDiffView::FillFileMenu));
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("ViewMenu", "View"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SUnrealGitWidgetDiffView::FillViewMenu));
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenu", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SUnrealGitWidgetDiffView::FillWindowMenu));
	return MenuBarBuilder.MakeWidget();
}

void SUnrealGitWidgetDiffView::FillFileMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FileClose", "Close"),
		LOCTEXT("FileCloseTooltip", "Close the widget diff viewer"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			OnCloseClicked();
		})));
}

void SUnrealGitWidgetDiffView::FillViewMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection(FName(TEXT("UnrealGitWidgetDiffViewMode")), LOCTEXT("ViewModeSection", "View Mode"));

	auto AddModeEntry = [this, &MenuBuilder](int32 Index, const FText& Label)
	{
		MenuBuilder.AddMenuEntry(
			Label,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Index]() { SelectViewMode(Index); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Index]() { return IsViewMode(Index); })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	};

	AddModeEntry(0, LOCTEXT("ModeSideBySide", "Side by Side"));
	AddModeEntry(1, LOCTEXT("ModeDifference", "Difference"));
	AddModeEntry(2, LOCTEXT("ModeOnion", "Onion"));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(FName(TEXT("UnrealGitWidgetDiffViewLayout")), LOCTEXT("ViewLayoutSection", "Layout"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("MenuFitToWindow", "Fit to Window"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ToggleFitToWindow),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SUnrealGitWidgetDiffView::IsFitToWindow)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("MenuResetView", "Reset View"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ResetViewport)));
	MenuBuilder.EndSection();
}

void SUnrealGitWidgetDiffView::SelectViewMode(int32 Index)
{
	if (!ModeOptions.IsValidIndex(Index))
	{
		return;
	}

	CurrentModeOption = ModeOptions[Index];
	switch (Index)
	{
	case 0: ViewMode = EViewMode::SideBySide; break;
	case 1: ViewMode = EViewMode::Difference; break;
	case 2: ViewMode = EViewMode::Onion; break;
	default: return;
	}
	RequestRefresh();
}

bool SUnrealGitWidgetDiffView::IsViewMode(int32 Index) const
{
	return static_cast<int32>(ViewMode) == Index;
}

void SUnrealGitWidgetDiffView::ToggleFitToWindow()
{
	OnFitChanged(bFitToWindow ? ECheckBoxState::Unchecked : ECheckBoxState::Checked);
}

bool SUnrealGitWidgetDiffView::IsFitToWindow() const
{
	return bFitToWindow;
}

void SUnrealGitWidgetDiffView::ResetViewport()
{
	if (LeftCanvas.IsValid()) LeftCanvas->ResetView();
	if (RightCanvas.IsValid()) RightCanvas->ResetView();
	if (DiffCanvas.IsValid()) DiffCanvas->ResetView();
}

void SUnrealGitWidgetDiffView::FillWindowMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection(FName(TEXT("UnrealGitWidgetDiffPanels")), LOCTEXT("WindowPanelsSection", "Panels"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("WindowHierarchy", "Hierarchy"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ShowDockTab, FName(TEXT("UnrealGitWidgetDiff.Hierarchy")))));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("WindowViewer", "Viewer"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ShowDockTab, FName(TEXT("UnrealGitWidgetDiff.Viewer")))));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("WindowChanges", "Changes"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ShowDockTab, FName(TEXT("UnrealGitWidgetDiff.Changes")))));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("WindowDetails", "Details"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ShowDockTab, FName(TEXT("UnrealGitWidgetDiff.Details")))));
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("WindowResetLayout", "Reset Layout"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealGitWidgetDiffView::ResetDockLayout)));
}

void SUnrealGitWidgetDiffView::ShowDockTab(FName TabId)
{
	if (DockManager.IsValid())
	{
		DockManager->TryInvokeTab(FTabId(TabId));
	}
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildToolbar()
{
	return
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
					if (!NewSelection.IsValid())
					{
						return;
					}
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
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("PreviousChangeTooltip", "Select and focus the previous changed widget"))
				.IsEnabled_Lambda([this]() { return HasChanges(); })
				.OnClicked_Lambda([this]()
				{
					NavigateChange(-1);
					return FReply::Handled();
				})
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.ArrowLeft"))) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("NextChangeTooltip", "Select and focus the next changed widget"))
				.IsEnabled_Lambda([this]() { return HasChanges(); })
				.OnClicked_Lambda([this]()
				{
					NavigateChange(1);
					return FReply::Handled();
				})
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.ArrowRight"))) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetChangeCounterText(); })
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
					ResetViewport();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetLayout", "Reset Layout"))
				.ToolTipText(LOCTEXT("ResetLayoutTooltip", "Restore the default panel docking layout"))
				.OnClicked_Lambda([this]()
				{
					ResetDockLayout();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton).OnClicked(this, &SUnrealGitWidgetDiffView::OnCloseClicked)
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Close"))) ]
			]
	;
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildOnionBlendRow()
{
	return
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
	;
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildViewer()
{
	return
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
								if (!Session->GetResult().Error.IsEmpty()) return Session->GetResult().Error;
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
	;
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildHierarchyPanel()
{
	return
		SNew(SBorder).Padding(4.0f)
		[
			SAssignNew(HierarchyList, STreeView<TSharedPtr<FUnrealGitWidgetNode>>)
			.TreeItemsSource(&HierarchyRoots)
			.OnGenerateRow(this, &SUnrealGitWidgetDiffView::OnGenerateHierarchyRow)
			.OnGetChildren(this, &SUnrealGitWidgetDiffView::OnGetHierarchyChildren)
			.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnHierarchySelectionChanged)
			.OnMouseButtonClick(this, &SUnrealGitWidgetDiffView::OnHierarchyRowClicked)
			.SelectionMode(ESelectionMode::Single)
		]
	;
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildSummaryPanel()
{
	return
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
	;
}

void SUnrealGitWidgetDiffView::RegisterDockSpawners()
{
	if (!DockManager.IsValid())
	{
		return;
	}

	DockManager->RegisterTabSpawner(TEXT("UnrealGitWidgetDiff.Hierarchy"), FOnSpawnTab::CreateSP(this, &SUnrealGitWidgetDiffView::SpawnHierarchyTab))
		.SetDisplayName(LOCTEXT("DockTabHierarchy", "Hierarchy"));
	DockManager->RegisterTabSpawner(TEXT("UnrealGitWidgetDiff.Viewer"), FOnSpawnTab::CreateSP(this, &SUnrealGitWidgetDiffView::SpawnViewerTab))
		.SetDisplayName(LOCTEXT("DockTabViewer", "Viewer"));
	DockManager->RegisterTabSpawner(TEXT("UnrealGitWidgetDiff.Changes"), FOnSpawnTab::CreateSP(this, &SUnrealGitWidgetDiffView::SpawnChangesTab))
		.SetDisplayName(LOCTEXT("DockTabChanges", "Changes"));
	DockManager->RegisterTabSpawner(TEXT("UnrealGitWidgetDiff.Details"), FOnSpawnTab::CreateSP(this, &SUnrealGitWidgetDiffView::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DockTabDetails", "Details"));
}

void SUnrealGitWidgetDiffView::ResetDockLayout()
{
	if (!DockManager.IsValid() || !DockHost.IsValid())
	{
		return;
	}

	DockManager->CloseAllAreas();
	DockHost->SetContent(DockManager->RestoreFrom(MakeDefaultLayout(), nullptr).ToSharedRef());
}

TSharedRef<FTabManager::FLayout> SUnrealGitWidgetDiffView::MakeDefaultLayout()
{
	return FTabManager::NewLayout("UnrealGitWidgetDiffWorkspace")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(TEXT("UnrealGitWidgetDiff.Hierarchy"), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.55f)
				->AddTab(TEXT("UnrealGitWidgetDiff.Viewer"), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(TEXT("UnrealGitWidgetDiff.Details"), ETabState::OpenedTab)
				->AddTab(TEXT("UnrealGitWidgetDiff.Changes"), ETabState::OpenedTab)
			)
		);
}

TSharedRef<SDockTab> SUnrealGitWidgetDiffView::SpawnHierarchyTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			BuildHierarchyPanel()
		];
}

TSharedRef<SDockTab> SUnrealGitWidgetDiffView::SpawnViewerTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			BuildViewer()
		];
}

TSharedRef<SDockTab> SUnrealGitWidgetDiffView::SpawnChangesTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			BuildSummaryPanel()
		];
}

TSharedRef<SDockTab> SUnrealGitWidgetDiffView::SpawnDetailsTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			BuildDetailsPanel()
		];
}

namespace UnrealGitWidgetDiffDetails
{
	/**
	 * Finds a property on a class or any of its UWidget ancestors.
	 * @param Class Class to search from.
	 * @param PropertyName Property name.
	 * @return Property, or null when not found.
	 */
	static const FProperty* FindPropertyInHierarchy(const UClass* Class, FName PropertyName)
	{
		for (const UClass* CurrentClass = Class; CurrentClass && CurrentClass->IsChildOf(UWidget::StaticClass()); CurrentClass = CurrentClass->GetSuperClass())
		{
			if (const FProperty* Property = CurrentClass->FindPropertyByName(PropertyName))
			{
				return Property;
			}
		}
		return nullptr;
	}

	/**
	 * @class FChangedPropertyCustomization
	 * Wraps properties that differ between the two revisions in an orange border
	 * inside the native read-only details views.
	 */
	class FChangedPropertyCustomization : public IDetailCustomization
	{
	public:
		/**
		 * @param InChangedRoots Top-level property names that changed.
		 */
		explicit FChangedPropertyCustomization(TSharedPtr<TSet<FName>> InChangedRoots, UClass* InCustomizedClass)
			: ChangedRoots(MoveTemp(InChangedRoots))
			, CustomizedClass(InCustomizedClass)
		{
		}

		/**
		 * @param InChangedRoots Top-level property names that changed.
		 * @param InCustomizedClass Class this customisation is registered for.
		 * @return New customization instance.
		 */
		static TSharedRef<IDetailCustomization> MakeInstance(TSharedPtr<TSet<FName>> InChangedRoots, UClass* InCustomizedClass)
		{
			return MakeShared<FChangedPropertyCustomization>(MoveTemp(InChangedRoots), InCustomizedClass);
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			if (!ChangedRoots.IsValid() || ChangedRoots->Num() == 0)
			{
				return;
			}

			TArray<TWeakObjectPtr<UObject>> Objects;
			DetailBuilder.GetObjectsBeingCustomized(Objects);
			UWidget* Widget = Objects.Num() > 0 ? Cast<UWidget>(Objects[0].Get()) : nullptr;
			if (!Widget)
			{
				return;
			}

			for (const FName& PropertyName : *ChangedRoots)
			{
				const FProperty* Property = FindPropertyInHierarchy(Widget->GetClass(), PropertyName);
				if (!Property || Property->GetOwnerStruct() != CustomizedClass.Get())
				{
					continue;
				}

				const FString CategoryName = Property->GetMetaData(FName(TEXT("Category")));
				if (CategoryName.IsEmpty())
				{
					continue;
				}

				IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(FName(*CategoryName));
				IDetailPropertyRow& Row = Category.AddProperty(PropertyName);

				TSharedPtr<SWidget> NameWidget;
				TSharedPtr<SWidget> ValueWidget;
				Row.GetDefaultWidgets(NameWidget, ValueWidget);
				if (!NameWidget.IsValid() || !ValueWidget.IsValid())
				{
					continue;
				}

				Row.CustomWidget()
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.95f, 0.55f, 0.10f, 1.0f))
						.Padding(1.0f)
						[
							ValueWidget.ToSharedRef()
						]
					];
			}
		}

	private:
		TSharedPtr<TSet<FName>> ChangedRoots;
		TWeakObjectPtr<UClass> CustomizedClass;
	};
}

void SUnrealGitWidgetDiffView::CreateDetailsViews()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowObjectLabel = false;
	DetailsArgs.bShowModifiedPropertiesOption = false;
	DetailsArgs.bShowDifferingPropertiesOption = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	LeftDetailsScrollBar = SNew(SScrollBar);
	RightDetailsScrollBar = SNew(SScrollBar);

	FDetailsViewArgs LeftArgs = DetailsArgs;
	LeftArgs.ExternalScrollbar = LeftDetailsScrollBar;
	FDetailsViewArgs RightArgs = DetailsArgs;
	RightArgs.ExternalScrollbar = RightDetailsScrollBar;
	LeftDetailsView = PropertyEditorModule.CreateDetailView(LeftArgs);
	RightDetailsView = PropertyEditorModule.CreateDetailView(RightArgs);

	const FIsPropertyEditingEnabled ReadOnlyEditing = FIsPropertyEditingEnabled::CreateLambda([]()
	{
		return false;
	});
	const FIsPropertyReadOnly ReadOnlyProperty = FIsPropertyReadOnly::CreateLambda([](const FPropertyAndParent&)
	{
		return true;
	});

	LeftDetailsView->SetIsPropertyEditingEnabledDelegate(ReadOnlyEditing);
	RightDetailsView->SetIsPropertyEditingEnabledDelegate(ReadOnlyEditing);
	LeftDetailsView->SetIsPropertyReadOnlyDelegate(ReadOnlyProperty);
	RightDetailsView->SetIsPropertyReadOnlyDelegate(ReadOnlyProperty);
}

TSharedRef<SWidget> SUnrealGitWidgetDiffView::BuildDetailsPanel()
{
	auto BuildSide = [](const TSharedPtr<IDetailsView>& DetailsView, const FText& Label) -> TSharedRef<SWidget>
	{
		if (!DetailsView.IsValid())
		{
			return SNew(STextBlock).Text(LOCTEXT("DetailsUnavailable", "Details view unavailable."));
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
			[
				SNew(STextBlock).Text(Label)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				DetailsView->AsShared()
			];
	};

	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			SNew(STextBlock).Text_Lambda([this]() { return GetInspectorHeaderText(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot().Value(0.5f)
			[
				BuildSide(LeftDetailsView, FText::Format(LOCTEXT("DetailsBefore", "Before - {0}"), GetLeftLabel()))
			]
			+ SSplitter::Slot().Value(0.5f)
			[
				BuildSide(RightDetailsView, FText::Format(LOCTEXT("DetailsAfter", "After - {0}"), GetRightLabel()))
			]
		];
}

UWidget* SUnrealGitWidgetDiffView::ResolveWidgetForBlueprint(const UWidgetBlueprint* Blueprint, const FString& Name) const
{
	if (!Blueprint || !Blueprint->WidgetTree || Name.IsEmpty())
	{
		return nullptr;
	}

	return Blueprint->WidgetTree->FindWidget(FName(*Name));
}

void SUnrealGitWidgetDiffView::ApplyDetailsCustomization(const TSharedPtr<IDetailsView>& View, TArray<TWeakObjectPtr<UClass>>& CustomizedClasses, UClass* WidgetClass)
{
	if (!View.IsValid())
	{
		return;
	}

	for (const TWeakObjectPtr<UClass>& CustomizedClass : CustomizedClasses)
	{
		if (CustomizedClass.IsValid())
		{
			View->UnregisterInstancedCustomPropertyLayout(CustomizedClass.Get());
		}
	}
	CustomizedClasses.Reset();

	if (!WidgetClass || !DetailsChangedRoots.IsValid())
	{
		return;
	}

	TSet<UClass*> OwnerClasses;
	for (const FName& RootProperty : *DetailsChangedRoots)
	{
		const FProperty* Property = UnrealGitWidgetDiffDetails::FindPropertyInHierarchy(WidgetClass, RootProperty);
		if (!Property)
		{
			continue;
		}

		if (UClass* OwnerClass = Cast<UClass>(Property->GetOwnerStruct()))
		{
			OwnerClasses.Add(OwnerClass);
		}
	}

	TSharedPtr<TSet<FName>> Roots = DetailsChangedRoots;
	for (UClass* OwnerClass : OwnerClasses)
	{
		View->RegisterInstancedCustomPropertyLayout(OwnerClass, FOnGetDetailCustomizationInstance::CreateLambda([Roots, OwnerClass]()
		{
			return UnrealGitWidgetDiffDetails::FChangedPropertyCustomization::MakeInstance(Roots, OwnerClass);
		}));
		CustomizedClasses.Add(OwnerClass);
	}
}

void SUnrealGitWidgetDiffView::RefreshDetailsViews(const FUnrealGitWidgetNode* Node)
{
	DetailsChangedRoots = MakeShared<TSet<FName>>();
	if (Node)
	{
		for (const FUnrealGitPropertyDiff& Difference : Node->PropertyDiffs)
		{
			int32 CutIndex = Difference.Path.Len();
			int32 DotIndex = INDEX_NONE;
			if (Difference.Path.FindChar(TEXT('.'), DotIndex))
			{
				CutIndex = FMath::Min(CutIndex, DotIndex);
			}
			int32 BracketIndex = INDEX_NONE;
			if (Difference.Path.FindChar(TEXT('['), BracketIndex))
			{
				CutIndex = FMath::Min(CutIndex, BracketIndex);
			}

			const FString RootProperty = Difference.Path.Left(CutIndex);
			if (!RootProperty.IsEmpty() && RootProperty != TEXT("Slot"))
			{
				DetailsChangedRoots->Add(FName(*RootProperty));
			}
		}
	}

	const UUnrealGitWidgetDiffImages* Images = Session.IsValid() ? Session->GetImages() : nullptr;
	const FString WidgetName = Node ? Node->Name : FString();
	UWidget* LeftWidget = Images ? ResolveWidgetForBlueprint(Images->LeftBlueprint, WidgetName) : nullptr;
	UWidget* RightWidget = Images ? ResolveWidgetForBlueprint(Images->RightBlueprint, WidgetName) : nullptr;

	ApplyDetailsCustomization(LeftDetailsView, LeftDetailsCustomizedClasses, LeftWidget ? LeftWidget->GetClass() : nullptr);
	ApplyDetailsCustomization(RightDetailsView, RightDetailsCustomizedClasses, RightWidget ? RightWidget->GetClass() : nullptr);

	if (LeftDetailsView.IsValid())
	{
		LeftDetailsView->SetObject(LeftWidget);
	}
	if (RightDetailsView.IsValid())
	{
		RightDetailsView->SetObject(RightWidget);
	}

	LastLeftScrollProgress = -1.0f;
	LastRightScrollProgress = -1.0f;
}

FText SUnrealGitWidgetDiffView::GetInspectorHeaderText() const
{
	if (InspectorWidgetName.IsEmpty())
	{
		return LOCTEXT("InspectorTitle", "Property Details");
	}
	return FText::Format(LOCTEXT("InspectorTitleForWidget", "Property Details - {0}"), FText::FromString(InspectorWidgetName));
}

void SUnrealGitWidgetDiffView::RefreshInspector(const FUnrealGitWidgetNode* Node)
{
	InspectorWidgetName = Node ? Node->Name : FString();
	RefreshDetailsViews(Node);
	Invalidate(EInvalidateWidget::Paint);
}

void SUnrealGitWidgetDiffView::WireSession()
{
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

void SUnrealGitWidgetDiffView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (LeftCanvas.IsValid()) LeftCanvas->TickPendingFocus();
	if (RightCanvas.IsValid()) RightCanvas->TickPendingFocus();
	if (DiffCanvas.IsValid()) DiffCanvas->TickPendingFocus();

	if (LeftDetailsScrollBar.IsValid() && RightDetailsScrollBar.IsValid() && !bSyncingDetailsScroll)
	{
		auto ComputeProgress = [](const TSharedPtr<SScrollBar>& Bar) -> float
		{
			const float ScrollableRange = 1.0f - Bar->ThumbSizeFraction();
			if (ScrollableRange <= KINDA_SMALL_NUMBER)
			{
				return 0.0f;
			}
			return FMath::Clamp(Bar->DistanceFromTop() / ScrollableRange, 0.0f, 1.0f);
		};

		const float LeftProgress = ComputeProgress(LeftDetailsScrollBar);
		const float RightProgress = ComputeProgress(RightDetailsScrollBar);
		const bool bLeftMoved = !FMath::IsNearlyEqual(LeftProgress, LastLeftScrollProgress, 0.001f);
		const bool bRightMoved = !FMath::IsNearlyEqual(RightProgress, LastRightScrollProgress, 0.001f);

		if (bLeftMoved && !FMath::IsNearlyEqual(LeftProgress, RightProgress, 0.001f))
		{
			bSyncingDetailsScroll = true;
			const float TargetThumb = RightDetailsScrollBar->ThumbSizeFraction();
			RightDetailsScrollBar->SetState(LeftProgress * FMath::Max(0.0f, 1.0f - TargetThumb), TargetThumb, true);
			LastLeftScrollProgress = LeftProgress;
			LastRightScrollProgress = LeftProgress;
			bSyncingDetailsScroll = false;
		}
		else if (bRightMoved && !FMath::IsNearlyEqual(LeftProgress, RightProgress, 0.001f))
		{
			bSyncingDetailsScroll = true;
			const float TargetThumb = LeftDetailsScrollBar->ThumbSizeFraction();
			LeftDetailsScrollBar->SetState(RightProgress * FMath::Max(0.0f, 1.0f - TargetThumb), TargetThumb, true);
			LastLeftScrollProgress = RightProgress;
			LastRightScrollProgress = RightProgress;
			bSyncingDetailsScroll = false;
		}

		LastLeftScrollProgress = ComputeProgress(LeftDetailsScrollBar);
		LastRightScrollProgress = ComputeProgress(RightDetailsScrollBar);
	}
}

void SUnrealGitWidgetDiffView::OnHierarchyRowClicked(TSharedPtr<FUnrealGitWidgetNode> Item)
{
	if (Item.IsValid())
	{
		FocusWidgetNode(*Item);
	}
}

void SUnrealGitWidgetDiffView::FocusWidgetNode(const FUnrealGitWidgetNode& Node)
{
	if (Node.Name.IsEmpty())
	{
		return;
	}

	switch (ViewMode)
	{
	case EViewMode::SideBySide:
		// Reveal inside scroll boxes on both revisions, then centre one of them;
		// both canvases share the pan state.
		if (LeftCanvas.IsValid())
		{
			LeftCanvas->FocusNode(Node.Name);
		}
		if (RightCanvas.IsValid())
		{
			RightCanvas->FocusNode(Node.Name);
		}
		break;
	case EViewMode::Difference:
		if (DiffCanvas.IsValid())
		{
			DiffCanvas->FocusNode(Node.Name);
		}
		break;
	default:
		break;
	}
}

bool SUnrealGitWidgetDiffView::HasChanges() const
{
	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (Node.IsValid() && Node->Change != EUnrealGitWidgetChange::Unchanged)
		{
			return true;
		}
	}
	return false;
}

FText SUnrealGitWidgetDiffView::GetChangeCounterText() const
{
	int32 TotalChanges = 0;
	int32 SelectedIndex = INDEX_NONE;

	TArray<TSharedPtr<FUnrealGitWidgetNode>> SelectedItems;
	if (HierarchyList.IsValid())
	{
		SelectedItems = HierarchyList->GetSelectedItems();
	}
	const TSharedPtr<FUnrealGitWidgetNode> Selected = SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr;

	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (!Node.IsValid() || Node->Change == EUnrealGitWidgetChange::Unchanged)
		{
			continue;
		}

		if (Node == Selected)
		{
			SelectedIndex = TotalChanges;
		}
		++TotalChanges;
	}

	if (TotalChanges == 0)
	{
		return LOCTEXT("ChangeCounterNone", "No changes");
	}
	if (SelectedIndex == INDEX_NONE)
	{
		return FText::Format(LOCTEXT("ChangeCounterUnselected", "Change - of {0}"), FText::AsNumber(TotalChanges));
	}

	return FText::Format(LOCTEXT("ChangeCounterPosition", "Change {0} of {1}"),
		FText::AsNumber(SelectedIndex + 1),
		FText::AsNumber(TotalChanges));
}

TSharedPtr<FUnrealGitWidgetNode> SUnrealGitWidgetDiffView::FindHierarchyNode(const FString& Name) const
{
	if (Name.IsEmpty())
	{
		return nullptr;
	}

	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (Node.IsValid() && Node->Name == Name)
		{
			return Node;
		}
	}
	return nullptr;
}

void SUnrealGitWidgetDiffView::ExpandAncestors(const FUnrealGitWidgetNode& Node)
{
	if (!HierarchyList.IsValid())
	{
		return;
	}

	// Names are unique, and the visited set keeps malformed parent links bounded.
	TSet<FString> Visited;
	FString ParentName = Node.ParentName;
	while (!ParentName.IsEmpty() && !Visited.Contains(ParentName))
	{
		Visited.Add(ParentName);

		const TSharedPtr<FUnrealGitWidgetNode> Parent = FindHierarchyNode(ParentName);
		if (!Parent.IsValid())
		{
			return;
		}
		HierarchyList->SetItemExpansion(Parent, true);
		ParentName = Parent->ParentName;
	}
}

void SUnrealGitWidgetDiffView::NavigateChange(int32 Direction)
{
	if (!HierarchyList.IsValid() || Direction == 0)
	{
		return;
	}

	// Walk the changes in hierarchy order. The current position is derived from
	// the live selection so navigation stays correct across session rebuilds.
	TArray<TSharedPtr<FUnrealGitWidgetNode>> Changes;
	int32 CurrentIndex = INDEX_NONE;

	const TArray<TSharedPtr<FUnrealGitWidgetNode>> SelectedItems = HierarchyList->GetSelectedItems();
	const TSharedPtr<FUnrealGitWidgetNode> Selected = SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr;

	for (const TSharedPtr<FUnrealGitWidgetNode>& Node : HierarchyItems)
	{
		if (!Node.IsValid() || Node->Change == EUnrealGitWidgetChange::Unchanged)
		{
			continue;
		}
		if (Node == Selected)
		{
			CurrentIndex = Changes.Num();
		}
		Changes.Add(Node);
	}

	if (Changes.Num() == 0)
	{
		return;
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	int32 NextIndex = 0;
	if (CurrentIndex == INDEX_NONE)
	{
		NextIndex = Step > 0 ? 0 : Changes.Num() - 1;
	}
	else
	{
		NextIndex = (CurrentIndex + Step + Changes.Num()) % Changes.Num();
	}

	const TSharedPtr<FUnrealGitWidgetNode>& Target = Changes[NextIndex];
	ExpandAncestors(*Target);
	HierarchyList->SetSelection(Target);
	HierarchyList->RequestScrollIntoView(Target);
	FocusWidgetNode(*Target);
}

void SUnrealGitWidgetDiffView::EnsureLivePreviews()
{
	if (bLivePreviewsBuilt || !Session.IsValid() || Session->State != EUnrealGitWidgetDiffState::Ready || Session->GetImages() == nullptr)
	{
		return;
	}

	CreatePreviewWidget(Session->GetImages()->LeftBlueprint, EThumbnailPreviewSizeMode::MatchDesignerMode, LeftLiveWidget, LeftPreviewWidget, LeftDesignSize);
	CreatePreviewWidget(Session->GetImages()->RightBlueprint, EThumbnailPreviewSizeMode::MatchDesignerMode, RightLiveWidget, RightPreviewWidget, RightDesignSize);
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

	const TSharedPtr<FUnrealGitWidgetNode> Node = FindHierarchyNode(Name);
	if (Node.IsValid())
	{
		HierarchyList->SetSelection(Node);
		HierarchyList->RequestScrollIntoView(Node);
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

	if (Session.IsValid() && Session->GetImages() != nullptr)
	{
		LeftTex = Session->GetImages()->LeftTexture;
		RightTex = Session->GetImages()->RightTexture;
		DiffTex = Session->GetImages()->DiffTexture;
		HighlightTex = Session->GetImages()->HighlightTexture;
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
	*CanvasNodes = Session->GetResult().Hierarchy;

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
	for (const FUnrealGitWidgetNode& Node : Session->GetResult().Hierarchy)
	{
		Names.Add(Node.Name);
	}

	for (const FUnrealGitWidgetNode& Node : Session->GetResult().Hierarchy)
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

		const TArray<TSharedPtr<FUnrealGitWidgetNode>> SelectedItems = HierarchyList->GetSelectedItems();
		const TSharedPtr<FUnrealGitWidgetNode> Selected = SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr;
		RefreshInspector(Selected.IsValid() ? FindHierarchyNode(Selected->Name).Get() : nullptr);
	}
	else
	{
		RefreshInspector(nullptr);
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
		Color = GetChangeColor(Item->Change);
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
		for (const FUnrealGitWidgetNode& Node : Session->GetResult().Hierarchy)
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
		if (!Entry.IsValid())
		{
			continue;
		}
		if (Entry->Change == EUnrealGitWidgetChange::Added && !bShowAdded)
		{
			continue;
		}
		if (Entry->Change == EUnrealGitWidgetChange::Removed && !bShowRemoved)
		{
			continue;
		}
		if (Entry->Change == EUnrealGitWidgetChange::Modified && !bShowModified)
		{
			continue;
		}
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
	RefreshInspector(Item.Get());
	if (LeftCanvas.IsValid()) LeftCanvas->Invalidate(EInvalidateWidget::Paint);
	if (RightCanvas.IsValid()) RightCanvas->Invalidate(EInvalidateWidget::Paint);
	if (DiffCanvas.IsValid()) DiffCanvas->Invalidate(EInvalidateWidget::Paint);
	Invalidate(EInvalidateWidget::Paint);

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

	const TSharedPtr<FUnrealGitWidgetNode> Node = FindHierarchyNode(Item->Name);
	if (Node.IsValid())
	{
		ExpandAncestors(*Node);
		HierarchyList->SetSelection(Node);
		HierarchyList->RequestScrollIntoView(Node);
		FocusWidgetNode(*Node);
	}
}

TSharedRef<ITableRow> SUnrealGitWidgetDiffView::OnGeneratePropertyRow(TSharedPtr<FDiffEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FLinearColor Color(0.6f, 0.6f, 0.6f, 1.0f);
	FString Glyph = TEXT("~");
	if (Item.IsValid())
	{
		Color = GetChangeBadgeColor(Item->Change);
		Glyph = GetChangeGlyph(Item->Change);
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
	if (!NewSelection.IsValid())
	{
		return;
	}
	CurrentModeOption = NewSelection;
	if (*NewSelection == TEXT("Side by Side"))
	{
		ViewMode = EViewMode::SideBySide;
	}
	else if (*NewSelection == TEXT("Difference"))
	{
		ViewMode = EViewMode::Difference;
	}
	else if (*NewSelection == TEXT("Onion"))
	{
		ViewMode = EViewMode::Onion;
	}
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
		return bSwapped ? Session->GetResult().Right.Label : Session->GetResult().Left.Label;
	}
	return FText::GetEmpty();
}

FText SUnrealGitWidgetDiffView::GetRightLabel() const
{
	if (Session.IsValid())
	{
		return bSwapped ? Session->GetResult().Left.Label : Session->GetResult().Right.Label;
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
