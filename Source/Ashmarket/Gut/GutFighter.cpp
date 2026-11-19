// Ashmarket prototype: the Gut.

#include "GutFighter.h"
#include "GutBeast.h"
#include "EngineUtils.h"
#include "GutOpponentController.h"
#include "GutAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GutArmorData.h"
#include "GutBrainController.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GutDroppedWeapon.h"
#include "GutGameMode.h"
#include "GutGore.h"
#include "GutSound.h"
#include "TimerManager.h"
#include "GutHazard.h"
#include "GutWeaponData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "RetargetComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogGut);

namespace GutFighterConstants
{
	/** Name shared by every push so a new one always replaces the last */
	const FName PushSourceName("GutPush");

	/** Time a knockback takes */
	const float KnockbackDuration = 0.15f;

	/** Gap a lunge leaves between the two capsules */
	const float LungeStopGap = 10.0f;

	/** -GutNoStrikeShift leaves every blow where its own clip and the old step put it, to compare */
	const bool bStrikesSeenToLand = !FParse::Param(FCommandLine::Get(), TEXT("GutNoStrikeShift"));

	/** How far in front of a body each clip's blow ends, as the last body to throw it found: where a body starts from */
	TMap<FName, float> StrikeReachKnownToAnybody;

	const FLinearColor DamageTextColor(1.0f, 0.25f, 0.2f);
	const FLinearColor BlockTextColor(0.6f, 0.8f, 1.0f);
	const FLinearColor DodgeTextColor(0.7f, 1.0f, 0.7f);
	const FLinearColor BreakTextColor(1.0f, 0.75f, 0.1f);

	/** Edge length of the engine's basic cube, which armor plates are made from */
	const float CubeSize = 100.0f;

	/** Radius of the smallest splatter of blood */
	const float MinBloodRadius = 9.0f;

	/** How far from the fighter a splatter can land */
	const float BloodThrowDistance = 110.0f;

	/** Blood let by a limb being crippled, by one being cut off, and by a fighter dying, on the scale of an attack's damage */
	const float CrippleBlood = 30.0f;
	const float SeverBlood = 60.0f;

	/** Speed a severed limb leaves the body with */
	const float GibSpeed = 380.0f;

	/** Bone a severed head comes off at, which gets a round stand-in and not a long one */
	const FName HeadBone("head");

	/** Speed a dropped weapon leaves the hand with */
	const float WeaponDropSpeed = 150.0f;

	/** Names of the limbs as they are written over a fighter's head, indexed by EGutLimb */
	const TCHAR* LimbNames[GutNumLimbs] = { TEXT("LEFT ARM"), TEXT("RIGHT ARM"), TEXT("LEFT LEG"), TEXT("RIGHT LEG") };
}

AGutFighter::AGutFighter()
{
	PrimaryActorTick.bCanEverTick = true;

	DisplayName = FText::FromString(TEXT("FIGHTER"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// fighters must never push the camera around or stand on each other
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Don't rotate when the controller rotates. Facing is set by movement and by the fight.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;

	// create the guard plate
	GuardPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuardPlate"));
	GuardPlate->SetupAttachment(RootComponent);
	GuardPlate->SetRelativeLocation(FVector(48.0f, 0.0f, 25.0f));
	GuardPlate->SetRelativeScale3D(FVector(0.06f, 0.55f, 0.7f));
	GuardPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuardPlate->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GuardPlate->SetStaticMesh(CubeMesh.Object);
	}

	// placed fighters without a player get an AI brain
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// at ease the arms swing with the walk: they are not held where a stance has them
	AtEaseStance.bKeepUpperBodyWhileMoving = false;

	// create the weapon shape. What it looks like comes from the weapon in hand.
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetReceivesDecals(false);
	WeaponMesh->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetVisibility(false);

	// create the collar. Its shape comes from the Blueprint.
	Collar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Collar"));
	Collar->SetupAttachment(GetMesh(), CollarBone);
	Collar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collar->SetVisibility(false);

	// create the body the player sees. Which body, if any, comes from the Blueprint.
	BodyRetarget = CreateDefaultSubobject<URetargetComponent>(TEXT("BodyRetarget"));
	BodyRetarget->bForceOtherMeshesToFollowControlledMesh = false;

	// Note: attacks and finishers are not set here. They live on weapon assets, bare hands included.
}

void AGutFighter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	Stamina = MaxStamina;

	MeshRestLocation = GetMesh()->GetRelativeLocation();

	// for trying rules without touching the Blueprints: -GutDodge=380 sets how far a dodge goes, in cm,
	// and -GutMove=2048,8,2000 the push-off, the grip and the braking
	FParse::Value(FCommandLine::Get(), TEXT("GutDodge="), DodgeDistance);

	// -GutWounds and -GutNoWounds switch the blood on bodies on and off whatever the fighter says, to compare
	bShowWounds = (bShowWounds || FParse::Param(FCommandLine::Get(), TEXT("GutWounds"))) && !FParse::Param(FCommandLine::Get(), TEXT("GutNoWounds"));

	FString Moves;

	if (FParse::Value(FCommandLine::Get(), TEXT("GutMove="), Moves, false))
	{
		TArray<FString> Parts;
		Moves.ParseIntoArray(Parts, TEXT(","));

		if (Parts.Num() == 3)
		{
			MoveAcceleration = FCString::Atof(*Parts[0]);
			MoveGrip = FCString::Atof(*Parts[1]);
			MoveBraking = FCString::Atof(*Parts[2]);
		}
	}

	// set here and not in the constructor, so that a Blueprint that once saved the engine's own values does not keep them
	GetCharacterMovement()->MaxAcceleration = MoveAcceleration;
	GetCharacterMovement()->GroundFriction = MoveGrip;
	GetCharacterMovement()->BrakingDecelerationWalking = MoveBraking;

	for (float& Limb : LimbHealth)
	{
		Limb = MaxLimbHealth;
	}

	if (!Fists)
	{
		UE_LOG(LogGut, Warning, TEXT("%s has no Fists set, so it cannot attack without a weapon"), *GetName());
	}

	EquipWeapon(StartingWeapon);

	// until it is told otherwise, a fighter in the cage is somebody's property
	Collar->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CollarBone);
	Collar->SetRelativeTransform(CollarTransform);
	Collar->SetStaticMesh(CollarMesh);
	Collar->SetMaterial(0, CollarMaterial);

	SetCollared(true);

	if (SpawnAsBeast)
	{
		BecomeBeast(SpawnAsBeast);
	}
	else
	{
		SetBody(BodyClass, BodyRetargeter);

		for (UGutArmorData* Piece : StartingArmor)
		{
			WearArmor(Piece);
		}
	}

	// the placeholder animations carry root motion, but movement in the cage is driven by the fighter
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}

	// tint the body so the two fighters are easy to tell apart
	if (bApplyBodyTint)
	{
		SetBodyTint(BodyTint);
	}

	SetState(EGutFighterState::Idle);
}

