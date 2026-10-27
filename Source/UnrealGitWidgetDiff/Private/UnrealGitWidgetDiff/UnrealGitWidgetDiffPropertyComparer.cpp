// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffPropertyComparer.h"

#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "UObject/UnrealType.h"

static bool ShouldSkipProperty(const FProperty* Property)
{
	if (!Property)
	{
		return true;
	}

	static const uint64 SkippedFlags =
		CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_TextExportTransient | CPF_Deprecated;
	return Property->HasAnyPropertyFlags(SkippedFlags);
}

static FString ExportValue(const FProperty* Property, const void* Value, const UObject* Owner)
{
	if (!Property || !Value)
	{
		return FString();
	}

	FString Text;
	Property->ExportTextItem_Direct(Text, Value, nullptr, const_cast<UObject*>(Owner), PPF_None, nullptr);
	Text.TrimStartAndEndInline();
	Text.ReplaceInline(TEXT("\r"), TEXT(" "));
	Text.ReplaceInline(TEXT("\n"), TEXT(" "));
	return Text;
}

static FString ResolveCategory(const FProperty* Property, bool bIsSlot)
{
	if (bIsSlot)
	{
		return TEXT("Slot");
	}

	const FString MetaCategory = Property->GetMetaData(FName(TEXT("Category")));
	if (!MetaCategory.IsEmpty())
	{
		return MetaCategory;
	}

	const UClass* OwnerClass = Cast<UClass>(Property->GetOwnerStruct());
	return OwnerClass ? OwnerClass->GetName() : FString();
}

static void AddDiff(
	TArray<FUnrealGitPropertyDiff>& OutDiffs,
	const FString& Path,
	const FString& Category,
	EUnrealGitWidgetChange Change,
	FString Before,
	FString After)
{
	if (Change == EUnrealGitWidgetChange::Modified && Before == After)
	{
		return;
	}

	FUnrealGitPropertyDiff& Diff = OutDiffs.AddDefaulted_GetRef();
	Diff.Path = Path;
	Diff.Category = Category;
	Diff.Change = Change;
	Diff.Before = MoveTemp(Before);
	Diff.After = MoveTemp(After);
}

static FString ExportSetCanonical(const FSetProperty* Property, const void* Value, const UObject* Owner)
{
	FScriptSetHelper Helper(Property, Value);
	TArray<FString> Parts;
	Parts.Reserve(Helper.Num());
	for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
	{
		if (!Helper.IsValidIndex(Index))
		{
			continue;
		}
		Parts.Add(ExportValue(Property->ElementProp, Helper.GetElementPtr(Index), Owner));
	}
	Parts.Sort();
	return FString::Printf(TEXT("{%s}"), *FString::Join(Parts, TEXT(", ")));
}

static FString ExportMapCanonical(const FMapProperty* Property, const void* Value, const UObject* Owner)
{
	FScriptMapHelper Helper(Property, Value);
	TArray<FString> Parts;
	Parts.Reserve(Helper.Num());
	for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
	{
		if (!Helper.IsValidIndex(Index))
		{
			continue;
		}
		Parts.Add(FString::Printf(TEXT("%s: %s"),
			*ExportValue(Property->KeyProp, Helper.GetKeyPtr(Index), Owner),
			*ExportValue(Property->ValueProp, Helper.GetValuePtr(Index), Owner)));
	}
	Parts.Sort();
	return FString::Printf(TEXT("{%s}"), *FString::Join(Parts, TEXT(", ")));
}

