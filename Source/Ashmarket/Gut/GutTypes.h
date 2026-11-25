// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequenceBase.h"
#include "GutTypes.generated.h"

/** Log category for everything that happens in the cage */
DECLARE_LOG_CATEGORY_EXTERN(LogGut, Log, All);

/**
 *  The words for one person, a man or a woman. Nobody the game names is "they" or "it": every body in it is plainly a
 *  man's or a woman's, and whoever is bought, signed, led, fed or buried is spoken of as he or she (24 November).
 */
struct FGutWho
{
	bool bWoman = false;

	FGutWho() = default;
	explicit FGutWho(bool bIsWoman) : bWoman(bIsWoman) {}

	const TCHAR* He() const { return bWoman ? TEXT("she") : TEXT("he"); }
	const TCHAR* Him() const { return bWoman ? TEXT("her") : TEXT("him"); }
	const TCHAR* His() const { return bWoman ? TEXT("her") : TEXT("his"); }
	const TCHAR* Hers() const { return bWoman ? TEXT("hers") : TEXT("his"); }
	const TCHAR* HeCap() const { return bWoman ? TEXT("She") : TEXT("He"); }
	const TCHAR* HisCap() const { return bWoman ? TEXT("Her") : TEXT("His"); }
	const TCHAR* Man() const { return bWoman ? TEXT("woman") : TEXT("man"); }
};

/** What a fighter is doing right now. A fighter is always in exactly one state. */
UENUM(BlueprintType)
enum class EGutFighterState : uint8
{
	/** Free to move and start any action */
	Idle,

	/** Attack is telegraphed but cannot hit yet */
	Windup,

	/** Attack can hit */
	Active,

	/** Attack is over and the fighter is committed until it ends */
	Recovery,

	/** Guard is up */
	Blocking,

	/** Mid dodge */
	Dodging,

	/** Reeling from a hit or a broken guard */
	Stunned,

	/** Beaten and helpless. Can be finished, and gets a second wind if it is not. */
	Staggered,

	/** Finishing a staggered fighter */
	Finishing,

	/** Out of the fight for good */
	Dead
};

/** The two melee attack weights */
UENUM(BlueprintType)
enum class EGutAttackType : uint8
{
	Light,
	Heavy
};

/** The parts of a fighter that can be crippled */
UENUM(BlueprintType)
enum class EGutLimb : uint8
{
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg
};

/** Number of limbs in EGutLimb */
inline constexpr int32 GutNumLimbs = 4;

/** The side that dogs let in on a fight are on: neither the player's (0) nor the opponent's (1) */
inline constexpr int32 GutDogsSide = 2;

/** Where on the body an attack lands when it is thrown from in front */
UENUM(BlueprintType)
enum class EGutHitZone : uint8
{
	/** The body or head: health damage only */
	Torso,

	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg
};

/** Where a blow comes in from, as the one it lands on sees it when the two stand face to face. It decides which way that one reels. */
UENUM(BlueprintType)
enum class EGutBlowFrom : uint8
{
	/** Straight in at the head: it snaps back */
	Front,

	/** Round from the target's left: the head goes right */
	Left,

	/** Round from the target's right: the head goes left */
	Right,

	/** Down from above: the knees give */
	Above,

	/** Into the middle: the body folds */
	Body
};

/** What kind of blow an attack is. Armor is made against one kind or the other. */
UENUM(BlueprintType)
enum class EGutDamageType : uint8
{
	/** Clubs, chains, shields, fists */
	Blunt,

	/** Anything with an edge */
	Bladed
};

/** The parts of the body a piece of armor can cover */
UENUM(BlueprintType)
enum class EGutArmorSlot : uint8
{
	Torso,
	Arms,
	Legs
};

/** The two things a fighter may take before the bell. Both are legal in the ring, and both are paid for later. */
UENUM(BlueprintType)
enum class EGutDrug : uint8
{
	/** Speed and a heavier hand for one fight. The habit comes fast, and a dose on top of a full habit damages the heart for good. */
	Dust,

