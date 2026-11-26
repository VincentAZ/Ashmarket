// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GutTypes.h"
#include "GutFighter.generated.h"

class UDecalComponent;
class AGutFighter;
class AGutDroppedWeapon;
class AGutHazard;
class UGutArmorData;
class UGutBeastData;
class UGutWeaponData;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UNiagaraSystem;
class UAnimSequenceBase;
class UAnimSequenceBase;
class UIKRetargeter;
class UInstancedStaticMeshComponent;
class URetargetComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGutFighterDiedSignature, AGutFighter*, Fighter, AGutFighter*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGutFighterAttackSignature, AGutFighter*, Fighter, AGutFighter*, Other, EGutHitResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGutFighterLimbSignature, AGutFighter*, Fighter, EGutLimb, Limb);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGutFighterSignature, AGutFighter*, Fighter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGutFighterHazardSignature, AGutFighter*, Fighter, AGutHazard*, Hazard);

/** A running count of how a fighter's own attacks have gone in this fight */
struct FGutFighterTally
{
	/** Attacks started */
	int32 Swings = 0;

	/** Attacks that landed clean */
	int32 Hits = 0;

	/** Attacks stopped by a guard */
	int32 Blocked = 0;

	/** Attacks that broke a guard */
	int32 GuardBreaks = 0;

	/** Attacks that passed through a dodge */
	int32 Dodged = 0;
};

/**
 *  A cage fighter. The player and every opponent are the same class with a different brain:
 *  - Health and stamina
 *  - A weapon, or bare hands, each with a light attack chain, a heavy attack and two finishers
 *  - Block, with guard break when stamina runs out. Some weapons guard better than others.
 *  - Dodge, with a window where attacks pass through
 *  - Four limbs that can be crippled: a crippled weapon arm drops the weapon, a crippled leg slows the fighter
 *  - Stun, stagger and death. A beaten fighter staggers instead of dying, and dies only when it is finished.
 *  - The cage's hazards, which hurt like a blow but kill outright when they take the last of a fighter's health
 *  - Armor, a piece to a part of the body, which takes some of a blow that lands there
 *  - Blood, left on the floor wherever a blow lands, and limbs that some finishers cut off
 *
 *  All timing is driven by timers on the fighter, not by animation, so the fight is the same with placeholder art.
 */
UCLASS(abstract)
class AGutFighter : public ACharacter
{
	GENERATED_BODY()

	/** Slab held out front while blocking. Grey-box stand-in for a block pose. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* GuardPlate;

	/** Grey-box shape of the weapon in the fighter's hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;

	/** The collar round the neck of a fighter that is still somebody's property */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Collar;

	/** The rope round the wrists of somebody who is tied */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Tie;

	/** The character the player sees, when the fighter has been given one: a whole actor with body, face and hair, spawned for the fight */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> BodyActor;

	/** The part of that character that follows the skeleton that does the fighting. The rest of the character follows it. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Copies the fighting skeleton's pose onto the body, limb for limb */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	URetargetComponent* BodyRetarget;

protected:

	/** Name shown on the HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut")
	FText DisplayName;

	/** Health at the start of a fight. This is the main lever on how long a fight lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Health", meta = (ClampMin = 1, ClampMax = 1000))
	float MaxHealth = 125.0f;

	/** Current health */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gut|Health")
	float Health = 0.0f;

	/** Stamina at the start of a fight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 1, ClampMax = 1000))
	float MaxStamina = 100.0f;

	/** Current stamina */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gut|Stamina")
	float Stamina = 0.0f;

