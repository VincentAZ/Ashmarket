// Ashmarket prototype: the Gut.

#include "GutFighter.h"
#include "Components/LODSyncComponent.h"
#include "GutBeast.h"
#include "EngineUtils.h"
#include "GutOpponentController.h"
#include "GutAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "GutCameraManager.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
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
#include "GutClearOp.h"
#include "RetargetComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogGut);

namespace GutFighterConstants
{
	/** How many of a seen body's levels of detail are ever used, finest first: see where the body is made */
	// Three: at the fourth a bought jacket's sleeves are left standing out from arms that hang, and at the third they are
	// not (looked at on the yard hand with each forced, 23 November). It was two for a day, which took Block Square from
	// about 75 frames a second to 46: fifty-six bystanders posed at the second level. -GutBodyLods=N tries another.
	const int32 BodyLevelsOfDetail = []() { int32 Levels = 3; FParse::Value(FCommandLine::Get(), TEXT("GutBodyLods="), Levels); return FMath::Clamp(Levels, 1, 8); }();

	/** Name shared by every push so a new one always replaces the last */
	const FName PushSourceName("GutPush");

	/** Time a knockback takes */
	const float KnockbackDuration = 0.15f;

	/** Gap a lunge leaves between the two capsules */
	const float LungeStopGap = 10.0f;

	/** -GutNoStrikeShift leaves every blow where its own clip and the old step put it, to compare */
	const bool bStrikesSeenToLand = !FParse::Param(FCommandLine::Get(), TEXT("GutNoStrikeShift"));

	// blows are taken the rest of the way onto the body they are thrown at: from this long before one is due until this long
	// after, from no further off than this, this much of what is left each time the body is posed, and back this fast after
	const bool bBlowsSteered = !FParse::Param(FCommandLine::Get(), TEXT("GutNoBlowSteer"));
	const float BlowSteerLead = 0.14f;
	const float BlowSteerAfter = 0.05f;
	const float BlowSteerMost = 36.0f;
	const float BlowSteerGain = 0.7f;
	const float BlowSteerBack = 10.0f;

	// a blow that lands stops both fighters for a moment: how nearly, and for how long by how hard it was
	const bool bBlowsStop = !FParse::Param(FCommandLine::Get(), TEXT("GutNoBlowStop"));
	const float BlowStopRate = 0.04f;
	const float BlowStopLeast = 0.045f;
	const float BlowStopMost = 0.11f;
	const float BlowStopPerDamage = 0.0028f;
	const float BlowStopBlocked = 0.035f;

	// arms at ease hang by the body's own sides: the wrist this far from the hip, counted as hanging if it is below the hips
	// by this and within this of them, the arm brought in or let out this fast and no further than these
	const bool bArmsHang = !FParse::Param(FCommandLine::Get(), TEXT("GutNoArmHang"));
	// (the bodies' own shapes are fuller than the bodies: a wrist that reads as this far from them is beside the hip)
	// the deepest a dressed trunk can be at the ribs, in cm, for hands clasped in front of it to meet: Bo in his vest is 43, Jorge 41
	const float DeepestToClasp = 33.5f;

	const float ArmHangGap = []() { float Gap = 2.5f; FParse::Value(FCommandLine::Get(), TEXT("GutArmGap="), Gap); return Gap; }();
	const float ArmHangBelow = 4.0f;
	const float ArmHangNear = 30.0f;
	const float ArmHangGain = 22.0f;
	const float ArmHangMostIn = 30.0f;
	const float ArmHangMostOut = 10.0f;

	// finishers that two people performed together are played as pairs; -GutNoPairedFinishers plays each as one body acting on another, as before
	const bool bPairedFinishers = !FParse::Param(FCommandLine::Get(), TEXT("GutNoPairedFinishers"));

	// Everybody's own arms, hands and what they hold are kept out of their own body as it is dressed (FGutClearOp, which
	// runs on the body that is seen). -GutNoArmClear leaves them where the clips have them.
	const bool bArmsClear = !FParse::Param(FCommandLine::Get(), TEXT("GutNoArmClear"));

	// how far an upper arm may lie into a trunk that is skin or a shirt, in cm: -GutArmGive=0 holds every arm wholly clear
	const float ArmGive = []() { float Give = 2.5f; FParse::Value(FCommandLine::Get(), TEXT("GutArmGive="), Give); return Give; }();

	// armor is made to the body that wears it, this far clear of it all round; -GutArmorOneSize is the one size it was
	const bool bArmorFitted = !FParse::Param(FCommandLine::Get(), TEXT("GutArmorOneSize"));
	const float ArmorClearOfChest = []() { float Clear = 2.2f; FParse::Value(FCommandLine::Get(), TEXT("GutArmorClear="), Clear); return Clear; }();
	const float ArmorClearOfLimb = 1.3f;

	// and with a piece round the chest the arms hang this much further from the body's sides than they do without
	const float ArmHangClearOfChest = []() { float Clear = 5.0f; FParse::Value(FCommandLine::Get(), TEXT("GutArmorArmGap="), Clear); return Clear; }();

	// the furthest the seen body is lowered onto the floor and raised out of it, and how fast it is brought there
	const float MostLowered = 14.0f;
	const float MostRaised = 12.0f;
	const float GroundRate = 16.0f;

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

	BoundArms = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Gut/Art/Motion/crowd_watch_hands_behind.crowd_watch_hands_behind")));

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

	Tie = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tie"));
	Tie->SetupAttachment(RootComponent);
	Tie->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tie->SetVisibility(false);

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

	// Somebody stood at ease as they were made has their clothes begin as they hang on somebody standing, and not as they
	// were thrown by the body being posed for the first time: twice, a few frames apart, once the body has been posed
	if (bAtEase && BodyActor && ClothSettles < 2 && ++ClothFrames >= (ClothSettles == 0 ? 3 : 14))
	{
		static const bool bLeave = FParse::Param(FCommandLine::Get(), TEXT("GutNoClothSettle"));

		for (UActorComponent* Component : bLeave ? TSet<UActorComponent*>() : BodyActor->GetComponents())
		{
			if (USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component))
			{
				Part->ForceClothNextUpdateTeleportAndReset();
			}
		}

		++ClothSettles;
	}

	// with a body of its own the chain is moved when that body has been posed, which is later in the frame than this
	if (!HasBody())
	{
		UpdateChain();
		UpdateTie();
		ListenToFeet();
	}

	// a blow's stop is over by the world's clock, not the fighter's own, which is all but stopped
	if (BlowStopEnds > 0.0f && GetWorld()->GetTimeSeconds() >= BlowStopEnds)
	{
		BlowStopEnds = 0.0f;
		CustomTimeDilation = 1.0f;
	}

	UpdateCarried(DeltaTime);
	UpdateStrikeShift(DeltaTime);
	UpdateStanding(DeltaTime);
	UpdateFidget(DeltaTime);
	Bleed(DeltaTime);

	// tied wrists: where those of the body that is seen have come to, across the body, and so how much further to bring them
	if (BoundClip)
	{
		static const FName LeftHand(TEXT("hand_l"));
		static const FName RightHand(TEXT("hand_r"));

		static const FName LeftShoulder(TEXT("upperarm_l"));
		static const FName RightShoulder(TEXT("upperarm_r"));

		if (const USkeletalMeshComponent* Seen = GetShownMesh(); Seen && Seen->GetBoneIndex(LeftHand) != INDEX_NONE && Seen->GetBoneIndex(RightHand) != INDEX_NONE
			&& Seen->GetBoneIndex(LeftShoulder) != INDEX_NONE && Seen->GetBoneIndex(RightShoulder) != INDEX_NONE)
		{
			// across the body is from one shoulder to the other, whichever way the body is turned
			const FVector Across = (Seen->GetBoneLocation(RightShoulder) - Seen->GetBoneLocation(LeftShoulder)).GetSafeNormal2D();
			const float Apart = FVector::DotProduct(Seen->GetBoneLocation(RightHand) - Seen->GetBoneLocation(LeftHand), Across);

			BoundPast = FMath::Clamp(BoundPast + ((Apart + BoundCross) * 0.5f * FMath::Min(DeltaTime * 8.0f, 1.0f)), -10.0f, 20.0f);

			// -GutBoundLog on the command line writes down, four times a second, where the tied wrists are and what is being done about it
			static const bool bBoundLog = FParse::Param(FCommandLine::Get(), TEXT("GutBoundLog"));

			if (bBoundLog && GFrameCounter % 15 == 0)
			{
				UE_LOG(LogGut, Log, TEXT("[bound] %s: the seen wrists are %.1f cm apart across the body (%.1f in all), the fighting skeleton's %.1f; each is taken %.1f past the middle"),
					*GetName(), Apart, FVector::Dist(Seen->GetBoneLocation(RightHand), Seen->GetBoneLocation(LeftHand)),
					FVector::DotProduct(GetMesh()->GetBoneLocation(RightHand) - GetMesh()->GetBoneLocation(LeftHand), Across), BoundPast);
			}
		}
	}

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
				PlaySlotAnimation(Vary(Clips, Clips.Stagger), 1.0f, 0.0f, 0.2f, 0.3f, 100);
			}
		}

		// the clock stops while somebody is in the middle of finishing this fighter
		if (!Finisher.IsValid() && StateTime >= CurrentStaggerDuration)
		{
			RecoverFromStagger();
		}
		else if (bBeingTaken && !CarriedClip)
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

		if (Target && !bFinisherLanded && CurrentFinisher.PairApart <= 0.0f)
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
			if (Target && CurrentFinisher.PairApart > 0.0f)
			{
				GetCapsuleComponent()->IgnoreActorWhenMoving(Target, false);
			}

			FinisherTarget = nullptr;

			EndCarried();
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

	if (!bExerting && Stamina < MaxStamina && Now - LastStaminaUseTime >= StaminaRegenDelay * StaminaRegenDelayScale)
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

