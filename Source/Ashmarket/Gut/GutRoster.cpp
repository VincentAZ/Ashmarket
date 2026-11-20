// Ashmarket prototype: the Gut.
//
// Who is alive in the cage. Somebody who dies in it is dead whichever way the player came to see it, as a fighter,
// at the window or as an owner, and is not seen again: somebody new comes in for each of them, in their rank.
// And what each has won where the player could see it is counted, and the book reads the count.
// These are members of the cage's game mode, kept in a file of their own.

#include "GutGameMode.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutWeaponData.h"

namespace GutRosterConstants
{
	/** Names for whoever comes in after the dead. Cage names: short, and nobody's own. None is a name the Block gives. */
	const TCHAR* Names[] =
	{
		TEXT("Gravel"), TEXT("Tallow"), TEXT("Hinge"), TEXT("Cleat"), TEXT("Brine"), TEXT("Suet"), TEXT("Clinker"), TEXT("Rasp"),
		TEXT("Knuckle"), TEXT("Tinder"), TEXT("Sump"), TEXT("Grit"), TEXT("Scab"), TEXT("Welt"), TEXT("Cobble"), TEXT("Dross"),
		TEXT("Gristle"), TEXT("Mallet"), TEXT("Spoke"), TEXT("Rind"), TEXT("Pith"), TEXT("Canker"), TEXT("Clout"), TEXT("Shiv"),
		TEXT("Gaff"), TEXT("Tongs"), TEXT("Bung"), TEXT("Froth"), TEXT("Chaff"), TEXT("Stub"), TEXT("Offal"), TEXT("Ratchet"),
		TEXT("Kindling"), TEXT("Lug"), TEXT("Cinch"), TEXT("Barb"), TEXT("Scour"), TEXT("Thresh"), TEXT("Dregs"), TEXT("Callus"),
		TEXT("Burlap"), TEXT("Sprocket"), TEXT("Trowel"), TEXT("Gouge"), TEXT("Slake"), TEXT("Wick"), TEXT("Plumb"), TEXT("Flense")
	};

	/** Who they were before. One line, and nothing in it that says man or woman: it is given to either. */
	const TCHAR* Billings[] =
	{
		TEXT("Carried water for the Pump until a tank came up short."),
		TEXT("Came in off the Flats with a split lip and no tag."),
		TEXT("Fought dogs for a kennel man. The kennel man sold both."),
		TEXT("Dug the ground behind the pumps. Says the pay here is better."),
		TEXT("A cook off the grill carts. Knows what goes on the grill."),
		TEXT("Stood on the Block twice. Nobody kept it long."),
		TEXT("Walked out of the Kiln alone. Will not say who did not."),
		TEXT("Owes Mother Ledger. This is the paying."),
		TEXT("Stood guard on a convoy that lost its water."),
		TEXT("Picked scrap on the dam face. Has the hands for it."),
		TEXT("Ran dice on Main Street until somebody looked at the dice."),
		TEXT("Somebody's youngest. The eldest is on a wall."),
		TEXT("Mended pumps. Mended one wrong."),
		TEXT("Came to watch, bet, lost, and stayed."),
		TEXT("Sold by a crew that needed the water more."),
		TEXT("Boxed in the old city, before. Still remembers the rules."),
		TEXT("Never fought. Was told that does not matter."),
		TEXT("Drove a floodlight truck. Is on the other side of the lamps now."),
		TEXT("Took a tag off a dead man. It was the wrong dead man."),
		TEXT("Informed for a Warden, until the Warden moved on."),
		TEXT("Hauled bodies out of this pool for a season."),
		TEXT("Has a child held at the Counting House against a debt."),
		TEXT("Lost a stall to the drought, and the rest to the Flats."),
		TEXT("Says it was a mistake. They all say it was a mistake."),
	};

	/** What each fight won where the player could see it is worth to the book, and the most a record can move it */
	const float EdgePerWin = 0.04f;
	const float MostEdge = 0.2f;

