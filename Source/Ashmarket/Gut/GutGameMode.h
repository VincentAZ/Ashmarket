// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GutTypes.h"
#include "GutStableTypes.h"
#include "GutWindowTypes.h"
#include "GutGameMode.generated.h"

class AGutCrowd;
class AGutFighter;
class AGutHazard;
class AGutWeaponRack;
class UGutArmorData;
class UGutBeastData;
class UGutGameInstance;
class UGutLadderData;
class UGutWeaponData;
struct FGutLadderRung;
struct FGutLadderState;
struct FGutSponsor;
struct FGutLadderRung;

/** The kinds of thing that can be chosen on the card before a ladder fight */
enum class EGutCardItemType : uint8
{
	/** Have the cutter patch a crippled limb */
	Patch,

	/** Take, or hand back, the sponsor's offer for this fight */
	Sponsor,

	/** Buy a piece of armor */
	Armor,

	/** In the owner's yard: a line of whichever page of the card is up. The index says what it does: see GutStableLines. */
	Stable,

	/** Take Dust or Hush for this fight. The index is which: see EGutDrug. */
	Drug,

	/** Outside the Gut: a way in, or on a card inside it, the way back out. The index says which: see GutDoorLines. */
	Door,

	/** Take, or hand back, the pit boss's special card for this fight */
	Special,

	/** At the window: arrange for one of the two fighters to lose. The index is the kind of fix times two, plus the corner. */
	Fix,

	/** Leave the card for the rack */
	Rack
};

/** The lines outside the Gut (design document, chapter 7: the player can enter as fighter, owner or gambler) */
namespace GutDoorLines
{
	enum
	{
		/** Through the pens, in a collar: the ladder */
		Fight = 0,

		/** Through the yard gate: a stable */
		Own = 1,

		/** Up the stairs: the window */
		Bet = 2,

		/** A single fight with nothing riding on it */
		Spar = 3,

		/** On a card inside: back out to the street */
		Leave = 9,

		/** Plus one of the three ways in: forget how far that one had got, and go in again from the start */
		Afresh = 10
	};
}

/** One line that can be chosen on the card */
struct FGutCardItem
{
	EGutCardItemType Type = EGutCardItemType::Rack;

	/** For armor, the piece's place in the rank's shop */
	int32 Index = 0;

	bool operator==(const FGutCardItem& Other) const { return Type == Other.Type && Index == Other.Index; }
};

/** What one ladder fight came to in water, for the HUD to read out */
struct FGutSettlement
{
	/** If true a ladder fight has been settled and the rest of this is meaningful */
	bool bSettled = false;

	bool bPlayerWon = false;

	/** If true the player lost but the opponent would not finish it */
	bool bSpared = false;

	/** Purse paid to the player */
	float Purse = 0.0f;

	/** What the player staked at the window, and on whom */
	float Stake = 0.0f;
	EGutBetSide BetSide = EGutBetSide::None;

	/** What the window paid back, stake included. Zero for a losing bet. */
	float BetReturn = 0.0f;

	/** If true the player killed with the finish the crowd was calling for */
	bool bCrowdCallMatched = false;

	/** What the crowd threw in for it */
	float CrowdBonus = 0.0f;

	/** The player's cut of the gate */
	float GateCut = 0.0f;

	/** What the pit boss kept out of the purse against the player's debt */
	float DebtPaid = 0.0f;

	/** Name gained */
	int32 NameGained = 0;

	/** If true the player had taken a sponsor's offer for this fight */
	bool bSponsored = false;

	/** What the sponsor paid. Zero if its demand was not met. */
	float SponsorPay = 0.0f;

	/** True if the player went down in a fight it was paid to lose, and was left alive as arranged */
	bool bThrown = false;

	/** Water owed back to a sponsor whose bought fight the player won */
	float SponsorOwed = 0.0f;

	/** Name lost with the crowd for going down easy */
	int32 NameLost = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGutPhaseSignature, EGutFightPhase, NewPhase);

/**
 *  Runs one cage fight between the player and one opponent placed in the level:
 *  the weapon rack, a short intro, the fight, and a result when one of them is dead.
 *
 *  The fight runs on without a break, but its clock is cut into rounds. Each hazard placed in the level goes live
 *  in the round set on it, after a warning, and stays live until the fight is over.
 *
 *  The level can be opened with options to set what is drawn from the rack to begin with, for example
 *  /Game/Gut/Lvl_Gut?PlayerWeapon=Chain?EnemyWeapon=Shield. Each is any part of a weapon asset's name.
 *  A rematch uses them to come back to the rack with the same two weapons drawn.
 *  ?Hazards=0 opens it with every hazard switched off for the whole fight.
 *
 *  If a ladder is set, the cage runs it: one fight after another against the opponents on its rungs, each with a card before it,
 *  the rack, a betting window, and water changing hands after, until the player is at the gate. Injuries and water are carried
 *  from fight to fight by the game instance. ?Sandbox=1 opens the cage for single fights instead, with no ladder.
 */