	/** Stamina regained per second once regeneration has started */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 0, ClampMax = 500))
	float StaminaRegenRate = 25.0f;

	/** Time after spending stamina before it starts to come back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float StaminaRegenDelay = 0.8f;

	/**
	 *  A blow thrown without the stamina for it still comes, late and weak: the button always answers, and somebody who
	 *  hammers it fights tired until they stop. How much longer the windup and the recovery of such a blow take,
	 *  and how much is left of its damage, its stun and its push.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 1, ClampMax = 4))
	float TiredWindupScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 1, ClampMax = 4))
	float TiredRecoveryScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 0, ClampMax = 1))
	float TiredPowerScale = 0.55f;

	/** Stamina regeneration multiplier while the guard is up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stamina", meta = (ClampMin = 0, ClampMax = 1))
	float BlockingRegenScale = 0.25f;

	/** What the fighter fights with when it has no weapon. Needed: a fighter without this cannot attack once disarmed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	TObjectPtr<UGutWeaponData> Fists;

	/** Weapon the fighter starts the fight holding. Leave empty to start bare handed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	TObjectPtr<UGutWeaponData> StartingWeapon;

	/** Armor the fighter starts the fight wearing. On the ladder the fight's own armor is put on over it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	TArray<TObjectPtr<UGutArmorData>> StartingArmor;

	/** Socket on the body mesh the weapon is held at */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	FName WeaponSocketName = FName("HandGrip_R");

	/** Socket on the body mesh an off hand weapon, such as a shield, is held at */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	FName OffHandSocketName = FName("HandGrip_L");

	/** The arm that holds the weapon. Crippling it makes the fighter drop the weapon. An off hand weapon is held by the other arm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Weapon")
	EGutLimb WeaponArm = EGutLimb::RightArm;

	/** Time after a light attack ends in which the next light attack continues the chain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Attack", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboWindow = 0.5f;

	/** An action pressed this long before it becomes possible still happens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Attack", meta = (ClampMin = 0, ClampMax = 2, Units = "s"))
	float InputBufferTime = 0.3f;

	/** Attacks started with the opponent inside this distance turn to face them during the windup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Attack", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float AttackAssistRange = 450.0f;

	/** Turn speed while tracking the opponent during a windup or behind the guard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Attack", meta = (ClampMin = 0, ClampMax = 3600))
	float TrackingTurnRate = 540.0f;

	/** Height difference beyond which an attack cannot connect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Attack", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float MaxAttackHeightDifference = 120.0f;

	/** Half width of the arc the guard covers, measured from the fighter's facing. Attacks from outside it are not blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Block", meta = (ClampMin = 1, ClampMax = 180, Units = "deg"))
	float BlockHalfAngle = 70.0f;

	/** Share of the damage that gets through when the guard breaks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Block", meta = (ClampMin = 0, ClampMax = 1))
	float GuardBreakDamageScale = 0.5f;

	/** Time the fighter is stunned when the guard breaks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Block", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float GuardBreakStunTime = 1.1f;

	/** Distance a blocked attack still pushes the blocker back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Block", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float BlockedKnockback = 15.0f;

	/** Damage each limb can take before it is crippled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Limbs", meta = (ClampMin = 1, ClampMax = 1000))
	float MaxLimbHealth = 30.0f;

	/** Walking speed and dodge distance multiplier for each crippled leg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Limbs", meta = (ClampMin = 0, ClampMax = 1))
	float CrippledLegSpeedScale = 0.7f;

	/**
	 *  Blocking costs this much more stamina for each crippled arm, as a share of the normal cost.
	 *  Keep it small: if blocking a light attack ever costs more than throwing it, hammering at a guard beats the guard.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Limbs", meta = (ClampMin = 0, ClampMax = 5))
	float CrippledArmBlockPenalty = 0.25f;

	/** An attacker standing further round to one side than this hits the limb on that side, whatever the attack's hit zone says */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Limbs", meta = (ClampMin = 0, ClampMax = 90, Units = "deg"))
	float SideHitAngle = 35.0f;

	/** Time a beaten fighter stays staggered the first time, waiting to be finished */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.1, ClampMax = 30, Units = "s"))
	float StaggerDuration = 4.0f;

	/** Each later stagger lasts this share of the one before, so second winds get harder to deny */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.1, ClampMax = 1))
	float StaggerDurationFalloff = 0.75f;

	/** No stagger is shorter than this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.1, ClampMax = 30, Units = "s"))
	float MinStaggerDuration = 2.0f;

	/** Health a staggered fighter comes back with if nobody finishes it in time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 1, ClampMax = 1000))
	float SecondWindHealth = 15.0f;

	/** Stamina a staggered fighter comes back with, at least */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0, ClampMax = 1000))
	float SecondWindStamina = 50.0f;

	/**
	 *  How far a staggered fighter reels from a blow thrown to drive it (TryDrive), and how long that takes. It is not hurt:
	 *  it has nothing left to lose. But if the way it reels is onto the spikes or the cable, the cage ends it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DriveDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.05, ClampMax = 2, Units = "s"))
	float DriveDuration = 0.6f;

	/** How close a fighter must be to a staggered opponent to finish it, measured like an attack's reach */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float FinisherRange = 170.0f;

	/** How fast a fighter walks in to a finisher that does not reach from where it was started, as a share of its walking speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.1, ClampMax = 1))
	float FinisherClosingSpeedScale = 0.6f;

	/** The longest it walks in before doing it from wherever it has got to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Stagger", meta = (ClampMin = 0.1, ClampMax = 5, Units = "s"))
	float FinisherClosingLimit = 1.5f;

	/** Stamina paid to dodge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0, ClampMax = 500))
	float DodgeStaminaCost = 22.0f;

	/** Time a dodge takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float DodgeDuration = 0.45f;

	/** Distance a dodge covers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float DodgeDistance = 260.0f;

	/** Time into the dodge when attacks start passing through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float DodgeInvulnerableStart = 0.04f;

	/** Time into the dodge when attacks stop passing through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float DodgeInvulnerableEnd = 0.32f;

	/** Time after a dodge ends before another can start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Dodge", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float DodgeCooldown = 0.15f;

	/** Walking speed when free to move */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm/s"))
	float WalkSpeed = 450.0f;

	/**
	 *  A body has weight. How hard a fighter can push off (cm/s2), how quickly it can change the way it is already going
	 *  (the higher, the sharper it turns and reverses), and how hard it can brake (cm/s2).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 100, ClampMax = 10000))
	float MoveAcceleration = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 0.5, ClampMax = 20))
	float MoveGrip = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 100, ClampMax = 10000))
	float MoveBraking = 1500.0f;

	/** Walking speed multiplier while the guard is up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 0, ClampMax = 1))
	float BlockingSpeedScale = 0.4f;

	/** Walking speed multiplier during the recovery of an attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Movement", meta = (ClampMin = 0, ClampMax = 1))
	float RecoverySpeedScale = 0.15f;

	/**
	 *  A fighter the cage kills within this long of being struck by its opponent counts as killed by the opponent,
	 *  which drove it there. Later than that, the cage killed it alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Death", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float HazardKillCreditTime = 2.0f;

	/** Speed given to the body when it is killed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Death", meta = (ClampMin = 0, ClampMax = 5000, Units = "cm/s"))
	float DeathImpulse = 450.0f;

	/** Moving in the stance: forward, back and to each side, at a walk and at a run. Shared by every weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FGutLocomotionAnimations Locomotion;

	/** How the fighter stands and reacts when what it holds does not say. A weapon's own stance comes first, clip by clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FGutStanceAnimations Stance;

	/**
	 *  How the fighter stands when it is not in a fight at all, as in the street: at ease, hands down.
	 *  Its arms are left to the walking clip, not held where the stance has them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease")
	FGutStanceAnimations AtEaseStance;

	/** And how it gets about at ease: a walk and a jog. Out of a fight a body turns to face where it is going, so only the forward clips are ever seen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease")
	FGutLocomotionAnimations AtEaseLocomotion;

	/** The same for a woman, where there are clips a woman performed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease")
	FGutLocomotionAnimations AtEaseLocomotionWoman;

	/** Speed at a walk when at ease */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease", meta = (ClampMin = 10, ClampMax = 1000, Units = "cm/s"))
	float AtEaseWalkSpeed = 112.0f;

	/** Speed at a jog when at ease */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease", meta = (ClampMin = 10, ClampMax = 1000, Units = "cm/s"))
	float AtEaseJogSpeed = 340.0f;

	/** Speed at a flat-out run when at ease */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease", meta = (ClampMin = 10, ClampMax = 1200, Units = "cm/s"))
	float AtEaseRunSpeed = 500.0f;

	/** The ways there are of standing about at ease. Each body keeps one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease")
	TArray<FGutStandFamily> AtEaseFamilies;

	/** Where somebody's arms are when their wrists are tied behind their back: a clip the arms are taken from, whatever the rest of them is doing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease")
	TSoftObjectPtr<UAnimSequenceBase> BoundArms;

	/** How far the tied wrists of the body that is seen lie across one another: the right one this far to the left of the left one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation|At Ease", meta = (ClampMin = -10, ClampMax = 20, Units = "cm"))
	float BoundCross = 3.0f;

	/** Animation slot the placeholder animations play on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FName AnimSlotName = FName("DefaultSlot");

	/** Placeholder animation for a dodge. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UAnimSequenceBase> DodgeAnimation;

	/** Placeholder animation for a stagger. Only its opening is played, stretched over the stagger. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UAnimSequenceBase> StaggerAnimation;

	/** How much of the stagger animation, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float StaggerAnimationLength = 0.45f;

	/** Placeholder animation for a stun. Only its opening is played, stretched over the stun. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UAnimSequenceBase> StunAnimation;

	/** How much of the dodge animation, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float DodgeAnimationLength = 0.6f;

	/** How much of the stun animation, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float StunAnimationLength = 0.3f;

	/** Effect spawned on a fighter that takes a hit. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UNiagaraSystem> HitEffect;

	/** Decal left on the floor when the fighter bleeds. Leave empty for a fighter that does not bleed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore")
	TObjectPtr<UMaterialInterface> BloodMaterial;

	/** Material for the blood thrown by a blow, where it differs from the blood that pools. Leave empty to use the one above for both. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore")
	TObjectPtr<UMaterialInterface> BloodSplashMaterial;

	/**
	 *  If true a blow that lands leaves its blood on the body it landed on: the same photographed blood that is thrown on the
	 *  floor, laid on the skin where the blow came in, and carried by that limb for the rest of the fight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore")
	bool bShowWounds = true;

	/** The most marks one body carries: the oldest goes when another is made */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0))
	int32 MaxWounds = 60;

	/** Radius of the splatter left by a blow, for each point of its damage, on top of the smallest splatter there is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0, ClampMax = 20, Units = "cm"))
	float BloodRadiusPerDamage = 1.8f;

	/** Drops thrown into the air by a blow that draws blood, for each point of its damage: each flies and leaves its own mark where it falls */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0, ClampMax = 5))
	float BloodDropsPerDamage = 0.9f;

	/** Somebody who has been opened up goes on dripping: seconds between drops with one mark on the body, less with every mark more */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0, ClampMax = 60, Units = "s"))
	float BleedInterval = 5.0f;

	/**
	 *  A blow is seen to land. Where a clip's own motion would leave the fist or the weapon short of the one it hits, the
	 *  body that is seen is carried forward by the difference as the blow is thrown, this far at most, and brought back
	 *  after it. Only what is seen moves: the fighter stays where the rules have it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look", meta = (ClampMin = 0, ClampMax = 100, Units = "cm"))
	float StrikeReachOut = 32.0f;

	/** And the fighter itself steps this much further into a blow than the blow's own step, where that is what it takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float StrikeStepIn = 45.0f;

	/** And this far back at most, where the blow would otherwise go through the one it hits and out the other side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look", meta = (ClampMin = 0, ClampMax = 100, Units = "cm"))
	float StrikePullBack = 14.0f;

	/** How far into the body it lands on the end of a blow is aimed: on the skin is a miss to the eye */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look", meta = (ClampMin = 0, ClampMax = 30, Units = "cm"))
	float StrikeDepth = 5.0f;

	/** Radius the pool under a dead fighter spreads to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float DeathPoolRadius = 110.0f;

	/** Time the pool under a dead fighter takes to spread */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore", meta = (ClampMin = 0.1, ClampMax = 60, Units = "s"))
	float DeathPoolSpreadTime = 9.0f;

	/** Material for the grey-box stand-in for a severed limb. It is colored to match the body. Leave empty for limbs that vanish. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Gore")
	TObjectPtr<UMaterialInterface> GibMaterial;

	/** A character made for this fighter, such as a MetaHuman. When set, it is shown in place of the placeholder, which goes on fighting unseen underneath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut|Look")
	TSoftClassPtr<AActor> BodyClass;

	/** Name of the skeletal mesh in that character that the rest of it follows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName BodyComponentName = FName("Body");

	/** Name of the skeletal mesh in that character that is its head and neck */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName FaceComponentName = FName("Face");

	/** A woman's body beside a man's. What her blows carry: the damage, the stop, the push, what they take from a guard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Body", meta = (ClampMin = 0.3, ClampMax = 1.5))
	float WomanPowerScale = 0.85f;

	/** How long her blows take to come and to get back from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Body", meta = (ClampMin = 0.3, ClampMax = 1.5))
	float WomanSwingTimeScale = 0.94f;

	/** How fast she covers the floor, and how far a dodge carries her */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Body", meta = (ClampMin = 0.3, ClampMax = 1.5))
	float WomanPaceScale = 1.08f;

	/** What a dodge costs her */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Body", meta = (ClampMin = 0.3, ClampMax = 1.5))
	float WomanDodgeCostScale = 0.8f;

	/** How the placeholder's pose is carried over to the body's own skeleton */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut|Look")
	TObjectPtr<UIKRetargeter> BodyRetargeter;

	/** A captured performance of a face, looped on the fighter's own face while it lives. Only its curves are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	TObjectPtr<UAnimSequenceBase> FaceLoop;

	/** The same for a blow that has just landed on it, and for a fighter that is beaten and knows it. Each is optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	TObjectPtr<UAnimSequenceBase> FaceHurt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	TObjectPtr<UAnimSequenceBase> FaceAfraid;

	/** How far the eyes close when the fighter is dead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look", meta = (ClampMin = 0, ClampMax = 1))
	float DeadEyesClosed = 0.8f;

	/** Pose in the Body Retargeter that closes the right hand, used while that hand holds a weapon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName GripPoseRight = FName("GripRight");

	/** Pose in the Body Retargeter that closes the left hand, used while that arm carries something */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName GripPoseLeft = FName("GripLeft");

	/** Pose in the Body Retargeter that closes both hands: bare fists, or a fist beside an arm that carries something */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName GripPoseBoth = FName("GripBoth");

	/** Pose in the Body Retargeter that half closes both hands: somebody whose wrists are tied, with nothing to do with them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Look")
	FName GripPoseBound = FName("GripLoose");

	/**
	 *  What ties the wrists of somebody who is bound: a band of rope, lying in its own space with its opening along Z, its
	 *  longer way along X and its knot toward +Y. Leave empty and the wrists are held together by nothing that is seen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UStaticMesh> TieMesh;

	/** How long that band's opening is at its own size, how thick a wrist is taken to be, and how far up the arms from the wrists it lies when they are side by side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 1, ClampMax = 50, Units = "cm"))
	float TieSpan = 10.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 1, ClampMax = 20, Units = "cm"))
	float TieWrist = 3.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = -10, ClampMax = 20, Units = "cm"))
	float TieUpTheArm = 2.0f;

	/** Shape of the collar. Leave empty for a fighter with no collar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UStaticMesh> CollarMesh;

	/** Material for the collar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UMaterialInterface> CollarMaterial;

	/** Bone of the body mesh the collar sits on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FName CollarBone = FName("neck_01");

	/** How the collar sits on that bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FTransform CollarTransform = FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(4.0f, 0.0f, 0.0f));

	/** A body of its own has the collar fitted round its own neck: this far out from the bone is still neck, and beyond it shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 4, ClampMax = 30, Units = "cm"))
	float CollarReach = 12.0f;

	/** The gap left between that neck and the inside of the ring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0, ClampMax = 5, Units = "cm"))
	float CollarSlack = 0.2f;

	/** How much of the skin under the ring it clears: at 1 it touches nothing, and below that it presses into the widest of it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0.5, ClampMax = 1))
	float CollarTightness = 0.8f;

	/** On such a body the ring rides on this bone, in the middle of the neck, square across it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	FName CollarNeckBone = FName("neck_02");

	/** The lowest it may lie along the neck, measured from that bone (below it is less than nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = -10, ClampMax = 10, Units = "cm"))
	float CollarLowest = -3.0f;

	/** And the highest: above this the jaw is in its way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = -10, ClampMax = 12, Units = "cm"))
	float CollarHighest = 2.0f;

	/** It lies as low as the neck is no more than this much wider (each side) than at its narrowest: below that are the shoulders */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (ClampMin = 0, ClampMax = 5, Units = "cm"))
	float CollarShoulder = 0.8f;

	/** If true, the body materials are tinted with Body Tint so fighters are easy to tell apart */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	bool bApplyBodyTint = false;

	/** Color applied to the body materials */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (EditCondition = "bApplyBodyTint"))
	FLinearColor BodyTint = FLinearColor(0.6f, 0.08f, 0.05f);

	/** Name of the color parameter on the body materials */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation", meta = (EditCondition = "bApplyBodyTint"))
	FName BodyTintParameter = FName("Paint Tint");

