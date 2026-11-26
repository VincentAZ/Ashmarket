// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutBrainController.h"
#include "GutTypes.h"
#include "GutOpponentController.generated.h"

class AGutHazard;

/** What the opponent's brain is doing right now */
UENUM(BlueprintType)
enum class EGutOpponentState : uint8
{
	/** Walking in from far away */
	Approach,

	/** Circling just outside reach, deciding what to do */
	Hold,

	/** Stepping in and swinging */
	Attack,

	/** Standing behind its guard */
	Guard,

	/** Backing off to get its stamina back */
	Retreat,

	/** Closing in on a staggered target to finish it */
	Finish
};

/**
 *  Everything that makes one opponent fight differently from another.
 *  Chances are 0 to 1. A harder opponent reacts sooner, guards more and wastes less stamina.
 *
 *  The defaults were set by playing the opponent against itself with Tools/run_gut_balance.sh
 *  until the middle fight lasted about 70 seconds. Re-run it after changing them.
 */
USTRUCT(BlueprintType)
struct FGutOpponentTuning
{
	GENERATED_BODY()

	/** The opponent circles no closer than this far beyond the reach of its target's light attack, lunge included */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spacing", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float HoldMarginMin = 15.0f;

	/** The opponent circles no further than this far beyond the reach of its target's light attack, lunge included */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spacing", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float HoldMarginMax = 100.0f;

	/** A retreating opponent tries to get this far away */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spacing", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float RetreatGap = 420.0f;

	/** The opponent swings once its target is inside this share of the attack's reach */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spacing", meta = (ClampMin = 0.1, ClampMax = 1.5))
	float StrikeReachFraction = 0.85f;

