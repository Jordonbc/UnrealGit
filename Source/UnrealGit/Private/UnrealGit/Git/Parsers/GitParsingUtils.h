// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UnrealGit::Parsing
{
	/**
	 * Converts a UTF-8 byte buffer into an FString.
	 *
	 * @param Data Pointer to the UTF-8 bytes (may be null).
	 * @param Length Number of bytes to convert.
	 * @return The decoded string, or empty when Data is null or Length is non-positive.
	 */
	inline FString Utf8BytesToString(const uint8* Data, int32 Length)
	{
		if (Data == nullptr || Length <= 0)
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data), Length);
		return FString(Converter.Length(), Converter.Get());
	}

	/**
	 * Converts a UTF-8 byte array into an FString.
	 *
	 * @param Bytes UTF-8 bytes to convert.
	 * @return The decoded string, or empty when Bytes is empty.
	 */
	inline FString Utf8BytesToString(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() == 0)
		{
			return FString();
		}
		return Utf8BytesToString(Bytes.GetData(), Bytes.Num());
	}

	/**
	 * Parses an ISO-8601 timestamp into a UTC FDateTime.
	 *
	 * @param IsoString ISO-8601 string (may include a timezone offset).
	 * @param OutUtc Receives the parsed UTC time on success.
	 * @return True when parsing succeeded.
	 */
	inline bool ParseIso8601Utc(const FString& IsoString, FDateTime& OutUtc)
	{
		// Git can emit ISO-8601 strings with timezone offsets; FDateTime::ParseIso8601 returns in UTC if the
		// string includes an offset.
		return FDateTime::ParseIso8601(*IsoString, OutUtc);
	}

	/**
	 * @class FNullSeparatedReader
	 * Sequential reader over a NUL-delimited byte buffer.
	 *
	 * Yields one token per call to ReadNext, splitting on NUL bytes; the final
	 * trailing segment (without a terminating NUL) is also returned.
	 */
	class FNullSeparatedReader final
	{
	public:
		/** Constructs a reader over the given byte array. */
		explicit FNullSeparatedReader(const TArray<uint8>& InData)
			: Data(MakeArrayView(InData.GetData(), InData.Num()))
		{
		}

		/** Constructs a reader over the given byte view. */
		explicit FNullSeparatedReader(TArrayView<const uint8> InData)
			: Data(InData)
		{
		}

		/**
		 * Reads the next NUL-delimited token.
		 *
		 * @param OutToken Receives the next token slice (may be empty for empty segments, which are skipped).
		 * @return True when a token was read; false when the end of the buffer has been reached.
		 */
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