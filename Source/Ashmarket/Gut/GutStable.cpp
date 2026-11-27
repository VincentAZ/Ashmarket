// Ashmarket prototype: the Gut.

// The Gut entered as an owner (design document, chapters 6 and 7): the player buys fighters at the Block or signs them for a wage,
// trains them, dopes them or does not, promises them their tags or does not, chooses their fights and watches them from the wall.
// These are members of the cage's game mode, kept in a file of their own.

#include "GutGameMode.h"
#include "Algo/IndexOf.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GutArmorData.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "GutSound.h"
#include "GutWeaponData.h"
#include "Kismet/GameplayStatics.h"

namespace GutStableConstants
{
	/** Names for the people at the Block. Cage names: short, and nobody's own. */
	const TCHAR* Names[] =
	{
		TEXT("Ash"), TEXT("Mole"), TEXT("Crow"), TEXT("Splint"), TEXT("Lye"), TEXT("Burr"), TEXT("Slag"), TEXT("Tar"),
		TEXT("Flint"), TEXT("Gull"), TEXT("Nettle"), TEXT("Rivet"), TEXT("Soot"), TEXT("Marrow"), TEXT("Thorn"), TEXT("Cinder"),
		TEXT("Wren"), TEXT("Husk"), TEXT("Bolt"), TEXT("Grist"), TEXT("Vetch"), TEXT("Shard"), TEXT("Pitch"), TEXT("Lark")
	};

	/** Who they were, and how they came to be for sale. The document's own list of ways, and of backgrounds. */
	const TCHAR* Owned[] =
	{
		TEXT("Ran a kiosk until the drought. The debt is in Mother Ledger's book."),
		TEXT("Taken in the Flats without a tag. Says nothing about before."),
		TEXT("Used to be a Warden. Sentenced by the people who worked the next desk."),
		TEXT("A raider out of the Kiln. Caught with a leg wound that has healed."),
		TEXT("Sold by a brother, against a debt of nine litres."),
		TEXT("Set bones for the Scrappers until one died on the table."),
		TEXT("Lost everything at this window, and then the tag."),
		TEXT("A drifter. Nobody came to the gate to ask."),
		TEXT("Handed over by a rival crew, to settle something."),
		TEXT("Drove convoys. The convoy did not arrive, and somebody had to owe for it."),
	};

	/** And the ones nobody owns, who are here because the cage pays */
	const TCHAR* Signed[] =
	{
		TEXT("Has a tag and a family under the overpass. Fights for the wage."),
		TEXT("Three years on a scrap crew. Says this is quicker."),
		TEXT("Won a tag in this cage once. Has come back for the water."),
		TEXT("Owes nobody. Wants it kept that way."),
		TEXT("Walked in from the Bones with a tag and nothing else."),
	};

	/** How a fighter with nothing taught it and one with everything taught it fight */
	const float MostTaught = 5.0f;

	/** Who stands in the yard is the street's own kind of person: one that stands and does nothing else */
	const TCHAR* YardBodyClass = TEXT("/Game/Gut/Blueprints/BP_GutDummy.BP_GutDummy_C");

	/**
	 *  Where the yard's people stand as the owner looks at them, in centimetres ahead of its eye and to either side of
	 *  the middle: the first two, then the two behind them. Clear of the card, which fills the middle of the picture.
	 */
	const float YardDepth[] = { 450.0f, 600.0f };
	const float YardAside[] = { 195.0f, 324.0f };

	/** And no nearer the pool's walls than this, from its middle */
	const FVector2D YardHalf(640.0f, 440.0f);
}

bool AGutGameMode::IsAtStable() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return Ladder && GameInstance && GameInstance->bStable && !GameInstance->bSandbox && !GameInstance->bWindow && !GameInstance->bAtDoor;
}

const FGutStableState& AGutGameMode::GetStableState() const
{
	static const FGutStableState Empty;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance ? GameInstance->Stable : Empty;
}

void AGutGameMode::EnterStable(int32 Seed)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !Ladder)
	{
		return;
	}

	GameInstance->bSandbox = false;
	GameInstance->bWindow = false;
	GameInstance->bStable = true;

	GameInstance->Stable = FGutStableState();
	GameInstance->Stable.bStarted = true;
	GameInstance->PendingStory = EGutStory::Owner;
	GameInstance->Stable.Water = FMath::Max(GameInstance->GetStartingWater(StableStartingWater), 0.0f);
	GameInstance->Stable.Seed = Seed != 0 ? Seed : FMath::RandRange(1, 1000000);

	UE_LOG(LogGut, Log, TEXT("Stable: an owner comes to the cage with %.0f L"), StableStartingWater);

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true);
}

const UGutLadderData* AGutGameMode::FindRank(int32 Code) const
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	return (Code >= 0 && Ranks.IsValidIndex(Code / 100)) ? Ranks[Code / 100] : nullptr;
}

UGutArmorData* AGutGameMode::FindArmorPiece(const FName& Name) const
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	for (const UGutLadderData* Rank : Ranks)
	{
		for (UGutArmorData* Piece : Rank ? Rank->ArmorShop : TArray<TObjectPtr<UGutArmorData>>())
		{
			if (Piece && Piece->GetFName() == Name)
			{
				return Piece;
			}
		}
	}

	return nullptr;
}

const FGutLadderRung* AGutGameMode::FindRung(int32 Code) const
{
	const UGutLadderData* Rank = FindRank(Code);

	if (!Rank)
	{
		return nullptr;
	}

	// one of the rank's own, or somebody who came into it after the dead
	return Rank->Rungs.IsValidIndex(Code % 100) ? &Rank->Rungs[Code % 100] : FindNewcomerRung(Code);
}

void AGutGameMode::GetBlockOffers(TArray<FGutStableFighter>& OutOffers) const
{
	GetBlockOffersFor(GetStableState(), OutOffers);
}

void AGutGameMode::BeginStableState(FGutStableState& State, int32 Seed) const
{
	State = FGutStableState();
	State.bStarted = true;

	// what an owner comes with: the yard's own stake, or with one purse what the player already holds
	const UGutGameInstance* Run = GetGutGameInstance();

	State.Water = Run ? FMath::Max(Run->GetStartingWater(StableStartingWater), 0.0f) : StableStartingWater;
	State.Seed = Seed != 0 ? Seed : FMath::RandRange(1, 1000000);
}

FText AGutGameMode::GetCageName(int32 Number)
{
	return FText::FromString(GutStableConstants::Names[FMath::Abs(Number) % UE_ARRAY_COUNT(GutStableConstants::Names)]);
}

bool AGutGameMode::TakeBlockOffer(FGutStableState& State, const FGutStableFighter& Offer, int32 OfferIndex, FString* Why) const
{
	auto No = [Why](const FString& Because)
	{
		if (Why)
		{
			*Why = Because;
		}

		return false;
	};

	if (State.BoughtToday.Contains(OfferIndex))
	{
		return No(TEXT("taken"));
	}

	if (State.Fighters.Num() >= StableMostFighters)
	{
		return No(TEXT("yard full"));
	}

	if (Offer.Price > State.Water)
	{
		return No(FString::Printf(TEXT("%.0f L short"), Offer.Price - State.Water));
	}

	State.Water -= Offer.Price;
	State.Spent += Offer.Price;

	State.BoughtToday.Add(OfferIndex);
	State.Fighters.Add(Offer);

	return true;
}