	/** Takes the pain away: a blow that lands stops the fighter for half as long. Without it, once the habit has hold, health is halved. */
	Hush
};

/** What a sponsor asks of a fighter in return for its water */
UENUM(BlueprintType)
enum class EGutSponsorDemand : uint8
{
	/** Finish the opponent the slow way */
	KillSlowly,

	/** Fight in the sponsor's colors, which means without armor over them */
	WearColors,

	/**
	 *  Lose. The other corner has been paid to leave the fighter breathing: beaten, it is spared, paid, and fights the same fight again later.
	 *  A fighter that takes the water and wins anyway keeps the win and owes the water back.
	 */
	ThrowFight
};

/** The two ways to finish a staggered fighter */
UENUM(BlueprintType)
enum class EGutFinisherStyle : uint8
{
	/** Over quickly. On the light attack input. */
	Clean,

	/** Drawn out. On the heavy attack input. */
	Slow
};

/** What happened when an attack reached a fighter */
UENUM(BlueprintType)
enum class EGutHitResult : uint8
{
	/** The target could not be hit at all (dead, or the fight is not running) */
	Ignored,

	/** Full damage */
	Hit,

	/** Stopped by the guard, paid for in stamina */
	Blocked,

	/** Guard was up but stamina ran out: reduced damage and a long stun */
	GuardBroken,

	/** Passed through a dodge */
	Dodged
};

/** The stages of a single cage fight */
UENUM(BlueprintType)
enum class EGutFightPhase : uint8
{
	/** On a ladder: the card before the fight, with the story so far, the opponent, and the cutter */
	Card,

	/** Fighters are at the rack, drawing their weapons */
	Choosing,

	/** On a ladder: the betting window */
	Betting,

	/** Weapons are drawn, the bell has not rung */
	Intro,

	/** Bell has rung */
	Fighting,

	/** The enemy is dead */
	PlayerWon,

	/** The player is dead, or was beaten and spared */
	PlayerLost,

	/** On a ladder: the last fight is over and the player is at the gate with the bill */
	Gate
};

/** Who a bet at the window is on */
UENUM(BlueprintType)
enum class EGutBetSide : uint8
{
	/** No bet */
	None,

	/** On the player's own fighter */
	Player,

	/** On the opponent, against the player's own fighter */
	Opponent
};

/** The inputs that work the screens before and after a fight: the card, the rack, the betting window and the gate */
UENUM(BlueprintType)
enum class EGutMenuInput : uint8
{
	Left,
	Right,
	Up,
	Down,
	Confirm
};

/**
 *  One melee attack, described as data.
 *  Timing, reach and damage live here and never in the animation, so a fight plays the same with or without art.
 *  The animation is only played to match.
 */
/** Another way of throwing the same blow: another performance of it, and where in that the blow begins and lands */
USTRUCT(BlueprintType)
struct FGutAttackWay
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimStartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimImpactTime = 0.45f;
};

USTRUCT(BlueprintType)
struct FGutAttackDef
{
	GENERATED_BODY()

