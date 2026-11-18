// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TownTypes.generated.h"

class UAnimSequenceBase;

/** What using a door does, or giving somebody one of the answers they wait for */
UENUM(BlueprintType)
enum class ETownDeed : uint8
{
	/** Nothing but the words */
	None,

	/** Into the cage as a fighter, wherever the ladder had got to */
	Fight,

	/** Into the cage as a fighter, from the first fight, forgetting the run that was kept */
	FightAfresh,

	/** A single fight, with no ladder */
	Spar,

	/** To the window, to bet on the night's fights */
	Bet,

	/** Round to the yard, as an owner of fighters */
	Own,

	/** On foot to another place in the town: the door says which, and where in it */
	Go,

	/** To the Block's clerk as an owner: straight to who is for sale today */
	Buy,

	/** The lender gives water and writes down more than she gave */
	Borrow,

	/** The lender is paid back as much of what is owed as the borrower holds */
	Repay,

	/** To the armorers' counter: what they have made for the cage's fighters, to be bought with a fighter's own water */
	Armor
};

/** One thing the player can say back */
USTRUCT(BlueprintType)
struct FTownAnswer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	FString Words;

	/** What saying it does. Anything but None ends the talk and does it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	ETownDeed Deed = ETownDeed::None;

	/** The line it is answered with: its place in the speaker's lines. Less than nought ends the talk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	int32 Next = -1;
};

/** One thing somebody says, and what can be said back to it */
USTRUCT(BlueprintType)
struct FTownLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	FString Words;

	/** What can be said back. With none, the talk goes on to Next when the player is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	TArray<FTownAnswer> Answers;

	/** The line that follows when there is nothing to answer. Less than nought ends the talk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talk")
	int32 Next = -1;
};

/** One stop on the round somebody walks: where it is, how long they stand there, and which way they look while they do */
USTRUCT(BlueprintType)
struct FTownBeatStop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beat")
	FVector Place = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beat", meta = (ClampMin = 0, Units = "s"))
	float Wait = 0.0f;

	/** Which way they turn to while they stand there. Less than nought is whichever way they came. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beat", meta = (Units = "deg"))
	float Facing = -1.0f;
};

/** Somebody who stands in a place and can be spoken to */
USTRUCT(BlueprintType)
struct FTownPerson
{
	GENERATED_BODY()

	/** What they are called on the screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	FString Name;

	/** What the player is offered when they are in reach: "Talk to Chalk" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	FString Prompt;

	/** The body they are seen as: one of the people built from Epic's presets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	TSoftClassPtr<AActor> Body;

	/** The colour their clothes are dyed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	FLinearColor Cloth = FLinearColor(0.05f, 0.045f, 0.04f);

	/** Where they stand, in the level, and which way they face */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	FVector Place = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person", meta = (Units = "deg"))
	float Facing = 0.0f;

	/** How they stand: a loop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	TObjectPtr<UAnimSequenceBase> Idle;

	/** If true they wear a collar: somebody's property, a lot on the Block */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	bool bCollared = false;

	/** If true they stand on whatever is under them at their place, a truck's deck say, and not on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	bool bStandsOnWhatIsThere = false;

	/** If more than nought they are a lot in the place's sale, and this is their number: they walk off when it is knocked down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person", meta = (ClampMin = 0))
	int32 Lot = 0;

	/**
	 *  The round they walk, if they walk one: from each stop to the next and from the last back to the first, for as long as
	 *  anybody is there to see it. They begin at the first. With none they stand where they are put.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	TArray<FTownBeatStop> Beat;

	/** How fast they walk it, against the pace everybody walks at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person", meta = (ClampMin = 0.2, ClampMax = 2))
	float Pace = 0.85f;

	/** What they say. The talk begins at the first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Person")
	TArray<FTownLine> Lines;
};

/** A way out of a place: a door, a gate, a hatch */
USTRUCT(BlueprintType)
struct FTownDoor
{
	GENERATED_BODY()

	/** What the player is offered when it is in reach: "Go in at the fighters' door" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FString Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FVector Place = FVector::ZeroVector;

	/** What going through it does. With None it is shut, and Shut says why. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	ETownDeed Deed = ETownDeed::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FString Shut;

	/** For a door that goes to another place in the town: that place's level, and which of its ways in to arrive by */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FName To;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FName Arrive;

	/** If true it is an open way and not a door: walking up to it is going through it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	bool bWalkedThrough = false;

	/** How near an open way has to be come to be gone through: as wide as the gap it is. Nought is the place's own figure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta = (ClampMin = 0, Units = "cm"))
	float Reach = 0.0f;

	/**
	 *  For an open way that is the whole width of a street: the stretch of ground that is its mouth, in the level
	 *  (centimetres, X and Y). Whoever walks into it has gone through, wherever across it they are.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FBox2D Mouth = FBox2D(ForceInit);

	/** True if somebody standing there is going through this way, when it is one that is walked through */
	bool IsEntered(const FVector& Where, float OwnReach) const
	{
		return Mouth.bIsValid ? Mouth.IsInside(FVector2D(Where.X, Where.Y)) : FVector::Dist2D(Where, Place) < (Reach > 0.0f ? Reach : OwnReach);
	}
};

/** A way into a place from another: where whoever comes that way is stood, and which way they face */
USTRUCT(BlueprintType)
struct FTownArrival
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival")
	FVector Place = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrival", meta = (Units = "deg"))
	float Facing = 0.0f;
};

/** One thing called out over a place for everybody in it to hear: an auctioneer's patter, a line at a time */
USTRUCT(BlueprintType)
struct FTownCall
{
	GENERATED_BODY()