void AGutGameMode::GetBlockOffersFor(const FGutStableState& State, TArray<FGutStableFighter>& OutOffers, int32 Count) const
{
	const int32 Offered = Count > 0 ? Count : BlockOffersPerDay;

	OutOffers.Reset();

	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	// the bodies of the cage's own fighters are what there is to draw from
	TArray<TSoftClassPtr<AActor>> Bodies;

	for (const UGutLadderData* Rank : Ranks)
	{
		for (const FGutLadderRung& Rung : Rank->Rungs)
		{
			if (!Rung.Body.IsNull())
			{
				Bodies.AddUnique(Rung.Body);
			}
		}
	}

	// the same seed and the same day are the same people at the Block
	FRandomStream Draw(State.Seed + (State.Day * 104729));

	for (int32 i = 0; i < Offered; ++i)
	{
		FGutStableFighter Offer;

		// the last on the list is somebody with a tag, who will sign and cannot be bought
		Offer.bOwned = i < Offered - 1;

		Offer.FighterName = FText::FromString(GutStableConstants::Names[Draw.RandRange(0, UE_ARRAY_COUNT(GutStableConstants::Names) - 1)]);

		// (nobody on one day's list has another's name: the next name along that is free)
		for (int32 Tried = 0; Tried < UE_ARRAY_COUNT(GutStableConstants::Names) && OutOffers.ContainsByPredicate([&Offer](const FGutStableFighter& Other) { return Other.FighterName.EqualTo(Offer.FighterName); }); ++Tried)
		{
			const int32 Was = FMath::Max(0, static_cast<int32>(Algo::IndexOfByPredicate(GutStableConstants::Names, [&Offer](const TCHAR* Name) { return Offer.FighterName.ToString() == Name; })));

			Offer.FighterName = FText::FromString(GutStableConstants::Names[(Was + 1) % UE_ARRAY_COUNT(GutStableConstants::Names)]);
		}

		Offer.Billing = FText::FromString(Offer.bOwned
			? GutStableConstants::Owned[Draw.RandRange(0, UE_ARRAY_COUNT(GutStableConstants::Owned) - 1)]
			: GutStableConstants::Signed[Draw.RandRange(0, UE_ARRAY_COUNT(GutStableConstants::Signed) - 1)]);

		if (!Bodies.IsEmpty())
		{
			Offer.Body = Bodies[Draw.RandRange(0, Bodies.Num() - 1)];
		}

		// what it has in its hands when it is led out: one of the rack's weapons, or nothing
		const int32 Held = Draw.RandRange(0, RackWeapons.Num());

		Offer.Weapon = RackWeapons.IsValidIndex(Held) ? RackWeapons[Held] : nullptr;

		// somebody who signs has done this before
		const int32 Least = Offer.bOwned ? 0 : 1;
		const int32 Most = Offer.bOwned ? 2 : 3;

		Offer.Strength = Draw.RandRange(Least, Most);
		Offer.Speed = Draw.RandRange(Least, Most);
		Offer.Nerve = Draw.RandRange(Least, Most);

		// (drawn on its own, so that who is at the Block on a day is who was before there was a fourth thing to draw)
		Offer.Cunning = FRandomStream(State.Seed + (State.Day * 104729) + (i * 31)).RandRange(Least, Most);

		const float Taught = Offer.GetTaught();

		if (Offer.bOwned)
		{
			// the document's rule: health, skills and reputation make the price
			Offer.Price = FMath::RoundToFloat(BlockBasePrice + (BlockPricePerSkill * Taught) + (Offer.Weapon ? BlockPricePerSkill : 0.0f));
			Offer.Resentment = 20.0f;
		}
		else
		{
			Offer.Wage = SignedBaseWage + (0.5f * Taught);
		}

		OutOffers.Add(Offer);
	}
}

void AGutGameMode::GetStableOffers(TArray<int32>& OutAgainst) const
{
	OutAgainst.Reset();

	const FGutStableState& State = GetStableState();

	// whoever in the cage is alive, however the others died
	TArray<int32> Living;
	GetCageFighters(Living);

	Living.RemoveAll([this, &State](int32 Code) { return State.Dead.Contains(Code) || IsFallen(Code); });

	FRandomStream Draw(State.Seed + (State.Day * 15485863));

	while (OutAgainst.Num() < StableOffersPerDay && !Living.IsEmpty())
	{
		const int32 Index = Draw.RandRange(0, Living.Num() - 1);

		OutAgainst.Add(Living[Index]);
		Living.RemoveAt(Index);
	}

	// easiest first
	OutAgainst.Sort();
}

float AGutGameMode::GetStablePurse(int32 Against) const
{
	const FGutLadderRung* Rung = FindRung(Against);

	// an owner is paid the whole purse and a share of the gate, which grows with the stable's name
	return Rung ? FMath::RoundToFloat(Rung->Purse * (StablePurseScale + (0.05f * GetStableState().Name))) : 0.0f;
}

float AGutGameMode::GetStableBookChance() const
{
	const FGutStableState& State = GetStableState();
	const FGutLadderRung* Rung = FindRung(State.MatchedAgainst);

	if (!State.Fighters.IsValidIndex(State.Matched) || !Rung)
	{
		return 0.5f;
	}

	const FGutStableFighter& Fighter = State.Fighters[State.Matched];

	// what the book would give a newcomer against the stable's fighter: the more it has been taught, the less
	const float Own = FMath::Clamp(0.92f - (0.045f * Fighter.GetTaught()), 0.2f, 0.92f);

	float Chance = 0.5f + (Rung->BookChance - Own);

	// what each has won in front of the book
	Chance += GetRecordEdge(Fighter.Wins, GetCageWins(State.MatchedAgainst));

	// what the book can see: a fighter led in hurt, or with nothing in its hands
	if (Fighter.IsCrippled())
	{
		Chance -= 0.15f;
	}

	if (!Fighter.Weapon && Rung->Weapon)
	{
		Chance -= 0.15f;
	}

	return FMath::Clamp(Chance, 0.12f, 0.88f);
}

bool AGutGameMode::SpendStableWater(float Amount)
{
	FGutStableState& State = GetGutGameInstance()->Stable;

	if (Amount > State.Water)
	{
		return false;
	}

	State.Water -= Amount;
	State.Spent += Amount;

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && Amount > 0.0f)
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	return true;
}

bool AGutGameMode::BuyFighter(int32 OfferIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	TArray<FGutStableFighter> Offers;
	GetBlockOffers(Offers);

	if (!Offers.IsValidIndex(OfferIndex) || !TakeBlockOffer(State, Offers[OfferIndex], OfferIndex))
	{
		return false;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && Offers[OfferIndex].Price > 0.0f)
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %s %s, %.0f L in hand"), *Offers[OfferIndex].FighterName.ToString(), Offers[OfferIndex].bOwned ? TEXT("is bought") : TEXT("signs"), State.Water);

	return true;
}