void AGutFighter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// the character spawned to be seen goes when the fighter goes
	if (BodyActor)
	{
		BodyActor->Destroy();
		BodyActor = nullptr;
		Body = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AGutFighter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LastTickFrame = GFrameCounter;

	if (Beast)
	{
		UpdateBeast(DeltaTime);
	}

	// with a body of its own the chain is moved when that body has been posed, which is later in the frame than this
	if (!HasBody())
	{
		UpdateChain();
		ListenToFeet();
	}

	UpdateStrikeShift(DeltaTime);
	Bleed(DeltaTime);

	if (State == EGutFighterState::Dead)
	{
		return;
	}

	StateTime += DeltaTime;

	const float Now = GetWorld()->GetTimeSeconds();

	// somebody showing the crowd it won stops when it is given something else to do
	if (bCelebrating && (State != EGutFighterState::Idle || GetVelocity().SizeSquared2D() > FMath::Square(60.0f)))
	{
		bCelebrating = false;
		bSparing = false;

		if (State == EGutFighterState::Idle)
		{
			StopSlotAnimation(0.25f);
		}
	}

	switch (State)
	{
	case EGutFighterState::Windup:

		// keep the attack pointed at the opponent until it is thrown
		if (bTrackOpponent && HasLiveOpponent())
		{
			TurnToward(Opponent->GetActorLocation(), DeltaTime, TrackingTurnRate);
		}

		// the swing is heard a moment before it lands
		if (!bSwingHeard && StateTime >= CurrentAttack.Windup - 0.1f)
		{
			bSwingHeard = true;

			MakeSound(WeaponSound(TEXT("swing"), GetArmament()), CurrentAttackType == EGutAttackType::Heavy ? 1.0f : 0.75f, nullptr, CurrentAttackType == EGutAttackType::Heavy ? 0.9f : 1.05f);
		}

		if (StateTime >= CurrentAttack.Windup)
		{
			SetState(EGutFighterState::Active);
			PerformAttackSweep();
		}
		break;

	case EGutFighterState::Active:

		PerformAttackSweep();

		if (StateTime >= CurrentAttack.Active)
		{
			SetState(EGutFighterState::Recovery);
		}
		break;

	case EGutFighterState::Recovery:

		if (StateTime >= CurrentAttack.Recovery)
		{
			FinishAttack();
		}
		break;

	case EGutFighterState::Dodging:

		if (StateTime >= DodgeDuration)
		{
			LastDodgeEndTime = Now;
			StopSlotAnimation(0.15f);
			SetState(EGutFighterState::Idle);
		}
		break;

	case EGutFighterState::Stunned:

		if (StateTime >= StunDuration)
		{
			StopSlotAnimation();
			SetState(EGutFighterState::Idle);
		}
		break;

	case EGutFighterState::Staggered:

		// beaten and let live: it stays down, with no second wind to get up on
		if (bYielded)
		{
			break;
		}

		// let go of by whoever was finishing it: back to swaying
		if (bBeingTaken && !Finisher.IsValid())
		{
			bBeingTaken = false;

			if (const FGutStanceAnimations Clips = GetStanceAnimations(); Clips.Stagger)
			{
				PlaySlotAnimation(Clips.Stagger, 1.0f, 0.0f, 0.2f, 0.3f, 100);
			}
		}

		// the clock stops while somebody is in the middle of finishing this fighter
		if (!Finisher.IsValid() && StateTime >= CurrentStaggerDuration)
		{
			RecoverFromStagger();
		}
		else if (bBeingTaken)
		{
			// what is done to it is done from in front
			TurnToward(Finisher->GetActorLocation(), DeltaTime, TrackingTurnRate * 0.5f);
		}
		break;

	case EGutFighterState::Finishing:
	{
		AGutFighter* Target = FinisherTarget.Get();

		// still walking in: the windup begins when the blow can reach
		if (bFinisherClosing)
		{
			if (!Target || !Target->IsStaggered())
			{
				// whatever it was going to finish is gone
				bFinisherClosing = false;

				InterruptAction();
				SetState(EGutFighterState::Idle);
				break;
			}

			TurnToward(Target->GetActorLocation(), DeltaTime, TrackingTurnRate);

			// nobody steers this but the fighter. If something is in the way it does it from where it has got to.
			ConsumeMovementInputVector();

			if (GetGapTo(Target) <= CurrentFinisher.Reach || StateTime > FinisherClosingLimit)
			{
				bFinisherClosing = false;
				StateTime = 0.0f;

				UpdateMovementForState();
				BeginFinisherWindup(Target);
			}
			else
			{
				AddMovementInput((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.0f);
			}

			break;
		}

		if (Target && !bFinisherLanded)
		{
			TurnToward(Target->GetActorLocation(), DeltaTime, TrackingTurnRate);
		}

		// the killing blow
		if (!bFinisherLanded && StateTime >= CurrentFinisher.Windup)
		{
			bFinisherLanded = true;

			if (Target && Target->IsStaggered())
			{
				// let whoever is watching know how it was done
				Target->bKilledByFinisher = true;
				Target->KillingFinisherStyle = CurrentFinisherStyle;

				const FVector Away = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

				Target->Kill(this, Away);

				// some finishers do not leave the body whole
				for (const FName& Bone : CurrentFinisher.SeverBones)
				{
					Target->SeverLimb(Bone, Away);
				}
			}
		}

		if (StateTime >= CurrentFinisher.Windup + CurrentFinisher.Recovery)
		{
			FinisherTarget = nullptr;

			StopSlotAnimation();
			SetState(EGutFighterState::Idle);
		}
		break;
	}

	case EGutFighterState::Blocking:

		if (!bWantsBlock || !bCombatEnabled)
		{
			SetState(EGutFighterState::Idle);
		}
		else if (HasLiveOpponent())
		{
			// the guard follows the opponent
			TurnToward(Opponent->GetActorLocation(), DeltaTime, TrackingTurnRate);
		}
		break;

	case EGutFighterState::Idle:

		if (bSquareUp && HasLiveOpponent())
		{
			TurnToward(Opponent->GetActorLocation(), DeltaTime, TrackingTurnRate);
		}
		break;

	default:
		break;
	}

	// a fighter that has just become free acts on what was pressed while it was busy
	if (State == EGutFighterState::Idle)
	{
		ConsumeBufferedAction();
	}

	if (State == EGutFighterState::Idle && bWantsBlock && bCombatEnabled)
	{
		SetState(EGutFighterState::Blocking);
	}

	// stamina comes back after a short rest, and slowly behind the guard
	const bool bExerting = IsAttacking() || State == EGutFighterState::Dodging;

	if (!bExerting && Stamina < MaxStamina && Now - LastStaminaUseTime >= StaminaRegenDelay)
	{
		const float RegenScale = (State == EGutFighterState::Blocking) ? BlockingRegenScale : 1.0f;

		Stamina = FMath::Min(MaxStamina, Stamina + (StaminaRegenRate * RegenScale * StaminaRegenScale * DeltaTime));
	}
}

bool AGutFighter::TryLightAttack()
{
	if (!IsAlive() || !bCombatEnabled)
	{
		return false;
	}

	// busy? remember the press
	if (State != EGutFighterState::Idle && State != EGutFighterState::Blocking)
	{
		BufferAction(EBufferedAction::Light);
		return false;
	}

	// a staggered opponent in reach is finished, not hit
	if (TryFinisher(EGutFinisherStyle::Clean))
	{
		return true;
	}

	const FGutAttackDef* Attack = GetNextLightAttack();

	if (!Attack)
	{
		return false;
	}

	const int32 NextIndex = GetNextComboIndex();

	if (StartAttack(*Attack, EGutAttackType::Light))
	{
		ComboIndex = NextIndex;
		return true;
	}

	return false;
}

bool AGutFighter::TryHeavyAttack()
{
	if (!IsAlive() || !bCombatEnabled)
	{
		return false;
	}

	// busy? remember the press
	if (State != EGutFighterState::Idle && State != EGutFighterState::Blocking)
	{
		BufferAction(EBufferedAction::Heavy);
		return false;
	}

	// a beaten opponent with the spikes or the cable behind it is knocked onto them, when it is the player's hand that
	// throws the blow: the cage's own decide that for themselves (AGutOpponentController::TickCageFinish)
	if (IsPlayerControlled() && FindDriveHazard(Opponent.Get()) && TryDrive())
	{
		return true;
	}

	// a staggered opponent in reach is finished, not hit
	if (TryFinisher(EGutFinisherStyle::Slow))
	{
		return true;
	}

	if (!GetArmament())
	{
		return false;
	}

	if (StartAttack(GetHeavyAttack(), EGutAttackType::Heavy))
	{
		// a heavy attack ends the light chain
		LastLightAttackEndTime = -100.0f;
		return true;
	}

	return false;
}

bool AGutFighter::TryDrive()
{
	if (!IsAlive() || !bCombatEnabled || !GetArmament() || (State != EGutFighterState::Idle && State != EGutFighterState::Blocking))
	{
		return false;
	}

	if (!StartAttack(GetHeavyAttack(), EGutAttackType::Heavy))
	{
		return false;
	}

	LastLightAttackEndTime = -100.0f;
	bDriving = true;

	return true;
}

void AGutFighter::SetBlocking(bool bBlock)
{
	// an animal has no guard to put up
	bWantsBlock = bBlock && !Beast;
	bBlock = bWantsBlock;

	// a free fighter's guard goes up or down at once. A busy one's follows in Tick, as soon as it is free.
	if (bBlock && State == EGutFighterState::Idle && bCombatEnabled)
	{
		SetState(EGutFighterState::Blocking);
	}
	else if (!bBlock && State == EGutFighterState::Blocking)
	{
		SetState(EGutFighterState::Idle);
	}
}

bool AGutFighter::TryDodge(FVector WorldDirection)
{
	if (!IsAlive() || !bCombatEnabled)
	{
		return false;
	}

	// busy? remember the press
	if (State != EGutFighterState::Idle && State != EGutFighterState::Blocking)
	{
		BufferAction(EBufferedAction::Dodge, WorldDirection);
		return false;
	}

	return StartDodge(WorldDirection);
}

AGutHazard* AGutFighter::FindDriveHazard(const AGutFighter* Target) const
{
	if (!Target || !Target->IsStaggered() || Target->IsBeingFinished() || Beast || Target->Beast || !GetArmament())
	{
		return nullptr;
	}

	// near enough for the blow to land
	if (GetGapTo(Target) > FMath::Min(GetFinisherRange(), GetHeavyAttack().Reach))
	{
		return nullptr;
	}

	const AGutGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AGutGameMode>() : nullptr;

	if (!GameMode)
	{
		return nullptr;
	}

	// the blow sends it straight on from whoever throws it. Its body touches a hazard when it comes within its own width
	// of it, anywhere along the way it is sent.
	const FVector There = Target->GetActorLocation();
	const FVector Push = (There - GetActorLocation()).GetSafeNormal2D();
	const float Radius = Target->GetCapsuleComponent()->GetScaledCapsuleRadius();

	static const int32 Looks = 8;

	for (AGutHazard* Hazard : GameMode->GetHazards())
	{
		if (!Hazard || !Hazard->IsLive())
		{
			continue;
		}

		for (int32 i = 1; i <= Looks; ++i)
		{
			FVector Away;

			if (Hazard->GetDangerDistance(There + (Push * (Target->GetDriveDistance() * 0.9f * i / Looks)), Away) <= Radius)
			{
				return Hazard;
			}
		}
	}

	return nullptr;
}

bool AGutFighter::TryFinisher(EGutFinisherStyle Style)
{
	AGutFighter* Target = Opponent.Get();

	if (!CanFinish(Target))
	{
		return false;
	}

	StartFinisher(Target, Style);

	return true;
}

bool AGutFighter::EquipWeapon(UGutWeaponData* NewWeapon)
{
	// a crippled arm cannot hold anything, and an animal has no hand to hold it in
	if (NewWeapon && (Beast || IsLimbCrippled(GetArmFor(NewWeapon))))
	{
		return false;
	}

	if (NewWeapon && NewWeapon != Weapon)
	{
		MakeSound(WeaponSound(TEXT("equip"), NewWeapon), 0.8f);
	}

	Weapon = NewWeapon;

	// a different weapon is a different chain
	ComboIndex = 0;
	LastLightAttackEndTime = -100.0f;

	UpdateWeaponMesh();

	return true;
}

void AGutFighter::WearArmor(UGutArmorData* Piece)
{
	if (!Piece || Beast)
	{
		return;
	}

	// one piece to a part of the body
	Armor.RemoveAll([Piece](const UGutArmorData* Worn) { return !Worn || Worn->Slot == Piece->Slot; });
	Armor.Add(Piece);

	UpdateArmorPlates();
}

void AGutFighter::ClearArmor()
{
	Armor.Reset();

	UpdateArmorPlates();
}

UGutArmorData* AGutFighter::GetArmorOn(EGutArmorSlot Slot) const
{
	for (UGutArmorData* Piece : Armor)
	{
		if (Piece && Piece->Slot == Slot)
		{
			return Piece;
		}
	}

	return nullptr;
}

void AGutFighter::UpdateArmorPlates()
{
	for (UStaticMeshComponent* Plate : ArmorPlates)
	{
		if (Plate)
		{
			Plate->DestroyComponent();
		}
	}

	ArmorPlates.Reset();

	for (const UGutArmorData* Piece : Armor)
	{
		if (!Piece || !Piece->PlateMesh)
		{
			continue;
		}

		for (const FGutArmorPlate& PlateDef : Piece->Plates)
		{
			UStaticMeshComponent* Plate = NewObject<UStaticMeshComponent>(this);

			Plate->SetStaticMesh(Piece->PlateMesh);
			Plate->SetMaterial(0, Piece->PlateMaterial);
			Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Plate->SetupAttachment(GetShownMesh(), PlateDef.Bone);

			// lay the plate along the line from its bone to the next joint, centered between the two
			const FTransform BoneTransform = GetShownMesh()->GetSocketTransform(PlateDef.Bone, RTS_Component);
			const FVector Toward = GetShownMesh()->GetSocketTransform(PlateDef.TowardBone, RTS_Component).GetRelativeTransform(BoneTransform).GetLocation();

			const float Length = FMath::Max(Toward.Size() * PlateDef.LengthShare, 1.0f);

			if (Piece->bPlateIsModel && !Toward.IsNearlyZero())
			{
				// a real tube round the limb: its length down the bone, its front the way the fighter faces, at its own shape
				Plate->EmptyOverrideMaterials();

				const FBox Bounds = Piece->PlateMesh->GetBoundingBox();
				const FVector Size = Bounds.GetSize().ComponentMax(FVector(1.0f));
				const FVector Front = BoneTransform.GetRotation().UnrotateVector(FVector::YAxisVector);

				const float Width = PlateDef.Thickness * Piece->ModelWidthScale;
				const FVector Scale(Length / Size.X, Width / Size.Y, (Width * Piece->ModelDepthShare) / Size.Z);
				const FQuat Turn = FRotationMatrix::MakeFromXZ(Toward, Front).ToQuat();

				Plate->SetRelativeTransform(FTransform(Turn, (Toward * 0.5f) - Turn.RotateVector(Bounds.GetCenter() * Scale), Scale));
			}
			else
			{
				Plate->SetRelativeTransform(FTransform(
					Toward.IsNearlyZero() ? FQuat::Identity : FRotationMatrix::MakeFromX(Toward).ToQuat(),
					Toward * 0.5f,
					FVector(Length, PlateDef.Thickness, PlateDef.Thickness) / GutFighterConstants::CubeSize));
			}

			Plate->RegisterComponent();

			ArmorPlates.Add(Plate);
		}
	}
}

void AGutFighter::DropWeapon()
{
	if (!Weapon)
	{
		return;
	}

	UGutWeaponData* DroppedWeapon = Weapon;
	AGutDroppedWeapon* Prop = nullptr;

	// leave the weapon on the floor where the hand was
	if (DroppedWeapon->Mesh)
	{
		const FTransform HandTransform = WeaponMesh->GetComponentTransform();

		Prop = GetWorld()->SpawnActor<AGutDroppedWeapon>(HandTransform.GetLocation(), HandTransform.Rotator());

		if (Prop)
		{
			const FVector Velocity = GetVelocity() + ((GetActorRightVector() + FVector::UpVector) * GutFighterConstants::WeaponDropSpeed);

			Prop->Drop(DroppedWeapon, HandTransform.GetScale3D(), Velocity);
		}
	}

	EquipWeapon(nullptr);

	// it is heard when it has had time to reach the floor
	const FName Clatter = WeaponSound(TEXT("drop"), DroppedWeapon);
	const FVector Where = GetActorLocation() + (GetActorRightVector() * 60.0f) - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	GetWorldTimerManager().SetTimer(WeaponFallTimer, FTimerDelegate::CreateWeakLambda(this, [this, Clatter, Where]()
	{
		MakeSound(Clatter, 1.0f, &Where);
	}), 0.42f, false);

	UE_LOG(LogGut, Log, TEXT("%s drops its weapon"), *GetName());

	BP_OnWeaponDropped(DroppedWeapon, Prop);
}

EGutHitResult AGutFighter::ReceiveAttack(AGutFighter* Attacker, const FGutAttackDef& Attack)
{
	// the dead and the fighters outside a running fight cannot be hurt
	if (!IsAlive() || !bCombatEnabled)
	{
		return EGutHitResult::Ignored;
	}

	// Nor can a staggered fighter: it is already beaten, and only a finisher ends it. But nobody is holding it up. A blow
	// thrown to drive it sends it reeling the way the blow was going, and if that is onto the spikes or the cable, the
	// cage ends it there.
	if (IsStaggered())
	{
		if (!Attacker || !Attacker->bDriving || IsBeingFinished())
		{
			return EGutHitResult::Ignored;
		}

		FVector Driven = (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();

		if (Driven.IsNearlyZero())
		{
			Driven = Attacker->GetActorForwardVector();
		}

		++TimesDriven;

		// whoever sends it there is who the cage's kill is put down to
		LastAttacker = Attacker;
		LastAttackedTime = GetWorld()->GetTimeSeconds();
		LastHitResult = EGutHitResult::Hit;

		MakeSound(VoiceSound(TEXT("pain")), 0.9f);
		SpillBlood(Driven, 6.0f);
		ApplyPush(Driven, DriveDistance, DriveDuration);

		UE_LOG(LogGut, Log, TEXT("%s -> %s: driven %.0f cm while staggered"), *Attacker->GetName(), *GetName(), DriveDistance);

		return EGutHitResult::Hit;
	}

	// let an AI brain make a reaction that has come due, even if its own turn this frame is still to come
	if (AGutBrainController* Brain = Cast<AGutBrainController>(GetController()))
	{
		Brain->NotifyIncomingAttack(this, Attacker);
	}

	// two blows that land in the same instant both land. Without this the fighter that happens to take its turn first
	// each frame would win every such exchange, by stunning the other before its swing was counted.
	if (HasAttackDueThisFrame())
	{
		PerformAttackSweep();
	}

	// which way is the blow travelling?
	FVector FromAttacker = Attacker ? (GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;

	if (FromAttacker.IsNearlyZero())
	{
		FromAttacker = -GetActorForwardVector();
	}

	EGutHitResult Result = EGutHitResult::Hit;

	// what the blow does to the fighter once it has been judged
	bool bBeaten = false;
	float Stun = 0.0f;
	float Knockback = 0.0f;
	UAnimSequenceBase* Reaction = nullptr;

	const float BlockCost = GetBlockCost(Attack);

	// the guard only stops what comes from the front
	const float GuardDot = FVector::DotProduct(GetActorForwardVector(), -FromAttacker);
	const bool bGuardCovers = State == EGutFighterState::Blocking && GuardDot >= FMath::Cos(FMath::DegreesToRadians(BlockHalfAngle));

	if (IsInvulnerable())
	{
		Result = EGutHitResult::Dodged;

		ShowCombatText(TEXT("DODGED"), GutFighterConstants::DodgeTextColor);
	}
	else if (bGuardCovers && Stamina >= BlockCost)
	{
		Result = EGutHitResult::Blocked;

		SpendStamina(BlockCost);

		Knockback = BlockedKnockback;

		ShowCombatText(TEXT("BLOCKED"), GutFighterConstants::BlockTextColor);

		// the guard holds, and the one behind it is jolted
		if (const FGutStanceAnimations Clips = GetStanceAnimations(); Clips.BlockHit)
		{
			PlaySlotAnimation(Clips.BlockHit, 1.3f, 0.0f, 0.05f, 0.2f);
		}
	}
	else
	{
		// a blow is as heavy as the one who throws it
		float Damage = Attack.Damage * (Attacker ? Attacker->DamageScale : 1.0f);

		Stun = Attack.StunTime;
		Knockback = Attack.Knockback;

		if (bGuardCovers)
		{
			// not enough stamina left to hold the guard: it breaks
			Result = EGutHitResult::GuardBroken;

			Damage *= GuardBreakDamageScale;
			Stun = GuardBreakStunTime;

			Stamina = 0.0f;
			LastStaminaUseTime = GetWorld()->GetTimeSeconds();
		}

		// where the blow lands decides which piece of armor, if any, is in its way
		EGutLimb Limb = EGutLimb::LeftArm;

		const bool bOnLimb = ResolveLimb(Attack, FromAttacker, Limb);
		const bool bOnArm = bOnLimb && (Limb == EGutLimb::LeftArm || Limb == EGutLimb::RightArm);

		const UGutArmorData* Piece = GetArmorOn(!bOnLimb ? EGutArmorSlot::Torso : (bOnArm ? EGutArmorSlot::Arms : EGutArmorSlot::Legs));

		// and which way the fighter reels
		Reaction = ChooseReaction(Attack, FromAttacker, bOnLimb, Limb);

		if (Piece)
		{
			Damage *= Piece->GetDamageScale(Attack.DamageType);
		}

		// say what it cost, now that it is known
		ShowCombatText(Result == EGutHitResult::GuardBroken
			? FString::Printf(TEXT("GUARD BROKEN  -%d"), FMath::RoundToInt32(Damage))
			: FString::Printf(TEXT("-%d"), FMath::RoundToInt32(Damage)),
			Result == EGutHitResult::GuardBroken ? GutFighterConstants::BreakTextColor : GutFighterConstants::DamageTextColor);

		bBeaten = ApplyHealthDamage(Damage);

		SpillBlood(FromAttacker, Damage);
		AddWound(Attack, FromAttacker, bOnLimb, Limb, Damage);

		// an edge, or anything heavy, marks the body in more than one place
		if (Attack.DamageType == EGutDamageType::Bladed || Damage >= 16.0f)
		{
			AddWound(Attack, FromAttacker, bOnLimb, Limb, Damage * 0.7f);
		}

		// the further gone, the more of the body is covered, and some of it is on whoever did it
		Soak();

		if (Attacker && !Attacker->IsBeast())
		{
			Attacker->Bespatter(-FromAttacker, Damage);
		}

		// a blow that lands on a limb wears that limb down as well
		if (bOnLimb)
		{
			ApplyLimbDamage(Limb, Damage * Attack.LimbDamageScale);
		}
	}

	LastHitResult = Result;

	// remember who is driving this fighter around the cage, in case the cage finishes what they started
	if (Attacker && Result != EGutHitResult::Dodged)
	{
		LastAttacker = Attacker;
		LastAttackedTime = GetWorld()->GetTimeSeconds();
	}

	UE_LOG(LogGut, Log, TEXT("%s -> %s: %s %s (health %.0f, stamina %.0f)"),
		*GetNameSafe(Attacker), *GetName(), *Attack.Label.ToString(), *UEnum::GetDisplayValueAsText(Result).ToString(), Health, Stamina);

	// a fighter with no health left is beaten, not dead: it staggers until somebody finishes it.
	// An animal does not stand there waiting to be finished: the blow that beats it kills it.
	if (bBeaten && Beast)
	{
		Die(Attacker, FromAttacker);
	}
	else if (bBeaten)
	{
		EnterStagger();
	}
	else if (Stun > 0.0f)
	{
		EnterStun(Stun, Reaction);
	}

	// only a blow that connects moves the fighter. A dodged one must leave the dodge's own movement alone.
	if (Knockback > 0.0f)
	{
		ApplyPush(FromAttacker, Knockback, GutFighterConstants::KnockbackDuration);
	}

	HandleAttackReceived(Attacker, Result, Attack);

	return Result;
}

void AGutFighter::SpillBlood(FVector Direction, float Amount)
{
	UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>();

	if (!Gore || !BloodMaterial || Amount <= 0.0f)
	{
		return;
	}

	FVector Thrown = Direction.GetSafeNormal2D();

	if (Thrown.IsNearlyZero())
	{
		Thrown = -GetActorForwardVector();
	}

	++TimesBloodDrawn;

	MakeSound(FName("gore_splat"), FMath::Clamp(Amount / 25.0f, 0.4f, 1.2f));

	// one splatter the size of the blow, the way it was travelling, and a few small ones round it
	const float Radius = GutFighterConstants::MinBloodRadius + (Amount * BloodRadiusPerDamage);

	UMaterialInterface* Splash = BloodSplashMaterial ? BloodSplashMaterial.Get() : BloodMaterial.Get();

	Gore->Splatter(Gore->GetMark(EGutBloodMark::Thrown, Splash), GetActorLocation() + (Thrown * FMath::FRandRange(0.3f, 1.0f) * GutFighterConstants::BloodThrowDistance), Radius * 1.3f);

	const int32 Drops = FMath::Clamp(FMath::RoundToInt32(Amount / 4.0f), 4, 12);

	for (int32 i = 0; i < Drops; ++i)
	{
		const FVector Spread = Thrown.RotateAngleAxis(FMath::FRandRange(-50.0f, 50.0f), FVector::UpVector) * FMath::FRandRange(0.4f, 1.6f) * GutFighterConstants::BloodThrowDistance;

		Gore->Splatter(Gore->GetMark(i % 2 == 0 ? EGutBloodMark::Thrown : EGutBloodMark::Dripped, Splash), GetActorLocation() + Spread, Radius * FMath::FRandRange(0.3f, 0.75f));
	}

	// and into the air, from about where it was hit: each drop flies the way the blow was going and marks the floor where it falls
	const int32 InTheAir = FMath::Clamp(FMath::RoundToInt32(Amount * BloodDropsPerDamage), 6, 40);

	Gore->Spray(Splash, GetActorLocation() + FVector(0.0f, 0.0f, FMath::FRandRange(15.0f, 55.0f)) - (Thrown * 12.0f), Thrown, InTheAir, 260.0f + (Amount * 9.0f));
}

void AGutFighter::AddWound(const FGutAttackDef& Attack, const FVector& FromAttacker, bool bOnLimb, EGutLimb Limb, float Damage)
{
	USkeletalMeshComponent* Skin = GetShownMesh();
	UMaterialInterface* Blood = BloodSplashMaterial ? BloodSplashMaterial.Get() : BloodMaterial.Get();

	// an animal's coat is not skin, and a body that takes no decals cannot show one
	if (!bShowWounds || !Skin || !Blood || Beast || !BodyActor || MaxWounds <= 0 || Damage <= 0.0f)
	{
		return;
	}

	// the stretch of the body the blow landed on: from one joint to the next, and how thick the body is there
	FName From = FName("spine_03");
	FName To = FName("spine_04");
	float Thick = 13.0f;

	if (bOnLimb)
	{
		const bool bLeft = Limb == EGutLimb::LeftArm || Limb == EGutLimb::LeftLeg;
		const bool bArm = Limb == EGutLimb::LeftArm || Limb == EGutLimb::RightArm;
		const bool bUpper = FMath::RandBool();

		From = FName(*FString::Printf(TEXT("%s_%s"), bArm ? (bUpper ? TEXT("upperarm") : TEXT("lowerarm")) : (bUpper ? TEXT("thigh") : TEXT("calf")), bLeft ? TEXT("l") : TEXT("r")));
		To = FName(*FString::Printf(TEXT("%s_%s"), bArm ? (bUpper ? TEXT("lowerarm") : TEXT("hand")) : (bUpper ? TEXT("calf") : TEXT("foot")), bLeft ? TEXT("l") : TEXT("r")));
		Thick = bArm ? 4.0f : (bUpper ? 7.0f : 5.0f);
	}
	else if (Attack.BlowFrom == EGutBlowFrom::Body)
	{
		From = FName("spine_01");
		To = FName("spine_03");
		Thick = 12.0f;
	}
	else if (Attack.BlowFrom == EGutBlowFrom::Above || FMath::RandBool())
	{
		// a blow at the head: the skull above the joint it turns on
		From = FName("head");
		To = NAME_None;
		Thick = 8.0f;
	}

	if (Skin->GetBoneIndex(From) == INDEX_NONE)
	{
		return;
	}

	FVector Where = Skin->GetBoneLocation(From);

	if (To.IsNone())
	{
		Where += Skin->GetBoneQuaternion(From).GetAxisX() * 9.0f;
	}
	else if (Skin->GetBoneIndex(To) != INDEX_NONE)
	{
		Where = FMath::Lerp(Where, Skin->GetBoneLocation(To), FMath::FRandRange(0.25f, 0.75f));
	}

	// on the side the blow came in from, and only that side: the box the blood is laid in stops at the middle of the body
	FVector Into = FromAttacker.GetSafeNormal2D();

	if (Into.IsNearlyZero())
	{
		Into = -GetActorForwardVector();
	}

	Into = Into.RotateAngleAxis(FMath::FRandRange(-25.0f, 25.0f), FVector::UpVector);

	const FVector Surface = Where - (Into * Thick * 0.9f);

	// as big as the blow was
	const float Size = FMath::Clamp(7.0f + (Damage * 0.45f), 8.0f, bOnLimb ? 12.0f : 22.0f);
	const bool bEdge = Attack.DamageType == EGutDamageType::Bladed;

	UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>();

	if (!Gore)
	{
		return;
	}

	// an edge leaves a line with what runs out of it, and anything else a wet patch
	if (bEdge)
	{
		MarkSkin(Gore->GetMark(EGutBloodMark::Gash, Blood), From, Surface, Into, Thick * 0.85f, Size * 0.6f, Size * 0.9f, FMath::FRandRange(-30.0f, 30.0f));
	}
	else
	{
		MarkSkin(Gore->GetMark(EGutBloodMark::Wound, Blood), From, Surface, Into, Thick * 0.85f, Size * 0.7f, Size * 0.7f, FMath::FRandRange(0.0f, 360.0f));
	}

	// what the blow threw off it lies round it, finer and wider
	MarkSkin(Gore->GetMark(EGutBloodMark::Thrown, Blood), From, Surface, Into, Thick * 0.85f, Size * 1.3f, Size * 1.3f, FMath::FRandRange(0.0f, 360.0f));

	// a body that is opened runs: under a wound on the trunk or the head that was worth the name, blood has run down from it
	if (!bOnLimb && (bEdge || Damage >= 12.0f))
	{
		const float Long = Size * FMath::FRandRange(1.3f, 2.0f);

		MarkSkin(Gore->GetMark(EGutBloodMark::Ran, nullptr), From, Surface - FVector(0.0f, 0.0f, Long * 0.4f), Into, Thick * 0.85f, Long * 0.5f, Long * 0.38f, FMath::FRandRange(-8.0f, 8.0f));
	}
}

UDecalComponent* AGutFighter::MarkSkin(UMaterialInterface* Picture, FName Bone, const FVector& Where, const FVector& Into, float Deep, float Tall, float Wide, float Slant, bool bKept)
{
	USkeletalMeshComponent* Skin = GetShownMesh();

	if (!Picture || !Skin || Skin->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return nullptr;
	}

	Wounds.RemoveAll([](const UDecalComponent* Mark) { return !IsValid(Mark); });

	while (!bKept && Wounds.Num() >= MaxWounds && !Wounds.IsEmpty())
	{
		Wounds[0]->DestroyComponent();
		Wounds.RemoveAt(0);
	}

	// a picture's top is toward the decal's own minus Y: with its Y turned down the picture is the right way up on the
	// body, and Slant leans it from there
	const FVector Down = FVector(0.0f, 0.0f, -1.0f).RotateAngleAxis(Slant, Into);
	const FRotator Turn = FRotationMatrix::MakeFromXY(Into, Down).Rotator();

	UDecalComponent* Mark = UGameplayStatics::SpawnDecalAttached(Picture, FVector(Deep, Tall, Wide), Skin, Bone, Where, Turn, EAttachLocation::KeepWorldPosition);

	if (Mark)
	{
		// small as it is, it is not dropped for being small on the screen
		Mark->SetFadeScreenSize(0.0f);

		// the later mark lies over the earlier
		Mark->SortOrder = ++MarksMade;

		(bKept ? Soaks : Wounds).Add(Mark);
	}

	return Mark;
}

void AGutFighter::Soak()
{
	UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>();
	USkeletalMeshComponent* Skin = GetShownMesh();

	if (!bShowWounds || !Gore || !Skin || Beast || !BodyActor || MaxHealth <= 0.0f)
	{
		return;
	}

	// how far gone: a body does not lose most of what is in it and stay clean
	static const float Marks[] = { 0.65f, 0.45f, 0.28f, 0.12f };

	const FVector Front = -GetActorForwardVector();

	while (SoakStage < static_cast<int32>(UE_ARRAY_COUNT(Marks)) && Health <= MaxHealth * Marks[SoakStage])
	{
		const auto Lay = [this, Gore, Skin](FName Bone, const FVector& Into, float Deep, float Size, float Lower)
		{
			if (Skin->GetBoneIndex(Bone) != INDEX_NONE)
			{
				const FVector Where = Skin->GetBoneLocation(Bone) - (Into * Deep * 0.9f) - FVector(0.0f, 0.0f, Lower);

				MarkSkin(Gore->GetMark(EGutBloodMark::Soaked, nullptr), Bone, Where, Into, Deep * 0.85f, Size, Size, FMath::FRandRange(-20.0f, 20.0f), true);
			}
		};

		switch (SoakStage)
		{
			// down the chest
			case 0: Lay(FName("spine_04"), Front.RotateAngleAxis(FMath::FRandRange(-20.0f, 20.0f), FVector::UpVector), 13.0f, 10.0f, 0.0f); break;

			// the face, and the belly under the chest
			case 1:
				Lay(FName("head"), Front, 9.0f, 9.0f, -8.0f);
				Lay(FName("spine_02"), Front.RotateAngleAxis(FMath::FRandRange(-25.0f, 25.0f), FVector::UpVector), 12.0f, 17.0f, 0.0f);
				break;

			// a shoulder and the back: by now it has been turned and hit from everywhere
			case 2:
				Lay(FName("spine_05"), -Front, 12.0f, 18.0f, 0.0f);
				Lay(FName(FMath::RandBool() ? "upperarm_l" : "upperarm_r"), Front, 5.0f, 11.0f, 6.0f);
				break;

			// and at the end, the whole front of it
			default:
				Lay(FName("spine_03"), Front, 13.0f, 26.0f, 0.0f);
				Lay(FName(FMath::RandBool() ? "thigh_l" : "thigh_r"), Front, 7.0f, 13.0f, 12.0f);
				break;
		}

		++SoakStage;
	}
}

void AGutFighter::Bespatter(const FVector& FromVictim, float Amount)
{
	UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>();
	USkeletalMeshComponent* Skin = GetShownMesh();

	if (!bShowWounds || !Gore || !Skin || Beast || !BodyActor || Amount < 8.0f)
	{
		return;
	}

	// whoever opens somebody up at arm's length wears some of it: on the arm that did it, or across the front
	FVector Into = FromVictim.GetSafeNormal2D();

	if (Into.IsNearlyZero())
	{
		Into = -GetActorForwardVector();
	}

	Into = Into.RotateAngleAxis(FMath::FRandRange(-30.0f, 30.0f), FVector::UpVector);

	static const FName Places[] = { FName("lowerarm_r"), FName("spine_04"), FName("spine_02"), FName("head"), FName("upperarm_r") };

	const FName Bone = Places[FMath::RandHelper(static_cast<int32>(UE_ARRAY_COUNT(Places)))];
	const bool bTrunk = Bone == FName("spine_04") || Bone == FName("spine_02");
	const float Deep = bTrunk ? 13.0f : (Bone == FName("head") ? 9.0f : 4.5f);
	const float Size = FMath::Clamp(Amount * 0.6f, 7.0f, bTrunk ? 16.0f : 10.0f);

	if (Skin->GetBoneIndex(Bone) != INDEX_NONE)
	{
		MarkSkin(Gore->GetMark(EGutBloodMark::Thrown, nullptr), Bone, Skin->GetBoneLocation(Bone) - (Into * Deep * 0.9f), Into, Deep * 0.85f, Size, Size, FMath::FRandRange(0.0f, 360.0f));
	}
}

bool AGutFighter::SeverLimb(FName Bone, FVector ThrowDirection)
{
	const USkeletalMesh* BodyAsset = GetMesh()->GetSkeletalMeshAsset();
	const int32 BoneIndex = GetMesh()->GetBoneIndex(Bone);

	if (IsAlive() || Beast || !BodyAsset || BoneIndex == INDEX_NONE || SeveredBones.Contains(Bone))
	{
		return false;
	}

	// measure what is coming off before it goes: from the cut to the far end of the limb
	const FVector CutLocation = GetMesh()->GetBoneLocation(Bone);

	MakeSound(FName("gore_sever"), 1.0f, &CutLocation);

	FVector FarEnd = CutLocation;

	const FReferenceSkeleton& Skeleton = BodyAsset->GetRefSkeleton();

	for (int32 i = BoneIndex + 1; i < Skeleton.GetNum(); ++i)
	{
		if (Skeleton.BoneIsChildOf(i, BoneIndex))
		{
			const FVector Location = GetMesh()->GetBoneLocation(Skeleton.GetBoneName(i));

			if (FVector::DistSquared(Location, CutLocation) > FVector::DistSquared(FarEnd, CutLocation))
			{
				FarEnd = Location;
			}
		}
	}

	// the body loses everything below the cut
	GetMesh()->HideBoneByName(Bone, PBO_Term);

	if (HasBody())
	{
		const bool bHead = Bone == GutFighterConstants::HeadBone;

		for (UActorComponent* Component : BodyActor->GetComponents())
		{
			USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

			if (Part && Part->GetBoneIndex(Bone) != INDEX_NONE)
			{
				Part->HideBoneByName(Bone, PBO_None);
			}
			else if (UMeshComponent* Hair = Cast<UMeshComponent>(Component); Hair && !Part && bHead)
			{
				// hair, brows and beard go with the head
				Hair->SetVisibility(false);
			}
		}
	}

	SeveredBones.Add(Bone);

	const FVector Thrown = (ThrowDirection.GetSafeNormal2D() + FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), 0.9f)).GetSafeNormal();

	// and a grey-box stand-in for it lands on the floor
	if (GibMaterial)
	{
		const bool bHead = Bone == GutFighterConstants::HeadBone;
		const float Length = FMath::Max(FVector::Dist(CutLocation, FarEnd), 12.0f);

		UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, bHead ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

		// a round head, or a limb as long as what came off and a third as thick, lying along where it was
		const FVector Scale = bHead ? FVector(0.22f) : FVector(FMath::Clamp(Length * 0.3f, 8.0f, 15.0f) / 100.0f, FMath::Clamp(Length * 0.3f, 8.0f, 15.0f) / 100.0f, Length / 100.0f);
		const FRotator Rotation = bHead ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ(FarEnd - CutLocation).Rotator();

		if (AGutGib* Gib = GetWorld()->SpawnActor<AGutGib>((CutLocation + FarEnd) * 0.5f, Rotation))
		{
			Gib->Throw(Bone, Shape, Scale, GibMaterial, bApplyBodyTint ? BodyTint : FLinearColor(0.75f, 0.75f, 0.73f), Thrown * GutFighterConstants::GibSpeed);

			// the fighter's own limb where there is one to show, the shape where there is not
			if (HasBody())
			{
				TArray<USkeletalMeshComponent*> Parts;
				BodyActor->GetComponents<USkeletalMeshComponent>(Parts);

				Gib->ShowLimb(Parts, CutLocation);
			}
		}
	}

	SpillBlood(ThrowDirection, GutFighterConstants::SeverBlood);

	UE_LOG(LogGut, Log, TEXT("%s: cut through at %s"), *GetName(), *Bone.ToString());

	return true;
}

void AGutFighter::Kill(AGutFighter* InKiller, FVector ImpulseDirection)
{
	if (IsAlive())
	{
		Die(InKiller, ImpulseDirection.GetSafeNormal());
	}
}

bool AGutFighter::ReceiveHazard(AGutHazard* Hazard, FVector AwayDirection)
{
	// a staggered fighter can be hurt by the cage: nobody is holding it up, and the cage does not wait for a finisher
	if (!Hazard || !IsAlive() || !bCombatEnabled)
	{
		return false;
	}

	// some hazards can be dodged across
	if (Hazard->DoesDodgeAvoid() && IsInvulnerable())
	{
		return false;
	}

	const float Damage = Hazard->GetDamage();
	const FVector Away = AwayDirection.GetSafeNormal2D();

	MakeSound(VoiceSound(TEXT("pain")), 1.1f);

	++HazardHits;

	ShowCombatText(FString::Printf(TEXT("%s  -%d"), *Hazard->GetHitText().ToString(), FMath::RoundToInt32(Damage)), GutFighterConstants::BreakTextColor);

	UE_LOG(LogGut, Log, TEXT("%s -> %s: %s (health %.0f)"), *Hazard->GetName(), *GetName(), *Hazard->GetHitText().ToString(), FMath::Max(0.0f, Health - Damage));

	BP_OnHazardHit(Hazard);

	OnHazardHit.Broadcast(this, Hazard);

	SpillBlood(Away, Damage);

	// the cage's verdict is final: no stagger, no second wind
	if (ApplyHealthDamage(Damage))
	{
		KillingHazard = Hazard;

		// whoever drove the fighter into it a moment ago gets the credit
		AGutFighter* Credited = (LastAttacker.IsValid() && GetWorld()->GetTimeSeconds() - LastAttackedTime <= HazardKillCreditTime) ? LastAttacker.Get() : nullptr;

		Die(Credited, Away);

		return true;
	}

	// a staggered fighter that survives it stays down. Anyone else is left reeling.
	if (!IsStaggered())
	{
		EnterStun(Hazard->GetStunTime());
	}

	ApplyPush(Away, Hazard->GetThrowDistance(), Hazard->GetThrowDuration());

	return true;
}

void AGutFighter::TurnToward(const FVector& WorldPoint, float DeltaTime, float RateDegreesPerSecond)
{
	FVector ToPoint = WorldPoint - GetActorLocation();
	ToPoint.Z = 0.0f;

	if (ToPoint.IsNearlyZero())
	{
		return;
	}

	const FRotator Desired(0.0f, ToPoint.Rotation().Yaw, 0.0f);

	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Desired, DeltaTime, RateDegreesPerSecond));
}

