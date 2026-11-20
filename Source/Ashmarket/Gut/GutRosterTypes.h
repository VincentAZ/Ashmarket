// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutRosterTypes.generated.h"

class UGutWeaponData;

/**
 *  Somebody who came into the cage after it had killed somebody: new meat, in a dead fighter's rank.
 *  It fights as one of that rank's own fighters does, and is seen and named as nobody else is.
 */
USTRUCT(BlueprintType)
struct FGutNewcomer
{
	GENERATED_BODY()

	/** Where it stands: its rank times 100 plus its place, and its place counts on from the rank's own fights */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	int32 Code = -1;

	UPROPERTY(BlueprintReadOnly, Category="Roster")
	FText FighterName;

	/** One line on who this is */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	FText Billing;

	/** The character that is seen */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	TSoftClassPtr<AActor> Body;

	UPROPERTY(BlueprintReadOnly, Category="Roster")
	TObjectPtr<UGutWeaponData> Weapon;

	/** Which of its rank's own fights it fights like: that one's health, purse, armor and way of fighting */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	int32 Like = 0;
};

/**
 *  Who is alive in the cage, whichever way the player has come to it: as a fighter, at the window, or as an owner.
 *  Somebody who dies in front of one of those is dead for all three, and is not seen again. What each has won
 *  while the player was there to see is kept too, and the book reads it.
 */
USTRUCT(BlueprintType)
struct FGutRosterState
{
	GENERATED_BODY()

	/** Everybody who has died, in the order they did: each as rank times 100 plus place */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	TArray<int32> Dead;

	/** Fights won in the cage since the player came to it, by the same number */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	TMap<int32, int32> Wins;

	/** Whoever has come in since, in the order they came */
	UPROPERTY(BlueprintReadOnly, Category="Roster")
	TArray<FGutNewcomer> Newcomers;
};
