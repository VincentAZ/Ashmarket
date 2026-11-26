// Ashmarket: the town outside the cage.
//
// What the town knows of the player and does about it: the marks, what is asked of them and what changes them, and who
// is in a place because of them. The rest of ATownGameMode is in TownGameMode.cpp.

#include "TownGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutSound.h"

DEFINE_LOG_CATEGORY_STATIC(LogTownStory, Log, All);

namespace TownStoryWords
{
	/** A mark that is one of the powers' standing begins with this */
	const TCHAR* const Standing = TEXT("st.");

	/** A mark that is how many of a good are in the bag begins with this */
	const TCHAR* const Bag = TEXT("bag.");

	/** In what is done: walks whoever is talked to along a way of theirs */
	const TCHAR* const Walk = TEXT("walk:");

	/** In what is done: makes a sound of a kind, so many times ("sound:gate_bell*3"), this long apart */
	const TCHAR* const Sound = TEXT("sound:");
	const float SoundApart = 1.9f;

	/** The ways somebody comes into a place and goes out of it when the marks say they are there, or are not */
	const FName WayIn("in");
	const FName WayOut("out");

	/** Splits a term at its sign: the mark before it, the figure after. False if it has none of the signs given. */
	bool Split(const FString& Term, const TArray<const TCHAR*>& Signs, FString& OutKey, FString& OutSign, FString& OutFigure)
	{
		for (const TCHAR* Sign : Signs)
		{
			if (int32 At = Term.Find(Sign, ESearchCase::CaseSensitive, ESearchDir::FromStart, 1); At > 0)
			{
				OutKey = Term.Left(At);
				OutSign = Sign;
				OutFigure = Term.Mid(At + FCString::Strlen(Sign));
				return true;
			}
		}

		return false;
	}
}

int32 ATownGameMode::GetMark(const FName& Key) const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return 0;
	}

	const FString Named = Key.ToString();

	if (Named == TEXT("water"))
	{
		const float* Held = GetMarketWater();

		return Held ? FMath::FloorToInt(FMath::Max(*Held, 0.0f) + 0.01f) : 0;
	}

	// how many of those behind the player are theirs to sell
	if (Named == TEXT("tow"))
	{
		return GetTowToSell();
	}

	if (Named == TEXT("day"))
	{
		return GameInstance->Stable.Day;
	}

	if (Named == TEXT("owns"))
	{
		return GameInstance->Stable.Fighters.Num();
	}

	if (Named == TEXT("tow"))
	{
		return Tow.Num();
	}

	if (Named == TEXT("name"))
	{
		return GameInstance->Ladder.Name;
	}

	if (Named == TEXT("fighter"))
	{
		return GameInstance->bLadderStarted ? 1 : 0;
	}

	if (Named.StartsWith(TownStoryWords::Bag))
	{
		return GameInstance->GetBagCount(FName(*Named.Mid(FCString::Strlen(TownStoryWords::Bag))));
	}

	if (const int32* Kept = GameInstance->Story.Marks.Find(Key))
	{
		return *Kept;
	}

	if (const int32* Begins = StoryData ? StoryData->Starts.Find(Key) : nullptr)
	{
		return *Begins;
	}

	return 0;
}

void ATownGameMode::SetMark(const FName& Key, int32 Value)
{
	if (UGutGameInstance* GameInstance = GetGutGameInstance())
	{
		GameInstance->Story.Marks.Add(Key, Value);
	}
}

bool ATownGameMode::Holds(const FString& When, FString* OutLacking) const
{
	TArray<FString> Terms;
	When.ParseIntoArrayWS(Terms);

	for (const FString& Term : Terms)
	{
		TArray<FString> Either;
		Term.ParseIntoArray(Either, TEXT("|"));

		bool bAny = false;

		for (const FString& One : Either)
		{
			FString Key, Sign, Figure;

			if (One.StartsWith(TEXT("!")))
			{
				bAny = GetMark(FName(*One.Mid(1))) == 0;
			}
			else if (TownStoryWords::Split(One, { TEXT(">="), TEXT("<="), TEXT("!="), TEXT("="), TEXT(">"), TEXT("<") }, Key, Sign, Figure))
			{
				const int32 Stands = GetMark(FName(*Key));
				const int32 Asked = FCString::Atoi(*Figure);

				bAny = Sign == TEXT(">=") ? Stands >= Asked : Sign == TEXT("<=") ? Stands <= Asked : Sign == TEXT("!=") ? Stands != Asked
					: Sign == TEXT("=") ? Stands == Asked : Sign == TEXT(">") ? Stands > Asked : Stands < Asked;
			}
			else
			{
				bAny = GetMark(FName(*One)) != 0;
			}

			if (bAny)
			{
				break;
			}
		}

		if (!bAny)
		{
			if (OutLacking)
			{
				*OutLacking = Term;
			}

			return false;
		}
	}

	return true;
}

