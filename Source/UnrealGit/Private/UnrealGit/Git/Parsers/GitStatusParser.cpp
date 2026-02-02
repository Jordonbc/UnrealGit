// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitStatusParser.h"

#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

namespace UnrealGit::Parsing
{
	class FNullSeparatedReader final
	{
	public:
		explicit FNullSeparatedReader(const TArray<uint8>& InData)
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

			OutToken = MakeArrayView(Data.GetData() + Start, End - Start);
			Offset = (End < Data.Num()) ? (End + 1) : End;
			return true;
		}

	private:
		const TArray<uint8>& Data;
		int32 Offset = 0;
	};

	static bool ReadFieldAt(const uint8* Data, int32 Length, int32& InOutOffset, FString& OutField)
	{
		while (InOutOffset < Length && Data[InOutOffset] == ' ')
		{
			++InOutOffset;
		}

		if (InOutOffset >= Length)
		{
			return false;
		}

		const int32 Start = InOutOffset;
		while (InOutOffset < Length && Data[InOutOffset] != ' ')
		{
			++InOutOffset;
		}

		OutField = Utf8BytesToString(Data + Start, InOutOffset - Start);
		return true;
	}

	static FString ReadRemainder(const uint8* Data, int32 Length, int32 Offset)
	{
		while (Offset < Length && Data[Offset] == ' ')
		{
			++Offset;
		}
		return Utf8BytesToString(Data + Offset, Length - Offset);
	}

	static EGitFileState MapStateFromXY(const TCHAR IndexStatus, const TCHAR WorkTreeStatus, const bool bIsUnmerged)
	{
		if (bIsUnmerged)
		{
			return EGitFileState::Conflicted;
		}

		const auto IsDeleted = [](TCHAR C) { return C == 'D'; };
		const auto IsAdded = [](TCHAR C) { return C == 'A'; };
		const auto IsRenamed = [](TCHAR C) { return C == 'R' || C == 'C'; };
		const auto IsModified = [](TCHAR C) { return C == 'M' || C == 'T'; };

		if (IsRenamed(IndexStatus) || IsRenamed(WorkTreeStatus))
		{
			return EGitFileState::Renamed;
		}
		if (IsDeleted(IndexStatus) || IsDeleted(WorkTreeStatus))
		{
			return EGitFileState::Deleted;
		}
		if (IsAdded(IndexStatus) || IsAdded(WorkTreeStatus))
		{
			return EGitFileState::Added;
		}
		if (IsModified(IndexStatus) || IsModified(WorkTreeStatus))
		{
			return EGitFileState::Modified;
		}

		if (IndexStatus == '.' && WorkTreeStatus == '.')
		{
			return EGitFileState::Unchanged;
		}

		return EGitFileState::Unknown;
	}
}

bool FGitStatusParser::ParsePorcelainV2Z(const TArray<uint8>& StdOut, FGitStatusSnapshot& OutSnapshot, FString& OutError)
{
	OutSnapshot = FGitStatusSnapshot();
	OutError.Reset();

	UnrealGit::Parsing::FNullSeparatedReader Reader(StdOut);
	TArrayView<const uint8> Token;

	while (Reader.ReadNext(Token))
	{
		if (Token.Num() == 0)
		{
			continue;
		}

		const uint8 First = Token[0];
		const uint8* Data = Token.GetData();
		const int32 Length = Token.Num();

		if (First == '#')
		{
			// Example records:
			// "# branch.head main"
			// "# branch.ab +1 -2"
			int32 Offset = 1;
			FString Key;
			if (!UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, Key))
			{
				continue;
			}

			if (Key == TEXT("branch.head"))
			{
				OutSnapshot.Branch.Head = UnrealGit::Parsing::ReadRemainder(Data, Length, Offset);
			}
			else if (Key == TEXT("branch.ab"))
			{
				FString AheadToken;
				FString BehindToken;
				if (UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, AheadToken) && UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, BehindToken))
				{
					AheadToken.RemoveFromStart(TEXT("+"));
					BehindToken.RemoveFromStart(TEXT("-"));

					int32 Ahead = 0;
					int32 Behind = 0;
					if (LexTryParseString(Ahead, *AheadToken))
					{
						OutSnapshot.Branch.Ahead = Ahead;
					}
					if (LexTryParseString(Behind, *BehindToken))
					{
						OutSnapshot.Branch.Behind = Behind;
					}
				}
			}
			continue;
		}

		FGitFileStatus Status;

		if (First == '?' || First == '!')
		{
			// "? <path>" or "! <path>"
			const int32 PathOffset = 2;
			Status.RelativePath = UnrealGit::Parsing::ReadRemainder(Data, Length, PathOffset);
			Status.bIsTracked = false;
			Status.State = (First == '?') ? EGitFileState::Untracked : EGitFileState::Ignored;
			OutSnapshot.Files.Add(MoveTemp(Status));
			continue;
		}

		if (First != '1' && First != '2' && First != 'u')
		{
			continue;
		}

		int32 Offset = 0;
		FString RecordType;
		FString XY;
		FString IgnoredField;

		if (!UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, RecordType) || !UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, XY))
		{
			continue;
		}

		const TCHAR IndexStatus = XY.Len() >= 1 ? XY[0] : '.';
		const TCHAR WorkTreeStatus = XY.Len() >= 2 ? XY[1] : '.';

		Status.bIsTracked = true;
		Status.bIsStaged = (IndexStatus != '.');
		Status.bIsUnstaged = (WorkTreeStatus != '.');
		Status.bIsConflicted = (First == 'u') || (IndexStatus == 'U') || (WorkTreeStatus == 'U');

		if (First == '1')
		{
			// 1 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <path>
			for (int32 FieldIndex = 0; FieldIndex < 6; ++FieldIndex)
			{
				UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, IgnoredField);
			}
			Status.RelativePath = UnrealGit::Parsing::ReadRemainder(Data, Length, Offset);
			Status.State = UnrealGit::Parsing::MapStateFromXY(IndexStatus, WorkTreeStatus, Status.bIsConflicted);
			OutSnapshot.Files.Add(MoveTemp(Status));
			continue;
		}

		if (First == 'u')
		{
			// u <XY> <sub> <m1> <m2> <m3> <mW> <h1> <h2> <h3> <path>
			for (int32 FieldIndex = 0; FieldIndex < 9; ++FieldIndex)
			{
				UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, IgnoredField);
			}
			Status.RelativePath = UnrealGit::Parsing::ReadRemainder(Data, Length, Offset);
			Status.State = EGitFileState::Conflicted;
			OutSnapshot.Files.Add(MoveTemp(Status));
			continue;
		}

		// 2 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <score> <path>\0<orig_path>\0
		for (int32 FieldIndex = 0; FieldIndex < 7; ++FieldIndex)
		{
			UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, IgnoredField);
		}
		UnrealGit::Parsing::ReadFieldAt(Data, Length, Offset, IgnoredField); // score

		Status.RelativePath = UnrealGit::Parsing::ReadRemainder(Data, Length, Offset);
		Status.State = EGitFileState::Renamed;

		TArrayView<const uint8> OrigToken;
		if (Reader.ReadNext(OrigToken))
		{
			Status.OriginalPath = UnrealGit::Parsing::Utf8BytesToString(OrigToken.GetData(), OrigToken.Num());
		}

		OutSnapshot.Files.Add(MoveTemp(Status));
	}

	return true;
}