bool AGutGameMode::SellFighter(int32 FighterIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex))
	{
		return false;
	}

	const FGutStableFighter Gone = State.Fighters[FighterIndex];

	State.Fighters.RemoveAt(FighterIndex);

	if (Gone.bOwned)
	{
		// the Chainmen take their fifth, and a fighter with a limb gone is worth half
		State.Water += FMath::RoundToFloat(Gone.Price * 0.8f * (Gone.IsCrippled() ? 0.5f : 1.0f));

		// the rest of the yard watched it be led off
		for (FGutStableFighter& Other : State.Fighters)
		{
			if (Other.bOwned)
			{
				Other.Resentment = FMath::Min(100.0f, Other.Resentment + ResentmentForSale);
			}
		}
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %s %s"), *Gone.FighterName.ToString(), Gone.bOwned ? TEXT("is sold at the Block") : TEXT("is let go"));

	return true;
}

bool AGutGameMode::TrainFighter(int32 FighterIndex, EGutTraining What)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex))
	{
		return false;
	}

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	int32& Taught = What == EGutTraining::Strength ? Fighter.Strength : (What == EGutTraining::Speed ? Fighter.Speed : (What == EGutTraining::Nerve ? Fighter.Nerve : Fighter.Cunning));

	if (Taught >= StableMostTaught || !SpendStableWater(TrainingCost))
	{
		return false;
	}

	++Taught;

	UE_LOG(LogGut, Log, TEXT("Stable: %s is trained: strength %d, speed %d, nerve %d, cunning %d"), *Fighter.FighterName.ToString(), Fighter.Strength, Fighter.Speed, Fighter.Nerve, Fighter.Cunning);

	// it takes the day
	AdvanceStableDay();

	return true;
}

float AGutGameMode::GetBlockPrice(const FGutStableFighter& Who) const
{
	return FMath::RoundToFloat(BlockBasePrice + (BlockPricePerSkill * Who.GetTaught()) + (Who.Weapon ? BlockPricePerSkill : 0.0f));
}

float AGutGameMode::GetHirePay(const FGutStableFighter& Who) const
{
	// a back and a pair of legs: two litres for turning up, and more for whoever can carry more of it faster
	return 2.0f + Who.Strength + FMath::FloorToFloat(Who.Speed * 0.5f);
}

bool AGutGameMode::HireOutFighter(int32 FighterIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex) || State.Fighters[FighterIndex].IsCrippled())
	{
		return false;
	}

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	const float Pay = GetHirePay(Fighter);

	State.Water += Pay;

	// somebody with a collar on knows whose water a day's hauling earned
	if (Fighter.bOwned)
	{
		Fighter.Resentment = FMath::Min(100.0f, Fighter.Resentment + 6.0f);
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %s is hired out for the day and brings back %.0f L"), *Fighter.FighterName.ToString(), Pay);

	// it takes the day
	AdvanceStableDay();

	return true;
}

bool AGutGameMode::PatchFighter(int32 FighterIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex) || !State.Fighters[FighterIndex].IsCrippled() || !SpendStableWater(StablePatchCost))
	{
		return false;
	}

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	// the cutter does one limb for the money: the first that is gone
	for (float& Limb : Fighter.LimbHealth)
	{
		if (Limb == 0.0f)
		{
			Limb = -1.0f;
			break;
		}
	}

	// medicine is one of the things that is remembered kindly
	Fighter.Resentment = FMath::Max(0.0f, Fighter.Resentment - ResentmentEasedByCare);

	return true;
}

bool AGutGameMode::DopeFighter(int32 FighterIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex) || State.Fighters[FighterIndex].bDoped || !SpendStableWater(DopeCost))
	{
		return false;
	}

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	Fighter.bDoped = true;
	Fighter.Habit = FMath::Min(1.0f, Fighter.Habit + DopeHabit);

	// nobody is asked first
	if (Fighter.bOwned)
	{
		Fighter.Resentment = FMath::Min(100.0f, Fighter.Resentment + ResentmentForDope);
	}

	return true;
}

bool AGutGameMode::IsTagDue(int32 FighterIndex) const
{
	const FGutStableState& State = GetStableState();

	return State.Fighters.IsValidIndex(FighterIndex) && State.Fighters[FighterIndex].bPromisedTag
		&& State.Fighters[FighterIndex].Wins - State.Fighters[FighterIndex].WinsAtPromise >= WinsForTag;
}

bool AGutGameMode::PromiseTag(int32 FighterIndex)
{
	if (!IsYardOpen())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	if (!State.Fighters.IsValidIndex(FighterIndex))
	{
		return false;
	}

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	// only somebody with a collar wants a tag, and nobody believes the same hand twice
	if (!Fighter.bOwned || Fighter.bPromisedTag || Fighter.bBetrayed)
	{
		return false;
	}

	Fighter.bPromisedTag = true;
	Fighter.WinsAtPromise = Fighter.Wins;
	Fighter.Resentment = FMath::Max(0.0f, Fighter.Resentment - ResentmentEasedByPromise);

	UE_LOG(LogGut, Log, TEXT("Stable: %s is promised its tag for %d more wins"), *Fighter.FighterName.ToString(), WinsForTag);

	return true;
}