void AGutFighter::SetWading(bool bInWater, float SpeedScale, float DodgeScale)
{
	bWading = bInWater;
	WadeSpeedScale = bInWater ? FMath::Clamp(SpeedScale, 0.1f, 1.0f) : 1.0f;
	WadeDodgeScale = bInWater ? FMath::Clamp(DodgeScale, 0.1f, 1.0f) : 1.0f;

	UpdateMovementForState();
}

void AGutFighter::SetMoveSpeedScale(float Scale)
{
	MoveSpeedScale = FMath::Max(Scale, 0.1f);

	UpdateMovementForState();
}

void AGutFighter::SetWoman(bool bIsWoman)
{
	bWoman = bIsWoman;

	UpdateMovementForState();
}

void AGutFighter::SetSquareUp(bool bEnabled)
{
	bSquareUp = bEnabled;

	UpdateMovementForState();
}

void AGutFighter::SetOpponent(AGutFighter* NewOpponent)
{
	Opponent = NewOpponent;

	UpdateMovementForState();
}

void AGutFighter::SetCombatEnabled(bool bEnabled)
{
	bCombatEnabled = bEnabled;

	if (!bCombatEnabled)
	{
		// nothing pressed before the bell carries over
		BufferedAction = EBufferedAction::None;
	}
}

void AGutFighter::SetCollared(bool bCollared)
{
	// an animal's collar, if it has one, is part of the animal
	Collar->SetVisibility(bCollared && CollarMesh != nullptr && !Beast);
}

void AGutFighter::SetBody(TSoftClassPtr<AActor> NewBodyClass, UIKRetargeter* NewRetargeter)
{
	// an animal stays the animal it is
	if (Beast)
	{
		return;
	}

	BodyClass = NewBodyClass;
	BodyRetargeter = NewRetargeter;

	// the body decides the voice
	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Voice = Sounds->VoiceOf(BodyClass.GetAssetName());

		// and the voice was told apart by the body's sex, which decides more than the voice
		SetWoman(Voice == FName("woman"));
	}

	if (BodyActor)
	{
		BodyRetarget->SetControlledMesh(nullptr);
		BodyActor->Destroy();
	}

	BodyActor = nullptr;
	Body = nullptr;

	// -GutNoBodies on the command line leaves every fighter as the placeholder, to tell what the characters cost a slow machine
	const bool bBodies = !FParse::Param(FCommandLine::Get(), TEXT("GutNoBodies"));

	UClass* Class = BodyRetargeter && bBodies ? BodyClass.LoadSynchronous() : nullptr;

	if (Class && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		BodyActor = GetWorld()->SpawnActor<AActor>(Class, GetMesh()->GetComponentTransform(), Params);
	}

	if (BodyActor)
	{
		// it is there to be looked at: every blow lands on the placeholder
		BodyActor->SetActorEnableCollision(false);
		BodyActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		for (UActorComponent* Component : BodyActor->GetComponents())
		{
			if (USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component); Part && Part->GetFName() == BodyComponentName)
			{
				Body = Part;
			}

			// what is painted on the pool stays on the pool: feet standing on a lane line do not turn into tile.
			// Unless bodies are to show the blood of the blows they take: then skin and cloth take decals, and the pool's
			// paint, which reaches a few centimetres off its tile, can get as far as the sole of a foot.
			if (UPrimitiveComponent* Seen = Cast<UPrimitiveComponent>(Component))
			{
				Seen->SetReceivesDecals(bShowWounds && Seen->IsA<USkeletalMeshComponent>());
			}
		}

		if (!Body)
		{
			UE_LOG(LogGut, Warning, TEXT("%s: %s has no skeletal mesh called %s, so it cannot be this fighter's body"), *GetName(), *Class->GetName(), *BodyComponentName.ToString());

			BodyActor->Destroy();
			BodyActor = nullptr;
		}
	}

	const bool bOwnBody = Body != nullptr;

	// the placeholder goes on fighting where nobody sees it
	GetMesh()->SetVisibility(!bOwnBody, false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (bOwnBody)
	{
		Body->AddTickPrerequisiteComponent(GetMesh());

		BodyRetarget->SetControlledMesh(Body);
		BodyRetarget->SetSourcePerformerMesh(GetMesh());
		BodyRetarget->SetRetargetAsset(BodyRetargeter);

		// the face takes its expression from the body, so that is where the performance is fed in. The body takes the hook with it when it goes.
		Body->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &AGutFighter::OnBodyPosed));

		for (const FName& Bone : SeveredBones)
		{
			Body->HideBoneByName(Bone, PBO_None);
		}
	}

	// what the fighter holds and wears moves to the skeleton that is seen
	FitCollar();

	UpdateWeaponMesh();
	UpdateArmorPlates();
}

void AGutFighter::FitCollar()
{
	Collar->AttachToComponent(GetShownMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CollarBone);
	Collar->SetRelativeTransform(CollarTransform);

	// the placeholder wears it as the Blueprint says. A body of its own has a neck of its own, and the ring is made to go round that.
	if (!BodyActor || !Body || !Body->GetSkeletalMeshAsset() || !CollarMesh || !CollarMesh->GetRenderData() || CollarMesh->GetRenderData()->LODResources.IsEmpty())
	{
		return;
	}

	// the ring as it was modelled: which way the hole through it runs (its shortest side), and how wide the hole is
	const FBox RingBox = CollarMesh->GetBoundingBox();
	const FVector RingMiddle = RingBox.GetCenter();
	const FVector RingSize = RingBox.GetExtent();

	const int32 Through = (RingSize.X <= RingSize.Y && RingSize.X <= RingSize.Z) ? 0 : (RingSize.Y <= RingSize.Z ? 1 : 2);

	const FPositionVertexBuffer& Ring = CollarMesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;

	if (Ring.GetNumVertices() == 0 || !Ring.GetVertexData())
	{
		return;
	}

	float Hole = TNumericLimits<float>::Max();

	for (uint32 i = 0; i < Ring.GetNumVertices(); ++i)
	{
		const FVector Out = FVector(Ring.VertexPosition(i)) - RingMiddle;

		Hole = FMath::Min(Hole, FVector2D(Out[(Through + 1) % 3], Out[(Through + 2) % 3]).Size());
	}

	// the ring lies square across the neck and goes where the neck goes: it rides on the bone in the middle of the neck.
	// As the body was modelled it stands upright, facing along its own Y.
	const FReferenceSkeleton& Skeleton = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 NeckBone = Skeleton.FindBoneIndex(CollarNeckBone);

	if (NeckBone == INDEX_NONE)
	{
		return;
	}

	const FTransform Neckwards = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, NeckBone);

	const FVector Along = Neckwards.GetUnitAxis(EAxis::X) * (Neckwards.GetUnitAxis(EAxis::X).Z < 0.0 ? -1.0 : 1.0);
	const FVector Over = (Along ^ FVector(1.0, 0.0, 0.0)).GetSafeNormal();
	const FVector Across = Over ^ Along;

	// measured across the middle of the band, not its whole height
	const float Thick = FMath::Max(0.6f, RingSize[Through] * CollarTransform.GetScale3D()[Through] * 0.5f);

	// every point of the skin of the neck and what it stands on, as modelled, from that bone
	TArray<FVector> Skin;

	for (UActorComponent* Component : BodyActor->GetComponents())
	{
		const USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

		if (!Part || (Part->GetFName() != BodyComponentName && Part->GetFName() != FaceComponentName) || !Part->GetSkeletalMeshAsset())
		{
			continue;
		}

		const USkeletalMesh* Model = Part->GetSkeletalMeshAsset();
		const FSkeletalMeshRenderData* Data = Model->GetResourceForRendering();
		const int32 Bone = Model->GetRefSkeleton().FindBoneIndex(CollarNeckBone);

		if (!Data || Bone == INDEX_NONE)
		{
			continue;
		}

		const FVector Base = FAnimationRuntime::GetComponentSpaceTransformRefPose(Model->GetRefSkeleton(), Bone).GetLocation();

		for (const FSkeletalMeshLODRenderData& Detail : Data->LODRenderData)
		{
			const FPositionVertexBuffer& Points = Detail.StaticVertexBuffers.PositionVertexBuffer;

			// the finest level of detail that is in memory to be read
			if (Points.GetNumVertices() == 0 || !Points.GetVertexData())
			{
				continue;
			}

			for (uint32 i = 0; i < Points.GetNumVertices(); ++i)
			{
				const FVector Off = FVector(Points.VertexPosition(i)) - Base;

				if (Off.SizeSquared() < FMath::Square(CollarReach + 12.0f))
				{
					Skin.Add(Off);
				}
			}

			break;
		}
	}

	// the neck where a ring would lie at a height up it: its middle, how far it reaches each way, and by how much more than
	// that the ring has to be widened to clear every point of it
	struct FSection
	{
		FVector2D Middle = FVector2D::ZeroVector;
		FVector2D Half = FVector2D::ZeroVector;
		float Tightest = 1.0f;
		int32 Points = 0;
	};

	auto Cut = [&](float Rise)
	{
		FSection Section;
		FBox2D Reach(ForceInit);
		TArray<FVector2D> Flat;

		for (const FVector& Off : Skin)
		{
			if (FMath::Abs((Off | Along) - Rise) > Thick)
			{
				continue;
			}

			const FVector2D Point(Off | Across, Off | Over);

			// a neck, not the shoulders it stands on
			if (Point.Size() < CollarReach)
			{
				Flat.Add(Point);
				Reach += Point;
			}
		}

		// a neck is about as deep as it is wide. Anything further forward of the nape than that is the jaw hanging over it,
		// which the ring goes under, not round.
		if (Flat.Num() >= 12)
		{
			const float Front = Reach.Min.Y + (Reach.GetSize().X * 1.05f);

			Flat.RemoveAll([Front](const FVector2D& Point) { return Point.Y > Front; });

			Reach = FBox2D(Flat);
		}

		Section.Points = Flat.Num();

		if (Flat.Num() >= 12)
		{
			Section.Middle = Reach.GetCenter();
			Section.Half = Reach.GetExtent().ComponentMax(FVector2D(1.0, 1.0));

			// a collar is riveted on tight. It is sized to most of the skin under it, not to the furthest point of it: where
			// the shoulders rise into the neck it presses in a little, as iron does.
			TArray<float> Out;

			for (const FVector2D& Point : Flat)
			{
				Out.Add(((Point - Section.Middle) / Section.Half).Size());
			}

			Out.Sort();

			Section.Tightest = Out[FMath::Clamp(FMath::FloorToInt32(Out.Num() * CollarTightness), 0, Out.Num() - 1)];
		}

		return Section;
	};

	// as low as the neck is still a neck: going up from the shoulders, the first height at which it is nearly as narrow as it gets.
	// Heights are from the bone, and the lowest is below it.
	TArray<FSection> Sections;
	float Narrowest = TNumericLimits<float>::Max();

	for (int32 i = 0; CollarLowest + i <= CollarHighest; ++i)
	{
		Sections.Add(Cut(CollarLowest + i));

		if (Sections.Last().Points >= 12)
		{
			Narrowest = FMath::Min(Narrowest, Sections.Last().Half.X * Sections.Last().Tightest);
		}
	}

	int32 Chosen = INDEX_NONE;

	for (int32 i = 0; i < Sections.Num() && Chosen == INDEX_NONE; ++i)
	{
		if (Sections[i].Points >= 12 && Sections[i].Half.X * Sections[i].Tightest <= Narrowest + CollarShoulder)
		{
			Chosen = i;
		}
	}

	if (Chosen == INDEX_NONE)
	{
		UE_LOG(LogGut, Log, TEXT("%s: no neck could be measured on %s, so its collar sits as the Blueprint says"), *GetName(), *BodyActor->GetClass()->GetName());
		return;
	}

	const FSection& Neck = Sections[Chosen];
	const float Rise = CollarLowest + Chosen;

	// bent to the neck it is put on: widened each way by what that way needs, and made no taller or thicker
	FVector Size = CollarTransform.GetScale3D();

	Size[(Through + 1) % 3] = ((Neck.Half.X * Neck.Tightest) + CollarSlack) / FMath::Max(Hole, 0.1f);
	Size[(Through + 2) % 3] = ((Neck.Half.Y * Neck.Tightest) + CollarSlack) / FMath::Max(Hole, 0.1f);

	FMatrix Turned = FMatrix::Identity;

	Turned.SetAxis(Through, Along);
	Turned.SetAxis((Through + 1) % 3, Across);
	Turned.SetAxis((Through + 2) % 3, Over);

	const FQuat Turn(Turned);

	// where that is on the body as modelled, and from there onto the bone
	FTransform Fitted(Turn, FVector::ZeroVector, Size);

	Fitted.SetLocation(Neckwards.GetLocation() + (Along * Rise) + (Across * Neck.Middle.X) + (Over * Neck.Middle.Y) - Turn.RotateVector(RingMiddle * Size));

	Collar->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CollarNeckBone);
	Collar->SetRelativeTransform(Fitted.GetRelativeTransform(Neckwards));

	UE_LOG(LogGut, Log, TEXT("%s: the neck of %s is %.1f cm wide and %.1f deep %.0f cm up from the bone, its middle %.1f cm forward of it (%d points); the ring's hole is %.1f, so it is worn %.2f and %.2f of its size across"),
		*GetName(), *BodyActor->GetClass()->GetName(), Neck.Half.X * Neck.Tightest * 2.0f, Neck.Half.Y * Neck.Tightest * 2.0f, Rise, Neck.Middle.Y, Neck.Points, Hole * 2.0f, Size[(Through + 1) % 3], Size[(Through + 2) % 3]);
}