	/** Short name shown on the HUD while this attack is winding up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	FName Label = NAME_None;

	/** Health removed on a clean hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 500))
	float Damage = 10.0f;

	/** Stamina the attacker pays to start the attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 500))
	float StaminaCost = 12.0f;

	/** Time from input to the first moment the attack can hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Timing", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float Windup = 0.22f;

	/** Time the attack stays able to hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Timing", meta = (ClampMin = 0.01, ClampMax = 5, Units = "s"))
	float Active = 0.1f;

	/** Time the attacker stays committed after the attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Timing", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float Recovery = 0.26f;

	/** How far the attack reaches, measured from the attacker's center to the surface of the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Shape", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float Reach = 125.0f;

	/** Half width of the attack arc, measured from the attacker's facing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Shape", meta = (ClampMin = 1, ClampMax = 180, Units = "deg"))
	float HalfAngle = 55.0f;

	/** Distance the attacker steps forward during the windup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Shape", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float LungeDistance = 35.0f;

	/**
	 *  Time the target is stunned by a clean hit.
	 *  For an attack in a chain, keep this longer than the time until the next attack of the chain lands, less the fastest windup.
	 *  Otherwise a fighter who has just been hit can swing first, and landing a hit becomes a disadvantage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float StunTime = 0.45f;

	/** Distance the target is pushed back by a clean hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float Knockback = 30.0f;

	/**
	 *  Stamina a blocking target pays to stop this attack.
	 *  Light attacks cost less to block than to throw, so hammering at a raised guard tires the attacker first. Heavies are what break guards.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit", meta = (ClampMin = 0, ClampMax = 500))
	float BlockStaminaDamage = 8.0f;

	/**
	 *  The part of the target this attack lands on when thrown from in front. Limbs take damage of their own and can be crippled.
	 *  An attacker standing well off to one side hits the limb on that side instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit")
	EGutHitZone HitZone = EGutHitZone::Torso;

	/** What kind of blow this is. Armor is made against one kind or the other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit")
	EGutDamageType DamageType = EGutDamageType::Blunt;

	/** Limb damage dealt on a clean hit, as a share of the attack's damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit", meta = (ClampMin = 0, ClampMax = 10))
	float LimbDamageScale = 1.0f;

	/** Where the blow comes in from, which decides how a target it lands on cleanly reels. A blow on a limb, or from behind, has its own way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|On Hit")
	EGutBlowFrom BlowFrom = EGutBlowFrom::Front;

	/** Placeholder animation to play. Optional: the attack works without one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** Time in the animation to start from, to skip any lead-in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimStartTime = 0.0f;

	/** Time in the animation where the blow lands. The play rate is set so this lines up with the end of the windup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimImpactTime = 0.45f;

	/** Other performances of the same blow. Each time it is thrown it is thrown one of these ways or the first, never the same way twice running: it lands when it lands whichever it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Animation")
	TArray<FGutAttackWay> Others;

	/** Total time the attacker is committed */
	float GetTotalTime() const { return Windup + Active + Recovery; }
};

/** Other ways of doing what one clip does: whenever that clip would be played, it or one of these is */
USTRUCT(BlueprintType)
struct FGutOtherWays
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> For;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TArray<TObjectPtr<UAnimSequenceBase>> Ways;
};

/**
 *  How a fighter stands and reacts while it holds one weapon: the clips that are not attacks.
 *  Every one is optional. Where a weapon leaves one empty the fighter's own is used, and where both are empty nothing plays.
 */
USTRUCT(BlueprintType)
struct FGutStanceAnimations
{
	GENERATED_BODY()

