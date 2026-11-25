// Ashmarket prototype: the Gut.

#include "GutAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GutFighter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutAnimInstance)

namespace GutAnimConstants
{
	/** Speed at which the fighter is fully walking and no longer standing */
	const float MovingSpeed = 60.0f;

	/**
	 *  How fast the pose follows a change: in standing or walking, in which way, in guard up or down.
	 *  Standing or walking and the guard are eased both ways, as something with weight starts and stops, so that a guard
	 *  dropped for one frame and raised again is a twitch too small to see and not a jerk of the hands.
	 */
	const float MoveRate = 11.0f;
	const float WayRate = 9.0f;
	const float BlockRate = 22.0f;

	/** How far a body may stand turned from where its fighter faces before it steps round, and how far before it takes the long step */
	const float TurnStepAngle = 40.0f;
	const float TurnFarAngle = 68.0f;

	/** A body that has stood this far off for this long steps round anyway. Under that, it is left: nobody stands square. */
	const float AskewAngle = 26.0f;
	const float AskewWait = 1.2f;

	/** How fast a step round is played, and how long its legs take to come in and go out */
	const float TurnPlayRate = 1.15f;
	const float TurnBlendTime = 0.14f;

	/** How fast a body that is walking, striking or reeling comes round to face where its fighter faces */
	const float FaceRate = 13.0f;

	/** The chest never twists further than this from the hips. Past it the whole body is behind until the feet catch up. */
	const float MostTwist = 55.0f;

	/** How much of the twist each bone up the spine takes */
	const float TwistShare[5] = { 0.12f, 0.2f, 0.24f, 0.24f, 0.2f };

	/** A weapon in a closed fist cannot lie along the forearm: this is how near to along it the stance may ask for, as a cosine (about 40 degrees off it) */
	const float WristReach = 0.76f;

	/** How far from the middle of the neck, and of the skull, a weapon is kept, less its own thickness. And how fast the wrist follows. */
	const float NeckRadius = 10.0f;
	const float SkullRadius = 12.5f;
	const float SkullHeight = 12.0f;
	const float HoldRate = 24.0f;

	/** The hand counts as where the stance has it within the first of these, and as gone for the blow beyond the second */
	const float StanceReachNear = 12.0f;
	const float StanceReachFar = 42.0f;
	const float MostHoldTurn = 85.0f;

	/** Seconds to change from one weapon's stance to another's */
	const float IdleChangeTime = 0.3f;

	/** A walking clip is never played slower or faster than this, however fast the fighter goes */
	// a foot is down when its ankle is within this of where it stands at rest, and lifted when it is this far above it
	const float FootDownAt = 2.6f;
	const float FootLiftedAt = 5.0f;

	// and it is put down, not swinging through, if it goes no faster over the floor than this, or this share of the body's own speed
	const float FootStillSpeed = 26.0f;
	const float FootStillShare = 0.45f;

	// walking on its own legs faster than this, a held foot that goes over the ground at this share of the body's speed is being swung
	const float FootWalking = 30.0f;
	const float FootSwungShare = 0.85f;

	// a step taken to catch up: how fast the foot travels (so how long the step is, within limits), how high it is lifted
	// for how far it goes, how far ahead of the body it lands for how fast the body is carried, and how far behind the
	// second foot may fall before it steps while the first is still in the air
	const float FootStepSpeed = 230.0f;
	const float FootStepShortest = 0.1f;
	const float FootStepLongest = 0.2f;
	const float FootStepLiftShare = 0.22f;
	const float FootStepLiftLeast = 1.5f;
	const float FootStepLiftMost = 9.0f;
	const float FootStepLead = 0.09f;
	const float FootMostLead = 26.0f;
	const float FootBothStep = 1.7f;

	// the furthest a held foot is left turned from the way the clip has it before it steps round, and how fast a step turns it
	const float FootMostTurned = 32.0f;
	const float FootStepTurnSpeed = 260.0f;

	// a body carried no faster than this is at rest; after this long at rest a held foot that is further than this from
	// where the standing clip has it steps there
	const float FootRestSpeed = 10.0f;
	const float FootSettleAfter = 0.18f;
	const float FootSettleOff = 3.5f;

	// how high a foot that the clip slides forward along the floor is picked up, and how fast it comes up and goes down
	const float FootShuffleLift = 4.0f;
	const float FootLiftRate = 26.0f;

	// the furthest a held foot is kept from where the clip has it before it is let go, how fast it then comes back, and how
	// near it has to be before it can be put down again
	const float FootMostHeld = 20.0f;
	const float FootReturnRate = 16.0f;
	const float FootTakenAgain = 3.0f;

	// looking: how far round and up and down a head goes, past what angle to the side it is not tried at all, how the turn is
	// shared between the top of the back, the neck and the head, and how fast it comes on and goes round
	const float LookMostRound = 62.0f;
	const float LookMostUp = 24.0f;
	const float LookMostDown = 30.0f;
	const float LookGivenUp = 105.0f;
	const float LookShare[3] = { 0.14f, 0.3f, 0.56f };
	const float LookRate = 5.0f;

	// how much of the way from where the clip has the face to where it is to look the head is taken: not all of it, so
	// that what the clip does with the head still shows
	const float LookHowMuch = 0.9f;

	// how somebody at ease carries their head (degrees above level), how much of a clip's hanging of it is taken out, the
	// most it is raised by, and how fast what the clip does with it is averaged
	const float HeadCarried = -3.0f;
	const float HeadRaisedShare = 0.8f;
	const float HeadMostRaised = 26.0f;
	const float HeadHangRate = 1.2f;
	const float LookTurnSpeed = 150.0f;

	const float SlowestStep = 0.6f;
	const float FastestStep = 1.5f;

	/** Bones up the spine and how much of the upper body each carries. Everything above the last is all upper body. */
	const TPair<FName, float> Spine[] =
	{
		{ FName("spine_01"), 0.2f },
		{ FName("spine_02"), 0.45f },
		{ FName("spine_03"), 0.7f },
		{ FName("spine_04"), 0.9f },
		{ FName("spine_05"), 1.0f },
	};

	/** Where each arm begins, as far as being tied goes: the collarbones stay with the shoulders' own movement */
	const FName ArmRoots[] = { FName("upperarm_l"), FName("upperarm_r") };

	/** Tied wrists lie one across the other: each this far above or below the middle of the two, in centimetres */
	const float BoundStack = 1.5f;
}

namespace
{
	/** Two poses mixed into a third: how much of the first, the rest the second */
	void Mix(FPoseContext& First, FPoseContext& Second, float AmountOfFirst, FPoseContext& Result)
	{
		const FAnimationPoseData FirstData(First);
		const FAnimationPoseData SecondData(Second);
		FAnimationPoseData ResultData(Result);

		FAnimationRuntime::BlendTwoPosesTogether(FirstData, SecondData, AmountOfFirst, ResultData);
	}

	/** Two poses mixed into a third bone by bone: how much of the second each bone takes */
	void MixByBone(FPoseContext& First, FPoseContext& Second, const TArray<float>& AmountOfSecond, FPoseContext& Result)
	{
		const FAnimationPoseData FirstData(First);
		const FAnimationPoseData SecondData(Second);
		FAnimationPoseData ResultData(Result);

		FAnimationRuntime::BlendTwoPosesTogetherPerBone(FirstData, SecondData, AmountOfSecond, ResultData);
	}
}

namespace
{
	/** Moves a value toward a target the way something with weight does: no sudden start, no sudden stop, no going past */
	void Ease(float& Value, float& Speed, float Target, float Rate, float DeltaTime)
	{
		const float Off = Value - Target;
		const float Decay = FMath::Exp(-Rate * DeltaTime);
		const float Carry = Speed + (Rate * Off);

		Value = Target + ((Off + (Carry * DeltaTime)) * Decay);
		Speed = (Speed - (Carry * Rate * DeltaTime)) * Decay;
	}

	/** The nearest two points of two lines that each have ends */
	void NearestPoints(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1, FVector& OnA, FVector& OnB)
	{
		FMath::SegmentDistToSegmentSafe(A0, A1, B0, B1, OnA, OnB);
	}
}

void UGutAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AGutFighter* Fighter = Cast<AGutFighter>(TryGetPawnOwner());

	if (!Fighter)
	{
		return;
	}

	const FGutStanceAnimations& Stance = Fighter->GetStanceAnimations();

	Inputs.Idle = Stance.Idle;
	Inputs.bIdleFromStart = Fighter->IsStandingInFamily();
	Inputs.Block = Stance.Block;
	Inputs.BlockHold = Stance.BlockHold;
	Inputs.Locomotion = Fighter->GetLocomotionAnimations();
	Inputs.bKeepUpperBodyWhileMoving = Stance.bKeepUpperBodyWhileMoving;
	Inputs.bBlocking = Fighter->IsBlocking();
	Inputs.Bound = Fighter->GetBoundArms();
	Inputs.BoundPast = Fighter->GetBoundPast();

	// only what its own legs are doing: a shove from a blow or the carry of a lunge is not walking
	const UCharacterMovementComponent* Movement = Fighter->GetCharacterMovement();
	const bool bWalking = Movement && Movement->IsMovingOnGround() && !Movement->HasRootMotionSources();

	// or what it is led at, by whatever is moving it where its own movement cannot go
	const FVector Velocity = !Fighter->GetLedVelocity().IsNearlyZero() ? Fighter->GetLedVelocity() : (bWalking ? Fighter->GetVelocity() : FVector::ZeroVector);
	const FVector Local = Fighter->GetActorRotation().UnrotateVector(Velocity);

	Inputs.Speed = Local.Size2D();
	Inputs.Direction = Inputs.Speed > 1.0f ? FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)) : 0.0f;

	Inputs.bFighter = true;
	Inputs.ActorYaw = Fighter->GetActorRotation().Yaw;

	// a fighter that is suddenly somewhere else was put there, and did not get there by itself
	const FVector Place = Fighter->GetActorLocation();

	// on its feet on the ground, and not thrown across it: then a foot that is put down stays where it was put
	static const bool bNoFootHold = FParse::Param(FCommandLine::Get(), TEXT("GutNoFootHold"));

	const EGutFighterState Doing = Fighter->GetFighterState();

	Inputs.bHoldFeet = !bNoFootHold && Fighter->IsAlive() && !Fighter->IsBeast() && Movement && Movement->IsMovingOnGround()
		&& Doing != EGutFighterState::Dodging && Doing != EGutFighterState::Staggered && Doing != EGutFighterState::Finishing && !Fighter->IsBeingFinished();

	// somebody at ease walks a walk a person performed (Tools/make_style_motion.py), or jogs one: its feet are its own
	Inputs.bFeetFree = Fighter->IsAtEase();

	static const bool bHeadsHang = FParse::Param(FCommandLine::Get(), TEXT("GutHeadsHang"));

	Inputs.bHeadUp = !bHeadsHang && Fighter->IsAtEase() && Fighter->IsAlive() && !Fighter->IsBeast() && !Fighter->HangsHead();

	Inputs.bLook = Fighter->GetLookAt(Inputs.LookAt);

	Inputs.bSlotUpperOnly = Fighter->IsFidgeting();
	Inputs.bSlotKeepsHead = Fighter->IsFidgeting() && Fighter->DoesGestureKeepHead();

	Inputs.ArmsIn[0] = Fighter->GetArmIn(0);
	Inputs.ArmsIn[1] = Fighter->GetArmIn(1);
	Inputs.bCarried = Fighter->IsCarriedPose();

	Inputs.BlowReach = Fighter->GetBlowReach();
	Inputs.BlowLimb = Fighter->GetBlowLimb();

	Inputs.FloorHeight = Place.Z - Fighter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - ((Movement && Movement->CurrentFloor.bBlockingHit) ? Movement->CurrentFloor.GetDistanceToFloor() : 0.0f);
	Inputs.CarriedWay = DeltaSeconds > UE_SMALL_NUMBER ? FVector(Place.X - LastPlace.X, Place.Y - LastPlace.Y, 0.0) / DeltaSeconds : FVector::ZeroVector;

	// put somewhere, not carried there
	if (Inputs.CarriedWay.SizeSquared() > FMath::Square(1500.0))
	{
		Inputs.CarriedWay = FVector::ZeroVector;
	}

	Inputs.Carried = static_cast<float>(Inputs.CarriedWay.Size());

	if (FVector::DistSquared(Place, LastPlace) > FMath::Square(120.0f))
	{
		Placed();
	}

	LastPlace = Place;

	const EGutFighterState State = Fighter->GetFighterState();

	Inputs.bHolds = State == EGutFighterState::Idle || State == EGutFighterState::Blocking;

	// the weapon in the fist, if it is the kind that is gripped, and where the stance points it
	Inputs.bWeaponLine = Fighter->GetWeaponLine(Inputs.WeaponBase, Inputs.WeaponTip, Inputs.WeaponRadius);
	Inputs.bFingersShutForIt = Fighter->AreFingersShutForIt();

	// how much further from the hand the neck and the head of the body that is seen are than those of the skeleton that fights
	Inputs.NeckShift = FVector::ZeroVector;
	Inputs.HeadShift = FVector::ZeroVector;
	Inputs.HandTurn = FQuat::Identity;

	const USkeletalMeshComponent* Own = Fighter->GetMesh();
	const USkeletalMeshComponent* Seen = Fighter->GetShownMesh();

	if (Inputs.bWeaponLine && Seen && Seen != Own)
	{
		static const FName HandBone(TEXT("hand_r"));
		static const FName NeckBone(TEXT("neck_01"));
		static const FName HeadBone(TEXT("head"));

		if (Seen->GetBoneIndex(HandBone) != INDEX_NONE && Seen->GetBoneIndex(NeckBone) != INDEX_NONE && Seen->GetBoneIndex(HeadBone) != INDEX_NONE)
		{
			const FTransform Space = Own->GetComponentTransform();

			auto FromHand = [&Space](const USkeletalMeshComponent* Mesh, const FName& Bone)
			{
				return Space.InverseTransformVector(Mesh->GetBoneLocation(Bone) - Mesh->GetBoneLocation(HandBone));
			};

			Inputs.NeckShift = FromHand(Seen, NeckBone) - FromHand(Own, NeckBone);
			Inputs.HeadShift = FromHand(Seen, HeadBone) - FromHand(Own, HeadBone);

			// and how the seen hand is turned against the fighting skeleton's own hand, which the retargeting keeps much the same
			Inputs.HandTurn = (Own->GetBoneQuaternion(HandBone).Inverse() * Seen->GetBoneQuaternion(HandBone)).GetNormalized();
		}
	}

	// -GutHoldLog on the command line writes down, once a second, which way the weapon points on each skeleton and where the stance asks for it
	static const bool bHoldLog = FParse::Param(FCommandLine::Get(), TEXT("GutHoldLog"));

	if (bHoldLog && Inputs.bWeaponLine && Seen && GFrameCounter % 60 == 0)
	{
		const FTransform Actor = Fighter->GetActorTransform();
		const FQuat OwnHand = Own->GetBoneQuaternion(FName("hand_r"));
		const FQuat SeenHand = Seen->GetBoneQuaternion(FName("hand_r"));
		const FVector Line = (Inputs.WeaponTip - Inputs.WeaponBase).GetSafeNormal();

		UE_LOG(LogGut, Log, TEXT("[hold] %s state %d: weapon points, in its own space, %s on the seen hand and %s on the fighting one; the stance asks for %s; hand turn %s; forearm %s"),
			*Fighter->GetName(), static_cast<int32>(State),
			*Actor.InverseTransformVectorNoScale(SeenHand.RotateVector(Line)).ToCompactString(),
			*Actor.InverseTransformVectorNoScale(OwnHand.RotateVector(Line)).ToCompactString(),
			*Stance.WeaponAim.GetSafeNormal().ToCompactString(),
			*Inputs.HandTurn.Rotator().ToCompactString(),
			*Actor.InverseTransformVectorNoScale((Seen->GetBoneLocation(FName("hand_r")) - Seen->GetBoneLocation(FName("lowerarm_r"))).GetSafeNormal()).ToCompactString());
	}

	const FTransform MeshInActor = Fighter->GetMesh()->GetRelativeTransform();

	Inputs.WeaponAim = MeshInActor.InverseTransformVectorNoScale(Stance.WeaponAim);
	Inputs.BlockWeaponAim = MeshInActor.InverseTransformVectorNoScale(Stance.BlockWeaponAim);
}

void UGutAnimInstance::SmoothOver(float Seconds)
{
	++Inputs.SmoothSerial;
	Inputs.SmoothTime = FMath::Max(Seconds, 0.05f);
}

void UGutAnimInstance::Placed()
{
	++Inputs.PlaceSerial;
}

void UGutAnimInstance::SetLook(bool bLook, const FVector& At)
{
	Inputs.bLook = bLook;
	Inputs.LookAt = At;

	// somebody standing in a crowd, with nothing just now to look at: head up, as anybody at ease carries it
	Inputs.bHeadUp = true;
}

void UGutAnimInstance::SetIdle(UAnimSequenceBase* Idle, float StartTime, float PlayRate, bool bFollowsOn)
{
	Inputs.Idle = Idle;
	Inputs.IdleStart = StartTime;
	Inputs.IdleRate = PlayRate;
	Inputs.bIdleFromStart = bFollowsOn;
}

FAnimInstanceProxy* UGutAnimInstance::CreateAnimInstanceProxy()
{
	return new FGutAnimInstanceProxy(this);
}

void FGutAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (const UGutAnimInstance* Instance = Cast<UGutAnimInstance>(InAnimInstance))
	{
		Inputs = Instance->GetInputs();
	}
}

void FGutAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	using namespace GutAnimConstants;

	const float DeltaTime = InContext.GetDeltaTime();

	// the slot is where attacks, dodges and blows are played. It has to be made known again whenever the instance starts over.
	if (!SlotCounter.IsSynchronized_Counter(GetSlotNodeInitializationCounter()))
	{
		SlotCounter.SynchronizeWith(GetSlotNodeInitializationCounter());

		RegisterSlotNodeWithAnimInstance(SlotName);
	}

	GetSlotWeight(SlotName, SlotWeight, SlotSourceWeight, SlotTotalWeight);
	UpdateSlotNodeWeight(SlotName, SlotWeight, 1.0f);

	// a new weapon is a new way of standing: change over to it, do not jump
	if (Inputs.Idle != CurrentIdle)
	{
		if (!CurrentIdle)
		{
			IdleTime = Inputs.IdleStart;
		}

		PreviousIdle = CurrentIdle;
		PreviousIdleTime = IdleTime;
		CurrentIdle = Inputs.Idle;
		IdleChange = PreviousIdle ? 0.0f : 1.0f;

		// one of a family of standing clips follows on from the last: it begins at its beginning, and the last runs on to its
		// end underneath while it is left, which takes a little longer than a change of weapon
		IdleChangeLength = IdleChangeTime;

		if (Inputs.bIdleFromStart && PreviousIdle)
		{
			IdleTime = 0.0f;
			IdleChangeLength = 0.55f;
		}
	}

	IdleChange = FMath::Min(1.0f, IdleChange + (DeltaTime / IdleChangeLength));
	PreviousIdleTime += DeltaTime * Inputs.IdleRate;

	if (CurrentIdle && CurrentIdle->GetPlayLength() > 0.0f)
	{
		IdleTime = FMath::Fmod(IdleTime + (DeltaTime * Inputs.IdleRate), CurrentIdle->GetPlayLength());
	}

	if (Inputs.Block && Inputs.Block->GetPlayLength() > 0.0f)
	{
		BlockTime = Inputs.BlockHold >= 0.0f ? FMath::Min(Inputs.BlockHold, Inputs.Block->GetPlayLength()) : FMath::Fmod(BlockTime + DeltaTime, Inputs.Block->GetPlayLength());
	}

	Elapsed += DeltaTime;

	Ease(BlockAmount, BlockSpeed, Inputs.bBlocking && Inputs.Block ? 1.0f : 0.0f, BlockRate, DeltaTime);
	Ease(MoveAmount, MoveSpeed, FMath::Clamp(Inputs.Speed / MovingSpeed, 0.0f, 1.0f), MoveRate, DeltaTime);

	BlockAmount = FMath::Clamp(BlockAmount, 0.0f, 1.0f);
	MoveAmount = FMath::Clamp(MoveAmount, 0.0f, 1.0f);

	// a fighter that is turned keeps its feet where they are: the body is now that much behind where the fighter faces
	if (Inputs.bFighter)
	{
		if (!bYawKnown || PlaceSerial != Inputs.PlaceSerial)
		{
			bYawKnown = true;
			PlaceSerial = Inputs.PlaceSerial;
			LastActorYaw = Inputs.ActorYaw;

			BodyYaw = 0.0f;
			BodyYawSpeed = 0.0f;
			bTurning = false;
			TurnAmount = 0.0f;
			PosesKept = 0;
			bCarrying = false;
			HoldTurn = FQuat::Identity;
		}

		BodyYaw = FRotator::NormalizeAxis(BodyYaw - FMath::FindDeltaAngleDegrees(LastActorYaw, Inputs.ActorYaw));
		LastActorYaw = Inputs.ActorYaw;

		const bool bStanding = Inputs.bHolds && Inputs.Speed < MovingSpeed * 0.5f && SlotWeight < 0.5f;

		if (!bStanding)
		{
			// walking, striking, reeling: the feet are moving anyway, and the body comes round
			bTurning = false;
			AskewTime = 0.0f;

			Ease(BodyYaw, BodyYawSpeed, 0.0f, FaceRate, DeltaTime);
		}
		else
		{
			BodyYawSpeed = 0.0f;

			AskewTime = FMath::Abs(BodyYaw) > AskewAngle ? AskewTime + DeltaTime : 0.0f;

			if (!bTurning && (FMath::Abs(BodyYaw) > TurnStepAngle || AskewTime > AskewWait))
			{
				if (const FGutTurnClip* Step = ChooseTurn(BodyYaw))
				{
					Turn = *Step;
					TurnTime = 0.0f;
					TurnDone = 0.0f;
					TurnOwed = BodyYaw;
					bTurning = true;
					AskewTime = 0.0f;
				}
				else
				{
					// nothing to step round with: come round as a walking body does
					Ease(BodyYaw, BodyYawSpeed, 0.0f, FaceRate, DeltaTime);
				}
			}

			if (bTurning)
			{
				// the step takes out the turn that was owed when it began, as fast as the body in the clip really turned
				TurnTime += DeltaTime * TurnPlayRate;

				const float Done = Turn.GetProgress(TurnTime);

				BodyYaw -= TurnOwed * (Done - TurnDone);
				TurnDone = Done;

				if (TurnTime >= Turn.Animation->GetPlayLength())
				{
					bTurning = false;
				}
			}
		}

		TurnAmount = FMath::FInterpConstantTo(TurnAmount, bTurning ? 1.0f : 0.0f, DeltaTime, 1.0f / TurnBlendTime);
	}

	// which way: shared between the two of forward, right, back and left that it lies between.
	// It is the legs that walk, so it is which way the legs face that counts.
	if (Inputs.Speed > 1.0f)
	{
		const float Quarter = FMath::Fmod(Inputs.Direction - BodyYaw + 720.0f, 360.0f) / 90.0f;
		const int32 First = FMath::FloorToInt32(Quarter) % 4;
		const float Share = Quarter - FMath::FloorToFloat(Quarter);

		float Wanted[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		Wanted[First] = 1.0f - Share;
		Wanted[(First + 1) % 4] = Share;

		float Total = 0.0f;

		for (int32 i = 0; i < 4; ++i)
		{
			WayAmount[i] = FMath::FInterpTo(WayAmount[i], Wanted[i], DeltaTime, WayRate);
			Total += WayAmount[i];
		}

		for (int32 i = 0; i < 4; ++i)
		{
			WayAmount[i] = Total > UE_SMALL_NUMBER ? WayAmount[i] / Total : (i == 0 ? 1.0f : 0.0f);
		}
	}

	// how fast the walking clips and the running clips go, and how long their steps take, the way they are mixed now
	float WalkSpeed = 0.0f;
	float RunSpeed = 0.0f;
	float WalkTime = 0.0f;
	float RunTime = 0.0f;

	// and flat out, where there is a clip for it: forward only
	const FGutMoveClip& Sprint = Inputs.Locomotion.Sprint;

	for (int32 i = 0; i < 4; ++i)
	{
		const FGutMoveClip& Walk = Inputs.Locomotion.GetWalk(i);
		const FGutMoveClip& Run = Inputs.Locomotion.GetRun(i).Animation ? Inputs.Locomotion.GetRun(i) : Walk;

		WalkSpeed += WayAmount[i] * Walk.Speed;
		RunSpeed += WayAmount[i] * Run.Speed;
		WalkTime += WayAmount[i] * Walk.GetCycleTime();
		RunTime += WayAmount[i] * Run.GetCycleTime();
	}

	// 0 a walk, 1 a run, 2 flat out, and anywhere between
	float WantedRun = RunSpeed > WalkSpeed + 1.0f ? FMath::Clamp((Inputs.Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.0f, 1.0f) : 0.0f;

	if (Sprint.Animation && Sprint.Speed > RunSpeed + 1.0f && Inputs.Speed > RunSpeed)
	{
		WantedRun = 1.0f + FMath::Clamp((Inputs.Speed - RunSpeed) / (Sprint.Speed - RunSpeed), 0.0f, 1.0f);
	}

	RunAmount = FMath::FInterpTo(RunAmount, WantedRun, DeltaTime, 7.0f);

	const float Faster = FMath::Clamp(RunAmount - 1.0f, 0.0f, 1.0f);
	const float ClipSpeed = FMath::Lerp(FMath::Lerp(WalkSpeed, RunSpeed, FMath::Min(RunAmount, 1.0f)), Sprint.Speed, Faster);
	const float StepTime = FMath::Lerp(FMath::Lerp(WalkTime, RunTime, FMath::Min(RunAmount, 1.0f)), Sprint.GetCycleTime(), Faster);

	// a fighter that sets off from standing sets off on the foot the walking clips begin on, which is the one its stance has forward
	if (MoveAmount < 0.02f)
	{
		StepPhase = 0.0f;
		StepCount = 0.0f;
	}

	if (ClipSpeed > 1.0f && StepTime > UE_SMALL_NUMBER)
	{
		const float Rate = FMath::Clamp(Inputs.Speed / ClipSpeed, SlowestStep, FastestStep);

		const float Next = StepPhase + (DeltaTime * Rate / StepTime);

		// which cycle it is on, for a clip that holds more than one: its second is not always its first again
		StepCount = FMath::Fmod(StepCount + FMath::FloorToFloat(Next), 64.0f);
		StepPhase = FMath::Frac(Next);
	}
}

void FGutAnimInstanceProxy::Sample(FPoseContext& Pose, const UAnimSequenceBase* Clip, float Time)
{
	if (!Clip)
	{
		Pose.ResetToRefPose();
		return;
	}

	FAnimationPoseData Data(Pose);

	Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, {}, true));
}

