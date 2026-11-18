// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GutTypes.h"
#include "GutWeaponData.generated.h"

class UStaticMesh;
class UMaterialInterface;

/**
 *  One thing a fighter can fight with, bare hands included.
 *  Holds everything that changes when the weapon changes: its attacks, its finishers, how well it guards and what it looks like.
 *  Every fighter using the weapon shares these numbers, so tuning a weapon here tunes it for both sides.
 */
UCLASS(BlueprintType)
class UGutWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown on the HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;

	/** One line on how the weapon fights, shown at the rack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText Description;

	/**
	 *  If true the weapon is carried on the off hand arm, and it is that arm that must be crippled to knock it loose.
	 *  For shields.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	bool bOffHand = false;

	/**
	 *  The light attack thrown while the guard is held: its place in LightAttacks. Less than nought and the guard makes
	 *  no difference to what is thrown. For a shield: with the guard up the blow is the shield itself, and the fist stays home.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	int32 GuardedLightAttack = INDEX_NONE;

	/** How a fighter holding this stands, guards, takes a blow and dodges */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Stance")
	FGutStanceAnimations Stance;

	/** Light attacks, played in order as a chain while the fighter keeps attacking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Attacks")
	TArray<FGutAttackDef> LightAttacks;

	/** Heavy attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Attacks")
	FGutAttackDef HeavyAttack;

	/**
	 *  Which of the light attacks goes in low, if one does, from zero; -1 if none. Against something that stands no higher than
	 *  a knee, a dog, every light attack is seen as this one. Only what is seen changes: the blow is the one that was thrown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Attacks", meta = (ClampMin = -1))
	int32 LowLightAttack = INDEX_NONE;

	/**
	 *  Multiplier on the stamina it costs to block while holding this. Below one guards well, above one guards badly.
	 *  Keep it low enough that blocking the lightest attack on the rack still costs less than throwing it,
	 *  or hammering at this weapon's guard beats the guard.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Guard", meta = (ClampMin = 0.05, ClampMax = 5))
	float BlockCostScale = 1.0f;

	/**
	 *  Multiplier on the limb damage taken by the arm that holds this. A shield takes most of what is aimed at the arm behind it.
	 *  Without this, an attack that lands on that arm from in front would knock a shield loose in a couple of blows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Guard", meta = (ClampMin = 0, ClampMax = 5))
	float HeldArmDamageScale = 1.0f;

	/** The quick way to finish a staggered fighter, on the light attack input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Finishers")
	FGutFinisherDef CleanFinisher;

	/** The drawn out way to finish a staggered fighter, on the heavy attack input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Finishers")
	FGutFinisherDef SlowFinisher;

	/**
	 *  What this sounds like, as part of the name of its sounds: a swing of it is swing_<this>, a hit with it hit_<this>,
	 *  a blow stopped by it block_<this>, and it falls on the floor as drop_<this>. Bare hands are "fists".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	FName SoundKind = FName("fists");

	/** Grey-box shape held in the weapon hand. Leave empty for bare hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<UStaticMesh> Mesh;

	/** Material for the shape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	TObjectPtr<UMaterialInterface> Material;

	/**
	 *  If true the shape is strapped along the forearm of the arm that carries it, its face turned outward and its top toward the elbow,
	 *  instead of being held in the fist. For a shield.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	bool bStrapToForearm = false;

	/** Where the shape sits in the hand, and how it is scaled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Presentation")
	FTransform GripTransform;

	/**
	 *  For a chain: one link of it. While the weapon is in a hand, this many of them hang from the Handle Mesh and swing with the arm,
	 *  in place of the one stiff shape, which is still what hangs on the rack and lies on the floor. Leave empty for anything else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain")
	TObjectPtr<UStaticMesh> LinkMesh;

	/** The part of the chain that is held, in the same place and at the same size as the whole shape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain")
	TObjectPtr<UStaticMesh> HandleMesh;

	/** What hangs on the last link */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain")
	TObjectPtr<UStaticMesh> EndMesh;

	/** How far up the shape the first link hangs from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain", meta = (Units = "cm"))
	float LinkStart = 16.0f;

	/** How many links hang there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain", meta = (ClampMin = 1, ClampMax = 64))
	int32 LinkCount = 11;

	/** Distance from the middle of one link to the middle of the next */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain", meta = (ClampMin = 1, Units = "cm"))
	float LinkPitch = 11.5f;

	/** Size of a link against its model */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Chain", meta = (ClampMin = 0.01))
	float LinkScale = 1.0f;

	const FGutFinisherDef& GetFinisher(EGutFinisherStyle Style) const { return Style == EGutFinisherStyle::Slow ? SlowFinisher : CleanFinisher; }

	/** Returns the average reach of the light attacks */
	float GetLightReach() const;

	/** Returns the average damage of the light attacks */
	float GetLightDamage() const;

	/** Returns the average time one light attack takes from input to being free again */
	float GetLightTime() const;
};
