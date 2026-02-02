// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"

#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

namespace UnrealGit::HistoryParsing
{
	static bool ParseMetadataLine(const TArrayView<const uint8> LineBytes, FGitRevisionInfo& OutInfo)
	{
		// Fields: commit, author name, author email, author date, subject
		const TCHAR FieldSep = 0x001F;
		const FString Line = UnrealGit::Parsing::Utf8BytesToString(LineBytes.GetData(), LineBytes.Num());
		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, &FieldSep, false);
		if (Fields.Num() < 5)
		{
			return false;
		}

		OutInfo.CommitId = Fields[0];
		OutInfo.AuthorName = Fields[1];
		OutInfo.AuthorEmail = Fields[2];

		FDateTime ParsedUtc;
		if (UnrealGit::Parsing::ParseIso8601Utc(Fields[3], ParsedUtc))
		{
			OutInfo.AuthorDateUtc = ParsedUtc;
		}

		OutInfo.Subject = Fields[4];
		return true;
	}

	class FNullReader final
	{
	public:
		explicit FNullReader(TArrayView<const uint8> InData)
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

bool FGitFileHistoryParser::Parse(const TArray<uint8>& StdOut, const FString& InitialRepoRelativePath, TArray<FGitFileRevision>& OutHistory, FString& OutError)
{
	OutHistory.Reset();
	OutError.Reset();

	if (StdOut.Num() == 0)
	{
		return true;
	}

	const uint8 RecordSep = 0x1E;
	const uint8 Newline = '\n';

	FString CurrentPath = InitialRepoRelativePath;

	int32 Offset = 0;
	while (Offset < StdOut.Num())
	{
		// Find the next record separator.
		while (Offset < StdOut.Num() && StdOut[Offset] != RecordSep)
		{
			++Offset;
		}

		if (Offset >= StdOut.Num())
		{
			break;
		}

		// Record begins after RS.
		const int32 RecordStart = Offset + 1;
		int32 RecordEnd = RecordStart;
		while (RecordEnd < StdOut.Num() && StdOut[RecordEnd] != RecordSep)
		{
			++RecordEnd;
		}

		const TArrayView<const uint8> RecordView = MakeArrayView(StdOut.GetData() + RecordStart, RecordEnd - RecordStart);
		Offset = RecordEnd;

		int32 LineEnd = 0;
		while (LineEnd < RecordView.Num() && RecordView[LineEnd] != Newline)
		{
			++LineEnd;
		}

		const TArrayView<const uint8> MetaLine = RecordView.Slice(0, LineEnd);
		FGitRevisionInfo Info;
		if (!UnrealGit::HistoryParsing::ParseMetadataLine(MetaLine, Info))
		{
			OutError = TEXT("Failed to parse git log metadata record.");
			return false;
		}

		FGitFileRevision FileRevision;
		FileRevision.Revision = MoveTemp(Info);
		FileRevision.RepoRelativePathAtRevision = CurrentPath;

		OutHistory.Add(MoveTemp(FileRevision));

		// Process name-status to handle renames for subsequent older commits.
		if (LineEnd >= RecordView.Num())
		{
			continue;
		}

		// Skip newline.
		int32 NameStatusOffset = LineEnd + 1;
		if (NameStatusOffset >= RecordView.Num())
		{
			continue;
		}

		UnrealGit::HistoryParsing::FNullReader NullReader(RecordView.Slice(NameStatusOffset, RecordView.Num() - NameStatusOffset));
		TArrayView<const uint8> Token;

		while (NullReader.ReadNext(Token))
		{
			if (Token.Num() == 0)
			{
				continue;
			}

			const FString Line = UnrealGit::Parsing::Utf8BytesToString(Token.GetData(), Token.Num());
			FString StatusCode;
			FString PathA;
			if (!Line.Split(TEXT("\t"), &StatusCode, &PathA))
			{
				continue;
			}

			if (StatusCode.StartsWith(TEXT("R")) || StatusCode.StartsWith(TEXT("C")))
			{
				TArrayView<const uint8> TokenB;
				if (!NullReader.ReadNext(TokenB))
				{
					continue;
				}

				const FString PathB = UnrealGit::Parsing::Utf8BytesToString(TokenB.GetData(), TokenB.Num());
				if (PathB == CurrentPath)
				{
					CurrentPath = PathA;
				}
			}
		}
	}

	return true;
}

