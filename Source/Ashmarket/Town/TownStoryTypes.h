// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TownStoryTypes.generated.h"

/**
 *  What the town knows about the player, kept with the run: one list of named whole numbers, and what has been done.
 *
 *  Everything is a mark. How the player stands with the Wardens is "st.wardens". How far a mission has got is the mark
 *  of its own name ("w1": nought not begun, then its steps, a hundred and over ended). What became of somebody is a mark
 *  ("wynn": 1 held, 2 tagged, 3 waved through). So one way of asking ("st.wardens>=20 w1=100") and one way of changing
 *  ("st.wardens+10 w1=100") serves every line, answer, door and person in every place, and a mission is words in a list
 *  and no code. See ATownGameMode::Holds and ATownGameMode::Apply.
 */
USTRUCT(BlueprintType)
struct FTownStoryState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, int32> Marks;

	/** What has been done, as it would be told, oldest first */
	UPROPERTY()
	TArray<FString> Journal;
};

/** One of the town's powers: the player stands somewhere with each */
USTRUCT(BlueprintType)
struct FTownFaction
{
	GENERATED_BODY()

	/** The end of its mark's name: "wardens" for "st.wardens" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString Name;

	/** Who they are, in a line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString About;
};

/** How a piece of work stands when its mark is at a figure: what is to be done next, or how it ended */
USTRUCT(BlueprintType)
struct FTownMissionStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	int32 At = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString Words;
};

/** A piece of work somebody has for the player */
/**
 *  One of the things a piece of work is made of, for showing how far it has got: "The Sump", done when "w2.sump" holds;
 *  or a count, "Tags read", w1.q of 6. ("show progress on the quest", 25 November.)
 */
USTRUCT(BlueprintType)
struct FTownMissionTally
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString Words;

	/** Done while this holds of the town's marks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString When;

	/** Or: the mark that counts it, and how many it takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FName Count;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	int32 Of = 0;
};

USTRUCT(BlueprintType)
struct FTownMission
{
	GENERATED_BODY()

	/** Its mark: nought is not begun, a hundred and over is ended */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString Name;

	/** Whose work it is: a faction's key, or none for a side job */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FName Faction;

	/** Who gave it, and where they are found */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	FString Giver;

	/** The step that is shown is the last whose figure the mark has reached */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FTownMissionStep> Steps;

	/** What it is made of, in the order it is likely done: see FTownMissionTally. May be empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FTownMissionTally> Tally;
};

/** The town's story as it is written down: its powers and its work. Written by Tools/build_town_story.py from Tools/town_story.json. */
UCLASS(BlueprintType)
class UTownStoryData : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FTownFaction> Factions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TArray<FTownMission> Missions;

	/** What a mark stands at before anything has changed it, where that is not nought: trust begins at 40 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
	TMap<FName, int32> Starts;
};

/** A mission's mark at and over this is a mission ended */
namespace TownStory
{
	inline constexpr int32 Ended = 100;
}
