// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

bool FGitLfsLocksParser::ParseJson(const FString& StdOutText, TArray<FGitLfsLock>& OutLocks, FString& OutError)
{
	OutLocks.Reset();
	OutError.Reset();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StdOutText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Failed to parse git lfs locks JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LocksArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("locks"), LocksArray) || LocksArray == nullptr)
	{
		// An empty result can omit the array or provide an empty one depending on git-lfs version.
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *LocksArray)
	{
		const TSharedPtr<FJsonObject> LockObj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!LockObj.IsValid())
		{
			continue;
		}

		FGitLfsLock Lock;
		Lock.Path = LockObj->GetStringField(TEXT("path"));
		Lock.Id = LockObj->GetStringField(TEXT("id"));

		const TSharedPtr<FJsonObject> OwnerObj = LockObj->GetObjectField(TEXT("owner"));
		if (OwnerObj.IsValid())
		{
			Lock.OwnerName = OwnerObj->GetStringField(TEXT("name"));
			Lock.OwnerEmail = OwnerObj->GetStringField(TEXT("email"));
		}

		FString LockedAt;
		if (LockObj->TryGetStringField(TEXT("locked_at"), LockedAt))
		{
			FDateTime ParsedUtc;
			if (UnrealGit::Parsing::ParseIso8601Utc(LockedAt, ParsedUtc))
			{
				Lock.LockedAtUtc = ParsedUtc;
			}
		}

		OutLocks.Add(MoveTemp(Lock));
	}

	return true;
}