void AGutFighter::BecomeBeast(UGutBeastData* NewBeast)
{
	if (!NewBeast || !NewBeast->Mesh || !IsAlive())
	{
		return;
	}

	// whatever person this fighter was seen as goes, with what it held and wore
	SetBody(nullptr, nullptr);
	EquipWeapon(nullptr);
	ClearArmor();

	Beast = NewBeast;

	Collar->SetVisibility(false);

	// the placeholder goes on fighting where nobody sees it, as it does under a person
	GetMesh()->SetVisibility(false, false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (!BeastBody)
	{
		BeastBody = NewObject<USkeletalMeshComponent>(this, TEXT("BeastBody"));
		BeastBody->SetupAttachment(GetCapsuleComponent());
		BeastBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BeastBody->SetReceivesDecals(false);
		BeastBody->RegisterComponent();
	}

	BeastBody->SetSkeletalMesh(Beast->Mesh);
	BeastBody->SetAnimInstanceClass(UGutBeastAnimInstance::StaticClass());
	BeastBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// its feet on the floor, under the middle of the fighter
	BeastBody->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), Beast->Turn);
	BeastBody->SetRelativeScale3D(FVector(Beast->Scale));

	// the next coat in the list, by how many of them there are in the cage already
	if (!Beast->Coats.IsEmpty())
	{
		int32 Others = 0;

		for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
		{
			Others += (*It != this && It->IsBeast()) ? 1 : 0;
		}

		if (UMaterialInstanceDynamic* Coat = BeastBody->CreateDynamicMaterialInstance(0))
		{
			Coat->SetVectorParameterValue(FName("BaseColorFactor"), Beast->Coats[Others % Beast->Coats.Num()]);
		}
	}

	// what it has for hands
	if (Beast->Bite)
	{
		Fists = Beast->Bite;
	}

	Voice = Beast->Voice;

	if (!Beast->DisplayName.IsEmpty())
	{
		DisplayName = Beast->DisplayName;
	}

	SetMaxHealth(Beast->Health);
	SetHealth(Beast->Health);
	SetMoveSpeedScale(Beast->SpeedScale);

	// for trying an animal's numbers without bringing it in again: -GutBeast=80,1.3 is its health and how hard it bites against what is written
	FString Tried;

	if (FParse::Value(FCommandLine::Get(), TEXT("GutBeast="), Tried, false))
	{
		TArray<FString> Parts;
		Tried.ParseIntoArray(Parts, TEXT(","));

		if (Parts.Num() == 2)
		{
			SetMaxHealth(FCString::Atof(*Parts[0]));
			SetHealth(GetMaxHealth());
			SetDamageScale(FCString::Atof(*Parts[1]));
		}
	}

	if (AGutOpponentController* Brain = Cast<AGutOpponentController>(GetController()))
	{
		Brain->SetTuning(Beast->Tuning);
	}

	BeastPose = static_cast<uint8>(EGutBeastPose::Stand);
	BeastPoseTime = FMath::FRandRange(0.0f, 2.0f);
	NextBeastSoundTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.3f, Beast->IdleSoundGap);

	UpdateWeaponMesh();

	UE_LOG(LogGut, Log, TEXT("%s is %s"), *GetName(), *Beast->GetName());
}

void AGutFighter::UpdateBeast(float DeltaTime)
{
	UGutBeastAnimInstance* Anim = BeastBody ? Cast<UGutBeastAnimInstance>(BeastBody->GetAnimInstance()) : nullptr;

	if (!Beast || !Anim)
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();

	EGutBeastPose Wanted = EGutBeastPose::Stand;

	switch (State)
	{
	case EGutFighterState::Dead:
		Wanted = EGutBeastPose::Dead;
		break;

	case EGutFighterState::Stunned:
	case EGutFighterState::Staggered:
		Wanted = EGutBeastPose::Hurt;
		break;

	case EGutFighterState::Windup:
	case EGutFighterState::Active:
	case EGutFighterState::Recovery:
		Wanted = (CurrentAttackType == EGutAttackType::Heavy && Beast->Leap) ? EGutBeastPose::Leap : EGutBeastPose::Strike;
		break;

	case EGutFighterState::Dodging:
		Wanted = EGutBeastPose::Move;
		break;

	case EGutFighterState::Finishing:

		// at the one it has beaten. While it is still going in to it, it goes as it goes anywhere.
		if (!bFinisherClosing)
		{
			Wanted = EGutBeastPose::Maul;
			break;
		}

		[[fallthrough]];

	default:
	{
		// between a walk and a run it stays as it is until the speed is well past the line, so that it does not flicker between them
		const bool bWasWalking = BeastPose == static_cast<uint8>(EGutBeastPose::Walk);
		const float Line = Beast->WalkUnderSpeed * (bWasWalking ? 1.15f : 0.85f);

		Wanted = Speed <= 25.0f ? EGutBeastPose::Stand : ((Beast->Walk && Speed < Line) ? EGutBeastPose::Walk : EGutBeastPose::Move);
		break;
	}
	}

	// a state begun again (a second bite straight after the first, a second blow taken) starts its clip again
	const bool bAgain = State != EGutFighterState::Dead && StateTime + 0.001f < BeastStateTimeBefore
		&& (Wanted == EGutBeastPose::Strike || Wanted == EGutBeastPose::Leap || Wanted == EGutBeastPose::Hurt) && State != EGutFighterState::Active && State != EGutFighterState::Recovery;

	const bool bChanged = static_cast<uint8>(Wanted) != BeastPose || bAgain;

	BeastStateTimeBefore = StateTime;

	if (bChanged)
	{
		BeastPose = static_cast<uint8>(Wanted);
		BeastPoseTime = 0.0f;
	}

	const UAnimSequenceBase* Clip = Beast->Stand;
	float Time = 0.0f;
	float Blend = 0.18f;

	const auto Looped = [](const UAnimSequenceBase* In, float At)
	{
		return In ? FMath::Fmod(At, FMath::Max(In->GetPlayLength(), 0.05f)) : 0.0f;
	};

	const auto Once = [](const UAnimSequenceBase* In, float At)
	{
		return In ? FMath::Clamp(At, 0.0f, FMath::Max(In->GetPlayLength() - 0.02f, 0.0f)) : 0.0f;
	};

	switch (Wanted)
	{
	case EGutBeastPose::Move:
	{
		// its legs go as fast as the ground under it does
		const float Rate = State == EGutFighterState::Dodging ? 1.6f : FMath::Clamp(Speed / FMath::Max(Beast->MoveClipSpeed, 1.0f), 0.45f, 1.8f);

		BeastPoseTime += DeltaTime * Rate;

		Clip = Beast->Move ? Beast->Move : Beast->Stand;
		Time = Looped(Clip, BeastPoseTime);
		break;
	}

	case EGutBeastPose::Walk:
		BeastPoseTime += DeltaTime * FMath::Clamp(Speed / FMath::Max(Beast->WalkClipSpeed, 1.0f), 0.6f, 1.9f);

		Clip = Beast->Walk;
		Time = Looped(Clip, BeastPoseTime);
		Blend = 0.22f;
		break;

	case EGutBeastPose::Strike:
	case EGutBeastPose::Leap:
	{
		const bool bLeap = Wanted == EGutBeastPose::Leap;

		Clip = bLeap ? Beast->Leap : (Beast->Strike ? Beast->Strike : Beast->Stand);

		// the jaws close when the fight says the bite lands, however long the clip takes to get there
		const float Impact = bLeap ? Beast->LeapImpactTime : Beast->StrikeImpactTime;

		if (State == EGutFighterState::Windup)
		{
			Time = Impact * FMath::Clamp(StateTime / FMath::Max(CurrentAttack.Windup, 0.01f), 0.0f, 1.0f);
		}
		else
		{
			Time = Impact + StateTime + (State == EGutFighterState::Recovery ? CurrentAttack.Active : 0.0f);
		}

		Time = Once(Clip, Time);
		Blend = 0.1f;
		break;
	}

	case EGutBeastPose::Maul:
		BeastPoseTime += DeltaTime;

		Clip = Beast->Maul ? Beast->Maul : (Beast->Strike ? Beast->Strike : Beast->Stand);
		Time = Looped(Clip, BeastPoseTime);
		break;

	case EGutBeastPose::Hurt:
		BeastPoseTime += DeltaTime;

		Clip = Beast->Hurt ? Beast->Hurt : Beast->Stand;
		Time = Beast->Hurt ? Once(Clip, BeastPoseTime) : Looped(Clip, BeastPoseTime);
		Blend = 0.08f;
		break;

	case EGutBeastPose::Dead:
		BeastPoseTime += DeltaTime;

		Clip = Beast->Death ? Beast->Death : Beast->Stand;
		Time = Beast->Death ? Once(Clip, Beast->DeathStartTime + BeastPoseTime) : 0.0f;
		Blend = 0.12f;
		break;

	default:
		BeastPoseTime += DeltaTime;

		Time = Looped(Clip, BeastPoseTime);
		Blend = 0.25f;
		break;
	}

	Anim->Show(Clip, Time, bChanged, Blend);

	// its feet are heard each time its stride comes round, and twice in a walk's
	if ((Wanted == EGutBeastPose::Move || Wanted == EGutBeastPose::Walk) && Clip && !bChanged)
	{
		const float Half = Clip->GetPlayLength() * 0.5f;
		const bool bRound = Time < BeastClipTimeBefore;
		const bool bHalfWay = Wanted == EGutBeastPose::Walk && BeastClipTimeBefore < Half && Time >= Half;

		if (bRound || bHalfWay)
		{
			MakeSound(bWading ? FName("wade") : FName("dog_paws"), Wanted == EGutBeastPose::Move ? 0.6f : 0.35f);
		}
	}

	BeastClipTimeBefore = Time;

	// and it is heard, now and again, for as long as it is loose
	const float Now = GetWorld()->GetTimeSeconds();

	if (IsAlive() && bCombatEnabled && Now >= NextBeastSoundTime && !Beast->IdleSound.IsNone())
	{
		NextBeastSoundTime = Now + (Beast->IdleSoundGap * FMath::FRandRange(0.6f, 1.5f));

		MakeSound(Beast->IdleSound, 0.7f);
	}
}

bool AGutFighter::HasBody() const
{
	return Body != nullptr;
}

USkeletalMeshComponent* AGutFighter::GetShownMesh() const
{
	return Body ? Body.Get() : GetMesh();
}

bool AGutFighter::IsCollared() const
{
	return Collar->IsVisible();
}

void AGutFighter::SetMaxHealth(float NewMaxHealth)
{
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	Health = MaxHealth;
}

void AGutFighter::SetBodyTint(FLinearColor NewTint)
{
	BodyTint = NewTint;

	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		// a material that is already an instance of its own is reused, not wrapped again
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(i));

		if (!Material)
		{
			Material = GetMesh()->CreateAndSetMaterialInstanceDynamic(i);
		}

		if (Material)
		{
			Material->SetVectorParameterValue(BodyTintParameter, BodyTint);
		}
	}
}

int32 AGutFighter::GetCrippledLimbCount() const
{
	int32 Count = 0;

	for (const float Limb : LimbHealth)
	{
		Count += Limb <= 0.0f ? 1 : 0;
	}

	return Count;
}

void AGutFighter::SetHealth(float NewHealth)
{
	if (IsAlive())
	{
		// this never kills: death always goes through an attack or Kill
		Health = FMath::Clamp(NewHealth, 1.0f, MaxHealth);
	}
}

void AGutFighter::SetStamina(float NewStamina)
{
	Stamina = FMath::Clamp(NewStamina, 0.0f, MaxStamina);
}

void AGutFighter::SetLimbHealth(EGutLimb Limb, float NewHealth)
{
	const bool bWasCrippled = IsLimbCrippled(Limb);

	LimbHealth[static_cast<int32>(Limb)] = FMath::Clamp(NewHealth, 0.0f, MaxLimbHealth);

	if (IsLimbCrippled(Limb) && !bWasCrippled)
	{
		CrippleLimb(Limb);
	}
	else
	{
		// a leg that has healed walks again
		UpdateMovementForState();
	}
}

float AGutFighter::GetStaggerTimeRemaining() const
{
	return IsStaggered() ? FMath::Max(0.0f, CurrentStaggerDuration - StateTime) : 0.0f;
}

bool AGutFighter::CanFinish(const AGutFighter* Target) const
{
	if (!IsAlive() || !bCombatEnabled || !Target || Target == this)
	{
		return false;
	}

	if (State != EGutFighterState::Idle && State != EGutFighterState::Blocking)
	{
		return false;
	}

	// one finisher at a time
	if (!Target->IsStaggered() || Target->IsBeingFinished())
	{
		return false;
	}

	const float HeightDifference = FMath::Abs(Target->GetActorLocation().Z - GetActorLocation().Z);

	return HeightDifference <= MaxAttackHeightDifference && GetGapTo(Target) <= FinisherRange;
}

const UGutWeaponData* AGutFighter::GetArmament() const
{
	return Weapon ? Weapon.Get() : Fists.Get();
}

FGutStanceAnimations AGutFighter::GetStanceAnimations() const
{
	// out of a fight altogether, whatever is in the hands
	if (bAtEase && AtEaseStance.Idle)
	{
		return AtEaseStance;
	}

	// the weapon's own clips, then the fighter's, then the single placeholders every fighter had before weapons had stances
	const UGutWeaponData* Armament = GetArmament();

	FGutStanceAnimations Result = Armament ? Armament->Stance : Stance;

	if (!Result.Idle)
	{
		Result.Idle = Stance.Idle;
	}

	if (!Result.Block)
	{
		Result.Block = Stance.Block;
	}

	if (!Result.Stagger)
	{
		Result.Stagger = Stance.Stagger;
	}

	if (!Result.Stun)
	{
		Result.Stun = Stance.Stun ? Stance.Stun : StunAnimation;
		Result.StunLength = Stance.Stun ? Stance.StunLength : StunAnimationLength;
	}

	if (!Result.Dodge)
	{
		Result.Dodge = Stance.Dodge ? Stance.Dodge : DodgeAnimation;
		Result.DodgeLength = Stance.Dodge ? Stance.DodgeLength : DodgeAnimationLength;
	}

	if (!Result.Backstep)
	{
		Result.Backstep = Stance.Backstep;
		Result.BackstepLength = Stance.BackstepLength;
	}

	return Result;
}

EGutLimb AGutFighter::GetArmFor(const UGutWeaponData* ForWeapon) const
{
	if (ForWeapon && ForWeapon->bOffHand)
	{
		return WeaponArm == EGutLimb::LeftArm ? EGutLimb::RightArm : EGutLimb::LeftArm;
	}

	return WeaponArm;
}

const FGutAttackDef& AGutFighter::GetHeavyAttack() const
{
	static const FGutAttackDef NoAttack;

	const UGutWeaponData* Armament = GetArmament();

	return Armament ? Armament->HeavyAttack : NoAttack;
}

bool AGutFighter::IsInvulnerable() const
{
	return State == EGutFighterState::Dodging && StateTime >= DodgeInvulnerableStart && StateTime <= DodgeInvulnerableEnd;
}

int32 AGutFighter::GetNextComboIndex() const
{
	// keep going down the chain only if the last light attack has just ended
	const bool bChaining = GetWorld()->GetTimeSeconds() - LastLightAttackEndTime <= ComboWindow;

	const UGutWeaponData* Armament = GetArmament();
	const int32 ChainLength = Armament ? Armament->LightAttacks.Num() : 0;

	// from behind the guard some weapons have one blow of their own: a shield hits with the shield
	if (Armament && (bWantsBlock || State == EGutFighterState::Blocking) && Armament->LightAttacks.IsValidIndex(Armament->GuardedLightAttack))
	{
		return Armament->GuardedLightAttack;
	}

	return (bChaining && ComboIndex + 1 < ChainLength) ? ComboIndex + 1 : 0;
}

const FGutAttackDef* AGutFighter::GetNextLightAttack() const
{
	const UGutWeaponData* Armament = GetArmament();

	return (Armament && !Armament->LightAttacks.IsEmpty()) ? &Armament->LightAttacks[GetNextComboIndex()] : nullptr;
}

float AGutFighter::GetTimeSinceStaminaFail() const
{
	return GetWorld()->GetTimeSeconds() - LastStaminaFailTime;
}

bool AGutFighter::GetCombatText(FString& OutText, FLinearColor& OutColor, float& OutAge) const
{
	OutText = CombatText;
	OutColor = CombatTextColor;
	OutAge = GetWorld()->GetTimeSeconds() - CombatTextTime;

	return !CombatText.IsEmpty();
}

