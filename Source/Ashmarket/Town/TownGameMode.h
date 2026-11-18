// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GutTypes.h"
#include "TownTypes.h"
#include "TownGameMode.generated.h"

class AGutFighter;
class UGutGameInstance;
class UGutLadderData;
class UIKRetargeter;

/** The lines of the menu that stops the game in the street */
namespace TownStoppedLines
{
	inline constexpr int32 GoOn = 0;
	inline constexpr int32 Sound = 1;
	inline constexpr int32 Leave = 2;
	inline constexpr int32 Count = 3;
}

/** Somebody being walked from where they stand to somewhere else: a lot down off the Block when it is sold, or up onto it */
struct FTownLed
{
	/** Who: their place among the place's people */
	int32 Person = INDEX_NONE;

	/** The places still to go through, in turn */
	TArray<FVector> Way;

	/** Which way they turn to face when they are there */
	float Facing = 0.0f;

	/** How long before they set off */
	float Wait = 0.0f;
};

/** Somebody who walks a round of the place, as the place's list says: where they have got to on it */
struct FTownBeat
{
	/** Who: their place among the place's people */
	int32 Person = INDEX_NONE;

	/** The stop they are walking to, or standing at */
	int32 Stop = 0;

	/** How long they still have to stand where they are */
	float Wait = 0.0f;
};

/** One thing on a counter, as it is offered to whoever is at it */
struct FTownShopLine
{
	FString Name;

	/** What it is for, in a few words */
	FString Detail;

	float Price = 0.0f;

	/** The player has it on already */
	bool bWorn = false;

	/** It can be bought now. If not, Why says why not. */
	bool bCanBuy = false;
	FString Why;
};

/**
 *  The rules of a place in the town where nobody is fighting: the player walks about it, talks to the people
 *  who stand in it, and leaves by its doors. The cage has its own game mode, which is all fight.
 *
 *  The player is the same fighter as in the cage, put at ease. The people are fighters at ease too, each given
 *  one of the built bodies, so anybody in the street can later be made to walk, or to fight.
 */
UCLASS()
class ATownGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ATownGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaTime) override;

