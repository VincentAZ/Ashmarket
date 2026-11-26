// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutTypes.h"
#include "GutStableTypes.generated.h"

class UGutWeaponData;

/** The three things a fighter can be trained in */
UENUM(BlueprintType)
enum class EGutTraining : uint8
{
	/** Hits harder and takes more */
	Strength,

	/** Covers the floor faster and gets its wind back sooner */
	Speed,

	/** Sees it coming: guards, answers and punishes like somebody who has done this before */
	Nerve,

	/**
	 *  Uses the cage: works the other round until their back is to the spikes or the cable and then hits them onto it,
	 *  keeps their own back off both, knows a heavy blow is the answer to a raised guard, and keeps something in hand.
	 *  ("the more a fighter has, the better he can fight - better at directing their opponent to the spike or cable",
	 *  25 November.)
	 */
	Cunning
};

/** Somebody the player owns, or pays, to fight in the cage for it */
USTRUCT(BlueprintType)
struct FGutStableFighter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	FText FighterName;

	/** One line on who this is, read out at the Block */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	FText Billing;

	/** The character that is seen */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	TSoftClassPtr<AActor> Body;

	/** What it fights with. Empty for bare hands. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	TObjectPtr<UGutWeaponData> Weapon;

	/**
	 *  What it has been given to wear, by the names of the pieces. Armor is bought at Needle's by anybody who holds the
	 *  water, carried in the bag and given to a fighter (25 November: "what if I want to buy armor for my slaves?"). The
	 *  fighter wears it in the cage at whatever rank the fight is.
	 */
	UPROPERTY()
	TArray<FName> Armor;

	/** The three things it can be trained in, each from 0 to the most a fighter can be taught */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Strength = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Speed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Nerve = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Cunning = 0;

	/** Everything taught, as the Block prices it and the book reckons it: four things counted as the three there were */
	float GetTaught() const { return (Strength + Speed + Nerve + Cunning) * 0.75f; }

	/** If true it was bought at the Block and wears a collar. If false it signed for a wage and a share of its purses, and can walk away. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bOwned = true;

	/** What was paid for it at the Block, or what it is paid each day */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Price = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Wage = 0.0f;

	/** If true it has been bought or signed and not yet brought to the yard: it is with its owner, wherever in the town that is */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bInTow = false;

	/** The number it stood on at the Block when it was bought, which its bill of sale has on it. Zero if it stood on none. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Lot = 0;

	/** If true it has been given bread, or tea, out of its owner's bag since its last fight: as for the player's own fighter, and gone when that fight is over */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bFed = false;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bSteadied = false;

	/** How its limbs are, carried from fight to fight. Negative for a fighter that has not been hurt yet. */
	UPROPERTY()
	float LimbHealth[GutNumLimbs] = { -1.0f, -1.0f, -1.0f, -1.0f };

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Wins = 0;

	/** If true it has been promised its tag, and how many wins it had when it was */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bPromisedTag = false;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 WinsAtPromise = 0;

	/** If true the promise was broken. It does not forget. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bBetrayed = false;

	/**
	 *  What it thinks of the hand that feeds it, from 0 to 100. Food, medicine and promises kept lower it. Dope, being sent in hurt,
	 *  seeing somebody sold and promises broken raise it. High, it fights badly, then lies down, then is gone in the night.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Resentment = 0.0f;

	/** If true it is doped for its next fight */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bDoped = false;

	/** How far gone it is on what it has been given, from 0 to 1. Past a half it is no good without it. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Habit = 0.0f;

	bool IsCrippled() const
	{
		for (const float Limb : LimbHealth)
		{
			if (Limb == 0.0f)
			{
				return true;
			}
		}

		return false;
	}
};

/**
 *  Somebody with no tag being taken. "No tag means you're property" is the third law, and anybody may make it so who can
 *  hold whoever it is: the player goes for them in the town, it is fought out bare-handed in a dead end between the
 *  stacks (the cage's rules, a place of its own), and what it came to is settled when the player is back in the town.
 *  ("Should be a way to capture people (for sale or fight or - can we do rent?) but an attempted capture is dangerous
 *  for our protagonist - almost fight like but a little easier", 25 November.)
 */
USTRUCT(BlueprintType)
struct FGutTaking
{
	GENERATED_BODY()

	/** True from the moment the player goes for them until it is settled in the town */
	UPROPERTY(BlueprintReadOnly, Category="Taking")
	bool bOn = false;

	/** Who: as they will be if they are taken, a fighter of the player's with a collar on */
	UPROPERTY(BlueprintReadOnly, Category="Taking")
	FGutStableFighter Who;

	/** The town's mark that says they are gone from where they stood, however it came out */
	UPROPERTY(BlueprintReadOnly, Category="Taking")
	FName Mark;

	/** Nought while it is undecided. 1 beaten down and taken, 2 killed, 3 got away with the player down */
	UPROPERTY(BlueprintReadOnly, Category="Taking")
	int32 Outcome = 0;
};

