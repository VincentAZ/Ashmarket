// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "GutGore.generated.h"

class UDecalComponent;
class UMaterialBillboardComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;

/** How blood came to be where it is, which decides what it looks like there */
enum class EGutBloodMark : uint8
{
	/** Thrown by a blow: a spatter, a burst, a line of cast-off drops */
	Thrown,

	/** Fallen straight down: round drops */
	Dripped,

	/** Dragged through: a smear */
	Smeared,

	/** Run down from where it came out: trickles, with their top uppermost */
	Ran,

	/** Where a blow broke the skin: a wet patch, nearly solid */
	Wound,

	/** Where an edge opened it: a line with what runs from it, its top uppermost */
	Gash,

	/** Where it has been bled over and wiped: most of a hand's breadth and more covered */
	Soaked
};

/**
 *  Keeps the blood on the cage floor: splatters where blows land and pools that spread under the dead.
 *  It only looks after the decals. Fighters decide when to bleed, and with what material.
 *  The floor holds a set number of splatters, and the oldest dry up first.
 */
UCLASS()
class UGutGoreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/**
	 *  Leaves a splatter of the given radius on the floor under the given point.
	 *  Returns nothing if there is no material or no floor there.
	 */
	UDecalComponent* Splatter(UMaterialInterface* Material, const FVector& Location, float Radius);

	/** Leaves a pool on the floor under the given point that spreads from one radius to another over the given time */
	UDecalComponent* Pool(UMaterialInterface* Material, const FVector& Location, float StartRadius, float EndRadius, float SpreadTime);

	/**
	 *  Throws drops of blood into the air from a point, the way given: each flies, falls, and leaves a small mark of its
	 *  own on the floor where it lands. What a drop looks like in the air is the picture kept for it; with none, the drops
	 *  are not seen flying and only their marks appear.
	 */
	void Spray(UMaterialInterface* MarkMaterial, const FVector& From, const FVector& Direction, int32 Count, float Speed);

	/**
	 *  One of the pictures of blood that came there that way, a different one from time to time: whichever of them have been
	 *  made (Tools/import_gut_blood.sh). With none made, the picture given.
	 */
	UMaterialInterface* GetMark(EGutBloodMark Kind, UMaterialInterface* Otherwise);

	/** Returns the number of splatters and pools on the floor */
	int32 GetNumBloodDecals() const;

	/** Returns the number of drops in the air */
	int32 GetNumDropsFlying() const { return Flying.Num(); }

	// ~begin FTickableGameObject interface

	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

	// ~end FTickableGameObject interface

private:

	/** A pool that is still spreading */
	struct FSpreadingPool
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		float StartRadius = 0.0f;
		float EndRadius = 0.0f;
		float SpreadTime = 1.0f;
		float Age = 0.0f;
	};

	/** Every splatter and pool on the floor, oldest first */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Decals;

	TArray<FSpreadingPool> SpreadingPools;

	/** A drop in the air */
	struct FFlyingDrop
	{
		TWeakObjectPtr<UMaterialBillboardComponent> Seen;
		TWeakObjectPtr<UMaterialInterface> Mark;
		FVector Place = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Size = 4.0f;
		float Age = 0.0f;
	};

	TArray<FFlyingDrop> Flying;

	/** What carries the drops that are seen, the ones not in use, and what a drop looks like */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DropHolder;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialBillboardComponent>> SpareDrops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DropMaterials;

	bool bLookedForDropMaterials = false;

	/** The pictures of blood on a surface, by how it came there */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> ThrownMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DrippedMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SmearedMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> RanMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> WoundMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> GashMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SoakedMarks;

	bool bLookedForMarks = false;
};

/**
 *  A piece of a fighter lying on the cage floor after it has been cut off: a grey-box stand-in for the limb.
 *  Like a dropped weapon it is only a prop.
 */
UCLASS()
class AGutGib : public AActor
{
	GENERATED_BODY()

	/** Physics enabled shape */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

	/** The bone this was cut off at */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gut", meta = (AllowPrivateAccess = "true"))
	FName Bone = NAME_None;

public:

	/** Constructor */
	AGutGib();

	/** Takes the given shape, size and color, and is thrown with the given speed */
	void Throw(FName InBone, UStaticMesh* Shape, const FVector& Scale, UMaterialInterface* Material, const FLinearColor& Color, const FVector& Velocity);

	/**
	 *  Shows the limb itself in place of the stand-in shape: a copy of each part of the fighter's own character (body, clothes, head),
	 *  frozen as it was, with everything but the limb drawn down to nothing. The shape goes on doing the falling, unseen.
	 */
	void ShowLimb(const TArray<USkeletalMeshComponent*>& Parts, const FVector& CutLocation);

	UFUNCTION(BlueprintPure, Category="Gut")
	FName GetBone() const { return Bone; }
};
