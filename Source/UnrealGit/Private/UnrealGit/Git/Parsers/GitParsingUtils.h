// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UnrealGit::Parsing
{
	inline FString Utf8BytesToString(const uint8* Data, int32 Length)
	{
		if (Data == nullptr || Length <= 0)
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data), Length);
		return FString(Converter.Length(), Converter.Get());
	}

	inline FString Utf8BytesToString(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() == 0)
		{
			return FString();
		}
		return Utf8BytesToString(Bytes.GetData(), Bytes.Num());
	}

	inline bool ParseIso8601Utc(const FString& IsoString, FDateTime& OutUtc)
	{
		// Git can emit ISO-8601 strings with timezone offsets; FDateTime::ParseIso8601 returns in UTC if the
		// string includes an offset.
		return FDateTime::ParseIso8601(*IsoString, OutUtc);
	}
}
