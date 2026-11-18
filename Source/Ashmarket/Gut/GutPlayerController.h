// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GutTypes.h"
#include "GutPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 *  Player Controller for a cage fight.
 *  Manages input mappings, the inputs for the screens before and after a fight, and the restart input.
 */
UCLASS(abstract)
class AGutPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts. Later entries take priority over earlier ones. */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Restart Input Action. Only does something once the fight is over. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RestartAction;

	/**
	 *  Input Mapping Context for the screens before and after a fight: the card, the weapon rack, the betting window and the gate.
	 *  Only added while one of them is up, on top of all the others.
	 */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputMappingContext* RackMappingContext;

	/** Rack Input Action: draw the weapon before this one */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputAction* RackPreviousAction;

	/** Rack Input Action: draw the weapon after this one */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputAction* RackNextAction;

	/** Rack Input Action: up. At the rack in a single fight it changes what the opponent draws, which is a convenience for trying matchups. */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputAction* RackOpponentAction;

	/** Rack Input Action: down */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputAction* RackDownAction;

	/** Rack Input Action: take the drawn weapon into the fight */
	UPROPERTY(EditAnywhere, Category="Input|Rack")
	UInputAction* RackConfirmAction;

	/** If true one of the screens is up: the rack inputs are live and walking is switched off */
	bool bAtRack = false;

	/** Camera pitch when the fight starts */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = -89, ClampMax = 89, Units = "deg"))
	float StartingCameraPitch = -14.0f;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called for restart input */
	void RestartPressed();

	/** Called for the rack inputs */
	void RackPreviousPressed();
	void RackNextPressed();
	void RackOpponentPressed();
	void RackDownPressed();
	void RackConfirmPressed();

	/** Passes one of the rack inputs on to whichever screen is up */
	void SendMenuInput(EGutMenuInput Input);

	/** The key that stops the game and starts it again, and the three that work the menu while it is stopped */
	void StopPressed();
	void StoppedUpPressed();
	void StoppedDownPressed();
	void StoppedConfirmPressed();

	/** Enter: yes to whatever is asked, and on from a fight that is over */
	void EnterPressed();

	/** In the street: the key that talks to whoever is in reach or uses the door in reach, and the one that is held to jog */
	void InteractPressed();
	void JogPressed();
	void JogReleased();

	/** Switches the rack inputs on or off */
	void SetAtRack(bool bNewAtRack);

	/** Called when the fight moves to a new stage */
	UFUNCTION()
	void OnPhaseChanged(EGutFightPhase NewPhase);
};