float AGutFighter::GetGapTo(const AGutFighter* Other) const
{
	if (!Other)
	{
		return UE_BIG_NUMBER;
	}

	const float CenterDistance = FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());

	return FMath::Max(0.0f, CenterDistance - Other->GetCapsuleComponent()->GetScaledCapsuleRadius());
}

bool AGutFighter::IsInArc(const AGutFighter* Other, float Reach, float HalfAngleDegrees) const
{
	if (!Other)
	{
		return false;
	}

	const FVector ToOther = Other->GetActorLocation() - GetActorLocation();

	if (FMath::Abs(ToOther.Z) > MaxAttackHeightDifference || GetGapTo(Other) > Reach)
	{
		return false;
	}

	const FVector Direction = ToOther.GetSafeNormal2D();

	// standing on top of each other counts as in front
	if (Direction.IsNearlyZero())
	{
		return true;
	}

	return FVector::DotProduct(GetActorForwardVector(), Direction) >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

void AGutFighter::SetState(EGutFighterState NewState)
{
	// -GutStateLog on the command line writes every change of state to the log with the time, to find what took how long
	static const bool bLogStates = FParse::Param(FCommandLine::Get(), TEXT("GutStateLog"));

	if (bLogStates)
	{
		UE_LOG(LogGut, Log, TEXT("%s: %s -> %s at %.3f, %.0f cm from its opponent"), *GetName(), *UEnum::GetValueAsString(State), *UEnum::GetValueAsString(NewState),
			GetWorld()->GetTimeSeconds(), Opponent.IsValid() ? GetGapTo(Opponent.Get()) : 0.0f);
	}

	State = NewState;

	// time in a state counts from the fighter's next turn. If the state was set from outside before this frame's turn,
	// by a blow or by a controller, that turn would otherwise count against it, and the fighter would be a frame ahead.
	StateTime = IsAwaitingTick() ? -GetWorld()->GetDeltaSeconds() : 0.0f;

	// the plate stands for a raised guard on the placeholder skeleton. A fighter with a body of its own shows its guard itself.
	GuardPlate->SetHiddenInGame(State != EGutFighterState::Blocking || HasBody() || Beast);

	UpdateMovementForState();
}

void AGutFighter::UpdateMovementForState()
{
	float SpeedScale = 0.0f;
	bool bFaceMovement = false;

	switch (State)
	{
	case EGutFighterState::Idle:
		SpeedScale = 1.0f;

		// a fighter that has squared up is turned toward its opponent in Tick instead
		bFaceMovement = !(bSquareUp && HasLiveOpponent());
		break;

	case EGutFighterState::Blocking:
		SpeedScale = BlockingSpeedScale;

		// with nobody to guard against, just walk normally
		bFaceMovement = !HasLiveOpponent();
		break;

	case EGutFighterState::Recovery:
		SpeedScale = RecoverySpeedScale;
		break;

	case EGutFighterState::Finishing:

		// only to walk in to where the finisher reaches
		SpeedScale = bFinisherClosing ? FinisherClosingSpeedScale : 0.0f;
		break;

	default:
		break;
	}

	// at ease nobody hurries as a fighter does: a walk, or a jog
	const float Pace = bAtEase ? (bJogging ? AtEaseJogSpeed : AtEaseWalkSpeed) : WalkSpeed;

	GetCharacterMovement()->MaxWalkSpeed = Pace * SpeedScale * GetLegSpeedScale() * MoveSpeedScale * WadeSpeedScale * (bWoman ? WomanPaceScale : 1.0f);
	GetCharacterMovement()->bOrientRotationToMovement = bFaceMovement;
}

void AGutFighter::SetAtEase(bool bNewAtEase)
{
	if (bAtEase == bNewAtEase)
	{
		return;
	}

	bAtEase = bNewAtEase;
	bJogging = false;

	if (bAtEase)
	{
		SetCombatEnabled(false);
	}

	// its pace, and what its empty hands do
	UpdateMovementForState();
	UpdateWeaponMesh();
}

void AGutFighter::SetJogging(bool bNewJogging)
{
	if (bJogging != bNewJogging)
	{
		bJogging = bNewJogging;

		UpdateMovementForState();
	}
}

bool AGutFighter::StartAttack(const FGutAttackDef& Attack, EGutAttackType Type)
{
	// without the stamina for it the blow still comes, late and weak. The screen shows why, as it does for a dodge refused.
	const bool bTired = !CheckStamina(Attack.StaminaCost);

	SpendStamina(Attack.StaminaCost);

	CurrentAttack = Attack;

	// a woman's blow carries less than a man's, and comes and goes sooner
	if (bWoman)
	{
		CurrentAttack.Windup *= WomanSwingTimeScale;
		CurrentAttack.Recovery *= WomanSwingTimeScale;
		CurrentAttack.Damage *= WomanPowerScale;
		CurrentAttack.StunTime *= WomanPowerScale;
		CurrentAttack.Knockback *= WomanPowerScale;
		CurrentAttack.BlockStaminaDamage *= WomanPowerScale;
	}

	if (bTired)
	{
		CurrentAttack.Windup *= TiredWindupScale;
		CurrentAttack.Recovery *= TiredRecoveryScale;
		CurrentAttack.Damage *= TiredPowerScale;
		CurrentAttack.StunTime *= TiredPowerScale;
		CurrentAttack.Knockback *= TiredPowerScale;
		CurrentAttack.BlockStaminaDamage *= TiredPowerScale;
		CurrentAttack.LungeDistance *= TiredPowerScale;
	}

	CurrentAttackType = Type;
	bDriving = false;
	SwingHitFighters.Reset();

	++AttackSerial;
	++Tally.Swings;
	AttackStartTime = GetWorld()->GetTimeSeconds();

	// close to the opponent, the attack turns to face them
	const AGutFighter* Target = Opponent.Get();

	bTrackOpponent = HasLiveOpponent() && FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) <= AttackAssistRange;

	SetState(EGutFighterState::Windup);

	// a heavy blow is grunted into, a light one now and then
	bSwingHeard = false;

	if (Type == EGutAttackType::Heavy || bTired || FMath::FRand() < 0.4f)
	{
		MakeSound(VoiceSound(TEXT("effort")), Type == EGutAttackType::Heavy ? 1.0f : 0.6f);
	}

	// what is seen: the attack's own clip, or, swung at an animal, the one of this weapon's light attacks that goes in low,
	// so that nobody swings at head height over a dog's back
	const FGutAttackDef* Seen = &Attack;

	if (const UGutWeaponData* Armament = GetArmament(); Armament && Type == EGutAttackType::Light && Opponent.IsValid() && Opponent->IsBeast()
		&& Armament->LightAttacks.IsValidIndex(Armament->LowLightAttack) && Armament->LightAttacks[Armament->LowLightAttack].Animation)
	{
		Seen = &Armament->LightAttacks[Armament->LowLightAttack];
	}

	// step into the attack, but never through the opponent
	FVector LungeDirection = GetActorForwardVector();
	float LungeDistance = Attack.LungeDistance;

	if (bTrackOpponent)
	{
		const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

		if (!ToTarget.IsNearlyZero())
		{
			LungeDirection = ToTarget;
		}

		const float Room = GetGapTo(Target) - GetCapsuleComponent()->GetScaledCapsuleRadius() - GutFighterConstants::LungeStopGap;

		// A blow that the rules will let land is stepped into as far as it takes for it to be seen to land: the rules reach
		// further than an arm does. This is a rule, so it goes by the weapon and not by the clip or the body that is seen:
		// a fight is the same fought with bodies to look at or without. What the step leaves over, the seen body makes up.
		if (GutFighterConstants::bStrikesSeenToLand && !Target->IsBeast() && GetGapTo(Target) <= Attack.Reach + Attack.LungeDistance)
		{
			const float Apart = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());

			LungeDistance = FMath::Clamp(Apart - (GetArmReach() + Target->GetStruckDepth() - StrikeDepth), 0.0f, Attack.LungeDistance + StrikeStepIn);
		}

		LungeDistance = FMath::Clamp(Room, 0.0f, LungeDistance);
	}

	ApplyPush(LungeDirection, LungeDistance, Attack.Windup);

	// played so that its blow lands as the windup ends
	if (Seen->Animation)
	{
		const float AnimWindup = FMath::Max(Seen->AnimImpactTime - Seen->AnimStartTime, 0.01f);

		PlaySlotAnimation(Seen->Animation, AnimWindup / FMath::Max(Attack.Windup, 0.01f), Seen->AnimStartTime);
	}

	// and what the step leaves it short by, the body that is seen makes up: see UpdateStrikeShift
	StrikeClip = Seen->Animation;
	StrikeGoal = 0.0f;
	bStrikeMeasured = false;
	StrikeNearest = UE_BIG_NUMBER;

	BP_OnAttackStarted(Attack, Type);

	return true;
}

bool AGutFighter::StartDodge(const FVector& WorldDirection)
{
	if (GetWorld()->GetTimeSeconds() - LastDodgeEndTime < DodgeCooldown)
	{
		return false;
	}

	if (!CheckStamina(GetDodgeStaminaCost()))
	{
		return false;
	}

	FVector Direction = WorldDirection;
	Direction.Z = 0.0f;

	const bool bSteered = Direction.Normalize();

	// -GutDodgeTurns on the command line dodges as fighters did before pass 11: turning to face the way they go.
	// For measuring what the change did to the fight.
	static const bool bDodgeTurnsAway = FParse::Param(FCommandLine::Get(), TEXT("GutDodgeTurns"));

	const FGutStanceAnimations Clips = GetStanceAnimations();

	// a fighter squared up to its opponent does not turn its back to dodge: it goes the way it is steering and keeps its eyes
	// on the other one, forward in a dash, back in a hop, to either side in a sidestep
	UAnimSequenceBase* Step = nullptr;
	float StepLength = Clips.DodgeLength;

	if (!bSteered)
	{
		// no direction given: step straight back, still facing forward
		Direction = -GetActorForwardVector();

		Step = Clips.Backstep;
		StepLength = Clips.BackstepLength;
	}
	else if (!bDodgeTurnsAway && bSquareUp && HasLiveOpponent() && Clips.DodgeLeft && Clips.DodgeRight && Clips.Backstep)
	{
		const FVector Own = GetActorRotation().UnrotateVector(Direction);
		const float Way = FMath::RadiansToDegrees(FMath::Atan2(Own.Y, Own.X));

		if (FMath::Abs(Way) <= 45.0f)
		{
			Step = Clips.Dodge;
		}
		else if (FMath::Abs(Way) >= 135.0f)
		{
			Step = Clips.Backstep;
			StepLength = Clips.BackstepLength;
		}
		else
		{
			Step = Way > 0.0f ? Clips.DodgeRight : Clips.DodgeLeft;
		}
	}
	else
	{
		// nobody to keep an eye on: turn and dash the way the fighter is steering
		SetActorRotation(Direction.Rotation());

		Step = Clips.Dodge;
	}

	SpendStamina(GetDodgeStaminaCost());

	// a dodge ends the light chain
	LastLightAttackEndTime = -100.0f;

	SetState(EGutFighterState::Dodging);

	// crippled legs do not carry a dodge as far
	ApplyPush(Direction, DodgeDistance * GetLegSpeedScale() * WadeDodgeScale * (bWoman ? WomanPaceScale : 1.0f), DodgeDuration);

	if (Step)
	{
		PlaySlotAnimation(Step, StepLength / DodgeDuration, 0.0f, 0.05f, 0.15f);
	}
	else if (Clips.Dodge)
	{
		PlaySlotAnimation(Clips.Dodge, Clips.DodgeLength / DodgeDuration, 0.0f, 0.05f, 0.15f);
	}

	// feet scuffing off the tile
	MakeSound(bWading ? FName("wade_fast") : FName("scuff"), 0.8f);

	BP_OnDodgeStarted(Direction);

	return true;
}

void AGutFighter::PerformAttackSweep()
{
	// find every pawn that could possibly be in reach
	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(CurrentAttack.Reach + 100.0f);

	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams, CollisionShape, QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AGutFighter* Target = Cast<AGutFighter>(Overlap.GetActor());

		// one swing reaches each fighter once
		if (!Target || !Target->IsAlive() || SwingHitFighters.Contains(Target))
		{
			continue;
		}

		// in a fight of more than two, nobody is hurt by its own side
		if (FightSide != INDEX_NONE && Target->FightSide == FightSide)
		{
			continue;
		}

		if (!IsInArc(Target, CurrentAttack.Reach, CurrentAttack.HalfAngle))
		{
			continue;
		}

		SwingHitFighters.Add(Target);

		const EGutHitResult Result = Target->ReceiveAttack(this, CurrentAttack);

		HandleAttackLanded(Target, Result, CurrentAttack);
	}
}

bool AGutFighter::IsAwaitingTick() const
{
	return LastTickFrame != GFrameCounter;
}

bool AGutFighter::HasAttackDueThisFrame() const
{
	if (!IsAwaitingTick())
	{
		return false;
	}

	// already able to hit, or about to become able to on this frame's turn
	return State == EGutFighterState::Active
		|| (State == EGutFighterState::Windup && StateTime + GetWorld()->GetDeltaSeconds() >= CurrentAttack.Windup);
}

void AGutFighter::FinishAttack()
{
	if (CurrentAttackType == EGutAttackType::Light)
	{
		LastLightAttackEndTime = GetWorld()->GetTimeSeconds();
	}

	SetState(EGutFighterState::Idle);
}

void AGutFighter::ConsumeBufferedAction()
{
	if (BufferedAction == EBufferedAction::None)
	{
		return;
	}

	const EBufferedAction Action = BufferedAction;
	const bool bFresh = GetWorld()->GetTimeSeconds() - BufferedActionTime <= InputBufferTime;

	BufferedAction = EBufferedAction::None;

	if (!bFresh || !bCombatEnabled)
	{
		return;
	}

	switch (Action)
	{
	case EBufferedAction::Light:
		TryLightAttack();
		break;

	case EBufferedAction::Heavy:
		TryHeavyAttack();
		break;

	case EBufferedAction::Dodge:
		StartDodge(BufferedDodgeDirection);
		break;

	default:
		break;
	}
}

void AGutFighter::BufferAction(EBufferedAction Action, const FVector& DodgeDirection)
{
	BufferedAction = Action;
	BufferedActionTime = GetWorld()->GetTimeSeconds();
	BufferedDodgeDirection = DodgeDirection;
}

UAnimSequenceBase* AGutFighter::ChooseReaction(const FGutAttackDef& Attack, const FVector& FromAttacker, bool bOnLimb, EGutLimb Limb) const
{
	const FGutStanceAnimations Clips = GetStanceAnimations();

	UAnimSequenceBase* Reaction = nullptr;

	if (FVector::DotProduct(GetActorForwardVector(), FromAttacker) > 0.35f)
	{
		// the blow is going the way the fighter faces: it came from behind
		Reaction = Clips.HitBack;
	}
	else if (bOnLimb)
	{
		switch (Limb)
		{
		case EGutLimb::LeftArm:
			Reaction = Clips.HitLeftArm;
			break;

		case EGutLimb::RightArm:
			Reaction = Clips.HitRightArm;
			break;

		case EGutLimb::LeftLeg:
			Reaction = Clips.HitLeftLeg;
			break;

		case EGutLimb::RightLeg:
			Reaction = Clips.HitRightLeg;
			break;

		default:
			break;
		}
	}
	else
	{
		switch (Attack.BlowFrom)
		{
		case EGutBlowFrom::Left:
			Reaction = Clips.HitLeft;
			break;

		case EGutBlowFrom::Right:
			Reaction = Clips.HitRight;
			break;

		case EGutBlowFrom::Above:
			Reaction = Clips.HitTop;
			break;

		case EGutBlowFrom::Body:
			Reaction = Clips.HitBody;
			break;

		default:
			break;
		}
	}

	return Reaction ? Reaction : Clips.Stun.Get();
}

void AGutFighter::EnterStun(float Duration, UAnimSequenceBase* Reaction)
{
	// a stun throws away whatever the fighter was doing or about to do
	InterruptAction();

	BufferedAction = EBufferedAction::None;
	LastLightAttackEndTime = -100.0f;

	// a fighter that feels less of it is stopped for less long
	Duration *= StunScale;

	StunDuration = Duration;

	SetState(EGutFighterState::Stunned);

	const FGutStanceAnimations Clips = GetStanceAnimations();

	if (!Reaction)
	{
		Reaction = Clips.Stun;
	}

	if (Reaction && Duration > 0.0f)
	{
		PlaySlotAnimation(Reaction, Clips.StunLength / Duration, 0.0f, 0.05f, 0.2f);
	}
}

bool AGutFighter::ApplyHealthDamage(float Damage)
{
	Health = FMath::Max(0.0f, Health - Damage);

	return Health <= 0.0f;
}

void AGutFighter::Die(AGutFighter* InKiller, const FVector& ImpulseDirection)
{
	if (State == EGutFighterState::Dead)
	{
		return;
	}

	Health = 0.0f;
	CreditedKiller = InKiller;

	InterruptAction();

	// nobody is finishing this fighter any more, and it is done holding things
	Finisher = nullptr;

	DropWeapon();

	bWantsBlock = false;
	BufferedAction = EBufferedAction::None;

	SetState(EGutFighterState::Dead);

	// the body stops being a fighter
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// some deaths are not instant: a throat that is cut, a body the current has hold of. Those are acted out, and the body goes limp after.
	UAnimSequenceBase* DeathClip = nullptr;
	float LimpTime = 0.0f;

	if (bKilledByFinisher && InKiller && InKiller->CurrentFinisher.DeathAnimation)
	{
		DeathClip = InKiller->CurrentFinisher.DeathAnimation;
		LimpTime = InKiller->CurrentFinisher.DeathLimpTime;
	}
	else if (KillingHazard.IsValid() && KillingHazard->Shocks() && Locomotion.DeathShock)
	{
		DeathClip = Locomotion.DeathShock;
		LimpTime = Locomotion.DeathShockLimpTime;
	}

	bBeingTaken = false;
	bLimp = false;

	if (DeathClip && LimpTime > 0.05f && GetMesh()->GetAnimInstance())
	{
		PlaySlotAnimation(DeathClip, 1.0f, 0.0f, 0.1f, 0.0f);

		GetWorldTimerManager().SetTimer(LimpTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { GoLimp(FVector::ZeroVector); }), LimpTime, false);
	}
	else
	{
		LimpTime = 0.0f;

		GoLimp(ImpulseDirection);
	}

	// what is left of it spreads out under the body
	if (UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>())
	{
		Gore->Pool(BloodMaterial, GetActorLocation() + (ImpulseDirection.GetSafeNormal2D() * 60.0f), GutFighterConstants::MinBloodRadius * 2.0f, DeathPoolRadius, DeathPoolSpreadTime);

		// and where it went down it is smeared, and a last spray goes the way it fell
		Gore->Splatter(Gore->GetMark(EGutBloodMark::Smeared, BloodSplashMaterial ? BloodSplashMaterial.Get() : BloodMaterial.Get()), GetActorLocation() + (ImpulseDirection.GetSafeNormal2D() * 25.0f), 55.0f);
		Gore->Spray(BloodSplashMaterial ? BloodSplashMaterial.Get() : BloodMaterial.Get(), GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), ImpulseDirection, 30, 420.0f);
	}

	UE_LOG(LogGut, Log, TEXT("%s is dead, killed by %s%s"), *GetName(), *GetNameSafe(InKiller), KillingHazard.IsValid() ? *FString::Printf(TEXT(" and %s"), *KillingHazard->GetName()) : TEXT(""));

	MakeSound(VoiceSound(TEXT("death")), 1.0f);

	// and the body is heard when it meets the floor
	GetWorldTimerManager().SetTimer(BodyFallTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		const FVector Where = GetMesh()->GetBoneLocation(FName("pelvis"));

		MakeSound(bWading ? FName("splash") : FName("bodyfall"), 1.0f, &Where);
	}), 0.5f + (LimpTime * 0.8f), false);

	HandleDeath(InKiller);

	OnDied.Broadcast(this, InKiller);
}

