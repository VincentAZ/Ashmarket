// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutHazard.h"
#include "GutSpikeWall.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 *  A stretch of cage wall with spikes behind it. When it goes live the spikes come out of the wall,
 *  and a fighter that is pushed or walks onto them is hurt and thrown off.
 *
 *  Place it on the face of a wall at floor height, with its forward axis pointing into the cage.
 *  The spikes come out along that axis.
 */
UCLASS()
class AGutSpikeWall : public AGutHazard
{
	GENERATED_BODY()

	/** Root component, on the face of the wall at floor height */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** Steel plate on the wall that the spikes come through */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Plate;

	/** The spikes. Moved out of the wall as a group. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UInstancedStaticMeshComponent* Spikes;

protected:

	/** Length of the stretch of wall the spikes cover */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 50, ClampMax = 5000, Units = "cm"))
	float Width = 800.0f;

	/** Height of the plate the spikes come through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 50, ClampMax = 1000, Units = "cm"))
	float Height = 190.0f;

	/** How far the spikes stand out of the wall when they are out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 5, ClampMax = 300, Units = "cm"))
	float SpikeLength = 45.0f;

	/** Distance between two spikes, along the wall and up it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 10, ClampMax = 500, Units = "cm"))
	float SpikeSpacing = 45.0f;

	/** Width of a spike at the wall */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 1, ClampMax = 100, Units = "cm"))
	float SpikeThickness = 12.0f;

	/** Time the spikes take to come out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 0.01, ClampMax = 5, Units = "s"))
	float ExtendTime = 0.25f;

	/** How far out the spikes come, as a share of their length, as they rattle during the warning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Spikes", meta = (ClampMin = 0, ClampMax = 1))
	float WarningExtent = 0.3f;

	/** Material for the plate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> PlateMaterial;

	/** Material for the spikes while they are in the wall or rattling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> SpikeMaterial;

	/** Material for the spikes once they are out. Must be usable on instanced static meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> LiveSpikeMaterial;

	/**
	 *  A panel of spikes made elsewhere: upright, its width along Y, its spikes pointing along +X.
	 *  It is repeated across the wall in place of the cones, stretched so that a whole number fits and the spikes
	 *  reach as far as Spike Length says, and it slides in and out of the wall as they did. Leave empty for the cones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UStaticMesh> SpikePanelMesh;

	/** The panels standing in for the cones */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SpikePanels;

	/** How far the spikes are out of the wall right now, as a share of their length */
	float Extent = 0.0f;

public:

	/** Constructor */
	AGutSpikeWall();

	/** Rebuilds the plate and spikes whenever a property changes */
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual float GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const override;

	/** Returns how far the spikes are out of the wall, as a share of their length */
	UFUNCTION(BlueprintPure, Category="Gut|Spikes")
	float GetExtent() const { return Extent; }

protected:

	virtual void OnHazardStateChanged(EGutHazardState NewState) override;

	virtual void TickPresentation(float DeltaTime) override;

	/** Sizes the plate and lays out the spikes from the properties */
	void BuildSpikes();

	/** Moves the spikes out of the wall by the given share of their length */
	void SetExtent(float NewExtent);
};