const TArray<float>& FGutAnimInstanceProxy::GetUpperBody(const FBoneContainer& Bones)
{
	if (bUpperBodyBuilt && UpperBodySerial == Bones.GetSerialNumber())
	{
		return UpperBody;
	}

	bUpperBodyBuilt = true;
	UpperBodySerial = Bones.GetSerialNumber();

	const int32 Count = Bones.GetCompactPoseNumBones();

	UpperBody.Init(0.0f, Count);
	Arms.Init(0.0f, Count);

	const FReferenceSkeleton& Skeleton = Bones.GetReferenceSkeleton();

	// parents come before children, so each bone can take its parent's share unless it is one of the spine's own
	for (FCompactPoseBoneIndex Bone(0); Bone.GetInt() < Count; ++Bone)
	{
		const FName Name = Skeleton.GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt());
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);

		float Share = Parent.GetInt() >= 0 ? UpperBody[Parent.GetInt()] : 0.0f;

		for (const TPair<FName, float>& Step : GutAnimConstants::Spine)
		{
			if (Step.Key == Name)
			{
				Share = Step.Value;
			}
		}

		UpperBody[Bone.GetInt()] = Share;

		// an arm is everything from its upper arm down
		Arms[Bone.GetInt()] = (Parent.GetInt() >= 0 && Arms[Parent.GetInt()] > 0.0f) || Name == GutAnimConstants::ArmRoots[0] || Name == GutAnimConstants::ArmRoots[1] ? 1.0f : 0.0f;
	}

	// and the bones that are turned by hand
	auto Find = [&Bones](const TCHAR* Name)
	{
		const int32 Index = Bones.GetPoseBoneIndexForBoneName(FName(Name));

		return Index != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)) : FCompactPoseBoneIndex(INDEX_NONE);
	};

	Named.Root = FCompactPoseBoneIndex(0);
	Named.Spine[0] = Find(TEXT("spine_01"));
	Named.Spine[1] = Find(TEXT("spine_02"));
	Named.Spine[2] = Find(TEXT("spine_03"));
	Named.Spine[3] = Find(TEXT("spine_04"));
	Named.Spine[4] = Find(TEXT("spine_05"));
	Named.Neck = Find(TEXT("neck_01"));
	Named.Head = Find(TEXT("head"));
	Named.Forearm = Find(TEXT("lowerarm_r"));
	Named.Hand = Find(TEXT("hand_r"));

	Named.Leg[0][0] = Find(TEXT("thigh_l"));
	Named.Leg[0][1] = Find(TEXT("calf_l"));
	Named.Leg[0][2] = Find(TEXT("foot_l"));
	Named.Leg[1][0] = Find(TEXT("thigh_r"));
	Named.Leg[1][1] = Find(TEXT("calf_r"));
	Named.Leg[1][2] = Find(TEXT("foot_r"));

	Named.Ball[0] = Find(TEXT("ball_l"));
	Named.Ball[1] = Find(TEXT("ball_r"));

	// how high its ankle stands off the floor, and the ball of its foot
	if (const int32 Ankle = Skeleton.FindBoneIndex(FName(TEXT("foot_l"))); Ankle != INDEX_NONE)
	{
		AnkleRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Ankle).GetLocation().Z;
	}

	// which way the head's own bone has the face, found from how it stands at rest: whichever of its axes points most
	// nearly the way the body faces
	if (const int32 Head = Skeleton.FindBoneIndex(FName(TEXT("head"))); Head != INDEX_NONE)
	{
		const FQuat AtRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Head).GetRotation();

		float Best = -2.0f;

		for (const FVector& Axis : { FVector::ForwardVector, FVector::RightVector, FVector::UpVector, -FVector::ForwardVector, -FVector::RightVector, -FVector::UpVector })
		{
			if (const float Along = static_cast<float>(FVector::DotProduct(AtRest.RotateVector(Axis), FVector(0.0, 1.0, 0.0))); Along > Best)
			{
				Best = Along;
				HeadFaces = Axis;
			}
		}
	}

	if (const int32 Ball = Skeleton.FindBoneIndex(FName(TEXT("ball_l"))); Ball != INDEX_NONE)
	{
		BallRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Ball).GetLocation().Z;
	}

	Named.Arm[0][0] = Find(TEXT("upperarm_l"));
	Named.Arm[0][1] = Find(TEXT("lowerarm_l"));
	Named.Arm[0][2] = Find(TEXT("hand_l"));
	Named.Arm[1][0] = Find(TEXT("upperarm_r"));
	Named.Arm[1][1] = Find(TEXT("lowerarm_r"));
	Named.Arm[1][2] = Find(TEXT("hand_r"));

	// everything below a wrist
	Fingers.Reset();

	for (const FCompactPoseBoneIndex Bone : Bones.ForEachCompactPoseBoneIndex())
	{
		for (FCompactPoseBoneIndex Up = Bones.GetParentBoneIndex(Bone); Up.GetInt() >= 0; Up = Bones.GetParentBoneIndex(Up))
		{
			if (Up == Named.Arm[0][2] || Up == Named.Arm[1][2])
			{
				Fingers.Add(Bone);
				break;
			}
		}
	}

	return UpperBody;
}

FTransform FGutAnimInstanceProxy::InBody(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
{
	FTransform Result = Pose[Bone];

	for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent.GetInt() >= 0; Parent = Pose.GetParentBoneIndex(Parent))
	{
		Result *= Pose[Parent];
	}

	return Result;
}

void FGutAnimInstanceProxy::LookToward(FPoseContext& Output, float DeltaTime)
{
	using namespace GutAnimConstants;

	if (Named.Head.GetInt() < 0 || Named.Neck.GetInt() < 0 || DeltaTime <= 0.0f)
	{
		return;
	}

	// straight ahead for the body, in the space of its own mesh
	const FVector Ahead(0.0f, 1.0f, 0.0f);

	// The performer whose walk and standing the town has looked at the floor of the room he was recorded in, in nearly
	// everything: a street of people all studying their feet. Somebody at ease carries their head up. How far the clip
	// hangs it is taken over a second or so, and most of that is given back, so that what the head does from moment to
	// moment in the clip (a nod, a glance) is still done, from a head held level.
	if (Inputs.bHeadUp)
	{
		const FVector Hung = InBody(Output.Pose, Named.Head).GetRotation().RotateVector(HeadFaces).GetSafeNormal(UE_SMALL_NUMBER, Ahead);
		const float Hangs = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Hung.Z, -1.0f, 1.0f)));

		HeadHangs = bHeadKnown ? FMath::FInterpTo(HeadHangs, Hangs, DeltaTime, HeadHangRate) : Hangs;
		bHeadKnown = true;
	}
	else
	{
		bHeadKnown = false;
	}

	const float Raise = Inputs.bHeadUp ? FMath::Clamp((HeadCarried - HeadHangs) * HeadRaisedShare, 0.0f, HeadMostRaised) * (1.0f - FMath::Clamp(SlotWeight, 0.0f, 1.0f)) : 0.0f;

	HeadRaised = FMath::FInterpTo(HeadRaised, Raise, DeltaTime, LookRate);

	if (HeadRaised > 0.1f)
	{
		// about the body's own right-to-left line, which lifts the face
		const FQuat Lift(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(HeadRaised));
		const FCompactPoseBoneIndex Lifted[3] = { Named.Spine[4], Named.Neck, Named.Head };

		for (int32 i = 0; i < 3; ++i)
		{
			if (Lifted[i].GetInt() < 0)
			{
				continue;
			}

			const FQuat Part = FQuat::Slerp(FQuat::Identity, Lift, LookShare[i]);
			const FQuat Parent = InBody(Output.Pose, Output.Pose.GetParentBoneIndex(Lifted[i])).GetRotation();
			const FQuat Own = InBody(Output.Pose, Lifted[i]).GetRotation();

			Output.Pose[Lifted[i]].SetRotation((Parent.Inverse() * Part * Own).GetNormalized());
		}
	}

	FVector Wanted = Ahead;
	bool bLooking = false;

	if (Inputs.bLook)
	{
		const FTransform& InWorld = GetComponentTransform();
		const FVector Eyes = InWorld.TransformPosition(InBody(Output.Pose, Named.Head).GetLocation());

		FVector Way = InWorld.InverseTransformVectorNoScale(Inputs.LookAt - Eyes);

		if (Way.Normalize())
		{
			// round to the side, and up or down, each no further than a neck goes
			const float Round = FMath::RadiansToDegrees(FMath::Atan2(-Way.X, Way.Y));
			const float Raised = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Way.Z, -1.0f, 1.0f)));

			// behind the body: nobody's head goes round that far, and it is left alone
			if (FMath::Abs(Round) < LookGivenUp)
			{
				const float Yaw = FMath::DegreesToRadians(FMath::Clamp(Round, -LookMostRound, LookMostRound));
				const float Pitch = FMath::DegreesToRadians(FMath::Clamp(Raised, -LookMostDown, LookMostUp));

				Wanted = FVector(-FMath::Sin(Yaw) * FMath::Cos(Pitch), FMath::Cos(Yaw) * FMath::Cos(Pitch), FMath::Sin(Pitch));
				bLooking = true;
			}
		}
	}

	// not while something is playing over the body: a head that is knocked back is not held on its target
	const float Amount = bLooking ? 1.0f - FMath::Clamp(SlotWeight, 0.0f, 1.0f) : 0.0f;

	LookAmount = FMath::FInterpTo(LookAmount, Amount, DeltaTime, LookRate);

	if (bLooking)
	{
		LookWay = FMath::VInterpNormalRotationTo(LookWay, Wanted, DeltaTime, LookTurnSpeed);
	}

	if (LookAmount < 0.01f)
	{
		return;
	}

	// from the way the clip has the face turned now, whichever way that is, to the way it is to look: a clip that hangs
	// its head, as the ones made for watching a pit from above do, does not go on hanging it at somebody's knees
	const FVector Faces = InBody(Output.Pose, Named.Head).GetRotation().RotateVector(HeadFaces).GetSafeNormal(UE_SMALL_NUMBER, Ahead);
	const FQuat Whole = FQuat::FindBetweenNormals(Faces, LookWay.GetSafeNormal(UE_SMALL_NUMBER, Ahead));

	// shared out up the back of the neck, most of it in the head itself
	const FCompactPoseBoneIndex Bones[3] = { Named.Spine[4], Named.Neck, Named.Head };

	for (int32 i = 0; i < 3; ++i)
	{
		if (Bones[i].GetInt() < 0)
		{
			continue;
		}

		const FQuat Part = FQuat::Slerp(FQuat::Identity, Whole, LookShare[i] * LookAmount * LookHowMuch);

		const FQuat Parent = InBody(Output.Pose, Output.Pose.GetParentBoneIndex(Bones[i])).GetRotation();
		const FQuat Own = InBody(Output.Pose, Bones[i]).GetRotation();

		Output.Pose[Bones[i]].SetRotation((Parent.Inverse() * Part * Own).GetNormalized());
	}
}

