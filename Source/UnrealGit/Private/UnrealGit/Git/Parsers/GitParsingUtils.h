// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

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

	class FNullSeparatedReader final
	{
	public:
		explicit FNullSeparatedReader(const TArray<uint8>& InData)
			: Data(MakeArrayView(InData.GetData(), InData.Num()))
		{
		}

		explicit FNullSeparatedReader(TArrayView<const uint8> InData)
			: Data(InData)
		{
		}

		bool ReadNext(TArrayView<const uint8>& OutToken)
		{
			if (Offset >= Data.Num())
			{
				return false;
			}

			const int32 Start = Offset;
			int32 End = Start;
			while (End < Data.Num() && Data[End] != 0)
			{
				++End;
			}

			OutToken = Data.Slice(Start, End - Start);
			Offset = (End < Data.Num()) ? (End + 1) : End;
			return true;
		}

	private:
		TArrayView<const uint8> Data;
		int32 Offset = 0;
	};
}