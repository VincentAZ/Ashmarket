// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutHazard.h"
#include "GutFlood.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  The flooded floor (design document, chapter 7: "hazards the pit boss switches on in later rounds: spiked walls, a live
 *  generator cable, a flooded floor, released dogs"). A valve is opened and the pool floor goes under water to over the ankles.
 *
 *  Water hurts nobody by itself. It slows everybody's feet, shortens a dodge, and carries the live cable's current
 *  well beyond the cable. It is the last of the hazards, so by the time it comes the spikes and the cable are already on.
 */
UCLASS()
class AGutFlood : public AGutHazard
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** The surface of the water */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Water;

protected:

	/** The floor the water covers: length and width, centered on this actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (Units = "cm"))
	FVector2D FloorSize = FVector2D(1400.0f, 1000.0f);

	/** How deep the water stands once the valve has been open long enough */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (ClampMin = 1, ClampMax = 60, Units = "cm"))
	float Depth = 14.0f;

	/** How long the water takes to get there, from the moment the valve is opened: the warning, and a little after */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (ClampMin = 0.5, ClampMax = 60, Units = "s"))
	float RiseTime = 8.0f;

	/** What a fighter keeps of its speed on its feet, and of the length of its dodge, while it stands in the water */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (ClampMin = 0.1, ClampMax = 1))
	float WadeSpeedScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (ClampMin = 0.1, ClampMax = 1))
	float WadeDodgeScale = 0.8f;

	/** How far a live cable's current carries through the water, beyond the cable itself */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Flood", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float CurrentReach = 60.0f;

	/** What the water looks like: a surface made elsewhere */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** The flat sheet the surface is drawn on */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Sheet;

	/** How high the water stands now, from 0 to Depth */
	float Level = 0.0f;

	/** True once the fighters have been told they are standing in it */
	bool bWaded = false;

public:

	AGutFlood();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** How high the water stands now, in cm */
	UFUNCTION(BlueprintPure, Category="Gut|Flood")
	float GetWaterLevel() const { return Level; }

	float GetDepth() const { return Depth; }

protected:

	virtual void OnHazardStateChanged(EGutHazardState NewState) override;
	virtual void TickPresentation(float DeltaTime) override;

	/** Puts the surface where the water now stands */
	void PlaceWater();

	/** Tells everybody in the cage, and the cable, whether the floor is under water */
	void SetEverybodyWading(bool bInWater);
};