void FGutAnimInstanceProxy::HoldFeet(FPoseContext& Output, float DeltaTime)
{
	using namespace GutAnimConstants;

	if (!Inputs.bHoldFeet || Named.Leg[0][2].GetInt() < 0 || Named.Leg[1][2].GetInt() < 0 || Named.Leg[0][0].GetInt() < 0 || Named.Leg[1][0].GetInt() < 0 || DeltaTime <= 0.0f)
	{
		// off its feet, or just put somewhere: nothing is held, and nothing is owed
		bFootKnown = false;
		bFootHeld[0] = bFootHeld[1] = false;
		bFootStepping[0] = bFootStepping[1] = false;
		FootLift[0] = FootLift[1] = 0.0f;
		FootTurn[0] = FootTurn[1] = 0.0f;
		FootOff[0] = FootOff[1] = FVector::ZeroVector;
		return;
	}

	const FTransform& InWorld = GetComponentTransform();

	if (FeetPlaceSerial != Inputs.PlaceSerial)
	{
		FeetPlaceSerial = Inputs.PlaceSerial;
		bFootKnown = false;
		bFootHeld[0] = bFootHeld[1] = false;
		bFootStepping[0] = bFootStepping[1] = false;
		FootLift[0] = FootLift[1] = 0.0f;
		FootTurn[0] = FootTurn[1] = 0.0f;
		FootOff[0] = FootOff[1] = FVector::ZeroVector;
	}

	// walking on its own legs the clip does the stepping, and says which foot is down by which is still against the floor.
	// Carried any other way (the push of a lunge, a shove from a blow, the reach of a strike) the clip's feet stay where
	// they are under the body, and both are down.
	const bool bOwnLegs = Inputs.Speed > FootWalking;

	// a step lands a little ahead of where the body is, the way it is going, so that it is not behind again at once
	const FVector Lead = (Inputs.CarriedWay * FootStepLead).GetClampedToMaxSize2D(FootMostLead);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FTransform Foot = InBody(Output.Pose, Named.Leg[Side][2]);

		// where the clip has the foot, in the world: its ankle, and the ball of it. A foot stands on whichever of the two is
		// nearer the floor: somebody in a fighting stance has the back heel up and stands on the ball of that foot, and it is
		// planted all the same. How high that is, and how fast it is going over the ground.
		const FVector Ankle = InWorld.TransformPosition(Foot.GetLocation());
		const FVector Ball = Named.Ball[Side].GetInt() >= 0 ? InWorld.TransformPosition(InBody(Output.Pose, Named.Ball[Side]).GetLocation()) : Ankle;

		const float AnkleUp = Ankle.Z - Inputs.FloorHeight - AnkleRest;
		const float BallUp = Named.Ball[Side].GetInt() >= 0 ? Ball.Z - Inputs.FloorHeight - BallRest : UE_BIG_NUMBER;

		// with a little to spare, so that a flat foot does not go from one to the other and back
		const bool bOnBall = BallUp < AnkleUp - (bFootOnBall[Side] ? 0.3f : 1.2f);
		const FVector Stood = bOnBall ? Ball : Ankle;

		const float Up = FMath::Min(AnkleUp, BallUp);
		const float Going = (bFootKnown && bOnBall == bFootOnBall[Side]) ? FVector::Dist2D(Stood, FootWas[Side]) / DeltaTime : (bFootKnown ? FootWent[Side] : UE_BIG_NUMBER);
		const bool bLow = Up < FootDownAt;

		// and which way the clip has it turned, in the world: a body that turns to keep facing somebody turns its feet with
		// it, and a planted foot swivelling on the floor is sliding as much as one that is dragged
		const FQuat Turned = InWorld.GetRotation() * Foot.GetRotation();

		// held by the one, and now standing on the other: held where that one is seen to be
		if (bOnBall != bFootOnBall[Side] && bFootHeld[Side])
		{
			FootHeldAt[Side] = Stood + FVector(FootOff[Side].X, FootOff[Side].Y, 0.0f);
		}

		bFootOnBall[Side] = bOnBall;
		FootWas[Side] = Stood;
		FootWent[Side] = Going;

		// everything below is said of the point the foot stands on; the leg is bent to the ankle, which goes with it
		const FVector Asked = Stood;

		// A walk performed by a person plants its own feet, and rolls each from the heel to the toe as it does: held by the
		// ankle, that roll was taken for sliding and stopped, and the foot stepped three times a stride to catch up
		// (measured 21 November). Such a walk is let alone: whatever a foot was held back by dies away as it sets off.
		const bool bFree = Inputs.bFeetFree && bOwnLegs;

		if (bFree)
		{
			bFootHeld[Side] = false;
			bFootStepping[Side] = false;
		}

		if (bFootStepping[Side])
		{
			// in the air, from where it was held to where the clip has it now: up and over, never along the floor
			FootStepTime[Side] += DeltaTime / FootStepLength[Side];

			const float Done = FMath::Clamp(FootStepTime[Side], 0.0f, 1.0f);
			const FVector To = FVector(Asked.X + Lead.X, Asked.Y + Lead.Y, Asked.Z);

			FVector Now = FMath::Lerp(FootStepFrom[Side], To, FMath::SmoothStep(0.0f, 1.0f, Done));

			Now.Z = Asked.Z + (FootStepLift[Side] * FMath::Sin(Done * UE_PI));

			FootOff[Side] = Now - Asked;

			// and round to the way the clip has it turned, while it is in the air
			FootTurn[Side] = FootStepTurn[Side] * (1.0f - FMath::SmoothStep(0.0f, 1.0f, Done));

			if (Done >= 1.0f)
			{
				// down, and held where it came down, turned as it came down
				bFootStepping[Side] = false;
				bFootHeld[Side] = Up < FootLiftedAt;
				FootHeldAt[Side] = To;
				FootHeldTurned[Side] = Turned;
				FootOff[Side] = To - Asked;
				FootTurn[Side] = 0.0f;
			}
		}
		else if (bFootHeld[Side])
		{
			FootOff[Side] = FootHeldAt[Side] - Asked;
			FootOff[Side].Z = 0.0f;

			const float Behind = FootOff[Side].Size2D();

			// how far the clip has turned it since it was put down, about the upright: that much is taken back off
			FQuat Tilted;
			FQuat Round;

			(Turned * FootHeldTurned[Side].Inverse()).ToSwingTwist(FVector::UpVector, Tilted, Round);

			FootTurn[Side] = -FRotator::NormalizeAxis(FMath::RadiansToDegrees(Round.GetTwistAngle(FVector::UpVector)));

			// a foot the clip is swinging forward along the floor, as a shuffling step does without ever lifting it:
			// walking on its own legs, a foot that goes over the ground about as fast as the body does is not planted
			const bool bSwung = bOwnLegs && Going > FMath::Max(FootStillSpeed * 2.0f, Inputs.Speed * FootSwungShare);

			// come to rest with a foot still where the last step left it: nobody stands like that. Once the body has been
			// still a moment, each foot in turn is brought under it, where the standing clip has it.
			FootRest[Side] = (!bOwnLegs && Inputs.Carried < FootRestSpeed) ? FootRest[Side] + DeltaTime : 0.0f;

			const bool bSettling = FootRest[Side] > FootSettleAfter && Behind > FootSettleOff;

			if (Up > FootLiftedAt)
			{
				// lifted by the clip: let go, and what it was held back by dies away while it is in the air
				bFootHeld[Side] = false;
			}
			else if ((Behind > FootMostHeld || FMath::Abs(FootTurn[Side]) > FootMostTurned || bSettling || (bSwung && Behind > FootTakenAgain)) && (!bFootStepping[1 - Side] || Behind > FootMostHeld * FootBothStep))
			{
				// the leg would have to stretch: it takes a step. One foot at a time, unless this one is left too far behind.
				bFootHeld[Side] = false;
				bFootStepping[Side] = true;
				FootStepFrom[Side] = FVector(FootHeldAt[Side].X, FootHeldAt[Side].Y, Asked.Z);
				FootStepTurn[Side] = FootTurn[Side];
				FootStepTime[Side] = 0.0f;
				FootRest[Side] = 0.0f;
				FootStepLength[Side] = FMath::Clamp(FMath::Max(Behind / FootStepSpeed, FMath::Abs(FootTurn[Side]) / FootStepTurnSpeed), FootStepShortest, FootStepLongest);
				FootStepLift[Side] = FMath::Clamp(Behind * FootStepLiftShare, FootStepLiftLeast, FootStepLiftMost);
			}
			else if (bSwung)
			{
				// swung from where the clip has it anyway: nothing to catch up
				bFootHeld[Side] = false;
			}
		}
		else
		{
			// what it was let go with dies away: that is the foot coming back to where the clip has it, in the air
			FootOff[Side] *= FMath::Exp(-FootReturnRate * DeltaTime);
			FootOff[Side].Z = 0.0f;
			FootTurn[Side] *= FMath::Exp(-FootReturnRate * DeltaTime);

			// down, and either the body is not on its own legs or the foot is all but still against the floor: it is put down here
			if (!bFree && bLow && (!bOwnLegs || Going < FMath::Max(FootStillSpeed, Inputs.Speed * FootStillShare)) && FootOff[Side].Size() < FootTakenAgain)
			{
				bFootHeld[Side] = true;
				FootHeldAt[Side] = Asked + FootOff[Side];

				// turned as it is seen now, which is the clip's way and whatever is still owed
				FootHeldTurned[Side] = FQuat(FVector::UpVector, FMath::DegreesToRadians(FootTurn[Side])) * Turned;
			}
		}

		// a fighting shuffle swings its feet forward along the floor without lifting them, which reads as skating: a foot
		// that is free, low and going over the ground is picked up a little while it goes
		const bool bSkating = !bFree && !bFootHeld[Side] && !bFootStepping[Side] && bLow && bOwnLegs && Going > FootStillSpeed * 2.0f;

		FootLift[Side] = FMath::FInterpTo(FootLift[Side], bSkating ? FootShuffleLift : 0.0f, DeltaTime, bSkating ? FootLiftRate : FootLiftRate * 0.7f);

		if (!bFootStepping[Side])
		{
			FootOff[Side].Z = FootLift[Side];
		}

		if (FootOff[Side].Size() < 0.2f && FMath::Abs(FootTurn[Side]) < 0.3f)
		{
			continue;
		}

		// bend the leg to it, and keep the foot turned as the clip has it, less what the body has turned under it
		if (FootOff[Side].Size() >= 0.2f)
		{
			ReachTo(Output.Pose, Named.Leg[Side], InWorld.InverseTransformPosition(Ankle + FootOff[Side]));
		}

		const FQuat Calf = InBody(Output.Pose, Named.Leg[Side][1]).GetRotation();
		const FQuat Kept = InWorld.GetRotation().Inverse() * FQuat(FVector::UpVector, FMath::DegreesToRadians(FootTurn[Side])) * Turned;

		Output.Pose[Named.Leg[Side][2]].SetRotation((Calf.Inverse() * Kept).GetNormalized());
	}

	bFootKnown = true;
}