FString ATownGameMode::GetMarkName(const FName& Key) const
{
	const FString Named = Key.ToString();

	if (Named.StartsWith(TownStoryWords::Standing))
	{
		const FName Power(*Named.Mid(FCString::Strlen(TownStoryWords::Standing)));

		if (const FTownFaction* Faction = StoryData ? StoryData->Factions.FindByPredicate([&Power](const FTownFaction& One) { return One.Key == Power; }) : nullptr)
		{
			return Faction->Name;
		}

		return Power.ToString();
	}

	if (Named == TEXT("trust") || Named == TEXT("fear") || Named == TEXT("heat"))
	{
		return Named.Left(1).ToUpper() + Named.Mid(1);
	}

	return FString();
}

FString ATownGameMode::Apply(const FString& Does)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || Does.IsEmpty())
	{
		return FString();
	}

	TArray<FString> Terms;
	Does.ParseIntoArrayWS(Terms);

	TArray<FString> Told;

	for (const FString& Whole : Terms)
	{
		// something asked in brackets before a term: it is done only if that holds, as things stand now
		FString Term = Whole;

		if (FString Asked, Rest; Whole.StartsWith(TEXT("(")) && Whole.Mid(1).Split(TEXT(")"), &Asked, &Rest))
		{
			if (!Holds(Asked))
			{
				continue;
			}

			Term = Rest;
		}

		if (Term.StartsWith(TownStoryWords::Walk))
		{
			// whoever is being talked to goes off along a way of theirs, and is gone at its end if the marks no longer have them here
			if (!WalkPerson(TalkingTo, FName(*Term.Mid(FCString::Strlen(TownStoryWords::Walk))), false))
			{
				UE_LOG(LogTownStory, Warning, TEXT("Story: \"%s\": nobody is being talked to who has that way"), *Term);
			}

			continue;
		}

		if (Term.StartsWith(TownStoryWords::Sound))
		{
			FString Kind = Term.Mid(FCString::Strlen(TownStoryWords::Sound));
			FString Times;

			if (Kind.Split(TEXT("*"), &Kind, &Times))
			{
				Kind.TrimStartAndEndInline();
			}

			for (int32 i = 0; i < FMath::Clamp(Times.IsEmpty() ? 1 : FCString::Atoi(*Times), 1, 12); ++i)
			{
				SoundsDue.Emplace(FName(*Kind), GetWorld()->GetTimeSeconds() + 0.6f + (i * TownStoryWords::SoundApart));
			}

			continue;
		}

		FString Key, Sign, Figure;

		if (!TownStoryWords::Split(Term, { TEXT("="), TEXT("+"), TEXT("-") }, Key, Sign, Figure))
		{
			UE_LOG(LogTownStory, Warning, TEXT("Story: \"%s\" does nothing: it has no =, + or -"), *Term);
			continue;
		}

		// a throw of a die: "=?4" is one to four
		int32 By = Figure.StartsWith(TEXT("?")) ? FMath::RandRange(1, FMath::Max(1, FCString::Atoi(*Figure.Mid(1)))) : FCString::Atoi(*Figure);

		if (Figure.StartsWith(TEXT("?")) && Thrown.IsSet())
		{
			By = Thrown.GetValue();
		}

		const FName Mark(*Key);

		// water is what the player holds, and is never taken below nothing
		if (Key == TEXT("water"))
		{
			float* Held = GetMarketWater();

			if (!Held || Sign == TEXT("="))
			{
				UE_LOG(LogTownStory, Warning, TEXT("Story: \"%s\": there is no water to change, or it was to be set outright"), *Term);
				continue;
			}

			const float Change = Sign == TEXT("+") ? By : -FMath::Min(static_cast<float>(By), FMath::Max(*Held, 0.0f));

			*Held += Change;

			// the three ways of holding water are one purse
			GameInstance->ShareWater();

			if (!FMath::IsNearlyZero(Change))
			{
				Told.Add(Change > 0.0f ? FString::Printf(TEXT("%.0f L paid to you: you hold %.0f"), Change, FMath::Max(*Held, 0.0f)) : FString::Printf(TEXT("%.0f L paid: you hold %.0f"), -Change, FMath::Max(*Held, 0.0f)));
			}

			continue;
		}

		if (Key.StartsWith(TownStoryWords::Bag))
		{
			const FName Good(*Key.Mid(FCString::Strlen(TownStoryWords::Bag)));
			const int32 Had = GameInstance->GetBagCount(Good);
			const int32 Has = FMath::Max(0, Sign == TEXT("=") ? By : Sign == TEXT("+") ? Had + By : Had - By);

			GameInstance->AddToBag(Good, Has - Had);

			if (Has != Had)
			{
				Told.Add(FString::Printf(TEXT("%s: %s your bag"), *GetGoodName(Good), Has > Had ? TEXT("in") : TEXT("out of")));
			}

			continue;
		}

		const int32 Stood = GetMark(Mark);

		int32 Stands = Sign == TEXT("=") ? By : Sign == TEXT("+") ? Stood + By : Stood - By;

		if (Key.StartsWith(TownStoryWords::Standing))
		{
			Stands = FMath::Clamp(Stands, -100, 100);
		}
		else if (Key == TEXT("trust") || Key == TEXT("fear"))
		{
			Stands = FMath::Clamp(Stands, 0, 100);
		}
		else if (Key == TEXT("heat"))
		{
			Stands = FMath::Clamp(Stands, 0, 5);
		}

		GameInstance->Story.Marks.Add(Mark, Stands);

		if (const FString Called = GetMarkName(Mark); !Called.IsEmpty() && Stands != Stood)
		{
			Told.Add(FString::Printf(TEXT("%s %s%d"), *Called, Stands > Stood ? TEXT("+") : TEXT("−"), FMath::Abs(Stands - Stood)));
		}

		UE_LOG(LogTownStory, Log, TEXT("Story: %s was %d, is %d"), *Key, Stood, Stands);
	}

	// who is in the place may have changed with them
	RefreshPresent();

	return FString::Join(Told, TEXT("  ·  "));
}