	/** Walking speed when closing in, as a share of the fighter's walking speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta = (ClampMin = 0, ClampMax = 1))
	float ApproachSpeedScale = 0.9f;

	/** Walking speed when circling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta = (ClampMin = 0, ClampMax = 1))
	float CircleSpeedScale = 0.4f;

	/** Walking speed when backing away. Slower than the approach, so a retreating fighter can be caught. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta = (ClampMin = 0, ClampMax = 1))
	float RetreatSpeedScale = 0.6f;

	/** Shortest wait between two decisions while circling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decisions", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MinDecisionInterval = 1.0f;

	/** Longest wait between two decisions while circling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decisions", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MaxDecisionInterval = 2.2f;

	/** Chance that a decision is to attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decisions", meta = (ClampMin = 0, ClampMax = 1))
	float AttackChance = 0.3f;

	/** Chance that a decision is to raise the guard and wait behind it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decisions", meta = (ClampMin = 0, ClampMax = 1))
	float GuardChance = 0.25f;

	/** When the target walks inside the circling distance the opponent either swings at it or guards. This is the chance it swings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decisions", meta = (ClampMin = 0, ClampMax = 1))
	float CrowdedAttackChance = 0.2f;

	/** Chance that an attack is the heavy rather than a light chain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float HeavyChance = 0.2f;

	/** Chance of the heavy when the target is standing behind its guard. Heavies break guards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float HeavyChanceAgainstGuard = 0.55f;

	/**
	 *  Chance of the heavy when its knockback would carry the target onto a hazard that is live or about to be.
	 *  This is the one way the opponent uses the cage against its target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float HeavyChanceIntoHazard = 0.7f;

	/**
	 *  How far it works the cage, nought to one. With any, when it changes the way it circles it takes the way that puts
	 *  it on the far side of its target from the nearest spikes or cable, so the target's back comes round to them; it
	 *  is likelier to attack at all when a heavy blow would carry the target onto them; and it keeps its own body that
	 *  much further off them (up to GutOpponentConstants::CunningMargin more). Nought is a fighter who circles as it
	 *  falls out: what every fighter was before 25 November.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float Cunning = 0.0f;

	/** Longest light chain the opponent plans */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 1, ClampMax = 10))
	int32 MaxLightChain = 3;

	/** Chance of carrying on with a planned chain after a light attack lands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float ChainAfterHitChance = 0.8f;

	/** Chance of carrying on with a planned chain after a light attack is blocked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float ChainAfterBlockChance = 0.4f;

	/** Time the opponent keeps trying to get into range before it gives up on an attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MaxClosingTime = 1.2f;

	/** Stamina the opponent keeps back when it starts an attack or carries on a chain, so it can still guard afterwards */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 500))
	float AttackStaminaReserve = 15.0f;

	/** Chance of hitting back straight after blocking or dodging an attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float CounterChance = 0.8f;

	/** Chance of stepping in when the target swings and misses */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta = (ClampMin = 0, ClampMax = 1))
	float PunishChance = 0.5f;

	/** Shortest time between seeing an attack start and doing something about it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MinReactionTime = 0.2f;

	/** Longest time between seeing an attack start and doing something about it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MaxReactionTime = 0.35f;

	/** Chance of trying to block a light attack on reaction. Light attacks are usually too fast for this to work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 1))
	float BlockLightChance = 0.5f;

	/** Chance of blocking a heavy attack on reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 1))
	float BlockHeavyChance = 0.4f;

	/** Chance of dodging a heavy attack on reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 1))
	float DodgeHeavyChance = 0.3f;

	/** How long before a heavy attack lands the opponent starts its dodge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 2, Units = "s"))
	float DodgeLeadTime = 0.14f;

	/** Shortest time the opponent stands behind its guard when it decides to wait */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MinGuardTime = 0.7f;

	/** Longest time the opponent stands behind its guard when it decides to wait */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MaxGuardTime = 1.4f;

	/** Chance of covering up behind the guard straight after taking a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence", meta = (ClampMin = 0, ClampMax = 1))
	float GuardAfterHitChance = 0.85f;

	/**
	 *  If true this one does not cover up when its opponent crowds it, which every other brain does whenever it is crowded
	 *  and does not strike. For somebody paid to lose: an animal, which never guards either, still takes that pause.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence")
	bool bNeverCovers = false;

	/** Shortest time the opponent takes to start walking in on a target that has just staggered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finishing", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MinFinishDelay = 0.3f;

	/** Longest time the opponent takes to start walking in on a target that has just staggered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finishing", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MaxFinishDelay = 0.9f;

	/** Chance the opponent finishes its target the slow way rather than the clean way */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finishing", meta = (ClampMin = 0, ClampMax = 1))
	float SlowFinisherChance = 0.3f;

	/**
	 *  Chance the opponent lets the cage finish a target that has staggered within a blow's drive of the spikes or the
	 *  cable while they are live: it gets round to the other side of it and knocks it onto them. Only done to a target
	 *  that another brain is fighting, never to the player, who is finished by hand as it always was.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finishing", meta = (ClampMin = 0, ClampMax = 1))
	float CageFinishChance = 0.5f;

	/** With less than this left of the target's stagger and no blow yet thrown to drive it, the opponent gives the cage up and finishes it by hand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Finishing", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float CageFinishGiveUpTime = 1.5f;

	/** Below this much stamina the opponent stops what it is doing and backs off */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retreat", meta = (ClampMin = 0, ClampMax = 500))
	float RetreatStamina = 25.0f;

	/** The opponent comes back in once it has this much stamina */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retreat", meta = (ClampMin = 0, ClampMax = 500))
	float RecoveredStamina = 65.0f;

	/** Longest time the opponent keeps backing off, whatever its stamina */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retreat", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MaxRetreatTime = 3.5f;

	/**
	 *  The opponent makes its decisions at the pace set above when its light attacks take this long on average, which is the club.
	 *  With a quicker weapon it decides sooner in proportion, and with a slower one later, so a fast weapon is pressed fast.
	 *  Zero switches this off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float TempoReferenceTime = 0.777f;

	/** The most a quick weapon can shorten the wait between decisions, as a share of the normal wait */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta = (ClampMin = 0.1, ClampMax = 1))
	float MinTempoScale = 0.7f;

	/** The most a slow weapon can lengthen the wait between decisions, as a share of the normal wait */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta = (ClampMin = 1, ClampMax = 5))
	float MaxTempoScale = 1.3f;
};

