// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutBrainController.h"
#include "GutDummyController.generated.h"

/**
 *  The simplest brain that can still kill: walks at its opponent and swings on a timer.
 *  It never blocks, dodges or backs off. Kept as a punching bag now that the Gut Opponent Controller does the real fighting.
 */
UCLASS()
class AGutDummyController : public AGutBrainController
{
	GENERATED_BODY()

protected:

	/** Shortest wait between two attacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MinAttackInterval = 1.5f;

	/** Longest wait between two attacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MaxAttackInterval = 2.3f;

	/** Wait after the bell before the first attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float FirstAttackDelay = 1.0f;

	/** Every Nth attack is heavy. Zero means never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 20))
	int32 HeavyAttackEvery = 3;

	/** How fast the dummy walks, as a share of its fighter's walking speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 1))
	float ApproachSpeedScale = 0.45f;

	/** The dummy walks in until the opponent is inside this share of its attack reach, then stops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0.1, ClampMax = 1))
	float StopAtReachFraction = 0.7f;

	/** The dummy attacks when the opponent is inside this share of its attack reach */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0.1, ClampMax = 1.5))
	float AttackAtReachFraction = 0.9f;

	/** Turn speed while standing in front of the opponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dummy", meta = (ClampMin = 0, ClampMax = 3600))
	float TurnRate = 240.0f;

	/** Time left until the dummy may attack again */
	float AttackCooldown = 0.0f;

	/** Attacks thrown so far */
	int32 AttackCount = 0;

protected:

	// ~begin GutBrainController interface

	virtual void OnFightStarted(AGutFighter* Fighter, AGutFighter* Target) override;

	virtual void TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime) override;

	// ~end GutBrainController interface
};