const FGutTurnClip* FGutAnimInstanceProxy::ChooseTurn(float Offset) const
{
	using namespace GutAnimConstants;

	// a body turned to the left of where its fighter faces has to step round to the right
	const bool bRight = Offset < 0.0f;
	const bool bFar = FMath::Abs(Offset) > TurnFarAngle;

	const FGutTurnClip& Near = bRight ? Inputs.Locomotion.TurnRight : Inputs.Locomotion.TurnLeft;
	const FGutTurnClip& Far = bRight ? Inputs.Locomotion.TurnRightFar : Inputs.Locomotion.TurnLeftFar;

	const FGutTurnClip& Wanted = (bFar && Far.Animation) ? Far : (Near.Animation ? Near : Far);

	return (Wanted.Animation && Wanted.Animation->GetPlayLength() > 0.0f) ? &Wanted : nullptr;
}

void FGutAnimInstanceProxy::TurnBody(FPoseContext& Output) const
{
	using namespace GutAnimConstants;

	if (FMath::Abs(BodyYaw) < 0.05f)
	{
		return;
	}

	FCompactPose& Pose = Output.Pose;

	// the whole body, feet and all, stays turned to where it was
	const FQuat Stay(FVector::UpVector, FMath::DegreesToRadians(BodyYaw));

	FTransform& Root = Pose[Named.Root];

	Root.SetRotation(Stay * Root.GetRotation());
	Root.SetTranslation(Stay.RotateVector(Root.GetTranslation()));

	if (Named.Spine[0].GetInt() < 0)
	{
		return;
	}

	// and the chest comes back the other way, a share at each bone up the spine, so that the shoulders, the head and whatever
	// is in the hands face where the fighter faces
	const float Twist = -FMath::Clamp(BodyYaw, -MostTwist, MostTwist);

	FQuat Parent = InBody(Pose, Pose.GetParentBoneIndex(Named.Spine[0])).GetRotation();

	for (int32 i = 0; i < 5; ++i)
	{
		if (Named.Spine[i].GetInt() < 0)
		{
			continue;
		}

		const FQuat Share(FVector::UpVector, FMath::DegreesToRadians(Twist * TwistShare[i]));

		FTransform& Bone = Pose[Named.Spine[i]];

		const FQuat Turned = Share * (Parent * Bone.GetRotation());

		Bone.SetRotation((Parent.Inverse() * Turned).GetNormalized());

		Parent = Turned;
	}
}

void FGutAnimInstanceProxy::HoldWeapon(FPoseContext& Output)
{
	using namespace GutAnimConstants;

	if (!Inputs.bWeaponLine || Named.Hand.GetInt() < 0 || Named.Forearm.GetInt() < 0 || Named.Neck.GetInt() < 0 || Named.Head.GetInt() < 0)
	{
		HoldTurn = FQuat::Identity;
		return;
	}

	FCompactPose& Pose = Output.Pose;

	const FTransform Hand = InBody(Pose, Named.Hand);
	const FTransform Forearm = InBody(Pose, Named.Forearm);
	const FVector Wrist = Hand.GetLocation();

	// the weapon is in the hand of the body that is seen, and that hand is not turned quite as the fighting skeleton's is
	const FQuat Grip = Hand.GetRotation() * Inputs.HandTurn;

	const FVector Base = Wrist + Grip.RotateVector(Inputs.WeaponBase);
	const FVector Tip = Wrist + Grip.RotateVector(Inputs.WeaponTip);
	const FVector Along = (Tip - Base).GetSafeNormal();

	FQuat Pointed = FQuat::Identity;

	// where the stance holds it. The clips know nothing of the weapon, so while the fighter only stands or walks the wrist is
	// turned to point it. A blow is left to the clip.
	const FVector Aim = FMath::Lerp(Inputs.WeaponAim, Inputs.BlockWeaponAim.IsNearlyZero() ? Inputs.WeaponAim : Inputs.BlockWeaponAim, BlockAmount);
	// A clip that starts or ends in the stance holds the weapon as the stance does for as long as the hand is where the stance
	// has it: the wrist is let go only as the hand leaves for the blow, and taken up again as it comes back.
	const float InStance = 1.0f - FMath::SmoothStep(StanceReachNear, StanceReachFar, WristFromStance);
	const float AimAmount = Aim.IsNearlyZero() ? 0.0f : FMath::Clamp(FMath::Max(1.0f - SlotWeight, InStance), 0.0f, 1.0f);

	if (AimAmount > 0.0f)
	{
		// as near to that as a wrist can turn: a bar in a fist lies across the forearm, never along it
		const FVector Arm = (Wrist - Forearm.GetLocation()).GetSafeNormal();
		const FVector Asked = Aim.GetSafeNormal();
		const float AlongArm = FMath::Clamp(FVector::DotProduct(Asked, Arm), -WristReach, WristReach);
		const FVector Across = (Asked - (Arm * FVector::DotProduct(Asked, Arm))).GetSafeNormal();

		if (!Across.IsNearlyZero())
		{
			const FVector Reachable = ((Across * FMath::Sqrt(1.0f - FMath::Square(AlongArm))) + (Arm * AlongArm)).GetSafeNormal();

			Pointed = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Along, Reachable), AimAmount);
		}
	}

	// the wrist comes round to that, it does not snap to it
	HoldTurn = FQuat::Slerp(HoldTurn, Pointed, 1.0f - FMath::Exp(-HoldRate * FMath::Max(Elapsed, 0.001f))).GetNormalized();

	const FQuat Parent = InBody(Pose, Pose.GetParentBoneIndex(Named.Hand)).GetRotation();

	Pose[Named.Hand].SetRotation((Parent.Inverse() * (HoldTurn * Hand.GetRotation())).GetNormalized());
}

void FGutAnimInstanceProxy::ReachTo(FCompactPose& Pose, const FCompactPoseBoneIndex Arm[3], const FVector& Target)
{
	const FTransform Upper = InBody(Pose, Arm[0]);
	const FTransform Lower = InBody(Pose, Arm[1]);

	const FVector Shoulder = Upper.GetLocation();
	const FVector Elbow = Lower.GetLocation();
	const FVector Wrist = InBody(Pose, Arm[2]).GetLocation();

	const float UpperLength = FVector::Dist(Shoulder, Elbow);
	const float LowerLength = FVector::Dist(Elbow, Wrist);

	if (UpperLength < UE_KINDA_SMALL_NUMBER || LowerLength < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	// no further than the arm is long, and no nearer than it can fold
	const FVector Toward = (Target - Shoulder).GetSafeNormal();
	const float Distance = FMath::Clamp(FVector::Dist(Target, Shoulder), FMath::Abs(UpperLength - LowerLength) + 0.5f, UpperLength + LowerLength - 0.5f);

	// the elbow stays out on the side it is out on
	FVector Out = (Elbow - Shoulder) - (Toward * FVector::DotProduct(Elbow - Shoulder, Toward));

	if (!Out.Normalize())
	{
		return;
	}

	const float Along = (FMath::Square(UpperLength) - FMath::Square(LowerLength) + FMath::Square(Distance)) / (2.0f * Distance);
	const float Off = FMath::Sqrt(FMath::Max(FMath::Square(UpperLength) - FMath::Square(Along), 0.0f));

	const FVector NewElbow = Shoulder + (Toward * Along) + (Out * Off);
	const FVector NewWrist = Shoulder + (Toward * Distance);

	const FQuat UpperTurn = FQuat::FindBetweenNormals((Elbow - Shoulder).GetSafeNormal(), (NewElbow - Shoulder).GetSafeNormal());
	const FQuat LowerTurn = FQuat::FindBetweenNormals(UpperTurn.RotateVector((Wrist - Elbow).GetSafeNormal()), (NewWrist - NewElbow).GetSafeNormal());

	const FQuat UpperTurned = UpperTurn * Upper.GetRotation();
	const FQuat LowerTurned = LowerTurn * UpperTurn * Lower.GetRotation();

	const FQuat Parent = InBody(Pose, Pose.GetParentBoneIndex(Arm[0])).GetRotation();

	// the hand goes with the forearm, turned on it as it was
	Pose[Arm[0]].SetRotation((Parent.Inverse() * UpperTurned).GetNormalized());
	Pose[Arm[1]].SetRotation((UpperTurned.Inverse() * LowerTurned).GetNormalized());
}

void FGutAnimInstanceProxy::BindArms(FPoseContext& Output)
{
	using namespace GutAnimConstants;

	if (!Inputs.Bound || Arms.Num() != Output.Pose.GetNumBones())
	{
		return;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Part = 0; Part < 3; ++Part)
		{
			if (Named.Arm[Side][Part].GetInt() < 0)
			{
				return;
			}
		}
	}

	// the arms as the clip holds them, behind the back, on whatever the legs and the back are doing
	FPoseContext Held(Output);
	FPoseContext Mixed(Output);

	Sample(Held, Inputs.Bound, FMath::Fmod(IdleTime, FMath::Max(Inputs.Bound->GetPlayLength(), 0.01f)));

	MixByBone(Output, Held, Arms, Mixed);

	Output = Mixed;

	// and the wrists brought together, one across the other: somebody tied cannot hold their hands a foot apart
	FCompactPose& Pose = Output.Pose;

	const FVector Left = InBody(Pose, Named.Arm[0][2]).GetLocation();
	const FVector Right = InBody(Pose, Named.Arm[1][2]).GetLocation();
	const FVector Middle = (Left + Right) * 0.5f;

	// which way is across the body is read off the shoulders, which never change sides: the clip's own wrists do, as its hands shift
	const FVector Across = (InBody(Pose, Named.Arm[1][0]).GetLocation() - InBody(Pose, Named.Arm[0][0]).GetLocation()).GetSafeNormal2D();

	ReachTo(Pose, Named.Arm[0], Middle + (Across * Inputs.BoundPast) + (FVector::UpVector * BoundStack));
	ReachTo(Pose, Named.Arm[1], Middle - (Across * Inputs.BoundPast) - (FVector::UpVector * BoundStack));
}