	/** Who calls it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call")
	FString Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call")
	FString Words;

	/** How long it stays up before the next */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call", meta = (ClampMin = 0.5, ClampMax = 60, Units = "s"))
	float Seconds = 3.0f;

	/** If true the crowd has something to say about it: a lot knocked down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call")
	bool bStirsTheCrowd = false;

	/** The lot this knocks down, which is walked off as it is called. Nought is none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call", meta = (ClampMin = 0))
	int32 Sells = 0;

	/** If true every lot that has been walked off comes back up as it is called: the next of them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Call")
	bool bBringsUp = false;
};

/** Something that stands on the ground of a place and has to be walked round: where it stands, seen from above */
USTRUCT(BlueprintType)
struct FTownBlock
{
	GENERATED_BODY()

	/** Its middle, in the level (centimetres, X and Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block")
	FVector2D Middle = FVector2D::ZeroVector;

	/** Half its length and half its width, along its own two sides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block")
	FVector2D HalfSize = FVector2D::ZeroVector;

	/** How far its own sides are turned from the level's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta = (Units = "deg"))
	float Yaw = 0.0f;

	/** The level's place as the block sees it, with the block square to its own sides and its middle at nought */
	FVector2D ToOwn(const FVector2D& Point) const
	{
		const float Turn = FMath::DegreesToRadians(Yaw);
		const FVector2D Off = Point - Middle;

		return FVector2D((Off.X * FMath::Cos(Turn)) + (Off.Y * FMath::Sin(Turn)), (Off.Y * FMath::Cos(Turn)) - (Off.X * FMath::Sin(Turn)));
	}

	/** How far a place is from the block: nought inside it */
	float GetDistance(const FVector2D& Point) const
	{
		const FVector2D Own = ToOwn(Point);

		return FVector2D(FMath::Max(FMath::Abs(Own.X) - HalfSize.X, 0.0f), FMath::Max(FMath::Abs(Own.Y) - HalfSize.Y, 0.0f)).Size();
	}

	/** Returns the nearest place to Point that is no nearer the block than Clear: Point itself if it is clear of it already */
	FVector2D KeepClear(const FVector2D& Point, float Clear) const
	{
		FVector2D Own = ToOwn(Point);

		const FVector2D OnIt(FMath::Clamp(Own.X, -HalfSize.X, HalfSize.X), FMath::Clamp(Own.Y, -HalfSize.Y, HalfSize.Y));
		const FVector2D Gap = Own - OnIt;
		const float Distance = Gap.Size();

		if (Distance >= Clear)
		{
			return Point;
		}

		if (Distance > UE_KINDA_SMALL_NUMBER)
		{
			Own = OnIt + (Gap * (Clear / Distance));
		}
		else if (HalfSize.X - FMath::Abs(Own.X) < HalfSize.Y - FMath::Abs(Own.Y))
		{
			// inside it: out by whichever side is nearest
			Own.X = (Own.X < 0.0f ? -1.0f : 1.0f) * (HalfSize.X + Clear);
		}
		else
		{
			Own.Y = (Own.Y < 0.0f ? -1.0f : 1.0f) * (HalfSize.Y + Clear);
		}

		const float Turn = FMath::DegreesToRadians(Yaw);

		return Middle + FVector2D((Own.X * FMath::Cos(Turn)) - (Own.Y * FMath::Sin(Turn)), (Own.X * FMath::Sin(Turn)) + (Own.Y * FMath::Cos(Turn)));
	}
};

/**
 *  One place in the town that can be walked in: who stands in it, its ways out, and where the ground ends.
 *  Written by Tools/build_town_level.py from the place's own list, with the level it describes.
 */
UCLASS(BlueprintType)
class UTownPlaceData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** What the place is called on the screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	FString Name;

	/** What there is to do here, in a few short lines, kept on the screen for anybody who has just arrived */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FString> Errand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FTownPerson> People;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FTownDoor> Doors;

	/** The ways in from other places. Somebody who comes by none of them is stood where the level starts its player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FTownArrival> Arrivals;

	/** What is called out over the place, round and round, for as long as anybody is in it: a sale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FTownCall> Calls;

	/**
	 *  The way the lots of a sale come up to where they are sold from, as places in the level gone through in turn: from
	 *  where they are kept, out of the way, to the head of the deck they stand on. A lot that is sold goes back down it,
	 *  and those kept wait in a file that runs back from its first place, away from its second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FVector> LotWay;

	/** How far behind its own mark a lot walks along the line of the others, to get to it or away from it without going through them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta = (ClampMin = 0, Units = "cm"))
	float LotAisle = 70.0f;

	/**
	 *  How high the ground is: level at StreetZ as far north as ClimbFrom (the level's X), and from there climbing
	 *  northward by ClimbRise for each centimetre. Whoever is walked along a street is kept on it by this, and not by
	 *  what can be felt under them, which is only there where the place is drawn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta = (Units = "cm"))
	float StreetZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta = (Units = "cm"))
	float ClimbFrom = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	float ClimbRise = 0.0f;

	float GetGroundZ(float X) const { return StreetZ + (FMath::Max(0.0f, X - ClimbFrom) * ClimbRise); }

	/**
	 *  The ground that can be walked on, as rectangles in the level (centimetres, X and Y). Whoever walks is kept inside
	 *  them. They are only the last word: each runs on into whatever stands along its edge, so that what stops a walker
	 *  is always something that can be seen. Tools/build_town_level.py refuses a place where an edge is open.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FBox2D> Ground;

	/**
	 *  Everything that stands on that ground and is walked round: each wall, container, fence, barrel. The models have no
	 *  shape of their own to stop anybody, so the place keeps where each one stands, measured from the model itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place")
	TArray<FTownBlock> Blocks;
};
