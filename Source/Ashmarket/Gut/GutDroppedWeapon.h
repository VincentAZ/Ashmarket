// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GutDroppedWeapon.generated.h"

class UStaticMeshComponent;
class UGutWeaponData;

/**
 *  A weapon lying on the cage floor after it has been knocked out of a fighter's hand.
 *  It is only a prop for now: fighters walk through it and nobody can pick it up.
 */
UCLASS()
class AGutDroppedWeapon : public AActor
{
	GENERATED_BODY()

	/** Physics enabled weapon shape */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

	/** The weapon this was */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gut", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGutWeaponData> Weapon;

public:

	/** Constructor */
	AGutDroppedWeapon();

	/** Takes on the look of the given weapon and falls with the given speed */
	void Drop(UGutWeaponData* InWeapon, const FVector& Scale, const FVector& Velocity);

	UFUNCTION(BlueprintPure, Category="Gut")
	UGutWeaponData* GetWeapon() const { return Weapon; }
};