bool AGutGameMode::GiveTag(int32 FighterIndex)
{
	if (!IsYardOpen() || !IsTagDue(FighterIndex) || !SpendStableWater(TagPrice))
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;
	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	const FString Who = Fighter.FighterName.ToString();

	// the rest of the yard saw a promise kept
	for (FGutStableFighter& Other : State.Fighters)
	{
		Other.Resentment = FMath::Max(0.0f, Other.Resentment - ResentmentEasedByTag);
	}

	++State.Name;

	// free. One that was treated well enough stays on for a wage; the rest walk out of the gate.
	const bool bStays = Fighter.Resentment <= StaysOnBelowResentment;

	if (bStays)
	{
		Fighter.bOwned = false;
		Fighter.bPromisedTag = false;
		Fighter.Resentment = 0.0f;
		Fighter.Wage = SignedBaseWage + (0.5f * Fighter.GetTaught());
	}
	else
	{
		State.Fighters.RemoveAt(FighterIndex);
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %s is given its tag, and %s"), *Who, bStays ? TEXT("stays on for a wage") : TEXT("walks out of the gate"));

	return true;
}

bool AGutGameMode::BreakPromise(int32 FighterIndex)
{
	if (!IsYardOpen() || !IsTagDue(FighterIndex))
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;
	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	Fighter.bPromisedTag = false;
	Fighter.bBetrayed = true;

	// the document's rule: breaking the promise spikes resentment across the whole stable
	for (FGutStableFighter& Other : State.Fighters)
	{
		if (Other.bOwned)
		{
			Other.Resentment = FMath::Min(100.0f, Other.Resentment + (&Other == &Fighter ? ResentmentForBrokenPromise * 2.0f : ResentmentForBrokenPromise));
		}
	}

	UE_LOG(LogGut, Log, TEXT("Stable: the promise to %s is broken"), *Fighter.FighterName.ToString());

	return true;
}

float AGutGameMode::GetBodyPrice(EGutBodyGoesTo Where) const
{
	if (Where == EGutBodyGoesTo::Parts)
	{
		return FMath::RoundToFloat(BodyPartsPrice * (GetStableState().bBodyBroken ? 0.5f : 1.0f));
	}

	// (nobody pays for one that is kept, and it costs nothing but what the yard thinks)
	if (Where == EGutBodyGoesTo::Jerky)
	{
		return 0.0f;
	}

	return Where == EGutBodyGoesTo::Grill ? BodyGrillPrice : -BurialCost;
}

bool AGutGameMode::SendBodyTo(EGutBodyGoesTo Where)
{
	if (!IsYardOpen() || !GetStableState().bBodyWaiting)
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;

	float Grudge = 0.0f;

	if (Where == EGutBodyGoesTo::Ground)
	{
		// the ground behind the pumps is not free either
		if (!SpendStableWater(BurialCost))
		{
			return false;
		}

		Grudge = -ResentmentEasedByBurial;

		++State.Buried;
	}
	else if (Where == EGutBodyGoesTo::Jerky)
	{
		// salted down, and in the owner's own bag: food, of a kind
		GetGutGameInstance()->AddToBag(FName("jerky"), JerkyFromBody);

		Grudge = ResentmentForBodyToJerky;

		++State.BodiesSold;
	}
	else
	{
		State.Water += GetBodyPrice(Where);

		Grudge = Where == EGutBodyGoesTo::Parts ? ResentmentForBodyToParts : ResentmentForBodyToGrill;

		++State.BodiesSold;

		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			Sounds->Play(FName("cash"), FVector::ZeroVector);
		}
	}

	// everybody in a collar knows by morning where it went, and that they are next
	for (FGutStableFighter& Other : State.Fighters)
	{
		if (Other.bOwned)
		{
			Other.Resentment = FMath::Clamp(Other.Resentment + Grudge, 0.0f, 100.0f);
		}
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %s goes to %s, %.0f L in hand"), *State.BodyOf.ToString(),
		Where == EGutBodyGoesTo::Parts ? TEXT("Doc Stitch") : (Where == EGutBodyGoesTo::Grill ? TEXT("the grill carts") : (Where == EGutBodyGoesTo::Jerky ? TEXT("the salt, and the owner's bag") : TEXT("the ground"))), State.Water);

	State.bBodyWaiting = false;
	State.bBodyBroken = false;
	State.BodyOf = FText();

	GetGutGameInstance()->SaveRun();

	return true;
}

void AGutGameMode::RestDay()
{
	if (IsYardOpen())
	{
		AdvanceStableDay();
	}
}

void AGutGameMode::AdvanceStableDay()
{
	FGutStableState& State = GetGutGameInstance()->Stable;

	++State.Day;

	State.BoughtToday.Reset();

	// everybody eats, and the ones with tags are paid
	float Due = 0.0f;

	for (const FGutStableFighter& Fighter : State.Fighters)
	{
		Due += FeedPerDay + (Fighter.bOwned ? 0.0f : Fighter.Wage);
	}

	const bool bPaid = Due <= State.Water;

	if (bPaid)
	{
		State.Water -= Due;
		State.Spent += Due;
	}
	else
	{
		// what cannot be paid is written down, with the lender's share on top
		State.News += FString::Printf(TEXT("The day's feed and wages were %.0f L and you held %.0f: what was short is written against you, with half as much again on top. "), Due, State.Water);

		State.Debt += (Due - State.Water) * 1.5f;
		State.Spent += State.Water;
		State.Water = 0.0f;
	}

	for (int32 i = State.Fighters.Num() - 1; i >= 0; --i)
	{
		FGutStableFighter& Fighter = State.Fighters[i];

		// what it was given wears off slowly, if it is not given more
		Fighter.Habit = FMath::Max(0.0f, Fighter.Habit - HabitLostPerDay);

		if (!Fighter.bOwned)
		{
			// somebody with a tag who is not paid walks
			if (!bPaid)
			{
				UE_LOG(LogGut, Log, TEXT("Stable: %s is not paid, and walks"), *Fighter.FighterName.ToString());

				State.News += FString::Printf(TEXT("%s was on a wage, was not paid for the day, and has walked. "), *Fighter.FighterName.ToString());

				State.Fighters.RemoveAt(i);
			}

			continue;
		}

		// a day fed is a day's less grudge; a day hungry is the other thing
		Fighter.Resentment = FMath::Clamp(Fighter.Resentment + (bPaid ? -ResentmentEasedPerDay : ResentmentForHunger), 0.0f, 100.0f);

		// and past bearing, it is gone in the night
		if (Fighter.Resentment >= RunsAtResentment)
		{
			UE_LOG(LogGut, Log, TEXT("Stable: %s is gone in the night"), *Fighter.FighterName.ToString());

			++State.FightersLost;

			State.Fighters.RemoveAt(i);
		}
	}

	UE_LOG(LogGut, Log, TEXT("Stable: day %d, %d in the yard, %.0f L in hand, %.0f L owed"), State.Day, State.Fighters.Num(), State.Water, State.Debt);

	GetGutGameInstance()->SaveRun();
}

bool AGutGameMode::MakeMatch(int32 FighterIndex, int32 Against)
{
	if (Phase != EGutFightPhase::Card || !bFightersReady || !IsAtStable())
	{
		return false;
	}

	FGutStableState& State = GetGutGameInstance()->Stable;
	const FGutLadderRung* Rung = FindRung(Against);

	if (!State.Fighters.IsValidIndex(FighterIndex) || !Rung || State.Dead.Contains(Against) || IsFallen(Against))
	{
		return false;
	}

	State.Matched = FighterIndex;
	State.MatchedAgainst = Against;

	FGutStableFighter& Fighter = State.Fighters[FighterIndex];

	// Whoever goes in from the yard eats and drinks off its rack first, if there is bread or tea on it: what the yard
	// keeps is for that (25 November). Said when the owner is next in the yard.
	if (UGutGameInstance* Run = GetGutGameInstance(); Run && !LentTo)
	{
		FString Had;

		if (!Fighter.bFed && Run->GetRackCount(FName("bread")) > 0)
		{
			Fighter.bFed = true;
			Run->AddToRack(FName("bread"), -1);
			Had = TEXT("bread");
		}

		if (!Fighter.bSteadied && Run->GetRackCount(FName("tea")) > 0)
		{
			Fighter.bSteadied = true;
			Run->AddToRack(FName("tea"), -1);
			Had += Had.IsEmpty() ? TEXT("tea") : TEXT(" and tea");
		}

		if (!Had.IsEmpty())
		{
			State.News += FString::Printf(TEXT("%s had %s off the rack before the fight. "), *Fighter.FighterName.ToString(), *Had);

			UE_LOG(LogGut, Log, TEXT("Stable: %s has %s off the rack before going in"), *Fighter.FighterName.ToString(), *Had);
		}
	}

	// sent in hurt: it will remember who sent it
	if (Fighter.IsCrippled() && Fighter.bOwned)
	{
		Fighter.Resentment = FMath::Min(100.0f, Fighter.Resentment + ResentmentForSentInHurt);
	}

	// does it mean to fight? Past a point, a collared fighter would sooner cost its owner the purse
	bStableLayDown = Fighter.bOwned && Fighter.Resentment >= LaysDownAtResentment
		&& (NextStableRoll >= 0.0f ? NextStableRoll : FMath::FRand()) < LayDownChance;

	NextStableRoll = -1.0f;

	MakeStableFighter(PlayerFighter, Fighter, bStableLayDown);
	MakeFighter(EnemyFighter, *Rung, Against / 100 == 0);

	UE_LOG(LogGut, Log, TEXT("Stable: %s is sent in against %s for %.0f L"), *Fighter.FighterName.ToString(), *Rung->OpponentName.ToString(), GetStablePurse(Against));

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("card"), FVector::ZeroVector);
	}

	SetBet(EGutBetSide::None, 0.0f);
	SetPhase(EGutFightPhase::Betting);

	return true;
}

