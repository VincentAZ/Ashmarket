// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutHazard.h"
#include "GutDogs.generated.h"

/**
 *  The last thing the pit boss does to a fight that will not end (design document, chapter 7: "released dogs").
 *  It has no place in the cage that hurts: when it goes live the dogs are let in, and they do the hurting themselves.
 *  They are on nobody's side and go for whoever is nearest. Killing one wins nothing but room.
 *
 *  The game mode makes one of these for itself when it has dogs to let in; it is not placed in the level.
 */
UCLASS()
class AGutDogs : public AGutHazard
{
	GENERATED_BODY()

public:

	AGutDogs();

	/** How many are let in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 1, ClampMax = 4))
	int32 Count = 2;

	void SetActivationRound(int32 Round) { ActivationRound = Round; }

protected:

	virtual void OnHazardStateChanged(EGutHazardState NewState) override;
};
