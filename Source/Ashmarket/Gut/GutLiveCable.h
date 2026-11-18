// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutHazard.h"
#include "GutLiveCable.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UNiagaraSystem;

/**
 *  A generator against the cage wall, with a heavy cable running from it across the floor.
 *  When the generator is switched on, a fighter that steps or is knocked onto the cable is shocked:
 *  hurt, left convulsing for longer than any blow stuns, and thrown off it.
 *  A fighter can dodge across the cable without touching it.
 *
 *  Place it on the cage floor against a wall, with its forward axis pointing into the cage. The cable runs along that axis.
 */
UCLASS()
class AGutLiveCable : public AGutHazard
{
	GENERATED_BODY()

	/** Root component, on the floor at the face of the wall */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** The generator box. Fighters cannot walk through it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Generator;

	/** The cable lying on the floor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Cable;

protected:

	/** Length of cable lying on the floor, from the front of the generator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cable", meta = (ClampMin = 50, ClampMax = 5000, Units = "cm"))
	float Length = 600.0f;

	/** How far the current carries beyond the cable itself. Nothing on dry tile; something when the floor is under water. */
	float WetReach = 0.0f;

	/** Thickness of the cable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cable", meta = (ClampMin = 1, ClampMax = 50, Units = "cm"))
	float CableThickness = 9.0f;

	/** Size of the generator: depth out of the wall, width along it, height */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cable", meta = (Units = "cm"))
	FVector GeneratorSize = FVector(70.0f, 90.0f, 100.0f);

	/** Effect spawned at random points along the live cable, as sparks. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UNiagaraSystem> SparkEffect;

	/** Time between two sparks along the live cable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0.02, ClampMax = 10, Units = "s"))
	float SparkInterval = 0.35f;

	/** Material for the generator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> GeneratorMaterial;

	/** A generator made elsewhere, standing on its own base with its long side along X. It is shown at its own size in place of the box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UStaticMesh> GeneratorMesh;

	/** The box that stands in for a generator */
	UPROPERTY()
	TObjectPtr<UStaticMesh> GeneratorBox;

	/**
	 *  A length of real cable made elsewhere, lying along X. It is laid end to end in place of the cylinder, stretched a little
	 *  so a whole number covers the length. Being real it cannot change colour: when it is live, it arcs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UStaticMesh> CableMesh;

	/** The lengths of real cable */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CableSections;

	/** The arcs along a live cable */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UPointLightComponent>> Arcs;

	/** Material for the cable while it is dead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> CableMaterial;

	/** Material for the cable while it is live. The cable flickers between this and the dead one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> LiveCableMaterial;

	/** Time of the next spark */
	float NextSparkTime = 0.0f;

	/** Time the cable next changes between lit and dark while it flickers */
	float NextFlickerTime = 0.0f;

	/** If true the cable is showing its live material */
	bool bShowingLit = false;

public:

	/** Sets how far the current carries through water on the floor, in cm. Zero for a dry floor. */
	void SetWetReach(float Reach) { WetReach = FMath::Max(Reach, 0.0f); }
	float GetWetReach() const { return WetReach; }


	/** Constructor */
	AGutLiveCable();

	/** Rebuilds the generator and cable whenever a property changes */
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual float GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const override;

	/** The cable runs out from a wall, so the way round it is past its free end */
	virtual bool GetWayRound(const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint) const override;

	/** Returns the two ends of the cable on the floor, the generator end first */
	UFUNCTION(BlueprintPure, Category="Gut|Cable")
	void GetCableEnds(FVector& OutStart, FVector& OutEnd) const;

protected:

	virtual void OnHazardStateChanged(EGutHazardState NewState) override;

	virtual void TickPresentation(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual FVector GetSoundPlace() const override;

	/** Sizes the generator and lays the cable from the properties */
	void BuildCable();

	/** Shows the cable lit or dark */
	void ShowLit(bool bLit);
};
