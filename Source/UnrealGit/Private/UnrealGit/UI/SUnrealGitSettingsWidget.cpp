// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "SUnrealGitSettingsWidget.h"

#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"
#include "UnrealGit/Git/GitEnvironmentInfo.h"

#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "DesktopPlatformModule.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SErrorText.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UnrealGitSettingsWidget"

void SUnrealGitSettingsWidget::Construct(const FArguments& InArgs)
{
	Provider = InArgs._Provider;

	bCreateGitIgnore = true;
	bCreateReadme = true;
	bCreateGitAttributes = false;
	bCreateInitialCommit = true;
	bInitializing = false;

	ChildSlot
	[
		SNew(SBox)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Git Executable
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GitExecutableLabel", "Git Executable"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(3.0f)
				[
					SAssignNew(GitPathBox, SEditableTextBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseEllipsis", "..."))
					.ToolTipText(LOCTEXT("BrowseGitExecutable", "Select Git Executable"))
					.OnClicked(this, &SUnrealGitSettingsWidget::OnBrowseGitPath)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SetGitPath", "Set"))
					.OnClicked(this, &SUnrealGitSettingsWidget::OnSetGitPath)
				]
			]

			// Repository status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SAssignNew(StatusGrid, SUniformGridPanel)
				.SlotPadding(FMargin(4.0f, 2.0f))
			]

			// Initialize project with Git
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SVerticalBox)
				.Visibility(this, &SUnrealGitSettingsWidget::CanInitializeRepository)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RepoNotFound", "Current project is not contained in a Git repository. Fill the form below to initialize a new repository."))
					.WrapTextAt(450.0f)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OriginUrlLabel", "URL of the remote server 'origin' (optional)"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(2.0f)
					[
						SAssignNew(OriginBox, SMultiLineEditableTextBox)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SCheckBox)
					.IsChecked(bCreateGitIgnore ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SUnrealGitSettingsWidget::OnCheckedGitIgnore)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateGitIgnore", "Add a .gitignore file"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SCheckBox)
					.IsChecked(bCreateReadme ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SUnrealGitSettingsWidget::OnCheckedReadme)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateReadme", "Add a basic README.md file"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SCheckBox)
					.IsChecked(bCreateGitAttributes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SUnrealGitSettingsWidget::OnCheckedGitAttributes)
					.IsEnabled_Lambda([this]() { return Provider && Provider->GetEnvironmentInfo().bGitLfsAvailable; })
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateGitAttributes", "Add a .gitattributes file to enable Git LFS"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SCheckBox)
					.IsChecked(bCreateInitialCommit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SUnrealGitSettingsWidget::OnCheckedInitialCommit)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InitialCommit", "Make the initial Git Commit"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("InitializeGit", "Initialize project with Git"))
					.HAlign(HAlign_Center)
					.ContentPadding(6.0f)
					.IsEnabled_Lambda([this]() { return !bInitializing; })
					.OnClicked(this, &SUnrealGitSettingsWidget::OnInitialize)
				]
			]

			// Error text
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SAssignNew(ErrorText, SErrorText)
			]
		]
	];

	RefreshStatus();
}

void SUnrealGitSettingsWidget::RefreshStatus()
{
	if (GitPathBox.IsValid())
	{
		GitPathBox->SetText(FText::FromString(Provider->GetGitExecutablePath()));
	}

	if (StatusGrid.IsValid())
	{
		StatusGrid->ClearChildren();

		FGitEnvironmentInfo Env = Provider->GetEnvironmentInfo();

		int32 Row = 0;
		auto AddRow = [this, &Row](const FText& Label, const FText& Value)
		{
			StatusGrid->AddSlot(0, Row)
			[
				SNew(STextBlock).Text(Label)
			];
			StatusGrid->AddSlot(1, Row)
			[
				SNew(STextBlock).Text(Value)
			];
			++Row;
		};

		const FString Root = Provider->GetRepositoryRoot();
		const FText RootText = Root.IsEmpty()
			? LOCTEXT("NotAGitRepository", "Not a git repository")
			: FText::FromString(Root);

		AddRow(LOCTEXT("RepositoryRootLabel", "Repository Root"), RootText);
		AddRow(LOCTEXT("BranchLabel", "Branch"), StatusValue(Provider->GetBranchName()));
		AddRow(LOCTEXT("RemoteLabel", "Remote"), StatusValue(Provider->GetRemoteUrl()));
		AddRow(LOCTEXT("UserNameLabel", "User Name"), StatusValue(Env.UserName));
		AddRow(LOCTEXT("UserEmailLabel", "E-Mail"), StatusValue(Env.UserEmail));
		AddRow(LOCTEXT("GitVersionLabel", "Git Version"), StatusValue(Env.GitVersion));
		AddRow(LOCTEXT("GitLfsVersionLabel", "Git LFS Version"), StatusValue(Env.GitLfsVersion));
	}

	if (ErrorText.IsValid())
	{
		ErrorText->SetError(Provider->GetLastError());
	}
}