	/** A place in a rank is two figures, so a rank has room for this many in all, its own and whoever came after */
	const int32 MostPlaces = 100;
}

void AGutGameMode::GetCageFighters(TArray<int32>& OutCodes, int32 NewcomersUpTo) const
{
	OutCodes.Reset();

	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	// from the bottom of the ladders to the top, each rank's own first and then whoever has come into it, in the order they came
	for (int32 Rank = 0; Rank < Ranks.Num(); ++Rank)
	{
		for (int32 Rung = 0; Rung < Ranks[Rank]->Rungs.Num(); ++Rung)
		{
			OutCodes.Add((Rank * 100) + Rung);
		}

		for (int32 i = 0; GameInstance && i < GameInstance->Roster.Newcomers.Num() && i < NewcomersUpTo; ++i)
		{
			if (GameInstance->Roster.Newcomers[i].Code / 100 == Rank)
			{
				OutCodes.Add(GameInstance->Roster.Newcomers[i].Code);
			}
		}
	}
}

bool AGutGameMode::IsFallen(int32 Code) const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance && GameInstance->Roster.Dead.Contains(Code);
}

int32 AGutGameMode::GetCageWins(int32 Code) const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const int32* Wins = GameInstance ? GameInstance->Roster.Wins.Find(Code) : nullptr;

	return Wins ? *Wins : 0;
}

float AGutGameMode::GetRecordEdge(int32 FirstWins, int32 SecondWins) const
{
	return FMath::Clamp((FirstWins - SecondWins) * GutRosterConstants::EdgePerWin, -GutRosterConstants::MostEdge, GutRosterConstants::MostEdge);
}

void AGutGameMode::RecordCageFight(int32 Winner, int32 Loser, bool bLoserDied)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return;
	}

	// either may be nobody of the cage's: the player's own fighter, or one out of an owner's yard
	if (Winner >= 0)
	{
		++GameInstance->Roster.Wins.FindOrAdd(Winner);
	}

	if (Loser >= 0 && bLoserDied)
	{
		GameInstance->Roster.Dead.AddUnique(Loser);
	}

	UE_LOG(LogGut, Log, TEXT("Roster: %d won (%d now), %d %s; %d dead in all"), Winner, GetCageWins(Winner), Loser, bLoserDied ? TEXT("is dead") : TEXT("lost and lives"), GameInstance->Roster.Dead.Num());
}

const FGutLadderRung* AGutGameMode::FindNewcomerRung(int32 Code) const
{
	if (const TSharedPtr<FGutLadderRung>* Made = NewcomerRungs.Find(Code))
	{
		return Made->Get();
	}

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Rank = FindRank(Code);

	const FGutNewcomer* Who = GameInstance ? GameInstance->Roster.Newcomers.FindByPredicate([Code](const FGutNewcomer& One) { return One.Code == Code; }) : nullptr;

	if (!Who || !Rank || Rank->Rungs.IsEmpty())
	{
		return nullptr;
	}

	// it fights as one of its rank's own does, and is itself in everything that is seen and read
	TSharedPtr<FGutLadderRung> Rung = MakeShared<FGutLadderRung>(Rank->Rungs[FMath::Clamp(Who->Like, 0, Rank->Rungs.Num() - 1)]);

	Rung->OpponentName = Who->FighterName;
	Rung->Billing = Who->Billing;
	Rung->Body = Who->Body;
	Rung->Weapon = Who->Weapon;
	Rung->Card.Reset();
	Rung->bSparesPlayer = false;
	Rung->Dogs = 0;

	return NewcomerRungs.Add(Code, Rung).Get();
}

