// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "UnrealGitWidgetDiffSession.generated.h"

class UTexture2D;

UCLASS(Transient)
class UUnrealGitWidgetDiffImages : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UTexture2D> LeftTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> RightTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> DiffTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> HighlightTexture = nullptr;
};

enum class EUnrealGitWidgetDiffState : uint8
{
	Loading,
	Ready,
	Error,
	Empty,
};

/**
 * Shared state for a single widget diff session. The tab UI binds to this.
 */
class FUnrealGitWidgetDiffSession : public TSharedFromThis<FUnrealGitWidgetDiffSession, ESPMode::ThreadSafe>
{
public:
	FString AssetFileName;
	FString LeftRevision;
	FString RightRevision;

	EUnrealGitWidgetDiffState State = EUnrealGitWidgetDiffState::Loading;
	FText StatusMessage;
	FUnrealGitWidgetDiffResult Result;
	TStrongObjectPtr<UUnrealGitWidgetDiffImages> Images;

	FSimpleMulticastDelegate OnUpdated;

	void SetReady(const FUnrealGitWidgetDiffResult& InResult, const TStrongObjectPtr<UUnrealGitWidgetDiffImages>& InImages);
	void SetError(const FText& InMessage);
};
