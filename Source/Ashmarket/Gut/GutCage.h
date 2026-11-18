// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GutCage.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 *  The cage room as grey boxes: an emptied pool with a welded lid of bars.
 *  Everything is built from cubes sized by the properties below, so the room can be reshaped without modeling.
 */
UCLASS()
class AGutCage : public AActor
{
	GENERATED_BODY()

	/** Root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** Pool floor, including the ground under the walls */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Floor;

	/** Pool wall on the +X side */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WallFront;

	/** Pool wall on the -X side */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WallBack;

	/** Pool wall on the +Y side */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WallRight;

	/** Pool wall on the -Y side */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WallLeft;

	/** Bars of the lid */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UInstancedStaticMeshComponent* LidBars;

	/** Keeps the camera under a roof of wire, which hides the fight from above as bars did not */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* LidCameraStop;

protected:

	/** Size of the fighting floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 200, ClampMax = 10000, Units = "cm"))
	FVector2D FloorSize = FVector2D(1400.0f, 1000.0f);

	/** Depth of the pool, which is also the height of the lid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 100, ClampMax = 2000, Units = "cm"))
	float WallHeight = 320.0f;

	/** Thickness of the walls. Thick walls read as the deck around the pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 10, ClampMax = 2000, Units = "cm"))
	float WallThickness = 300.0f;

	/** Thickness of the floor slab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 10, ClampMax = 1000, Units = "cm"))
	float FloorThickness = 50.0f;

	/** Distance between two bars of the lid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 20, ClampMax = 2000, Units = "cm"))
	float BarSpacing = 140.0f;

	/** Thickness of one bar of the lid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage", meta = (ClampMin = 1, ClampMax = 100, Units = "cm"))
	float BarThickness = 7.0f;

	/** Material for the floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	/** Material for the walls */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UMaterialInterface> WallMaterial;

	/** Material for the lid. Must be usable on instanced static meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UMaterialInterface> BarMaterial;

	/**
	 *  A section of fencing made elsewhere, standing upright with its width along X, to roof the cage with.
	 *  It is laid flat and repeated across the opening, stretched a little so a whole number fits. Leave empty for the welded grid of bars.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UStaticMesh> LidPanelMesh;

	/** Material for the deck round the pool, where it is not the pool's own tile. Leave empty for a deck of the wall's tile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UMaterialInterface> DeckMaterial;

	/**
	 *  One coping stone made elsewhere, lying flat with its length along X and its rounded edge toward -Y.
	 *  It is laid end to end round the pool's rim, rounded edge to the water, stretched a little so a whole number fits each side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	TObjectPtr<UStaticMesh> CopingMesh;

	/** Turn the coping stones end for end, if the model's rounded edge is on its other side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Cage")
	bool bCopingTurned = false;

	/** The coping stones round the rim */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CopingStones;

	/** The deck: a skin of its own ground laid over the tops of the walls */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DeckSlabs;

	/** The sections of fencing that roof the cage. Each is a component of its own: their materials are not made for instancing. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LidPanels;

public:

	/** Constructor */
	AGutCage();

	/** Rebuilds the room whenever a property changes */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	/** Sizes and places every box from the cage properties */
	void BuildCage();

	/** Creates one of the pool walls */
	UStaticMeshComponent* CreateWall(FName Name, UStaticMesh* Mesh);
};
