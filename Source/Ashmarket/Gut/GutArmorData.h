// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GutTypes.h"
#include "GutArmorData.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 *  One grey-box plate of a piece of armor. It is fixed to one bone of the fighter's body and runs toward another further down
 *  the same limb, so it is described by the two bones and not by measurements that would only suit one skeleton.
 */
USTRUCT(BlueprintType)
struct FGutArmorPlate
{
	GENERATED_BODY()

	/** Bone of the body mesh the plate is fixed to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plate")
	FName Bone = NAME_None;

	/** Bone the plate runs toward: the next joint down the limb */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plate")
	FName TowardBone = NAME_None;

	/** How much of the way between the two bones the plate covers, centered between them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plate", meta = (ClampMin = 0.05, ClampMax = 2))
	float LengthShare = 0.8f;

	/** Width of the plate across the limb */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plate", meta = (ClampMin = 1, ClampMax = 200, Units = "cm"))
	float Thickness = 12.0f;
};

/**
 *  One piece of armor: the part of the body it covers and what it does to a blow that lands there.
 *  A fighter wears at most one piece on each part. Armor does nothing against the cage's hazards.
 */
UCLASS(BlueprintType)
class UGutArmorData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown where armor is bought */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor")
	FText DisplayName;

	/** The part of the body this covers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor")
	EGutArmorSlot Slot = EGutArmorSlot::Torso;

	/** Multiplier on the damage of a blunt blow that lands on the covered part, to health and to the limb. Below one protects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor", meta = (ClampMin = 0, ClampMax = 2))
	float BluntScale = 1.0f;

	/** Multiplier on the damage of a bladed blow that lands on the covered part, to health and to the limb. Below one protects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor", meta = (ClampMin = 0, ClampMax = 2))
	float BladedScale = 1.0f;

	/** What it costs to buy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor", meta = (ClampMin = 0, ClampMax = 10000))
	float Price = 15.0f;

	/** Shape every plate of the piece is made from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation")
	TObjectPtr<UStaticMesh> PlateMesh;

	/** Material for the plates */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation")
	TObjectPtr<UMaterialInterface> PlateMaterial;

	/**
	 *  If true the plate mesh is a model made elsewhere: an open-ended tube with its length along X, its width along Y
	 *  and its front toward +Z. It is fitted round the limb in its own shape and its own materials, instead of being stretched like the box.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation")
	bool bPlateIsModel = false;

	/** How much wider than a plate's Thickness the tube is made, so the limb fits inside its wall */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation", meta = (EditCondition = "bPlateIsModel", ClampMin = 0.5, ClampMax = 4))
	float ModelWidthScale = 1.45f;

	/** Depth of the tube, front to back, against its width: a chest is wider than it is deep */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation", meta = (EditCondition = "bPlateIsModel", ClampMin = 0.2, ClampMax = 2))
	float ModelDepthShare = 1.0f;

	/** The plates, each fixed to a bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Armor|Presentation")
	TArray<FGutArmorPlate> Plates;

	/** Returns the multiplier on the damage of a blow of the given kind that lands on the covered part */
	float GetDamageScale(EGutDamageType Type) const { return Type == EGutDamageType::Bladed ? BladedScale : BluntScale; }
};