/** Where somebody who runs a stable of fighters stands */
USTRUCT(BlueprintType)
struct FGutStableState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bStarted = false;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Water = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Debt = 0.0f;

	/**
	 *  What has happened to the yard that its owner has not been told: somebody on a wage who went unpaid and walked, a
	 *  day written down as owed. Said when the owner next stands in the yard, and then forgotten. Until 25 November
	 *  nothing said it: a fighter died, the day could not be paid, the other walked, and the yard was simply empty ("I
	 *  had two fighters in the yard - one died in a fight - now there is no one in the yard").
	 */
	UPROPERTY()
	FString News;

	/** Which day it is, from one. Training, a fight and waiting each take one. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Day = 1;

	/** What the Block and the bill are drawn from */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	TArray<FGutStableFighter> Fighters;

	/** The stable's name with the crowd: a win raises it, and the purses offered follow it */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Name = 0;

	/** The fighters who have died against this stable, each as rank times 100 plus place on its ladder */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	TArray<int32> Dead;

	/** Who was bought at the Block today, so that the same body is not for sale twice: places on today's list */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	TArray<int32> BoughtToday;

	/** The fight that has been made: which of the stable's fighters, and against whom (rank times 100 plus place). Negative for none. */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Matched = -1;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 MatchedAgainst = -1;

	/** Sums over the whole run */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Purses = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	float Spent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 FightsWon = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 FightersLost = 0;

	/** If true one of the stable's collared fighters is lying dead in the pit boss's cold room, and is the owner's to see to */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bBodyWaiting = false;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	FText BodyOf;

	/** If true whoever lies there was a woman */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bBodyWoman = false;

	/** The day it died on */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 BodySince = 0;

	/** If true it lost a limb before it died, and Doc Stitch pays for what is still good */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	bool bBodyBroken = false;

	/** How many of its dead the stable has put in the ground, and how many it has sold */
	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 Buried = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stable")
	int32 BodiesSold = 0;
};

/** What a stable's fight came to, for the HUD to read out */
struct FGutStableSettlement
{
	bool bSettled = false;
	bool bWon = false;

	FText FighterName;

	/** If true the one who fought is a woman: she, and not he, in what is read out */
	bool bWoman = false;

	float Purse = 0.0f;

	/** What a signed fighter kept of it */
	float FightersShare = 0.0f;

	/** The bet, as at the window */
	int32 Backed = -1;
	float Stake = 0.0f;
	float Return = 0.0f;

	/** True if the fighter went down on purpose, out of what it thinks of its owner */
	bool bLayDown = false;

	/** True if it has now won enough to ask for the tag it was promised */
	bool bTagDue = false;

	/** What was kept out of the purse and the window's payment against what the owner owes */
	float DebtPaid = 0.0f;
};

/** The pages of the owner's card */
enum class EGutStablePage : uint8
{
	/** The yard: the fighters, and the ways out of it */
	Yard,

	/** One fighter, and what can be done with it */
	Fighter,

	/** Who is for sale or will sign today */
	Block,

	/** The fights on offer today */
	Bill,

	/** Which of the stable's fighters takes the fight that was picked */
	Pick,

	/** One of the stable's own is dead, and nothing else is done until it is seen to */
	Body
};

/** What is done with a collared fighter that died in the cage (design document, chapter 7: after the bell) */
enum class EGutBodyGoesTo : uint8
{
	/** Doc Stitch, for parts */
	Parts,

	/** The grill carts */
	Grill,

	/** The ground, at the owner's cost, with the yard standing by */
	Ground
};

/** What a line on the owner's card does, kept in the line's index. A line about one of several things adds which one. */
namespace GutStableLines
{
	enum
	{
		Back = 0,
		ToBlock = 1,
		ToBill = 2,
		Wait = 3,

		TrainStrength = 10,
		TrainSpeed = 11,
		TrainNerve = 12,
		Patch = 13,
		Dope = 14,
		Promise = 15,
		HandOverTag = 16,
		DenyTag = 17,
		Sell = 18,

		/** Everything the fighter has on, back into the bag */
		TakeArmor = 24,

		/** The fourth thing taught: see EGutTraining::Cunning. (Not beside the other three, whose numbers are taken.) */
		TrainCunning = 25,

		/** Out for the day to whoever pays for a back: see AGutGameMode::HireOutFighter */
		HireOut = 26,

		/** Out of the yard gate to the street, with this one walking behind */
		WalkOut = 19,

		BodyToParts = 20,
		BodyToGrill = 21,
		BodyToGround = 22,

		/** Plus the fighter's place in the stable */
		OpenFighter = 100,

		/** Plus the place on today's list at the Block */
		Buy = 200,

		/** Plus which of the pieces of armor in the bag: put on the fighter */
		GiveArmor = 300,

		/** Plus which of the kinds of thing in the bag: one of them put on the yard's rack */
		RackPut = 400,

		/** Plus which of the kinds of thing on the rack: one of them taken into the bag */
		RackTake = 500,

		/** Plus the place on today's bill */
		TakeFight = 300,

		/** Plus the fighter's place in the stable */
		SendFighter = 400,

		/** Out of the yard gate with all of them behind */
		WalkOutAll = 23,

		/** Out of the yard gate with one of them behind: plus the fighter's place in the stable. On the yard's own page, so that
		 *  nobody has to open a fighter's page to find that they can be brought along. */
		WalkWith = 500
	};
}