bool AGutFighter::MeasureRound(const FName& Bone, const FName& TowardBone, float Share, float Reach, FVector& OutToward, FVector& OutFront, FBox2D& OutBody, float Below, float PastShoulders,
	const AActor* Dressed, const USkeletalMeshComponent* DressedBody) const
{
	// its own body, or one it is handed: somebody in a crowd is measured the same way
	const AActor* Measured = Dressed ? Dressed : BodyActor.Get();
	const USkeletalMeshComponent* Shown = Dressed ? DressedBody : GetShownMesh();
	const USkeletalMesh* Model = (Measured && Shown) ? Shown->GetSkeletalMeshAsset() : nullptr;

	if (!Model)
	{
		return false;
	}

	const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();

	const int32 From = Skeleton.FindBoneIndex(Bone);
	const int32 To = Skeleton.FindBoneIndex(TowardBone);

	if (From == INDEX_NONE || To == INDEX_NONE)
	{
		return false;
	}

	// everything here is the body standing as it was made, arms away from its sides: nothing it is doing now comes into it
	const FTransform Rest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, From);
	const FVector Start = Rest.GetLocation();
	const FVector Line = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, To).GetLocation() - Start;
	const float Length = Line.Size();

	if (Length < 1.0f)
	{
		return false;
	}

	// the frame the piece is laid in: along the stretch, the body's front (the mesh faces +Y), and across
	const FVector Along = Line / Length;
	const FVector Front = (FVector::YAxisVector - (Along * FVector::DotProduct(FVector::YAxisVector, Along))).GetSafeNormal();
	const FVector Across = FVector::CrossProduct(Front, Along);

	if (Front.IsNearlyZero())
	{
		return false;
	}

	const float Lowest = (Length * (0.5f - (Share * 0.5f))) - Below;
	const float Highest = Length * (0.5f + (Share * 0.5f));

	// A chest is measured between the arms: no further out than the shoulder joints, so that the arms themselves are not
	// taken for ribs. A limb is measured on its own side of the body, so that one thigh is not taken for part of the other.
	const bool bTrunk = FMath::Abs(Start.X) < 3.0f;

	float Widest = Reach;

	if (bTrunk)
	{
		if (const int32 Shoulder = Skeleton.FindBoneIndex(FName("upperarm_l")); Shoulder != INDEX_NONE)
		{
			Widest = FMath::Abs(FVector::DotProduct(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Shoulder).GetLocation() - Start, Across)) + PastShoulders;
		}
	}

	TArray<float> Sideways;
	TArray<float> Forward;

	for (UActorComponent* Component : Measured->GetComponents())
	{
		const USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);
		const USkeletalMesh* Shape = Part ? Part->GetSkeletalMeshAsset() : nullptr;

		// the body and what it is dressed in: not the head, and not hair
		if (!Shape || !Part->IsVisible() || Part->GetFName() == FaceComponentName || Shape->GetName().Contains(TEXT("Hair")) || Shape->GetName().Contains(TEXT("Beard")))
		{
			continue;
		}

		if (bMeasureBodyOnly && !Dressed && Part->GetFName() != BodyComponentName)
		{
			continue;
		}

		const FSkeletalMeshRenderData* Data = Shape->GetResourceForRendering();

		if (!Data)
		{
			continue;
		}

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
				const FVector Point(Points.VertexPosition(i));
				const FVector Off = Point - Start;
				const float Up = FVector::DotProduct(Off, Along);

				if (Up < Lowest || Up > Highest || (!bTrunk && Point.X * Start.X < 0.0f))
				{
					continue;
				}

				const float Side = FVector::DotProduct(Off, Across);
				const float Fore = FVector::DotProduct(Off, Front);

				if (bTrunk ? FMath::Abs(Side) > Widest : FMath::Square(Side) + FMath::Square(Fore) > FMath::Square(Reach))
				{
					continue;
				}

				Sideways.Add(Side);
				Forward.Add(Fore);
			}

			break;
		}
	}

	if (Sideways.Num() < 40)
	{
		return false;
	}

	// all but the odd point that stands out of it: a strap end, a pocket flap
	Sideways.Sort();
	Forward.Sort();

	const int32 Trim = Sideways.Num() / 100;

	OutBody = FBox2D(FVector2D(Sideways[Trim], Forward[Trim]), FVector2D(Sideways[Sideways.Num() - 1 - Trim], Forward[Forward.Num() - 1 - Trim]));

	// said in the bone's own space, as the piece is fixed to the bone
	OutToward = Rest.GetRotation().UnrotateVector(Line);
	OutFront = Rest.GetRotation().UnrotateVector(Front);

	return true;
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

	bChestPiece = false;
	ChestPieceHalf = FVector2D::ZeroVector;

	using namespace GutFighterConstants;

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

				float Width = PlateDef.Thickness * Piece->ModelWidthScale;
				float Depth = Width * Piece->ModelDepthShare;

				FVector Line = Toward;
				FVector Faces = Front;
				FVector2D Middle = FVector2D::ZeroVector;

				// Made to the body that wears it. One size was put on everybody: a vest 54 cm across on a woman 28 cm across
				// the ribs stood out past her arms, and they hung through its sides.
				const bool bChest = Piece->Slot == EGutArmorSlot::Torso;

				FVector MeasuredToward, MeasuredFront;
				FBox2D Round(ForceInit);

				if (bArmorFitted && MeasureRound(PlateDef.Bone, PlateDef.TowardBone, PlateDef.LengthShare, bChest ? Width : PlateDef.Thickness * 0.62f, MeasuredToward, MeasuredFront, Round))
				{
					const float Clear = bChest ? ArmorClearOfChest : ArmorClearOfLimb;

					Line = MeasuredToward;
					Faces = MeasuredFront;
					Middle = Round.GetCenter();

					UE_LOG(LogGut, Log, TEXT("%s: %s on %s is made %.0f cm across and %.0f deep for a body %.0f by %.0f there (it was %.0f by %.0f)"), *GetName(), *Piece->GetName(),
						*PlateDef.Bone.ToString(), Round.GetSize().X + (Clear * 2.0f), Round.GetSize().Y + (Clear * 2.0f), Round.GetSize().X, Round.GetSize().Y, Width, Depth);

					Width = Round.GetSize().X + (Clear * 2.0f);
					Depth = Round.GetSize().Y + (Clear * 2.0f);
				}

				bChestPiece = bChestPiece || bChest;

				if (bChest)
				{
					ChestPieceHalf = FVector2D(Width, Depth) * 0.5f;
				}

				const FVector Scale(Line.Size() * PlateDef.LengthShare / Size.X, Width / Size.Y, Depth / Size.Z);
				const FQuat Turn = FRotationMatrix::MakeFromXZ(Line, Faces).ToQuat();

				// its middle on the middle of the body there, which is not always on the line between the bones
				const FVector Off = Turn.RotateVector(FVector(0.0f, Middle.X, Middle.Y));

				Plate->SetRelativeTransform(FTransform(Turn, (Line * 0.5f) + Off - Turn.RotateVector(Bounds.GetCenter() * Scale), Scale));
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
			PlaySlotAnimation(Vary(Clips, Clips.BlockHit), 1.3f, 0.0f, 0.05f, 0.2f);
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

		bTrunkMeasured = false;
		Trunk.Reset();

		BodyActor = GetWorld()->SpawnActor<AActor>(Class, GetMesh()->GetComponentTransform(), Params);

		// Never drawn coarser than the second of its levels of detail. Coarser than that the body stops posing the small
		// bones along its arms, and a bought jacket, which is one mesh at every distance and hangs its sleeves on those
		// bones, is left with its sleeves standing out as it was modelled while the arms hang (22 November: "his clothes are
		// off"). On a screen twice as wide as it is tall a body a few metres off already counted as small enough for that.
		if (BodyActor)
		{
			for (UActorComponent* Component : BodyActor->GetComponents())
			{
				if (ULODSyncComponent* Sync = Cast<ULODSyncComponent>(Component))
				{
					Sync->NumLODs = GutFighterConstants::BodyLevelsOfDetail;
					Sync->RefreshSyncComponents();
				}
			}
		}
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

void AGutFighter::WearBag(UStaticMesh* Bag)
{
	USkeletalMeshComponent* Shown = GetShownMesh();

	static const FName Pelvis(TEXT("pelvis"));
	static const FName LeftThigh(TEXT("thigh_l"));

	const USkinnedAsset* Asset = Shown ? Shown->GetSkinnedAsset() : nullptr;

	if (!Bag || !Asset || Shown->GetBoneIndex(Pelvis) == INDEX_NONE || Shown->GetBoneIndex(LeftThigh) == INDEX_NONE)
	{
		if (BagMesh)
		{
			BagMesh->SetVisibility(false);
		}

		return;
	}

	if (!BagMesh)
	{
		BagMesh = NewObject<UStaticMeshComponent>(this, TEXT("Bag"));
		BagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BagMesh->SetReceivesDecals(false);
		BagMesh->SetupAttachment(Shown, Pelvis);
		BagMesh->RegisterComponent();
	}

	static float Out = 1.0f;
	static float Down = 22.0f;
	static float Round = 32.0f;
	static float Tip = 4.0f;
	static float Thin = 0.62f;
	static bool bRead = false;

	if (!bRead)
	{
		bRead = true;

		FParse::Value(FCommandLine::Get(), TEXT("GutBagOut="), Out);
		FParse::Value(FCommandLine::Get(), TEXT("GutBagDown="), Down);
		FParse::Value(FCommandLine::Get(), TEXT("GutBagRound="), Round);
		FParse::Value(FCommandLine::Get(), TEXT("GutBagTip="), Tip);
		FParse::Value(FCommandLine::Get(), TEXT("GutBagThin="), Thin);
	}

	// Where the hip is and which way is out from it, from the body standing at rest. It hangs behind the hip, not beside
	// it: beside it is where the hand hangs, and the hand went into the bag. Round is how far round toward the back, in
	// degrees from straight out to the side. The bag's own face is its +Y and its top its +Z: the face is turned outward,
	// the top up, and its back lies against the body. It is made a little thinner than it was modelled, which was fat.
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	const FTransform Hips = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Skeleton.FindBoneIndex(Pelvis));
	const FTransform Thigh = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Skeleton.FindBoneIndex(LeftThigh));

	const FVector Side(FMath::Sign(Thigh.GetLocation().X - Hips.GetLocation().X), 0.0f, 0.0f);
	const FVector Ahead = FVector::CrossProduct(FVector::UpVector, Side) * Side.X;
	const float Turned = FMath::DegreesToRadians(Round);
	const FVector Outward = ((Side * FMath::Cos(Turned)) - (Ahead * FMath::Sin(Turned))).GetSafeNormal();

	const float Deep = Bag->GetBoundingBox().GetSize().Y * 0.5f * Thin;
	const float HipOut = FMath::Abs(Thigh.GetLocation().X - Hips.GetLocation().X);

	// (the hips are wider than they are deep: about a hand's width past the joint to the side, and less than that behind)
	const FVector At = Hips.GetLocation() + (Side * FMath::Cos(Turned) * (HipOut + 8.0f)) - (Ahead * FMath::Sin(Turned) * 13.0f) + (Outward * (Deep + Out)) - FVector(0.0f, 0.0f, Down);
	const FQuat Turn = FQuat(FVector::CrossProduct(FVector::UpVector, Outward), FMath::DegreesToRadians(Tip)) * FRotationMatrix::MakeFromYZ(Outward, FVector::UpVector).ToQuat();

	BagMesh->SetStaticMesh(Bag);
	BagMesh->AttachToComponent(Shown, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Pelvis);
	BagMesh->SetRelativeTransform(FTransform(Turn, At, FVector(1.0f, Thin, 1.0f)) * Hips.Inverse());
	BagMesh->SetVisibility(true);
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
	const float Pace = bAtEase ? (bRunning ? AtEaseRunSpeed : (bJogging ? AtEaseJogSpeed : AtEaseWalkSpeed)) : WalkSpeed;

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
	bRunning = false;

	// One of the families of standing, where there are any: the one asked for, or one dealt by the body's own name from those
	// that suit it. Whoever was given a single clip of their own where they were placed keeps that.
	Family = INDEX_NONE;

	if (bAtEase && !AtEaseFamilies.IsEmpty() && (!bIdlePlaced || !FamilyAsked.IsNone()))
	{
		Family = AtEaseFamilies.IndexOfByPredicate([this](const FGutStandFamily& One) { return One.Name == FamilyAsked && !One.Loops.IsEmpty(); });

		if (Family == INDEX_NONE)
		{
			TArray<int32> Dealt;

			for (int32 i = 0; i < AtEaseFamilies.Num(); ++i)
			{
				if ((AtEaseFamilies[i].DealtTo & (bWoman ? 2 : 1)) != 0 && !AtEaseFamilies[i].Loops.IsEmpty())
				{
					Dealt.Add(i);
				}
			}

			Family = Dealt.IsEmpty() ? INDEX_NONE : Dealt[GetTypeHash(GetName()) % Dealt.Num()];
		}

		FamilyLoop = INDEX_NONE;
		FamilyLeft = 0.0f;

		KeepFromClasping();

		if (Family != INDEX_NONE)
		{
			UpdateStanding(0.0f);
		}
	}

	// somebody who was given no way of standing where they were placed takes one of the common ones, and keeps it
	if (bAtEase && Family == INDEX_NONE && !bIdlePlaced && !AtEaseIdles.IsEmpty())
	{
		if (UAnimSequenceBase* Idle = AtEaseIdles[GetTypeHash(GetName()) % AtEaseIdles.Num()])
		{
			AtEaseStance.Idle = Idle;
		}
	}

	if (bAtEase)
	{
		SetCombatEnabled(false);
	}

	// its pace, and what its empty hands do
	UpdateMovementForState();
	UpdateWeaponMesh();
}

void AGutFighter::SetBound(bool bNewBound)
{
	BoundClip = bNewBound ? BoundArms.LoadSynchronous() : nullptr;
	BoundPast = 2.0f;

	// and what its hands do
	UpdateWeaponMesh();
}

const FGutLocomotionAnimations& AGutFighter::GetFightLocomotion() const
{
	const UGutWeaponData* Armament = GetArmament();

	if (!Armament || !Armament->Moves.WalkForward.Animation)
	{
		return Locomotion;
	}

	if (HeldLocomotionFor.Get() != Armament)
	{
		HeldLocomotionFor = Armament;
		HeldLocomotion = Locomotion;

		HeldLocomotion.WalkForward = Armament->Moves.WalkForward;
		HeldLocomotion.WalkRight = Armament->Moves.WalkRight;
		HeldLocomotion.WalkBack = Armament->Moves.WalkBack;
		HeldLocomotion.WalkLeft = Armament->Moves.WalkLeft;
		HeldLocomotion.RunForward = Armament->Moves.RunForward;
		HeldLocomotion.RunRight = Armament->Moves.RunRight;
		HeldLocomotion.RunBack = Armament->Moves.RunBack;
		HeldLocomotion.RunLeft = Armament->Moves.RunLeft;

		// The fighter's own steps round on the spot are stepped from its own stance, which this weapon is not held in: turned
		// with them, the feet changed places. Without them a foot is lifted and put down again as the body comes round.
		HeldLocomotion.TurnLeft = HeldLocomotion.TurnRight = HeldLocomotion.TurnLeftFar = HeldLocomotion.TurnRightFar = FGutTurnClip();
	}

	return HeldLocomotion;
}

void AGutFighter::SetRunning(bool bNewRunning)
{
	if (bRunning != bNewRunning)
	{
		bRunning = bNewRunning;

		UpdateMovementForState();
	}
}

void AGutFighter::SetAtEaseFamily(const FName& Name)
{
	FamilyAsked = Name;

	if (bAtEase)
	{
		// taken up at once
		bAtEase = false;
		SetAtEase(true);
	}
}

