// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "GutRosterTypes.h"
#include "GutTypes.h"
#include "GutStableTypes.h"
#include "GutWindowTypes.h"
#include "TownStoryTypes.h"
#include "GutGameInstance.generated.h"

class UGutLadderData;

/** How far the player has got up the ladder, and what it is carrying */
USTRUCT(BlueprintType)
struct FGutLadderState
{
	GENERATED_BODY()

	/** The rank the player is fighting at, from zero: each rank is a ladder of its own, reached through the one before */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	int32 Rank = 0;

	/** The fight of this rank the player is on, from zero */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	int32 Rung = 0;

	/** Water in hand. Below zero it is owed to the pit boss on the slate. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float Water = 0.0f;

	/** What the player owes the pit boss from the bill at the gate. Paid down out of later purses. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float Debt = 0.0f;

	/** Cage fame. Goes up with every win, and faster for giving the crowd the finish it called for. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	int32 Name = 0;

	/** If true the player has eaten since its last fight: its wind comes back faster in the next. Gone when that fight is over. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	bool bFed = false;

	/** If true the player has had tea since its last fight: its wind starts coming back sooner in the next. Gone when that fight is over. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	bool bSteadied = false;

	/** If true the limb health below was carried out of an earlier fight. If false the player is whole. */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	bool bInjuriesCarried = false;

	/** Damage each limb can still take, indexed by EGutLimb. Zero means crippled. */
	UPROPERTY()
	float LimbHealth[GutNumLimbs] = { 0.0f, 0.0f, 0.0f, 0.0f };

	/** The armor the player owns and wears, by the name of each piece's asset */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	TArray<FName> Armor;

	/** Purses won in this rank so far */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float Purses = 0.0f;

	/** What the betting window has paid out in this rank, less what was staked there */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float BettingNet = 0.0f;

	/** Paid in this rank on top of purses: by the crowd for the finish it called for, by sponsors, and from the gate */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float Extras = 0.0f;

	/** Water spent in this rank on the cutter and on armor */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float PatchSpend = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float ArmorSpend = 0.0f;

	/** Taken out of this rank's purses against the debt */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float DebtPaid = 0.0f;

	/** If true the last fight of this rank is over and the player is at the gate */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	bool bFinished = false;

	/** How far gone the player is on each of the two things it can take before a fight, from 0 to 1, by EGutDrug */
	UPROPERTY()
	float Habit[2] = { 0.0f, 0.0f };

	/** Which of them it has taken for the fight it is on */
	UPROPERTY()
	bool bDustTaken = false;

	UPROPERTY()
	bool bHushTaken = false;

	bool IsTaken(int32 Which) const { return Which == 0 ? bDustTaken : bHushTaken; }

	void SetTaken(int32 Which, bool bTaken) { (Which == 0 ? bDustTaken : bHushTaken) = bTaken; }

	/** Health lost for good to Dust taken on top of a full habit */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	float HeartDamage = 0.0f;

	/** If true the last opponent spared the player */
	UPROPERTY(BlueprintReadOnly, Category="Ladder")
	bool bSpared = false;
};

/** What the player has set that belongs to no run, as it is written to disk */
UCLASS()
class UGutSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	int32 Loudness = 3;

	UPROPERTY()
	int32 Speech = 0;
};

/** So many of one thing, carried in the player's bag: bought at a stall, and used when the player chooses */
USTRUCT(BlueprintType)
struct FGutBagItem
{
	GENERATED_BODY()

	/** What it is, by the name the town's goods have it under */
	UPROPERTY(BlueprintReadOnly, Category="Bag")
	FName Good;

	UPROPERTY(BlueprintReadOnly, Category="Bag")
	int32 Count = 0;
};

/** A run up the ladder, as it is written to disk */
UCLASS()
class UGutSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FGutLadderState Ladder;

	/** Whether there is a ladder run at all, and the gambler's and the owner's runs, each kept whether or not it is the one being played */
	UPROPERTY()
	bool bLadderStarted = true;

	UPROPERTY()
	FGutWindowState Window;

	UPROPERTY()
	FGutStableState Stable;

	/** What the player carries, whichever of those it is being */
	UPROPERTY()
	TArray<FGutBagItem> Bag;

	/** Who in the cage is dead, who has come in since, and what each has won: the same for all three */
	UPROPERTY()
	FGutRosterState Roster;

	/** What has been picked up in the town and is not lying there any more: the place's name, a colon, and which of its finds */
	UPROPERTY()
	TArray<FName> Found;

	/** The one purse, and whether there is one yet: see UGutGameInstance::ShareWater */
	UPROPERTY()
	float Purse = 0.0f;

	UPROPERTY()
	bool bOnePurse = false;

	/** What the town knows of the player: how it stands with each of its powers, the work it has taken and how each piece came out */
	UPROPERTY()
	FTownStoryState Story;
};

