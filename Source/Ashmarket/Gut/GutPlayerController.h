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

public:

	AGutPlayerController();

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

	virtual void PlayerTick(float DeltaTime) override;

	/** Looking up and down is not taken at all while the street's camera is locked: the camera turns, and keeps its height */
	virtual void AddPitchInput(float Val) override;

	/** True while the street's camera is locked at a height and the player is walking under it */
	bool IsCameraHeld() const;

	/** At a betting screen: how long a key is held before the stake starts to run, and how long each litre takes once it does */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0.05, ClampMax = 2, Units = "s"))
	float StakeRunDelay = 0.4f;

	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0.01, ClampMax = 1, Units = "s"))
	float StakeRunEvery = 0.06f;

	float StakeHeldFor = 0.0f;
	float StakeRunFor = 0.0f;

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

	/** Backs out of whatever screen is up, in the cage or in the town, where there is anywhere to back out to */
	void BackPressed();

	/** Puts the town's map up, or takes it down */
	void MapPressed();

	/** I, or right on the D-pad: the bag, in the street */
	void BagPressed();
	void StoppedDownPressed();
	void StoppedConfirmPressed();

	/** Enter: yes to whatever is asked, and on from a fight that is over */
	void EnterPressed();

	/** In the street: the key that talks to whoever is in reach or uses the door in reach, and the one that is held to jog */
	void InteractPressed();
	void JogPressed();
	void JogReleased();
	void RunPressed();

	/** Locks the street's camera at the height it is at now, or lets it go */
	void CameraLockPressed();

	/** Puts the street's camera at the next of its set heights (shoulder, three-quarters overhead, overhead) and locks it there */
	void CameraStepPressed();

	/** When the jog key was last let go: pressed again at once, it is a run */
	double JogLetGo = 0.0;

	/** True while a run was begun by pressing the left stick in, which ends when the stick comes back to the middle */
	bool bRunByStick = false;


	/** Switches the rack inputs on or off */
	void SetAtRack(bool bNewAtRack);

	/** Called when the fight moves to a new stage */
	UFUNCTION()
	void OnPhaseChanged(EGutFightPhase NewPhase);
};