void AGutFighter::KeepFromClasping()
{
	static const FName Clasp(TEXT("clasp"));
	static const FName Plain(TEXT("stand"));

	if (!AtEaseFamilies.IsValidIndex(Family) || AtEaseFamilies[Family].Name != Clasp || !HasBody())
	{
		return;
	}

	if (!bTrunkMeasured)
	{
		MeasureTrunk();
	}

	// Hands clasped in front are held where the performer's arms reach, in front of the belt. A body that is deep through
	// the trunk, with a belly or a vest of plates, has nowhere in front of it that both its arms reach: each hand stopped
	// short on its own thigh, and the Warden at the Block stood with his fists in his groin ("do you think this stance is
	// unnatural?", 24 November: yes). Such a body stands plainly instead.
	if (!Trunk.IsEmpty() && Trunk.Last().Half.Y * 2.0f > GutFighterConstants::DeepestToClasp)
	{
		if (const int32 Stand = AtEaseFamilies.IndexOfByPredicate([](const FGutStandFamily& One) { return One.Name == Plain && !One.Loops.IsEmpty(); }); Stand != INDEX_NONE)
		{
			UE_LOG(LogGut, Log, TEXT("%s: %.0f cm deep through the trunk as dressed, too deep to clasp hands in front of: stands plainly"), *GetName(), Trunk.Last().Half.Y * 2.0f);

			Family = Stand;
			FamilyLoop = INDEX_NONE;
			FamilyLeft = 0.0f;
		}
	}
}

void AGutFighter::UpdateStanding(float DeltaTime)
{
	// a body that was not there to be measured when its way of standing was chosen is looked at again once it is
	if (bAtEase && !bTrunkMeasured)
	{
		KeepFromClasping();
	}

	if (!bAtEase || !AtEaseFamilies.IsValidIndex(Family))
	{
		return;
	}

	const FGutStandFamily& Mine = AtEaseFamilies[Family];

	FamilyLeft -= DeltaTime;

	if (FamilyLeft > 0.0f || Mine.Loops.IsEmpty())
	{
		return;
	}

	// the next of them, by the body's own name and the time, and not the one it has just done
	FRandomStream Draw(GetTypeHash(GetName()) + FMath::FloorToInt32(GetWorld()->GetTimeSeconds() * 10.0f));

	int32 Next = Draw.RandRange(0, Mine.Loops.Num() - 1);

	if (Next == FamilyLoop && Mine.Loops.Num() > 1)
	{
		Next = (Next + 1) % Mine.Loops.Num();
	}

	FamilyLoop = Next;

	if (UAnimSequenceBase* Clip = Mine.Loops[Next])
	{
		AtEaseStance.Idle = Clip;

		// a short one is gone round until six seconds are up; the change to the next begins a little before the end
		const float Length = FMath::Max(Clip->GetPlayLength(), 0.5f);

		FamilyLeft = (Length * FMath::CeilToFloat(6.0f / Length)) - 0.4f;

		// the first of all is come upon part way through, so that a street of people is not all at the start of something
		if (DeltaTime <= 0.0f)
		{
			FamilyLeft *= Draw.FRandRange(0.3f, 1.0f);
		}
	}
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

	// thrown one of the ways there are of throwing it: not the way the last blow was thrown
	static const bool bOneWay = FParse::Param(FCommandLine::Get(), TEXT("GutOneWay"));

	if (!bOneWay && !Attack.Others.IsEmpty())
	{
		for (int32 Try = 0; Try < 4; ++Try)
		{
			const int32 Which = FMath::RandHelper(Attack.Others.Num() + 1);
			const UAnimSequenceBase* Chosen = Which == 0 ? Attack.Animation.Get() : Attack.Others[Which - 1].Animation.Get();

			if (!Chosen || Chosen == LastThrown.Get())
			{
				continue;
			}

			if (Which > 0)
			{
				CurrentAttack.Animation = Attack.Others[Which - 1].Animation;
				CurrentAttack.AnimStartTime = Attack.Others[Which - 1].AnimStartTime;
				CurrentAttack.AnimImpactTime = Attack.Others[Which - 1].AnimImpactTime;
			}

			break;
		}
	}

	LastThrown = CurrentAttack.Animation.Get();
	CurrentAttack.Others.Reset();

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
		PlaySlotAnimation(Vary(Clips, Step), StepLength / DodgeDuration, 0.0f, 0.05f, 0.15f);
	}
	else if (Clips.Dodge)
	{
		PlaySlotAnimation(Vary(Clips, Clips.Dodge), Clips.DodgeLength / DodgeDuration, 0.0f, 0.05f, 0.15f);
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

UAnimSequenceBase* AGutFighter::Vary(const FGutStanceAnimations& Clips, UAnimSequenceBase* Clip) const
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutOneWay"));

	if (!Clip || bOff)
	{
		return Clip;
	}

	for (const FGutOtherWays& Other : Clips.Others)
	{
		if (Other.For != Clip || Other.Ways.IsEmpty())
		{
			continue;
		}

		// one of them all, the first among them, and not what was given last
		for (int32 Try = 0; Try < 4; ++Try)
		{
			const int32 Which = FMath::RandHelper(Other.Ways.Num() + 1);

			UAnimSequenceBase* Chosen = Which == 0 ? Clip : Other.Ways[Which - 1].Get();

			if (Chosen && Chosen != LastVaried.Get())
			{
				LastVaried = Chosen;

				return Chosen;
			}
		}
	}

	return Clip;
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

	return Vary(Clips, Reaction ? Reaction : Clips.Stun.Get());
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

	// Killed in the middle of something two people performed together, it goes on in its own half of it, down to the floor
	const bool bGoesOn = bKilledByFinisher && InKiller && InKiller->CurrentFinisher.PairApart > 0.0f && CarriedClip;
	const float GoesOnFor = bGoesOn ? FMath::Max(InKiller->CurrentFinisher.DeathLimpTime - CarriedTime, 0.1f) : 0.0f;

	TObjectPtr<UAnimSequence> GoingOnIn = bGoesOn ? CarriedClip : nullptr;
	const float GoneThrough = CarriedTime;
	const FVector GoingFrom = CarriedFrom;
	const FVector GoingSlip = CarriedSlip;
	const FVector GoingRoot = CarriedRootAtStart;
	const FQuat GoingTurn = CarriedTurn;

	if (bGoesOn)
	{
		CancelPush();
	}
	else
	{
		InterruptAction();
	}

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

	if (bGoesOn)
	{
		// nothing that was done on the way here has stopped it: taken along as before, and limp when its clip has it down
		CarriedClip = GoingOnIn;
		CarriedTime = GoneThrough;
		CarriedFrom = GoingFrom;
		CarriedSlip = GoingSlip;
		CarriedRootAtStart = GoingRoot;
		CarriedTurn = GoingTurn;
		bCarriedPose = true;

		LimpTime = GoesOnFor;

		GetWorldTimerManager().SetTimer(LimpTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { GoLimp(FVector::ZeroVector); }), LimpTime, false);
	}
	else if (DeathClip && LimpTime > 0.05f && GetMesh()->GetAnimInstance())
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

	// the way of whoever holds this, if there is one: somebody with a blade in the hand does not win as a boxer does
	const UGutWeaponData* Held = GetArmament();
	const TArray<TObjectPtr<UAnimSequenceBase>>& Ways = (Held && !Held->Stance.Victory.IsEmpty()) ? Held->Stance.Victory : Locomotion.Victory;

	if (UAnimSequenceBase* Clip = Ways[FMath::RandHelper(Ways.Num())])
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

	EndCarried();

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

	// whatever is played next is not carried unless it is begun as such
	EndCarried();
	bCarriedPose = false;

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

	// what makes a blow look as if it met something: the two of them stop for an instant where it met
	if (Target && (Result == EGutHitResult::Hit || Result == EGutHitResult::Blocked || Result == EGutHitResult::GuardBroken))
	{
		const float Stop = Result == EGutHitResult::Blocked ? GutFighterConstants::BlowStopBlocked
			: FMath::Clamp(Attack.Damage * GutFighterConstants::BlowStopPerDamage, GutFighterConstants::BlowStopLeast, GutFighterConstants::BlowStopMost);

		// Everybody in the fight stops, not only the two the blow was between. Held alone, they gave whoever else was in
		// the cage that instant for nothing: the Kennel Master's dog, the other two of three. Twelve runs of the ladder
		// each way on 21 November: the Kennel Master took 118 tries with only the two held, and 58 with nobody held.
		for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
		{
			if (It->IsCombatEnabled())
			{
				It->StopForBlow(Stop);
			}
		}

		// and whoever is watching feels it
		AGutCameraManager::JarViewers(this, Result == EGutHitResult::Blocked ? 0.2f : FMath::Clamp(Attack.Damage / 34.0f, 0.3f, 1.0f));
	}

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
		PlaySlotAnimation(Vary(Clips, Clips.Stagger), 1.0f, 0.0f, 0.2f, 0.3f, 100);
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
	// One that two people performed together is begun from as far apart as they stood: nearer than that the two are stood
	// off as it begins, further and the fighter walks in.
	// (two people performed it: it is not done to a dog, or by one)
	if (CurrentFinisher.PairApart > 0.0f && GutFighterConstants::bPairedFinishers && CurrentFinisher.Animation && CurrentFinisher.TakenAnimation && !Beast && !Target->IsBeast())
	{
		CurrentFinisher.Reach = FMath::Max(CurrentFinisher.PairApart - Target->GetCapsuleComponent()->GetScaledCapsuleRadius() + 4.0f, 1.0f);
	}
	else
	{
		CurrentFinisher.PairApart = 0.0f;
	}

	bFinisherClosing = CurrentFinisher.Reach > 0.0f && GetGapTo(Target) > CurrentFinisher.Reach;

	SetState(EGutFighterState::Finishing);

	if (!bFinisherClosing)
	{
		BeginFinisherWindup(Target);
	}

	UE_LOG(LogGut, Log, TEXT("%s starts to finish %s: %s%s"), *GetName(), *Target->GetName(), *CurrentFinisher.Label.ToString(), bFinisherClosing ? TEXT(", walking in first") : TEXT(""));

	BP_OnFinisherStarted(Target, Style);
}

void AGutFighter::BeginCarried(UAnimSequenceBase* Clip, const FVector& From)
{
	UAnimSequence* Performed = Cast<UAnimSequence>(Clip);

	if (!Performed)
	{
		return;
	}

	PlaySlotAnimation(Performed, 1.0f, 0.0f, 0.15f, 0.25f);

	CarriedClip = Performed;
	CarriedTime = 0.0f;
	CarriedFrom = FVector(From.X, From.Y, GetActorLocation().Z);
	CarriedSlip = GetActorLocation() - CarriedFrom;
	CarriedRootAtStart = Performed->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr).GetLocation();
	CarriedTurn = GetMesh()->GetComponentQuat();
	bCarriedPose = true;
}

void AGutFighter::UpdateCarried(float DeltaTime)
{
	if (!CarriedClip)
	{
		// the pose is held to its place until the clip has faded right out
		if (bCarriedPose)
		{
			const UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

			if (!AnimInstance || !AnimInstance->IsSlotActive(AnimSlotName))
			{
				bCarriedPose = false;
			}
		}

		return;
	}

	CarriedTime = FMath::Min(CarriedTime + DeltaTime, CarriedClip->GetPlayLength());

	// where the performer had got to by now, laid out from where this body began
	const FVector Gone = CarriedClip->ExtractRootTrackTransform(FAnimExtractContext(static_cast<double>(CarriedTime)), nullptr).GetLocation() - CarriedRootAtStart;
	const FVector Slip = CarriedSlip * (1.0f - FMath::SmoothStep(0.0f, 0.35f, CarriedTime));
	const FVector Place = CarriedFrom + CarriedTurn.RotateVector(FVector(Gone.X, Gone.Y, 0.0f)) + FVector(Slip.X, Slip.Y, 0.0f);

	// somebody alive is stopped by a wall; a body is put where its clip has it
	SetActorLocation(FVector(Place.X, Place.Y, GetActorLocation().Z), IsAlive());

	if (CarriedTime >= CarriedClip->GetPlayLength())
	{
		EndCarried();
	}
}

void AGutFighter::EndCarried()
{
	CarriedClip = nullptr;
}

void AGutFighter::BeginFinisherWindup(AGutFighter* Target)
{
	// One that two people performed together. The two are stood as far apart as the performers stood, facing, and each is
	// played as it was performed and taken along its own clip.
	if (CurrentFinisher.PairApart > 0.0f)
	{
		const FVector There = Target->GetActorLocation();

		FVector Line = (GetActorLocation() - There).GetSafeNormal2D();

		if (Line.IsNearlyZero())
		{
			Line = -GetActorForwardVector().GetSafeNormal2D();
		}

		// the fighter is stood off, or brought in. With a wall at its back, the other is stood off instead for what is left.
		const float Now = FVector::Dist2D(GetActorLocation(), There);
		float Mine = CurrentFinisher.PairApart - Now;
		float Theirs = 0.0f;

		if (Mine > 1.0f)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(GutPairStandsOff), false, this);
			Query.AddIgnoredActor(Target);

			FHitResult Wall;

			if (GetWorld()->SweepSingleByChannel(Wall, GetActorLocation(), GetActorLocation() + (Line * Mine), FQuat::Identity, GetCapsuleComponent()->GetCollisionObjectType(),
				GetCapsuleComponent()->GetCollisionShape(), Query))
			{
				const float Free = FMath::Max(Wall.Distance - 2.0f, 0.0f);

				Theirs = Mine - Free;
				Mine = Free;
			}
		}

		const FVector MyStart = GetActorLocation() + (Line * Mine);
		const FVector TheirStart = There - (Line * Theirs);

		// the two pass through one another's space as the performers did
		GetCapsuleComponent()->IgnoreActorWhenMoving(Target, true);
		Target->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);

		SetActorRotation(FRotator(0.0f, (-Line).Rotation().Yaw, 0.0f));
		Target->SetActorRotation(FRotator(0.0f, Line.Rotation().Yaw, 0.0f));

		Target->bBeingTaken = true;
		Target->BeginCarried(CurrentFinisher.TakenAnimation, TheirStart);

		BeginCarried(CurrentFinisher.Animation, MyStart);

		UE_LOG(LogGut, Log, TEXT("%s and %s are stood %.0f cm apart for it (they were %.0f): performed by two"), *GetName(), *Target->GetName(), CurrentFinisher.PairApart, Now);

		return;
	}

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
	EndCarried();
	StopSlotAnimation(0.05f);

	bFinisherClosing = false;

	// a finisher that is cut short lets its target go
	if (AGutFighter* Target = FinisherTarget.Get())
	{
		// and the two are in one another's way again
		GetCapsuleComponent()->IgnoreActorWhenMoving(Target, false);
		Target->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);

		if (Target->IsAlive())
		{
			Target->EndCarried();
		}

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