bool AGutGameMode::AddNewcomer(int32 Rank)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	if (!GameInstance || !Ranks.IsValidIndex(Rank) || Ranks[Rank]->Rungs.IsEmpty())
	{
		return false;
	}

	FGutRosterState& Roster = GameInstance->Roster;

	// the next place in the rank after its own fights and everybody who has come into it before
	int32 Place = Ranks[Rank]->Rungs.Num();

	for (const FGutNewcomer& One : Roster.Newcomers)
	{
		if (One.Code / 100 == Rank)
		{
			Place = FMath::Max(Place, (One.Code % 100) + 1);
		}
	}

	if (Place >= GutRosterConstants::MostPlaces)
	{
		return false;
	}

	// the same cage with the same dead gets the same newcomer
	FRandomStream Draw((Roster.Newcomers.Num() * 7919) + (Rank * 104729) + 31);

	// what the living are called and look like, and what anybody has ever been called here
	TSet<FString> LivingNames;
	TSet<FString> EverNames;
	TSet<FSoftObjectPath> LivingBodies;

	TArray<int32> Everybody;
	GetCageFighters(Everybody);

	for (const int32 Code : Everybody)
	{
		if (const FGutLadderRung* Rung = FindRung(Code))
		{
			EverNames.Add(Rung->OpponentName.ToString());

			if (!IsFallen(Code))
			{
				LivingNames.Add(Rung->OpponentName.ToString());
				LivingBodies.Add(Rung->Body.ToSoftObjectPath());
			}
		}
	}

	FGutNewcomer New;

	New.Code = (Rank * 100) + Place;

	// a name nobody here has had. When there is none left, one that nobody alive has.
	TArray<FString> Free;

	for (const TSet<FString>* Taken : { &EverNames, &LivingNames })
	{
		for (const TCHAR* Name : GutRosterConstants::Names)
		{
			if (!Taken->Contains(Name))
			{
				Free.Add(Name);
			}
		}

		if (!Free.IsEmpty())
		{
			break;
		}
	}

	New.FighterName = FText::FromString(Free.IsEmpty() ? GutRosterConstants::Names[Draw.RandRange(0, UE_ARRAY_COUNT(GutRosterConstants::Names) - 1)] : Free[Draw.RandRange(0, Free.Num() - 1)]);
	New.Billing = FText::FromString(GutRosterConstants::Billings[Draw.RandRange(0, UE_ARRAY_COUNT(GutRosterConstants::Billings) - 1)]);

	// somebody nobody has seen in this cage. When everybody made for it has been seen, the one who has been dead longest,
	// under the new name. And where nobody was made for it at all, one of the rank's own.
	TArray<TSoftClassPtr<AActor>> Unseen;

	for (const TSoftClassPtr<AActor>& Body : Ranks[0]->NewcomerBodies)
	{
		const bool bSeen = Roster.Newcomers.ContainsByPredicate([&Body](const FGutNewcomer& One) { return One.Body == Body; });

		if (!Body.IsNull() && !bSeen && !LivingBodies.Contains(Body.ToSoftObjectPath()))
		{
			Unseen.Add(Body);
		}
	}

	if (!Unseen.IsEmpty())
	{
		New.Body = Unseen[Draw.RandRange(0, Unseen.Num() - 1)];
	}
	else
	{
		for (const FGutNewcomer& One : Roster.Newcomers)
		{
			if (!One.Body.IsNull() && !LivingBodies.Contains(One.Body.ToSoftObjectPath()) && Ranks[0]->NewcomerBodies.Contains(One.Body))
			{
				New.Body = One.Body;
				break;
			}
		}
	}

	if (New.Body.IsNull())
	{
		New.Body = Ranks[Rank]->Rungs[Draw.RandRange(0, Ranks[Rank]->Rungs.Num() - 1)].Body;
	}

	New.Weapon = RackWeapons.IsEmpty() ? nullptr : RackWeapons[Draw.RandRange(0, RackWeapons.Num() - 1)];

	// like one of its rank who fights alone and to the end: nobody new comes in with the pit boss's dogs
	TArray<int32> Plain;

	for (int32 Rung = 0; Rung < Ranks[Rank]->Rungs.Num(); ++Rung)
	{
		if (!Ranks[Rank]->Rungs[Rung].bSparesPlayer && Ranks[Rank]->Rungs[Rung].Dogs == 0)
		{
			Plain.Add(Rung);
		}
	}

	New.Like = Plain.IsEmpty() ? 0 : Plain[Draw.RandRange(0, Plain.Num() - 1)];

	Roster.Newcomers.Add(New);

	UE_LOG(LogGut, Log, TEXT("Roster: %s comes into the cage among the %s, as %s (%d)"), *New.FighterName.ToString(), *Ranks[Rank]->RankName.ToString(), *New.Body.GetAssetName(), New.Code);

	return true;
}

