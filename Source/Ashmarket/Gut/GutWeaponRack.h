// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GutWeaponRack.generated.h"

class UGutWeaponData;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 *  The weapon rack on the cage wall, as grey boxes: a board with the shape of every weapon hung on it.
 *  It is only for show. The game mode decides what is on it and who has drawn what; a drawn weapon leaves the board.
 */
UCLASS()
class AGutWeaponRack : public AActor
{
	GENERATED_BODY()

	/** Root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** The board the weapons hang on. The rack faces along its forward axis, away from the wall. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Board;

protected:

	/** Width and height of the board */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack", meta = (ClampMin = 10, ClampMax = 2000, Units = "cm"))
	FVector2D BoardSize = FVector2D(400.0f, 220.0f);

	/** Thickness of the board */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack", meta = (ClampMin = 1, ClampMax = 100, Units = "cm"))
	float BoardThickness = 6.0f;

	/** How far in front of the board the weapons hang */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack", meta = (ClampMin = 0, ClampMax = 100, Units = "cm"))
	float WeaponStandoff = 10.0f;

	/** Material for the board */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack")
	TObjectPtr<UMaterialInterface> BoardMaterial;

	/**
	 *  A panel made elsewhere, standing upright with its width along X, for the weapons to hang on in place of the board.
	 *  It is repeated side by side, stretched a little so a whole number covers the board. Leave empty for the board.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Rack")
	TObjectPtr<UStaticMesh> BackingMesh;

	/** The panels standing in for the board */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BackingPanels;

	/** The weapons on the rack, in order along the board */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGutWeaponData>> Weapons;

	/** The shape hung on the board for each weapon. Empty for a weapon with no shape. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WeaponShapes;

public:

	/** Constructor */
	AGutWeaponRack();

	/** Resizes the board whenever a property changes */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Hangs the given weapons on the board, replacing whatever was there */
	void SetWeapons(const TArray<TObjectPtr<UGutWeaponData>>& NewWeapons);

	/** Takes the given weapons off the board and puts every other one back */
	void SetDrawn(const TArray<const UGutWeaponData*>& DrawnWeapons);

	/** Returns true if the given weapon is hanging on the board right now */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsOnBoard(const UGutWeaponData* Weapon) const;
};