void AGutFighter::UpdateTie()
{
	static const FName Hands[] = { FName("hand_l"), FName("hand_r") };
	static const FName Forearms[] = { FName("lowerarm_l"), FName("lowerarm_r") };

	const USkeletalMeshComponent* Seen = GetShownMesh();

	const bool bTied = BoundClip && TieMesh && !Beast && Seen
		&& Seen->GetBoneIndex(Hands[0]) != INDEX_NONE && Seen->GetBoneIndex(Hands[1]) != INDEX_NONE
		&& Seen->GetBoneIndex(Forearms[0]) != INDEX_NONE && Seen->GetBoneIndex(Forearms[1]) != INDEX_NONE;

	if (Tie->IsVisible() != bTied)
	{
		Tie->SetVisibility(bTied);
	}

	if (!bTied)
	{
		return;
	}

	if (Tie->GetStaticMesh() != TieMesh)
	{
		Tie->SetStaticMesh(TieMesh);
	}

	const FVector Left = Seen->GetBoneLocation(Hands[0]);
	const FVector Right = Seen->GetBoneLocation(Hands[1]);
	const FVector Middle = (Left + Right) * 0.5f;

	// the arms go through it. Wrists crossed behind the back come at one another from either side, so the two forearms
	// lie along one line there, one over the other, and that line is the way through; held side by side they would both
	// point the same way, and that would be.
	const FVector LeftArm = (Left - Seen->GetBoneLocation(Forearms[0])).GetSafeNormal();
	const FVector RightArm = (Right - Seen->GetBoneLocation(Forearms[1])).GetSafeNormal();

	const bool bCrossed = FVector::DotProduct(LeftArm, RightArm) < 0.0f;

	FVector Through = (bCrossed ? LeftArm - RightArm : LeftArm + RightArm).GetSafeNormal();

	if (Through.IsNearlyZero())
	{
		Through = LeftArm;
	}

	// its longer way from one wrist to the other, which crossed is from the one underneath to the one on top
	FVector Across = (Right - Left) - (Through * FVector::DotProduct(Right - Left, Through));
	const float Apart = Across.Size();

	if (!Across.Normalize())
	{
		Across = FVector::CrossProduct(Through, GetActorForwardVector()).GetSafeNormal();
	}

	// and its knot away from the back
	FVector Out = FVector::CrossProduct(Through, Across);

	if (FVector::DotProduct(Out, Middle - GetActorLocation()) < 0.0f)
	{
		Across = -Across;
		Out = -Out;
	}

	// big enough to go round both: as far as they are apart, and a wrist's thickness more on either side
	const float Size = FMath::Clamp((Apart + (TieWrist * 2.0f)) / FMath::Max(TieSpan, 1.0f), 0.85f, 1.3f);

	Tie->SetWorldLocationAndRotation(Middle - (Through * (bCrossed ? 0.0f : TieUpTheArm)), FMatrix(Across, Out, Through, FVector::ZeroVector).Rotator());
	Tie->SetWorldScale3D(FVector(Size));
}

void AGutFighter::OnBodyPosed()
{
	FeedFace();
	UpdateChain();
	UpdateTie();
	ListenToFeet();
	MeasureStrike();
	SteerBlow();
	HangArms();
	LogOwnCuts();
	GroundBody();
}

bool AGutFighter::MeasureShapeOf(const AActor* Dressed, const USkeletalMeshComponent* DressedBody, FGutClearShape& OutShape)
{
	static const FName Spine[] = { FName("pelvis"), FName("spine_01"), FName("spine_02"), FName("spine_03"), FName("spine_04"), FName("spine_05") };
	static const FName Arms[2][2] = { { FName("upperarm_l"), FName("lowerarm_l") }, { FName("upperarm_r"), FName("lowerarm_r") } };
	static const FName Hands[2] = { FName("hand_l"), FName("hand_r") };
	static const FName Legs[2][2] = { { FName("thigh_l"), FName("calf_l") }, { FName("thigh_r"), FName("calf_r") } };

	// as MeasureTrunk has them
	static const float Below[] = { 16.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	static const float PastShoulders[] = { 14.0f, 12.0f, 8.0f, 3.0f, 0.5f };

	// each kind of body once: the answer is the same for everybody who is seen as it
	static TMap<FName, FGutClearShape> Measured;

	if (!Dressed || !DressedBody || !DressedBody->GetSkeletalMeshAsset())
	{
		return false;
	}

	const FName Kind = Dressed->GetClass()->GetFName();

	if (const FGutClearShape* Known = Measured.Find(Kind))
	{
		OutShape = *Known;

		return !OutShape.Trunk.IsEmpty();
	}

	// its clothes have to be there to be measured: asked again until they are
	if (!DressedBody->GetSkeletalMeshAsset()->GetResourceForRendering())
	{
		return false;
	}

	const AGutFighter* Rule = GetDefault<AGutFighter>();

	FGutClearShape Shape;

	bool bStiff = false;

	for (const UActorComponent* Component : Dressed->GetComponents())
	{
		const USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

		for (const UMaterialInterface* Drawn : (Part && Part->IsVisible()) ? Part->GetMaterials() : TArray<UMaterialInterface*>())
		{
			const FString From = Drawn ? Drawn->GetPathName() : FString();

			bStiff |= From.Contains(TEXT("Tactical_Vest")) || From.Contains(TEXT("Soldier_Outfit"));
		}
	}

	for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(Spine); ++i)
	{
		FVector Toward, Front;
		FBox2D Round(ForceInit);

		if (!Rule->MeasureRound(Spine[i], Spine[i + 1], 1.0f, 40.0f, Toward, Front, Round, Below[i], PastShoulders[i], Dressed, DressedBody))
		{
			continue;
		}

		FGutClearShape::FSection& Section = Shape.Trunk.AddDefaulted_GetRef();

		Section.Bone = Spine[i];
		Section.Below = Below[i];
		Section.Height = Toward.Size();
		Section.Along = Toward.GetSafeNormal();
		Section.Front = Front;
		Section.Across = FVector::CrossProduct(Front, Section.Along);
		Section.Middle = Round.GetCenter();
		Section.Half = Round.GetExtent();
		Section.Give = bStiff ? 0.0f : GutFighterConstants::ArmGive;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		FVector Toward, Front;
		FBox2D Round(ForceInit);

		Shape.UpperArm[Side] = Rule->MeasureRound(Arms[Side][0], Arms[Side][1], 0.5f, 7.5f, Toward, Front, Round, 0.0f, 0.5f, Dressed, DressedBody) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 3.5f, 6.5f) : 4.5f;
		Shape.Forearm[Side] = Rule->MeasureRound(Arms[Side][1], Hands[Side], 0.5f, 6.5f, Toward, Front, Round, 0.0f, 0.5f, Dressed, DressedBody) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 2.8f, 5.5f) : 3.4f;
		Shape.Thigh[Side] = Rule->MeasureRound(Legs[Side][0], Legs[Side][1], 0.6f, 13.0f, Toward, Front, Round, 0.0f, 0.5f, Dressed, DressedBody) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 6.0f, 12.0f) : 8.5f;
	}

	// somebody in a crowd stands with their hands their own and empty
	Shape.HangIn = 1.0f;
	Shape.HangGap = GutFighterConstants::ArmHangGap;
	Shape.bBound = false;
	Shape.bOn = !Shape.Trunk.IsEmpty();

	Measured.Add(Kind, Shape);

	OutShape = Shape;

	return !OutShape.Trunk.IsEmpty();
}

void AGutFighter::MeasureTrunk()
{
	static const FName Spine[] = { FName("pelvis"), FName("spine_01"), FName("spine_02"), FName("spine_03"), FName("spine_04"), FName("spine_05") };
	static const FName Arms[2][2] = { { FName("upperarm_l"), FName("lowerarm_l") }, { FName("upperarm_r"), FName("lowerarm_r") } };
	static const FName Hands[2] = { FName("hand_l"), FName("hand_r") };
	static const FName Legs[2][2] = { { FName("thigh_l"), FName("calf_l") }, { FName("thigh_r"), FName("calf_r") } };

	// The hips are measured from the pelvis down as well as up. And lower on the body the arms of a body at rest are
	// further from it, so more of it can be taken for trunk: a belly is wider than the shoulder joints are apart.
	static const float Below[] = { 16.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	static const float PastShoulders[] = { 14.0f, 12.0f, 8.0f, 3.0f, 0.5f };

	Trunk.Reset();
	bTrunkMeasured = true;

	bool bStiff = false;

	for (UActorComponent* Component : BodyActor ? BodyActor->GetComponents() : TSet<UActorComponent*>())
	{
		const USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

		if (!Part || !Part->IsVisible())
		{
			continue;
		}

		for (const UMaterialInterface* Drawn : Part->GetMaterials())
		{
			const FString From = Drawn ? Drawn->GetPathName() : FString();

			bStiff |= From.Contains(TEXT("Tactical_Vest")) || From.Contains(TEXT("Soldier_Outfit"));
		}
	}

	for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(Spine); ++i)
	{
		FVector Toward, Front;
		FBox2D Round(ForceInit);

		if (!MeasureRound(Spine[i], Spine[i + 1], 1.0f, 40.0f, Toward, Front, Round, Below[i], PastShoulders[i]))
		{
			continue;
		}

		FTrunkSection Section;

		Section.Bone = Spine[i];
		Section.Below = Below[i];
		Section.Height = Toward.Size();
		Section.Along = Toward.GetSafeNormal();
		Section.Front = Front;
		Section.Across = FVector::CrossProduct(Front, Section.Along);
		Section.Middle = Round.GetCenter();
		Section.Half = Round.GetExtent();

		// An arm at rest lies against the ribs, and both give. Held wholly clear of them, arms stood off every body like a
		// doll's. Skin, a shirt, a coat give; a vest of plates and a soldier's rig do not, and that is what he saw an arm
		// go through. Which it is, is told from where what is worn comes from: the outfits' own folders.
		Section.Give = bStiff ? 0.0f : GutFighterConstants::ArmGive;

		Trunk.Add(Section);
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		FVector Toward, Front;
		FBox2D Round(ForceInit);

		// no further from the bone than an arm reaches: at rest the trunk is a hand's breadth from it, and was being taken for arm
		ArmRadius[Side] = MeasureRound(Arms[Side][0], Arms[Side][1], 0.5f, 7.5f, Toward, Front, Round) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 3.5f, 6.5f) : 4.5f;

		// the forearm with its sleeve, and the thigh with its trouser leg
		ForearmRadius[Side] = MeasureRound(Arms[Side][1], Hands[Side], 0.5f, 6.5f, Toward, Front, Round) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 2.8f, 5.5f) : 3.4f;
		ThighRadius[Side] = MeasureRound(Legs[Side][0], Legs[Side][1], 0.6f, 13.0f, Toward, Front, Round) ? FMath::Clamp((Round.GetExtent().X + Round.GetExtent().Y) * 0.5f, 6.0f, 12.0f) : 8.5f;
	}

	if (!Trunk.IsEmpty())
	{
		UE_LOG(LogGut, Log, TEXT("%s: the trunk of %s as it is dressed is %.0f cm across and %.0f deep at the ribs, its upper arms %.1f and %.1f cm thick, forearms %.1f and %.1f, thighs %.1f and %.1f; an arm may lie %.1f cm into its ribs"), *GetName(),
			*GetNameSafe(BodyActor ? BodyActor->GetClass() : nullptr), Trunk.Last().Half.X * 2.0f, Trunk.Last().Half.Y * 2.0f, ArmRadius[0] * 2.0f, ArmRadius[1] * 2.0f,
			ForearmRadius[0] * 2.0f, ForearmRadius[1] * 2.0f, ThighRadius[0] * 2.0f, ThighRadius[1] * 2.0f, Trunk.Last().Give);
	}
}