	/** Standing ready with the weapon. A loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Idle;

	/**
	 *  More than one way of taking a blow, of being driven back off a guard, of dodging: for any clip below, others that
	 *  do the same thing. Nobody reels the same way twice running (23 November: of 1,008 clips bought, 212 were in use).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TArray<FGutOtherWays> Others;

	/** How the winner shows the crowd, holding this: if there are none, the fighter's own ways are used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TArray<TObjectPtr<UAnimSequenceBase>> Victory;

	/** Holding the guard up. A loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Block;

	/**
	 *  Where in that clip the guard is held, in seconds, when the clip is not a loop but somebody putting a guard up and
	 *  taking it down again: the guard is that one moment of it, held still. Less than nought for a loop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	float BlockHold = -1.0f;

	/** Taking a blow on the guard: jolted, and the guard still up. Played once, at its own speed, each time a blow is stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> BlockHit;

	/** Taking a blow. Its opening is played, stretched over the stun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Stun;

	/** How much of the stun clip, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float StunLength = 0.5f;

	/**
	 *  Taking a blow from somewhere other than straight in front: on one side of the head, in the middle, on top of the head,
	 *  in the back, on an arm, on a leg. Each is used like the stun clip, and where one is empty the stun clip is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitBody;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitTop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitLeftArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitRightArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitLeftLeg;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Blows")
	TObjectPtr<UAnimSequenceBase> HitRightLeg;

	/** Out on its feet. A loop, played at its own speed for as long as the stagger lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Stagger;

	/** A dodge the way the fighter is steering. Its opening is played, stretched over the dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Dodge;

	/**
	 *  A dodge to the left and to the right of a fighter that keeps facing its opponent as it goes: a sidestep.
	 *  Used like the dodge clip. Where one is empty the fighter turns and dashes the way it is steering instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> DodgeLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> DodgeRight;

	/** How much of the dodge clip, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float DodgeLength = 0.5f;

	/** A dodge with no direction given: a step straight back, still facing forward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	TObjectPtr<UAnimSequenceBase> Backstep;

	/** How much of the backstep clip, in seconds from its start, is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance", meta = (ClampMin = 0.05, ClampMax = 5, Units = "s"))
	float BackstepLength = 0.5f;

	/**
	 *  If true the arms and chest of the idle are kept while the fighter walks, over the legs of the walking clip.
	 *  For anything held in a way the walking clips, which are walked with bare fists up, do not show.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance")
	bool bKeepUpperBodyWhileMoving = true;

	/**
	 *  Which way a weapon gripped in the fist points while the fighter stands ready, in the fighter's own space: X forward, Y right, Z up.
	 *  The clips are of a body with empty hands, and say nothing reliable about the turn of the wrist, so the wrist is turned to this.
	 *  Leave at zero for a weapon that hangs or is strapped on, and the wrist is left as the clip has it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Weapon")
	FVector WeaponAim = FVector::ZeroVector;

	/** The same with the guard up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stance|Weapon")
	FVector BlockWeaponAim = FVector::ZeroVector;
};

/** One clip of a fighter stepping round on the spot, with the turn taken out: the game turns the fighter */
USTRUCT(BlueprintType)
struct FGutTurnClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turn")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** How far round the body in the clip went, to the left if positive */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turn", meta = (Units = "deg"))
	float Degrees = 45.0f;

	/** How much of that turn the body had made at each thirtieth of a second of the clip, from 0 to 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turn")
	TArray<float> Progress;

	/** How much of the turn is made by a time in the clip */
	float GetProgress(float Time) const
	{
		if (Progress.Num() < 2)
		{
			return Animation && Animation->GetPlayLength() > 0.0f ? FMath::Clamp(Time / Animation->GetPlayLength(), 0.0f, 1.0f) : 1.0f;
		}

		const float At = FMath::Clamp(Time * 30.0f, 0.0f, static_cast<float>(Progress.Num() - 1));
		const int32 First = FMath::Min(FMath::FloorToInt32(At), Progress.Num() - 2);

		return FMath::Lerp(Progress[First], Progress[First + 1], At - First);
	}
};

/**
 *  One way of standing about, out of a fight: clips recorded from one stance (weight on the left leg, arms crossed, a hand
 *  on a hip), each of which begins and ends as the others do. Somebody who stands this way goes from one of them to the next
 *  and never does the same few seconds twice running.
 */
USTRUCT(BlueprintType)
struct FGutStandFamily
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stand")
	FName Name;

	/** Who is dealt it when they were given no way of their own: 0 nobody (it has to be asked for by name), 1 men, 2 women, 3 anybody */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stand", meta = (ClampMin = 0, ClampMax = 3))
	int32 DealtTo = 3;

	/** The standing itself: each can follow any other */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stand")
	TArray<TObjectPtr<UAnimSequenceBase>> Loops;

	/** Small things done from that stance and back to it, now and then: a look round, a hand to the neck */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stand")
	TArray<TObjectPtr<UAnimSequenceBase>> Extras;
};