static void CompareValue(
	const FProperty* Property,
	const void* LeftValue,
	const void* RightValue,
	const UObject* LeftOwner,
	const UObject* RightOwner,
	const FString& Path,
	const FString& Category,
	TArray<FUnrealGitPropertyDiff>& OutDiffs)
{
	if (!Property || !LeftValue || !RightValue || Property->Identical(LeftValue, RightValue, 0))
	{
		return;
	}

	const int32 DiffCountBefore = OutDiffs.Num();

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct && StructProperty->Struct->PropertyLink)
		{
			for (TFieldIterator<FProperty> PropertyIt(StructProperty->Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Child = *PropertyIt;
				if (ShouldSkipProperty(Child))
				{
					continue;
				}

				CompareValue(
					Child,
					Child->ContainerPtrToValuePtr<void>(LeftValue),
					Child->ContainerPtrToValuePtr<void>(RightValue),
					LeftOwner,
					RightOwner,
					Path + TEXT(".") + Child->GetName(),
					Category,
					OutDiffs);
			}

			if (OutDiffs.Num() > DiffCountBefore)
			{
				return;
			}
		}
	}
	else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper LeftHelper(ArrayProperty, LeftValue);
		FScriptArrayHelper RightHelper(ArrayProperty, RightValue);
		const FProperty* Inner = ArrayProperty->Inner;
		const int32 MaxCount = FMath::Max(LeftHelper.Num(), RightHelper.Num());
		for (int32 Index = 0; Index < MaxCount; ++Index)
		{
			const FString ElementPath = FString::Printf(TEXT("%s[%d]"), *Path, Index);
			if (!LeftHelper.IsValidIndex(Index))
			{
				AddDiff(OutDiffs, ElementPath, Category, EUnrealGitWidgetChange::Added, FString(), ExportValue(Inner, RightHelper.GetRawPtr(Index), RightOwner));
			}
			else if (!RightHelper.IsValidIndex(Index))
			{
				AddDiff(OutDiffs, ElementPath, Category, EUnrealGitWidgetChange::Removed, ExportValue(Inner, LeftHelper.GetRawPtr(Index), LeftOwner), FString());
			}
			else
			{
				CompareValue(Inner, LeftHelper.GetRawPtr(Index), RightHelper.GetRawPtr(Index), LeftOwner, RightOwner, ElementPath, Category, OutDiffs);
			}
		}

		if (OutDiffs.Num() > DiffCountBefore)
		{
			return;
		}
	}
	else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		AddDiff(
			OutDiffs,
			Path,
			Category,
			EUnrealGitWidgetChange::Modified,
			ExportSetCanonical(SetProperty, LeftValue, LeftOwner),
			ExportSetCanonical(SetProperty, RightValue, RightOwner));
		return;
	}
	else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		AddDiff(
			OutDiffs,
			Path,
			Category,
			EUnrealGitWidgetChange::Modified,
			ExportMapCanonical(MapProperty, LeftValue, LeftOwner),
			ExportMapCanonical(MapProperty, RightValue, RightOwner));
		return;
	}

	AddDiff(
		OutDiffs,
		Path,
		Category,
		EUnrealGitWidgetChange::Modified,
		ExportValue(Property, LeftValue, LeftOwner),
		ExportValue(Property, RightValue, RightOwner));
}

static void CollectProperties(const UClass* Class, const UClass* RequiredOwnerBase, FName SkipName, TArray<const FProperty*>& OutProperties)
{
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		if (ShouldSkipProperty(Property))
		{
			continue;
		}

		const UClass* OwnerClass = Cast<UClass>(Property->GetOwnerStruct());
		if (!OwnerClass || !OwnerClass->IsChildOf(RequiredOwnerBase))
		{
			continue;
		}
		if (!SkipName.IsNone() && Property->GetFName() == SkipName)
		{
			continue;
		}

		OutProperties.Add(Property);
	}
}