bool AGutFighter::GetClearShape(FGutClearShape& OutShape)
{
	using namespace GutFighterConstants;

	OutShape.Trunk.Reset();
	OutShape.Held[0].Reset();
	OutShape.Held[1].Reset();

	// somebody alive with a body of their own, which is posed from the fighting skeleton. A body that has gone limp is its own business.
	if (!bArmsClear || !HasBody() || !IsAlive() || Beast)
	{
		return false;
	}

	if (!bTrunkMeasured)
	{
		MeasureTrunk();
	}

	for (const FTrunkSection& Section : Trunk)
	{
		FGutClearShape::FSection& One = OutShape.Trunk.AddDefaulted_GetRef();

		One.Bone = Section.Bone;
		One.Height = Section.Height;
		One.Below = Section.Below;
		One.Along = Section.Along;
		One.Across = Section.Across;
		One.Front = Section.Front;
		One.Middle = Section.Middle;

		// a piece of armor round the chest stands out further than the clothes under it
		One.Half = Section.Half.ComponentMax((Section.Bone == FName("spine_01") || Section.Bone == FName("pelvis")) ? FVector2D::ZeroVector : ChestPieceHalf);

		// and gives nothing
		One.Give = (ChestPieceHalf.X > 0.0f && One.Half.X > Section.Half.X) ? 0.0f : Section.Give;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		OutShape.UpperArm[Side] = ArmRadius[Side];
		OutShape.Forearm[Side] = ForearmRadius[Side];
		OutShape.Thigh[Side] = ThighRadius[Side];
		OutShape.Held[Side].Append(HeldPlaces[Side]);
	}

	OutShape.bBound = BoundClip != nullptr;

	// An arm that hangs is brought in to this body's side: for somebody standing at ease with their hands their own and
	// empty, and eased off as they set out walking, where the arms swing and are left to.
	// Through a nod, a shift of weight or a word said with the hands as well: an arm that still hangs in one of those is
	// still an arm that hangs. This stood down for as long as any small thing was being done, and on a broad body the arms
	// stood out from the hips until it was over ("the gatekeeper's arms are misaligned, but in a second or two they look
	// perfect", 24 November). Whether an arm hangs is told from where the arm is, by the op, arm by arm.
	const bool bHangs = bArmsHang && bAtEase && !bCelebrating && State == EGutFighterState::Idle && !(Weapon && Weapon->Mesh)
		&& GetVelocity().SizeSquared2D() < FMath::Square(20.0f) && LedVelocity.IsNearlyZero();

	HangInShown = IsNewlyAtEase() ? (bHangs ? 1.0f : 0.0f) : FMath::FInterpConstantTo(HangInShown, bHangs ? 1.0f : 0.0f, GetWorld()->GetDeltaSeconds(), 2.2f);

	OutShape.HangIn = HangInShown;
	OutShape.HangGap = ArmHangGap;

	return !OutShape.Trunk.IsEmpty();
}

bool AGutFighter::GetClearBodyNow(FGutClearShape& OutShape, FGutClearBody& OutWhere)
{
	static const FName Legs[2][2] = { { FName("thigh_l"), FName("calf_l") }, { FName("thigh_r"), FName("calf_r") } };

	const USkeletalMeshComponent* Seen = GetShownMesh();

	if (!Seen || !HasBody())
	{
		return false;
	}

	if (!bTrunkMeasured)
	{
		MeasureTrunk();
	}

	if (Trunk.IsEmpty())
	{
		return false;
	}

	OutShape.Trunk.Reset();
	OutWhere.Spine.Reset();

	for (const FTrunkSection& Section : Trunk)
	{
		const int32 Bone = Seen->GetBoneIndex(Section.Bone);

		if (Bone == INDEX_NONE)
		{
			return false;
		}

		FGutClearShape::FSection& One = OutShape.Trunk.AddDefaulted_GetRef();

		One.Bone = Section.Bone;
		One.Height = Section.Height;
		One.Below = Section.Below;
		One.Along = Section.Along;
		One.Across = Section.Across;
		One.Front = Section.Front;
		One.Middle = Section.Middle;
		One.Half = Section.Half.ComponentMax((Section.Bone == FName("spine_01") || Section.Bone == FName("pelvis")) ? FVector2D::ZeroVector : ChestPieceHalf);

		OutWhere.Spine.Add(Seen->GetBoneTransform(Bone));
	}

	OutWhere.Shape = &OutShape;
	OutWhere.Clearance = 0.0;
	OutWhere.bLegs = Seen->GetBoneIndex(Legs[0][0]) != INDEX_NONE && Seen->GetBoneIndex(Legs[1][1]) != INDEX_NONE;

	for (int32 Side = 0; Side < 2 && OutWhere.bLegs; ++Side)
	{
		OutShape.Thigh[Side] = ThighRadius[Side];
		OutWhere.Hip[Side] = Seen->GetBoneLocation(Legs[Side][0]);
		OutWhere.Knee[Side] = Seen->GetBoneLocation(Legs[Side][1]);
	}

	if (Seen->GetBoneIndex(FName("head")) != INDEX_NONE && Seen->GetBoneIndex(FName("neck_02")) != INDEX_NONE)
	{
		OutWhere.bHead = true;
		OutWhere.Head = Seen->GetBoneLocation(FName("head")) + ((Seen->GetBoneLocation(FName("head")) - Seen->GetBoneLocation(FName("neck_02"))).GetSafeNormal() * 9.0);
	}

	return true;
}

void AGutFighter::LogOwnCuts()
{
	static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("GutClearLog"));

	const USkeletalMeshComponent* Seen = GetShownMesh();

	if (!bLog || !Seen || !HasBody() || !IsAlive() || Beast)
	{
		return;
	}

	static const FName Arms[2][3] = { { FName("lowerarm_l"), FName("hand_l"), FName("middle_01_l") }, { FName("lowerarm_r"), FName("hand_r"), FName("middle_01_r") } };

	// the same shape the arms are kept out of, set down where the body is now, and asked with no air left round it
	FGutClearShape Shape;
	FGutClearBody Where;

	const bool bWas = GutFighterConstants::bArmsClear;

	if (!GetClearBodyNow(Shape, Where))
	{
		return;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Seen->GetBoneIndex(Arms[Side][0]) == INDEX_NONE || Seen->GetBoneIndex(Arms[Side][1]) == INDEX_NONE)
		{
			continue;
		}

		const FVector Elbow = Seen->GetBoneLocation(Arms[Side][0]);
		const FTransform Hand = Seen->GetBoneTransform(Seen->GetBoneIndex(Arms[Side][1]));
		const FVector Wrist = Hand.GetLocation();

		DeepestHand = FMath::Max3(DeepestHand, static_cast<float>(Where.GetPush(Wrist, FMath::Min(ForearmRadius[Side], 3.2f), Elbow, false).Size()),
			static_cast<float>(Where.GetPush(FMath::Lerp(Elbow, Wrist, 0.55), ForearmRadius[Side], Elbow, false).Size()));

		for (const FVector4& Held : HeldPlaces[Side])
		{
			const float In = static_cast<float>(Where.GetPush(Hand.TransformPositionNoScale(FVector(Held)), Held.W, Wrist, true).Size());

			if (In > DeepestHeld + 1.0f && In > 2.5f)
			{
				UE_LOG(LogGut, Log, TEXT("[own] %s: what it holds is %.1f cm in, %s, place %d of %d, wrist %.0f cm from the middle of the head, %.0f above the pelvis"), *GetName(), In, *UEnum::GetValueAsString(State),
					static_cast<int32>(&Held - HeldPlaces[Side].GetData()), HeldPlaces[Side].Num(), Where.bHead ? FVector::Dist(Wrist, Where.Head) : -1.0f, Wrist.Z - Seen->GetBoneLocation(FName("pelvis")).Z);
			}

			DeepestHeld = FMath::Max(DeepestHeld, In);
		}
	}

	// not while the fighters are being stood up for a fight: for a frame or two a body is as it was made, weapon in hand
	if (State == EGutFighterState::Idle && !bAtEase && GetWorld()->GetTimeSeconds() - HeldSince < 1.5f)
	{
		DeepestHand = 0.0f;
		DeepestHeld = 0.0f;
	}

	if (const float Now = GetWorld()->GetTimeSeconds(); Now - DeepestSaid >= 5.0f)
	{
		UE_LOG(LogGut, Log, TEXT("[own] %s | %s | %s | hand or forearm %.1f cm into its own body, what it holds %.1f, in the last 5 s%s"), *GetName(), *GetNameSafe(BodyActor ? BodyActor->GetClass() : nullptr),
			Weapon ? *Weapon->GetName() : TEXT("nothing held"), DeepestHand, DeepestHeld, bWas ? TEXT("") : TEXT(" (not kept clear)"));

		DeepestSaid = Now;
		DeepestHand = 0.0f;
		DeepestHeld = 0.0f;
	}
}

void AGutFighter::MeasureHeld()
{
	HeldPlaces[0].Reset();
	HeldPlaces[1].Reset();

	HeldSince = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	const USkeletalMeshComponent* Shown = GetShownMesh();
	const UStaticMesh* Model = WeaponMesh ? WeaponMesh->GetStaticMesh().Get() : nullptr;

	// Something held in a hand of the body that is seen. What is strapped along a forearm (the shield) goes where the
	// forearm goes, and a chain hangs as it hangs.
	if (!Weapon || !Model || !Shown || !HasBody() || !WeaponMesh->IsVisible() || WeaponMesh->GetAttachParent() != Shown || Weapon->bStrapToForearm)
	{
		return;
	}

	const FString Bone = WeaponMesh->GetAttachSocketName().ToString();
	const int32 Side = Bone.EndsWith(TEXT("_l")) ? 0 : (Bone.EndsWith(TEXT("_r")) ? 1 : INDEX_NONE);

	if (Side == INDEX_NONE || !Bone.StartsWith(TEXT("hand")))
	{
		return;
	}

	// along its longest way, from one end to the other, as thick as it is across
	const FBox Box = Model->GetBoundingBox();
	const FVector Size = Box.GetSize();
	const int32 Long = Size.X >= Size.Y ? (Size.X >= Size.Z ? 0 : 2) : (Size.Y >= Size.Z ? 1 : 2);

	const FTransform InHand = WeaponMesh->GetRelativeTransform();
	const FVector Scale = InHand.GetScale3D().GetAbs();

	const float Thick = FMath::Clamp(FMath::Max(Size[(Long + 1) % 3] * Scale[(Long + 1) % 3], Size[(Long + 2) % 3] * Scale[(Long + 2) % 3]) * 0.4f, 1.2f, 4.5f);

	const int32 Count = FMath::Clamp(FMath::CeilToInt32(Size[Long] * Scale[Long] / 12.0f) + 1, 3, 8);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector At = Box.GetCenter();

		At[Long] = FMath::Lerp(Box.Min[Long], Box.Max[Long], static_cast<float>(i) / (Count - 1));

		const FVector Place = InHand.TransformPosition(At);

		HeldPlaces[Side].Add(FVector4(Place.X, Place.Y, Place.Z, Thick));
	}

	UE_LOG(LogGut, Verbose, TEXT("%s: what is in its %s hand is %.0f cm long and kept out of its own body at %d places"), *GetName(), Side == 0 ? TEXT("left") : TEXT("right"), Size[Long] * Scale[Long], Count);
}