/** One clip of a fighter moving in its stance, and how fast the body in it was really going */
USTRUCT(BlueprintType)
struct FGutMoveClip
{
	GENERATED_BODY()

	/** A loop of whole steps, with the travel taken out: the game moves the fighter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** Speed the clip was made at. Played faster or slower as the fighter goes faster or slower than this, within limits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move", meta = (ClampMin = 1, ClampMax = 2000, Units = "cm/s"))
	float Speed = 150.0f;

	/** How many times faster than it was made the clip is played at that speed: a stroll walked in a small room, played as a walk */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move", meta = (ClampMin = 0.25, ClampMax = 4))
	float Quickened = 1.0f;

	/** How many whole cycles (a left step and a right) the clip holds. Clips are kept in step with one another by the cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move", meta = (ClampMin = 1, ClampMax = 8))
	int32 Cycles = 1;

	/**
	 *  Where in its first cycle the clip's left foot passes its right going forward, as a share of the cycle. Clips that are
	 *  mixed are read so that these fall together: a walk and a run that were not recorded from the same footfall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move", meta = (ClampMin = 0, ClampMax = 1))
	float Phase = 0.0f;

	/** How long one cycle of it takes at its speed */
	float GetCycleTime() const { return Animation ? Animation->GetPlayLength() / (FMath::Max(Quickened, 0.25f) * FMath::Max(Cycles, 1)) : 1.0f; }
};

/**
 *  A fighter moving in its stance: forward, back and to each side, at a walk and at a run.
 *  The fighter keeps facing its opponent, so going sideways is a sidestep and going back is a retreat.
 */
USTRUCT(BlueprintType)
struct FGutLocomotionAnimations
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Walk")
	FGutMoveClip WalkForward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Walk")
	FGutMoveClip WalkRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Walk")
	FGutMoveClip WalkBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Walk")
	FGutMoveClip WalkLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Run")
	FGutMoveClip RunForward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Run")
	FGutMoveClip RunRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Run")
	FGutMoveClip RunBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Run")
	FGutMoveClip RunLeft;

	/** Faster than a run, forward only: a flat-out run in the street. Nothing in the cage has one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Run")
	FGutMoveClip Sprint;

	/** Stepping round on the spot to keep facing an opponent: a short turn and a long one, each way */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Turn")
	FGutTurnClip TurnLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Turn")
	FGutTurnClip TurnRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Turn")
	FGutTurnClip TurnLeftFar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Turn")
	FGutTurnClip TurnRightFar;

	/** How somebody killed by the live cable dies: rigid and shaking, and then down. And the time in it at which the body goes limp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Death")
	TObjectPtr<UAnimSequenceBase> DeathShock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Death", meta = (ClampMin = 0, Units = "s"))
	float DeathShockLimpTime = 0.0f;

	/** What somebody still standing does when the other one is dead: one of these, by chance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Victory")
	TArray<TObjectPtr<UAnimSequenceBase>> Victory;

	/** What somebody beaten and let live does: down onto the knees, once, and then kneeling there for as long as it lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Spared")
	TObjectPtr<UAnimSequenceBase> YieldDown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Spared")
	TObjectPtr<UAnimSequenceBase> YieldKneel;

	/** And what the one who will not finish them does: lets its hands drop, once, and then stands there over them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Spared")
	TObjectPtr<UAnimSequenceBase> SpareLower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move|Spared")
	TObjectPtr<UAnimSequenceBase> SpareStand;

	/** The walking clip for a way: 0 forward, 1 right, 2 back, 3 left */
	const FGutMoveClip& GetWalk(int32 Way) const
	{
		const FGutMoveClip* Clips[] = { &WalkForward, &WalkRight, &WalkBack, &WalkLeft };

		return *Clips[Way & 3];
	}

	/** The running clip for a way: 0 forward, 1 right, 2 back, 3 left */
	const FGutMoveClip& GetRun(int32 Way) const
	{
		const FGutMoveClip* Clips[] = { &RunForward, &RunRight, &RunBack, &RunLeft };

		return *Clips[Way & 3];
	}
};