void AGutFighter::Celebrate(float Delay)
{
	if (State == EGutFighterState::Dead || Locomotion.Victory.IsEmpty())
	{
		return;
	}

	VictoryTries = 0;

	GetWorldTimerManager().SetTimer(VictoryTimer, this, &AGutFighter::TryCelebrate, 0.25f, true, FMath::Max(Delay, 0.05f));
}

void AGutFighter::TryCelebrate()
{
	// a finisher still going is left to end, somebody walking is left to walk, and nobody waits for ever
	const bool bFree = State == EGutFighterState::Idle && GetVelocity().SizeSquared2D() < FMath::Square(30.0f);

	if (State == EGutFighterState::Dead || ++VictoryTries > 40)
	{
		GetWorldTimerManager().ClearTimer(VictoryTimer);
		return;
	}

	if (!bFree)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(VictoryTimer);

	if (UAnimSequenceBase* Clip = Locomotion.Victory[FMath::RandHelper(Locomotion.Victory.Num())])
	{
		PlaySlotAnimation(Clip, 1.0f, 0.0f, 0.3f, 0.4f);

		bCelebrating = true;

		MakeSound(VoiceSound(TEXT("effort")), 1.0f);

		UE_LOG(LogGut, Log, TEXT("%s shows the crowd: %s"), *GetName(), *Clip->GetName());
	}
}

void AGutFighter::PlayThenHold(UAnimSequenceBase* Once, UAnimSequenceBase* Held)
{
	if (!Once)
	{
		return;
	}

	PlaySlotAnimation(Once, 1.0f, 0.0f, 0.3f, 0.3f);

	if (Held)
	{
		// a little before the first runs out, so that the body is never handed back to its stance between the two
		GetWorldTimerManager().SetTimer(HeldClipTimer, FTimerDelegate::CreateWeakLambda(this, [this, Held]()
		{
			if (State != EGutFighterState::Dead && (bYielded || bSparing))
			{
				PlaySlotAnimation(Held, 1.0f, 0.0f, 0.2f, 0.3f, 1000);
			}
		}), FMath::Max(Once->GetPlayLength() - 0.15f, 0.05f), false);
	}
}

void AGutFighter::Yield(float Delay)
{
	if (State == EGutFighterState::Dead || bYielded || Beast || !Locomotion.YieldDown)
	{
		return;
	}

	// from now on it is not getting up again, whatever it does next
	bYielded = true;

	GetWorldTimerManager().SetTimer(YieldTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (State == EGutFighterState::Dead)
		{
			return;
		}

		bBeingTaken = false;

		PlayThenHold(Locomotion.YieldDown, Locomotion.YieldKneel);

		UE_LOG(LogGut, Log, TEXT("%s goes down onto its knees"), *GetName());
	}), FMath::Max(Delay, 0.05f), false);
}

void AGutFighter::Spare(float Delay)
{
	if (State == EGutFighterState::Dead || Beast || !Locomotion.SpareLower)
	{
		return;
	}

	VictoryTries = 0;

	GetWorldTimerManager().SetTimer(SpareTimer, this, &AGutFighter::TrySpare, 0.25f, true, FMath::Max(Delay, 0.05f));
}

void AGutFighter::TrySpare()
{
	// as with showing the crowd: whatever it was in the middle of is left to end first
	const bool bFree = State == EGutFighterState::Idle && GetVelocity().SizeSquared2D() < FMath::Square(30.0f);

	if (State == EGutFighterState::Dead || ++VictoryTries > 40)
	{
		GetWorldTimerManager().ClearTimer(SpareTimer);
		return;
	}

	if (!bFree)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpareTimer);

	bSparing = true;

	// the same flag as for showing the crowd, so that walking off ends it the same way
	bCelebrating = true;

	PlayThenHold(Locomotion.SpareLower, Locomotion.SpareStand);

	UE_LOG(LogGut, Log, TEXT("%s lets its hands drop and stands over the one it has beaten"), *GetName());
}

void AGutFighter::GoLimp(const FVector& ImpulseDirection)
{
	if (bLimp || State != EGutFighterState::Dead)
	{
		return;
	}

	bLimp = true;

	GetWorldTimerManager().ClearTimer(LimpTimer);

	// an animal dies as its own clip has it die
	if (Beast)
	{
		return;
	}

	// an obstacle-free ragdoll, from whatever pose the body was in
	if (GetMesh()->GetSkeletalMeshAsset() && GetMesh()->GetPhysicsAsset())
	{
		GetMesh()->SetCollisionProfileName(FName("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);

		if (!ImpulseDirection.IsNearlyZero())
		{
			GetMesh()->AddImpulse((ImpulseDirection + FVector(0.0f, 0.0f, 0.35f)) * DeathImpulse, NAME_None, true);
		}
	}
}

void AGutFighter::SpendStamina(float Amount)
{
	Stamina = FMath::Max(0.0f, Stamina - Amount);

	LastStaminaUseTime = GetWorld()->GetTimeSeconds();
}

bool AGutFighter::CheckStamina(float Amount)
{
	if (Stamina >= Amount)
	{
		return true;
	}

	// let the HUD show why nothing happened
	LastStaminaFailTime = GetWorld()->GetTimeSeconds();

	return false;
}

void AGutFighter::ApplyPush(const FVector& Direction, float Distance, float Duration)
{
	CancelPush();

	if (Distance <= 1.0f || Duration <= 0.0f)
	{
		return;
	}

	// a constant force root motion source moves the capsule at a set speed and overrides walking while it lasts
	TSharedPtr<FRootMotionSource_ConstantForce> Push = MakeShared<FRootMotionSource_ConstantForce>();

	Push->InstanceName = GutFighterConstants::PushSourceName;
	Push->AccumulateMode = ERootMotionAccumulateMode::Override;
	Push->Priority = 5;
	Push->Force = Direction.GetSafeNormal2D() * (Distance / Duration);
	Push->Duration = Duration;
	Push->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Push->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	GetCharacterMovement()->ApplyRootMotionSource(Push);
}

void AGutFighter::CancelPush()
{
	GetCharacterMovement()->RemoveRootMotionSource(GutFighterConstants::PushSourceName);
}

void AGutFighter::PlaySlotAnimation(UAnimSequenceBase* Animation, float PlayRate, float StartTime, float BlendIn, float BlendOut, int32 Loops)
{
	if (!Animation)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		// the fighter's own animation takes the body up from wherever it was and at the speed it was going, which a cross-fade
		// this short cannot: a fade of a few frames from mid-stride to the start of a blow is a jerk
		if (UGutAnimInstance* Own = Cast<UGutAnimInstance>(AnimInstance))
		{
			Own->SmoothOver(FMath::Clamp(BlendIn * 2.5f, 0.16f, 0.4f));

			BlendIn = 0.0f;
		}

		AnimInstance->PlaySlotAnimationAsDynamicMontage(Animation, AnimSlotName, BlendIn, BlendOut, PlayRate, Loops, -1.0f, StartTime);
	}
}

void AGutFighter::StopSlotAnimation(float BlendOut)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		UGutAnimInstance* Own = Cast<UGutAnimInstance>(AnimInstance);

		if (Own && AnimInstance->IsSlotActive(AnimSlotName))
		{
			Own->SmoothOver(FMath::Clamp(BlendOut * 1.5f, 0.2f, 0.45f));

			BlendOut = 0.0f;
		}

		AnimInstance->StopSlotAnimation(BlendOut, AnimSlotName);
	}
}

void AGutFighter::ShowCombatText(const FString& Text, const FLinearColor& Color)
{
	CombatText = Text;
	CombatTextColor = Color;
	CombatTextTime = GetWorld()->GetTimeSeconds();
}

bool AGutFighter::HasLiveOpponent() const
{
	return Opponent.IsValid() && Opponent->IsAlive();
}

void AGutFighter::HandleAttackLanded(AGutFighter* Target, EGutHitResult Result, const FGutAttackDef& Attack)
{
	LastStrikeResult = Result;

	switch (Result)
	{
	case EGutHitResult::Hit:
		++Tally.Hits;
		break;

	case EGutHitResult::Blocked:
		++Tally.Blocked;
		break;

	case EGutHitResult::GuardBroken:
		++Tally.GuardBreaks;
		break;

	case EGutHitResult::Dodged:
		++Tally.Dodged;
		break;

	default:
		break;
	}

	// what it sounds like depends on what it was thrown with, and, if it was stopped, on what stopped it
	if (Target)
	{
		const FVector Where = Target->GetActorLocation() + FVector(0.0f, 0.0f, 35.0f) + ((GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D() * 25.0f);
		const float Weight = FMath::Clamp(0.55f + (Attack.Damage / 32.0f), 0.6f, 1.5f);

		switch (Result)
		{
		case EGutHitResult::Hit:
			MakeSound(WeaponSound(TEXT("hit"), GetArmament()), Weight, &Where);
			break;

		case EGutHitResult::GuardBroken:
			MakeSound(WeaponSound(TEXT("hit"), GetArmament()), Weight, &Where);
			MakeSound(FName("guard_break"), 1.0f, &Where);
			break;

		case EGutHitResult::Blocked:
			// bare hands on anything but a shield are a thud whatever is held up against them
			MakeSound((!GetArmament() && !(Target->GetArmament() && Target->GetArmament()->bOffHand)) ? FName("block_fists") : WeaponSound(TEXT("block"), Target->GetArmament()), Weight, &Where);
			break;

		default:
			break;
		}
	}

	BP_OnAttackLanded(Target, Result, Attack);

	OnAttackLanded.Broadcast(this, Target, Result);

	// a weapon that has been seen to before the bell lasts until it meets something
	if (bWeaponSabotaged && Weapon && Result != EGutHitResult::Dodged && Result != EGutHitResult::Ignored)
	{
		bWeaponSabotaged = false;

		ShowCombatText(TEXT("IT COMES APART"), GutFighterConstants::BreakTextColor);

		UE_LOG(LogGut, Log, TEXT("%s: the weapon comes apart in its hand"), *GetName());

		DropWeapon();
	}
}

void AGutFighter::HandleAttackReceived(AGutFighter* Attacker, EGutHitResult Result, const FGutAttackDef& Attack)
{
	// show where the blow landed
	if (HitEffect && (Result == EGutHitResult::Hit || Result == EGutHitResult::GuardBroken))
	{
		FVector HitLocation = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

		if (Attacker)
		{
			HitLocation += (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * GetCapsuleComponent()->GetScaledCapsuleRadius();
		}

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, HitLocation);
	}

	// a blow that lands is cried out at, most of the time. One that is stopped is grunted behind, sometimes.
	if (IsAlive() && (Result == EGutHitResult::Hit || Result == EGutHitResult::GuardBroken) && FMath::FRand() < 0.8f)
	{
		MakeSound(VoiceSound(TEXT("pain")), FMath::Clamp(0.6f + (Attack.Damage / 40.0f), 0.6f, 1.3f));
	}
	else if (Result == EGutHitResult::Blocked && FMath::FRand() < 0.25f)
	{
		MakeSound(VoiceSound(TEXT("effort")), 0.5f);
	}

	BP_OnAttackReceived(Attacker, Result, Attack);

	OnAttackReceived.Broadcast(this, Attacker, Result);
}

void AGutFighter::HandleDeath(AGutFighter* InKiller)
{
	BP_OnDied(InKiller);
}

void AGutFighter::HandleStaggered()
{
	BP_OnStaggered();
}

void AGutFighter::EnterStagger()
{
	InterruptAction();

	Health = 0.0f;

	BufferedAction = EBufferedAction::None;
	LastLightAttackEndTime = -100.0f;

	// each stagger is shorter than the last, so a fighter that keeps getting up is harder to put down for good
	CurrentStaggerDuration = FMath::Max(MinStaggerDuration, StaggerDuration * FMath::Pow(StaggerDurationFalloff, StaggerCount));

	++StaggerCount;

	SetState(EGutFighterState::Staggered);

	MakeSound(VoiceSound(TEXT("pain")), 1.2f, nullptr, 0.92f);

	if (const FGutStanceAnimations Clips = GetStanceAnimations(); Clips.Stagger)
	{
		// a loop at its own speed, round and round until the stagger ends or somebody ends it
		PlaySlotAnimation(Clips.Stagger, 1.0f, 0.0f, 0.2f, 0.3f, 100);
	}
	else if (StaggerAnimation)
	{
		PlaySlotAnimation(StaggerAnimation, StaggerAnimationLength / CurrentStaggerDuration, 0.0f, 0.1f, 0.25f);
	}

	UE_LOG(LogGut, Log, TEXT("%s is staggered for %.1f seconds"), *GetName(), CurrentStaggerDuration);

	HandleStaggered();

	OnStaggered.Broadcast(this);
}

void AGutFighter::RecoverFromStagger()
{
	Health = FMath::Min(SecondWindHealth, MaxHealth);
	Stamina = FMath::Max(Stamina, FMath::Min(SecondWindStamina, MaxStamina));

	StopSlotAnimation(0.25f);
	SetState(EGutFighterState::Idle);

	ShowCombatText(TEXT("SECOND WIND"), GutFighterConstants::DodgeTextColor);

	UE_LOG(LogGut, Log, TEXT("%s gets a second wind"), *GetName());

	BP_OnSecondWind();
}

void AGutFighter::StartFinisher(AGutFighter* Target, EGutFinisherStyle Style)
{
	static const FGutFinisherDef BareFinisher;

	const UGutWeaponData* Armament = GetArmament();

	CurrentFinisher = Armament ? Armament->GetFinisher(Style) : BareFinisher;
	CurrentFinisherStyle = Style;
	FinisherTarget = Target;
	bFinisherLanded = false;

	// the target's stagger does not run out while this is going on
	Target->Finisher = this;

	// hands and a shield's rim do not reach as far as a finisher can be started from: the fighter walks the rest first
	bFinisherClosing = CurrentFinisher.Reach > 0.0f && GetGapTo(Target) > CurrentFinisher.Reach;

	SetState(EGutFighterState::Finishing);

	if (!bFinisherClosing)
	{
		BeginFinisherWindup(Target);
	}

	UE_LOG(LogGut, Log, TEXT("%s starts to finish %s: %s%s"), *GetName(), *Target->GetName(), *CurrentFinisher.Label.ToString(), bFinisherClosing ? TEXT(", walking in first") : TEXT(""));

	BP_OnFinisherStarted(Target, Style);
}

void AGutFighter::BeginFinisherWindup(AGutFighter* Target)
{
	// the one it is done to acts out what is being done to it, so that its part of the blow comes as the blow does
	if (CurrentFinisher.TakenAnimation && CurrentFinisher.TakenImpactTime > 0.0f)
	{
		Target->bBeingTaken = true;
		Target->PlaySlotAnimation(CurrentFinisher.TakenAnimation, CurrentFinisher.TakenImpactTime / FMath::Max(CurrentFinisher.Windup, 0.01f), 0.0f, 0.12f, 0.2f);
	}

	// play the placeholder animation so its blow lands as the windup ends
	if (CurrentFinisher.Animation)
	{
		const float AnimWindup = FMath::Max(CurrentFinisher.AnimImpactTime - CurrentFinisher.AnimStartTime, 0.01f);

		PlaySlotAnimation(CurrentFinisher.Animation, AnimWindup / FMath::Max(CurrentFinisher.Windup, 0.01f), CurrentFinisher.AnimStartTime);
	}
}

void AGutFighter::InterruptAction()
{
	CancelPush();
	StopSlotAnimation(0.05f);

	bFinisherClosing = false;

	// a finisher that is cut short lets its target go
	if (AGutFighter* Target = FinisherTarget.Get())
	{
		if (Target->Finisher == this)
		{
			Target->Finisher = nullptr;
		}
	}

	FinisherTarget = nullptr;
}

bool AGutFighter::ResolveLimb(const FGutAttackDef& Attack, const FVector& FromAttacker, EGutLimb& OutLimb) const
{
	// an animal is one body: nothing of it is crippled apart from the rest
	if (Attack.HitZone == EGutHitZone::Torso || Beast)
	{
		return false;
	}

	const bool bArm = Attack.HitZone == EGutHitZone::LeftArm || Attack.HitZone == EGutHitZone::RightArm;
	bool bLeft = Attack.HitZone == EGutHitZone::LeftArm || Attack.HitZone == EGutHitZone::LeftLeg;

	// an attacker standing well round to one side hits that side, whatever the attack would land on from in front
	const float Side = FVector::DotProduct(GetActorRightVector(), -FromAttacker);

	if (FMath::Abs(Side) > FMath::Sin(FMath::DegreesToRadians(SideHitAngle)))
	{
		bLeft = Side < 0.0f;
	}

	if (bArm)
	{
		OutLimb = bLeft ? EGutLimb::LeftArm : EGutLimb::RightArm;
	}
	else
	{
		OutLimb = bLeft ? EGutLimb::LeftLeg : EGutLimb::RightLeg;
	}

	return true;
}

void AGutFighter::ApplyLimbDamage(EGutLimb Limb, float Damage)
{
	if (IsLimbCrippled(Limb) || Damage <= 0.0f)
	{
		return;
	}

	// some weapons shelter the arm that carries them
	if (Weapon && Limb == GetArmFor(Weapon))
	{
		Damage *= Weapon->HeldArmDamageScale;
	}

	float& Remaining = LimbHealth[static_cast<int32>(Limb)];

	Remaining = FMath::Max(0.0f, Remaining - Damage);

	if (Remaining <= 0.0f)
	{
		CrippleLimb(Limb);
	}
}

void AGutFighter::CrippleLimb(EGutLimb Limb)
{
	LimbHealth[static_cast<int32>(Limb)] = 0.0f;

	ShowCombatText(FString::Printf(TEXT("%s CRIPPLED"), GutFighterConstants::LimbNames[static_cast<int32>(Limb)]), GutFighterConstants::BreakTextColor);

	UE_LOG(LogGut, Log, TEXT("%s: %s crippled"), *GetName(), GutFighterConstants::LimbNames[static_cast<int32>(Limb)]);

	SpillBlood(FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f), GutFighterConstants::CrippleBlood);

	MakeSound(FName("bone_break"), 1.0f);

	// a crippled weapon arm lets go
	if (Weapon && Limb == GetArmFor(Weapon))
	{
		DropWeapon();
	}

	// a crippled leg drags
	UpdateMovementForState();

	BP_OnLimbCrippled(Limb);

	OnLimbCrippled.Broadcast(this, Limb);
}

float AGutFighter::GetLegSpeedScale() const
{
	float Scale = 1.0f;

	for (const EGutLimb Leg : { EGutLimb::LeftLeg, EGutLimb::RightLeg })
	{
		if (IsLimbCrippled(Leg))
		{
			Scale *= CrippledLegSpeedScale;
		}
	}

	return Scale;
}

float AGutFighter::GetBlockCost(const FGutAttackDef& Attack) const
{
	float Scale = 1.0f;

	// the guard is held with the arms, so it costs more with fewer of them working
	for (const EGutLimb Arm : { EGutLimb::LeftArm, EGutLimb::RightArm })
	{
		if (IsLimbCrippled(Arm))
		{
			Scale += CrippledArmBlockPenalty;
		}
	}

	// and some things are better to hide behind than others
	if (const UGutWeaponData* Armament = GetArmament())
	{
		Scale *= Armament->BlockCostScale;
	}

	return Attack.BlockStaminaDamage * Scale;
}

void AGutFighter::OnBodyPosed()
{
	FeedFace();
	UpdateChain();
	ListenToFeet();
	MeasureStrike();
}

float AGutFighter::GetStruckDepth() const
{
	// behind a shield, behind a raised guard, or on the chest
	if (IsBlocking())
	{
		return (GetArmament() && GetArmament()->bOffHand) ? 44.0f : 34.0f;
	}

	return 11.0f;
}