void AGutFighter::HangArms()
{
	using namespace GutFighterConstants;

	static const FName Hands[2] = { FName("hand_l"), FName("hand_r") };
	static const FName Thighs[2] = { FName("thigh_l"), FName("thigh_r") };
	static const FName Middles[3] = { FName("pelvis"), FName("spine_01"), FName("spine_02") };
	static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("GutArmLog"));

	const USkeletalMeshComponent* Seen = GetShownMesh();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const UPhysicsAsset* Shape = (Seen && Seen != GetMesh()) ? Seen->GetPhysicsAsset() : nullptr;

	// only somebody at ease whose arms are their own and are not busy; and only where arms are not kept out of the body
	// as it is dressed by the step that does that (FGutClearOp), which brings hanging arms in itself: the two chased each other
	const bool bHanging = !bArmsClear && bArmsHang && Shape && bAtEase && IsAlive() && !Beast && !BoundClip && !bCelebrating && State == EGutFighterState::Idle && FidgetLeft <= 0.0f && !(Weapon && Weapon->Mesh);

	if (!bHanging || DeltaTime <= 0.0f)
	{
		ArmsIn[0] = FMath::FInterpTo(ArmsIn[0], 0.0f, DeltaTime, 6.0f);
		ArmsIn[1] = FMath::FInterpTo(ArmsIn[1], 0.0f, DeltaTime, 6.0f);
		return;
	}

	// found standing: walking, the arms swing and the same angle is kept
	if (GetVelocity().SizeSquared2D() > FMath::Square(20.0f) || !LedVelocity.IsNearlyZero())
	{
		return;
	}

	const int32 Pelvis = Seen->GetBoneIndex(Middles[0]);

	if (Pelvis == INDEX_NONE)
	{
		return;
	}

	const float PelvisHeight = Seen->GetBoneLocation(Middles[0]).Z;

	static const FName Forearms[2] = { FName("lowerarm_l"), FName("lowerarm_r") };

	// the body as it is dressed, where it is now
	FGutClearShape DressedShape;
	FGutClearBody Dressed;

	const bool bDressed = GutFighterConstants::bArmsClear && GetClearBodyNow(DressedShape, Dressed);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Seen->GetBoneIndex(Hands[Side]) == INDEX_NONE)
		{
			continue;
		}

		const FVector Wrist = Seen->GetBoneLocation(Hands[Side]);

		// how far the wrist is from the body's own side: its hips, the thigh under it and the small of its back, as the
		// body's own shapes have them
		float Gap = UE_BIG_NUMBER;

		for (const FName& Bone : { Middles[0], Middles[1], Middles[2], Thighs[Side] })
		{
			const int32 Part = Shape->FindBodyIndex(Bone);
			const int32 Index = Seen->GetBoneIndex(Bone);

			if (Part != INDEX_NONE && Index != INDEX_NONE && Shape->SkeletalBodySetups.IsValidIndex(Part) && Shape->SkeletalBodySetups[Part])
			{
				Gap = FMath::Min(Gap, Shape->SkeletalBodySetups[Part]->GetShortestDistanceToPoint(Wrist, Seen->GetBoneTransform(Index)));
			}
		}

		// an arm that is not hanging (folded, on a hip, held out) is left as the clip has it
		const bool bDown = Wrist.Z < PelvisHeight + ArmHangBelow && Gap < ArmHangNear + FMath::Abs(ArmsIn[Side]) * 1.2f;

		if (Gap >= UE_BIG_NUMBER * 0.5f || !bDown)
		{
			ArmsIn[Side] = FMath::FInterpTo(ArmsIn[Side], 0.0f, DeltaTime, 4.0f);
			continue;
		}

		float Step = FMath::Clamp((Gap - (ArmHangGap + (bChestPiece ? ArmHangClearOfChest : 0.0f))) * ArmHangGain * DeltaTime, -2.5f, 2.5f);

		// An arm is not brought in further than what the body wears lets it: the hips of a coat are wider than the hips
		// under it. The arm is kept out of the coat whatever is asked here (FGutClearOp), and asked for more than it could
		// have this wound itself to its limit in two seconds: an arm that hung thirty degrees inside its body, which the
		// step that keeps it out then took out behind the back with the hand turned over. So the same shape is asked how
		// near the arm is, with the gap it is to hang at added to its thickness, and anything inside that lets it back out.
		if (bDressed)
		{
			const FVector Elbow = Seen->GetBoneIndex(Forearms[Side]) != INDEX_NONE ? Seen->GetBoneLocation(Forearms[Side]) : Wrist;

			const float Near = FMath::Max3(
				static_cast<float>(Dressed.GetPush(Wrist, 3.2 + ArmHangGap, Elbow, false).Size()),
				static_cast<float>(Dressed.GetPush(FMath::Lerp(Elbow, Wrist, 0.5), ForearmRadius[Side] + (ArmHangGap * 0.5), Elbow, false).Size()),
				static_cast<float>(Dressed.GetPush(Elbow, ArmRadius[Side] + 1.0, Elbow, false).Size()));

			if (Near > 0.3f)
			{
				Step = FMath::Min(Step, FMath::Max(-Near * ArmHangGain * 0.6f * DeltaTime, -2.5f));
			}
		}

		ArmsIn[Side] = FMath::Clamp(ArmsIn[Side] + Step, -ArmHangMostOut, ArmHangMostIn);

		if (bLog && GFrameCounter % 90 == 0)
		{
			UE_LOG(LogGut, Log, TEXT("Arms: %s (%s) %s wrist %.1f cm from its side, arm brought in %.1f degrees"), *GetName(), *GetNameSafe(Seen->GetSkeletalMeshAsset()),
				Side == 0 ? TEXT("left") : TEXT("right"), Gap, ArmsIn[Side]);
		}
	}
}

void AGutFighter::SteerBlow()
{
	using namespace GutFighterConstants;

	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const AGutFighter* Target = Opponent.Get();

	// from a little before the blow is due until just after it: that is when the limb is on its way in
	const bool bComing = bBlowsSteered && StrikeClip && Target && Target->IsAlive() && !Beast && !Target->IsBeast() && !(Weapon && Weapon->LinkMesh)
		&& ((State == EGutFighterState::Windup && StateTime >= CurrentAttack.Windup - BlowSteerLead) || State == EGutFighterState::Active || (State == EGutFighterState::Recovery && StateTime <= BlowSteerAfter));

	FVector End;
	int32 Limb = 1;

	if (bComing && GetStrikingPoint(End, &Limb))
	{
		FVector Skin;

		const float Gap = Target->GetBodyGapFrom(End, nullptr, &Skin);

		// a blow that is going to pass within reach of the skin is brought onto it; one that is nowhere near was dodged or
		// fell short by the rules, and is left to miss
		if (Gap > 0.0f && Gap < BlowSteerMost)
		{
			BlowLimb = Limb;
			BlowReach = (BlowReach + ((Skin - End) * BlowSteerGain)).GetClampedToMaxSize(BlowSteerMost);
		}

		// -GutSteerLog says, frame by frame, how far off the blow is and how far it is being taken
		static const bool bSteerLog = FParse::Param(FCommandLine::Get(), TEXT("GutSteerLog"));

		if (bSteerLog)
		{
			UE_LOG(LogGut, Log, TEXT("Steer: %s %s at %.3f, state %d for %.3f s, gap %.1f cm, taken %.1f cm, limb %d"), *GetName(), *CurrentAttack.Label.ToString(),
				GetWorld()->GetTimeSeconds(), static_cast<int32>(State), StateTime, Gap, BlowReach.Size(), Limb);
		}

		return;
	}

	// done: the limb goes back to the clip's own path
	BlowReach = FMath::VInterpTo(BlowReach, FVector::ZeroVector, DeltaTime, BlowSteerBack);

	if (BlowReach.IsNearlyZero(0.1f))
	{
		BlowReach = FVector::ZeroVector;
		BlowLimb = INDEX_NONE;
	}
}

void AGutFighter::SetLeftOutOfPicture(bool bLeftOut)
{
	if (bLeftOutOfPicture == bLeftOut)
	{
		return;
	}

	bLeftOutOfPicture = bLeftOut;

	if (!bLeftOut)
	{
		// back, and only what was taken out for this: anything that was hidden for a reason of its own stays hidden
		for (const TWeakObjectPtr<AActor>& One : LeftOut)
		{
			if (One.IsValid())
			{
				One->SetActorHiddenInGame(false);
			}
		}

		LeftOut.Reset();
		return;
	}

	// itself, the body that is seen, and whatever either wears or carries
	TArray<AActor*> All;

	for (AActor* Root : { static_cast<AActor*>(this), GetBodyActor() })
	{
		if (Root)
		{
			All.AddUnique(Root);

			TArray<AActor*> Worn;
			Root->GetAttachedActors(Worn, false, true);

			for (AActor* One : Worn)
			{
				All.AddUnique(One);
			}
		}
	}

	for (AActor* One : All)
	{
		if (!One->IsHidden())
		{
			One->SetActorHiddenInGame(true);
			LeftOut.Add(One);
		}
	}
}

void AGutFighter::StopForBlow(float Seconds)
{
	if (!GutFighterConstants::bBlowsStop || Seconds <= 0.0f || !IsAlive())
	{
		return;
	}

	// nearly stopped, not stopped: the world's clock goes on, and the fighter's own takes up again when the time is out
	CustomTimeDilation = GutFighterConstants::BlowStopRate;
	BlowStopEnds = FMath::Max(BlowStopEnds, GetWorld()->GetTimeSeconds() + Seconds);
}

void AGutFighter::TakeInHand(UAnimSequenceBase* Taking)
{
	if (!Taking || !IsAlive() || State != EGutFighterState::Idle)
	{
		return;
	}

	bBoundAfterTaking = bBoundAfterTaking || IsBound();

	if (IsBound())
	{
		SetBound(false);
	}

	PlaySlotAnimation(Taking, 1.0f, 0.0f, 0.4f, 0.6f);

	TakingLeft = Taking->GetPlayLength();

	// nothing small is done over the top of it
	FidgetLeft = 0.0f;
	FidgetIn = FMath::Max(FidgetIn, TakingLeft + 4.0f);
}

void AGutFighter::Gesture(UAnimSequenceBase* Clip, bool bWithHead)
{
	const bool bStanding = GetVelocity().SizeSquared2D() < FMath::Square(25.0f) && LedVelocity.IsNearlyZero();

	// only with hands that are its own and free
	if (!Clip || !IsAlive() || Beast || !bAtEase || !bStanding || BoundClip || bCelebrating || TakingLeft > 0.0f || State != EGutFighterState::Idle || (Weapon && Weapon->Mesh))
	{
		return;
	}

	PlaySlotAnimation(Clip, 1.0f, 0.0f, 0.45f, 0.6f);

	// kept as one of the small things it does: over the upper body only, and nothing else small begun on top of it
	bGestureKeepsHead = !bWithHead;
	FidgetLeft = Clip->GetPlayLength();
	FidgetIn = FMath::Max(FidgetIn, FidgetLeft + 3.0f);
}

void AGutFighter::StopGesture()
{
	if (FidgetLeft > 0.0f)
	{
		FidgetLeft = 0.0f;

		if (State == EGutFighterState::Idle)
		{
			StopSlotAnimation(0.45f);
		}
	}
}

void AGutFighter::UpdateFidget(float DeltaTime)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoFidget"));

	// busy with something it was handed: until it is done, or the body has somewhere to go
	if (TakingLeft > 0.0f)
	{
		TakingLeft -= DeltaTime;

		const bool bGoing = GetVelocity().SizeSquared2D() > FMath::Square(25.0f) || !LedVelocity.IsNearlyZero();

		if (TakingLeft <= 0.0f || bGoing || State != EGutFighterState::Idle)
		{
			TakingLeft = 0.0f;

			if (bGoing && State == EGutFighterState::Idle)
			{
				StopSlotAnimation(0.3f);
			}

			if (bBoundAfterTaking)
			{
				bBoundAfterTaking = false;
				SetBound(true);
			}

			SetLookTarget(nullptr);
		}

		return;
	}

	const bool bStanding = GetVelocity().SizeSquared2D() < FMath::Square(25.0f) && LedVelocity.IsNearlyZero();

	// in the middle of one: it is dropped the moment the body has somewhere to go or something else to do
	if (FidgetLeft > 0.0f)
	{
		FidgetLeft -= DeltaTime;

		if (!bAtEase || !bStanding || State != EGutFighterState::Idle)
		{
			FidgetLeft = 0.0f;

			if (State == EGutFighterState::Idle)
			{
				StopSlotAnimation(0.25f);
			}
		}

		return;
	}

	// only somebody alive, at ease, standing still with their hands their own, and with nothing else playing over them
	// the small things of the family it stands in, which begin and end as that standing does; without a family, the common ones
	const bool bFamilys = AtEaseFamilies.IsValidIndex(Family);
	const TArray<TObjectPtr<UAnimSequenceBase>>& Fidgets = bFamilys ? AtEaseFamilies[Family].Extras : AtEaseFidgets;

	if (bOff || !bAtEase || !bStanding || !IsAlive() || Beast || BoundClip || bCelebrating || State != EGutFighterState::Idle || Fidgets.IsEmpty())
	{
		return;
	}

	// drawn from the body's own name, so that no two keep the same time
	FRandomStream Draw(GetTypeHash(GetName()) + FMath::FloorToInt32(GetWorld()->GetTimeSeconds() * 10.0f));

	if (FidgetIn < 0.0f)
	{
		FidgetIn = Draw.FRandRange(FidgetEvery.X * 0.4f, FidgetEvery.Y);
	}

	FidgetIn -= DeltaTime;

	if (FidgetIn > 0.0f)
	{
		return;
	}

	// not the one it did last
	int32 Pick = Draw.RandRange(0, Fidgets.Num() - 1);

	if (Pick == FidgetLast && Fidgets.Num() > 1)
	{
		Pick = (Pick + 1) % Fidgets.Num();
	}

	FidgetLast = Pick;
	FidgetIn = Draw.FRandRange(FidgetEvery.X, FidgetEvery.Y);

	if (UAnimSequenceBase* Clip = Fidgets[Pick])
	{
		PlaySlotAnimation(Clip, 1.0f, 0.0f, 0.5f, 0.6f);

		bGestureKeepsHead = false;
		FidgetLeft = Clip->GetPlayLength();
	}
}