/**
 *  The opponent's brain. It has five behaviours and moves between them on what it sees:
 *  - Approach and hold: walks in, then circles just outside reach
 *  - Attack: steps in for a light chain or a heavy, hits back after a block, punishes a miss
 *  - Block: guards on a hunch while waiting, and guards or dodges on reaction when it sees a swing coming
 *  - Retreat: backs off, still facing its target, when its stamina runs low
 *  - Finish: walks in on a staggered target and finishes it
 *
 *  Wherever it walks, it keeps clear of the cage's hazards once they are live or about to be, and treats them as walls.
 *  If one lies between it and its target it walks round before it does anything else.
 *  When its target has its back to one, it favours the heavy, whose knockback carries the target in.
 *
 *  It only knows what a player could see, and it reacts after a delay, so it can be feinted, baited and outrun.
 *
 *  It fights to its weapon in three ways, all read from the weapon's own numbers: it keeps the distance its target's reach
 *  dictates and swings from as far out as its own allows, it presses faster with a faster weapon,
 *  and it trusts its guard more the better its weapon guards.
 */
UCLASS()
class AGutOpponentController : public AGutBrainController
{
	GENERATED_BODY()

protected:

	/** How this opponent fights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Opponent", meta = (ShowOnlyInnerProperties))
	FGutOpponentTuning Tuning;

	/** What the brain is doing */
	EGutOpponentState State = EGutOpponentState::Approach;

	/** Time the brain entered its current state */
	float StateStartTime = 0.0f;

	/** Time of the next decision while circling */
	float NextDecisionTime = 0.0f;

	/** Time the opponent may next answer being crowded */
	float NextCrowdedDecisionTime = 0.0f;

	/** Which way the opponent is circling: 1 or -1 */
	float CircleDirection = 1.0f;

	/** Time the opponent next thinks about circling the other way */
	float NextCircleFlipTime = 0.0f;

	/** If true the planned attack is the heavy */
	bool bPlanHeavy = false;

	/** Light attacks left in the planned chain */
	int32 PlannedLights = 0;

	/** If true a swing of the planned attack has been thrown and its result is not yet acted on */
	bool bSwingThrown = false;

	/** What the last swing thrown did. Ignored means it hit nothing. */
	EGutHitResult LastSwingResult = EGutHitResult::Ignored;

	/** Time the guard comes down */
	float GuardUntilTime = 0.0f;

	/** If true the opponent hits back as soon as its guard comes down */
	bool bCounterQueued = false;

	/** If true enough of its own side are already on the one it is after, and it holds off until one of them is out of it */
	bool bWaitsItsTurn = false;

	/** If true the swing being set up is that answer: it is thrown from as far out as it will reach, without stepping in first */
	bool bCountering = false;

	/** If true the opponent covers up as soon as it can move again */
	bool bGuardWhenFree = false;

	/** Ways of answering an attack that is on its way */
	enum class EReaction : uint8 { None, Block, Dodge };

	/** How the opponent has decided to answer the attack it has seen */
	EReaction PendingReaction = EReaction::None;

	/** Time the pending reaction happens */
	float ReactionTime = 0.0f;

	/** The target's attack the opponent has already made up its mind about */
	int32 JudgedAttackSerial = -1;

	/** The target's attack the opponent has already considered punishing */
	int32 PunishJudgedSerial = -1;

	/** The target's attack that has already reached the opponent, so it is no longer a threat */
	int32 ResolvedAttackSerial = -1;