void AGutGameMode::MakeStableFighter(AGutFighter* Fighter, const FGutStableFighter& Who, bool bLayDown)
{
	// what it has been taught, what it has been given, and what it thinks of who gave it
	int32 Strength = Who.Strength;
	int32 Speed = Who.Speed;
	int32 Nerve = Who.Nerve;
	int32 Cunning = Who.Cunning;

	if (Who.bDoped)
	{
		Strength += DopeEdge;
		Speed += DopeEdge;
	}
	else if (Who.Habit >= 0.5f)
	{
		// it is no good without it now
		const int32 Lack = Who.Habit >= 1.0f ? 2 : 1;

		Strength = FMath::Max(0, Strength - Lack);
		Speed = FMath::Max(0, Speed - Lack);
		Nerve = FMath::Max(0, Nerve - Lack);
		Cunning = FMath::Max(0, Cunning - Lack);
	}

	if (Who.bOwned && Who.Resentment >= SulksAtResentment)
	{
		Nerve = FMath::Max(0, Nerve - 1);
	}

	// a promise is something to fight for
	if (Who.bPromisedTag)
	{
		++Nerve;
	}

	Fighter->SetDisplayName(Who.FighterName);
	Fighter->SetMaxHealth(StableBaseHealth + (HealthPerStrength * Strength));
	Fighter->SetDamageScale(1.0f + (DamagePerStrength * Strength));
	Fighter->SetMoveSpeedScale(1.0f + (MovePerSpeed * Speed));
	// and what it was given on the way home from the market, as the player's own fighter is
	Fighter->SetStaminaRegenScale((1.0f + (StaminaPerSpeed * Speed)) * (Who.bFed ? FedStaminaRegenScale : 1.0f));
	Fighter->SetStaminaRegenDelayScale(Who.bSteadied ? SteadiedRegenDelayScale : 1.0f);

	if (!Who.Body.IsNull())
	{
		Fighter->SetBody(Who.Body, Fighter->GetBodyRetargeter());
	}

	Fighter->ClearArmor();
	Fighter->SetCollared(Who.bOwned);

	// What it has been given to wear, on it: whatever rank the fight is at. (For half a day the cage let armor in only
	// from the rank it is sold at, and a vest put on a fighter was gone when he went in to fight, with nothing to say
	// why: "I put a vest on Lye and then when he went in to fight, it was missing", 25 November.)
	for (const FName& Name : Who.Armor)
	{
		if (UGutArmorData* Piece = FindArmorPiece(Name))
		{
			Fighter->WearArmor(Piece);
		}
	}

	// it comes to the fight in the state the last one left it. Empty its hands first: an arm that arrives crippled has nothing to drop.
	Fighter->EquipWeapon(nullptr);

	for (int32 i = 0; i < GutNumLimbs; ++i)
	{
		if (Who.LimbHealth[i] >= 0.0f)
		{
			Fighter->SetLimbHealth(static_cast<EGutLimb>(i), Who.LimbHealth[i]);
		}
	}

	Fighter->EquipWeapon(Who.Weapon);

	if (AGutOpponentController* Brain = Cast<AGutOpponentController>(Fighter->GetController()))
	{
		FGutOpponentTuning Taught = Brain->GetTuning();

		const float Share = FMath::Clamp(Nerve / GutStableConstants::MostTaught, 0.0f, 1.2f);

		Taught.MinReactionTime = FMath::Lerp(0.3f, 0.16f, Share);
		Taught.MaxReactionTime = FMath::Lerp(0.5f, 0.27f, Share);
		Taught.MinDecisionInterval = FMath::Lerp(1.3f, 0.9f, Share);
		Taught.MaxDecisionInterval = FMath::Lerp(2.8f, 1.9f, Share);
		Taught.AttackChance = FMath::Lerp(0.28f, 0.38f, Share);
		Taught.BlockLightChance = FMath::Lerp(0.3f, 0.65f, Share);
		Taught.BlockHeavyChance = FMath::Lerp(0.2f, 0.5f, Share);
		Taught.DodgeHeavyChance = FMath::Lerp(0.1f, 0.3f, Share);
		Taught.CounterChance = FMath::Lerp(0.4f, 0.95f, Share);
		Taught.PunishChance = FMath::Lerp(0.25f, 0.75f, Share);

		// and how well it uses the cage, and its head
		const float Wits = FMath::Clamp(Cunning / GutStableConstants::MostTaught, 0.0f, 1.2f);

		Taught.Cunning = FMath::Clamp(Wits, 0.0f, 1.0f);
		Taught.HeavyChanceIntoHazard = FMath::Lerp(0.2f, 0.95f, Wits);
		Taught.HeavyChanceAgainstGuard = FMath::Lerp(0.25f, 0.75f, Wits);
		Taught.AttackStaminaReserve = FMath::Lerp(4.0f, 26.0f, Wits);

		if (bLayDown)
		{
			// it has decided what this fight is worth to its owner
			Taught.AttackChance = 0.04f;
			Taught.CrowdedAttackChance = 0.04f;
			Taught.GuardChance = 0.0f;
			Taught.BlockLightChance = 0.0f;
			Taught.BlockHeavyChance = 0.0f;
			Taught.DodgeHeavyChance = 0.0f;
			Taught.CounterChance = 0.0f;
			Taught.PunishChance = 0.0f;
			Taught.GuardAfterHitChance = 0.0f;
			Taught.bNeverCovers = true;
		}

		Brain->SetTuning(Taught);
	}
}