bool AGutFighter::GetLookAt(FVector& OutPlace) const
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoLook"));

	if (bOff || !IsAlive() || Beast || State == EGutFighterState::Staggered)
	{
		return false;
	}

	// where their eyes are: on the body that is seen, a hand's breadth above the foot of the skull; or about where a head is on somebody standing
	const auto HeadOf = [](const AGutFighter* Who)
	{
		static const FName Head(TEXT("head"));

		if (const USkeletalMeshComponent* Seen = Who->HasBody() ? Who->GetShownMesh() : nullptr; Seen && Seen->GetBoneIndex(Head) != INDEX_NONE)
		{
			return Seen->GetBoneLocation(Head) + FVector(0.0f, 0.0f, 9.0f);
		}

		return Who->GetActorLocation() + FVector(0.0f, 0.0f, Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.72f);
	};

	// whoever it has been told to look at: the one it is talking to
	if (const AGutFighter* Told = LookTarget.Get(); Told && Told != this && Told->IsAlive())
	{
		OutPlace = HeadOf(Told);
		return true;
	}

	if (!bAtEase)
	{
		// in the cage: the one it is fighting
		const AGutFighter* Other = GetOpponent();

		if (!Other || Other == this || !Other->IsAlive() || !IsCombatEnabled())
		{
			return false;
		}

		OutPlace = HeadOf(Other);
		return true;
	}

	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();

	if (!Viewer)
	{
		return false;
	}

	// whoever walks the street, when they come near; they themselves look where they are going
	if (const AGutFighter* Walker = Cast<AGutFighter>(Viewer->GetPawn()))
	{
		if (Walker == this || !Walker->IsAtEase() || FVector::DistSquared(Walker->GetActorLocation(), GetActorLocation()) > FMath::Square(LookRange))
		{
			return false;
		}

		OutPlace = HeadOf(Walker);
		return true;
	}

	// nobody is walking: looked at from a card, as an owner looks over a yard. They look back.
	if (Viewer->PlayerCameraManager && FVector::DistSquared(Viewer->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) < FMath::Square(LookRange * 2.0f))
	{
		OutPlace = Viewer->PlayerCameraManager->GetCameraLocation();
		return true;
	}

	return false;
}

void AGutFighter::GroundBody()
{
	static const FName Feet[2][2] = { { FName("foot_l"), FName("ball_l") }, { FName("foot_r"), FName("ball_r") } };
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoGround"));
	static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("GutGroundLog"));

	const USkeletalMeshComponent* Seen = GetShownMesh();

	if (!Seen || Seen == GetMesh() || Beast || GroundedFrame == GFrameCounter)
	{
		return;
	}

	GroundedFrame = GFrameCounter;

	// on its feet, and on the ground: a body that is down, dead or in the air is not stood on anything
	const UCharacterMovementComponent* Movement = GetCharacterMovement();

	const bool bOnFeet = IsAlive() && State != EGutFighterState::Staggered && State != EGutFighterState::Finishing && Movement && Movement->IsMovingOnGround();

	const USkeletalMesh* Mesh = Seen->GetSkeletalMeshAsset();

	if (!bOnFeet || !Mesh || Seen->GetBoneIndex(Feet[0][0]) == INDEX_NONE || Seen->GetBoneIndex(Feet[1][0]) == INDEX_NONE)
	{
		GroundWanted = 0.0f;
		return;
	}

	// how high each ankle and the ball of each foot are when this body stands in its own reference pose: that is its sole on the floor
	if (SoleMesh.Get() != Mesh)
	{
		SoleMesh = Mesh;

		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();

		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 Part = 0; Part < 2; ++Part)
			{
				const int32 Bone = Skeleton.FindBoneIndex(Feet[Side][Part]);

				SoleRest[Side][Part] = Bone != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone).GetLocation().Z : 0.0f;
			}
		}
	}

	// the floor under the capsule: the engine keeps a walking capsule a couple of centimetres off it
	const float FloorHeight = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - (Movement->CurrentFloor.bBlockingHit ? Movement->CurrentFloor.GetDistanceToFloor() : 0.0f);

	// the lower of the two feet, by whichever of its ankle and the ball of it is nearer the floor
	float Lowest = UE_BIG_NUMBER;

	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Part = 0; Part < 2; ++Part)
		{
			if (Seen->GetBoneIndex(Feet[Side][Part]) != INDEX_NONE)
			{
				Lowest = FMath::Min(Lowest, Seen->GetBoneLocation(Feet[Side][Part]).Z - FloorHeight - SoleRest[Side][Part]);
			}
		}
	}

	// as the clip and the body's build have it, before anything was done about it
	const float AsPosed = Lowest - GroundShift;

	// never by more than a hand's breadth: a clip that really leaves the ground is left in the air
	GroundWanted = bOff ? 0.0f : FMath::Clamp(-AsPosed, -GutFighterConstants::MostLowered, GutFighterConstants::MostRaised);

	if (bLog)
	{
		// how far a foot that is down goes over the floor: that is sliding. Down is the sole within two centimetres of the floor,
		// this time and last.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			float Up = UE_BIG_NUMBER;

			for (int32 Part = 0; Part < 2; ++Part)
			{
				if (Seen->GetBoneIndex(Feet[Side][Part]) != INDEX_NONE)
				{
					Up = FMath::Min(Up, Seen->GetBoneLocation(Feet[Side][Part]).Z - FloorHeight - SoleRest[Side][Part]);
				}
			}

			// whichever of the ankle and the ball of the foot is the one on the floor: a foot rolling forward onto its toes
			// moves its ankle a hand's length without sliding at all
			const bool bHasBall = Seen->GetBoneIndex(Feet[Side][1]) != INDEX_NONE;
			const float AnkleUp = Seen->GetBoneLocation(Feet[Side][0]).Z - FloorHeight - SoleRest[Side][0];
			const float BallUp = bHasBall ? Seen->GetBoneLocation(Feet[Side][1]).Z - FloorHeight - SoleRest[Side][1] : UE_BIG_NUMBER;
			const int32 On = BallUp < AnkleUp ? 1 : 0;

			const FVector Now = Seen->GetBoneLocation(Feet[Side][On]);

			// against the lower of the two, not the floor: a body that stands two centimetres up has feet that are down all the same
			const bool bDown = Up - Lowest < 1.5f && FMath::Abs(Lowest) < 5.0f;
			const float Moved = FVector::Dist2D(Now, GroundPartWas[Side][On]);

			if (bDown && bGroundFootDown[Side] && Moved < 60.0f)
			{
				GroundLogSlid += Moved;

				// slower than a walk: slid, not swung through
				if (Moved / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f) < 140.0f)
				{
					GroundLogSlow += Moved;
				}
			}

			bGroundFootDown[Side] = bDown;
			GroundPartWas[Side][0] = Seen->GetBoneLocation(Feet[Side][0]);
			GroundPartWas[Side][1] = bHasBall ? Seen->GetBoneLocation(Feet[Side][1]) : GroundPartWas[Side][0];
		}

		// the same on the skeleton that does the fighting, which the seen body is carried from: an ankle that stays within a
		// few centimetres of where it stands at rest is down. And a foot that is down but going fast is being swung through
		// low, not slid: only what goes slower than a walk is counted, on both.
		static const FName OwnFeet[2] = { FName("foot_l"), FName("foot_r") };

		const float DeltaTime = FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f);

		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (GetMesh()->GetBoneIndex(OwnFeet[Side]) == INDEX_NONE)
			{
				continue;
			}

			const FVector Now = GetMesh()->GetBoneLocation(OwnFeet[Side]);

			// and how far the seen body's ankle is from the fighting skeleton's, over the ground: if the one is carried
			// faithfully from the other that does not change
			if (Seen->GetBoneIndex(OwnFeet[Side]) != INDEX_NONE)
			{
				const float Apart = FVector::Dist2D(Now, Seen->GetBoneLocation(OwnFeet[Side]));

				GroundLogApart[0] = FMath::Min(GroundLogApart[0], Apart);
				GroundLogApart[1] = FMath::Max(GroundLogApart[1], Apart);
			}

			const bool bDown = Now.Z - FloorHeight - GroundShift < 11.5f;
			const float Moved = FVector::Dist2D(Now, GroundOwnWas[Side]);

			// and the ball of that foot, which goes round the ankle when the foot swivels
			static const FName OwnBalls[2] = { FName("ball_l"), FName("ball_r") };

			const FVector Ball = GetMesh()->GetBoneIndex(OwnBalls[Side]) != INDEX_NONE ? GetMesh()->GetBoneLocation(OwnBalls[Side]) : Now;
			const FVector SeenBall = Seen->GetBoneIndex(OwnBalls[Side]) != INDEX_NONE ? Seen->GetBoneLocation(OwnBalls[Side]) : Now;

			if (bDown && bGroundOwnDown[Side] && Moved / DeltaTime < 140.0f)
			{
				GroundLogOwnSlid += Moved;
				GroundLogBalls[0] += FVector::Dist2D(Ball, GroundBallWas[Side][0]);
				GroundLogBalls[1] += FVector::Dist2D(SeenBall, GroundBallWas[Side][1]);
			}

			bGroundOwnDown[Side] = bDown;
			GroundOwnWas[Side] = Now;
			GroundBallWas[Side][0] = Ball;
			GroundBallWas[Side][1] = SeenBall;
		}

		if (const float Went = FVector::Dist2D(GetActorLocation(), GroundLogPlace); Went < 200.0f)
		{
			GroundLogWent += Went;
		}

		GroundLogPlace = GetActorLocation();

		GroundLogSum[0] += AsPosed;
		GroundLogSum[1] += Lowest;
		GroundLogWorst[0] = FMath::Abs(AsPosed) > FMath::Abs(GroundLogWorst[0]) ? AsPosed : GroundLogWorst[0];
		GroundLogWorst[1] = FMath::Abs(Lowest) > FMath::Abs(GroundLogWorst[1]) ? Lowest : GroundLogWorst[1];

		if (++GroundLogCount >= 120)
		{
			UE_LOG(LogGut, Log, TEXT("Ground: %s (%s, state %d%s) lower sole as posed %.1f cm off the floor on average, worst %.1f; as seen %.1f, worst %.1f; feet that were down slid %.0f cm while the body went %.0f; slowly %.0f, and on the fighting skeleton %.0f; ankles %.1f to %.1f cm apart; while its ankle was down the ball of the foot went %.0f on the fighting skeleton and %.0f on the seen body"), *GetName(), *GetNameSafe(Mesh->GetOuter()),
				static_cast<int32>(State), bAtEase ? TEXT(", at ease") : TEXT(""), GroundLogSum[0] / GroundLogCount, GroundLogWorst[0], GroundLogSum[1] / GroundLogCount, GroundLogWorst[1], GroundLogSlid, GroundLogWent,
				GroundLogSlow, GroundLogOwnSlid, GroundLogApart[0], GroundLogApart[1], GroundLogBalls[0], GroundLogBalls[1]);

			GroundLogBalls[0] = GroundLogBalls[1] = 0.0f;

			GroundLogApart[0] = UE_BIG_NUMBER;
			GroundLogApart[1] = 0.0f;

			GroundLogSlid = GroundLogWent = GroundLogSlow = GroundLogOwnSlid = 0.0f;
			GroundLogSum[0] = GroundLogSum[1] = GroundLogWorst[0] = GroundLogWorst[1] = 0.0f;
			GroundLogCount = 0;
		}
	}
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

bool AGutFighter::GetStrikingPoint(FVector& OutPoint, int32* OutLimb) const
{
	// the right arm, unless found otherwise below
	if (OutLimb)
	{
		*OutLimb = 1;
	}

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

			if (OutLimb)
			{
				*OutLimb = 0;
			}

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

			if (OutLimb)
			{
				*OutLimb = i;
			}
		}
	}

	return Furthest > -UE_BIG_NUMBER;
}