/** The pit boss's special cards (design document, chapter 7: "special cards run two on two, one against three") */
UENUM(BlueprintType)
enum class EGutSpecialCard : uint8
{
	/** One against one, as every other fight is */
	None,

	/** The player alone against the fighter on the card and two more */
	OneAgainstThree,

	/** The player and a second against the fighter on the card and its second */
	TwoOnTwo,

	/** The player alone against the pit boss's dogs: the fighter on the card sits it out */
	Animal,

	/** Not a card that is offered: the fighter on the card has dogs of its own, and brings them in */
	WithDogs
};

/** A part of the story that is told in pictures, each over the whole screen with a line or two under it */
UENUM(BlueprintType)
enum class EGutStory : uint8
{
	None,

	/** How the player came to be in the cage: before the first card of a new run up the ladder */
	Sold,

	/** Out of the gate with a tag: after the last fight of Meat */
	Tagged,

	/** The Champion is dead: after the last fight of all */
	Champion,

	/** Coming to the Gut with water to buy fighters: before an owner's first day */
	Owner,

	/** Coming to the Gut with water to put on them: before a gambler's first night */
	Gambler
};

/** One picture of such a part: a photograph, by the name it is kept under, and the words under it */
USTRUCT(BlueprintType)
struct FGutStoryPanel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story")
	FName Picture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story")
	TArray<FText> Words;
};

/**
 *  One way of finishing a staggered fighter, described as data like an attack.
 *  A finisher always kills. All that varies is how long it takes and what it looks like.
 */
USTRUCT(BlueprintType)
struct FGutFinisherDef
{
	GENERATED_BODY()

	/** Short name shown on the HUD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher")
	FName Label = NAME_None;

	/** Time from input to the killing blow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Timing", meta = (ClampMin = 0.05, ClampMax = 10, Units = "s"))
	float Windup = 0.6f;

	/** Time the finisher stays committed after the killing blow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Timing", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float Recovery = 0.5f;

	/** Bones of the body the killing blow cuts through. Everything below each comes off. Empty for a finisher that leaves the body whole. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher")
	TArray<FName> SeverBones;

	/** How near the one being finished has to be for the hands or the weapon to reach it: the gap, as a fighter measures it.
	 *  Started from further off, the fighter walks in first and the windup begins when it gets there. 0: done from where it stands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher", meta = (ClampMin = 0, Units = "cm"))
	float Reach = 0.0f;

	/** Placeholder animation to play. Optional: the finisher works without one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	/** Time in the animation to start from, to skip any lead-in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimStartTime = 0.0f;

	/** Time in the animation where the blow lands. The play rate is set so this lines up with the end of the windup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation", meta = (ClampMin = 0, Units = "s"))
	float AnimImpactTime = 0.45f;

	/** What the one it is done to does while it is done: played on the target from the finisher's start. Optional: without one it goes on swaying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation")
	TObjectPtr<UAnimSequenceBase> TakenAnimation;

	/** Time in that animation where the blow lands on it. The play rate is set so this lines up with the end of the windup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation", meta = (ClampMin = 0, Units = "s"))
	float TakenImpactTime = 0.0f;

	/** How the body dies once the blow has landed, if not by going limp at once: played on it from the blow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	/** Time in that animation at which the body goes limp and is left to the floor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation", meta = (ClampMin = 0, Units = "s"))
	float DeathLimpTime = 0.0f;

	/**
	 *  For a finisher that two people performed together: how far apart the two stood, facing, as it began. The two clips
	 *  are then played as they were performed, at their own speed and from their start, each body is carried along its own
	 *  clip, and the one it is done to goes on in its clip past the blow, down to the floor, until Death Limp Time (which is
	 *  then a time in the Taken Animation). 0: not such a finisher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finisher|Animation", meta = (ClampMin = 0, Units = "cm"))
	float PairApart = 0.0f;
};
