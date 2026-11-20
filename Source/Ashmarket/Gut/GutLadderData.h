// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GutOpponentController.h"
#include "GutTypes.h"
#include "GutLadderData.generated.h"

class UGutArmorData;
class UGutWeaponData;

/** Someone with water to spend on a fighter, and what they want for it */
USTRUCT(BlueprintType)
struct FGutSponsor
{
	GENERATED_BODY()

	/** Who is offering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor")
	FText SponsorName;

	/** What they want */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor")
	EGutSponsorDemand Demand = EGutSponsorDemand::KillSlowly;

	/** Water paid for a win in which the demand was met. A win in which it was not pays nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor", meta = (ClampMin = 0, ClampMax = 10000))
	float Pay = 10.0f;

	/** The sponsor's colors, worn on the body by a fighter that takes an offer to wear them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor")
	FLinearColor Colors = FLinearColor(0.8f, 0.6f, 0.05f);
};

/** One fight on the ladder: who the opponent is, how it fights, and what the fight is worth */
USTRUCT(BlueprintType)
struct FGutLadderRung
{
	GENERATED_BODY()

	/** The opponent's name, as shown on the HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung")
	FText OpponentName;

	/** One line about the opponent, shown under its name before the fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung")
	FText Billing;

	/** The story so far, shown line by line on the card before the fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung")
	TArray<FText> Card;

	/** The weapon the opponent draws. Leave empty for a random draw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	TObjectPtr<UGutWeaponData> Weapon;

	/** Armor the opponent wears */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	TArray<TObjectPtr<UGutArmorData>> Armor;

	/** The opponent's health */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent", meta = (ClampMin = 1, ClampMax = 1000))
	float Health = 125.0f;

	/** The character the opponent is seen as, such as a MetaHuman. Leave empty for the cage's stock opponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	TSoftClassPtr<AActor> Body;

	/** Color of the opponent's body, to tell one opponent from the next */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	FLinearColor BodyTint = FLinearColor(0.6f, 0.08f, 0.05f);

	/** If true the opponent fights with the tuning below instead of its own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	bool bOverrideTuning = false;

	/** How this opponent fights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent", meta = (EditCondition = "bOverrideTuning"))
	FGutOpponentTuning Tuning;

	/**
	 *  If true this opponent will not finish the player. A player who staggers has lost, but lives.
	 *  For the cellmate in the last fight of the Meat arc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent")
	bool bSparesPlayer = false;

	/**
	 *  How many of the pit boss's dogs this opponent brings into the cage with it. They are on its side, and the fight is
	 *  not over while one of them lives. No card is offered for it: it is how this one fights.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Opponent", meta = (ClampMin = 0, ClampMax = 3))
	int32 Dogs = 0;

	/** Water paid to the player for winning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Water", meta = (ClampMin = 0, ClampMax = 10000))
	float Purse = 10.0f;

	/**
	 *  What the book makes of the player's chance in this fight, before it looks at the weapons and at the player's injuries.
	 *  It is the book's opinion and need not be right: the book knows nothing about a new fighter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rung|Water", meta = (ClampMin = 0.01, ClampMax = 0.99))
	float BookChance = 0.4f;
};

/** What the book knows about one weapon against another */
USTRUCT(BlueprintType)
struct FGutFormEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form")
	TObjectPtr<UGutWeaponData> Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form")
	TObjectPtr<UGutWeaponData> Against;

	/** Share of fights the first weapon wins against the second, between evenly matched fighters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form", meta = (ClampMin = 0, ClampMax = 1))
	float WinShare = 0.5f;
};

/**
 *  A run of fights with one fighter, and the water that changes hands around them: purses, the betting window,
 *  the cutter who patches limbs between fights, and the bill at the end.
 *  The Meat arc is one of these with three rungs.
 *
 *  It only holds the fights and the sums. The game mode runs the fights, and the game instance remembers how far the player has got.
 */
UCLASS(BlueprintType)
class UGutLadderData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name of the rank this ladder is, as shown on the HUD: Meat, Dog, Butcher, Champion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	FText RankName;

