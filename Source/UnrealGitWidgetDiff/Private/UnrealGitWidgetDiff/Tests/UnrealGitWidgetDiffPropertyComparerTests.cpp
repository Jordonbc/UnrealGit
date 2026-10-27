// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "UObject/Package.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffPropertyComparer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealGitWidgetDiffPropertyComparerTest,
	"UnrealGit.WidgetDiff.PropertyComparer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitWidgetDiffPropertyComparerTest::RunTest(const FString&)
{
	UOverlay* LeftOverlay = NewObject<UOverlay>(GetTransientPackage(), NAME_None, RF_Transient);
	UOverlay* RightOverlay = NewObject<UOverlay>(GetTransientPackage(), NAME_None, RF_Transient);
	UImage* LeftImage = NewObject<UImage>(LeftOverlay, NAME_None, RF_Transient);
	UImage* RightImage = NewObject<UImage>(RightOverlay, NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Left image created"), LeftImage) || !TestNotNull(TEXT("Right image created"), RightImage))
	{
		return false;
	}

	LeftImage->SetVisibility(ESlateVisibility::Visible);
	RightImage->SetVisibility(ESlateVisibility::Collapsed);
	LeftImage->SetRenderOpacity(1.0f);
	RightImage->SetRenderOpacity(0.5f);
	LeftImage->SetColorAndOpacity(FLinearColor::White);
	RightImage->SetColorAndOpacity(FLinearColor::Red);

	UOverlaySlot* LeftSlot = LeftOverlay->AddChildToOverlay(LeftImage);
	UOverlaySlot* RightSlot = RightOverlay->AddChildToOverlay(RightImage);
	if (!TestNotNull(TEXT("Left slot created"), LeftSlot) || !TestNotNull(TEXT("Right slot created"), RightSlot))
	{
		return false;
	}

	LeftSlot->SetPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f));
	RightSlot->SetPadding(FMargin(12.0f, 4.0f, 4.0f, 4.0f));
	LeftSlot->SetHorizontalAlignment(HAlign_Fill);
	RightSlot->SetHorizontalAlignment(HAlign_Center);

	TArray<FUnrealGitPropertyDiff> Diffs;
	FUnrealGitWidgetDiffPropertyComparer::CompareWidgets(LeftImage, RightImage, Diffs);

	auto HasPath = [&Diffs](const TCHAR* Path)
	{
		return Diffs.ContainsByPredicate([Path](const FUnrealGitPropertyDiff& Diff)
		{
			return Diff.Path == Path;
		});
	};

	TestTrue(TEXT("Enum property difference reported"), HasPath(TEXT("Visibility")));
	TestTrue(TEXT("Float property difference reported"), HasPath(TEXT("RenderOpacity")));
	TestTrue(TEXT("Nested struct channel difference reported"), HasPath(TEXT("ColorAndOpacity.G")));
	TestTrue(TEXT("Slot struct difference reported"), HasPath(TEXT("Slot.Padding.Left")));
	TestTrue(TEXT("Slot enum difference reported"), HasPath(TEXT("Slot.HorizontalAlignment")));
	TestFalse(TEXT("Unchanged property excluded"), HasPath(TEXT("bIsEnabled")));

	const FUnrealGitPropertyDiff* PaddingLeft = Diffs.FindByPredicate([](const FUnrealGitPropertyDiff& Diff)
	{
		return Diff.Path == TEXT("Slot.Padding.Left");
	});
	if (TestNotNull(TEXT("Slot padding entry found"), PaddingLeft))
	{
		TestTrue(TEXT("Padding has distinct before/after"), PaddingLeft->Before != PaddingLeft->After);
		TestFalse(TEXT("Padding has a historical value"), PaddingLeft->Before.IsEmpty());
		TestFalse(TEXT("Padding has a current value"), PaddingLeft->After.IsEmpty());
		TestEqual(TEXT("Padding is categorised as a slot property"), PaddingLeft->Category, FString(TEXT("Slot")));
		TestTrue(TEXT("Padding is a modified property"), PaddingLeft->Change == EUnrealGitWidgetChange::Modified);
	}

	TArray<FUnrealGitPropertyDiff> NullDiffs;
	FUnrealGitWidgetDiffPropertyComparer::CompareWidgets(nullptr, RightImage, NullDiffs);
	TestEqual(TEXT("Null historical widget produces no differences"), NullDiffs.Num(), 0);

	TArray<FUnrealGitPropertyDiff> SameDiffs;
	FUnrealGitWidgetDiffPropertyComparer::CompareWidgets(LeftImage, LeftImage, SameDiffs);
	TestEqual(TEXT("Comparing a widget with itself produces no differences"), SameDiffs.Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