bool AGutFighter::GetStrikingPoint(FVector& OutPoint) const
{
	const USkeletalMeshComponent* Seen = GetShownMesh();

	if (!Seen || Beast || (Weapon && Weapon->LinkMesh))
	{
		return false;
	}

	const FVector Way = GetActorForwardVector();

	if (Weapon && Weapon->Mesh && WeaponMesh->IsVisible())
	{
		if (bWeaponLine)
		{
			// a club or a blade: most of the way along it, toward whichever end is furthest out
			const FBox Shape = Weapon->Mesh->GetBoundingBox();
			const FTransform Held = WeaponMesh->GetComponentTransform();

			const FVector Base = Held.TransformPosition(FVector(0.0f, 0.0f, Shape.Min.Z));
			const FVector Tip = Held.TransformPosition(FVector(0.0f, 0.0f, Shape.Max.Z));
			const bool bTipOut = FVector::DotProduct(Tip - Base, Way) >= 0.0f;

			OutPoint = FMath::Lerp(bTipOut ? Base : Tip, bTipOut ? Tip : Base, 0.85f);

			return true;
		}

		if (Weapon->bOffHand)
		{
			// a shield: its face, a little in front of its middle
			OutPoint = WeaponMesh->Bounds.Origin + (Way * 12.0f);

			return true;
		}
	}

	// bare hands, and feet: whichever of them is thrown furthest. The bone is the wrist or the ankle, and what lands is beyond it.
	static const FName Ends[] = { FName("hand_l"), FName("hand_r"), FName("foot_l"), FName("foot_r") };

	float Furthest = -UE_BIG_NUMBER;

	for (int32 i = 0; i < 4; ++i)
	{
		if (Seen->GetBoneIndex(Ends[i]) == INDEX_NONE)
		{
			continue;
		}

		const FVector End = Seen->GetBoneLocation(Ends[i]) + (Way * (i < 2 ? 8.0f : 12.0f));
		const float Out = FVector::DotProduct(End - GetActorLocation(), Way);

		if (Out > Furthest)
		{
			Furthest = Out;
			OutPoint = End;
		}
	}

	return Furthest > -UE_BIG_NUMBER;
}

float AGutFighter::GetBodyGapFrom(const FVector& Point, FName* OutPart) const
{
	const USkeletalMeshComponent* Seen = GetShownMesh();

	if (!Seen)
	{
		return UE_BIG_NUMBER;
	}

	// the body as a few thick lines from joint to joint
	struct FPart
	{
		FName From;
		FName To;
		float Thick;
	};

	static const FPart Parts[] =
	{
		{ FName("pelvis"), FName("spine_03"), 14.0f }, { FName("spine_03"), FName("neck_01"), 14.0f }, { FName("neck_01"), FName("head"), 8.0f },
		{ FName("upperarm_l"), FName("lowerarm_l"), 5.0f }, { FName("lowerarm_l"), FName("hand_l"), 4.0f },
		{ FName("upperarm_r"), FName("lowerarm_r"), 5.0f }, { FName("lowerarm_r"), FName("hand_r"), 4.0f },
		{ FName("thigh_l"), FName("calf_l"), 8.0f }, { FName("calf_l"), FName("foot_l"), 5.0f },
		{ FName("thigh_r"), FName("calf_r"), 8.0f }, { FName("calf_r"), FName("foot_r"), 5.0f },
	};

	float Nearest = UE_BIG_NUMBER;

	for (const FPart& Part : Parts)
	{
		if (Seen->GetBoneIndex(Part.From) == INDEX_NONE || Seen->GetBoneIndex(Part.To) == INDEX_NONE)
		{
			continue;
		}

		FVector From = Seen->GetBoneLocation(Part.From);
		FVector To = Seen->GetBoneLocation(Part.To);

		// the head goes on above the joint it turns on
		if (Part.To == FName("head"))
		{
			To += (To - From).GetSafeNormal() * 14.0f;
		}

		const float Gap = FMath::PointDistToSegment(Point, From, To) - Part.Thick;

		if (Gap < Nearest)
		{
			Nearest = Gap;

			if (OutPart)
			{
				*OutPart = Part.To;
			}
		}
	}

	return Nearest;
}

float AGutFighter::GetArmReach() const
{
	// a chain reaches as far as it is long, less the slack it is thrown with (see UpdateChain), from a hand held out
	if (Weapon && Weapon->LinkMesh)
	{
		return 35.0f + (Weapon->LinkPitch * Weapon->LinkCount * 0.78f);
	}

	// something gripped: an arm bent as it is at the moment of a blow, and most of the thing's length
	if (bWeaponLine && Weapon && Weapon->Mesh)
	{
		return 34.0f + (Weapon->Mesh->GetBoundingBox().GetSize().Z * Weapon->GripTransform.GetScale3D().Z * 0.6f);
	}

	// a shield's face, or a fist
	return (Weapon && Weapon->bOffHand) ? 55.0f : 62.0f;
}

bool AGutFighter::GetStrikeReach(const UAnimSequenceBase* Clip, float& OutReach) const
{
	// a chain reaches as far as it is long, less the slack it is thrown with (see UpdateChain), from a hand held out
	if (Weapon && Weapon->LinkMesh)
	{
		OutReach = 35.0f + (Weapon->LinkPitch * Weapon->LinkCount * 0.78f);
		return true;
	}

	if (!Clip)
	{
		return false;
	}

	if (const float* Own = StrikeReachKnown.Find(Clip))
	{
		OutReach = *Own;
		return true;
	}

	if (const float* Anybodys = GutFighterConstants::StrikeReachKnownToAnybody.Find(Clip->GetFName()))
	{
		OutReach = *Anybodys;
		return true;
	}

	return false;
}

void AGutFighter::UpdateStrikeShift(float DeltaTime)
{
	const bool bCarried = GutFighterConstants::bStrikesSeenToLand;

	if (Beast || !HasBody() || State == EGutFighterState::Dead)
	{
		return;
	}

	const bool bSwinging = StrikeClip && (State == EGutFighterState::Windup || State == EGutFighterState::Active || State == EGutFighterState::Recovery);

	if (!bSwinging)
	{
		// hit out of it, or done: back to where the fighter is
		StrikeClip = nullptr;
		StrikeShift = FMath::FInterpTo(StrikeShift, 0.0f, DeltaTime, 14.0f);
	}
	else if (State == EGutFighterState::Windup)
	{
		const AGutFighter* Target = Opponent.Get();

		float Known = 0.0f;

		StrikeGoal = 0.0f;

		// only a blow the rules will let land is helped to be seen to: one thrown at the air is left short
		if (bCarried && GetStrikeReach(StrikeClip, Known) && Target && Target->IsAlive() && !Target->IsBeast() && GetGapTo(Target) <= CurrentAttack.Reach + 15.0f)
		{
			const float Apart = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());

			StrikeGoal = FMath::Clamp((Apart - Target->GetStruckDepth() + StrikeDepth) - Known, -StrikePullBack, StrikeReachOut);
		}

		StrikeWay = GetActorForwardVector().GetSafeNormal2D();
		StrikeShift = StrikeGoal * FMath::SmoothStep(0.0f, 1.0f, StateTime / FMath::Max(CurrentAttack.Windup, 0.01f));
	}
	else if (State == EGutFighterState::Active)
	{
		StrikeShift = StrikeGoal;
	}
	else
	{
		StrikeShift = StrikeGoal * (1.0f - FMath::SmoothStep(0.0f, 1.0f, StateTime / FMath::Clamp(CurrentAttack.Recovery, 0.05f, 0.35f)));
	}

	GetMesh()->SetRelativeLocation(MeshRestLocation + GetActorQuat().UnrotateVector(StrikeWay * StrikeShift));
}

void AGutFighter::MeasureStrike()
{
	// -GutBlowLog writes down how near every blow came to the body it was thrown at: the nearest any part of the weapon, or
	// the fist or foot, came to its skin from just before the blow is due until just after
	static const bool bBlowLog = FParse::Param(FCommandLine::Get(), TEXT("GutBlowLog"));

	if (!StrikeClip)
	{
		return;
	}

	const bool bDue = State == EGutFighterState::Active || (State == EGutFighterState::Windup && StateTime >= CurrentAttack.Windup - 0.07f) || (State == EGutFighterState::Recovery && StateTime <= 0.1f);

	if (bBlowLog && bDue && Opponent.IsValid())
	{
		const AGutFighter* Target = Opponent.Get();

		FName Part;
		float Gap = UE_BIG_NUMBER;

		if (Weapon && Weapon->LinkMesh)
		{
			for (int32 i = ChainShown.Num() / 2; i < ChainShown.Num(); ++i)
			{
				FName At;
				const float Here = Target->GetBodyGapFrom(ChainShown[i], &At);

				if (Here < Gap)
				{
					Gap = Here;
					Part = At;
				}
			}
		}
		else if (bWeaponLine && Weapon && Weapon->Mesh && WeaponMesh->IsVisible())
		{
			const FBox Shape = Weapon->Mesh->GetBoundingBox();
			const FTransform Held = WeaponMesh->GetComponentTransform();

			// along the half of it that is swung, a hand's width at a time
			for (float Along = 0.4f; Along <= 1.0f; Along += 0.1f)
			{
				FName At;
				const float Here = Target->GetBodyGapFrom(Held.TransformPosition(FVector(0.0f, 0.0f, FMath::Lerp(Shape.Min.Z, Shape.Max.Z, Along))), &At) - WeaponLineRadius;

				if (Here < Gap)
				{
					Gap = Here;
					Part = At;
				}
			}
		}
		else
		{
			FVector End;

			if (GetStrikingPoint(End))
			{
				Gap = Target->GetBodyGapFrom(End, &Part) - 4.0f;
			}
		}

		if (Gap < StrikeNearest)
		{
			StrikeNearest = Gap;
			StrikeNearestPart = Part;
		}
	}

	if (bBlowLog && State == EGutFighterState::Recovery && StateTime > 0.1f && StrikeNearest < UE_BIG_NUMBER && Opponent.IsValid())
	{
		static const TCHAR* Came[] = { TEXT("landed"), TEXT("was blocked"), TEXT("broke the guard"), TEXT("was dodged") };

		UE_LOG(LogGut, Log, TEXT("Blow: %s %s with %s %s: came within %.0f cm of the skin at %s, carried %.0f, fighters %.0f cm apart, arm's reach %.0f"),
			*GetDisplayName().ToString(), *CurrentAttack.Label.ToString(), GetArmament() ? *GetArmament()->GetName() : TEXT("bare hands"),
			SwingHitFighters.Contains(Opponent.Get()) ? Came[FMath::Clamp(static_cast<int32>(LastStrikeResult), 0, 3)] : TEXT("missed"),
			StrikeNearest, *StrikeNearestPart.ToString(), StrikeGoal, FVector::Dist2D(GetActorLocation(), Opponent->GetActorLocation()), GetArmReach());

		StrikeNearest = UE_BIG_NUMBER;
	}

	if (bStrikeMeasured || State != EGutFighterState::Active)
	{
		return;
	}

	bStrikeMeasured = true;

	FVector End;

	if (!GetStrikingPoint(End))
	{
		return;
	}

	// how far out this body's blow ends by its own motion: what the body was carried is taken off
	const float Out = FVector::DotProduct(End - GetActorLocation(), StrikeWay) - StrikeShift;

	// every body's arm is its own length. The first time a body throws a blow it goes by what the last body to throw it found.
	float& Known = StrikeReachKnown.FindOrAdd(StrikeClip, GutFighterConstants::StrikeReachKnownToAnybody.FindRef(StrikeClip->GetFName()));

	Known = Known == 0.0f ? Out : FMath::Lerp(Known, Out, 0.5f);

	GutFighterConstants::StrikeReachKnownToAnybody.Add(StrikeClip->GetFName(), Known);
}

void AGutFighter::Bleed(float DeltaTime)
{
	if (Beast || State == EGutFighterState::Dead || BleedInterval <= 0.0f)
	{
		return;
	}

	Wounds.RemoveAll([](const UDecalComponent* Mark) { return !IsValid(Mark); });

	if (Wounds.IsEmpty())
	{
		return;
	}

	BleedIn -= DeltaTime;

	if (BleedIn > 0.0f)
	{
		return;
	}

	// the more it has been opened up, and the nearer it is to finished, the faster it drips
	const float Rate = (Wounds.Num() / 2.5f) * (Health < MaxHealth * 0.35f ? 2.0f : 1.0f);

	BleedIn = FMath::Clamp(BleedInterval / Rate, 0.3f, BleedInterval) * FMath::FRandRange(0.6f, 1.4f);

	if (UGutGoreSubsystem* Gore = GetWorld()->GetSubsystem<UGutGoreSubsystem>())
	{
		const FVector Under = GetActorLocation() + FVector(FMath::FRandRange(-22.0f, 22.0f), FMath::FRandRange(-22.0f, 22.0f), 0.0f);

		Gore->Splatter(Gore->GetMark(EGutBloodMark::Dripped, BloodSplashMaterial ? BloodSplashMaterial.Get() : BloodMaterial.Get()), Under, FMath::FRandRange(3.5f, 8.0f));
	}
}

void AGutFighter::UpdateChain()
{
	if (!ChainLinks || !Weapon || !Weapon->LinkMesh || !WeaponMesh->IsVisible())
	{
		return;
	}

	const FTransform Hold = WeaponMesh->GetComponentTransform();
	const float Size = Hold.GetScale3D().Z;
	const float Pitch = Weapon->LinkPitch * Size;
	const int32 Count = Weapon->LinkCount;

	// the chain is tied here
	const FVector Anchor = Hold.TransformPosition(FVector(0.0f, 0.0f, Weapon->LinkStart));

	const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (0.6f * Weapon->LinkPitch * Size);

	// a new chain, or a fighter that was just put somewhere else: let it hang from the hand and start from rest
	if (ChainJoints.Num() != Count + 1 || FVector::DistSquared(ChainJoints[0], Anchor) > FMath::Square(150.0f))
	{
		ChainJoints.SetNum(Count + 1);

		for (int32 i = 0; i <= Count; ++i)
		{
			ChainJoints[i] = Anchor - FVector(0.0f, 0.0f, Pitch * i);

			// what does not fit between the hand and the floor lies along the floor behind the fighter
			if (ChainJoints[i].Z < Floor)
			{
				ChainJoints[i] = FVector(Anchor.X, Anchor.Y, Floor) - (GetActorForwardVector() * (Floor - ChainJoints[i].Z));
			}
		}

		ChainJointsBefore = ChainJoints;
		ChainJointsWere = ChainJoints;
		ChainAnchorBefore = Anchor;
	}

	// A chain is not a stick. Nothing holds it straight: it hangs from the hand, trails behind a swing and is flung out by one,
	// as its own weight has it. What a fighter does do is throw the far end at what it means to hit, so for the moment of a blow
	// the far end, and only that, is drawn toward the place the rules say the blow lands, and never so far that the chain is
	// pulled tight: it arcs out to its mark with slack in it, and drops.
	float Throw = 0.0f;
	FVector Mark = FVector::ZeroVector;

	if (State == EGutFighterState::Windup || State == EGutFighterState::Active || State == EGutFighterState::Recovery)
	{
		switch (State)
		{
		case EGutFighterState::Windup:
			Throw = FMath::SmoothStep(0.6f, 1.0f, StateTime / FMath::Max(CurrentAttack.Windup, 0.01f));
			break;

		case EGutFighterState::Active:
			Throw = 1.0f;
			break;

		default:
			Throw = 1.0f - FMath::Clamp(StateTime / 0.05f, 0.0f, 1.0f);
			break;
		}

		// low for a blow at the legs, at the ribs for the rest
		const bool bLow = CurrentAttack.HitZone == EGutHitZone::LeftLeg || CurrentAttack.HitZone == EGutHitZone::RightLeg;
		const float Height = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * (bLow ? -0.5f : 0.25f);

		const AGutFighter* Target = Opponent.Get();

		if (HasLiveOpponent() && GetGapTo(Target) <= CurrentAttack.Reach + 60.0f)
		{
			Mark = Target->GetActorLocation() + FVector(0.0f, 0.0f, Height);
		}
		else
		{
			Mark = GetActorLocation() + (GetActorForwardVector() * CurrentAttack.Reach) + FVector(0.0f, 0.0f, Height);
		}
	}
	else if (State == EGutFighterState::Finishing && FinisherTarget.IsValid())
	{
		// a chain that finishes somebody goes round the neck
		Throw = FMath::SmoothStep(0.2f, 0.6f, StateTime / FMath::Max(CurrentFinisher.Windup, 0.01f));

		const AGutFighter* Target = FinisherTarget.Get();
		const USkeletalMeshComponent* Seen = Target->GetShownMesh();

		Mark = (Seen && Seen->GetBoneIndex(FName("neck_01")) != INDEX_NONE) ? Seen->GetBoneLocation(FName("neck_01")) : Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
	}

	// the mark is never further from the hand than most of the chain's length, or the chain would be drawn out as straight as a rod
	if (Throw > 0.0f)
	{
		const FVector Out = Mark - Anchor;
		const float Most = Pitch * Count * (State == EGutFighterState::Finishing ? 0.95f : 0.8f);

		if (Out.SizeSquared() > FMath::Square(Most))
		{
			Mark = Anchor + (Out.GetSafeNormal() * Most);
		}
	}

	// neither fighter is something a chain passes through: it lies against a leg or a back, and wraps what it is thrown round.
	// Each is its legs, its trunk and its neck to the chain, as thick as those are.
	struct FLimb
	{
		FVector From;
		FVector To;
		float Radius;
	};

	TArray<FLimb, TInlineAllocator<12>> Limbs;

	auto AddBody = [&Limbs](const AGutFighter* Who)
	{
		static const TTuple<FName, FName, float> Parts[] =
		{
			MakeTuple(FName("thigh_l"), FName("calf_l"), 9.0f),
			MakeTuple(FName("calf_l"), FName("foot_l"), 6.5f),
			MakeTuple(FName("thigh_r"), FName("calf_r"), 9.0f),
			MakeTuple(FName("calf_r"), FName("foot_r"), 6.5f),
			MakeTuple(FName("pelvis"), FName("spine_05"), 15.0f),
			MakeTuple(FName("spine_05"), FName("head"), 8.5f),
		};

		const USkeletalMeshComponent* Seen = Who ? Who->GetShownMesh() : nullptr;

		if (!Seen)
		{
			return;
		}

		for (const TTuple<FName, FName, float>& Part : Parts)
		{
			if (Seen->GetBoneIndex(Part.Get<0>()) != INDEX_NONE && Seen->GetBoneIndex(Part.Get<1>()) != INDEX_NONE)
			{
				Limbs.Add({ Seen->GetBoneLocation(Part.Get<0>()), Seen->GetBoneLocation(Part.Get<1>()), Part.Get<2>() + 2.0f });
			}
		}
	};

	AddBody(this);

	if (const AGutFighter* Other = Opponent.Get(); Other && Other->IsAlive())
	{
		AddBody(Other);
	}

	// small steps, so that a slow frame does not make the chain any stretchier than a fast one, with the hand moved a little in each
	const float DeltaTime = FMath::Min(GetWorld()->GetDeltaSeconds(), 0.05f);
	const int32 Steps = DeltaTime > 0.0f ? FMath::Clamp(FMath::CeilToInt32(DeltaTime * 300.0f), 2, 16) : 0;
	const float Step = Steps > 0 ? DeltaTime / Steps : 0.0f;

	const FVector Fall(0.0f, 0.0f, GetWorld()->GetGravityZ() * Step * Step);
	const float Air = FMath::Exp(-1.6f * Step);
	const float Drag = FMath::Exp(-10.0f * Step);

	float Jolt = 0.0f;
	bool bStruckFloor = false;

	for (int32 StepIndex = 0; StepIndex < Steps; ++StepIndex)
	{
		ChainJoints[0] = FMath::Lerp(ChainAnchorBefore, Anchor, static_cast<float>(StepIndex + 1) / Steps);

		for (int32 i = 1; i <= Count; ++i)
		{
			// carries on as it was going, a little slower, and falls. On the floor it drags.
			FVector Going = (ChainJoints[i] - ChainJointsBefore[i]) * Air;

			if (ChainJoints[i].Z <= Floor + 0.2f)
			{
				Going.X *= Drag;
				Going.Y *= Drag;
			}

			ChainJointsBefore[i] = ChainJoints[i];
			ChainJoints[i] += Going + Fall;
		}

		if (Throw > 0.0f)
		{
			// the outer half goes with the end, the end most
			const FVector Off = Mark - ChainJoints[Count];
			const int32 From = Count / 2;

			for (int32 i = From + 1; i <= Count; ++i)
			{
				const float Share = FMath::Square(static_cast<float>(i - From) / (Count - From));

				ChainJoints[i] += Off * (1.0f - FMath::Exp(-Throw * Share * 16.0f * Step));
			}
		}

		// links do not stretch, and they do not go through the floor or through a body
		for (int32 Pass = 0; Pass < 6; ++Pass)
		{
			for (int32 i = 1; i <= Count; ++i)
			{
				FVector Link = ChainJoints[i] - ChainJoints[i - 1];
				const float Length = Link.Size();

				if (Length > UE_KINDA_SMALL_NUMBER)
				{
					Link *= (Length - Pitch) / Length;

					if (i == 1)
					{
						ChainJoints[i] -= Link;
					}
					else if (i == Count)
					{
						// the hook is the heavy end: it gives less than the link it hangs on
						ChainJoints[i] -= Link * 0.3f;
						ChainJoints[i - 1] += Link * 0.7f;
					}
					else
					{
						ChainJoints[i] -= Link * 0.5f;
						ChainJoints[i - 1] += Link * 0.5f;
					}
				}

				for (const FLimb& Limb : Limbs)
				{
					const FVector Out = ChainJoints[i] - FMath::ClosestPointOnSegment(ChainJoints[i], Limb.From, Limb.To);
					const float Far = Out.Size();

					if (Far < Limb.Radius && Far > UE_KINDA_SMALL_NUMBER)
					{
						ChainJoints[i] += Out * ((Limb.Radius - Far) / Far);
					}
				}

				if (ChainJoints[i].Z < Floor)
				{
					bStruckFloor |= (ChainJointsBefore[i].Z - ChainJoints[i].Z) > 900.0f * Step && ChainJointsBefore[i].Z > Floor + 0.5f;

					ChainJoints[i].Z = Floor;
				}
			}
		}

		// how hard the chain was jerked this step: the change in the way its far end is going
		Jolt = FMath::Max(Jolt, FVector::Dist(ChainJoints[Count] - ChainJointsBefore[Count], ChainJointsBefore[Count] - ChainJointsWere[Count]) / FMath::Max(Step, UE_SMALL_NUMBER));

		ChainJointsWere = ChainJointsBefore;
	}

	ChainAnchorBefore = Anchor;

	ChainNoise(Jolt, bStruckFloor);

	// what is drawn is the chain with every link exactly its own length, measured out from the hand
	ChainShown = ChainJoints;

	for (int32 i = 1; i <= Count; ++i)
	{
		const FVector Way = (ChainShown[i] - ChainShown[i - 1]).GetSafeNormal();

		ChainShown[i] = ChainShown[i - 1] + ((Way.IsNearlyZero() ? FVector::DownVector : Way) * Pitch);
		ChainShown[i].Z = FMath::Max(ChainShown[i].Z, Floor);
	}

	// a link lies along the line between two joints, every other one turned a quarter turn as the links of a chain are.
	// Which way its flat faces is carried along the chain from the hand, so that no link flips as the chain swings.
	const FVector LinkSize = FVector(Weapon->LinkScale * Size);

	FVector Last = Hold.GetUnitAxis(EAxis::Z);
	FVector Flat = Hold.GetUnitAxis(EAxis::X);

	for (int32 i = 1; i <= Count; ++i)
	{
		const FVector Way = (ChainShown[i] - ChainShown[i - 1]).GetSafeNormal();

		if (!Way.IsNearlyZero())
		{
			Flat = FQuat::FindBetweenNormals(Last, Way).RotateVector(Flat);
			Last = Way;
		}

		const FQuat Turn = FRotationMatrix::MakeFromZX(Last, Flat).ToQuat() * FQuat(FVector::ZAxisVector, (i % 2) * UE_HALF_PI);

		ChainLinks->UpdateInstanceTransform(i - 1, FTransform(Turn, (ChainShown[i] + ChainShown[i - 1]) * 0.5f, LinkSize), true, i == Count, true);
	}

	if (ChainEnd)
	{
		ChainEnd->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZX(Last, Flat).ToQuat(), ChainShown[Count], FVector(Size)));
	}
}

