// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"

void FUnrealGitWidgetDiffSession::SetReady(const FUnrealGitWidgetDiffResult& InResult, const TStrongObjectPtr<UUnrealGitWidgetDiffImages>& InImages)
{
	State = EUnrealGitWidgetDiffState::Ready;
	Result = InResult;
	Images = InImages;
	OnUpdated.Broadcast();
}

void FUnrealGitWidgetDiffSession::SetError(const FText& InMessage)
{
	State = EUnrealGitWidgetDiffState::Error;
	StatusMessage = InMessage;
	OnUpdated.Broadcast();
}