FReply SUnrealGitSettingsWidget::OnSetGitPath()
{
	FText OutError;
	Provider->SetGitExecutableOverride(GitPathBox.IsValid() ? GitPathBox->GetText().ToString() : FString(), OutError);

	if (ErrorText.IsValid())
	{
		ErrorText->SetError(OutError);
	}

	RefreshStatus();
	return FReply::Handled();
}

FReply SUnrealGitSettingsWidget::OnBrowseGitPath()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != nullptr && GitPathBox.IsValid())
	{
#if PLATFORM_WINDOWS
		const FString FileFilter = TEXT("Git executable|git.exe;git");
#else
		const FString FileFilter = TEXT("git");
#endif
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			nullptr,
			TEXT("Select Git Executable"),
			FString(),
			FString(),
			FileFilter,
			0,
			OutFiles);

		if (bOpened && OutFiles.Num() > 0)
		{
			GitPathBox->SetText(FText::FromString(OutFiles[0]));
		}
	}
	return FReply::Handled();
}

FReply SUnrealGitSettingsWidget::OnInitialize()
{
	bInitializing = true;

	const FString OriginUrl = OriginBox.IsValid() ? OriginBox->GetText().ToString() : FString();

	const FString ProjectDir = FPaths::ProjectDir();

	if (bCreateGitIgnore)
	{
		const FString GitIgnoreContent = TEXT("Binaries/\nDerivedDataCache/\nIntermediate/\nSaved/\n.vs/\n*.VC.db\n*.opensdf\n*.opendb\n*.sdf\n*.sln\n*.suo\n*.xcodeproj/\n*.xcworkspace/\n");
		FFileHelper::SaveStringToFile(GitIgnoreContent, *FPaths::Combine(ProjectDir, TEXT(".gitignore")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	if (bCreateReadme)
	{
		const FString ReadmeContent = FString::Printf(TEXT("# %s\n\nDeveloped with Unreal Engine 5\n"), FApp::GetProjectName());
		FFileHelper::SaveStringToFile(ReadmeContent, *FPaths::Combine(ProjectDir, TEXT("README.md")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	if (bCreateGitAttributes)
	{
		const FString GitAttributesContent = TEXT("Content/** filter=lfs diff=lfs merge=lfs -text\n");
		FFileHelper::SaveStringToFile(GitAttributesContent, *FPaths::Combine(ProjectDir, TEXT(".gitattributes")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FText OutError;
	const bool bSuccess = Provider->InitializeRepository(OriginUrl, bCreateGitIgnore, bCreateReadme, bCreateGitAttributes, bCreateInitialCommit, OutError);

	bInitializing = false;

	FNotificationInfo Info(bSuccess
		? LOCTEXT("InitSuccess", "Repository initialized successfully")
		: (OutError.IsEmpty() ? LOCTEXT("InitFailed", "Failed to initialize repository") : OutError));
	Info.bUseSuccessFailIcons = true;
	if (bSuccess)
	{
		Info.Image = FAppStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
	}
	FSlateNotificationManager::Get().AddNotification(Info);

	if (!OutError.IsEmpty() && ErrorText.IsValid())
	{
		ErrorText->SetError(OutError);
	}

	RefreshStatus();
	return FReply::Handled();
}

void SUnrealGitSettingsWidget::OnCheckedGitIgnore(ECheckBoxState NewCheckedState)
{
	bCreateGitIgnore = (NewCheckedState == ECheckBoxState::Checked);
}

void SUnrealGitSettingsWidget::OnCheckedReadme(ECheckBoxState NewCheckedState)
{
	bCreateReadme = (NewCheckedState == ECheckBoxState::Checked);
}

void SUnrealGitSettingsWidget::OnCheckedGitAttributes(ECheckBoxState NewCheckedState)
{
	bCreateGitAttributes = (NewCheckedState == ECheckBoxState::Checked);
}

void SUnrealGitSettingsWidget::OnCheckedInitialCommit(ECheckBoxState NewCheckedState)
{
	bCreateInitialCommit = (NewCheckedState == ECheckBoxState::Checked);
}

EVisibility SUnrealGitSettingsWidget::CanInitializeRepository() const
{
	return (Provider != nullptr && Provider->IsGitToolAvailable() && !Provider->HasGitRepository())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FText SUnrealGitSettingsWidget::StatusValue(const FString& Value) const
{
	if (Value.IsEmpty())
	{
		return LOCTEXT("NotSet", "not set");
	}
	return FText::FromString(Value);
}

#undef LOCTEXT_NAMESPACE