bool AGutGameMode::BackToWalkedYard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || LentTo || !GameInstance->bStableFromYard || StablePage != EGutStablePage::Yard || !GameInstance->IsStreetOpen() || GameInstance->bGoingToStreet)
	{
		return false;
	}

	UE_LOG(LogGut, Log, TEXT("Stable: back out by the hatch to the yard"));

	// nothing of the cage's card is drawn from here on, and the level is left once this one has finished what it is doing
	GameInstance->bGoingToStreet = true;

	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UGutGameInstance* Run = GetGutGameInstance())
		{
			Run->GoToStreet(this);
		}
	}));

	return true;
}

void AGutGameMode::BeginStableCard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance->Stable.bStarted)
	{
		GameInstance->Stable = FGutStableState();
		GameInstance->Stable.bStarted = true;
		GameInstance->PendingStory = EGutStory::Owner;
		GameInstance->Stable.Water = FMath::Max(GameInstance->GetStartingWater(StableStartingWater), 0.0f);
		GameInstance->Stable.Seed = FMath::RandRange(1, 1000000);
	}

	GameInstance->Stable.Matched = -1;
	GameInstance->Stable.MatchedAgainst = -1;

	StableSettlement = FGutStableSettlement();
	StablePage = GameInstance->Stable.bBodyWaiting ? EGutStablePage::Body : EGutStablePage::Yard;

	// somebody who came by the Block's clerk is shown who is for sale, once: the yard is a page back from there
	if (GameInstance->bStableAtBlock && StablePage == EGutStablePage::Yard)
	{
		StablePage = EGutStablePage::Block;
	}

	GameInstance->bStableAtBlock = false;

	// and somebody who came in by the pit boss's hatch, out of a yard they walk about in, is shown the fights there are to be had
	if (GameInstance->bStableAtBill && StablePage == EGutStablePage::Yard)
	{
		StablePage = EGutStablePage::Bill;
	}

	// The dead come first, but the fights are still what they came for: once the body is seen to, the bill is the next
	// page. It was the yard's, which for a yard that is walked about in is the way out, so whoever came to make a fight
	// was put back outside with nobody in tow (24 November: "the screen crashed and took me back").
	bBillAfterBody = GameInstance->bStableAtBill && StablePage == EGutStablePage::Body;

	GameInstance->bStableAtBill = false;

	// and somebody whose yard is a place is never shown the card of it: a fight over, they are stood back in the yard
	if (BackToWalkedYard())
	{
		return;
	}

	StableFocus = 0;
	bStableLayDown = false;

	// both corners belong to the cage's brain: the owner watches from the wall
	StartSpectating();

	// the choice starts on the card's first line
	TArray<FGutCardItem> Items;
	GetStableCardItems(Items);

	CardSelection = Items.IsEmpty() ? FGutCardItem() : Items[0];

	// a game that is closed comes back to the yard as it stands
	GameInstance->SaveRun();

	SetPhase(EGutFightPhase::Card);

	// an owner's first day begins with where it has come to
	if (GameInstance->PendingStory == EGutStory::Owner)
	{
		TellStory(EGutStory::Owner);

		GameInstance->PendingStory = EGutStory::None;
	}
}