/**
 *  Remembers what has to outlast a single fight: the cage level is loaded afresh for each one.
 *  That is how far up the ladder the player is, its water and its injuries, or that there is no ladder at all
 *  and the cage is being used for single fights.
 *
 *  It also keeps the run on disk, so that closing the game does not lose it. The run is written at the card before each fight
 *  and after each fight is paid out, and read back when the game starts. -GutNoSave on the command line switches that off,
 *  which every automated run does so that it cannot touch the player's own run.
 */
UCLASS()
class UGutGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	/**
	 *  If true the cage runs single fights with no ladder, no water and nothing carried over.
	 *  For trying things out, and for every test that is not about the ladder.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bSandbox = false;

	/** If true the ladder has been started, and the state below is the player's. If false it is still blank. */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	bool bLadderStarted = false;

	/** A part of the story that is to be told when the next card comes up: set when a run is begun, and told once */
	EGutStory PendingStory = EGutStory::None;

	/** Where the player stands on the ladder now */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FGutLadderState Ladder;

	/** Where the player stood when it came to the fight it is on, before the cutter and the betting window. A fight lost is fought again from here. */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FGutLadderState LadderAtRungStart;

	/**
	 *  If true the player has come to the cage to bet, not to fight: it watches the fights on the night's bill from the wall,
	 *  with water on them, and whatever it has done to help one fighter lose.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bWindow = false;

	/** Where the gambler stands */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FGutWindowState Window;

	/** If true the player has come to the cage as an owner: it buys or signs fighters, trains them, and sends them in */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bStable = false;

	/** Where the owner stands */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FGutStableState Stable;

	/** What the player carries about the town: bought at its stalls, kept with the run */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	TArray<FGutBagItem> Bag;

	/** Who in the cage is dead, who has come in since, and what each has won, whichever way the player comes to it */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FGutRosterState Roster;

	/**
	 *  What the town knows of the player: how it stands with each of its powers, the work it has taken, what became of
	 *  whoever it dealt with. Kept with the run. -GutStoryAfresh on the command line forgets it when the game opens.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	FTownStoryState Story;

	/** How many of something are in the bag, and puts so many more in it, or takes them out */
	int32 GetBagCount(FName Good) const;
	void AddToBag(FName Good, int32 Count);

	/**
	 *  If true the player is standing outside, choosing how to come in: to fight, to own or to bet.
	 *  And if it came in that way, the cards have a way back out to here.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bAtDoor = false;

	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bCameByDoor = false;

	/**
	 *  The street outside the cage, as a place to walk in: the level that "out to the street" opens. With none, or where
	 *  IsStreetOpen() says no, the street is the card with the ways in on it that it was before there was a place.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	FName StreetLevel = FName("/Game/Town/Lvl_PoolStreet");

	/** The place in the town where the lender sits at her table. Where it has been built, nobody is lent to at the window. */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	FName LenderLevel = FName("/Game/Town/Lvl_BlockSquare");

	/** Set by a test to have the street as a place, or as the card, whatever it would otherwise be */
	TOptional<bool> bStreetForced;

	/**
	 *  The place in the town the player was last in, which is where the cage's way out leads: its level, and where in it the
	 *  player was standing and looking when it left. With none, the cage's way out leads to StreetLevel.
	 */
	FName TownLevel;
	TOptional<FTransform> TownStood;

	/** Which of the next place's ways in the player is arriving by, when it has walked there from another place */
	FName TownArrive;

	/** If true the owner has come to the cage by the Block's clerk, and the first thing it is shown is who is for sale */
	bool bStableAtBlock = false;

	/** True if the cage is to open, as an owner's, on the pit boss's bill: come to by the hatch in a yard that is walked about in */
	bool bStableAtBill = false;

	/**
	 *  True if the owner came into the cage by the hatch of a yard that is walked about in. The cage's own yard card is then
	 *  never shown: wherever it would be (a fight over, the bill backed out of, a body seen to), the player is stood back in
	 *  the yard itself. He was shown the old card after a fight, took "out with both", and was put in the yard with nobody.
	 */
	bool bStableFromYard = false;

	/** How loud the game is, in thirds: 3 is as it was mixed, 0 is silent. Set from the menu that stops the game, and kept between sittings. */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	int32 Loudness = 3;

	/**
	 *  How what people say reaches the player: 0 is heard and written, 1 is written only, 2 is heard, with only what the
	 *  player can say back written. Set from the menu that stops the game, and kept between sittings with the loudness.
	 *  Somebody whose line has no recording is always written.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	int32 Speech = 0;

	bool HearsSpeech() const { return Speech != 1; }

	/**
	 *  The street's camera, locked: it stays as high and as steep as it was when it was locked, in every place, and only
	 *  turns. Kept here so that it is the same from one place to the next.
	 */
	bool bCameraLocked = false;
	float CameraLockedPitch = -6.0f;
	bool ReadsSpeech() const { return Speech != 2; }

	/** If true the run is kept on disk */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	bool bSaveEnabled = true;

	/** Name of the save the run is kept in */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	FString SaveSlotName = TEXT("GutRun");

	/** Name of the save the player's own settings are kept in */
	UPROPERTY(BlueprintReadWrite, Category="Gut")
	FString SettingsSlotName = TEXT("GutSettings");