protected:

	/** The place this level is. Read at the start; with none the street is empty and has no edges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	TSoftObjectPtr<UTownPlaceData> Place;

	/** What the people of the place are made from: a fighter with no brain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	TSoftClassPtr<AGutFighter> PersonClass;

	/** The cage, which the ways in open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	FName CageLevel = FName("/Game/Gut/Lvl_Gut");

	/** How near somebody or something has to be to be spoken to or used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 50, ClampMax = 1000, Units = "cm"))
	float Reach = 230.0f;

	/** How near the middle of whoever walks can come to anything that stands on the ground: about half a body's width */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 5, ClampMax = 200, Units = "cm"))
	float WalkerRadius = 34.0f;

	/** How near an open way to another place has to be come to be gone through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 20, ClampMax = 500, Units = "cm"))
	float WayReach = 110.0f;

	/** What the lender counts out at a time, what she writes down for it, and the most she will have anybody owe her: the window's own lender's numbers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0))
	float LoanGiven = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0))
	float LoanOwed = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0))
	float LoanLimit = 105.0f;

	/** A lot is walked at this much of its own walking pace, turns this fast, and waits this long after the hammer before it moves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0.1, ClampMax = 2))
	float LotPace = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 30, ClampMax = 1080, Units = "deg"))
	float LotTurnSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float LotSoldWait = 1.4f;

	/** Lots coming up set off this long after one another, and those kept wait this far apart */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float LotFileTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 30, ClampMax = 300, Units = "cm"))
	float LotFileGap = 85.0f;

	/** The first rank of the cage's ladder, from which the rank a fighter has reached is found: what the armorers sell goes by it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	TSoftObjectPtr<UGutLadderData> FirstLadder;

	/** Somebody walking their round stops for whoever is in front of them and this near, and walks on when the way is clear */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 30, ClampMax = 400, Units = "cm"))
	float BeatRoom = 130.0f;

	/** While somebody is talked to the camera turns to look past the player at them: this far to one side, this far down, this fast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = -90, ClampMax = 90, Units = "deg"))
	float TalkCameraYaw = -22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = -45, ClampMax = 45, Units = "deg"))
	float TalkCameraPitch = -6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0, ClampMax = 20))
	float TalkCameraTurnSpeed = 4.0f;

	/** The place, once read */
	UPROPERTY(Transient)
	TObjectPtr<UTownPlaceData> PlaceData;

	/** The people standing in it, in the order the place lists them */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGutFighter>> People;

	/** If true the player's fighter has been found and put at ease, and the people have been stood where they belong */
	bool bSettled = false;

	/** True once the player's fighter has been seen to, and how many frames have gone by since: the people are stood after it, one to a frame */
	bool bWalkerSettled = false;
	int32 FramesSettling = 0;

	/** Who is in reach, or which door: its place in the place's list, or less than nought */
	int32 PersonInReach = INDEX_NONE;
	int32 DoorInReach = INDEX_NONE;

	/** Who is being talked to, which of their lines they are at, and which answer the player is on */
	int32 TalkingTo = INDEX_NONE;
	int32 TalkLine = 0;
	int32 TalkChoice = 0;

	/** Words put up by something that is not a person: a door that is shut */
	FString Notice;
	float NoticeUntil = 0.0f;

	bool bStoppedMenu = false;
	int32 StoppedChoice = 0;

	/** How long the place has been settled: nobody is walked back out by the way they have that moment come in by */
	float SettledFor = 0.0f;

	/** Which of the place's calls is up, and for how long it has been */
	int32 CallIndex = 0;
	float CallTime = 0.0f;

	/** What the last dealing with the lender came to, in words, for her to say back */
	FString Dealt;

	/** Everybody who is being walked somewhere just now */
	TArray<FTownLed> Led;

	/** Everybody who walks a round of the place */
	TArray<FTownBeat> Beats;

	/** Whose counter the player is at, as their place among the people: less than nought is nobody's */
	int32 ShopKeeper = INDEX_NONE;

	/** True while the player is at a counter, which of its lines it is on, and what the last thing done there came to */
	bool bShopping = false;
	int32 ShopChoice = 0;
	FString ShopSaid;

	/** The cage's ladder, once read */
	UPROPERTY(Transient)
	TObjectPtr<UGutLadderData> LadderData;

	/** The lots that have been walked off and are kept, as their places among the people, in the order they went: the first is at the far end of the file */
	TArray<int32> KeptLots;

	/** Finds the player's fighter and stands the people in their places, once both can be done */
	void Settle();

	/** Keeps whoever walks on the ground the place has, and out of everything that stands on it */
	void KeepOnGround(AGutFighter* Walker) const;

	/** Works out who or what is in reach of the player */
	void FindReach(const AGutFighter* Walker);

	void BeginTalk(int32 Person);
	void EndTalk();

	/** Moves the talk on from the line it is at: by the answer chosen, or to the line that follows */
	void AdvanceTalk();

	/** Does what a door or an answer does. A door that leads to another place says which. */
	void Do(ETownDeed Deed, const FTownDoor* Door = nullptr);

	/** Borrows from the lender or pays her back, out of the purse the player bets from, which is the one she keeps a page for */
	void Lend(bool bBorrow);

	/** Words with what the player holds and owes written into them: {held}, {owed}, {dealt} */
	FString Told(const FString& Words) const;

	/** Moves the place's calls on */
	void UpdateCalls(float DeltaTime);

	/** Does what a call does as it is called: a lot walked off, the next of them brought up */
	void BeginCall(const FTownCall& Call);

	/** Walks a lot down off where it is sold from to the file of those kept */
	void SellLot(int32 Lot);

	/** Walks every lot that is kept back up to its mark, one after another */
	void BringLotsUp();

	/** Where the lot that is so many back in the file of those kept waits */
	FVector GetKeptPlace(int32 Back) const;

	/** Moves everybody who is being walked somewhere, on whatever is under them */
	void UpdateLed(float DeltaTime);

	/** Moves everybody who walks a round of the place on along it */
	void UpdateBeats(float DeltaTime);

	/** Where somebody is: where they stand, or where their round has taken them */
	FVector GetPersonPlace(int32 Person) const;

	/** The rank of the cage's ladder the player's fighter has reached, or the first if it has none; and the rank whose armor is on the counter */
	const UGutLadderData* GetFighterRank() const;
	const UGutLadderData* GetShelfRank() const;

	UGutGameInstance* GetGutGameInstance() const;

