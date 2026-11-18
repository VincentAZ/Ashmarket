// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GutBrainController.generated.h"

class AGutFighter;

/**
 *  Base for the AI brains that fight in the cage.
 *  Works out when a fight is running and hands the fighter and its opponent to the brain every tick.
 */
UCLASS(abstract)
class AGutBrainController : public AAIController
{
	GENERATED_BODY()

protected:

	/** If false the fighter stands still and does nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Brain")
	bool bBrainEnabled = true;

	/** If true the fight was running last tick */
	bool bWasFighting = false;

public:

	/** Constructor */
	AGutBrainController();

	/** Runs the brain while a fight is on */
	virtual void Tick(float DeltaTime) override;

	/** Switches the brain on or off */
	UFUNCTION(BlueprintCallable, Category="Gut|Brain")
	void SetBrainEnabled(bool bEnabled) { bBrainEnabled = bEnabled; }

	/** Returns true while the brain is fighting: switched on, with a fight running */
	UFUNCTION(BlueprintPure, Category="Gut|Brain")
	bool IsBrainRunning() const { return bBrainEnabled && bWasFighting; }

	/**
	 *  Called by the fighter when a blow is about to land on it, before the blow is judged.
	 *  Gives the brain its turn for this frame early if it has not had it, so that a reaction that is due
	 *  does not miss by a frame just because the attacker's turn came first.
	 */
	virtual void NotifyIncomingAttack(AGutFighter* Fighter, AGutFighter* Attacker);

protected:

	/** Called on the first tick of a fight */
	virtual void OnFightStarted(AGutFighter* Fighter, AGutFighter* Target);

	/** Called every tick of a fight while both fighters are alive */
	virtual void TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime);

	/** Called when the fight stops, the brain is switched off or the fighter is lost */
	virtual void OnFightStopped(AGutFighter* Fighter);
};
