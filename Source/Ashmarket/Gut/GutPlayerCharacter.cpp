// Ashmarket prototype: the Gut.

#include "GutPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AGutPlayerCharacter::AGutPlayerCharacter()
{
	DisplayName = FText::FromString(TEXT("YOU"));

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;

	// look over the right shoulder, so the player's own body does not hide the opponent
	CameraBoom->SocketOffset = FVector(0.0f, 70.0f, 70.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// the player is never driven by an AI brain
	AutoPossessAI = EAutoPossessAI::Disabled;

	// set the player tag
	Tags.Add(FName("Player"));
}

void AGutPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// lock on by itself when the bell rings
	const bool bCombatEnabledNow = IsCombatEnabled();

	if (bCombatEnabledNow && !bWasCombatEnabled && bLockOnAtBell && IsPlayerControlled())
	{
		SetLockedOn(true);
	}

	bWasCombatEnabled = bCombatEnabledNow;

	if (!bLockedOn)
	{
		return;
	}

	const AGutFighter* Target = GetOpponent();

	// nothing left to look at, or nobody looking
	if (!IsAlive() || !Target || !Target->IsAlive() || !IsPlayerControlled())
	{
		SetLockedOn(false);
		return;
	}

	// swing the camera round so the opponent stays in front of it
	FRotator Desired = (Target->GetActorLocation() - GetActorLocation()).Rotation();
	Desired.Pitch = LockOnPitch;
	Desired.Roll = 0.0f;

	GetController()->SetControlRotation(FMath::RInterpTo(GetController()->GetControlRotation(), Desired, DeltaTime, LockOnTurnSpeed));
}

void AGutPlayerCharacter::SetLockedOn(bool bLock)
{
	const AGutFighter* Target = GetOpponent();

	bLockedOn = bLock && IsAlive() && Target && Target->IsAlive();

	// locked on, the fighter faces the opponent and the movement keys circle it
	SetSquareUp(bLockedOn);
}

void AGutPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGutPlayerCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGutPlayerCharacter::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGutPlayerCharacter::Look);

		// Attacks
		EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &AGutPlayerCharacter::LightAttackPressed);
		EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &AGutPlayerCharacter::HeavyAttackPressed);

		// Block
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &AGutPlayerCharacter::BlockPressed);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &AGutPlayerCharacter::BlockReleased);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Canceled, this, &AGutPlayerCharacter::BlockReleased);

		// Dodge
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AGutPlayerCharacter::DodgePressed);

		// Lock on
		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &AGutPlayerCharacter::LockOnPressed);
	}
	else
	{
		UE_LOG(LogGut, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}
}

void AGutPlayerCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AGutPlayerCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AGutPlayerCharacter::LightAttackPressed()
{
	TryLightAttack();
}

void AGutPlayerCharacter::HeavyAttackPressed()
{
	TryHeavyAttack();
}

void AGutPlayerCharacter::BlockPressed()
{
	SetBlocking(true);
}

void AGutPlayerCharacter::BlockReleased()
{
	SetBlocking(false);
}

void AGutPlayerCharacter::DodgePressed()
{
	DoDodge();
}

void AGutPlayerCharacter::LockOnPressed()
{
	SetLockedOn(!bLockedOn);
}

void AGutPlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AGutPlayerCharacter::DoLook(float Yaw, float Pitch)
{
	// the locked camera looks after itself
	if (GetController() != nullptr && !bLockedOn)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AGutPlayerCharacter::DoDodge()
{
	// dodge the way the stick is held. With no direction held this is zero, which steps straight back.
	TryDodge(GetLastMovementInputVector());
}

void AGutPlayerCharacter::HandleAttackLanded(AGutFighter* Target, EGutHitResult Result, const FGutAttackDef& Attack)
{
	Super::HandleAttackLanded(Target, Result, Attack);

	if (Result == EGutHitResult::Hit || Result == EGutHitResult::GuardBroken)
	{
		PlayCameraShake(HitLandedShake);
	}
}

void AGutPlayerCharacter::HandleAttackReceived(AGutFighter* Attacker, EGutHitResult Result, const FGutAttackDef& Attack)
{
	Super::HandleAttackReceived(Attacker, Result, Attack);

	if (Result == EGutHitResult::Hit || Result == EGutHitResult::GuardBroken)
	{
		PlayCameraShake(HitReceivedShake);
	}
}

void AGutPlayerCharacter::HandleDeath(AGutFighter* Killer)
{
	Super::HandleDeath(Killer);

	// pull the camera back and centre it on the body
	CameraBoom->TargetArmLength = DeathCameraDistance;
	CameraBoom->SocketOffset = FVector::ZeroVector;

	if (AController* PlayerController = GetController())
	{
		float Yaw = PlayerController->GetControlRotation().Yaw;

		// look at the body from the killer's side, so the killer stands over it and no wall is in the way
		if (Killer)
		{
			const FVector FromKiller = GetActorLocation() - Killer->GetActorLocation();

			if (!FromKiller.IsNearlyZero())
			{
				Yaw = FromKiller.Rotation().Yaw;
			}
		}

		PlayerController->SetControlRotation(FRotator(DeathCameraPitch, Yaw, 0.0f));
	}
}

void AGutPlayerCharacter::PlayCameraShake(TSubclassOf<UCameraShakeBase> Shake)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (PlayerController && Shake)
	{
		PlayerController->ClientStartCameraShake(Shake);
	}
}