void FGutAnimInstanceProxy::ClearWeapon(FPoseContext& Output) const
{
	using namespace GutAnimConstants;

	if (!Inputs.bWeaponLine || Named.Hand.GetInt() < 0 || Named.Neck.GetInt() < 0 || Named.Head.GetInt() < 0)
	{
		return;
	}

	FCompactPose& Pose = Output.Pose;

	const FTransform Hand = InBody(Pose, Named.Hand);
	const FVector Wrist = Hand.GetLocation();

	// the weapon is in the hand of the body that is seen, and that hand is not turned quite as the fighting skeleton's is
	const FQuat Grip = Hand.GetRotation() * Inputs.HandTurn;

	const FVector Base = Wrist + Grip.RotateVector(Inputs.WeaponBase);
	const FVector Tip = Wrist + Grip.RotateVector(Inputs.WeaponTip);

	FQuat Wanted = FQuat::Identity;

	// never through the fighter's own neck or skull, whatever the clip does with the wrist. The neck and skull are those of the
	// body that is seen, which is not built quite as the skeleton that fights is: where they are is measured from the hand on both.
	const FVector NeckLow = InBody(Pose, Named.Neck).GetLocation() + Inputs.NeckShift;
	const FVector NeckHigh = InBody(Pose, Named.Head).GetLocation() + Inputs.HeadShift;
	const FVector Up = (NeckHigh - NeckLow).GetSafeNormal();

	const TTuple<FVector, FVector, float> Parts[] =
	{
		MakeTuple(NeckLow - (Up * 4.0f), NeckHigh, NeckRadius),
		MakeTuple(NeckHigh + (Up * 4.0f), NeckHigh + (Up * SkullHeight), SkullRadius),
	};

	// This is not eased: how far the weapon is pushed out grows from nothing as it comes near, so there is nothing to snap.
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		for (const TTuple<FVector, FVector, float>& Part : Parts)
		{
			const FVector Low = Wrist + Wanted.RotateVector(Base - Wrist);
			const FVector High = Wrist + Wanted.RotateVector(Tip - Wrist);

			FVector OnWeapon;
			FVector OnPart;

			NearestPoints(Low, High, Part.Get<0>(), Part.Get<1>(), OnWeapon, OnPart);

			const float Room = Part.Get<2>() + Inputs.WeaponRadius;
			const float Gap = FVector::Dist(OnWeapon, OnPart);
			const FVector Lever = OnWeapon - Wrist;

			// a weapon that is in the flesh where the fist itself is cannot be turned out of it
			if (Gap >= Room || Lever.SizeSquared() < FMath::Square(4.0f))
			{
				continue;
			}

			FVector Away = (OnWeapon - OnPart).GetSafeNormal();

			if (Away.IsNearlyZero())
			{
				Away = FVector::CrossProduct(Up, High - Low).GetSafeNormal();
			}

			const FQuat Out = FQuat::FindBetweenNormals(Lever.GetSafeNormal(), (Lever + (Away * (Room - Gap))).GetSafeNormal());

			Wanted = Out * Wanted;
		}
	}

	// no further than a wrist goes
	FVector Axis;
	float Angle;

	Wanted.ToAxisAndAngle(Axis, Angle);

	if (Angle > FMath::DegreesToRadians(MostHoldTurn))
	{
		Wanted = FQuat(Axis, FMath::DegreesToRadians(MostHoldTurn));
	}

	const FQuat Parent = InBody(Pose, Pose.GetParentBoneIndex(Named.Hand)).GetRotation();

	Pose[Named.Hand].SetRotation((Parent.Inverse() * (Wanted * Hand.GetRotation())).GetNormalized());
}

void FGutAnimInstanceProxy::CarryOver(FPoseContext& Output)
{
	FCompactPose& Pose = Output.Pose;

	const int32 Count = Pose.GetNumBones();
	const uint16 Serial = Pose.GetBoneContainer().GetSerialNumber();

	// a different set of bones: there is nothing to carry over between the two
	if (LastPose.Num() != Count || PoseSerial != Serial)
	{
		LastPose.SetNum(Count);
		PoseBefore.SetNum(Count);
		PoseSerial = Serial;
		PosesKept = 0;
		bCarrying = false;
	}

	// worked out again with no time gone by: the same answer
	const float Step = Elapsed;

	// what was just begun has a speed of its own, known now that it has moved once. The body was to carry on at the speed it had,
	// not at that speed and the new one together: the new one comes off what is carried.
	if (bSpeedOwed && bCarrying && Step > 0.0f && PoseAsked.Num() == Count && CarriedPlaceSpeed.Num() == Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const FTransform& Now = Pose[FCompactPoseBoneIndex(i)];
			const FTransform& Before = PoseAsked[i];

			FQuat Going = Now.GetRotation() * Before.GetRotation().Inverse();
			Going.EnforceShortestArcWith(FQuat::Identity);

			CarriedPlaceSpeed[i] = (CarriedPlaceSpeed[i] - ((Now.GetTranslation() - Before.GetTranslation()) / Step)).GetClampedToMaxSize(400.0f);
			CarriedTurnSpeed[i] = (CarriedTurnSpeed[i] - (Going.ToRotationVector() / Step)).GetClampedToMaxSize(14.0f);
		}
	}

	bSpeedOwed = false;

	// the pose as it was asked for, before anything is carried over onto it
	if (Step > 0.0f)
	{
		PoseAsked.SetNum(Count);

		for (int32 i = 0; i < Count; ++i)
		{
			PoseAsked[i] = Pose[FCompactPoseBoneIndex(i)];
		}
	}

	if (SmoothSerial != Inputs.SmoothSerial)
	{
		SmoothSerial = Inputs.SmoothSerial;

		if (PosesKept >= 1 && Step > 0.0f)
		{
			// how far each bone was from where the new pose has it, and how fast it was going. Both die away together,
			// so the body carries on as it was for a moment and settles into what it is doing now.
			CarriedPlace.SetNum(Count);
			CarriedPlaceSpeed.SetNum(Count);
			CarriedTurn.SetNum(Count);
			CarriedTurnSpeed.SetNum(Count);

			const bool bSpeedKnown = PosesKept >= 2 && LastStep > UE_SMALL_NUMBER;

			for (int32 i = 0; i < Count; ++i)
			{
				const FTransform& Now = Pose[FCompactPoseBoneIndex(i)];
				const FTransform& Last = LastPose[i];

				CarriedPlace[i] = Last.GetTranslation() - Now.GetTranslation();

				FQuat Off = Last.GetRotation() * Now.GetRotation().Inverse();
				Off.EnforceShortestArcWith(FQuat::Identity);

				CarriedTurn[i] = Off.ToRotationVector();

				CarriedPlaceSpeed[i] = FVector::ZeroVector;
				CarriedTurnSpeed[i] = FVector::ZeroVector;

				if (bSpeedKnown)
				{
					const FTransform& Before = PoseBefore[i];

					FQuat Going = Last.GetRotation() * Before.GetRotation().Inverse();
					Going.EnforceShortestArcWith(FQuat::Identity);

					CarriedPlaceSpeed[i] = ((Last.GetTranslation() - Before.GetTranslation()) / LastStep).GetClampedToMaxSize(250.0f);
					CarriedTurnSpeed[i] = (Going.ToRotationVector() / LastStep).GetClampedToMaxSize(9.0f);
				}
			}

			CarryTime = 0.0f;
			CarryRate = 5.0f / Inputs.SmoothTime;
			bCarrying = true;
			bSpeedOwed = true;
		}
	}

	if (bCarrying && CarriedPlace.Num() == Count)
	{
		CarryTime += Step;

		const float Decay = FMath::Exp(-CarryRate * CarryTime);

		for (int32 i = 0; i < Count; ++i)
		{
			FTransform& Bone = Pose[FCompactPoseBoneIndex(i)];

			const FVector Place = (CarriedPlace[i] + ((CarriedPlaceSpeed[i] + (CarriedPlace[i] * CarryRate)) * CarryTime)) * Decay;
			const FVector Twist = (CarriedTurn[i] + ((CarriedTurnSpeed[i] + (CarriedTurn[i] * CarryRate)) * CarryTime)) * Decay;

			Bone.AddToTranslation(Place);
			Bone.SetRotation((FQuat::MakeFromRotationVector(Twist) * Bone.GetRotation()).GetNormalized());
		}

		if (Decay < 0.002f)
		{
			bCarrying = false;
		}
	}

	if (Step > 0.0f)
	{
		Swap(LastPose, PoseBefore);

		for (int32 i = 0; i < Count; ++i)
		{
			LastPose[i] = Pose[FCompactPoseBoneIndex(i)];
		}

		LastStep = Step;
		PosesKept = FMath::Min(PosesKept + 1, 2);
	}
}