void AGutFighter::ChainNoise(float Jolt, bool bStruckFloor)
{
	// a chain that is jerked rattles, the harder the louder. One left to hang says nothing.
	if (Jolt > 2500.0f && ChainShown.Num() > 1)
	{
		const FVector Middle = ChainShown[ChainShown.Num() / 2];

		MakeSound(FName("chain_rattle"), FMath::Clamp(Jolt / 9000.0f, 0.3f, 1.2f), &Middle);
	}

	if (bStruckFloor && ChainShown.Num() > 0)
	{
		MakeSound(FName("chain_floor"), 0.8f, &ChainShown.Last());
	}
}

void AGutFighter::MakeSound(FName Kind, float Volume, const FVector* Where, float Pitch) const
{
	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(Kind, Where ? *Where : GetActorLocation() + FVector(0.0f, 0.0f, 30.0f), Volume, Pitch);
	}
}

FName AGutFighter::WeaponSound(const TCHAR* Prefix, const UGutWeaponData* ForWeapon)
{
	static const FName Fists(TEXT("fists"));

	const FName What = (ForWeapon && !ForWeapon->SoundKind.IsNone()) ? ForWeapon->SoundKind : Fists;

	return FName(*FString::Printf(TEXT("%s_%s"), Prefix, *What.ToString()));
}

FName AGutFighter::VoiceSound(const TCHAR* Prefix) const
{
	return FName(*FString::Printf(TEXT("%s_%s"), Prefix, *Voice.ToString()));
}

void AGutFighter::ListenToFeet()
{
	static const FName Feet[2] = { FName("foot_l"), FName("foot_r") };

	const USkeletalMeshComponent* Seen = GetShownMesh();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	if (!Seen || DeltaTime <= 0.0f || !IsAlive() || Beast)
	{
		return;
	}

	const float FloorHeight = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// -GutFootLog writes down every footfall and what the foot was doing, to find out what is heard and why
	static const bool bFootLog = FParse::Param(FCommandLine::Get(), TEXT("GutFootLog"));

	// a body can be posed more than once in a frame, and the second time no foot has moved: that is not a foot coming to rest
	if (FeetHeardFrame == GFrameCounter)
	{
		return;
	}

	FeetHeardFrame = GFrameCounter;

	// a foot on the ground under a body that is walking somewhere slides a little, the faster the body the more, and the foot
	// in the air goes about twice as fast as the body: so for somebody at ease what counts as moving and as come to rest goes
	// up with the body's speed. In a fight the feet are listened to as they always were.
	const float Going = FMath::Max(GetVelocity().Size2D(), LedVelocity.Size2D());
	const float FastEnough = bAtEase ? FMath::Max(110.0f, Going * 1.3f) : 110.0f;
	const float SlowEnough = bAtEase ? FMath::Max(40.0f, Going * 0.5f) : 40.0f;

	// somebody at ease who has stopped has stopped: feet finding their place under a body that stands still are not footsteps
	const bool bHeard = !bAtEase || Going > 20.0f;

	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Now = Seen->GetBoneLocation(Feet[i]);
		const float Moved = FVector::Dist(Now, FootBefore[i]);
		const float Speed = Moved / DeltaTime;

		FootBefore[i] = Now;

		// put somewhere else, not walked there: no foot goes a metre in a frame or twenty-five in a second
		if (Moved > 100.0f || Speed > 2500.0f)
		{
			bFootGoing[i] = false;
			FootFastest[i] = 0.0f;
			continue;
		}

		if (Speed > FastEnough)
		{
			bFootGoing[i] = true;
			FootFastest[i] = FMath::Max(FootFastest[i], Speed);
		}
		else if (bFootGoing[i] && Speed < SlowEnough && Now.Z - FloorHeight < 22.0f)
		{
			if (bFootLog)
			{
				UE_LOG(LogGut, Log, TEXT("Foot: %s %s at %.3f, %s, fastest %.0f, now %.0f cm/s, %.1f cm up, body going %.0f cm/s"), *GetName(), i == 0 ? TEXT("left") : TEXT("right"),
					GetWorld()->GetTimeSeconds(), bHeard ? TEXT("heard") : TEXT("not heard"), FootFastest[i], Speed, Now.Z - FloorHeight, Going);
			}

			if (bHeard)
			{
				// it has come down and stopped: the faster it came, the harder it lands
				const FVector On(Now.X, Now.Y, FloorHeight);

				UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this);

				// tile in the pool, and whatever the ground is anywhere else
				MakeSound(bWading ? FName("wade") : (Sounds ? Sounds->GetFootfall() : FName("step")), FMath::Clamp(FootFastest[i] / 420.0f, 0.15f, 1.0f), &On);
			}

			bFootGoing[i] = false;
			FootFastest[i] = 0.0f;
		}
	}
}

void AGutFighter::FeedFace()
{
	UAnimInstance* Driver = Body ? Body->GetAnimInstance() : nullptr;

	if (!Driver)
	{
		return;
	}

	if (!IsAlive())
	{
		// the dead do not glare: the lids come down and the jaw goes slack
		static const FName LeftLid(TEXT("CTRL_expressions_eyeBlinkL"));
		static const FName RightLid(TEXT("CTRL_expressions_eyeBlinkR"));
		static const FName Jaw(TEXT("CTRL_expressions_jawOpen"));

		Driver->AddCurveValue(LeftLid, DeadEyesClosed);
		Driver->AddCurveValue(RightLid, DeadEyesClosed);
		Driver->AddCurveValue(Jaw, 0.25f);

		return;
	}

	// what the face has to say: a blow shows on it at once and wears off, and being beaten comes over it slowly and stays
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	const bool bHurt = FaceHurt && State == EGutFighterState::Stunned;
	const bool bAfraid = FaceAfraid && State == EGutFighterState::Staggered;

	FaceHurtShown = FMath::FInterpTo(FaceHurtShown, bHurt ? 1.0f : 0.0f, DeltaTime, bHurt ? 30.0f : 4.0f);
	FaceAfraidShown = FMath::FInterpTo(FaceAfraidShown, bAfraid ? (bBeingTaken ? 1.0f : 0.75f) : 0.0f, DeltaTime, 5.0f);

	// each fighter starts somewhere else in its loops, so two faces do not blink together
	const double Clock = GetWorld()->GetTimeSeconds() + (GetUniqueID() % 16);

	auto Show = [Driver, Clock](const UAnimSequenceBase* Loop, float Weight)
	{
		const float Length = Loop ? Loop->GetPlayLength() : 0.0f;

		if (Length <= 0.0f || Weight <= 0.01f)
		{
			return;
		}

		FBlendedCurve Expression;
		Loop->EvaluateCurveData(Expression, FAnimExtractContext(FMath::Fmod(Clock, static_cast<double>(Length))), false);

		Expression.ForEachElement([Driver, Weight](const UE::Anim::FCurveElement& Element)
		{
			Driver->AddCurveValue(Element.Name, Element.Value * Weight);
		});
	};

	// the three share the face between them
	const float Hurt = FaceHurtShown;
	const float Afraid = FaceAfraidShown * (1.0f - Hurt);

	Show(FaceLoop, 1.0f - Hurt - Afraid);
	Show(FaceHurt, Hurt);
	Show(FaceAfraid, Afraid);
}

bool AGutFighter::GetWeaponLine(FVector& OutBase, FVector& OutTip, float& OutRadius) const
{
	OutBase = WeaponLineBase;
	OutTip = WeaponLineTip;
	OutRadius = WeaponLineRadius;

	return bWeaponLine;
}

float AGutFighter::GetWeaponNeckGap(bool bOnFightingSkeleton) const
{
	const USkeletalMeshComponent* Shown = bOnFightingSkeleton ? GetMesh() : GetShownMesh();

	if (!bWeaponLine || !Shown || !Weapon || !Weapon->Mesh || !WeaponMesh->IsVisible())
	{
		return -1.0f;
	}

	const FBox Shape = Weapon->Mesh->GetBoundingBox();

	// on the fighting skeleton nothing is held: the weapon is where it would be if it were in that skeleton's hand the same way
	const FTransform Held = bOnFightingSkeleton
		? (Weapon->GripTransform * GetMesh()->GetSocketTransform(WeaponSocketName, RTS_ParentBoneSpace)) * GetMesh()->GetBoneTransform(GetMesh()->GetSocketBoneName(WeaponSocketName))
		: WeaponMesh->GetComponentTransform();

	const FVector Base = Held.TransformPosition(FVector(0.0f, 0.0f, Shape.Min.Z));
	const FVector Tip = Held.TransformPosition(FVector(0.0f, 0.0f, Shape.Max.Z));

	const FVector Neck = Shown->GetBoneLocation(FName("neck_01"));
	const FVector Head = Shown->GetBoneLocation(FName("head"));
	const FVector Crown = Head + ((Head - Neck).GetSafeNormal() * 12.0f);

	FVector OnWeapon;
	FVector OnNeck;

	FMath::SegmentDistToSegmentSafe(Base, Tip, Neck, Crown, OnWeapon, OnNeck);

	return FVector::Dist(OnWeapon, OnNeck);
}

void AGutFighter::UpdateWeaponMesh()
{
	// a thing gripped in the fist is a line from the hand, which the fighter's animation keeps pointed and out of its own neck.
	// A shield is strapped on, a chain hangs, and bare hands hold nothing.
	bWeaponLine = Weapon && Weapon->Mesh && !Weapon->bOffHand && !Weapon->bStrapToForearm && !Weapon->LinkMesh;

	if (bWeaponLine)
	{
		const FBox Shape = Weapon->Mesh->GetBoundingBox();
		const FTransform InHand = Weapon->GripTransform * GetMesh()->GetSocketTransform(WeaponSocketName, RTS_ParentBoneSpace);

		// its length is its own Z
		WeaponLineBase = InHand.TransformPosition(FVector(0.0f, 0.0f, Shape.Min.Z));
		WeaponLineTip = InHand.TransformPosition(FVector(0.0f, 0.0f, Shape.Max.Z));
		WeaponLineRadius = FMath::Clamp(0.5f * FMath::Min(Shape.GetSize().X, Shape.GetSize().Y) * Weapon->GripTransform.GetScale3D().X, 1.5f, 4.0f);
	}

	if (Weapon && Weapon->Mesh)
	{
		// the shape goes in whichever hand carries this weapon
		const FName Socket = Weapon->bOffHand ? OffHandSocketName : WeaponSocketName;

		// a chain in the hand is its handle, with its links hung from it
		const bool bChain = Weapon->LinkMesh && Weapon->HandleMesh && FApp::CanEverRender();

		WeaponMesh->SetStaticMesh(bChain ? Weapon->HandleMesh.Get() : Weapon->Mesh.Get());
		WeaponMesh->SetMaterial(0, Weapon->Material);

		if (bChain && !ChainLinks)
		{
			ChainLinks = NewObject<UInstancedStaticMeshComponent>(this);
			ChainLinks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ChainLinks->SetReceivesDecals(false);
			ChainLinks->SetUsingAbsoluteLocation(true);
			ChainLinks->SetUsingAbsoluteRotation(true);
			ChainLinks->SetUsingAbsoluteScale(true);
			ChainLinks->SetupAttachment(RootComponent);
			ChainLinks->RegisterComponent();

			ChainEnd = NewObject<UStaticMeshComponent>(this);
			ChainEnd->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ChainEnd->SetReceivesDecals(false);
			ChainEnd->SetUsingAbsoluteLocation(true);
			ChainEnd->SetUsingAbsoluteRotation(true);
			ChainEnd->SetUsingAbsoluteScale(true);
			ChainEnd->SetupAttachment(RootComponent);
			ChainEnd->RegisterComponent();
		}

		if (ChainLinks)
		{
			ChainLinks->ClearInstances();
			ChainLinks->SetVisibility(bChain);
			ChainEnd->SetVisibility(bChain && Weapon->EndMesh);
			ChainJoints.Reset();

			if (bChain)
			{
				ChainLinks->SetStaticMesh(Weapon->LinkMesh);
				ChainEnd->SetStaticMesh(Weapon->EndMesh);

				for (int32 i = 0; i < Weapon->LinkCount; ++i)
				{
					ChainLinks->AddInstance(FTransform::Identity, true);
				}
			}
		}

		USkeletalMeshComponent* Shown = GetShownMesh();

		const FName HandBone = GetMesh()->GetSocketBoneName(Socket);
		const FName ArmBone = Shown->GetParentBone(HandBone);
		const USkinnedAsset* Asset = Shown->GetSkinnedAsset();

		if (Weapon->bStrapToForearm && Asset && ArmBone != NAME_None && Shown->GetBoneIndex(HandBone) != INDEX_NONE)
		{
			// strapped to the forearm. Where that is, and which side of it faces away from the body, is read from the body standing at rest,
			// so it comes out the same whatever the fighter is doing when it picks the thing up.
			const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
			const FTransform Arm = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Skeleton.FindBoneIndex(ArmBone));
			const FTransform Hand = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Skeleton.FindBoneIndex(HandBone));

			const FVector Along = Arm.InverseTransformVectorNoScale(Hand.GetLocation() - Arm.GetLocation());
			const FVector Outward = Arm.InverseTransformVectorNoScale(FVector(FMath::Sign(Arm.GetLocation().X), 0.0f, 0.0f));

			// the shape's own top is its Z, and its face is whatever its grip turns toward X
			const FVector Face = Weapon->GripTransform.GetRotation().UnrotateVector(FVector::XAxisVector);
			// top toward the elbow, so it reads the right way up on an arm that hangs
			const FQuat Turn = FRotationMatrix::MakeFromZX(-Along, Outward).ToQuat() * FRotationMatrix::MakeFromZX(FVector::ZAxisVector, Face).ToQuat().Inverse();

			WeaponMesh->AttachToComponent(Shown, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ArmBone);
			WeaponMesh->SetRelativeTransform(FTransform(Turn, (Along * 0.55f) + (Outward.GetSafeNormal() * 7.0f), Weapon->GripTransform.GetScale3D()));
		}
		else if (HasBody())
		{
			// the fighter's own character has the same bones but not the placeholder's grip sockets: hold it the same way on the same bone
			WeaponMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GetMesh()->GetSocketBoneName(Socket));
			WeaponMesh->SetRelativeTransform(Weapon->GripTransform * GetMesh()->GetSocketTransform(Socket, RTS_ParentBoneSpace));
		}
		else
		{
			WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			WeaponMesh->SetRelativeTransform(Weapon->GripTransform);
		}

		WeaponMesh->SetVisibility(true);
	}
	else
	{
		WeaponMesh->SetVisibility(false);

		if (ChainLinks)
		{
			ChainLinks->SetVisibility(false);
			ChainEnd->SetVisibility(false);
		}
	}

	// the hand that holds it closes round it, and opens again when it is empty
	if (HasBody())
	{
		FRetargetProfile Profile;

		if (Weapon && Weapon->Mesh)
		{
			Profile.bApplyTargetRetargetPose = true;
			Profile.TargetRetargetPoseName = Weapon->bOffHand ? GripPoseBoth : GripPoseRight;
		}
		else if (!bAtEase)
		{
			// bare hands are fists
			Profile.bApplyTargetRetargetPose = true;
			Profile.TargetRetargetPoseName = GripPoseBoth;
		}

		BodyRetarget->SetCustomRetargetProfile(Profile);
	}
}
