// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FUnrealGitSourceControlProvider;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SErrorText;
class SUniformGridPanel;
class SCheckBox;
class SButton;

enum class ECheckBoxState : uint8;

class SUnrealGitSettingsWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealGitSettingsWidget) {}
		SLATE_ARGUMENT(FUnrealGitSourceControlProvider*, Provider)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshStatus();
	FReply OnSetGitPath();
	FReply OnBrowseGitPath();
	FReply OnInitialize();

	void OnCheckedGitIgnore(ECheckBoxState NewCheckedState);
	void OnCheckedReadme(ECheckBoxState NewCheckedState);
	void OnCheckedGitAttributes(ECheckBoxState NewCheckedState);
	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);

	EVisibility CanInitializeRepository() const;

	FText StatusValue(const FString& Value) const;

	FUnrealGitSourceControlProvider* Provider = nullptr;
	TSharedPtr<SEditableTextBox> GitPathBox;
	TSharedPtr<SMultiLineEditableTextBox> OriginBox;
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SUniformGridPanel> StatusGrid;

	bool bCreateGitIgnore = true;
	bool bCreateReadme = true;
	bool bCreateGitAttributes = false;
	bool bCreateInitialCommit = true;
	bool bInitializing = false;
};