void AGutGameMode::UpdateYardBodies()
{
	const FGutStableState& State = GetStableState();

	// only while the yard's own pages are up: at the Block it is other people who are looked at, and nobody stands about over the dead
	const bool bWanted = IsAtStable() && Phase == EGutFightPhase::Card && SpectatorCamera && bSpectatorPlaced && PlayerFighter
		&& StablePage != EGutStablePage::Block && StablePage != EGutStablePage::Body;

	// The two the cage keeps standing ready are nobody's on any of the owner's pages: they are not the owner's people and not
	// yet anybody's opponent. Left in the picture they stood in their guard at either side of the owner's own, and the yard
	// looked like four strangers staring (22 November). They are back the moment a fight is made.
	const bool bCageOut = IsAtStable() && Phase == EGutFightPhase::Card;

	for (AGutFighter* Ready : { PlayerFighter.Get(), EnemyFighter.Get() })
	{
		if (Ready)
		{
			Ready->SetLeftOutOfPicture(bCageOut);
		}
	}

	FString Wanted;

	if (bWanted)
	{
		for (const FGutStableFighter& Fighter : State.Fighters)
		{
			Wanted += FString::Printf(TEXT("%s|%s|%d;"), *Fighter.FighterName.ToString(), *Fighter.Body.ToString(), Fighter.bOwned ? 1 : 0);
		}
	}

	if (Wanted == YardStood)
	{
		return;
	}

	for (AGutFighter* Body : YardBodies)
	{
		if (Body)
		{
			Body->Destroy();
		}
	}

	YardBodies.Reset();
	YardStood = Wanted;

	if (!bWanted)
	{
		return;
	}

	// the same people the street is made of: they stand, and do nothing else
	UClass* Kind = TSoftClassPtr<AGutFighter>(FSoftObjectPath(GutStableConstants::YardBodyClass)).LoadSynchronous();

	if (!Kind && EnemyFighter)
	{
		Kind = EnemyFighter->GetClass();
	}

	// to either side of the card as the owner looks at it, the first two nearer and the next two behind and outside them
	const FVector Eye = SpectatorCamera->GetActorLocation();

	FVector Ahead = (SpectatorLookAt - Eye).GetSafeNormal2D();

	if (Ahead.IsNearlyZero())
	{
		Ahead = FVector::RightVector;
	}

	const FVector Aside = FVector::CrossProduct(FVector::UpVector, Ahead);

	for (int32 i = 0; i < State.Fighters.Num(); ++i)
	{
		const FGutStableFighter& Who = State.Fighters[i];

		const int32 Row = FMath::Min(i / 2, static_cast<int32>(UE_ARRAY_COUNT(GutStableConstants::YardDepth)) - 1);

		FVector Where = Eye + (Ahead * GutStableConstants::YardDepth[Row]) + (Aside * GutStableConstants::YardAside[Row] * ((i % 2) == 0 ? -1.0f : 1.0f));

		// inside the pool, on its floor
		Where.X = FMath::Clamp(Where.X, -GutStableConstants::YardHalf.X, GutStableConstants::YardHalf.X);
		Where.Y = FMath::Clamp(Where.Y, -GutStableConstants::YardHalf.Y, GutStableConstants::YardHalf.Y);
		Where.Z = PlayerFighter->GetActorLocation().Z;

		const FTransform Stood(FRotator(0.0f, (Eye - Where).Rotation().Yaw, 0.0f), Where);

		AGutFighter* Body = Kind ? GetWorld()->SpawnActorDeferred<AGutFighter>(Kind, Stood, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;

		// a place is kept for each, so that the bodies and the stable stay in step
		YardBodies.Add(Body);

		if (!Body)
		{
			continue;
		}

		if (!Who.Body.IsNull())
		{
			Body->SetBodyClass(Who.Body);
		}

		UGameplayStatics::FinishSpawningActor(Body, Stood);

		// with what they fight with in hand and what they have been given to wear on them: they were stood bare, and an
		// owner looking at the pit boss's bill could not see what his fighter would go in with ("why isn't he displayed
		// holding his weapon?", 25 November)
		Body->EquipWeapon(Who.Weapon);

		for (const FName& Name : Who.Armor)
		{
			if (UGutArmorData* Piece = FindArmorPiece(Name))
			{
				Body->WearArmor(Piece);
			}
		}

		Body->SetCollared(Who.bOwned);
		Body->SetAtEase(true);
		Body->SetCombatEnabled(false);
		Body->GetCharacterMovement()->DisableMovement();
	}

	UE_LOG(LogGut, Log, TEXT("Stable: %d stood in the yard"), YardBodies.Num());
}

void AGutGameMode::SettleStableFight(const AGutFighter* DeadFighter)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	FGutStableState& State = GameInstance->Stable;

	StableSettlement = FGutStableSettlement();
	StableSettlement.bSettled = true;
	StableSettlement.bWon = DeadFighter != PlayerFighter;
	StableSettlement.bLayDown = bStableLayDown;
	StableSettlement.Backed = BetSide == EGutBetSide::None ? -1 : (BetSide == EGutBetSide::Player ? 0 : 1);
	StableSettlement.Stake = BetStake;

	if (!State.Fighters.IsValidIndex(State.Matched))
	{
		return;
	}

	FGutStableFighter& Fighter = State.Fighters[State.Matched];

	StableSettlement.FighterName = Fighter.FighterName;
	StableSettlement.bWoman = UGutSoundSubsystem::WhoIs(this, Fighter.Body).bWoman;

	// the window pays whoever backed the one still standing, the owner like anybody else
	if (StableSettlement.Backed == (StableSettlement.bWon ? 0 : 1))
	{
		StableSettlement.Return = BetStake * BetReturnPerLitre;

		State.Water += StableSettlement.Return;
	}

	const float WaterBefore = State.Water - StableSettlement.Return;

	Fighter.bDoped = false;
	Fighter.bFed = false;
	Fighter.bSteadied = false;

	if (StableSettlement.bWon)
	{
		StableSettlement.Purse = GetStablePurse(State.MatchedAgainst);

		// somebody with a tag fights for a share
		StableSettlement.FightersShare = Fighter.bOwned ? 0.0f : FMath::RoundToFloat(StableSettlement.Purse * SignedPurseShare);

		State.Water += StableSettlement.Purse - StableSettlement.FightersShare;
		State.Purses += StableSettlement.Purse - StableSettlement.FightersShare;

		State.Dead.AddUnique(State.MatchedAgainst);

		// dead for the window and the ladder too
		RecordCageFight(-1, State.MatchedAgainst, true);

		++State.FightsWon;
		++State.Name;
		++Fighter.Wins;

		// it walks out in whatever state the fight left it
		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			Fighter.LimbHealth[i] = PlayerFighter->GetLimbHealth(static_cast<EGutLimb>(i));
		}

		// what it holds is what it walked out holding
		if (!PlayerFighter->GetWeapon())
		{
			Fighter.Weapon = nullptr;
		}

		StableSettlement.bTagDue = Fighter.bPromisedTag && Fighter.Wins - Fighter.WinsAtPromise >= WinsForTag;
	}
	else
	{
		++State.FightersLost;

		// a collared fighter's body is its owner's, like the rest of it. Somebody with a tag goes home to whoever is waiting.
		State.bBodyWaiting = Fighter.bOwned;
		State.BodyOf = Fighter.bOwned ? Fighter.FighterName : FText();
		State.bBodyWoman = UGutSoundSubsystem::WhoIs(this, Fighter.Body).bWoman;
		State.BodySince = State.Day;
		State.bBodyBroken = Fighter.IsCrippled();

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			State.bBodyBroken |= PlayerFighter->GetLimbHealth(static_cast<EGutLimb>(i)) <= 0.0f;
		}

		State.Fighters.RemoveAt(State.Matched);
	}

	State.Matched = -1;

	// every debt gets paid: of what this fight brought in, the purse and the window's payment together, a share goes to what is owed
	const float CameIn = State.Water - WaterBefore;

	if (State.Debt > 0.0f && CameIn > 0.0f)
	{
		StableSettlement.DebtPaid = FMath::Min(State.Debt, FMath::RoundToFloat(CameIn * DebtShareOfWinnings));

		State.Debt -= StableSettlement.DebtPaid;
		State.Water -= StableSettlement.DebtPaid;
	}

	// and Mother Ledger's note says the same of her: half of anything the window or the pit pays is kept against it. Until
	// 25 November that was true only of a bet won at the window: an owner's purse went by her. What the yard's own debt
	// did not take of the share is hers. (Not where each way of playing keeps its own water, as the tests do.)
	if (FGutWindowState& Hers = GameInstance->Window; GameInstance->IsOnePurse() && Hers.bStarted && Hers.Debt > 0.5f && CameIn > 0.0f)
	{
		const float Kept = FMath::Min(Hers.Debt, FMath::RoundToFloat(CameIn * DebtShareOfWinnings) - StableSettlement.DebtPaid);

		if (Kept > 0.5f)
		{
			Hers.Debt -= Kept;
			State.Water -= Kept;

			StableSettlement.DebtPaid += Kept;

			GameInstance->WriteDebt(FString::Printf(TEXT("kept at the pit, of %.0f L won"), CameIn), -Kept, -Kept, Hers.Debt);
		}
	}

	// whoever beat the yard's fighter has that to show the book
	if (!StableSettlement.bWon)
	{
		RecordCageFight(State.MatchedAgainst, -1, false);
	}

	// and for whoever the cage has lost, somebody new is brought in: nobody comes back
	RefillRoster();

	UE_LOG(LogGut, Log, TEXT("Stable: %s %s. Purse %.0f L, the window pays %.0f L, %.0f L in hand"), *StableSettlement.FighterName.ToString(),
		StableSettlement.bWon ? TEXT("walks out") : (StableSettlement.bLayDown ? TEXT("lay down and is dead") : TEXT("is dead")),
		StableSettlement.Purse - StableSettlement.FightersShare, StableSettlement.Return, State.Water);

	// a fight takes the day
	AdvanceStableDay();
}

