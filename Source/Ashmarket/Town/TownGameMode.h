// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GutTypes.h"
#include "TownTypes.h"
#include "TownGameMode.generated.h"

class ACameraActor;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class AGutFighter;
class UAudioComponent;
class AGutGameMode;
class ULightComponent;
class UGutGameInstance;
struct FGutStableFighter;
struct FGutStableState;
class UGutLadderData;
class UIKRetargeter;
class USoundBase;
class UAnimSequenceBase;

/** The lines of the menu that stops the game in the street */
namespace TownStoppedLines
{
	inline constexpr int32 GoOn = 0;

	/** The bag and the map each have a key, and are here too for whoever has not found it */
	inline constexpr int32 Bag = 1;
	inline constexpr int32 Map = 2;
	inline constexpr int32 Sound = 3;
	inline constexpr int32 Speech = 4;
	inline constexpr int32 Leave = 5;
	inline constexpr int32 Count = 6;
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

/** Somebody the player has bought or signed and not yet brought to their yard: they come along behind, wherever the player goes */
struct FTownTow
{
	/** Which of the stable's fighters they are, and which line of the day's book they were */
	int32 Fighter = INDEX_NONE;
	int32 Offer = INDEX_NONE;

	/** The places they first go through to get down to the ground, off the Block, before they follow anybody */
	TArray<FVector> Way;

	/** How long before they set off */
	float Wait = 0.0f;

	/** Where the player's feet have been since, oldest first. They are walked along it, and so round whatever the player went round. */
	TArray<FVector> Trail;

	/** The same marks where the player's own feet were, and not to their side: the way that is known to be free */
	TArray<FVector> Own;

	/** How long they have been getting nowhere, and whether they have been put on the player's own way for it */
	float Held = 0.0f;
	bool bOnOwnWay = false;
};

/** A light in the place that is a fire: how bright the list made it, about which it wavers */
struct FTownFire
{
	TWeakObjectPtr<ULightComponent> Light;

	float Brightness = 0.0f;

	/** Where in the wavering it is, so that no two fires waver together */
	float Phase = 0.0f;
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

	/** The price in words, where it is not so many litres down: a wage, say */
	FString PriceText;

	/** More about it, shown while the player is on its line */
	FString About;

	/** The player has it on already */
	bool bWorn = false;

	/** It can be bought now. If not, Why says why not. */
	bool bCanBuy = false;
	FString Why;
};

/** What sort of thing a thing in the bag is */
enum class ETownBagKind : uint8
{
	/** Water in hand, which is what everything is paid in: one for each way the player has come to the cage */
	Water,

	/** Somebody the player owns, or pays: the paper that says so. They themselves walk behind, or are in the yard. */
	Person,

	/** Something bought at a stall, to be used or given */
	Good,

	/** Armor the player's fighter has on */
	Worn,

	/** The map of the town */
	Map,

	/** What is owed, written down by whoever is owed it */
	Note
};

/** One thing in the bag, as the bag's screen shows it */
struct FTownBagLine
{
	ETownBagKind Kind = ETownBagKind::Good;

	/** The part of the bag it is in, which is written over the first thing of each part */
	FString Part;

	FString Name;

	/** How many, or how much, in a word. Empty for one of a thing there is only one of. */
	FString Count;

	/** What it is for, or how it stands, in a few words beside its name */
	FString Detail;

	/** More about it, beside its picture, a line at a time */
	TArray<FString> About;

	/** The picture of it, by name, among the pictures the screens are drawn with */
	FName Picture;

	/** What the key that says yes does with it, in a word or two. Empty if there is nothing to be done with it from here. */
	FString Verb;

	/** It can be done now. If not, Why says why not. */
	bool bCanUse = false;
	FString Why;

	/** Which good it is, or which of the player's people */
	FName Good;
	int32 Fighter = INDEX_NONE;
};

/** Somebody a thing out of the bag can be used on, or given to */
struct FTownBagTaker
{
	FString Name;

	/** Which of the player's people. None for the player's own fighter. */
	int32 Fighter = INDEX_NONE;

	bool bCan = false;
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

	/** The cage's rules, as its game mode's defaults hold them: who is at the Block, what they cost and what an owner comes with are read off these */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	TSoftClassPtr<AGutGameMode> CageRules;