void ATownGameMode::UpdateSoundsDue()
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 i = SoundsDue.Num() - 1; i >= 0; --i)
	{
		if (SoundsDue[i].Value <= Now)
		{
			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(SoundsDue[i].Key, GetWalker() ? GetWalker()->GetActorLocation() : FVector::ZeroVector);
			}

			UE_LOG(LogTownStory, Log, TEXT("Story: sounded: %s"), *SoundsDue[i].Key.ToString());

			SoundsDue.RemoveAt(i);
		}
	}
}

void ATownGameMode::WriteLog(const FString& Words)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || Words.IsEmpty())
	{
		return;
	}

	GameInstance->Story.Journal.Add(GameInstance->Stable.Day > 0 ? FString::Printf(TEXT("Day %d. %s"), GameInstance->Stable.Day, *Words) : Words);

	UE_LOG(LogTownStory, Log, TEXT("Story: written down: %s"), *Words);
}

FString ATownGameMode::GetStandingWord(int32 Standing)
{
	return Standing <= -50 ? TEXT("hunted by them") : Standing <= -20 ? TEXT("cold: no work, and prices up") : Standing < 10 ? TEXT("nobody to them") : Standing < 20 ? TEXT("known to them")
		: Standing < 40 ? TEXT("trusted with work") : TEXT("one of theirs");
}

void ATownGameMode::GetMissionTally(const FTownMission& Mission, TArray<FString>& OutWords, TArray<bool>& OutDone) const
{
	OutWords.Reset();
	OutDone.Reset();

	for (const FTownMissionTally& One : Mission.Tally)
	{
		if (!One.Count.IsNone() && One.Of > 0)
		{
			const int32 Got = FMath::Clamp(GetMark(One.Count), 0, One.Of);

			OutWords.Add(FString::Printf(TEXT("%s: %d of %d"), *One.Words, Got, One.Of));
			OutDone.Add(Got >= One.Of);
		}
		else
		{
			OutWords.Add(One.Words);
			OutDone.Add(Holds(One.When));
		}
	}
}

void ATownGameMode::GetWorkInHand(TArray<const FTownMission*>& OutWork) const
{
	OutWork.Reset();

	if (!StoryData)
	{
		return;
	}

	for (const FTownMission& Mission : StoryData->Missions)
	{
		if (const int32 Stands = GetMark(Mission.Key); Stands > 0 && Stands < TownStory::Ended)
		{
			OutWork.Insert(&Mission, 0);
		}
	}
}

int32 ATownGameMode::GetDay() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return (GameInstance && GameInstance->Stable.bStarted) ? GameInstance->Stable.Day : 0;
}

FString ATownGameMode::GetMissionStep(const FTownMission& Mission) const
{
	const int32 Stands = GetMark(Mission.Key);

	if (Stands <= 0)
	{
		return FString();
	}

	const FTownMissionStep* Shown = nullptr;

	for (const FTownMissionStep& Step : Mission.Steps)
	{
		if (Step.At <= Stands && (!Shown || Step.At >= Shown->At))
		{
			Shown = &Step;
		}
	}

	return Shown ? Shown->Words : FString();
}

bool ATownGameMode::IsPresent(int32 Person) const
{
	return PlaceData && PlaceData->People.IsValidIndex(Person) && Holds(PlaceData->People[Person].When);
}