	/** A cornered opponent does not try to back off again before this time */
	float NoRetreatUntilTime = 0.0f;

	/** Time the opponent starts walking in on its staggered target */
	float FinishStartTime = 0.0f;

	/** How the opponent means to finish its staggered target */
	EGutFinisherStyle PlannedFinisher = EGutFinisherStyle::Clean;

	/** The hazard the opponent means to knock its staggered target onto, if it means to let the cage finish it */
	TWeakObjectPtr<AGutHazard> CageFinish;

	/** Whom it is fighting, as of its last thought: for what is worked out without being told (which way to circle) */
	TWeakObjectPtr<const AGutFighter> Fought;

	/** If true the blow that is to drive the target onto it has been thrown */
	bool bCageBlowThrown = false;

	/** Which way round its target the opponent is going to get behind it: 1 or -1, chosen once */
	float CageRoundSide = 0.0f;

	/** The cage's hazards, found when the fight starts */
	TArray<TWeakObjectPtr<AGutHazard>> Hazards;

	/** The way the opponent is turning to get round a hazard in its path: 1 or -1, or zero when nothing is in the way */
	float DetourSide = 0.0f;

	/** Time a hazard was last in the opponent's way */
	float LastDetourTime = -100.0f;

public:

	virtual void NotifyIncomingAttack(AGutFighter* Fighter, AGutFighter* Attacker) override;

	/** Returns what the brain is doing */
	UFUNCTION(BlueprintPure, Category="Gut|Opponent")
	EGutOpponentState GetBrainState() const { return State; }

	/** Returns how this opponent fights */
	UFUNCTION(BlueprintPure, Category="Gut|Opponent")
	const FGutOpponentTuning& GetTuning() const { return Tuning; }

	/** In a fight of more than two: holds this fighter off from starting attacks while enough of its side are already at it */
	void SetWaitsItsTurn(bool bWaits) { bWaitsItsTurn = bWaits; }
	bool WaitsItsTurn() const { return bWaitsItsTurn; }

	/** Changes how this opponent fights */
	UFUNCTION(BlueprintCallable, Category="Gut|Opponent")
	void SetTuning(const FGutOpponentTuning& NewTuning) { Tuning = NewTuning; }

	/** Returns the closest the opponent means to circle the given target */
	UFUNCTION(BlueprintPure, Category="Gut|Opponent")
	float GetHoldGapMin(const AGutFighter* Target) const;

	/** Returns the furthest the opponent means to circle the given target */
	UFUNCTION(BlueprintPure, Category="Gut|Opponent")
	float GetHoldGapMax(const AGutFighter* Target) const;

protected:

	// ~begin AController interface

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	// ~end AController interface

	// ~begin GutBrainController interface

	virtual void OnFightStarted(AGutFighter* Fighter, AGutFighter* Target) override;

	virtual void TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime) override;

	virtual void OnFightStopped(AGutFighter* Fighter) override;

	// ~end GutBrainController interface

