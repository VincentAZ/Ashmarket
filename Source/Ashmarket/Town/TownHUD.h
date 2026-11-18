// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "GutHUD.h"
#include "TownHUD.generated.h"

class ATownGameMode;

/**
 *  What is drawn over the street: what is in reach and the key that uses it, what somebody is saying and what
 *  can be said back, what the player holds and owes, and the menu that stops the game.
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
	void DrawTownStopped(const ATownGameMode* Town, float CenterX);
};