bool ATownGameMode::WalkPerson(int32 Person, const FName& Way, bool bGoes)
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person) || !People.IsValidIndex(Person) || !People[Person])
	{
		return false;
	}

	const FTownPerson& Who = PlaceData->People[Person];
	const FTownWay* Along = Who.FindWay(Way);

	if (!Along || Along->Points.IsEmpty())
	{
		return false;
	}

	FTownLed Walk;
	Walk.Person = Person;
	Walk.Way = Along->Points;
	Walk.Facing = Along->Facing;
	Walk.bGoes = bGoes;

	// the way in begins where they were stood, and ends where the place has them stand
	if (Way == TownStoryWords::WayIn)
	{
		Walk.Way.RemoveAt(0);
		Walk.Way.Add(Who.Place);
		Walk.Facing = Who.Facing;
	}

	// they are walked, and not moved about by whatever moves a fighter
	People[Person]->GetCharacterMovement()->DisableMovement();

	Led.RemoveAll([Person](const FTownLed& One) { return One.Person == Person; });
	Led.Add(MoveTemp(Walk));

	UE_LOG(LogTownStory, Log, TEXT("Story: %s walks %s"), *Who.Name, *Way.ToString());

	return true;
}

void ATownGameMode::RefreshPresent()
{
	// not before everybody the place begins with has been stood: they are stood one to a frame
	if (!bSettled || !PlaceData)
	{
		return;
	}

	for (int32 i = 0; i < PlaceData->People.Num() && i < People.Num(); ++i)
	{
		const FTownPerson& Who = PlaceData->People[i];

		if (Who.When.IsEmpty())
		{
			continue;
		}

		const bool bHere = IsPresent(i);
		FTownLed* Walking = Led.FindByPredicate([i](const FTownLed& One) { return One.Person == i; });

		if (bHere && !People[i])
		{
			// come to be here while the player is: in by their way in, if they have one
			const FTownWay* In = Who.FindWay(TownStoryWords::WayIn);

			People[i] = StandPerson(i, (In && !In->Points.IsEmpty()) ? &In->Points[0] : nullptr);

			if (People[i] && In && In->Points.Num() > 0)
			{
				WalkPerson(i, TownStoryWords::WayIn, false);
			}

			// (seen whole from the first: somebody not yet ready is grey squares, or worse)
			UGutGameInstance::FinishPreparing(*Who.Name);

			UE_LOG(LogTownStory, Log, TEXT("Story: %s is in the place now"), *Who.Name);
		}
		else if (!bHere && People[i])
		{
			if (Walking && Walking->bGoes)
			{
				continue;
			}

			if (Walking)
			{
				// already on their way somewhere: they are gone when they get there
				Walking->bGoes = true;
			}
			else if (!WalkPerson(i, TownStoryWords::WayOut, true))
			{
				People[i]->Destroy();
				People[i] = nullptr;
			}

			UE_LOG(LogTownStory, Log, TEXT("Story: %s is in the place no longer"), *Who.Name);
		}
	}
}

TArray<int32> ATownGameMode::GetShownAnswers(const FTownLine& Line) const
{
	TArray<int32> Shown;

	for (int32 i = 0; i < Line.Answers.Num(); ++i)
	{
		if (Holds(Line.Answers[i].When))
		{
			Shown.Add(i);
		}
	}

	return Shown;
}

bool ATownGameMode::IsSwitch(const FTownLine& Line) const
{
	const TArray<int32> Shown = GetShownAnswers(Line);

	return !Shown.IsEmpty() && !Shown.ContainsByPredicate([&Line](int32 Which) { return !Line.Answers[Which].Words.IsEmpty(); });
}

int32 ATownGameMode::GetOpeningLine(int32 Person) const
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person))
	{
		return 0;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[Person].Lines;

	// the last of the lines a talk can begin at whose time it is: what was added later is about what happened later
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		if (Lines[i].bOpens && Holds(Lines[i].When))
		{
			return i;
		}
	}

	return 0;
}

void ATownGameMode::DoTalkLine()
{
	if (!IsTalking() || (TalkingTo == DonePerson && TalkLine == DoneLine))
	{
		return;
	}

	DonePerson = TalkingTo;
	DoneLine = TalkLine;

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	if (!Lines.IsValidIndex(TalkLine) || (Lines[TalkLine].Does.IsEmpty() && Lines[TalkLine].Log.IsEmpty()))
	{
		return;
	}

	// copied: what it does may be the end of whoever says it
	const FString Does = Lines[TalkLine].Does;
	const FString Log = Lines[TalkLine].Log;

	const FString Came = Apply(Does);

	WriteLog(Log);

	if (!Came.IsEmpty())
	{
		TalkTold = TalkTold.IsEmpty() ? Came : TalkTold + TEXT("  ·  ") + Came;
		TalkToldLine = TalkLine;
	}

	if (UGutGameInstance* GameInstance = GetGutGameInstance())
	{
		GameInstance->SaveRun();
	}
}