UCLASS(abstract)
class AGutGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/**
	 *  The weapons on the rack. Each fighter draws one before the bell.
	 *  Leave empty for a fight with no rack: the fighters then keep the weapons they start with.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack")
	TArray<TObjectPtr<UGutWeaponData>> RackWeapons;

	/** The weapon the opponent draws. Leave empty for a random draw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack")
	TObjectPtr<UGutWeaponData> OpponentWeapon;

	/**
	 *  The run of fights the cage is running, or the first rank of it: each ladder can name the one that follows.
	 *  Leave empty for single fights only.
	 *  Needs the project's game instance to be a Gut Game Instance, which remembers the run between fights.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Ladder")
	TObjectPtr<UGutLadderData> Ladder;

	/** The rank from which Dust and Hush are sold at the card, from zero. Below it the player is property and is given nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0))
	int32 DrugsFromRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0))
	float DustPrice = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0))
	float HushPrice = 8.0f;

	/** What Dust does for one fight: ground covered, and how hard a blow lands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 1))
	float DustSpeedScale = 1.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 1))
	float DustDamageScale = 1.2f;

	/** What Hush does for one fight: how long a blow that lands stops the fighter, against what the attack says */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float HushStunScale = 0.5f;

	/** How much of a habit one dose is, and how much of it a fight without one takes off. Dust takes hold fast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float DustHabitPerDose = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float HushHabitPerDose = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float HabitLostPerFight = 0.08f;

	/** The habit at which going without is felt: without Dust a step slower and lighter in the hand, without Hush half the health */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float WithdrawalHabit = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float DustWithdrawalScale = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0, ClampMax = 1))
	float HushWithdrawalHealthScale = 0.5f;

	/** Health a dose of Dust on top of a full habit takes off the heart for good */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drugs", meta = (ClampMin = 0))
	float DustHeartDamage = 25.0f;

	/** Water an owner comes to the cage with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 0))
	float StableStartingWater = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 1, ClampMax = 8))
	int32 StableMostFighters = 4;

	/** How far a fighter can be taught in each of the three things */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 1, ClampMax = 10))
	int32 StableMostTaught = 5;

	/** People at the Block each day, the last of them somebody who will sign, and fights on offer each day */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 1, ClampMax = 6))
	int32 BlockOffersPerDay = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 1, ClampMax = 6))
	int32 StableOffersPerDay = 3;

	/** What somebody costs at the Block with nothing taught, and what each thing taught adds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float BlockBasePrice = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float BlockPricePerSkill = 6.0f;

	/** What somebody with a tag is paid each day, before anything for what it knows, and the share of a purse it keeps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float SignedBaseWage = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0, ClampMax = 1))
	float SignedPurseShare = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float FeedPerDay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float TrainingCost = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float StablePatchCost = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float DopeCost = 5.0f;

	/** What a tag costs at the Pump office */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float TagPrice = 30.0f;

	/** What Doc Stitch gives for a dead fighter (half for one that lost a limb first), what the grill carts give by weight, and what it costs to put one in the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float BodyPartsPrice = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float BodyGrillPrice = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float BurialCost = 2.0f;

	/** An owner's purse against a ladder fighter's own purse for the same fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Costs", meta = (ClampMin = 0))
	float StablePurseScale = 2.0f;

	/** Wins after the promise before the tag is due */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable", meta = (ClampMin = 1))
	int32 WinsForTag = 3;

	/** What being taught is worth in the cage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Fight", meta = (ClampMin = 1))
	float StableBaseHealth = 115.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Fight", meta = (ClampMin = 0))
	float HealthPerStrength = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Fight", meta = (ClampMin = 0))
	float DamagePerStrength = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Fight", meta = (ClampMin = 0))
	float MovePerSpeed = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Fight", meta = (ClampMin = 0))
	float StaminaPerSpeed = 0.08f;

	/** What dope is worth: this much strength and speed for one fight, and this much habit, which wears off by the day */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Dope", meta = (ClampMin = 0))
	int32 DopeEdge = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Dope", meta = (ClampMin = 0, ClampMax = 1))
	float DopeHabit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Dope", meta = (ClampMin = 0, ClampMax = 1))
	float HabitLostPerDay = 0.04f;

	/** What raises a collared fighter's resentment, and what eases it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForDope = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForSentInHurt = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForSale = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForHunger = 10.0f;

	/** The yard knows where its dead went: cut up for Doc Stitch, onto the carts, or into the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForBodyToParts = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForBodyToGrill = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentEasedByBurial = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentForBrokenPromise = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentEasedPerDay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentEasedByCare = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentEasedByPromise = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float ResentmentEasedByTag = 12.0f;

	/** What resentment does: fights worse past the first, may lie down past the second, is gone in the night at the third */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float SulksAtResentment = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float LaysDownAtResentment = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment", meta = (ClampMin = 0, ClampMax = 1))
	float LayDownChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float RunsAtResentment = 95.0f;

	/** A freed fighter that thinks no worse of its owner than this stays on for a wage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stable|Resentment")
	float StaysOnBelowResentment = 30.0f;

	/** Water a gambler comes to the window with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0))
	float WindowStartingWater = 40.0f;

	/** Fights on a night's bill */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 1, ClampMax = 8))
	int32 WindowFightsPerNight = 4;

	/** The ladder rank from which the pit boss has special cards, and what each pays against the purse of the same fight one against one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 0))
	int32 SpecialsFromRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1))
	float OneAgainstThreePurseScale = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1))
	float TwoOnTwoPurseScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1))
	float AnimalPurseScale = 1.25f;

	/** How many of the three may be at the player at once in one against three, and how much of their health the two extra ones come in with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1, ClampMax = 3))
	int32 MostOnOneOfThree = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 0.1, ClampMax = 1))
	float ThreeExtraHealthScale = 0.15f;

	/** If true the two extra ones come in as they were sold, with nothing in their hands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special")
	bool bThreeExtrasUnarmed = true;

	/** The pit boss's dogs: what is put in against the player on the card of that name, and let in on a fight that will not end */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special")
	TObjectPtr<UGutBeastData> Dog;

	/** How many of them the card puts in against the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1, ClampMax = 4))
	int32 DogsOnCard = 2;

	/** The round in which the dogs are let in on a fight that is still going, and how many. Round 0: never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 0, ClampMax = 20))
	int32 DogsRound = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1, ClampMax = 4))
	int32 DogsLoosed = 2;

	/** How many of one side may be attacking the same fighter at once. The rest hold off and wait for room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 1, ClampMax = 4))
	int32 MostOnOne = 2;

	/** How far a fighter's target has to be beaten for nearness before it turns to somebody else */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Special", meta = (ClampMin = 0, Units = "cm"))
	float RetargetMargin = 120.0f;

	/** What a fighter wants for going down, as a multiple of the purse it is fighting for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0))
	float BribePurseScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0))
	float DrugBucketCost = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0))
	float TamperRackCost = 6.0f;

	/** The chance each fix is noticed, before the pit boss has any reason to look */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float BribeDetectChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float DrugBucketDetectChance = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float TamperRackDetectChance = 0.2f;

	/** How much harder the gambler is looked at after a fix that got by, after one that did not, and how much of that wears off each night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float HeatPerFix = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float HeatWhenCaught = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0, ClampMax = 1))
	float HeatLostPerNight = 0.15f;

	/** What a caught fixer pays on the spot, besides losing the stake and going into the cage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0))
	float FixFine = 15.0f;

	/** How a drugged fighter is slowed: its stamina comes back at this share of the usual rate, and it takes this many times as long to react */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 0.05, ClampMax = 1))
	float DruggedStaminaScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window|Fixes", meta = (ClampMin = 1, ClampMax = 4))
	float DruggedReactionScale = 1.7f;

	/** What the lender puts in the hand of a gambler with nothing left to stake, and what that adds to the debt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0))
	float LenderStake = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0))
	float LenderOwed = 35.0f;

	/**
	 *  Every debt gets paid (Market Law 2). Of anything the window or the pit pays to somebody who owes, at the window or in
	 *  the yard, this share is kept against the debt. And the most the lender will let a gambler owe: cleaned out past that,
	 *  the debt is paid in the cage, and walking out alive clears it (Law 4).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0, ClampMax = 1))
	float DebtShareOfWinnings = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0))
	float LenderLimit = 105.0f;

	/** The rank a caught fixer's opponent is drawn from, from zero */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Window", meta = (ClampMin = 0))
	int32 CaughtFixerRank = 1;

	/** Time between the weapons being drawn and the bell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float IntroDuration = 2.5f;

	/** Length of one round. The clock runs on from one round to the next without a break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rounds", meta = (ClampMin = 1, ClampMax = 600, Units = "s"))
	float RoundLength = 30.0f;

	/** Time before a hazard goes live that it starts to show it is coming */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rounds", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float HazardWarningTime = 3.0f;

	/** If false every hazard stays off for the whole fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rounds")
	bool bHazardsEnabled = true;

	/** Time after the fight ends before it can be restarted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float RestartDelay = 1.5f;

	/** What up and down change a stake by at any betting screen, and the least that can be staked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Ladder", meta = (ClampMin = 1))
	float StakeStep = 1.0f;

	/** Current stage of the fight */
	EGutFightPhase Phase = EGutFightPhase::Choosing;

	/** Position on the rack of the weapon the player has drawn */
	int32 PlayerChoice = 0;

	/** Position on the rack of the weapon the opponent has drawn */
	int32 EnemyChoice = 0;

	/** If true the fighters took their weapons from the rack, so a rematch should offer the same two again */
	bool bWeaponsDrawn = false;

	/** Weapon names asked for when the level was opened. Empty if none were. */
	FString RequestedPlayerWeapon;
	FString RequestedEnemyWeapon;

	/** For single fights: 3 for one against three, 2 for two on two, from the level's options */
	int32 RequestedSpecial = 0;

	/** Who the player's bet on this fight is on */
	EGutBetSide BetSide = EGutBetSide::None;

	/** Water the player has staked, or means to stake, on this fight */
	float BetStake = 0.0f;

	/** What a litre of the player's bet comes back as if it wins, fixed when the bet was placed */
	float BetReturnPerLitre = 0.0f;

	/** What the fight came to in water, once it is over */
	FGutSettlement Settlement;

	/** The line chosen on the card */
	FGutCardItem CardSelection;

	/** If true the player has taken the sponsor's offer for this fight */
	bool bSponsorAccepted = false;

	/** If true the crowd is calling for a finisher style in this fight */
	bool bCrowdCalling = false;

	/** The style it is calling for */
	EGutFinisherStyle CrowdCall = EGutFinisherStyle::Clean;

	/** The hazards in the level */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGutHazard>> Hazards;

	/** The rack in the level, if there is one. Only for show. */
	UPROPERTY(Transient)
	TObjectPtr<AGutWeaponRack> Rack;

	/** The crowd on the deck, if the level has one. Only for show. */
	UPROPERTY(Transient)
	TObjectPtr<AGutCrowd> Crowd;

	/** Time spent in the current stage */
	float PhaseTime = 0.0f;

	/** Time from the bell to the first death. Keeps counting while the fight is running. */
	float FightTime = 0.0f;

	/** If true both fighters have been found and introduced to each other */
	bool bFightersReady = false;

	/** The player's fighter */
	UPROPERTY(Transient)
	TObjectPtr<AGutFighter> PlayerFighter;

	/** The opponent */
	UPROPERTY(Transient)
	TObjectPtr<AGutFighter> EnemyFighter;

	/** In a special card, everybody else in the cage, both sides */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGutFighter>> ExtraFighters;

	/** What kind of fight is in the cage, and whether the player took the special card it was offered */
	EGutSpecialCard SpecialCard = EGutSpecialCard::None;
	bool bSpecialAccepted = false;

	/** True once the dogs have been let into a fight between people: they are on nobody's side */
	bool bDogsLoose = false;

	/** True while the menu that stops the game is up, and the line it is on */
	bool bStoppedMenu = false;
	int32 StoppedChoice = 0;

	/** The part of the story being told in pictures, if one is, and the picture it has got to */
	TArray<FGutStoryPanel> Story;
	int32 StoryIndex = INDEX_NONE;

	/** True if stories are told at all: not in a run nobody is watching, unless -GutStory asks */
	bool bStories = true;

	/** Brings one more fighter into the cage for a special card */
	AGutFighter* SpawnExtra(int32 Side, const FVector& Where, const FGutLadderRung* As, const FText& Name, UGutBeastData* AsBeast = nullptr);

	/** In a fight of more than two, turns each fighter toward the one it should be fighting */
	void UpdateSpecialTargets();

	/** The owner's card: which page is up, which fighter it is about, and which fight on the bill was picked */
	EGutStablePage StablePage = EGutStablePage::Yard;
	int32 StableFocus = 0;
	int32 StablePicked = 0;

	FGutStableSettlement StableSettlement;

	/** True if the stable's fighter in the cage has decided to lose */
	bool bStableLayDown = false;

	float NextStableRoll = -1.0f;

	/** What the gambler has arranged for the fight on the card, and to which corner */
	EGutFix WindowFix = EGutFix::None;
	int32 WindowFixCorner = 0;

	FGutWindowSettlement WindowSettlement;

	/** If not negative, what the next roll for being noticed comes up as */
	float NextWindowRoll = -1.0f;

	/** The camera the gambler watches from, and what it is looking at */
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> SpectatorCamera;

	FVector SpectatorLookAt = FVector::ZeroVector;

	bool bSpectatorPlaced = false;

	/** True if the first corner's fighter is on the right of the picture somebody is watching from the wall */
	bool bFirstCornerOnRight = false;

	/** The stake the betting screen is set to while it is on nobody: see GetBetWanted */
	float BetWanted = 0.0f;

	/** If true whoever watched a fight to its end has the arrow on the street: see CanLeaveForStreet */
	bool bLeaveChosen = false;

