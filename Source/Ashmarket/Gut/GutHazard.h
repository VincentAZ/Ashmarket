// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GutHazard.generated.h"

class AGutFighter;
class UNiagaraSystem;

/** What a hazard is doing */
UENUM(BlueprintType)
enum class EGutHazardState : uint8
{
	/** Switched off. Harmless. */
	Off,

	/** About to switch on. Still harmless, but it shows. */
	Warning,

	/** Switched on. Hurts whoever touches it. */
	Live
};

/**
 *  Something in the cage that the pit boss switches on in a later round, and that hurts any fighter who touches it.
 *  A touch costs health, stuns the fighter and throws it clear. If it takes the last of the fighter's health,
 *  the cage kills it outright: no stagger, no second wind.
 *
 *  The game mode decides when a hazard warns and goes live, from the round set on it.
 *  Subclasses say where the hazard is, and what it looks like in each state.
 */
UCLASS(abstract)
class AGutHazard : public AActor
{
	GENERATED_BODY()

protected:

	/** Name used on the HUD, for example SPIKES */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard")
	FText DisplayName;

	/** What the HUD announces when the hazard goes live, for example THE SPIKES ARE OUT */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard")
	FText LiveText;

	/** Word written over a fighter's head when the hazard hurts it, before the damage, for example SPIKED */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard")
	FText HitText;

	/** Round of the fight in which the hazard goes live. Round 1 starts at the bell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 1, ClampMax = 20))
	int32 ActivationRound = 2;

	/** Health a fighter loses each time it touches the live hazard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 0, ClampMax = 1000))
	float Damage = 20.0f;

	/** Time a fighter is stunned by a touch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float StunTime = 0.8f;

	/** Distance a touch throws the fighter clear of the hazard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float ThrowDistance = 150.0f;

	/** Time a throw takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 0.01, ClampMax = 2, Units = "s"))
	float ThrowDuration = 0.2f;

	/** Time after a touch before the same fighter can be hurt by this hazard again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float HitCooldown = 1.0f;

	bool bShocks = false;

	/** The names of the sounds it makes: when it hurts somebody, while it is about to switch on, while it is on, and as it switches on. Each is optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hazard|Sound")
	FName HurtSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hazard|Sound")
	FName WarningSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hazard|Sound")
	FName LiveSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hazard|Sound")
	FName SwitchSound = NAME_None;

	/** Where its sounds come from */
	virtual FVector GetSoundPlace() const { return GetActorLocation(); }

	/** The sound that plays for as long as the hazard is in its present state, if any */
	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> StateSound;

	/** If true a fighter in the middle of a dodge passes over the hazard unhurt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Hazard")
	bool bDodgeAvoids = false;

	/** Effect spawned on a fighter that touches the live hazard. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Presentation")
	TObjectPtr<UNiagaraSystem> HitEffect;

	/** Current state */
	EGutHazardState State = EGutHazardState::Off;

	/** Time spent in the current state */
	float StateTime = 0.0f;

	/** Time each fighter was last hurt by this hazard */
	TMap<TWeakObjectPtr<AGutFighter>, float> LastHitTimes;

public:

	/** Constructor */
	AGutHazard();

	/** Hurts any fighter touching the hazard while it is live */
	virtual void Tick(float DeltaTime) override;

	/** Switches the hazard off, to its warning, or on */
	UFUNCTION(BlueprintCallable, Category="Gut|Hazard")
	void SetHazardState(EGutHazardState NewState);

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	EGutHazardState GetHazardState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	bool IsLive() const { return State == EGutHazardState::Live; }

	/** Returns the time the hazard has been in its current state */
	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetStateTime() const { return StateTime; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	int32 GetActivationRound() const { return ActivationRound; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	FText GetLiveText() const { return LiveText; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	FText GetHitText() const { return HitText; }

	/** True if what this does to a body is done by current: somebody it kills dies rigid and shaking before going down */
	bool Shocks() const { return bShocks; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetStunTime() const { return StunTime; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetThrowDistance() const { return ThrowDistance; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetThrowDuration() const { return ThrowDuration; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	float GetHitCooldown() const { return HitCooldown; }

	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	bool DoesDodgeAvoid() const { return bDodgeAvoids; }

	/**
	 *  Returns the distance on the floor from a point to the dangerous part of the hazard, and the way out of it.
	 *  Zero or less means the point is inside it. Whether the hazard is live does not matter here.
	 */
	virtual float GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const;

	/**
	 *  Returns true if the hazard lies across the straight line between two points on the floor, so that walking it
	 *  would mean crossing the dangerous part, and gives a point to walk to first to get round it.
	 *  Clearance is how far the walker wants to keep its center from the hazard.
	 *  A hazard that stands against a wall and cannot be crossed, like the spikes, never does.
	 */
	virtual bool GetWayRound(const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint) const;

	/**
	 *  Looks through every hazard in the world that is live or about to be for one that lies across the straight line
	 *  between two points, and gives a point to walk to first to get round it. Returns false if the line is open.
	 */
	static bool FindWayRound(const UObject* WorldContextObject, const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint);

	/** Returns true if the fighter's body is touching the dangerous part of the hazard. Whether the hazard is live does not matter here. */
	UFUNCTION(BlueprintPure, Category="Gut|Hazard")
	bool IsTouching(const AGutFighter* Fighter) const;

protected:

	/** Hurts the fighter if it is touching the live hazard and is not protected */
	void TryHurt(AGutFighter* Fighter, float Now);

	/** Called when the hazard changes state, to change how it looks */
	virtual void OnHazardStateChanged(EGutHazardState NewState);

	/** Called every tick, to animate the hazard in its current state */
	virtual void TickPresentation(float DeltaTime);

	/** Blueprint hook: the hazard changed state */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut|Hazard", meta = (DisplayName = "On Hazard State Changed"))
	void BP_OnHazardStateChanged(EGutHazardState NewState);

	/** Blueprint hook: the hazard hurt a fighter */
	UFUNCTION(BlueprintImplementableEvent, Category="Gut|Hazard", meta = (DisplayName = "On Fighter Hurt"))
	void BP_OnFighterHurt(AGutFighter* Fighter);
};