void AGutGameMode::RefillRoster()
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	TArray<int32> Everybody;
	GetCageFighters(Everybody);

	// each rank is kept at the number it was made with: for everybody dead in it there is somebody new
	for (int32 Rank = 0; Rank < Ranks.Num(); ++Rank)
	{
		int32 Living = 0;

		for (const int32 Code : Everybody)
		{
			Living += (Code / 100 == Rank && !IsFallen(Code)) ? 1 : 0;
		}

		while (Living < Ranks[Rank]->Rungs.Num() && AddNewcomer(Rank))
		{
			++Living;
		}
	}
}

void AGutGameMode::ResetRoster()
{
	if (UGutGameInstance* GameInstance = GetGutGameInstance())
	{
		GameInstance->Roster = FGutRosterState();
	}

	NewcomerRungs.Reset();
	LadderOpponents.Reset();
	StandInRungs.Reset();
}

int32 AGutGameMode::GetLadderRank() const
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	return IsOnLadder() ? Ranks.IndexOfByKey(GetLadder()) : INDEX_NONE;
}

int32 AGutGameMode::GetLadderOpponent(int32 Rung) const
{
	const int32 Rank = GetLadderRank();

	if (Rank == INDEX_NONE)
	{
		return -1;
	}

	// decided once, the first time the fight is looked at: whoever dies in it afterwards is still who was fought
	if (const int32* Decided = LadderOpponents.Find(Rung))
	{
		return *Decided;
	}

	const int32 Own = (Rank * 100) + Rung;

	int32 Code = Own;

	if (IsFallen(Own))
	{
		// the fight's own fighter died somewhere the player was not fighting it: whoever came into the rank first and still lives has the place
		const UGutGameInstance* GameInstance = GetGutGameInstance();

		for (const FGutNewcomer& One : GameInstance->Roster.Newcomers)
		{
			if (One.Code / 100 == Rank && !IsFallen(One.Code))
			{
				Code = One.Code;
				break;
			}
		}
	}

	// with nobody yet come in for the dead there is nothing to decide: asked again once somebody has
	if (Code == Own && IsFallen(Own))
	{
		return Own;
	}

	return LadderOpponents.Add(Rung, Code);
}

const FGutLadderRung* AGutGameMode::GetLadderFight(int32 Rung) const
{
	const UGutLadderData* Rank = GetLadder();

	if (!Rank || !Rank->Rungs.IsValidIndex(Rung))
	{
		return nullptr;
	}

	const FGutLadderRung* Own = &Rank->Rungs[Rung];

	const int32 Code = GetLadderOpponent(Rung);
	const int32 OwnCode = (GetLadderRank() * 100) + Rung;

	if (Code < 0 || Code == OwnCode)
	{
		return Own;
	}

	if (const TSharedPtr<FGutLadderRung>* Made = StandInRungs.Find(Rung))
	{
		return Made->Get();
	}

	const FGutLadderRung* Who = FindNewcomerRung(Code);

	if (!Who)
	{
		return Own;
	}

	// the fight is what it was, for what it was, and somebody else is in it
	TSharedPtr<FGutLadderRung> StandIn = MakeShared<FGutLadderRung>(*Own);

	StandIn->OpponentName = Who->OpponentName;
	StandIn->Billing = Who->Billing;
	StandIn->Body = Who->Body;
	StandIn->Weapon = Who->Weapon;

	StandIn->Card.Reset();
	StandIn->Card.Add(FText::FromString(FString::Printf(TEXT("%s is dead. %s has the place."), *Own->OpponentName.ToString(), *Who->OpponentName.ToString())));

	return StandInRungs.Add(Rung, StandIn).Get();
}