protected:

	/** Changes what the brain is doing */
	void SetBrainState(EGutOpponentState NewState);

	/** Starts circling and schedules the next decision */
	void EnterHold(float ExtraDelay = 0.0f);

	/** Raises the guard until at least the given time */
	void EnterGuard(AGutFighter* Fighter, float UntilTime);

	/** Starts backing off */
	void EnterRetreat(AGutFighter* Fighter);

	/** Plans an attack and starts closing in. Returns false if the fighter cannot afford one. */
	bool BeginAttack(AGutFighter* Fighter, AGutFighter* Target);

	/** Notices attacks coming in and answers them after a delay */
	void UpdateReaction(AGutFighter* Fighter, AGutFighter* Target);

	void TickApproach(AGutFighter* Fighter, AGutFighter* Target);

	void TickHold(AGutFighter* Fighter, AGutFighter* Target);

	void TickAttack(AGutFighter* Fighter, AGutFighter* Target);

	void TickGuard(AGutFighter* Fighter, AGutFighter* Target);

	void TickRetreat(AGutFighter* Fighter, AGutFighter* Target);

	void TickFinish(AGutFighter* Fighter, AGutFighter* Target);

	/** Returns how far the target's light attack reaches, lunge included */
	float GetLightThreatRange(const AGutFighter* Target) const;

	/** Returns the multiplier on the wait between decisions for the weapon the fighter is holding. Below one is faster. */
	float GetTempoScale() const;

	/**
	 *  Returns the chance of choosing to guard with the weapon the fighter is holding, given the chance with an ordinary one.
	 *  The share of times it chooses not to goes up and down with what its weapon costs to guard with,
	 *  so an opponent with a shield spends much of the fight behind it and one with a chain rarely bothers.
	 */
	float GetGuardChance(const AGutFighter* Fighter, float BaseChance) const;

	/** Picks what to do next while circling */
	void Decide(AGutFighter* Fighter, AGutFighter* Target);

	/** Walks sideways around the target, turning back at walls. Returns false if there is nowhere to go. */
	bool Circle(AGutFighter* Fighter, const FVector& ToTarget, float SpeedScale);

	/**
	 *  Walks the fighter the given way. If a hazard that is live or about to be is in the way it turns aside and follows
	 *  the hazard's edge round, always to the same side until the way is clear again, so it cannot dither in front of it.
	 *  Hemmed in, it stands still.
	 */
	void Move(AGutFighter* Fighter, const FVector& Direction, float SpeedScale);

	/**
	 *  Returns the way to walk to get to the target: straight at it, or toward the way round a hazard that lies between them.
	 *  Says whether it was the way round.
	 */
	FVector GetWayTo(const AGutFighter* Fighter, const AGutFighter* Target, bool& bOutRoundHazard) const;

	/** Returns how far the fighter could walk the given way before it met a wall, up to the given distance */
	float GetRoomToward(const AGutFighter* Fighter, const FVector& Direction, float MaxDistance) const;

	/** Returns the distance from a point to the nearest hazard that is live or about to be, the way out of it, and which hazard it is */
	float GetHazardDistance(const FVector& Point, FVector& OutAwayDirection, const AGutHazard** OutHazard = nullptr) const;

	/**
	 *  Returns the live hazard a staggered target could be knocked onto from where it stands, or nothing: the nearest
	 *  one that a blow's drive would carry it to.
	 */
	AGutHazard* FindCageFinish(const AGutFighter* Fighter, const AGutFighter* Target) const;

	/**
	 *  Gets to the far side of the staggered target from the hazard and knocks it on. Returns false when that is no
	 *  longer worth trying, and the target is to be finished by hand.
	 */
	bool TickCageFinish(AGutFighter* Fighter, AGutFighter* Target);

	/** Returns true if a clean hit that knocks the target back the given distance would carry it onto a hazard */
	bool WouldKnockOntoHazard(const AGutFighter* Fighter, const AGutFighter* Target, float Knockback) const;

	/** Returns true if the fighter can take a step the given way without coming too close to a hazard */
	bool IsHazardPathClear(const AGutFighter* Fighter, const FVector& Direction) const;

	/** Returns true if a wall, or a hazard that is live or about to be, is close in the given direction */
	bool IsWallClose(const AGutFighter* Fighter, const FVector& Direction) const;

	/** Returns true if the target's swing in progress could reach the fighter */
	bool IsThreatened(const AGutFighter* Fighter, const AGutFighter* Target) const;

	/** Called when one of the fighter's attacks reaches its target */
	UFUNCTION()
	void OnFighterAttackLanded(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result);

	/** Called when an attack reaches the fighter */
	UFUNCTION()
	void OnFighterAttackReceived(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result);

	/** Called when one of the cage's hazards hurts the fighter */
	UFUNCTION()
	void OnFighterHazardHit(AGutFighter* Fighter, AGutHazard* Hazard);
};
