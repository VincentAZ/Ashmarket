// Ashmarket prototype: the Gut.

#include "GutAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"
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
	Inputs.Block = Stance.Block;
	Inputs.Locomotion = Fighter->GetLocomotionAnimations();
	Inputs.bKeepUpperBodyWhileMoving = Stance.bKeepUpperBodyWhileMoving;
	Inputs.bBlocking = Fighter->IsBlocking();

	// only what its own legs are doing: a shove from a blow or the carry of a lunge is not walking
	const UCharacterMovementComponent* Movement = Fighter->GetCharacterMovement();
	const bool bWalking = Movement && Movement->IsMovingOnGround() && !Movement->HasRootMotionSources();

	const FVector Velocity = bWalking ? Fighter->GetVelocity() : FVector::ZeroVector;
	const FVector Local = Fighter->GetActorRotation().UnrotateVector(Velocity);

	Inputs.Speed = Local.Size2D();
	Inputs.Direction = Inputs.Speed > 1.0f ? FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)) : 0.0f;

	Inputs.bFighter = true;
	Inputs.ActorYaw = Fighter->GetActorRotation().Yaw;

	// a fighter that is suddenly somewhere else was put there, and did not get there by itself
	const FVector Place = Fighter->GetActorLocation();

	if (FVector::DistSquared(Place, LastPlace) > FMath::Square(120.0f))
	{
		Placed();
	}

	LastPlace = Place;

	const EGutFighterState State = Fighter->GetFighterState();

	Inputs.bHolds = State == EGutFighterState::Idle || State == EGutFighterState::Blocking;

	// the weapon in the fist, if it is the kind that is gripped, and where the stance points it
	Inputs.bWeaponLine = Fighter->GetWeaponLine(Inputs.WeaponBase, Inputs.WeaponTip, Inputs.WeaponRadius);

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

void UGutAnimInstance::SetIdle(UAnimSequenceBase* Idle, float StartTime, float PlayRate)
{
	Inputs.Idle = Idle;
	Inputs.IdleStart = StartTime;
	Inputs.IdleRate = PlayRate;
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
		CurrentIdle = Inputs.Idle;
		IdleChange = PreviousIdle ? 0.0f : 1.0f;
	}

	IdleChange = FMath::Min(1.0f, IdleChange + (DeltaTime / IdleChangeTime));

	if (CurrentIdle && CurrentIdle->GetPlayLength() > 0.0f)
	{
		IdleTime = FMath::Fmod(IdleTime + (DeltaTime * Inputs.IdleRate), CurrentIdle->GetPlayLength());
	}

	if (Inputs.Block && Inputs.Block->GetPlayLength() > 0.0f)
	{
		BlockTime = FMath::Fmod(BlockTime + DeltaTime, Inputs.Block->GetPlayLength());
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

	for (int32 i = 0; i < 4; ++i)
	{
		const FGutMoveClip& Walk = Inputs.Locomotion.GetWalk(i);
		const FGutMoveClip& Run = Inputs.Locomotion.GetRun(i).Animation ? Inputs.Locomotion.GetRun(i) : Walk;

		WalkSpeed += WayAmount[i] * Walk.Speed;
		RunSpeed += WayAmount[i] * Run.Speed;
		WalkTime += WayAmount[i] * (Walk.Animation ? Walk.Animation->GetPlayLength() : 1.0f);
		RunTime += WayAmount[i] * (Run.Animation ? Run.Animation->GetPlayLength() : 1.0f);
	}

	const float WantedRun = RunSpeed > WalkSpeed + 1.0f ? FMath::Clamp((Inputs.Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.0f, 1.0f) : 0.0f;

	RunAmount = FMath::FInterpTo(RunAmount, WantedRun, DeltaTime, 7.0f);

	const float ClipSpeed = FMath::Lerp(WalkSpeed, RunSpeed, RunAmount);
	const float StepTime = FMath::Lerp(WalkTime, RunTime, RunAmount);

	// a fighter that sets off from standing sets off on the foot the walking clips begin on, which is the one its stance has forward
	if (MoveAmount < 0.02f)
	{
		StepPhase = 0.0f;
	}

	if (ClipSpeed > 1.0f && StepTime > UE_SMALL_NUMBER)
	{
		const float Rate = FMath::Clamp(Inputs.Speed / ClipSpeed, SlowestStep, FastestStep);

		StepPhase = FMath::Frac(StepPhase + (DeltaTime * Rate / StepTime));
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

		Sample(Before, PreviousIdle, FMath::Fmod(IdleTime, FMath::Max(PreviousIdle->GetPlayLength(), 0.01f)));

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
			for (int32 Pace = 0; Pace < 2; ++Pace)
			{
				const UAnimSequenceBase* Clip = (Pace == 0 ? Inputs.Locomotion.GetWalk(i) : Inputs.Locomotion.GetRun(i)).Animation;
				const float Amount = WayAmount[i] * (Pace == 0 ? 1.0f - RunAmount : RunAmount);

				if (!Clip || Amount < 0.005f)
				{
					continue;
				}

				FPoseContext One(Output);

				Sample(One, Clip, StepPhase * Clip->GetPlayLength());

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

	Elapsed = 0.0f;

	return true;
}