public:

	/** Called when the fight moves to a new stage */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutPhaseSignature OnPhaseChanged;

public:

	/** Constructor */
	AGutGameMode();

	/** Reads which weapons to start with from the level options */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Runs the intro timer */
	virtual void Tick(float DeltaTime) override;

	/** Moves the player's draw along the rack by the given number of places, wrapping round. Only at the rack. */
	UFUNCTION(BlueprintCallable, Category="Gut|Rack")
	void CyclePlayerChoice(int32 Step);

	/** Moves the opponent's draw along the rack by the given number of places, wrapping round. Only at the rack. */
	UFUNCTION(BlueprintCallable, Category="Gut|Rack")
	void CycleEnemyChoice(int32 Step);

	/** Leaves the rack with the weapons drawn, and starts the intro */
	UFUNCTION(BlueprintCallable, Category="Gut|Rack")
	void ConfirmChoice();

	/** Leaves the rack with nothing drawn from it: both fighters go back to the weapons they start with */
	UFUNCTION(BlueprintCallable, Category="Gut|Rack")
	void SkipRack();

	/** Rings the bell now, skipping whatever is left of the rack and the intro. The fighters fight with what they are holding. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void StartFight();

	/** Switches the hazards on or off for the rest of the fight. Switched off, they stay off whatever the round. */
	UFUNCTION(BlueprintCallable, Category="Gut|Rounds")
	void SetHazardsEnabled(bool bEnabled);

	/** Moves the fight clock on by the given time, as if it had passed. Only while the fight is running. For tests. */
	UFUNCTION(BlueprintCallable, Category="Gut|Rounds")
	void AdvanceFightTime(float Seconds);

	/**
	 *  Reloads the cage for another fight. Only works once the fight is over.
	 *  On a ladder that is the next fight after a win, the same fight again after a death, and the gate after the last one.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void RequestRestart();

	/** Passes an input to whichever screen is up: the card, the rack, the betting window or the gate */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void HandleMenuInput(EGutMenuInput Input);

	/**
	 *  Leaves the ladder for single fights, now and for every fight after, until the ladder is started again.
	 *  If the card or the gate is up the cage goes straight to the rack.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void EnterSandbox();

	/** True if the player is outside, choosing how to come in */
	bool IsAtDoor() const;

	/** The special card the pit boss has for this ladder fight, if any */
	EGutSpecialCard GetSpecialOffer() const;

	/** Takes the special card for this fight, or hands it back. Only at the card. */
	void SetSpecialAccepted(bool bAccepted);

	bool IsSpecialAccepted() const { return bSpecialAccepted; }

	/** What kind of fight is in the cage */
	EGutSpecialCard GetSpecialCard() const { return SpecialCard; }

	/** What a special card's purse is, against the purse of the same fight one against one */
	float GetSpecialPurseScale(EGutSpecialCard Card) const;

	/**
	 *  Makes the fight in the cage a special card: the others are brought in and everybody is given a side.
	 *  Before the bell only. Each of the others is made from the rung given for it, or left as the cage's own fighter is.
	 */
	void BeginSpecial(EGutSpecialCard Card, const FGutLadderRung* Others = nullptr, const FGutLadderRung* Second = nullptr);

	/**
	 *  The menu that stops the game: go on, how loud, out to the street, leave. Up, everything in the cage stands still
	 *  and no other screen takes input. It is put up and taken down by one key (Escape, or the pad's left special button).
	 */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetStopped(bool bStopped);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsStopped() const { return bStoppedMenu; }

	int32 GetStoppedChoice() const { return StoppedChoice; }

	/** The menu's lines, as they stand now */
	TArray<FString> GetStoppedLines() const;

	/** Up, down or confirm, while the menu is up */
	void HandleStoppedInput(EGutMenuInput Input);

	/** Makes the game as loud as the game instance says */
	void ApplyLoudness();

	/**
	 *  Begins a part of the story told in pictures. While it is being told nothing else on the screen takes input:
	 *  each press of the confirm key shows the next picture, and after the last the screen underneath is there again.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void TellStory(EGutStory Which);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsTellingStory() const { return Story.IsValidIndex(StoryIndex); }

	const FGutStoryPanel* GetStoryPanel() const { return Story.IsValidIndex(StoryIndex) ? &Story[StoryIndex] : nullptr; }

	int32 GetStoryIndex() const { return StoryIndex; }

	int32 GetStoryLength() const { return Story.Num(); }

	/** Whether stories are told at all. They are not in a run nobody is watching. */
	void SetStoriesEnabled(bool bEnabled) { bStories = bEnabled; }

	/** The next picture, or the end of them */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void AdvanceStory();

	/** Lets the pit boss's dogs into the fight that is going on. They go for whoever is nearest, and killing them wins nothing. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void LooseDogs(int32 Count);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool AreDogsLoose() const { return bDogsLoose; }

	/** Takes the dogs out again: a new fight, or the hazards switched off */
	void CallOffDogs();

	const UGutBeastData* GetDog() const { return Dog; }

	int32 GetDogsOnCard() const { return DogsOnCard; }

	int32 GetDogsRound() const { return DogsRound; }

	int32 GetDogsLoosed() const { return DogsLoosed; }

	/** Everybody the pit boss brought in for a special card, both sides */
	const TArray<TObjectPtr<AGutFighter>>& GetExtraFighters() const { return ExtraFighters; }

	/** How many on a side are still alive */
	int32 CountLiving(int32 Side) const;

	/** True if the player came in from outside, and so can walk back out from a card */
	bool CameByDoor() const;

	/** Comes in by one of the ways in GutDoorLines, or with Afresh added, forgets that way's run and starts it again */
	void EnterBy(int32 Way);

	/** Walks back out to the street from a card. Whatever run was in hand is kept as it stood. */
	void GoToDoor();

	/**
	 *  The key that backs out of whatever screen is up, where there is somewhere to back out to: from the betting to the
	 *  street with no bet (or to the yard, for an owner), from a card to the street, from a fight that was watched to
	 *  the street. Nobody is kept on a screen because the only key it listens to goes on.
	 */
	void HandleBack();

	/** True if the back key does something on the screen that is up, for the screen to say so */
	bool CanGoBack() const;

	/**
	 *  Comes to the cage to bet, not to fight, from the first night: the fights on the bill are between the ladder's own fighters,
	 *  watched from the wall, with water on them and whatever has been arranged. The seed fixes who is on the bills.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Window")
	void EnterWindow(int32 Seed = 0);

	/**
	 *  Comes to the cage as an owner, from the first day and with no fighters: they are bought at the Block or signed for a wage,
	 *  trained, patched, doped, promised their tags, matched against the ladder's fighters, and watched from the wall.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Stable")
	void EnterStable(int32 Seed = 0);

	/** Buys, or signs, whoever is at that place on today's list at the Block. Returns false if there is no room or no water. */
	bool BuyFighter(int32 OfferIndex);

	/** Sells an owned fighter back at the Block, or lets a signed one go */
	bool SellFighter(int32 FighterIndex);

	/** Has a fighter taught for the day. Costs food, and the day. */
	bool TrainFighter(int32 FighterIndex, EGutTraining What);

	/** Has the cutter see to one crippled limb */
	bool PatchFighter(int32 FighterIndex);

	/** Dopes a fighter for its next fight: an edge now, and a habit later */
	bool DopeFighter(int32 FighterIndex);

	/** Promises an owned fighter its tag for a number of wins, gives it once it is due, or goes back on it */
	bool PromiseTag(int32 FighterIndex);
	bool GiveTag(int32 FighterIndex);
	bool BreakPromise(int32 FighterIndex);

	/** Lets a day go by */
	void RestDay();

	/** Sees to the stable's dead fighter: sold for parts, sold to the grill carts, or buried */
	bool SendBodyTo(EGutBodyGoesTo Where);

	float GetBodyPrice(EGutBodyGoesTo Where) const;

	/** Sends one of the stable's fighters in against one of the ladder's (rank times 100 plus place on its ladder), and goes on to the betting */
	bool MakeMatch(int32 FighterIndex, int32 Against);

	/** For tests: what the next roll for a fighter lying down comes up as, from 0 to 1 */
	void SetNextStableRoll(float Roll) { NextStableRoll = Roll; }

	/** Arranges for the fighter in a corner (0 or 1) to lose the fight on the card, or with None arranges nothing */
	UFUNCTION(BlueprintCallable, Category="Gut|Window")
	void SetWindowFix(EGutFix Fix, int32 Corner);

	/** Turns the gambler's night over: a new bill, the sums begun again. Done on the way out of a night, whichever way that is. */
	void TurnWindowNight();

	/** Leaves the night's reckoning for the next night's first fight */
	UFUNCTION(BlueprintCallable, Category="Gut|Window")
	void LeaveWindowNight();

	/** For tests: what the next roll for being noticed comes up as, from 0 to 1. A fix is noticed if this is under its chance. */
	void SetNextWindowRoll(float Roll) { NextWindowRoll = Roll; }

	/** Puts the player back at the foot of the ladder and reloads the cage for its first fight. The run on disk is forgotten. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void StartNewLadder();

	/**
	 *  Leaves the gate. If another rank follows, the player goes on to its first fight, the bill settled if this rank has one.
	 *  If none does, the ladder starts again from the bottom.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void LeaveGate();

	/** Leaves the card for the rack: the opponent takes on this rung's name and numbers, and the player its carried injuries */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void ConfirmCard();

	/** Moves the choice on the card up or down its lines by the given number of places. Only at the card. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void MoveCardSelection(int32 Step);

	/** Does whatever the chosen line of the card is for. On the last line that is leaving for the rack. Only at the card. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void ActivateCardSelection();

	/** Buys a piece of armor from the rank's shop, to be worn in place of whatever the player had on that part. Only at the card. Returns false if it is already worn or there is no water for it. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	bool BuyArmor(int32 ShopIndex);

	/** Takes the sponsor's offer for this fight, or hands it back. Only at the card. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void SetSponsorAccepted(bool bAccepted);

	/** Takes a dose of Dust or Hush for this fight, on the slate if need be. Returns false if it has already had one or cannot pay. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	bool TakeDrug(EGutDrug Drug);

	/** Pays the cutter to patch the player's worst crippled limb. Only at the card. Returns false if there is nothing to patch or no water for it. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	bool PatchLimb();

	/** Sets the bet the player means to place: who on, and how much. The stake is held to what the window will take. Only at the window. */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void SetBet(EGutBetSide Side, float Stake);

	/** Places the bet, if there is one, and leaves the window for the intro */
	UFUNCTION(BlueprintCallable, Category="Gut|Ladder")
	void ConfirmBet();