public:

	/** Returns the player's fighter, at ease in the street */
	UFUNCTION(BlueprintPure, Category="Town")
	AGutFighter* GetWalker() const;

	UFUNCTION(BlueprintPure, Category="Town")
	bool IsSettled() const { return bSettled; }

	const UTownPlaceData* GetPlace() const { return PlaceData; }

	float GetWalkerRadius() const { return WalkerRadius; }

	const TArray<TObjectPtr<AGutFighter>>& GetPeople() const { return People; }

	/** The key that talks to whoever is in reach, uses the door in reach, or moves a talk on */
	UFUNCTION(BlueprintCallable, Category="Town")
	void Interact();

	/** Up, down and confirm, for the answers of a talk and for the menu that stops the game */
	void HandleMenuInput(EGutMenuInput Input);

	/** What the player is offered just now ("Talk to Chalk"), or nothing */
	FString GetPrompt() const;

	bool IsTalking() const { return TalkingTo != INDEX_NONE; }

	/** Who is speaking, what they are saying, what can be said back, and which answer the player is on */
	FString GetSpeaker() const;
	FString GetWords() const;
	TArray<FString> GetAnswers() const;
	int32 GetTalkChoice() const { return TalkChoice; }

	/** Words put up by a door that is shut, while they last */
	FString GetNotice() const;

	/** What the player holds and owes, a line for each of the ways it has come to the cage: as a fighter, at the window, as an owner */
	TArray<FString> GetPurses() const;

	/** What is being called out over the place just now, or nothing */
	const FTownCall* GetCall() const;

	/** Which of the place's calls is up */
	int32 GetCallIndex() const { return CallIndex; }

	/** How many of the sale's lots have been walked off and not brought back, and how many people are being walked somewhere now */
	int32 GetLotsKept() const { return KeptLots.Num(); }
	int32 GetLedCount() const { return Led.Num(); }

	/** Opens the armorers' counter, kept by whichever of the people it is, and shuts it */
	void OpenShop(int32 Keeper);
	void CloseShop();

	bool IsShopping() const { return bShopping; }

	/** Who keeps the counter the player is at */
	FString GetShopKeeper() const;

	/** What is on the counter, a line to a piece, in the order it is sold in */
	TArray<FTownShopLine> GetShopLines() const;

	/** Which line the player is on: one past the last piece is the line that leaves */
	int32 GetShopChoice() const { return ShopChoice; }

	/** What the fighter holds to spend there, and whether there is a fighter at all */
	float GetShopWater() const;
	bool HasFighter() const;

	/** What the last thing done at the counter came to, in words */
	const FString& GetShopSaid() const { return ShopSaid; }

	/** Buys the piece on that line with the fighter's own water. Returns false if it cannot be bought, and says why. */
	bool BuyAtShop(int32 Line);

	/** For the tests: how many walk a round, and how far along it one of them is */
	int32 GetBeatCount() const { return Beats.Num(); }

	/** For the tests: has the call before the given one all but run out, so that the given one is called next */
	void SkipToCall(int32 Index);

	/** Stops the game and puts up a menu, or starts it again */
	void SetStopped(bool bStopped);

	bool IsStopped() const { return bStoppedMenu; }

	int32 GetStoppedChoice() const { return StoppedChoice; }

	TArray<FString> GetStoppedLines() const;

	/** For the tests: who and which door is in reach */
	int32 GetPersonInReach() const { return PersonInReach; }
	int32 GetDoorInReach() const { return DoorInReach; }
};