float AGutFighter::GetBodyGapFrom(const FVector& Point, FName* OutPart, FVector* OutNearest) const
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

			if (OutNearest)
			{
				// on its skin, on the side the point is on
				const FVector Core = FMath::ClosestPointOnSegment(Point, From, To);

				*OutNearest = Core + ((Point - Core).GetSafeNormal() * Part.Thick);
			}

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

	if (Beast || !HasBody())
	{
		return;
	}

	// down onto the floor or up out of it, quickly but never in a jump
	GroundShift = FMath::FInterpTo(GroundShift, GroundWanted, DeltaTime, GutFighterConstants::GroundRate);

	if (State == EGutFighterState::Dead)
	{
		// Where it fell, and no longer held down or up. A body that has gone limp is where its own weight puts it: moving
		// it by hand every frame did nothing but have the engine complain 150,000 times a test sitting (21 November).
		if (!GetMesh()->IsSimulatingPhysics())
		{
			GetMesh()->SetRelativeLocation(MeshRestLocation + GetActorQuat().UnrotateVector(StrikeWay * StrikeShift) + FVector(0.0f, 0.0f, GroundShift));
		}

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

	GetMesh()->SetRelativeLocation(MeshRestLocation + GetActorQuat().UnrotateVector(StrikeWay * StrikeShift) + FVector(0.0f, 0.0f, GroundShift));
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
		// its own voice: nobody has the same throat. Each body speaks a little higher or lower than the recordings, always
		// the same for that body, and has about half of them to itself. (He heard every woman make the same sounds, 21 November.)
		static const bool bOneVoice = FParse::Param(FCommandLine::Get(), TEXT("GutOneVoice"));

		uint32 Whose = 0;

		if (!bOneVoice && !Beast && !Voice.IsNone() && Kind.ToString().EndsWith(Voice.ToString()) && (Kind == VoiceSound(TEXT("effort")) || Kind == VoiceSound(TEXT("pain")) || Kind == VoiceSound(TEXT("death"))))
		{
			Whose = FMath::Max(GetTypeHash(BodyClass.GetAssetName()), 1u);

			// from about a tone and a half down to two tones up
			Pitch *= FMath::Lerp(0.9f, 1.13f, static_cast<float>(Whose % 1000) / 999.0f);
		}

		Sounds->Play(Kind, Where ? *Where : GetActorLocation() + FVector(0.0f, 0.0f, 30.0f), Volume, Pitch, Whose);
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
	// nor are the feet of somebody who has just let go of walking and is coming to rest: a foot that steps under the body
	// as it stops is the body settling (he heard one or two steps after stopping, 17 November, and did not want them)
	const UCharacterMovementComponent* Legs = GetCharacterMovement();
	const bool bDriven = !LedVelocity.IsNearlyZero() || (Legs && (!Legs->GetCurrentAcceleration().IsNearlyZero(1.0) || Legs->HasRootMotionSources()));

	const bool bHeard = !bAtEase || (Going > 20.0f && bDriven);

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

			// nobody's foot comes down twice in a moment, or a moment after their other one. A foot that is held to the floor
			// and steps to catch the body up can land a second time just after the clip's own step did (measured 21 November:
			// two left footfalls a tenth of a second apart at the start of every walk): that is one footfall, heard once.
			const double Time = GetWorld()->GetTimeSeconds();
			const bool bTwice = Time - FootHeardAt[i] < (bAtEase ? 0.4 : 0.22) || Time - FootHeardAt[1 - i] < (bAtEase ? 0.18 : 0.1);

			if (bHeard && !bTwice)
			{
				FootHeardAt[i] = Time;

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

	// Everything the face is to show is gathered first and given to it once. Given piece by piece, each piece took the
	// place of the last on any control they shared: a face at ease had its blinks written and then written over by its
	// expression in the same frame, and nobody in the town ever blinked.
	TMap<FName, float> Shown;

	auto Show = [&Shown, Clock](const UAnimSequenceBase* Loop, float Weight)
	{
		const float Length = Loop ? Loop->GetPlayLength() : 0.0f;

		if (Length <= 0.0f || Weight <= 0.01f)
		{
			return;
		}

		FBlendedCurve Expression;
		Loop->EvaluateCurveData(Expression, FAnimExtractContext(FMath::Fmod(Clock, static_cast<double>(Length))), false);

		Expression.ForEachElement([&Shown, Weight](const UE::Anim::FCurveElement& Element)
		{
			Shown.FindOrAdd(Element.Name) += Element.Value * Weight;
		});
	};

	float LidsShut = 0.0f;

	// the three share the face between them
	const float Hurt = FaceHurtShown;
	const float Afraid = FaceAfraidShown * (1.0f - Hurt);

	// out of a fight a face is at rest, with a trace of what it wears in one: nobody is glared at for asking the way. The
	// fighting face has its blinks in it, so a face at rest is given its own.
	FaceFightShown = FMath::FInterpTo(FaceFightShown, bAtEase ? 0.14f : 1.0f, DeltaTime, 2.5f);

	if (bAtEase)
	{
		static const FName Lids[2] = { FName(TEXT("CTRL_expressions_eyeBlinkL")), FName(TEXT("CTRL_expressions_eyeBlinkR")) };

		const double Time = GetWorld()->GetTimeSeconds();

		if (BlinkAt <= 0.0 || Time > BlinkAt + 0.4)
		{
			BlinkAt = Time + 1.6 + (static_cast<double>((GetUniqueID() * 7919u + static_cast<uint32>(Time * 13.0)) % 1000u) / 1000.0) * 4.2;
		}

		// shut in six hundredths of a second and open again in nine
		const double Since = Time - BlinkAt;

		if (Since >= 0.0 && Since < 0.15)
		{
			const float Shut = Since < 0.06 ? static_cast<float>(Since / 0.06) : 1.0f - static_cast<float>((Since - 0.06) / 0.09);

			LidsShut = Shut;
		}
	}

	Show(FaceLoop, (1.0f - Hurt - Afraid) * FaceFightShown);
	Show(FaceHurt, Hurt);
	Show(FaceAfraid, Afraid);

	// and a line it is saying, as its recording was performed: the mouth, the jaw, the brows. Where the line and the
	// expression move the same part of the face, the line has it.
	if (SpeechFace)
	{
		const double Said = GetWorld()->GetTimeSeconds() - SpeechBegan;

		if (Said < 0.0 || Said >= SpeechFace->GetPlayLength())
		{
			SpeechFace = nullptr;
		}
		else
		{
			// eased in and out over a fifth of a second, so the mouth does not jump to its first shape or drop from its last
			const float Weight = FMath::Clamp(FMath::Min(Said, SpeechFace->GetPlayLength() - Said) / 0.2, 0.0, 1.0);

			FBlendedCurve Spoken;
			SpeechFace->EvaluateCurveData(Spoken, FAnimExtractContext(Said), false);

			Spoken.ForEachElement([&Shown, Weight](const UE::Anim::FCurveElement& Element)
			{
				float& Value = Shown.FindOrAdd(Element.Name);

				Value = FMath::Lerp(Value, Element.Value, Weight);
			});
		}
	}

	// The eyes. The head turns part of the way to whoever is looked at and the eyes go the rest: a head turned to somebody
	// with its eyes left where its expression had them looks past them.
	AimEyes(Shown, DeltaTime);

	// a blink shuts the lids whatever else the face is doing
	if (LidsShut > 0.0f)
	{
		static const FName Lids[2] = { FName(TEXT("CTRL_expressions_eyeBlinkL")), FName(TEXT("CTRL_expressions_eyeBlinkR")) };

		for (const FName& Lid : Lids)
		{
			float& Value = Shown.FindOrAdd(Lid);

			Value = FMath::Max(Value, LidsShut);
		}
	}

	for (const TPair<FName, float>& Control : Shown)
	{
		Driver->AddCurveValue(Control.Key, Control.Value);
	}
}

void AGutFighter::AimEyes(TMap<FName, float>& Shown, float DeltaTime)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoEyes"));
	static const FName EyeBones[2] = { FName(TEXT("FACIAL_L_Eye")), FName(TEXT("FACIAL_R_Eye")) };

	// to its own left, to its own right, up and down: each eye has its own of each
	static const FName Controls[4][2] =
	{
		{ FName(TEXT("CTRL_expressions_eyeLookLeftL")), FName(TEXT("CTRL_expressions_eyeLookLeftR")) },
		{ FName(TEXT("CTRL_expressions_eyeLookRightL")), FName(TEXT("CTRL_expressions_eyeLookRightR")) },
		{ FName(TEXT("CTRL_expressions_eyeLookUpL")), FName(TEXT("CTRL_expressions_eyeLookUpR")) },
		{ FName(TEXT("CTRL_expressions_eyeLookDownL")), FName(TEXT("CTRL_expressions_eyeLookDownR")) },
	};

	if (bOff || !BodyActor)
	{
		return;
	}

	if (!FaceSeen.IsValid())
	{
		for (UActorComponent* Component : BodyActor->GetComponents())
		{
			if (USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component); Part && Part->GetFName() == FaceComponentName)
			{
				FaceSeen = Part;
			}
		}
	}

	const USkeletalMeshComponent* Face = FaceSeen.Get();

	FVector Place;
	FVector2D Wanted = FVector2D::ZeroVector;
	bool bAimed = false;

	if (Face && GetLookAt(Place) && Face->GetBoneIndex(EyeBones[0]) != INDEX_NONE && Face->GetBoneIndex(EyeBones[1]) != INDEX_NONE)
	{
		const FVector Left = Face->GetBoneLocation(EyeBones[0]);
		const FVector Right = Face->GetBoneLocation(EyeBones[1]);
		const FVector Between = (Left + Right) * 0.5f;

		// the head as it is held now: across it from its left eye to its right, up, and the way it faces
		const FVector Across = (Right - Left).GetSafeNormal();
		const FVector Up = (FVector::UpVector - (Across * FVector::DotProduct(FVector::UpVector, Across))).GetSafeNormal();

		FVector Ahead = FVector::CrossProduct(Across, Up);

		if (FVector::DotProduct(Ahead, GetActorForwardVector()) < 0.0f)
		{
			Ahead = -Ahead;
		}

		const FVector To = (Place - Between).GetSafeNormal();

		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(To, Across), FVector::DotProduct(To, Ahead)));
		const float Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(To, Up), -1.0f, 1.0f)));

		// as far as eyes go: past that, somebody is not being looked at, and the eyes are left to the face
		if (!Across.IsNearlyZero() && FMath::Abs(Yaw) < 48.0f && FMath::Abs(Pitch) < 30.0f)
		{
			Wanted = FVector2D(Yaw, Pitch);
			bAimed = true;
		}
	}

	// Eyes do not rest on a point. They shift a little every second or so, and now and then go off somebody altogether
	// for a moment, down or to one side, and come back: nobody is stared at.
	const double Now = GetWorld()->GetTimeSeconds();

	if (Now >= EyesShiftAt)
	{
		if (bAimed && FMath::FRand() < 0.15f)
		{
			EyesOff = FVector2D(FMath::FRandRange(-15.0f, 15.0f), FMath::FRandRange(-11.0f, 2.0f));
			EyesShiftAt = Now + FMath::FRandRange(0.35f, 0.85f);
		}
		else
		{
			EyesOff = FVector2D(FMath::FRandRange(-2.0f, 2.0f), FMath::FRandRange(-1.3f, 1.3f));
			EyesShiftAt = Now + FMath::FRandRange(0.5f, 2.6f);
		}
	}

	EyesAimed = FMath::FInterpTo(EyesAimed, bAimed ? 1.0f : 0.0f, DeltaTime, 6.0f);

	if (bAimed)
	{
		// eyes move in jumps: there in a few hundredths of a second
		EyesShown = FMath::Vector2DInterpTo(EyesShown, Wanted + EyesOff, DeltaTime, 22.0f);
	}

	if (EyesAimed < 0.01f)
	{
		return;
	}

	// a control is at 1 with the eye turned about as far as it goes that way
	const float Turned[4] =
	{
		FMath::Clamp(-EyesShown.X / 40.0f, 0.0f, 1.0f), FMath::Clamp(EyesShown.X / 40.0f, 0.0f, 1.0f),
		FMath::Clamp(EyesShown.Y / 26.0f, 0.0f, 1.0f), FMath::Clamp(-EyesShown.Y / 30.0f, 0.0f, 1.0f),
	};

	for (int32 Way = 0; Way < 4; ++Way)
	{
		for (const FName& Control : Controls[Way])
		{
			float& Value = Shown.FindOrAdd(Control);

			Value = FMath::Lerp(Value, Turned[Way], EyesAimed);
		}
	}
}

void AGutFighter::Speak(UAnimSequenceBase* Face)
{
	SpeechFace = Face;
	SpeechBegan = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
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
			// top toward the hand, so it reads the right way up on an arm that is raised: which in the cage, where it is
			// worn, an arm always is. (It was toward the elbow, for an arm that hangs, and STOP was read upside down in
			// every stance and guard.)
			const FQuat Turn = FRotationMatrix::MakeFromZX(Along, Outward).ToQuat() * FRotationMatrix::MakeFromZX(FVector::ZAxisVector, Face).ToQuat().Inverse();

			WeaponMesh->AttachToComponent(Shown, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ArmBone);

			// Where on the arm. It sat at the forearm's middle and 13 cm off it, to keep a sign 78 cm across out of the
			// shoulder's armor and the side of the head: so far off that nothing held it, and so far across the body that
			// the other arm punched through it (25 November: "she is not holding the sign - see?"). Now it is close
			// enough that the closed hand is against its back as on a grip, a third of the way down the forearm so that
			// its top is at the eyes when the guard is up; and it is shifted along
			// its own width away from the body's middle, so that it covers the side it is worn on and leaves the other
			// arm its road. -GutShieldOff=, -GutShieldAlong= and -GutShieldAcross= try other places without a build.
			static float Off = 9.5f;
			static float AlongShare = 0.3f;
			static float AcrossBy = -14.0f;
			static bool bRead = false;

			if (!bRead)
			{
				bRead = true;

				FParse::Value(FCommandLine::Get(), TEXT("GutShieldOff="), Off);
				FParse::Value(FCommandLine::Get(), TEXT("GutShieldAlong="), AlongShare);
				FParse::Value(FCommandLine::Get(), TEXT("GutShieldAcross="), AcrossBy);
			}

			const FVector Across = FVector::CrossProduct(Along.GetSafeNormal(), Outward.GetSafeNormal());

			WeaponMesh->SetRelativeTransform(FTransform(Turn, (Along * AlongShare) + (Outward.GetSafeNormal() * Off) + (Across * AcrossBy), Weapon->GripTransform.GetScale3D()));
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
		else if (BoundClip)
		{
			// tied hands hang half shut
			Profile.bApplyTargetRetargetPose = true;
			Profile.TargetRetargetPoseName = GripPoseBound;
		}

		BodyRetarget->SetCustomRetargetProfile(Profile);

		MeasureHeld();

		bFingersShutForIt = Profile.bApplyTargetRetargetPose;
	}
}