public:

	UFUNCTION(BlueprintPure, Category="Gut")
	EGutFightPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetPhaseTime() const { return PhaseTime; }

	/** Returns the time since the bell. Stops when the fight is decided. */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetFightTime() const { return FightTime; }

	/** Returns the round the fight is in, from 1 at the bell. The round stops changing when the fight is decided. */
	UFUNCTION(BlueprintPure, Category="Gut|Rounds")
	int32 GetRound() const;

	/** Returns the time after the bell at which the given round starts */
	UFUNCTION(BlueprintPure, Category="Gut|Rounds")
	float GetRoundStartTime(int32 InRound) const { return (FMath::Max(1, InRound) - 1) * RoundLength; }

	UFUNCTION(BlueprintPure, Category="Gut|Rounds")
	bool AreHazardsEnabled() const { return bHazardsEnabled; }

	const TArray<TObjectPtr<AGutHazard>>& GetHazards() const { return Hazards; }

	/** Returns the time left until the bell */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetIntroTimeRemaining() const;

	/** Returns true if the fight is over and can be restarted */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool CanRestart() const;

	UFUNCTION(BlueprintPure, Category="Gut")
	AGutFighter* GetPlayerFighter() const { return PlayerFighter; }

	UFUNCTION(BlueprintPure, Category="Gut")
	AGutFighter* GetEnemyFighter() const { return EnemyFighter; }

	const TArray<TObjectPtr<UGutWeaponData>>& GetRackWeapons() const { return RackWeapons; }

	/** Returns the position on the rack of the weapon the player has drawn */
	UFUNCTION(BlueprintPure, Category="Gut|Rack")
	int32 GetPlayerChoice() const { return PlayerChoice; }

	/** Returns the position on the rack of the weapon the opponent has drawn */
	UFUNCTION(BlueprintPure, Category="Gut|Rack")
	int32 GetEnemyChoice() const { return EnemyChoice; }

	UFUNCTION(BlueprintPure, Category="Gut|Rack")
	AGutWeaponRack* GetRack() const { return Rack; }

	UFUNCTION(BlueprintPure, Category="Gut")
	AGutCrowd* GetCrowd() const { return Crowd; }

	/** Returns true if this fight is one of a ladder's, not a single fight */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	bool IsOnLadder() const;

	/** True if the player has come as an owner */
	bool IsAtStable() const;

	const FGutStableState& GetStableState() const;

	/** Who is for sale, or will sign, at the Block today */
	void GetBlockOffers(TArray<FGutStableFighter>& OutOffers) const;

	/**
	 *  The same for a stable that stands as the one given does. It reads nothing but the cage's own rules, so the Block's
	 *  clerk in the town can ask it of the cage's defaults without the cage being open.
	 */
	void GetBlockOffersFor(const FGutStableState& State, TArray<FGutStableFighter>& OutOffers) const;

	/** Makes a stable of what an owner comes with, for somebody who had none */
	void BeginStableState(FGutStableState& State, int32 Seed) const;

	/**
	 *  Writes whoever is at that place on the day's list into the stable, and takes their price out of its water.
	 *  Returns false, and says why, if they are bought already, the yard is full, or there is not the water.
	 */
	bool TakeBlockOffer(FGutStableState& State, const FGutStableFighter& Offer, int32 OfferIndex, FString* Why = nullptr) const;

	float GetStableStartingWater() const { return StableStartingWater; }

	/** The fights on offer today: each opponent as rank times 100 plus place on its ladder */
	void GetStableOffers(TArray<int32>& OutAgainst) const;

	/** The ladder fighter, and its rank, for such a number */
	const FGutLadderRung* FindRung(int32 Code) const;
	const UGutLadderData* FindRank(int32 Code) const;

	/** What the owner is paid if its fighter beats that opponent */
	float GetStablePurse(int32 Against) const;

	/** What the book makes the chance of the stable's fighter in the fight that has been made */
	float GetStableBookChance() const;

	/** True if a fighter has won what it was promised its tag for */
	bool IsTagDue(int32 FighterIndex) const;

	const FGutStableSettlement& GetStableSettlement() const { return StableSettlement; }

	EGutStablePage GetStablePage() const { return StablePage; }
	int32 GetStableFocus() const { return StableFocus; }
	int32 GetStablePicked() const { return StablePicked; }

	float GetTrainingCost() const { return TrainingCost; }
	float GetStablePatchCost() const { return StablePatchCost; }
	float GetDopeCost() const { return DopeCost; }
	float GetTagPrice() const { return TagPrice; }
	int32 GetWinsForTag() const { return WinsForTag; }
	int32 GetStableMostFighters() const { return StableMostFighters; }
	int32 GetStableMostTaught() const { return StableMostTaught; }
	float GetFeedPerDay() const { return FeedPerDay; }

	/** True if the player has come to bet: the fight in the cage is between two of the ladder's fighters, and the player watches */
	bool IsAtWindow() const;

	/** True if the player came to bet, was caught fixing, and this fight is its own */
	bool IsGamblersOwnFight() const;

	/**
	 *  The stake the betting screen is set to, whoever it is on and before it is on anybody: what left and right put down
	 *  and up and down change, a litre at a time, from one litre to all the window will take.
	 */
	float GetBetWanted() const;

	/**
	 *  True if a fight that was only watched is over and paid and there is another on the bill: whoever watched it can go
	 *  on to that, or back down the stairs to the street.
	 */
	bool CanLeaveForStreet() const;

	/** True if whoever watched has the arrow on the way back down to the street, and not on the next fight */
	bool IsLeaveChosen() const { return bLeaveChosen && CanLeaveForStreet(); }

	/**
	 *  True if the first of the two in a fight that is watched stands on the right of the picture, and the second on the left.
	 *  Settled before the bell and kept through the fight, so that what is written about each is on its own side and stays there.
	 */
	bool IsFirstCornerOnRight() const { return bFirstCornerOnRight; }

	/** True if every fight on tonight's bill has been fought, and what is on the screen is the night's reckoning */
	bool IsWindowNightOver() const;

	const FGutWindowState& GetWindowState() const;

	/** The fight on the card, and the fighter in each of its corners (0 or 1) with the rank it fights at */
	FGutWindowFight GetWindowFight() const;
	const FGutLadderRung* GetWindowRung(int32 Corner) const;
	const UGutLadderData* GetWindowRank(int32 Corner) const;

	/** What the book makes the chance of the fighter in corner 0, from what it knows of both and of what they carry */
	float GetWindowBookChance() const;

	/** What a fix costs, and the chance of its being noticed tonight */
	float GetFixCost(EGutFix Fix, int32 Corner) const;
	float GetFixDetectChance(EGutFix Fix) const;

	EGutFix GetWindowFix() const { return WindowFix; }
	int32 GetWindowFixCorner() const { return WindowFixCorner; }

	const FGutWindowSettlement& GetWindowSettlement() const { return WindowSettlement; }

	int32 GetWindowFightsPerNight() const { return WindowFightsPerNight; }

	/** Returns the ladder of the rank the player is at: the one set on the game mode, or one reached from it through its next ladders */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	const UGutLadderData* GetLadder() const;

	/** Returns the finisher style the crowd is calling for in this fight. Only meaningful if it is calling. */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	EGutFinisherStyle GetCrowdCall() const { return CrowdCall; }

	/** Returns true if the crowd is calling for a finisher style in this fight */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	bool IsCrowdCalling() const { return bCrowdCalling; }

	/** Returns where the player stands on the ladder. Only meaningful on a ladder. */
	const FGutLadderState& GetLadderState() const;

	/** Returns which fight of the ladder the player is on, from 1, or zero off the ladder */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	int32 GetLadderFightNumber() const;

	/** Returns the water the player holds on the ladder. Below zero it is owed. */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetWaterInHand() const;

	/** Returns the rung this fight is, or nothing off the ladder or at the gate */
	const FGutLadderRung* GetRung() const;

	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	EGutBetSide GetBetSide() const { return BetSide; }

	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetBetStake() const { return BetStake; }

	/** Returns the most the window will take from the player on this fight: what it holds and what the slate allows, up to the window's limit */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetMaxStake() const;

	/** Returns what the book makes of the player's chance in this fight, with the weapons drawn and the state the player is in */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetBookChance() const;

	/** Returns what a litre staked on the given side comes back as if it wins, stake included, were the player to stake the given amount there */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetReturnPerLitre(EGutBetSide Side, float Stake) const;

	/** Returns what the cutter charges for a limb, or zero off the ladder */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	float GetPatchPrice() const;

	/** Returns the lines that can be chosen on the card right now, in the order they are shown. The last is always the rack. */
	TArray<FGutCardItem> GetCardItems() const;

	const FGutCardItem& GetCardSelection() const { return CardSelection; }

	/** Returns the sponsor making an offer for this fight, or nothing */
	const FGutSponsor* GetSponsorOffer() const;

	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	bool IsSponsorAccepted() const { return bSponsorAccepted; }

	/** Whether the player has taken Dust or Hush for this fight, how far gone it is on it from 0 to 1, what a dose costs, and whether it is selling yet */
	bool IsDrugTaken(EGutDrug Drug) const;
	float GetDrugHabit(EGutDrug Drug) const;
	float GetDrugPrice(EGutDrug Drug) const { return Drug == EGutDrug::Dust ? DustPrice : HushPrice; }
	bool AreDrugsSold() const;

	/** True if the player's habit has hold of it and it has not had a dose for this fight */
	bool IsInWithdrawal(EGutDrug Drug) const;

	/** True if the player has taken water to lose this fight */
	bool IsThrowingFight() const;

	/** Returns true if the player owns, and so wears, the given piece of armor */
	UFUNCTION(BlueprintPure, Category="Gut|Ladder")
	bool IsArmorWorn(const UGutArmorData* Piece) const;

	/** Returns what the last fight came to in water */
	const FGutSettlement& GetSettlement() const { return Settlement; }

	/** Returns true if the player can hold the given weapon with the arms it has left */
	UFUNCTION(BlueprintPure, Category="Gut|Rack")
	bool CanPlayerHold(const UGutWeaponData* Weapon) const;

