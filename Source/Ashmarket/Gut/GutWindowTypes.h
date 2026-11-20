// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutTypes.h"
#include "GutWindowTypes.generated.h"

/** A way of making a fight come out as the gambler wants it to */
UENUM(BlueprintType)
enum class EGutFix : uint8
{
	None,

	/** Pay a fighter to go down. Dear, close to certain, and the likeliest to be noticed. */
	Bribe,

	/** Something in a fighter's water bucket. Cheap. It tires early and thinks slowly. */
	DrugBucket,

	/** A weapon on the rack that will not last. It comes apart at the first blow that meets anything. */
	TamperRack
};

/**
 *  Where somebody who has come to the Gut to bet stands: the water in hand, which night it is and which fight of the night,
 *  who on the roster is already dead, and how hard the pit boss is looking at them.
 */
USTRUCT(BlueprintType)
struct FGutWindowState
{
	GENERATED_BODY()

	/** If false the gambler has not been to the window yet and the rest is blank */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	bool bStarted = false;

	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Water = 0.0f;

	/** Water owed to the lender who staked a gambler that was cleaned out */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Debt = 0.0f;

	/** Which night at the cage this is, from one */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 Night = 1;

	/** Which fight on tonight's bill is next, from zero */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 Fight = 0;

	/** What the bills are drawn from: the same number gives the same nights */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 Seed = 0;

	/** The fighters who have died in front of this gambler, each as rank times 100 plus place on its ladder. They do not fight again. */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	TArray<int32> Dead;

	/** How many of the dead were dead before tonight: tonight's bill was posted without them, and stays as posted whoever dies on it */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 DeadBeforeTonight = 0;

	/** How many newcomers the cage had when tonight's bill was posted: anybody who has come in since is not on it */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 NewBeforeTonight = 0;

	/** How closely the gambler is watched, from 0 to 1: added to the chance that a fix is noticed. Every fix raises it, every night lowers it. */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Heat = 0.0f;

	/** If true the gambler has been caught fixing and the next fight is its own, in the cage */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	bool bCaught = false;

	UPROPERTY(BlueprintReadOnly, Category="Window")
	int32 TimesCaught = 0;

	/** Tonight's sums: staked, paid back by the window, spent on fixes, and fines */
	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Staked = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Returned = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Window")
	float FixSpend = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Window")
	float Fines = 0.0f;
};

/** One fight on the night's bill: two fighters off the ladders, each as a rank and a place on that rank's ladder */
struct FGutWindowFight
{
	int32 Rank[2] = { 0, 0 };
	int32 Rung[2] = { 0, 0 };

	bool IsValid() const { return Rank[0] != Rank[1] || Rung[0] != Rung[1]; }
};

/** What one fight came to for the gambler, for the HUD to read out */
struct FGutWindowSettlement
{
	bool bSettled = false;

	/** Which corner is still standing: 0 or 1 */
	int32 Winner = 0;

	/** Which corner the gambler backed, or -1 for no bet, what was staked and what the window paid back */
	int32 Backed = -1;
	float Stake = 0.0f;
	float Return = 0.0f;

	/** What was tried, on which corner, and whether it was noticed */
	EGutFix Fix = EGutFix::None;
	int32 FixedCorner = 0;
	bool bCaught = false;

	/** What being noticed cost on the spot */
	float Fine = 0.0f;

	/** True if this was the gambler's own fight, in the cage for fixing, and how it ended */
	bool bOwnFight = false;
	bool bSurvived = false;

	/** True if the lender had to stake the gambler again after this */
	bool bStakedAgain = false;

	/** True if the gambler was cleaned out and the lender is somebody to be walked to: nothing was lent, and the gambler is told where she sits */
	bool bSentToLender = false;

	/** What was kept out of the window's payment against what the gambler owes the lender */
	float DebtPaid = 0.0f;

	/** True if the gambler was cleaned out again and the lender would lend no more: the debt is to be paid in the cage */
	bool bCalledIn = false;

	/** What walking out of the cage alive cleared from the book */
	float DebtCleared = 0.0f;
};