public:

	/** Called once when this fighter dies */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterDiedSignature OnDied;

	/** Called when one of this fighter's attacks reaches another fighter. Other is the target. */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterAttackSignature OnAttackLanded;

	/** Called when an attack reaches this fighter. Other is the attacker. */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterAttackSignature OnAttackReceived;

	/** Called when one of this fighter's limbs is crippled */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterLimbSignature OnLimbCrippled;

	/** Called when this fighter is beaten and staggers */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterSignature OnStaggered;

	/** Called when one of the cage's hazards hurts this fighter */
	UPROPERTY(BlueprintAssignable, Category="Gut")
	FGutFighterHazardSignature OnHazardHit;

protected:

	/** Current state */
	EGutFighterState State = EGutFighterState::Idle;

	/** Time spent in the current state */
	float StateTime = 0.0f;

	/** Frame this fighter last ticked on. Lets it tell whether another fighter is acting on it before or after its own turn this frame. */
	uint64 LastTickFrame = 0;

	/** If false the fighter can move but cannot attack, block, dodge or be hurt */
	bool bCombatEnabled = true;

	/** If true the fighter wants its guard up as soon as it is free */
	bool bWantsBlock = false;

	/** If true the fighter keeps facing its opponent while it walks, instead of facing where it is going */
	bool bSquareUp = false;

	/** If true the fighter is not in a fight at all: see SetAtEase */
	bool bAtEase = false;

	bool bJogging = false;

	/**
	 *  The clip its arms are held by while its wrists are tied, or none; and how far past the middle of the two each wrist
	 *  of the fighting skeleton is taken, which is whatever puts the wrists of the body that is seen across one another:
	 *  no two bodies are as broad, so it is found by looking, a little each frame
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> BoundClip;

	bool bFingersShutForIt = false;

	/** The line the face is saying, and when by the world's clock it began */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> SpeechFace;

	double SpeechBegan = 0.0;

	float BoundPast = 2.0f;

	/** How fast it is being moved by something other than its own movement, for its legs to keep up with: see SetLedVelocity */
	FVector LedVelocity = FVector::ZeroVector;

	/** Counts the attacks this fighter has started, so others can tell one swing from the next */
	int32 AttackSerial = 0;

	/** Time the current attack started */
	float AttackStartTime = 0.0f;

	/** The fighter on the other side of the cage */
	TWeakObjectPtr<AGutFighter> Opponent;

	/** Weapon in the fighter's hand. Empty when bare handed. */
	UPROPERTY(Transient)
	TObjectPtr<UGutWeaponData> Weapon;

	/** Damage each limb can still take, indexed by EGutLimb. Zero means crippled. */
	float LimbHealth[GutNumLimbs] = { 0.0f, 0.0f, 0.0f, 0.0f };

	/** The armor the fighter is wearing, at most one piece to a part of the body */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGutArmorData>> Armor;

	/** The grey-box plates of the armor, fixed to the body */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmorPlates;

	bool bChestPiece = false;

	/** The links of a chain in the hand. They hang and swing on their own, tied to the fighter only at the handle. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ChainLinks;

	/** What hangs on the chain's last link */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ChainEnd;

	/** Where each joint of the chain is, from the handle outward, and where it was a moment before */
	float StaminaRegenScale = 1.0f;
	float StaminaRegenDelayScale = 1.0f;

	float DamageScale = 1.0f;

	float StunScale = 1.0f;

	float MoveSpeedScale = 1.0f;

	bool bWeaponSabotaged = false;

	/** Whose voice the fighter grunts and cries out in: "man" or "woman". Set from its body. */
	FName Voice = FName("man");

	/** If true the body is a woman's: weaker than a man's and quicker. Set from its body. */
	bool bWoman = false;

	/** Where each foot was last frame, the fastest it has gone since it last came to rest, and whether it is on its way somewhere */
	FVector FootBefore[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float FootFastest[2] = { 0.0f, 0.0f };
	bool bFootGoing[2] = { false, false };

	/** The frame the feet were last listened to in: a body can be posed more than once in a frame */
	uint64 FeetHeardFrame = 0;

	/** When each foot was last heard to land, by the world's clock */
	double FootHeardAt[2] = { -10.0, -10.0 };

	/** Whether the swing of the attack in hand has been heard yet */
	bool bSwingHeard = false;

	FTimerHandle BodyFallTimer;
	FTimerHandle WeaponFallTimer;

	/** How much of each of those the face is showing now, from 0 to 1 */
	float FaceHurtShown = 0.0f;

	/** How much of the fighting face is on: all of it in a fight, a trace of it at ease */
	float FaceFightShown = 1.0f;

	/** When, by the world's clock, this face next blinks, at ease: the fighting face has its own blinks in it */
	double BlinkAt = 0.0;

	/**
	 *  How far each arm is brought in toward the body from where the clip hangs it, in degrees, left then right.
	 *  The clips hang the arms as their one skeleton hangs them, a hand's length out from the hips of anybody slighter:
	 *  each body's arms are brought in until its own wrists are beside its own hips. See HangArms.
	 */
	float ArmsIn[2] = { 0.0f, 0.0f };

	/**
	 *  A clip in which the performer travels, and the body is taken along: where the body stood as it began, which way the
	 *  clip's own space lay in the world, how far through it is, and how far from where it should have begun the body was
	 *  (made up over the first third of a second, not at a jump).
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CarriedClip;

	FVector CarriedFrom = FVector::ZeroVector;
	FVector CarriedSlip = FVector::ZeroVector;
	FVector CarriedRootAtStart = FVector::ZeroVector;
	FQuat CarriedTurn = FQuat::Identity;
	float CarriedTime = 0.0f;

	/** True while what plays on the body is such a clip, to its last frame of fading out: its own travel is left out of the pose */
	bool bCarriedPose = false;

	/** Plays the clip at its own speed from its start and takes the body along it, from the given place */
	void BeginCarried(UAnimSequenceBase* Clip, const FVector& From);
	void UpdateCarried(float DeltaTime);
	void EndCarried();

	/** Brings the arms of somebody standing at ease in to their own sides */
	void HangArms();

	/** A level slice of the dressed body's trunk, in the space of the spine bone it starts at: where it is and how far it reaches */
	struct FTrunkSection
	{
		FName Bone;
		float Height = 0.0f;

		/** How far below the bone the slice begins: the hips are measured down from the pelvis, where there is no bone to begin at */
		float Below = 0.0f;
		FVector Along = FVector::ZeroVector;
		FVector Across = FVector::ZeroVector;
		FVector Front = FVector::ZeroVector;
		FVector2D Middle = FVector2D::ZeroVector;
		FVector2D Half = FVector2D::ZeroVector;

		/** How far an arm may lie into this slice: all of ArmGive where it is skin or a shirt, none where what is worn stands a finger's breadth and more off the body */
		float Give = 0.0f;
	};

	/** Measuring, only the body itself is read and not what it wears: for telling how thick what it wears is */
	mutable bool bMeasureBodyOnly = false;

	/** The trunk of the body that is seen, with what it wears, measured once as it stands at rest */
	TArray<FTrunkSection> Trunk;
	bool bTrunkMeasured = false;

	/** How thick each upper arm is with its sleeve, as a radius */
	float ArmRadius[2] = { 4.5f, 4.5f };

	/** Half the width and the depth of a piece of armor worn round the chest, if one is */
	FVector2D ChestPieceHalf = FVector2D::ZeroVector;

	/** How thick each forearm and each thigh is with what is worn on it, as a radius */
	float ForearmRadius[2] = { 3.4f, 3.4f };
	float ThighRadius[2] = { 8.5f, 8.5f };

	/** What is held in each hand, as places along it in the hand bone's space with how thick it is at each: see FGutClearShape */
	TArray<FVector4> HeldPlaces[2];

	/** Reads what is in the hands off the weapon's own shape, where it has been put on the body */
	void MeasureHeld();

	/**
	 *  Turns the eyes to whoever the head is turned toward, with the small shifts and the glances away that eyes have, and
	 *  writes it over whatever the face's expression or the line it is saying had the eyes doing. -GutNoEyes leaves them.
	 */
	void AimEyes(TMap<FName, float>& Shown, float DeltaTime);

	TWeakObjectPtr<USkeletalMeshComponent> FaceSeen;

	/** Where the eyes are turned, in degrees to its own right and up; the shift on top of where they are aimed; when they shift next; how much they are aimed at all */
	FVector2D EyesShown = FVector2D::ZeroVector;
	FVector2D EyesOff = FVector2D::ZeroVector;
	double EyesShiftAt = 0.0;
	float EyesAimed = 0.0f;

	/** The body as it is dressed, set down where it stands now, to be asked how far in a place is: false if there is nothing to ask */
	bool GetClearBodyNow(struct FGutClearShape& OutShape, struct FGutClearBody& OutWhere);

	/** -GutClearLog: how deep a hand, a forearm or what is held has been in this body's own trunk or thighs since it was last said */
	void LogOwnCuts();

	float DeepestHand = 0.0f;
	float DeepestHeld = 0.0f;
	float DeepestSaid = 0.0f;

	/** How much hanging arms are being brought in to the body's side just now: see FGutClearShape::HangIn */
	float HangInShown = 0.0f;

	/** How many frames somebody at ease has had a body, and how many times its clothes have been set to hang as they stand */
	int32 ClothFrames = 0;
	int32 ClothSettles = 0;
	float HeldSince = 0.0f;

	/** Measures the trunk and the upper arms of the body that is seen */
	void MeasureTrunk();

	/** Somebody too deep through the trunk for hands clasped in front of it to meet stands plainly instead */
	void KeepFromClasping();

	float FaceAfraidShown = 0.0f;

	/** Runs out when a body that is dying on its feet goes limp */
	FTimerHandle LimpTimer;

	/** Set if this fighter is an animal: what kind, the body it is seen as, and what that body is doing */
	UPROPERTY(Transient)
	TObjectPtr<UGutBeastData> Beast;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BeastBody;

	uint8 BeastPose = 0;
	float BeastPoseTime = 0.0f;
	float BeastStateTimeBefore = 0.0f;
	float NextBeastSoundTime = 0.0f;
	float BeastClipTimeBefore = 0.0f;

	/** Chooses the clip the animal's body is in from what the fighter is doing, and keeps it in step with the fight's own timing */
	void UpdateBeast(float DeltaTime);

	/** Asks, until it is so, whether the one left standing has nothing else to do and can show the crowd it won */
	FTimerHandle VictoryTimer;

	int32 VictoryTries = 0;

	/** True while this fighter is showing the crowd it won */
	bool bCelebrating = false;

	void TryCelebrate();

	/** The blood this body is carrying, oldest first */
	UPROPERTY()
	TArray<TObjectPtr<UDecalComponent>> Wounds;

	/** Lays the blood of a blow on the part of the body it landed on, on the side it came in from */
	void AddWound(const FGutAttackDef& Attack, const FVector& FromAttacker, bool bOnLimb, EGutLimb Limb, float Damage);

	/** What this body has been soaked with as its health went: these stay however many marks come after */
	UPROPERTY()
	TArray<TObjectPtr<UDecalComponent>> Soaks;

	int32 TimesBloodDrawn = 0;

	/** How many of the stages of being soaked this body has reached */
	int32 SoakStage = 0;

	/** How many marks have been laid on this body, which is the order they lie in */
	int32 MarksMade = 0;

	/**
	 *  Lays one picture of blood on the skin round a bone, the right way up: Where is at the skin, Into is the way it is
	 *  laid on, Deep how far either side of the skin it reaches, Tall and Wide half its size, Slant how far it leans.
	 *  A mark that is kept is not one of those that go when there are too many.
	 */
	UDecalComponent* MarkSkin(UMaterialInterface* Picture, FName Bone, const FVector& Where, const FVector& Into, float Deep, float Tall, float Wide, float Slant, bool bKept = false);

	/** Covers more of the body the less health is left in it */
	void Soak();

	/** Puts some of somebody else's blood on this fighter, which has just drawn it */
	void Bespatter(const FVector& FromVictim, float Amount);

	/** True once this fighter, beaten and let live, has gone down onto its knees: it stays there and gets no second wind */
	bool bYielded = false;

	/** True once this fighter has been told to stand over the one it will not finish */
	bool bSparing = false;

	FTimerHandle YieldTimer;
	FTimerHandle SpareTimer;
	FTimerHandle HeldClipTimer;

	void TrySpare();

	/** Plays a clip once and goes on, as it ends, into a second that is held for as long as nothing else is asked of the body */
	void PlayThenHold(UAnimSequenceBase* Once, UAnimSequenceBase* Held);

	/** True while this fighter is acting out what a finisher is doing to it */
	bool bBeingTaken = false;

	/** The dead body stops being animated and is left to the floor, thrown the way given if any */
	void GoLimp(const FVector& ImpulseDirection);

	/** The clip of the blow being thrown, the way it is thrown, how far the seen body is to be carried for it and is carried now */
	UPROPERTY(Transient)
	TObjectPtr<const UAnimSequenceBase> StrikeClip;

	FVector StrikeWay = FVector::ForwardVector;
	float StrikeGoal = 0.0f;
	float StrikeShift = 0.0f;

	/**
	 *  How far the limb that strikes is being taken off the clip's own path to meet the body it is thrown at, in the world,
	 *  and which limb: left arm, right arm, left leg, right leg. See SteerBlow.
	 */
	FVector BlowReach = FVector::ZeroVector;
	int32 BlowLimb = INDEX_NONE;

	/** When, by the world's clock, the stop that a landed blow puts on this fighter is over. Zero for none. */
	float BlowStopEnds = 0.0f;

	/**
	 *  In the last moments before a blow is due, takes the fist, the weapon or the foot the rest of the way onto the body
	 *  it is thrown at. The clips were made by one body alone and the bodies are of every build: a blow the rules count
	 *  can pass a hand's width off the skin, and that is what "does not make contact" is. Seen only: the rules never ask.
	 */
	void SteerBlow();

	/** Stops this fighter dead for a few hundredths of a second, as a blow that lands stops both the arm and the body it lands on */
	void StopForBlow(float Seconds);

	/** Somebody this body has been told to look at */
	TWeakObjectPtr<const AGutFighter> LookTarget;

	/** True if this body was given its own way of standing at ease where it was placed */
	bool bIdlePlaced = false;

	/** Seconds until the next small thing it does while standing about (below zero: not yet drawn), which it did last, and how long the one it is doing has left */
	float FidgetIn = -1.0f;
	int32 FidgetLast = INDEX_NONE;
	float FidgetLeft = 0.0f;

	/** Does the small things somebody standing about does */
	void UpdateFidget(float DeltaTime);

	/** The fighter's own clips with the walking and running of what it holds put in, and what that was made for */
	mutable FGutLocomotionAnimations HeldLocomotion;
	mutable TWeakObjectPtr<const UGutWeaponData> HeldLocomotionFor;

	/** Which of the families it stands in at ease (none: one clip, as before there were families), which clip of it, and how long that has left */
	int32 Family = INDEX_NONE;
	int32 FamilyLoop = INDEX_NONE;
	float FamilyLeft = 0.0f;

	/** A family asked for by name before the body was at ease */
	FName FamilyAsked;

	/** Moves on through the family's ways of standing */
	void UpdateStanding(float DeltaTime);

	bool bRunning = false;

	/** True once it has been given a walk of its own, which is then walked whoever it is */
	bool bOwnWalk = false;

	/** How much longer it is busy with something it was handed, and whether its wrists are to be tied again after */
	float TakingLeft = 0.0f;
	bool bBoundAfterTaking = false;

	/**
	 *  How far the seen body is lowered (below zero) or raised to stand on the floor, in centimetres. The clips were made
	 *  for one skeleton and are worn by bodies of every build, and a body whose legs are a little shorter than the clip
	 *  supposes hangs that much above the ground: see GroundBody.
	 */
	float GroundShift = 0.0f;

	/** What Ground Shift is being brought to */
	float GroundWanted = 0.0f;

	/** How far above the floor the sole of each foot of the seen body is when it stands in its own reference pose: ankle, then ball */
	float SoleRest[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

	/** The seen mesh those were measured on */
	TWeakObjectPtr<const USkeletalMesh> SoleMesh;

	uint64 GroundedFrame = 0;

	/** For -GutGroundLog: the sum and the worst of how far off the floor the lower foot was since the last line, as posed and as seen */
	float GroundLogSum[2] = { 0.0f, 0.0f };
	float GroundLogWorst[2] = { 0.0f, 0.0f };
	int32 GroundLogCount = 0;

	/** For -GutGroundLog: where each of the seen body's feet was last, whether it was down, how far feet that were down slid, and how far the body went */
	FVector GroundPartWas[2][2] = { { FVector::ZeroVector, FVector::ZeroVector }, { FVector::ZeroVector, FVector::ZeroVector } };
	bool bGroundFootDown[2] = { false, false };
	float GroundLogSlid = 0.0f;
	float GroundLogSlow = 0.0f;
	float GroundLogOwnSlid = 0.0f;
	float GroundLogApart[2] = { UE_BIG_NUMBER, 0.0f };
	float GroundLogBalls[2] = { 0.0f, 0.0f };
	FVector GroundBallWas[2][2] = { { FVector::ZeroVector, FVector::ZeroVector }, { FVector::ZeroVector, FVector::ZeroVector } };
	FVector GroundOwnWas[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bGroundOwnDown[2] = { false, false };
	float GroundLogWent = 0.0f;
	FVector GroundLogPlace = FVector::ZeroVector;

	/** Measures how far off the floor the seen body's lower foot is, and sets what the body is to be lowered or raised by */
	void GroundBody();
	bool bStrikeMeasured = false;

	/** For -GutBlowLog: the nearest the blow being thrown has come to the body it is thrown at, and where */
	float StrikeNearest = UE_BIG_NUMBER;
	FName StrikeNearestPart;

	/** Where the seen body rests on the fighter when nothing carries it */
	FVector MeshRestLocation = FVector(0.0f, 0.0f, -90.0f);

	/** How far in front of this fighter each clip's blow ends, as measured on this body the last times it threw it */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const UAnimSequenceBase>, float> StrikeReachKnown;

	/** What the last blow thrown came to */
	EGutHitResult LastStrikeResult = EGutHitResult::Hit;

	/** Time until a body that has been opened up drips again */
	float BleedIn = 0.0f;

	/** How far in front of this body the blow of a clip ends: as this body last threw it, or as anybody did. False if nobody has yet. */
	bool GetStrikeReach(const UAnimSequenceBase* Clip, float& OutReach) const;

	/** Carries the seen body out to where a blow lands and back. See StrikeReachOut. */
	void UpdateStrikeShift(float DeltaTime);

	/** Notes how far this body's blow reached, as the blow lands, for the next time it throws it */
	void MeasureStrike();

	/** Drips, if there is anything to drip from */
	void Bleed(float DeltaTime);

	/** The weapon in the fist as a line. See GetWeaponLine. */
	bool bWeaponLine = false;
	FVector WeaponLineBase = FVector::ZeroVector;
	FVector WeaponLineTip = FVector::ZeroVector;
	float WeaponLineRadius = 2.5f;

	TArray<FVector> ChainJoints;
	TArray<FVector> ChainJointsBefore;

	/** The joints a step earlier still, to tell how hard the chain is being jerked; the joints as drawn; and where the hand had it last frame */
	TArray<FVector> ChainJointsWere;
	TArray<FVector> ChainShown;
	FVector ChainAnchorBefore = FVector::ZeroVector;

	/** Number of times this fighter has staggered in this fight */
	int32 StaggerCount = 0;

	/** Length of the current stagger */
	float CurrentStaggerDuration = 0.0f;

	/** The fighter that is finishing this one. While set, the stagger does not run out. */
	TWeakObjectPtr<AGutFighter> Finisher;

	/** Finisher currently being performed */
	FGutFinisherDef CurrentFinisher;

	/** Style of the finisher currently being performed */
	EGutFinisherStyle CurrentFinisherStyle = EGutFinisherStyle::Clean;

	/** The staggered fighter this one is finishing */
	TWeakObjectPtr<AGutFighter> FinisherTarget;

	/** If true the killing blow of the current finisher has landed */
	bool bFinisherLanded = false;

	/** True while a fighter that started a finisher from too far off is still walking in to do it */
	bool bFinisherClosing = false;

	/** Attack currently being performed */
	FGutAttackDef CurrentAttack;

	/** Weight of the attack currently being performed */
	EGutAttackType CurrentAttackType = EGutAttackType::Light;

	/** If true the attack being performed was thrown to drive a staggered fighter across the cage, not to hurt anybody */
	bool bDriving = false;

	/** How many times this fighter has been driven while staggered, in this fight */
	int32 TimesDriven = 0;

	/** If true, the current attack turns to face the opponent during its windup */
	bool bTrackOpponent = false;

	/** Fighters the current swing has already reached, so one swing lands once */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGutFighter>> SwingHitFighters;

	/** Position in the light attack chain */
	int32 ComboIndex = 0;

	/** Time the last light attack ended */
	float LastLightAttackEndTime = -100.0f;

	/** Length of the current stun */
	float StunDuration = 0.0f;

	/** Time stamina was last spent */
	float LastStaminaUseTime = -100.0f;

	/** Time an action was last refused for lack of stamina */
	float LastStaminaFailTime = -100.0f;

	/** Time the last dodge ended */
	float LastDodgeEndTime = -100.0f;

	/** Kinds of action that can wait in the input buffer */
	enum class EBufferedAction : uint8 { None, Light, Heavy, Dodge };

	/** Action waiting for the fighter to become free */
	EBufferedAction BufferedAction = EBufferedAction::None;

	/** Time the buffered action was pressed */
	float BufferedActionTime = 0.0f;

	/** Direction of the buffered dodge */
	FVector BufferedDodgeDirection = FVector::ZeroVector;

	/** Result of the last attack that reached this fighter */
	EGutHitResult LastHitResult = EGutHitResult::Ignored;

	/** How this fighter's attacks have gone so far */
	FGutFighterTally Tally;

	/** The last fighter whose attack reached this one, blocked or not */
	TWeakObjectPtr<AGutFighter> LastAttacker;

	/** Time an attack last reached this fighter */
	float LastAttackedTime = -100.0f;

	/** Times the cage's hazards have hurt this fighter in this fight */
	int32 HazardHits = 0;

	/** The hazard that killed this fighter, if one did */
	TWeakObjectPtr<AGutHazard> KillingHazard;

	/** The fighter credited with killing this one, if any */
	TWeakObjectPtr<AGutFighter> CreditedKiller;

	/** Bones this fighter has been cut through at */
	TArray<FName> SeveredBones;

	/** If true this fighter was killed by a finisher, in the style below */
	bool bKilledByFinisher = false;

	int32 FightSide = INDEX_NONE;

	bool bWading = false;
	float WadeSpeedScale = 1.0f;
	float WadeDodgeScale = 1.0f;

	bool bLimp = false;

	EGutFinisherStyle KillingFinisherStyle = EGutFinisherStyle::Clean;

	/** Short text shown over the fighter's head, like a damage number */
	FString CombatText;

	/** Color of the combat text */
	FLinearColor CombatTextColor = FLinearColor::White;

	/** Time the combat text was set */
	float CombatTextTime = -100.0f;

public:

	/** Constructor */
	AGutFighter();

	/** Runs the state timers and stamina regeneration */
	virtual void Tick(float DeltaTime) override;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	/** Starts the next light attack in the chain, or buffers it if the fighter is busy. Returns true if the attack started now. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool TryLightAttack();

	/** Starts a heavy attack, or buffers it if the fighter is busy. Returns true if the attack started now. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool TryHeavyAttack();

	/** Raises or lowers the guard. The guard goes up as soon as the fighter is free. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void SetBlocking(bool bBlock);

	/** Dodges along the given world direction, or straight back if it is zero. Returns true if the dodge started now. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool TryDodge(FVector WorldDirection);

	/**
	 *  Finishes the opponent in the given style, if it is staggered and close enough.
	 *  The attack inputs do this by themselves: light finishes clean, heavy finishes slow.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool TryFinisher(EGutFinisherStyle Style);

	/**
	 *  The live hazard that a heavy blow thrown from where this fighter stands would send a beaten opponent onto: the
	 *  spikes or the cable, with the opponent between. None if the opponent is not beaten, is out of reach, or nothing
	 *  live lies that way. Whoever plays a fighter is offered it; the cage's own choose for themselves.
	 */
	class AGutHazard* FindDriveHazard(const AGutFighter* Target) const;

	/**
	 *  Throws the heavy attack at a staggered opponent to drive it, not to finish it: a blow that lands sends it reeling
	 *  the way the blow was going, and the cage ends it if that is onto something live. Returns true if the blow started now.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool TryDrive();

	/** Puts a weapon in the fighter's hand. Empty means bare hands. A fighter whose weapon arm is crippled cannot hold one. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool EquipWeapon(UGutWeaponData* NewWeapon);

	/** Puts on a piece of armor, in place of whatever was worn on that part of the body */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void WearArmor(UGutArmorData* Piece);

	/**
	 *  Hangs a bag at the left hip of the body that is seen, or takes it off (none). What the player carries in the town
	 *  is in it, and it is seen on them ("the bag needs to be visible on the person", 25 November). Where on the hip is
	 *  worked out from the body standing at rest, so it comes out the same on anybody. -GutBagOut=, -GutBagDown= (cm),
	 *  -GutBagRound=, -GutBagTip= (degrees) and -GutBagThin= (a share) try other places without a build.
	 */
	void WearBag(UStaticMesh* Bag);

	/** The bag at the hip, if one is worn */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BagMesh;

	/** Takes off all armor */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void ClearArmor();

	/** Lets go of the weapon. It falls to the floor and the fighter carries on bare handed. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void DropWeapon();

	/** Resolves an attack reaching this fighter. Called by the attacker. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	EGutHitResult ReceiveAttack(AGutFighter* Attacker, const FGutAttackDef& Attack);

	/**
	 *  Cuts the fighter's body through at the given bone. Everything below it comes off and lies on the floor.
	 *  Only for the dead: nothing in the fight rules knows about a fighter with a limb missing.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool SeverLimb(FName Bone, FVector ThrowDirection);

	/** Leaves blood on the floor near the fighter, thrown the given way. Amount is on the scale of an attack's damage. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void SpillBlood(FVector Direction, float Amount);

	/** Kills the fighter outright. Used by finishers. */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void Kill(AGutFighter* InKiller, FVector ImpulseDirection);

	/**
	 *  Resolves one of the cage's hazards hurting this fighter: damage, a stun and a throw along the given direction.
	 *  Kills outright, staggered or not, if the damage takes the last of the fighter's health. Called by the hazard.
	 *  Returns false if the fighter could not be hurt.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	bool ReceiveHazard(AGutHazard* Hazard, FVector AwayDirection);

	/** Turns the fighter toward a world position at the given rate */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void TurnToward(const FVector& WorldPoint, float DeltaTime, float RateDegreesPerSecond);

	/** Makes the fighter keep facing its opponent while it walks, so it can circle and back away without turning its back */
	UFUNCTION(BlueprintCallable, Category="Gut|Actions")
	void SetSquareUp(bool bEnabled);

	/** Makes the fighter get its stamina back faster or slower than its own rate: 1 is its own. For a fighter whose water has been seen to. */
	void SetStaminaRegenScale(float Scale) { StaminaRegenScale = FMath::Max(Scale, 0.0f); }

	/** How long after an effort its wind waits before it starts coming back, against how long it waits by nature */
	void SetStaminaRegenDelayScale(float Scale) { StaminaRegenDelayScale = FMath::Max(Scale, 0.0f); }

	/** Makes every blow the fighter lands do more or less than the attack says: 1 is as it says. For a fighter that has been trained, or doped. */
	void SetDamageScale(float Scale) { DamageScale = FMath::Max(Scale, 0.0f); }

	float GetDamageScale() const { return DamageScale; }

	/** Makes the fighter cover the floor faster or slower than its own speed: 1 is its own */
	void SetMoveSpeedScale(float Scale);

	/** Makes a blow that lands stop the fighter for longer or shorter than the attack says: 1 is as it says. For a fighter that feels less of it. */
	void SetStunScale(float Scale) { StunScale = FMath::Max(Scale, 0.0f); }

	/** If true the weapon in hand has been tampered with: it comes apart at the first blow that meets anything */
	void SetWeaponSabotaged(bool bSabotaged) { bWeaponSabotaged = bSabotaged; }

	bool IsWeaponSabotaged() const { return bWeaponSabotaged; }

	/** Sets the fighter on the other side of the cage */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetOpponent(AGutFighter* NewOpponent);

	/** Allows or forbids fighting. A fighter that cannot fight can still walk. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetCombatEnabled(bool bEnabled);

	/** Sets the name shown on the HUD. For a ladder, where the same fighter in the level stands in for one opponent after another. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetDisplayName(const FText& NewName) { DisplayName = NewName; }

	/** Sets the health the fighter starts a fight with, and fills it. Only before the bell. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetMaxHealth(float NewMaxHealth);

	/** Puts the collar on or takes it off. A fighter that has earned its tag wears none. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetCollared(bool bCollared);

	/** Gives the fighter a character of its own to be seen as, or with none takes it back to the placeholder */
	UFUNCTION(BlueprintCallable, Category="Gut|Look")
	void SetBody(TSoftClassPtr<AActor> NewBodyClass, UIKRetargeter* NewRetargeter);

	/** The skeleton the player sees: the fighter's own character if it has one, otherwise the placeholder */
	USkeletalMeshComponent* GetShownMesh() const;

	/** How the placeholder's pose is carried over to the fighter's own character */
	UIKRetargeter* GetBodyRetargeter() const { return BodyRetargeter; }

	/** Says whether the body is a woman's, which hits less hard than a man's and is quicker. A body it is given says so itself. */
	void SetWoman(bool bIsWoman);

	bool IsWoman() const { return bWoman; }

	/** Names the body this fighter will be given when it begins play. For a fighter made on the spot, before it has begun. */
	void SetBodyClass(TSoftClassPtr<AActor> NewBodyClass) { BodyClass = NewBodyClass; }

	/** True if the fighter is shown with a body of its own */
	UFUNCTION(BlueprintPure, Category="Gut|Look")
	bool HasBody() const;

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsCollared() const;

	/** Tints the body materials, so one fighter can be told from another */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetBodyTint(FLinearColor NewTint);

	/** Sets health directly. For tests and for carrying injuries between fights. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetHealth(float NewHealth);

	/** Sets stamina directly. For tests and for carrying exhaustion between fights. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetStamina(float NewStamina);

	/** Sets how much damage a limb can still take. Zero cripples it. For tests and for carrying injuries between fights. */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetLimbHealth(EGutLimb Limb, float NewHealth);

public:

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsAlive() const { return State != EGutFighterState::Dead; }

	UFUNCTION(BlueprintPure, Category="Gut")
	EGutFighterState GetFighterState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsAttacking() const { return State == EGutFighterState::Windup || State == EGutFighterState::Active || State == EGutFighterState::Recovery; }

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsBlocking() const { return State == EGutFighterState::Blocking; }

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsStaggered() const { return State == EGutFighterState::Staggered; }

	/** Returns the time left before a staggered fighter gets its second wind. Zero when not staggered. */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetStaggerTimeRemaining() const;

	/** Returns true while another fighter is in the middle of finishing this one */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsBeingFinished() const { return Finisher.IsValid(); }

	/** Returns true if this fighter could finish the other one right now */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool CanFinish(const AGutFighter* Target) const;

	UFUNCTION(BlueprintPure, Category="Gut")
	bool HasWeapon() const { return Weapon != nullptr; }

	/** Returns the weapon the fighter starts a fight holding when nothing is drawn from a rack */
	UFUNCTION(BlueprintPure, Category="Gut")
	UGutWeaponData* GetStartingWeapon() const { return StartingWeapon; }

	/** Returns the weapon in hand, or nothing when bare handed */
	UFUNCTION(BlueprintPure, Category="Gut")
	UGutWeaponData* GetWeapon() const { return Weapon; }

	/** Returns the armor worn on the given part of the body, or nothing */
	UFUNCTION(BlueprintPure, Category="Gut")
	UGutArmorData* GetArmorOn(EGutArmorSlot Slot) const;

	const TArray<TObjectPtr<UGutArmorData>>& GetArmor() const { return Armor; }

	/** The shapes of the armor worn, as they are on the body */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetArmorPlates() const { return ArmorPlates; }

	/** True while a made piece of armor is worn round the chest: the arms are then hung and held clear of it */
	bool HasChestPiece() const { return bChestPiece; }

	/** Returns the number of limbs that are crippled */
	UFUNCTION(BlueprintPure, Category="Gut")
	int32 GetCrippledLimbCount() const;

	/** Returns the weapon in hand, or the fists when bare handed */
	UFUNCTION(BlueprintPure, Category="Gut")
	const UGutWeaponData* GetArmament() const;

	/** Returns how the fighter stands and reacts with what it holds now: the weapon's own clips, and the fighter's where the weapon has none */
	FGutStanceAnimations GetStanceAnimations() const;

	/** Returns the clips of the fighter moving in its stance, or of its walk and jog when it is at ease and has them */
	const FGutLocomotionAnimations& GetLocomotionAnimations() const
	{
		if (bAtEase && bWoman && !bOwnWalk && AtEaseLocomotionWoman.WalkForward.Animation)
		{
			return AtEaseLocomotionWoman;
		}

		return (bAtEase && AtEaseLocomotion.WalkForward.Animation) ? AtEaseLocomotion : GetFightLocomotion();
	}

	/** The fighter's own walking and running in a fight, or with the clips of what it holds where that has its own */
	const FGutLocomotionAnimations& GetFightLocomotion() const;

	/** True while it stands at ease in one of the families, whose clips are each begun at their beginning */
	bool IsStandingInFamily() const { return bAtEase && Family != INDEX_NONE; }

	/** At ease: stands the way this family does (FGutStandFamily::Name), not the one it would have been dealt */
	void SetAtEaseFamily(const FName& Name);

	/** The ways of standing there are to choose from */
	const TArray<FGutStandFamily>& GetAtEaseFamilies() const { return AtEaseFamilies; }

	/** At ease: a flat-out run while this is set, whatever jogging is */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetRunning(bool bNewRunning);

	bool IsRunning() const { return bRunning; }

	/**
	 *  Puts the fighter at ease, as somebody is in the street, or back on its guard. At ease it cannot fight, stands and
	 *  walks as anybody does, and its empty hands hang open instead of being fists.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetAtEase(bool bNewAtEase);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsAtEase() const { return bAtEase; }

	/**
	 *  True for the first moments of somebody who was stood at ease as they were made: a person of the town as their
	 *  place opens. A fighter is made on its guard, and was eased down from it to standing over half a second, its arms
	 *  drawn in to its sides over another half and its jacket thrown about by both: so whoever came out of the cage beside
	 *  somebody saw them with their fists up and their clothes flying, "perfect once they move at all" (25 November; the
	 *  same on the 24th, when I mended something else). In these moments nothing is eased: they are standing already.
	 */
	bool IsNewlyAtEase() const { return bAtEase && GetGameTimeSinceCreation() < GutNewlyAtEase; }

	/** How long those first moments are */
	static constexpr float GutNewlyAtEase = 1.5f;

private:
	bool bHangsHead = false;
	bool bLeftOutOfPicture = false;

	/** What was hidden when it was left out of the picture, to be shown again */
	TArray<TWeakObjectPtr<AActor>> LeftOut;

public:

	/** How far the striking limb is being taken off its clip to land the blow, in the world, and which limb. See SteerBlow. */
	const FVector& GetBlowReach() const { return BlowReach; }
	int32 GetBlowLimb() const { return BlowLimb; }

	/** Left out of the picture, body, clothes and all, while the camera is right up against it: see AGutCameraManager */
	void SetLeftOutOfPicture(bool bLeftOut);
	bool IsLeftOutOfPicture() const { return bLeftOutOfPicture; }

	/** True for the instant it is held still by a blow that has just landed, its own or the other's */
	bool IsStoppedForBlow() const { return BlowStopEnds > 0.0f; }

	/** Somebody who hangs their head and is meant to: a lot on the Block, somebody led by the wrists. Everybody else at ease carries theirs up. */
	void SetHangsHead(bool bHangs) { bHangsHead = bHangs; }
	bool HangsHead() const { return bHangsHead || IsBound(); }

	/** True while it is doing one of the small things somebody standing about does: only its upper body does it */
	bool IsFidgeting() const { return FidgetLeft > 0.0f; }

	bool bGestureKeepsHead = false;

	/** How far the given arm (0 left, 1 right) is brought in toward the body from where the clip hangs it, in degrees */
	float GetArmIn(int32 Side) const { return ArmsIn[Side & 1]; }

	/** True while the body is being taken along a clip its performer travelled in: the clip's own travel is left out of the pose */
	bool IsCarriedPose() const { return bCarriedPose; }

	/**
	 *  The body as it is dressed and what is in its hands, for keeping its own arms, hands and weapon out of it: see
	 *  FGutClearOp, which asks every frame. False for somebody it is not to be done to (no body of their own, dead, a beast).
	 */
	bool GetClearShape(struct FGutClearShape& OutShape);

	/**
	 *  What this body's head is turned toward, if anything: in a fight the head of whoever it is fighting, and out of one
	 *  whoever is walking the street when they come near (or whoever is looking on, where nobody is walking).
	 *  @return false for nothing to look at
	 */
	bool GetLookAt(FVector& OutPlace) const;

	/** Somebody for this body to look at in particular, before anybody it would look at by itself: whoever it is talking to. None to clear. */
	void SetLookTarget(const AGutFighter* Whom) { LookTarget = Whom; }

	/**
	 *  Ways of standing at ease, for anybody who was not given one of their own where they were placed: each body takes
	 *  one of these and keeps it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fighter|At ease")
	TArray<TObjectPtr<UAnimSequenceBase>> AtEaseIdles;

	/**
	 *  Small things somebody standing about does without thinking: a hand to the back of the neck, a look over the
	 *  shoulder, a shift of the feet. One is laid over the standing every so often, and nobody does them in step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fighter|At ease")
	TArray<TObjectPtr<UAnimSequenceBase>> AtEaseFidgets;

	/** The shortest and the longest that somebody stands before the next of them, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fighter|At ease", meta = (ClampMin = 1, Units = "s"))
	FVector2D FidgetEvery = FVector2D(14.0, 38.0);

	/** How near somebody has to come before a body at ease turns its head to them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fighter|At ease", meta = (ClampMin = 0, Units = "cm"))
	float LookRange = 520.0f;

	/** At ease: a jog while this is set, a walk otherwise */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetJogging(bool bNewJogging);

	bool IsJogging() const { return bJogging; }

	/** Ties its wrists behind its back, or lets them go: a lot on the Block, somebody bought and led away */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void SetBound(bool bNewBound);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsBound() const { return BoundClip != nullptr; }

	/** True while the hands of the body that is seen are shut for it, round what it holds or into fists or half shut when tied: the clips' own fingers are then not used */
	bool AreFingersShutForIt() const { return bFingersShutForIt; }

	UAnimSequenceBase* GetBoundArms() const { return BoundClip; }

	float GetBoundPast() const { return BoundPast; }

	/**
	 *  Something done with the arms and the head over whatever way it is standing, as the small things it does by itself
	 *  are: somebody talking with their hands as they say a line, somebody listening. Dropped if it has somewhere to go.
	 */
	void Gesture(UAnimSequenceBase* Clip, bool bWithHead = true);

	/** True while the gesture going on is of the hands only: the head is left to look where it was looking */
	bool DoesGestureKeepHead() const { return bGestureKeepsHead; }

	/** Lets go of that, back to standing as it stood */
	void StopGesture();

	/**
	 *  Has the face say a line: a performance of the face made from the line's own recording (Epic's speech-to-face), begun
	 *  as the recording is. Nothing for no performance. He said of a Warden that he was "like a narrator": a voice came
	 *  from a face that did not move.
	 */
	void Speak(UAnimSequenceBase* Face);

	/**
	 *  Has it take something it is handed and do with it what this clip does (put it to the mouth, light it). Somebody whose
	 *  wrists are tied has them let go for as long as the clip lasts, and tied again after.
	 */
	void TakeInHand(UAnimSequenceBase* Taking);

	/** At ease: stands in this clip, not the one every fighter has. For the people of the street, who do not all stand alike. */
	void SetAtEaseIdle(UAnimSequenceBase* Idle) { AtEaseStance.Idle = Idle; bIdlePlaced = Idle != nullptr; }

	/** Gives this body a walk of its own for when it is at ease: a loop of whole steps in place, and the speed it was walked at */
	void SetAtEaseWalk(UAnimSequenceBase* Walk, float Speed)
	{
		if (Walk)
		{
			AtEaseLocomotion.WalkForward = AtEaseLocomotion.WalkRight = AtEaseLocomotion.WalkBack = AtEaseLocomotion.WalkLeft = FGutMoveClip{ Walk, Speed };
			bOwnWalk = true;
		}
	}

	/**
	 *  Has the fighter's legs walk at the speed something else is moving it at: somebody led across a deck that is no
	 *  ground to its own movement. Nought is standing where it was put.
	 */
	void SetLedVelocity(const FVector& Velocity) { LedVelocity = Velocity; }

	const FVector& GetLedVelocity() const { return LedVelocity; }

	/** The body that is seen, where the fighter has been given one */
	AActor* GetBodyActor() const { return BodyActor; }

	/**
	 *  A weapon gripped in the right fist as a line: its two ends in the space of the hand's bone, and half its thickness.
	 *  Returns false for bare hands and for anything strapped on or hanging, which the wrist does not point.
	 */
	bool GetWeaponLine(FVector& OutBase, FVector& OutTip, float& OutRadius) const;

	/**
	 *  The end of whatever this fighter is hitting with, on the body that is seen: toward the end of a club or a blade, the
	 *  boss of a shield, or the fist or foot that is furthest out. False for a chain, which finds its own way, and for an animal.
	 */
	bool GetStrikingPoint(FVector& OutPoint, int32* OutLimb = nullptr) const;

	/** How far a point is from the skin of the body that is seen, less than nought inside it, and which part of it is nearest */
	float GetBodyGapFrom(const FVector& Point, FName* OutPart = nullptr, FVector* OutNearest = nullptr) const;

	/**
	 *  How far in front of its own middle the end of what this fighter hits with lands, by what it is holding and nothing
	 *  else. The step into a blow is measured by it, which is a rule of the fight.
	 */
	float GetArmReach() const;

	/** How far in front of its own middle a blow meets this fighter: its chest, or whatever it is holding up */
	float GetStruckDepth() const;

	/**
	 *  How far the weapon in the fist is from the middle of the fighter's own neck and skull, on the body that is seen.
	 *  For checking that it is not held through them. Negative if nothing is gripped.
	 */
	float GetWeaponNeckGap(bool bOnFightingSkeleton = false) const;

	/** Returns the arm that holds, or would hold, the given weapon */
	UFUNCTION(BlueprintPure, Category="Gut")
	EGutLimb GetArmFor(const UGutWeaponData* ForWeapon) const;

	/** Returns the stamina it costs this fighter to block the given attack, with what it is holding and the arms it has left */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetBlockCost(const FGutAttackDef& Attack) const;

	/** Returns how this fighter's attacks have gone so far in this fight */
	const FGutFighterTally& GetTally() const { return Tally; }

	/** Returns the number of times the cage's hazards have hurt this fighter in this fight */
	UFUNCTION(BlueprintPure, Category="Gut")
	int32 GetHazardHits() const { return HazardHits; }

	/** Returns how many times this fighter has been sent reeling by a blow while it was staggered, in this fight */
	UFUNCTION(BlueprintPure, Category="Gut")
	int32 GetTimesDriven() const { return TimesDriven; }

	/** Returns how far a blow thrown to drive a staggered fighter sends it */
	float GetDriveDistance() const { return DriveDistance; }

	/** Returns the hazard that killed this fighter, or nothing if it is alive or something else killed it */
	UFUNCTION(BlueprintPure, Category="Gut")
	AGutHazard* GetKillingHazard() const { return KillingHazard.Get(); }

	/** Returns true if the fighter's body has been cut through at the given bone */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsSevered(FName Bone) const { return SeveredBones.Contains(Bone); }

	UFUNCTION(BlueprintPure, Category="Gut")
	int32 GetNumSevered() const { return SeveredBones.Num(); }

	/** Returns true if this fighter was killed by a finisher, and not by the cage */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool WasFinished() const { return bKilledByFinisher; }

	/** Standing in water over the ankles: slower on its feet, shorter in a dodge, and heard. 1 and 1 for dry tile. */
	void SetWading(bool bInWater, float SpeedScale = 1.0f, float DodgeScale = 1.0f);

	bool IsWading() const { return bWading; }

	/** Which side this fighter is on in a fight of more than two, or negative in a fight of two, where there are no sides */
	int32 GetSide() const { return FightSide; }
	void SetSide(int32 NewSide) { FightSide = NewSide; }

	/** True once a dead body has gone limp. A body that dies on its feet takes a moment to. */
	bool IsLimp() const { return bLimp; }

	/**
	 *  Makes this fighter an animal for the rest of the fight: the animal's body in place of a person's, its bite for a weapon,
	 *  its health and its speed, and the cage's brain told how such a thing fights. It is still a fighter in every rule
	 *  but these: it has no limbs to cripple, it does not reel on its feet when it is beaten but dies, and it holds nothing.
	 */
	UFUNCTION(BlueprintCallable, Category="Gut")
	void BecomeBeast(UGutBeastData* NewBeast);

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsBeast() const { return Beast != nullptr; }

	const UGutBeastData* GetBeast() const { return Beast; }

	/** An animal this fighter is from the moment it is put in the world, set before that: it is never given a person's body at all */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gut|Body")
	TObjectPtr<UGutBeastData> SpawnAsBeast;

	/** The animal's own body, if this fighter is one */
	USkeletalMeshComponent* GetBeastBody() const { return BeastBody; }

	/** The fight is over and this one is still standing: once it has nothing else to do, it shows the crowd. Walking off ends it. */
	void Celebrate(float Delay);

	bool IsCelebrating() const { return bCelebrating; }

	/** Beaten, and nobody is going to finish it: after a moment it goes down onto its knees and stays there */
	void Yield(float Delay);

	bool HasYielded() const { return bYielded; }

	/** The fight is over and this one will not kill the one it has beaten: once it has nothing else to do, it lets its hands drop and stands there */
	void Spare(float Delay);

	/** True while this fighter is acting out what a finisher is doing to it */
	bool IsBeingTaken() const { return bBeingTaken; }

	/** Returns the style of the finisher that killed this fighter. Only meaningful if one did. */
	UFUNCTION(BlueprintPure, Category="Gut")
	EGutFinisherStyle GetKillingFinisherStyle() const { return KillingFinisherStyle; }

	/** Returns the fighter credited with killing this one: its finisher, or the blow that drove it into a hazard. Nothing if the cage killed it alone. */
	UFUNCTION(BlueprintPure, Category="Gut")
	AGutFighter* GetKiller() const { return CreditedKiller.Get(); }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetLimbHealth(EGutLimb Limb) const { return LimbHealth[static_cast<int32>(Limb)]; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetMaxLimbHealth() const { return MaxLimbHealth; }

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsLimbCrippled(EGutLimb Limb) const { return LimbHealth[static_cast<int32>(Limb)] <= 0.0f; }

	/** Returns the finisher being performed. Only meaningful while finishing. */
	const FGutFinisherDef& GetCurrentFinisher() const { return CurrentFinisher; }

	float GetFinisherRange() const { return FinisherRange; }

	/** Returns true while a dodge lets attacks pass through */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsInvulnerable() const;

	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsCombatEnabled() const { return bCombatEnabled; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category="Gut")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category="Gut")
	AGutFighter* GetOpponent() const { return Opponent.Get(); }

	UFUNCTION(BlueprintPure, Category="Gut")
	FText GetDisplayName() const { return DisplayName; }

	/** Returns the attack being performed. Only meaningful while attacking. */
	UFUNCTION(BlueprintPure, Category="Gut")
	const FGutAttackDef& GetCurrentAttack() const { return CurrentAttack; }

	UFUNCTION(BlueprintPure, Category="Gut")
	EGutAttackType GetCurrentAttackType() const { return CurrentAttackType; }

	/** Returns the number of attacks this fighter has started. Changes every time a new swing begins. */
	UFUNCTION(BlueprintPure, Category="Gut")
	int32 GetAttackSerial() const { return AttackSerial; }

	/** Returns the time the current attack started. Only meaningful while attacking. */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetAttackStartTime() const { return AttackStartTime; }

	/** Returns the position in the light attack chain that would play if the fighter attacked now */
	int32 GetNextComboIndex() const;

	/** Returns the light attack that would start if the fighter attacked now */
	const FGutAttackDef* GetNextLightAttack() const;

	/** Returns the heavy attack that would start if the fighter attacked now */
	const FGutAttackDef& GetHeavyAttack() const;

	float GetDodgeStaminaCost() const { return DodgeStaminaCost * (bWoman ? WomanDodgeCostScale : 1.0f); }

	float GetStateTime() const { return StateTime; }

	EGutHitResult GetLastHitResult() const { return LastHitResult; }

	/** How many of the blows this fighter has taken drew blood. What is on the floor goes on growing after a blow, as the drops it threw come down and the wound drips. */
	int32 GetTimesBloodDrawn() const { return TimesBloodDrawn; }

	/** Returns the time since an action was last refused for lack of stamina */
	float GetTimeSinceStaminaFail() const;

	/** Returns the text to show over the fighter's head and how long it has been showing */
	bool GetCombatText(FString& OutText, FLinearColor& OutColor, float& OutAge) const;

	/** Returns the distance from this fighter's center to the surface of the other fighter */
	UFUNCTION(BlueprintPure, Category="Gut")
	float GetGapTo(const AGutFighter* Other) const;

	/** Returns true if the other fighter is inside an arc of the given reach and half angle in front of this fighter */
	UFUNCTION(BlueprintPure, Category="Gut")
	bool IsInArc(const AGutFighter* Other, float Reach, float HalfAngleDegrees) const;

protected:

	/** Changes state and applies everything that depends on it */
	void SetState(EGutFighterState NewState);

	/** Applies movement speed and rotation rules for the current state */
	void UpdateMovementForState();

	/** Starts an attack if the fighter is free and can pay for it */
	bool StartAttack(const FGutAttackDef& Attack, EGutAttackType Type);

	/** Starts a dodge if the fighter is free and can pay for it */
	bool StartDodge(const FVector& WorldDirection);

	/** Tests the current attack against every fighter in reach */
	void PerformAttackSweep();

	/** Returns true if this fighter has not had its turn yet this frame */
	bool IsAwaitingTick() const;

	/** Returns true if this fighter's own attack is due to land this frame, in a turn it has not had yet */
	bool HasAttackDueThisFrame() const;

	/** Returns the fighter to idle after an attack */
	void FinishAttack();

	/** Performs the buffered action if there is one and it is still fresh */
	void ConsumeBufferedAction();

	/** Stores an action to perform when the fighter becomes free */
	void BufferAction(EBufferedAction Action, const FVector& DodgeDirection = FVector::ZeroVector);

	/** Interrupts whatever the fighter is doing and stuns it */
	/** Reels for that long. With a clip, in that clip: how a blow is taken depends on where it landed. Without, as the stance takes one in the face. */
	void EnterStun(float Duration, UAnimSequenceBase* Reaction = nullptr);

	/** The clip for reeling from this blow: by the limb it landed on, or by where it came in from */
	UAnimSequenceBase* ChooseReaction(const FGutAttackDef& Attack, const FVector& FromAttacker, bool bOnLimb, EGutLimb Limb) const;

	/** That clip or another that does the same thing, if the stance has others for it: not the one it gave last time */
	UAnimSequenceBase* Vary(const FGutStanceAnimations& Clips, UAnimSequenceBase* Clip) const;

	mutable TWeakObjectPtr<const UAnimSequenceBase> LastVaried;

	/** The clip the last blow was thrown with */
	TWeakObjectPtr<const UAnimSequenceBase> LastThrown;

	/** Interrupts whatever the fighter is doing and leaves it helpless, waiting to be finished */
	void EnterStagger();

	/** Gives a staggered fighter that nobody finished its second wind */
	void RecoverFromStagger();

	/** Starts finishing the given staggered fighter */
	void StartFinisher(AGutFighter* Target, EGutFinisherStyle Style);

	/** Begins the finisher's windup, and what the one it is done to does meanwhile */
	void BeginFinisherWindup(AGutFighter* Target);

	/** Stops whatever the fighter is in the middle of: a push, an animation, a finisher on someone else */
	void InterruptAction();

	/** Works out which limb an attack lands on. Returns false if it lands on the body. */
	bool ResolveLimb(const FGutAttackDef& Attack, const FVector& FromAttacker, EGutLimb& OutLimb) const;

	/** Removes limb health and cripples the limb if it runs out */
	void ApplyLimbDamage(EGutLimb Limb, float Damage);

	/** Applies everything that follows from a limb being crippled */
	void CrippleLimb(EGutLimb Limb);

	/** Returns the walking speed multiplier from crippled legs */
	float GetLegSpeedScale() const;

	/** Rebuilds the plates on the body to match the armor worn */
	void UpdateArmorPlates();

	/**
	 *  Measures the dressed body, as it stands at rest, round the stretch of it a piece of armor covers: from one bone toward
	 *  another, the middle Share of the way. The answer is in that bone's own space: which way the stretch runs, which way
	 *  is the body's front, and how far the body reaches across (X) and front to back (Y) from the line between the bones.
	 *  False if the body cannot be measured there.
	 */
	bool MeasureRound(const FName& Bone, const FName& TowardBone, float Share, float Reach, FVector& OutToward, FVector& OutFront, FBox2D& OutBody,
		float Below = 0.0f, float PastShoulders = 0.5f, const AActor* Dressed = nullptr, const USkeletalMeshComponent* DressedBody = nullptr) const;

public:

	/**
	 *  The dressed shape of a body that is nobody's fighter: somebody standing in a crowd. Measured once for each kind of
	 *  body and kept, as a fighter's is, and with arms that hang brought in to its own sides. False if it cannot be measured.
	 */
	static bool MeasureShapeOf(const AActor* Dressed, const USkeletalMeshComponent* DressedBody, struct FGutClearShape& OutShape);

protected:

	/** Shows or hides the weapon shape to match the weapon in hand */
	void UpdateWeaponMesh();

	/** Called as the body's pose is settled each frame: the moment to do what depends on where its bones have ended up */
	void OnBodyPosed();

	/** Puts the rope round the two wrists of the body that is seen, wherever they have got to, or puts it away */
	void UpdateTie();

	/** Hands this frame of the Face Loop to the body, which the face reads its expression from */
	void FeedFace();

	/** Lets a chain in the hand hang and swing for this frame */
	void UpdateChain();

	/** The sound of the chain this frame: how hard its far end was jerked, and whether a link came down on the floor */
	void ChainNoise(float Jolt, bool bStruckFloor);

	/** Plays one of the Gut's sounds where the fighter is, or at the place given */
	void MakeSound(FName Kind, float Volume = 1.0f, const FVector* Where = nullptr, float Pitch = 1.0f) const;

	/** The name of a sound that depends on a weapon: the prefix, then what the weapon sounds like */
	static FName WeaponSound(const TCHAR* Prefix, const UGutWeaponData* ForWeapon);

	/** The name of one of this fighter's own noises: the prefix, then whose voice it has */
	FName VoiceSound(const TCHAR* Prefix) const;

	/** Puts the collar on the skeleton that is seen, and round the neck of a body of its own as that neck was modelled */
	void FitCollar();

	/** Listens for the feet: a foot that was moving and has come to rest on the floor is a footfall */
	void ListenToFeet();

	/** Removes health and returns true if that killed the fighter */
	bool ApplyHealthDamage(float Damage);

	/** Ends the fighter's part in the fight */
	void Die(AGutFighter* InKiller, const FVector& ImpulseDirection);

	/** Spends stamina and restarts the regeneration delay */
	void SpendStamina(float Amount);

	/** Returns true if the fighter has at least this much stamina, and flags the HUD if not */
	bool CheckStamina(float Amount);

	/** Moves the fighter a set distance over a set time, overriding walking */
	void ApplyPush(const FVector& Direction, float Distance, float Duration);

	/** Stops any push in progress */
	void CancelPush();

	/** Plays a placeholder animation on the animation slot */
	void PlaySlotAnimation(UAnimSequenceBase* Animation, float PlayRate, float StartTime, float BlendIn = 0.08f, float BlendOut = 0.2f, int32 Loops = 1);

	/** Stops the placeholder animation */
	void StopSlotAnimation(float BlendOut = 0.2f);

	/** Sets the text shown over the fighter's head */
	void ShowCombatText(const FString& Text, const FLinearColor& Color);

	/** Returns true if the opponent exists and is alive */
	bool HasLiveOpponent() const;

	/** Called when one of this fighter's attacks reaches a target */
	virtual void HandleAttackLanded(AGutFighter* Target, EGutHitResult Result, const FGutAttackDef& Attack);

	/** Called when an attack reaches this fighter */
	virtual void HandleAttackReceived(AGutFighter* Attacker, EGutHitResult Result, const FGutAttackDef& Attack);

	/** Called when this fighter dies, however it happened */
	virtual void HandleDeath(AGutFighter* InKiller);

	/** Called when this fighter is beaten and staggers */
	virtual void HandleStaggered();

protected:

	/** Blueprint hook: an attack has started its windup */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Attack Started"))
	void BP_OnAttackStarted(const FGutAttackDef& Attack, EGutAttackType Type);

	/** Blueprint hook: one of this fighter's attacks reached a target */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Attack Landed"))
	void BP_OnAttackLanded(AGutFighter* Target, EGutHitResult Result, const FGutAttackDef& Attack);

	/** Blueprint hook: an attack reached this fighter */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Attack Received"))
	void BP_OnAttackReceived(AGutFighter* Attacker, EGutHitResult Result, const FGutAttackDef& Attack);

	/** Blueprint hook: the fighter started a dodge */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Dodge Started"))
	void BP_OnDodgeStarted(FVector Direction);

	/** Blueprint hook: one of the fighter's limbs was crippled */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Limb Crippled"))
	void BP_OnLimbCrippled(EGutLimb Limb);

	/** Blueprint hook: the fighter let go of its weapon */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Weapon Dropped"))
	void BP_OnWeaponDropped(UGutWeaponData* DroppedWeapon, AGutDroppedWeapon* Prop);

	/** Blueprint hook: the fighter was beaten and staggered */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Staggered"))
	void BP_OnStaggered();

	/** Blueprint hook: the fighter got its second wind after a stagger */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Second Wind"))
	void BP_OnSecondWind();

	/** Blueprint hook: the fighter started finishing a staggered opponent */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Finisher Started"))
	void BP_OnFinisherStarted(AGutFighter* Target, EGutFinisherStyle Style);

	/** Blueprint hook: one of the cage's hazards hurt the fighter */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Hazard Hit"))
	void BP_OnHazardHit(AGutHazard* Hazard);

	/** Blueprint hook: the fighter died */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut", meta = (DisplayName = "On Died"))
	void BP_OnDied(AGutFighter* Killer);
};