bool FGutAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// standing
	FPoseContext Standing(Output);

	Sample(Standing, CurrentIdle, IdleTime);

	if (IdleChange < 1.0f && PreviousIdle)
	{
		FPoseContext Before(Output);
		FPoseContext Mixed(Output);

		Sample(Before, PreviousIdle, FMath::Fmod(PreviousIdleTime, FMath::Max(PreviousIdle->GetPlayLength(), 0.01f)));

		Mix(Before, Standing, 1.0f - IdleChange, Mixed);

		Standing = Mixed;
	}

	const TArray<float>& Upper = GetUpperBody(Output.Pose.GetBoneContainer());

	FPoseContext Base(Output);

	Base = Standing;

	// walking: up to four clips, the two ways it lies between at a walk and at a run, all read at the same point in their step
	if (MoveAmount > 0.01f)
	{
		FPoseContext Walking(Output);

		float Total = 0.0f;

		for (int32 i = 0; i < 4; ++i)
		{
			for (int32 Pace = 0; Pace < 3; ++Pace)
			{
				// a walk, a run, and flat out: the share of each falls off either side of its own pace
				const FGutMoveClip& Move = Pace == 0 ? Inputs.Locomotion.GetWalk(i) : (Pace == 1 ? Inputs.Locomotion.GetRun(i) : Inputs.Locomotion.Sprint);
				const UAnimSequenceBase* Clip = Move.Animation;
				const float Amount = WayAmount[i] * FMath::Clamp(1.0f - FMath::Abs(RunAmount - Pace), 0.0f, 1.0f);

				if (!Clip || Amount < 0.005f)
				{
					continue;
				}

				FPoseContext One(Output);

				// the phase is of one cycle: a clip that holds two is half way through when the cycle comes round
				const int32 Cycles = FMath::Max(Move.Cycles, 1);

				Sample(One, Clip, FMath::Fmod(StepPhase + Move.Phase + (FMath::FloorToInt(StepCount) % Cycles), static_cast<float>(Cycles)) * Clip->GetPlayLength() / Cycles);

				if (Total <= 0.0f)
				{
					Walking = One;
				}
				else
				{
					FPoseContext Mixed(Output);

					Mix(Walking, One, Total / (Total + Amount), Mixed);

					Walking = Mixed;
				}

				Total += Amount;
			}
		}

		if (Total > 0.0f)
		{
			// the legs walk. The arms go on holding the weapon the way the stance holds it, if the stance asks for that.
			if (Inputs.bKeepUpperBodyWhileMoving)
			{
				FPoseContext Layered(Output);

				MixByBone(Walking, Standing, Upper, Layered);

				Walking = Layered;
			}

			FPoseContext Mixed(Output);

			Mix(Standing, Walking, 1.0f - MoveAmount, Mixed);

			Base = Mixed;
		}
	}

	// stepping round on the spot: the legs of the step, under the arms of the stance if the stance keeps its arms
	if (TurnAmount > 0.01f && Turn.Animation)
	{
		FPoseContext Stepping(Output);

		Sample(Stepping, Turn.Animation, FMath::Min(TurnTime, Turn.Animation->GetPlayLength()));

		if (Inputs.bKeepUpperBodyWhileMoving)
		{
			FPoseContext Layered(Output);

			MixByBone(Stepping, Standing, Upper, Layered);

			Stepping = Layered;
		}

		FPoseContext Mixed(Output);

		Mix(Base, Stepping, 1.0f - (TurnAmount * (1.0f - MoveAmount)), Mixed);

		Base = Mixed;
	}

	// the guard: the whole body when standing, the upper body only when the legs are busy walking
	if (BlockAmount > 0.01f && Inputs.Block)
	{
		FPoseContext Guard(Output);
		FPoseContext Mixed(Output);

		Sample(Guard, Inputs.Block, BlockTime);

		Scratch.SetNumUninitialized(Upper.Num(), EAllowShrinking::No);

		for (int32 i = 0; i < Upper.Num(); ++i)
		{
			Scratch[i] = BlockAmount * FMath::Lerp(1.0f, Upper[i], MoveAmount);
		}

		MixByBone(Base, Guard, Scratch, Mixed);

		Base = Mixed;
	}

	// and over it all, whatever is playing on the slot
	if (SlotWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		Output = Base;
	}
	else
	{
		const FAnimationPoseData BaseData(Base);
		FAnimationPoseData OutputData(Output);

		SlotEvaluatePose(SlotName, BaseData, SlotSourceWeight, OutputData, SlotWeight, SlotTotalWeight);

		// a hand to the back of the neck is done with the arm and the head. The legs go on standing as they stood: the
		// clip of it was made on other feet, and laid over the whole body it shuffled them apart and back every time.
		if (Inputs.bSlotUpperOnly)
		{
			FPoseContext Whole(Output);

			Whole = Output;

			if (Inputs.bSlotKeepsHead && Arms.Num() == Upper.Num())
			{
				// with the arms, and only a third of what the performer's trunk did: one of the clips bows from the waist
				Scratch.SetNumUninitialized(Upper.Num(), EAllowShrinking::No);

				for (int32 i = 0; i < Upper.Num(); ++i)
				{
					Scratch[i] = FMath::Lerp(Arms[i], Upper[i], 0.3f);
				}

				MixByBone(Base, Whole, Scratch, Output);
			}
			else
			{
				MixByBone(Base, Whole, Upper, Output);
			}

			// Talking with the hands is done with the hands: the head stays the head of somebody looking at whoever they
			// are talking to. The clips carry their performer's head, and one of them took the Gate Warden's down to his
			// chest a second into his line.
			if (Inputs.bSlotKeepsHead && Named.Head.GetInt() >= 0 && Named.Neck.GetInt() >= 0)
			{
				for (FCompactPoseBoneIndex Bone = Named.Head; Bone.GetInt() >= 0; Bone = Output.Pose.GetParentBoneIndex(Bone))
				{
					Output.Pose[Bone].SetRotation(Base.Pose[Bone].GetRotation());

					if (Bone == Named.Neck)
					{
						break;
					}
				}
			}
		}
	}

	// a clip the performer travels in: the body is moved along it by the fighter, so the travel is not in the pose as well
	if (Inputs.bCarried)
	{
		Output.Pose[Named.Root] = Output.Pose.GetBoneContainer().GetRefPoseTransform(Named.Root);
	}

	BindArms(Output);

	// hands that are shut for the body that is seen are handed over open, as they rest: see bFingersShutForIt
	if (Inputs.bFingersShutForIt)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();

		for (const FCompactPoseBoneIndex Finger : Fingers)
		{
			if (Output.Pose.IsValidIndex(Finger))
			{
				Output.Pose[Finger] = Bones.GetRefPoseTransform(Finger);
			}
		}
	}

	if (Inputs.bFighter && Named.Hand.GetInt() >= 0)
	{
		// how far the hand is from where the stance has it, before the body is turned
		WristFromStance = FVector::Dist(InBody(Standing.Pose, Named.Hand).GetLocation(), InBody(Output.Pose, Named.Hand).GetLocation());
	}

	if (Inputs.bFighter)
	{
		// the weapon is cleared of the neck last of all, so that nothing carried over from the pose before can put it back in
		TurnBody(Output);
		HoldWeapon(Output);
		CarryOver(Output);
		ClearWeapon(Output);
	}

	// arms at ease are brought in to the sides of the body that wears the clip
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FCompactPoseBoneIndex* Arm = Named.Arm[Side];

		if (FMath::Abs(Inputs.ArmsIn[Side]) < 0.2f || Arm[0].GetInt() < 0 || Arm[2].GetInt() < 0 || Named.Spine[0].GetInt() < 0)
		{
			continue;
		}

		const FTransform Shoulder = InBody(Output.Pose, Arm[0]);
		const FVector Wrist = InBody(Output.Pose, Arm[2]).GetLocation();
		const FVector Middle = InBody(Output.Pose, Named.Spine[0]).GetLocation();

		const FVector Hangs = (Wrist - Shoulder.GetLocation()).GetSafeNormal();
		const FVector Inward = FVector(Middle.X - Wrist.X, Middle.Y - Wrist.Y, 0.0f).GetSafeNormal();

		if (Hangs.IsNearlyZero() || Inward.IsNearlyZero())
		{
			continue;
		}

		// about the shoulder, the way that takes the wrist toward the middle of the body
		const FQuat In(FQuat::FindBetweenNormals(Hangs, Inward).GetRotationAxis(), FMath::DegreesToRadians(Inputs.ArmsIn[Side] * (1.0f - FMath::Clamp(SlotWeight, 0.0f, 1.0f))));

		const FQuat Parent = InBody(Output.Pose, Output.Pose.GetParentBoneIndex(Arm[0])).GetRotation();

		Output.Pose[Arm[0]].SetRotation((Parent.Inverse() * In * Shoulder.GetRotation()).GetNormalized());
	}

	LookToward(Output, Elapsed);

	// a blow on its way in is taken the rest of the way onto the body it is thrown at
	if (Inputs.BlowLimb >= 0 && Inputs.BlowLimb < 4 && !Inputs.BlowReach.IsNearlyZero(0.1f))
	{
		const FCompactPoseBoneIndex* Limb = Inputs.BlowLimb < 2 ? Named.Arm[Inputs.BlowLimb] : Named.Leg[Inputs.BlowLimb - 2];

		if (Limb[0].GetInt() >= 0 && Limb[1].GetInt() >= 0 && Limb[2].GetInt() >= 0)
		{
			const FTransform End = InBody(Output.Pose, Limb[2]);

			ReachTo(Output.Pose, Limb, End.GetLocation() + GetComponentTransform().InverseTransformVector(Inputs.BlowReach));

			// the fist or the foot stays turned as the clip has it, and so does whatever is in it
			const FQuat Lower = InBody(Output.Pose, Limb[1]).GetRotation();

			Output.Pose[Limb[2]].SetRotation((Lower.Inverse() * End.GetRotation()).GetNormalized());
		}
	}

	// last of all, so that nothing done to the body above can move a foot that is held
	HoldFeet(Output, Elapsed);

	Elapsed = 0.0f;

	return true;
}