public:

	/** Writes how loud the game is where it is found the next time the game is opened */
	void KeepLoudness() const;

	/**
	 *  True if leaving the cage for the street goes to the street's own level. Not while the automated tests and the
	 *  simulations are running, which were written for a cage that only ever opens itself again, unless one of them asks.
	 */
	bool IsStreetOpen() const;

	/** Returns true if the lender is somebody the player can walk to: then she lends at her table, and not unasked at the window */
	bool IsLenderInTown() const;

	/** Leaves for the street's own level. The run is written down first. */
	void GoToStreet(const UObject* WorldContext);

	/** True from the moment the street is asked for until a level has begun: the cage's screens are not drawn while it is */
	bool bGoingToStreet = false;

	/** Applies how loud the game is to whatever world this is */
	void ApplyLoudness(const UObject* WorldContext) const;

	/** Reads the run back from disk, if there is one */
	virtual void Init() override;

	/**
	 *  Buys the fighter a piece of armor from what a rank sells, to be worn in place of whatever it had on that part of
	 *  the body. Credit is how far into the red whoever is selling lets it go. Returns false if the piece is already
	 *  worn or there is not the water for it. Whoever calls this says where it can be done: the cage at its card, the
	 *  armorers at their counter.
	 */
	bool BuyArmor(const UGutLadderData* Rank, int32 ShopIndex, float Credit);

	/** Puts the player back at the foot of the ladder, whole, with the given water, and forgets the run on disk */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void ResetLadder(float StartingWater);

	/**
	 *  One purse. The player is one person and holds one lot of water, whichever way they come to the cage: as a fighter,
	 *  to bet, or as an owner. (Until 21 November there were three purses that never mixed, and Vincent could not buy
	 *  anybody with a fighter's winnings in hand, or tell why.)
	 *
	 *  The rules of each way still keep their own figure and are left as they were written. This is what makes the three
	 *  one: whatever any of them has gained or spent since it was last called is put on the purse, and all three are set
	 *  to the purse. It is called all the time in the town and whenever the run is written down.
	 *  -GutThreePurses on the command line leaves the three apart, as they were.
	 */
	void ShareWater();

	/** True if the player holds one purse: after the first time Share Water has run */
	bool IsOnePurse() const { return bOnePurse; }

	/** What the player holds, where there is one purse */
	float GetPurse() const { return Purse; }

	/**
	 *  What somebody who starts one of the three ways comes to it with: the way's own starting water where the three are
	 *  kept apart, and what they already hold where there is one purse.
	 */
	float GetStartingWater(float Own) const { return bOnePurse ? Purse : Own; }

	/** What a new player holds before they have been anywhere, where there is one purse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut", meta=(ClampMin=0))
	float FirstWater = 40.0f;

	/** What has been picked up in the town and is not lying there any more: the place's name, a colon, and which of its finds */
	UPROPERTY(BlueprintReadOnly, Category="Gut")
	TArray<FName> Found;

	/** Lets Share Water run under the automation tests, which otherwise keep the three purses their rules were tested with */
	bool bSharesWaterInTests = false;

	/** Forgets the purse: the three are apart again until Share Water next runs. For tests. */
	void ForgetPurse() { bOnePurse = false; bPurseSeen = false; Purse = 0.0f; }

	/** Writes the run to disk as the player came to the fight it is on. Returns false if saving is off or failed. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	bool SaveRun();

protected:

	/** The one purse: what the player holds */
	float Purse = 0.0f;
	bool bOnePurse = false;

	/** What each way's own figure stood at when Share Water last ran (fighter, bettor, owner), and whether that is known in this sitting */
	float PurseSeen[3] = { 0.0f, 0.0f, 0.0f };
	bool bPurseSeen = false;

public:


	/** Reads the run from disk. Returns false if saving is off or there is nothing there. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	bool LoadRun();

	/** Returns true if there is a run on disk */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool HasSavedRun() const;
};