	/** Whoever is in tow keeps this far behind the player, and each of them this far behind the one in front */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 50, ClampMax = 500, Units = "cm"))
	float TowGap = 130.0f;

	/**
	 *  And this far to one side of the way the player went, the first to the left and the next to the right: at the
	 *  player's shoulder, where they can be seen, and not between the player and whoever is looking over the player's back
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0, ClampMax = 300, Units = "cm"))
	float TowAside = 80.0f;

	/** How near the player, in a straight line, whoever is led may stand: nearer, they step out of the way. Two bodies' room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 0, ClampMax = 300, Units = "cm"))
	float TowApart = 105.0f;

	/** The player's way is marked this often for them to follow, and they hurry when they are this much further behind than they should be */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 10, ClampMax = 200, Units = "cm"))
	float TowCrumb = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town", meta = (ClampMin = 50, ClampMax = 2000, Units = "cm"))
	float TowHurry = 250.0f;

	/** The colour of what somebody sold at the Block is dressed in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Town")
	FLinearColor TowCloth = FLinearColor(0.09f, 0.085f, 0.075f);

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

	/** Which of the place's finds is near enough to pick up, and what stands in the level for each of them: none for one that has been taken */
	int32 FindInReach = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> FindActors;

	/** Puts out whatever of the place's finds nobody has picked up yet */
	void LayFinds();

	/** The name a find is remembered as picked up under: the place's, and which of its finds */
	FName GetFindName(int32 Find) const;

	/** Who is being talked to, which of their lines they are at, and which answer the player is on */
	int32 TalkingTo = INDEX_NONE;
	int32 TalkLine = 0;
	int32 TalkChoice = 0;

	/** The last of their lines that had answers to it: what can still be said when something said in answer has nothing after it */
	int32 TalkHub = INDEX_NONE;

	/** What has been asked already in this talk: the line times a hundred, plus the answer */
	TSet<int32> TalkAsked;

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

	/** Which of the lender's lines says how the last dealing came out */
	FString DealtId;

	/** Everybody who is being walked somewhere just now */
	TArray<FTownLed> Led;

	/** Everybody who walks a round of the place */
	TArray<FTownBeat> Beats;

	/** Everybody who is in tow, nearest the player first, and the body of each */
	TArray<FTownTow> Tow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGutFighter>> TowBodies;

	/** Every light in the place that is a fire: the drums, the lanterns */
	TArray<FTownFire> Fires;

	/** Whose counter the player is at, as their place among the people: less than nought is nobody's */
	int32 ShopKeeper = INDEX_NONE;

	/** True while the player is at a counter, which of its lines it is on, and what the last thing done there came to */
	bool bShopping = false;
	int32 ShopChoice = 0;
	FString ShopSaid;

	/** True if the counter is the Block's book, where what is bought is people, and not the armorers' */
	bool bAtBook = false;

	/** True if it is a stall, where what is bought goes into the bag; and true if it is no counter at all but the player's own bag, open */
	bool bAtStall = false;
	bool bAtBag = false;

	/**
	 *  The yard, where it is a place that is walked about in (a place with stalls): the bodies of the player's own people
	 *  standing in it, which of the stable each is, the one in reach, and the one whose page is up.
	 */
	UPROPERTY()
	TArray<TObjectPtr<AGutFighter>> YardBodies;

	TArray<int32> YardFighters;

	int32 YardInReach = INDEX_NONE;
	int32 YardFocus = INDEX_NONE;

	/** What in the place is loose, to be shoved by whoever walks into it, and whether each is on the move */
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Loose;
	TArray<bool> LooseMoving;
	TArray<FTransform> LooseBegan;

	/** Who was in reach a moment ago, and when each person last nodded to the player walking up */
	int32 NoddedAt = INDEX_NONE;
	TMap<int32, double> Nodded;
	bool bAtYardPage = false;

	/** Stands the player's people in their places in the yard */
	void SettleYard();

	/** Puts up what can be done with one of them: which of the bodies standing there */
	void OpenYardPage(int32 Which);

	/** The lines of that page, and what each does (GutStableLines) */
	void GetYardPage(TArray<FTownShopLine>& OutLines, TArray<int32>& OutCodes) const;

	bool DoYardLine(int32 Line);

	/** What a day costs the player as things stand: a mouth's feed each, and the wage of whoever is paid */
	float GetDayDue() const;

	/**
	 *  After a day has gone by, which may have lost the player somebody (not paid, they walk; past bearing, they are gone
	 *  in the night). Before is the stable as it was. Every body in the yard and behind the player is matched again to who
	 *  it is in the stable now, whoever is gone leaves the place, and what comes back says how the day was paid for and
	 *  who went. On 23 November somebody not paid walked on paper and went on standing in the yard as nobody: the body of
	 *  the one after them could not be spoken to, and theirs answered as that one.
	 */
	FString SettleAfterDay(const TArray<FGutStableFighter>& Before, float DebtBefore);

	/** True while the bag is asking who a thing is for, which thing, and which of the answers the player is on */
	bool bBagAsking = false;
	FName BagAsked;
	int32 BagAskChoice = 0;

	/** What looks at whoever the book's line is about, while the book is open, and which line it was last turned to */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BookCamera;

	/**
	 *  The eye that looks at whoever the book's line is about, and the picture it makes of them, which is put on the card:
	 *  the player's own view stays the player's (22 November: "show them on the card - not outside the card").
	 */
	UPROPERTY()
	TObjectPtr<ASceneCapture2D> BookEye;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> BookPicture;

	/** The lamps turned on whoever is pictured */
	UPROPERTY()
	TArray<TObjectPtr<class ULightComponent>> BookLamps;

	/** Whoever the book's line is about, stood for their picture out of sight of the place; whose body it is; and when it was stood there */
	UPROPERTY()
	TObjectPtr<AGutFighter> BookSitter;

	FString BookSitting;
	float BookSatAt = 0.0f;

	/** True while the eye has somebody in front of it */
	bool bBookPictured = false;

	int32 BookShown = INDEX_NONE;

	/** Whoever is in the book and is nobody's lot, stood by the clerk's table for as long as the book is open, to be looked at */
	UPROPERTY(Transient)
	TObjectPtr<AGutFighter> BookSigner;

	/** True while the town's map is up */
	bool bMapUp = false;

	/** Whoever is being heard just now: somebody talked to, and whoever is calling over the place */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TalkVoice;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CallVoice;

	/** The line whose voice was last begun, so that a line is said once when it is come to: who, and which of their lines */
	int32 SaidPerson = INDEX_NONE;
	int32 SaidLine = INDEX_NONE;

	/** How long the call that is up takes to say, if it is spoken: it is not taken down before it has been said */
	float CallSpoken = 0.0f;

	/** What somebody who owns nobody yet would have their days at the Block drawn from, once they buy */
	int32 BookSeed = 0;

	/** The cage's ladder, once read */
	UPROPERTY(Transient)
	TObjectPtr<UGutLadderData> LadderData;

	/** The lots that have been walked off and are kept, as their places among the people, in the order they went: the first is at the far end of the file */
	TArray<int32> KeptLots;

	/** Finds the player's fighter and stands the people in their places, once both can be done */
	void Settle();

	/** Keeps whoever walks on the ground the place has, and out of everything that stands on it */
	void KeepOnGround(AGutFighter* Walker) const;

	/** Shuts the bag, turns the camera to whoever in tow was just handed something out of it, and has them take it */
	void ShowGiven(int32 Fighter, const FName& Good, const FString& Said);

	/** When the counter last refused something, by the world's clock */
	float ShopRefusedAt = 0.0f;

	/** Whoever was last handed something, and until when the camera stays on them */
	TWeakObjectPtr<AGutFighter> GivenTo;
	float GivenUntil = 0.0f;

	/** The nearest place to this one that is on the ground and clear of everything that stands in the place: itself, if it is */
	FVector2D GetKeptClear(const FVector2D& Flat) const { return GetKeptClear(Flat, WalkerRadius); }

	/** The same for something of another size: what is loose in a place is kept on its ground and out of what stands in it as a walker is */
	FVector2D GetKeptClear(const FVector2D& Flat, float Radius) const;

	/** Works out who or what is in reach of the player */
	void FindReach(const AGutFighter* Walker);

	void BeginTalk(int32 Person);
	void EndTalk();

	/** Moves the talk on from the line it is at: by the answer chosen, or to the line that follows */
	void AdvanceTalk();

	/** Says the line the talk is at, if it has a voice and has not been said since it was come to; stops whoever was speaking before */
	void SayTalkLine();

	/**
	 *  The line whose answers the player has in front of them: the one being said, or, when that was said in answer to
	 *  something and has nothing after it, the one it was an answer to. A talk with answers is left by the player, and
	 *  not by running out.
	 */
	const FTownLine* GetAnsweredLine() const;

	/** The cage's rules, or nothing if they cannot be found */
	const AGutGameMode* GetCageRules() const;

	/** The player's stable as it stands, or as it would on the day they began one */
	FGutStableState GetOwnerState() const;

	/** Who is in the Block's book today, and which of the place's lots each is: nought for somebody who is no lot */
	void GetBookOffers(TArray<FGutStableFighter>& OutOffers, TArray<int32>& OutLots) const;

	TArray<FTownShopLine> GetBookLines() const;
	bool BuyAtBook(int32 Line);

	/** Turns the book's camera on whoever the line the player is on is about: the lot on the Block, or the one who signs */
	void ShowBookLine();

	/** Gives the player its own eyes back, and sends whoever was stood by the table away */
	void EndBookShow();

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

	/** What a stall has for sale, as lines of the same panel a counter has */
	TArray<FTownShopLine> GetStallLines() const;

	bool BuyAtStall(int32 Line);

	/** The keys that choose and say yes, while the bag is open */
	void HandleBagInput(EGutMenuInput Input);

	/**
	 *  The water the market is paid in: the fighter's if the player has one, or an owner's, or a gambler's, in that order.
	 *  Nothing if the player has come to the cage as none of them yet. Whose says which, in words.
	 */
	float* GetMarketWater(FString* Whose = nullptr) const;

	/** Dyes what somebody is wearing */
	static void Dye(const AGutFighter* Body, const FLinearColor& Cloth);

	/** Stands somebody who is owned or signed in the place: collared and tied if they are owned */
	AGutFighter* StandOwned(const FGutStableFighter& Fighter, const FTransform& Where);

	/** Has somebody come along behind the player from now on: by Way first, if they have to get down from somewhere */
	void TakeInTow(AGutFighter* Body, int32 Fighter, int32 Offer, TArray<FVector> Way = TArray<FVector>(), float Wait = 0.0f);

	/** Takes off the Block whoever the player bought from it today, and stands behind the player whoever is in tow, when the place is come to */
	void SettleOwned();

	/** Moves everybody in tow along after the player: at a walk, or a jog when the player jogs or they have fallen behind */
	void UpdateTow(float DeltaTime);

	/** Whoever is in tow is in the yard from now on: the player has gone in at the Gut */
	void HandInTow();

	/** True if the lot of that number is not there to be sold: the player has bought it */
	bool IsLotGone(int32 Lot) const;

	/** Finds the place's fires, which the builder marks, and makes them waver */
	void FindFires();
	void UpdateFires();

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

	/** The performance of the face that goes with a recording of speech, if one has been made: Epic's speech-to-face, run by Tools/make_town_faces.sh */
	UAnimSequenceBase* GetFaceFor(const USoundBase* Recording) const;

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

	/** The key that backs out: takes the map down, shuts a counter or a book, leaves a talk */
	void HandleBack();

	/** Puts the map up, or takes it down. Not while somebody is being talked to. */
	void ToggleMap();

	bool IsMapUp() const { return bMapUp; }

	/**
	 *  Where the player is on the map's picture, as shares of its width and height from its top left. The map is the
	 *  town's own, scratched on a car bonnet, and not to scale: each place has a patch of it, and the player is put
	 *  in that patch where they are in the place. False if this place is not on it.
	 */
	bool GetMapSpot(FVector2D& OutSpot) const;

	/** What the player owns that it might be looking for, in words for the top of the screen, or nothing */
	FString GetOwnedLine() const;

	/** What the player is offered just now ("Talk to Chalk"), or nothing */
	FString GetPrompt() const;

	bool IsTalking() const { return TalkingTo != INDEX_NONE; }

	/** Who is speaking, what they are saying, what can be said back, and which answer the player is on */
	FString GetSpeaker() const;
	FString GetWords() const;

	/** What is written under the line and not spoken: the day's figures */
	FString GetTalkNote() const;

	/** True if what is being said is heard and the player has chosen to hear and not read: then only the answers are written */
	bool IsWordsUnwritten() const;
	TArray<FString> GetAnswers() const;
	int32 GetTalkChoice() const { return TalkChoice; }

	/** Which of those answers has been given already in this talk */
	TArray<bool> GetAnswersAsked() const;

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

	/** Opens the Block's book at its clerk's table: who is for sale today, to be bought there with an owner's water */
	void OpenBook(int32 Keeper);

	/** Opens the stall of whoever keeps it: what they sell, bought with water in hand, into the bag */
	void OpenStall(int32 Keeper);

	/** Opens the player's bag, or shuts it: what is carried, and the using of it. Not while anything else is up. */
	void ToggleBag();

	bool IsBagOpen() const { return bShopping && bAtBag; }

	/**
	 *  Everything the player has on it, as the bag's screen shows it: water, the papers for whoever it owns or pays,
	 *  what it has bought, what its fighter wears, the map, and what it owes. There is always the map.
	 */
	TArray<FTownBagLine> GetBagLines() const;

	/** Which of them the player is on, and what was last said about any of them */
	int32 GetBagChoice() const { return ShopChoice; }
	FString GetBagSaid() const { return ShopSaid; }

	/**
	 *  Does what can be done with a thing in the bag. A good that more than one could have is asked about first:
	 *  the answer is given to GiveFromBag. False if nothing was done.
	 */
	bool UseFromBag(int32 Line);

	/** Who a good could be used on or given to: the player's own fighter, and whoever of its people is walking with it */
	TArray<FTownBagTaker> GetBagTakers(const FName& Good) const;

	/** Uses a good on the player's own fighter, or gives it to one of its people who is with it. False if it would be wasted: it is kept. */
	bool GiveFromBag(const FName& Good, int32 Fighter);

	/** True while the bag is asking who a thing is for; then who could have it, and which the player is on. The line after the last is nobody. */
	bool IsBagAsking() const { return IsBagOpen() && bBagAsking; }
	FName GetBagAsked() const { return BagAsked; }
	int32 GetBagAskChoice() const { return BagAskChoice; }

	/** What the key that says yes does at the counter the player is at, in a word or two: "buy it", "use it" */
	FString GetShopVerb() const;

	/** What is written over the counter the player is at, and whose water is spent there, in words */
	FString GetShopTitle() const;
	FString GetShopPurse() const;

	bool IsShopping() const { return bShopping; }

	/** Who keeps the counter the player is at */
	FString GetShopKeeper() const;

	/** What is on the counter, a line to a piece, in the order it is sold in */
	TArray<FTownShopLine> GetShopLines() const;

	/** Which line the player is on: one past the last piece is the line that leaves */
	int32 GetShopChoice() const { return ShopChoice; }

	/** Puts a line up on the street for a few seconds */
	void ShowNotice(const FString& Words);

	/** How long ago the counter last refused something, in seconds: a long time if it never has */
	float GetShopRefusedAgo() const { return ShopRefusedAt > 0.0f ? GetWorld()->GetTimeSeconds() - ShopRefusedAt : 1000.0f; }

	bool IsAtBook() const { return bShopping && bAtBook; }

	bool IsAtYardPage() const { return bShopping && bAtYardPage; }

	/** One of the player's own people standing in their yard, by their place among those standing there */
	const AGutFighter* GetYardBody(int32 Index) const { return YardBodies.IsValidIndex(Index) ? YardBodies[Index].Get() : nullptr; }

	/** What in the place can be shoved about, for the tests */
	const TArray<TWeakObjectPtr<UStaticMeshComponent>>& GetLoose() const { return Loose; }

	/** True in a place that is the player's own yard */
	bool IsYard() const { return PlaceData && !PlaceData->Stalls.IsEmpty(); }

	/** The picture of whoever the book's chosen line is about, as they stand there now: none if there is nobody to see */
	UTextureRenderTarget2D* GetBookPicture() const { return (IsAtBook() && bBookPictured) ? BookPicture.Get() : nullptr; }

	/** Who a line of the book is about, and the number of their lot (0 for somebody who is nobody's lot). False if there is no such line. */
	bool GetBookPerson(int32 Line, FGutStableFighter& OutPerson, int32& OutLot) const;

	/** How old somebody looks to be: the same answer for the same person every time */
	static int32 GetAgeOf(const FGutStableFighter& Person);

	/** What the fighter holds to spend there, and whether there is a fighter at all */
	float GetShopWater() const;
	bool HasFighter() const;

	/** What the last thing done at the counter came to, in words */
	const FString& GetShopSaid() const { return ShopSaid; }

	/** Buys the piece on that line with the fighter's own water. Returns false if it cannot be bought, and says why. */
	bool BuyAtShop(int32 Line);

	/** How many are in tow behind the player, and the body of one of them */
	int32 GetTowCount() const { return Tow.Num(); }
	const AGutFighter* GetTowBody(int32 Index) const { return TowBodies.IsValidIndex(Index) ? TowBodies[Index].Get() : nullptr; }

	/** For the tests: how many walk a round, and how far along it one of them is */
	int32 GetBeatCount() const { return Beats.Num(); }

	/** For the tests: how many fires burn in the place */
	int32 GetFireCount() const { return Fires.Num(); }

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
	int32 GetFindInReach() const { return FindInReach; }

	/** Where each thing that still lies about the place to be found is, in the world: a little above the middle of it */
	TArray<FVector> GetFindsLying() const;

	/** Picks up one of the place's finds: it goes in the bag and is not there again. @return true if it was there to take */
	bool TakeFind(int32 Find);
};
