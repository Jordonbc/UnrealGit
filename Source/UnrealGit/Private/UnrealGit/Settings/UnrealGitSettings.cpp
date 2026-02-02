// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Settings/UnrealGitProjectSettings.h"
#include "UnrealGit/Settings/UnrealGitUserSettings.h"

FName UUnrealGitProjectSettings::GetCategoryName() const
{
	return "SourceControl";
}

FName UUnrealGitProjectSettings::GetContainerName() const
{
	return "Project";
}

FName UUnrealGitUserSettings::GetCategoryName() const
{
	return "SourceControl";
}

FName UUnrealGitUserSettings::GetContainerName() const
{
	return "Editor";
}

