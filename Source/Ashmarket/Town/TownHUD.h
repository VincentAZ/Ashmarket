// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "GutHUD.h"
#include "TownHUD.generated.h"

class ATownGameMode;

/**
 *  What is drawn over the street: what is in reach and the key that uses it, what somebody is saying and what
 *  can be said back, what is on a counter, what the player holds and owes, and the menu that stops the game.
 *  It is the cage's HUD for its panels and its type, and draws none of the cage's screens.
 */
UCLASS()
class ATownHUD : public AGutHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

protected:

	/** The most letters a line of talk has before it is carried over to the next */
	UPROPERTY(EditAnywhere, Category="Town", meta = (ClampMin = 20, ClampMax = 200))
	int32 TalkLineLength = 68;

	/** Cuts words into lines no longer than a number of letters, at the spaces */
	static TArray<FString> Wrap(const FString& Words, int32 Length);

	void DrawTalk(const ATownGameMode* Town, float CenterX);
	void DrawShop(const ATownGameMode* Town, float CenterX);

	/** The Block's book: the list of who is for sale, with whoever the choice is on pictured on the card and told about beside it */
	void DrawBook(const ATownGameMode* Town, float CenterX);
	void DrawTownStopped(const ATownGameMode* Town, float CenterX);

	/** The town's own map, with the player marked on it */
	void DrawMap(const ATownGameMode* Town, float CenterX);

	/** The player's bag, open: everything it has on it down the left, and the thing it is on, pictured, on the right */
	void DrawBag(const ATownGameMode* Town, float CenterX);

	/** One of the pictures the bag's things are shown by, fetched the first time it is wanted. Nothing if there is none of that name. */
	UTexture2D* GetBagPicture(const FName& Name);

	/** The map's picture, once fetched */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapPicture;

	/** The bag's pictures, by name, as they are fetched */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> BagPictures;
};