protected:

	/** Returns the game instance that remembers the ladder, or nothing if the project is not using one */
	UGutGameInstance* GetGutGameInstance() const;

	/** Puts up the card for the fight the player is on */
	void BeginCard();

	/** Pays out a ladder fight and moves the player on, or not */
	void SettleLadderFight(bool bPlayerWon, bool bSpared, bool bThrown = false);

	/** Called when the player's fighter is beaten and staggers */
	UFUNCTION()
	void OnPlayerStaggered(AGutFighter* Fighter);

	/** Called when either fighter is beaten and staggers, for the crowd */
	UFUNCTION()
	void OnAnyFighterStaggered(AGutFighter* Fighter);

	/** Called when an attack reaches either fighter, for the crowd */
	UFUNCTION()
	void OnAnyAttackReceived(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result);

	/** Sends both fighters to the rack and makes the first draw for each */
	void BeginChoosing();

	/** Puts the drawn weapons in the fighters' hands and takes them off the rack */
	void ApplyChoices();

	/** Returns the position on the rack of the weapon whose asset name contains the given text, or INDEX_NONE */
	int32 FindRackWeapon(const FString& NamePart) const;

	/** Looks for the player's fighter and the opponent, and sets them against each other. Returns true once both exist. */
	bool FindFighters();

	/** Puts up the owner's card for the day */
	void BeginStableCard();

	/** Makes a fighter standing in the cage into one of the stable's: what it has been taught, given and promised, and the state it is in */
	void MakeStableFighter(AGutFighter* Fighter, const FGutStableFighter& Who, bool bLayDown);

	/** Pays out, or does not, when one of the two is dead */
	void SettleStableFight(const AGutFighter* DeadFighter);

	/** A day goes by in the yard: everybody eats, the ones with tags are paid, grudges ease or grow */
	void AdvanceStableDay();

	/** Takes water out of the owner's hand, if it is there */
	bool SpendStableWater(float Amount);

	/** The lines of whichever page of the owner's card is up, and what choosing one does */
	void GetStableCardItems(TArray<FGutCardItem>& Items) const;
	void ActivateStableLine(int32 Code);
	void KeepStableChoice();

	/** The fights on a night's bill, drawn from the living fighters on every rank's ladder */
	void BuildWindowBill(const FGutWindowState& State, TArray<FGutWindowFight>& OutBill) const;

	/** Every rank's ladder, from the bottom */
	void GetRanks(TArray<const UGutLadderData*>& OutRanks) const;

	/** Makes a fighter standing in the cage into the one on a rung of a ladder: its name, health, body, armor, weapon and how it fights */
	void MakeFighter(AGutFighter* Fighter, const FGutLadderRung& Rung, bool bCollared);

	/** Puts up the card for the next fight on the bill, or the night's reckoning if the bill is fought out */
	void BeginWindowCard();

	/** Leaves the window's card for the betting: the two fighters are made, and what was arranged is done to the one it was arranged for */
	void ConfirmWindowCard();

	/** Sets up the fight of a gambler caught fixing: bare hands, a collar, and one of the cage's own for an opponent */
	void BeginGamblersOwnFight();

	/** Pays out, or does not, when one of the two fighters is dead */
	void SettleWindowFight(const AGutFighter* DeadFighter);

	/** Takes the player's fighter out of the player's hands and gives it a brain, and gives the player a place on the wall to watch from */
	void StartSpectating();

	void UpdateSpectatorCamera(float DeltaTime);

	/** Puts every hazard in the state the fight clock calls for */
	void UpdateHazards();

	/** Changes the stage of the fight */
	void SetPhase(EGutFightPhase NewPhase);

	/** Called when either fighter dies */
	UFUNCTION()
	void OnFighterDied(AGutFighter* Fighter, AGutFighter* Killer);

	/** Blueprint hook: the fight has moved to a new stage */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Phase Changed"))
	void BP_OnPhaseChanged(EGutFightPhase NewPhase);
};
