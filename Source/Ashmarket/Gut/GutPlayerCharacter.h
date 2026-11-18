// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GutFighter.h"
#include "GutPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCameraShakeBase;
class UInputAction;
struct FInputActionValue;

/**
 *  The player's fighter: a Gut Fighter with a third person camera and input bindings.
 *  The camera orbits freely, or locks on to the opponent and keeps it in view while the fighter circles.
 */
UCLASS(abstract)
class AGutPlayerCharacter : public AGutFighter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Light Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LightAttackAction;

	/** Heavy Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HeavyAttackAction;

	/** Block Input Action. Held. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BlockAction;

	/** Dodge Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeAction;

	/** Lock On Input Action. Switches the lock-on camera on and off. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LockOnAction;

	/** If true the camera locks on to the opponent by itself when the bell rings */
	UPROPERTY(EditAnywhere, Category="Camera|Lock On")
	bool bLockOnAtBell = true;

	/** Camera pitch while locked on */
	UPROPERTY(EditAnywhere, Category="Camera|Lock On", meta = (ClampMin = -89, ClampMax = 89, Units = "deg"))
	float LockOnPitch = -12.0f;

	/** How quickly the locked camera swings round to follow the opponent. Higher is snappier. */
	UPROPERTY(EditAnywhere, Category="Camera|Lock On", meta = (ClampMin = 0.1, ClampMax = 100))
	float LockOnTurnSpeed = 8.0f;

	/** If true the camera is locked on to the opponent */
	bool bLockedOn = false;

	/** If true the fighter was allowed to fight last tick */
	bool bWasCombatEnabled = false;

	/** Camera shake played when one of the player's attacks does damage */
	UPROPERTY(EditAnywhere, Category="Gut|Presentation")
	TSubclassOf<UCameraShakeBase> HitLandedShake;

	/** Camera shake played when the player takes damage */
	UPROPERTY(EditAnywhere, Category="Gut|Presentation")
	TSubclassOf<UCameraShakeBase> HitReceivedShake;

	/** Camera boom length while the player is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float DeathCameraDistance = 450.0f;

	/** Camera pitch while the player is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = -89, ClampMax = 0, Units = "deg"))
	float DeathCameraPitch = -25.0f;

public:

	/** Constructor */
	AGutPlayerCharacter();

	/** Keeps the locked camera on the opponent */
	virtual void Tick(float DeltaTime) override;

	/** Locks the camera on to the opponent, or lets it go. While locked the fighter keeps facing the opponent and moves around it. */
	UFUNCTION(BlueprintCallable, Category="Camera")
	void SetLockedOn(bool bLock);

	UFUNCTION(BlueprintPure, Category="Camera")
	bool IsLockedOn() const { return bLockedOn; }

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for light attack input */
	void LightAttackPressed();

	/** Called for heavy attack input */
	void HeavyAttackPressed();

	/** Called for block input pressed */
	void BlockPressed();

	/** Called for block input released */
	void BlockReleased();

	/** Called for dodge input */
	void DodgePressed();

	/** Called for lock on input */
	void LockOnPressed();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles dodge inputs from either controls or UI interfaces. Dodges the way the player is steering. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDodge();

protected:

	// ~begin GutFighter interface

	virtual void HandleAttackLanded(AGutFighter* Target, EGutHitResult Result, const FGutAttackDef& Attack) override;

	virtual void HandleAttackReceived(AGutFighter* Attacker, EGutHitResult Result, const FGutAttackDef& Attack) override;

	virtual void HandleDeath(AGutFighter* Killer) override;

	// ~end GutFighter interface

	/** Plays a camera shake on the owning player */
	void PlayCameraShake(TSubclassOf<UCameraShakeBase> Shake);

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
