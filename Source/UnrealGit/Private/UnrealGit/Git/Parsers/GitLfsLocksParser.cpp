// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

	bool FGitLfsLocksParser::ParseJson(const FString& StdOutText, TArray<FGitLfsLock>& OutLocks, FString& OutError)
{
	OutLocks.Reset();
	OutError.Reset();

	TSharedPtr<FJsonValue> RootValue;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StdOutText);
	if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
	{
		OutError = TEXT("Failed to parse git lfs locks JSON.");
		return false;
	}

	// `git lfs locks --json` returns a bare array; some versions wrap it as {"locks":[...]}.
	const TArray<TSharedPtr<FJsonValue>>* LocksArray = nullptr;
	if (RootValue->Type == EJson::Array)
	{
		LocksArray = &RootValue->AsArray();
	}
	else if (RootValue->Type == EJson::Object)
	{
		RootValue->AsObject()->TryGetArrayField(TEXT("locks"), LocksArray);
	}

	if (!LocksArray)
	{
		// An empty result can omit the array depending on git-lfs version.
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
		FString Path;
		FString Id;
		if (!LockObj->TryGetStringField(TEXT("path"), Path))
		{
			continue;
		}
		Lock.Path = MoveTemp(Path);
		LockObj->TryGetStringField(TEXT("id"), Id);
		Lock.Id = MoveTemp(Id);

		const TSharedPtr<FJsonObject>* OwnerObj = nullptr;
		if (LockObj->TryGetObjectField(TEXT("owner"), OwnerObj) && OwnerObj && OwnerObj->IsValid())
		{
			(*OwnerObj)->TryGetStringField(TEXT("name"), Lock.OwnerName);
			(*OwnerObj)->TryGetStringField(TEXT("email"), Lock.OwnerEmail);
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