static void CompareObjectProperties(
	const UObject* Left,
	const UObject* Right,
	const TArray<const FProperty*>& LeftProperties,
	const TArray<const FProperty*>& RightProperties,
	const FString& PathPrefix,
	bool bIsSlot,
	TArray<FUnrealGitPropertyDiff>& OutDiffs)
{
	TMap<FName, const FProperty*> RightByName;
	RightByName.Reserve(RightProperties.Num());
	for (const FProperty* Property : RightProperties)
	{
		RightByName.Add(Property->GetFName(), Property);
	}

	TSet<FName> LeftNames;
	LeftNames.Reserve(LeftProperties.Num());

	for (const FProperty* LeftProperty : LeftProperties)
	{
		LeftNames.Add(LeftProperty->GetFName());

		const FString Path = PathPrefix.IsEmpty()
			? LeftProperty->GetName()
			: PathPrefix + TEXT(".") + LeftProperty->GetName();
		const FString Category = ResolveCategory(LeftProperty, bIsSlot);
		const FProperty* RightProperty = RightByName.FindRef(LeftProperty->GetFName());

		if (!RightProperty)
		{
			AddDiff(
				OutDiffs,
				Path,
				Category,
				EUnrealGitWidgetChange::Removed,
				ExportValue(LeftProperty, LeftProperty->ContainerPtrToValuePtr<void>(Left), Left),
				FString());
			continue;
		}

		if (!LeftProperty->SameType(RightProperty))
		{
			AddDiff(
				OutDiffs,
				Path,
				Category,
				EUnrealGitWidgetChange::Modified,
				ExportValue(LeftProperty, LeftProperty->ContainerPtrToValuePtr<void>(Left), Left),
				ExportValue(RightProperty, RightProperty->ContainerPtrToValuePtr<void>(Right), Right));
			continue;
		}

		CompareValue(
			LeftProperty,
			LeftProperty->ContainerPtrToValuePtr<void>(Left),
			RightProperty->ContainerPtrToValuePtr<void>(Right),
			Left,
			Right,
			Path,
			Category,
			OutDiffs);
	}

	for (const FProperty* RightProperty : RightProperties)
	{
		if (LeftNames.Contains(RightProperty->GetFName()))
		{
			continue;
		}

		const FString Path = PathPrefix.IsEmpty()
			? RightProperty->GetName()
			: PathPrefix + TEXT(".") + RightProperty->GetName();
		AddDiff(
			OutDiffs,
			Path,
			ResolveCategory(RightProperty, bIsSlot),
			EUnrealGitWidgetChange::Added,
			FString(),
			ExportValue(RightProperty, RightProperty->ContainerPtrToValuePtr<void>(Right), Right));
	}
}

static void CompareSlots(const UWidget* Left, const UWidget* Right, TArray<FUnrealGitPropertyDiff>& OutDiffs)
{
	const UPanelSlot* LeftSlot = Left->Slot;
	const UPanelSlot* RightSlot = Right->Slot;

	if (!LeftSlot && !RightSlot)
	{
		return;
	}

	if (!LeftSlot || !RightSlot)
	{
		AddDiff(
			OutDiffs,
			TEXT("Slot"),
			TEXT("Slot"),
			LeftSlot ? EUnrealGitWidgetChange::Removed : EUnrealGitWidgetChange::Added,
			LeftSlot ? LeftSlot->GetClass()->GetName() : FString(),
			RightSlot ? RightSlot->GetClass()->GetName() : FString());
		return;
	}

	if (LeftSlot->GetClass() != RightSlot->GetClass())
	{
		AddDiff(
			OutDiffs,
			TEXT("Slot"),
			TEXT("Slot"),
			EUnrealGitWidgetChange::Modified,
			LeftSlot->GetClass()->GetName(),
			RightSlot->GetClass()->GetName());
		return;
	}

	TArray<const FProperty*> LeftProperties;
	TArray<const FProperty*> RightProperties;
	CollectProperties(LeftSlot->GetClass(), UPanelSlot::StaticClass(), NAME_None, LeftProperties);
	CollectProperties(RightSlot->GetClass(), UPanelSlot::StaticClass(), NAME_None, RightProperties);

	CompareObjectProperties(LeftSlot, RightSlot, LeftProperties, RightProperties, TEXT("Slot"), true, OutDiffs);
}

void FUnrealGitWidgetDiffPropertyComparer::CompareWidgets(const UWidget* Left, const UWidget* Right, TArray<FUnrealGitPropertyDiff>& OutDiffs)
{
	OutDiffs.Reset();

	if (!Left || !Right || Left == Right)
	{
		return;
	}

	TArray<const FProperty*> LeftProperties;
	TArray<const FProperty*> RightProperties;
	CollectProperties(Left->GetClass(), UWidget::StaticClass(), GET_MEMBER_NAME_CHECKED(UWidget, Slot), LeftProperties);
	CollectProperties(Right->GetClass(), UWidget::StaticClass(), GET_MEMBER_NAME_CHECKED(UWidget, Slot), RightProperties);

	CompareObjectProperties(Left, Right, LeftProperties, RightProperties, FString(), false, OutDiffs);
	CompareSlots(Left, Right, OutDiffs);
}