	/** The fights, in order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	TArray<FGutLadderRung> Rungs;

	/** The rank that comes after this one. Empty for the last. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	TObjectPtr<UGutLadderData> NextLadder;

	/**
	 *  Who the cage's new meat is seen as: the people who come in after the dead, each a character nobody else in the cage is.
	 *  Read from the first ladder only. Set by Tools/build_gut_metahuman.py.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder")
	TArray<TSoftClassPtr<AActor>> NewcomerBodies;

	/**
	 *  Armor on sale before each fight of this rank. Empty for a rank where the player cannot buy armor.
	 *  What the player owns is remembered by name, so a later rank must sell a piece for the player to go on wearing it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Armor")
	TArray<TObjectPtr<UGutArmorData>> ArmorShop;

	/** The sponsors who make offers at this rank, one to a fight, taken in turn. Empty for a rank with no sponsors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Sponsors")
	TArray<FGutSponsor> Sponsors;

	/** Shown at the gate when the player won the last fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Endings")
	TArray<FText> WonEnding;

	/** Shown at the gate when the last opponent spared the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Endings")
	TArray<FText> SparedEnding;

	/** Water the player starts the ladder with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Water", meta = (ClampMin = 0, ClampMax = 10000))
	float StartingWater = 0.0f;

	/** How far into the red the pit boss lets the player go, on bets and on the cutter. What is owed is added to the bill at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Water", meta = (ClampMin = 0, ClampMax = 10000))
	float SlateLimit = 20.0f;

	/** Size of one step up or down in the stake at the betting window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 1, ClampMax = 1000))
	float BetStep = 5.0f;

	/** The most the window takes on one fight from one fighter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 0, ClampMax = 10000))
	float MaxBet = 50.0f;

	/**
	 *  Water the crowd has already put on the fight, split between the two fighters by what the book makes of their chances.
	 *  All bets on a fight share one pot, so the smaller this is, the more the player's own stake moves the odds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 1, ClampMax = 100000))
	float CrowdPool = 200.0f;

	/** Share of the pot the house keeps before it pays the winners */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 0, ClampMax = 0.9))
	float HouseTake = 0.1f;

	/** How much of what it knows about the two weapons the book lets move its opinion. Zero ignores the weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 0, ClampMax = 2))
	float FormWeight = 0.6f;

	/** What each of the player's crippled limbs takes off the book's opinion of its chance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 0, ClampMax = 1))
	float CrippledLimbChancePenalty = 0.06f;

	/** What coming to the fight with no weapon takes off the book's opinion of the player's chance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting", meta = (ClampMin = 0, ClampMax = 1))
	float UnarmedChancePenalty = 0.12f;

	/** What the book knows about each weapon against each other. A pair it has no entry for is even. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Betting")
	TArray<FGutFormEntry> FormBook;

	/** Who patches limbs between fights, as named on the card */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Injuries")
	FText CutterName;

	/** What the cutter charges to patch one crippled limb between fights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Injuries", meta = (ClampMin = 0, ClampMax = 10000))
	float PatchPrice = 10.0f;

	/** How much of its health a patched limb comes back with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Injuries", meta = (ClampMin = 0.05, ClampMax = 1))
	float PatchedLimbShare = 0.5f;

	/**
	 *  If true the crowd calls for a finisher style in every fight. A kill in that style pays the bonus below and makes the player's name faster.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd")
	bool bCrowdCalls = true;

	/** Water the crowd throws in for the finish it called for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd", meta = (ClampMin = 0, ClampMax = 10000))
	float CrowdCallBonus = 5.0f;

	/** Name gained for any win */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd", meta = (ClampMin = 0, ClampMax = 100))
	int32 NameForWin = 1;

	/** Name gained on top of that for the finish the crowd called for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd", meta = (ClampMin = 0, ClampMax = 100))
	int32 NameForCrowdCall = 2;

	/** The winner's cut of the gate at this rank, before the player's name is counted. Zero for ranks that get no cut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd", meta = (ClampMin = 0, ClampMax = 10000))
	float GateCutBase = 0.0f;

	/** Added to the cut of the gate for each point of the player's name: a known fighter fills the deck */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Crowd", meta = (ClampMin = 0, ClampMax = 1000))
	float GateCutPerName = 0.0f;

	/** Share of each purse the pit boss keeps against what the player owes him, until it is paid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Bill", meta = (ClampMin = 0, ClampMax = 1))
	float DebtShareOfPurse = 0.0f;

	/**
	 *  If true the rank ends with the pit boss's bill: the player's water is taken against it and what is left over becomes a debt.
	 *  That is how the Meat arc ends. Later ranks end with the player keeping what it holds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Bill")
	bool bBillAtGate = true;

	/** The pit boss's bill for room and board, presented at the gate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Bill", meta = (ClampMin = 0, ClampMax = 100000))
	float RoomAndBoard = 100.0f;

	/** Added to the bill if the last opponent spared the player and the crowd went without its kill */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Bill", meta = (ClampMin = 0, ClampMax = 100000))
	float SparedPenalty = 60.0f;

	/** However much water the player is holding, the bill comes to at least this much more */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ladder|Bill", meta = (ClampMin = 0, ClampMax = 100000))
	float MinimumDebt = 25.0f;

public:

	/**
	 *  Returns what the book makes of the player's chance in a fight: the rung's own figure, moved by what the book knows
	 *  about the two weapons and by the state the player is in. Empty weapons mean bare hands.
	 */
	UFUNCTION(BlueprintPure, Category="Ladder")
	float GetBookChance(int32 Rung, const UGutWeaponData* PlayerWeapon, const UGutWeaponData* EnemyWeapon, int32 PlayerCrippledLimbs) const;

	/**
	 *  Returns what one litre staked comes back as if it wins, stake included, with the given stake added to the pot.
	 *  Every bet on the fight shares one pot, so a bigger stake on a side shortens that side's odds.
	 */
	UFUNCTION(BlueprintPure, Category="Ladder")
	float GetReturnPerLitre(float PlayerChance, EGutBetSide Side, float Stake) const;

	/** Returns what the player owes the pit boss at the gate, holding the given water. Never less than the minimum debt. */
	UFUNCTION(BlueprintPure, Category="Ladder")
	float GetGateDebt(float Water, bool bSpared) const;

	/** Returns the winner's cut of the gate at this rank for a fighter with the given name */
	UFUNCTION(BlueprintPure, Category="Ladder")
	float GetGateCut(int32 Name) const { return GateCutBase + (GateCutPerName * Name); }

	/** Returns the bill presented at the gate, before the player's water is taken against it */
	UFUNCTION(BlueprintPure, Category="Ladder")
	float GetGateBill(float Water, bool bSpared) const;
};