void AGutGameMode::GetStableCardItems(TArray<FGutCardItem>& Items) const
{
	using namespace GutStableLines;

	const FGutStableState& State = GetStableState();

	auto Line = [&Items](int32 Code) { Items.Add({ EGutCardItemType::Stable, Code }); };

	switch (StablePage)
	{
	case EGutStablePage::Fighter:

		if (State.Fighters.IsValidIndex(StableFocus))
		{
			const FGutStableFighter& Fighter = State.Fighters[StableFocus];

			Line(TrainStrength);
			Line(TrainSpeed);
			Line(TrainNerve);
			Line(TrainCunning);

			if (Fighter.IsCrippled())
			{
				Line(Patch);
			}

			Line(Dope);

			if (Fighter.bOwned)
			{
				if (IsTagDue(StableFocus))
				{
					Line(HandOverTag);
					Line(DenyTag);
				}
				else if (!Fighter.bPromisedTag && !Fighter.bBetrayed)
				{
					Line(Promise);
				}
			}

			// somebody who came in off the street can go back out on it with this one behind them
			if (const UGutGameInstance* GameInstance = GetGutGameInstance(); CameByDoor() && GameInstance && GameInstance->IsStreetOpen())
			{
				Line(WalkOut);
			}

			Line(Sell);
		}

		Line(Back);
		break;

	case EGutStablePage::Block:
	{
		TArray<FGutStableFighter> Offers;
		GetBlockOffers(Offers);

		for (int32 i = 0; i < Offers.Num(); ++i)
		{
			if (!State.BoughtToday.Contains(i))
			{
				Line(Buy + i);
			}
		}

		Line(Back);
		break;
	}

	case EGutStablePage::Bill:
	{
		TArray<int32> Against;
		GetStableOffers(Against);

		for (int32 i = 0; i < Against.Num(); ++i)
		{
			Line(TakeFight + i);
		}

		Line(Back);
		break;
	}

	case EGutStablePage::Pick:

		for (int32 i = 0; i < State.Fighters.Num(); ++i)
		{
			Line(SendFighter + i);
		}

		Line(Back);
		break;

	case EGutStablePage::Body:

		// no way back from this page: it is seen to, one way or another
		Line(BodyToGround);
		Line(BodyToParts);
		Line(BodyToGrill);
		Line(BodyToJerky);
		break;

	default:

		for (int32 i = 0; i < State.Fighters.Num(); ++i)
		{
			Line(OpenFighter + i);
		}

		Line(ToBlock);

		if (!State.Fighters.IsEmpty())
		{
			Line(ToBill);
		}

		Line(Wait);

		// the street, from the yard's own page: with any one of them behind, with all of them, or (the door's own line, added
		// after these) alone
		if (const UGutGameInstance* GameInstance = GetGutGameInstance(); CameByDoor() && GameInstance && GameInstance->IsStreetOpen())
		{
			for (int32 i = 0; i < State.Fighters.Num(); ++i)
			{
				Line(WalkWith + i);
			}

			if (State.Fighters.Num() > 1)
			{
				Line(WalkOutAll);
			}
		}

		break;
	}
}

void AGutGameMode::ActivateStableLine(int32 Code)
{
	using namespace GutStableLines;

	auto GoTo = [this](EGutStablePage Page)
	{
		StablePage = Page;
		CardSelection = FGutCardItem();

		if (BackToWalkedYard())
		{
			return;
		}

		// the first line of the new page
		TArray<FGutCardItem> Items;
		GetStableCardItems(Items);

		if (!Items.IsEmpty())
		{
			CardSelection = Items[0];
		}
	};

	if (Code >= WalkWith || Code == WalkOutAll)
	{
		// they follow whoever holds their paper, or pays them, until they are brought back in at the Gut
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			for (int32 i = 0; i < GameInstance->Stable.Fighters.Num(); ++i)
			{
				if (Code == WalkOutAll || i == Code - WalkWith)
				{
					GameInstance->Stable.Fighters[i].bInTow = true;

					UE_LOG(LogGut, Log, TEXT("Stable: out of the yard gate with %s"), *GameInstance->Stable.Fighters[i].FighterName.ToString());
				}
			}

			GoToDoor();
		}

		return;
	}

	if (Code >= SendFighter)
	{
		TArray<int32> Against;
		GetStableOffers(Against);

		if (Against.IsValidIndex(StablePicked))
		{
			MakeMatch(Code - SendFighter, Against[StablePicked]);
		}

		return;
	}

	if (Code >= TakeFight)
	{
		StablePicked = Code - TakeFight;

		GoTo(EGutStablePage::Pick);
		return;
	}

	if (Code >= Buy)
	{
		BuyFighter(Code - Buy);
		KeepStableChoice();
		return;
	}

	if (Code >= OpenFighter)
	{
		StableFocus = Code - OpenFighter;

		GoTo(EGutStablePage::Fighter);
		return;
	}

	switch (Code)
	{
	case ToBlock:
		GoTo(EGutStablePage::Block);
		break;

	case ToBill:
		GoTo(EGutStablePage::Bill);
		break;

	case Wait:
		RestDay();
		break;

	case TrainStrength:
		TrainFighter(StableFocus, EGutTraining::Strength);
		break;

	case TrainSpeed:
		TrainFighter(StableFocus, EGutTraining::Speed);
		break;

	case TrainNerve:
		TrainFighter(StableFocus, EGutTraining::Nerve);
		break;

	case TrainCunning:
		TrainFighter(StableFocus, EGutTraining::Cunning);
		break;

	case Patch:
		PatchFighter(StableFocus);
		break;

	case Dope:
		DopeFighter(StableFocus);
		break;

	case Promise:
		PromiseTag(StableFocus);
		break;

	case HandOverTag:
		GiveTag(StableFocus);
		GoTo(EGutStablePage::Yard);
		break;

	case DenyTag:
		BreakPromise(StableFocus);
		break;

	case Sell:
		SellFighter(StableFocus);
		GoTo(EGutStablePage::Yard);
		break;

	case WalkOut:

		// it follows whoever holds its paper, or pays it, until it is brought back in at the Gut
		if (UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && GameInstance->Stable.Fighters.IsValidIndex(StableFocus))
		{
			GameInstance->Stable.Fighters[StableFocus].bInTow = true;

			UE_LOG(LogGut, Log, TEXT("Stable: out of the yard gate with %s"), *GameInstance->Stable.Fighters[StableFocus].FighterName.ToString());

			GoTo(EGutStablePage::Yard);
			GoToDoor();
		}

		return;

	case BodyToParts:
	case BodyToGrill:
	case BodyToGround:
	case BodyToJerky:

		if (SendBodyTo(Code == BodyToParts ? EGutBodyGoesTo::Parts : (Code == BodyToGrill ? EGutBodyGoesTo::Grill : (Code == BodyToJerky ? EGutBodyGoesTo::Jerky : EGutBodyGoesTo::Ground))))
		{
			GoTo(bBillAfterBody ? EGutStablePage::Bill : EGutStablePage::Yard);

			bBillAfterBody = false;
		}

		break;

	default:

		// the dead are not left lying while the owner looks at something else
		if (StablePage == EGutStablePage::Body)
		{
			break;
		}

		GoTo(StablePage == EGutStablePage::Pick ? EGutStablePage::Bill : EGutStablePage::Yard);
		break;
	}

	// a fighter that is gone in the night, or a day that has turned, leaves nothing to stand on this page for
	if (StablePage == EGutStablePage::Fighter && !GetStableState().Fighters.IsValidIndex(StableFocus))
	{
		GoTo(EGutStablePage::Yard);
	}

	KeepStableChoice();
}

void AGutGameMode::KeepStableChoice()
{
	// a line that has gone, like somebody at the Block once bought, leaves the choice on the page's first line
	TArray<FGutCardItem> Items;
	GetStableCardItems(Items);

	if (Phase == EGutFightPhase::Card && !Items.Contains(CardSelection) && !Items.IsEmpty())
	{
		CardSelection = Items[0];
	}
}
