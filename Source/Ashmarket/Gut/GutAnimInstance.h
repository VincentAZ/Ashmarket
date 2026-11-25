// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "GutTypes.h"
#include "GutAnimInstance.generated.h"

class UAnimSequenceBase;

/** What the fighter is doing this frame, as far as its standing and walking go. Read on the game thread, used on a worker. */
USTRUCT()
struct FGutAnimInputs
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Block;

	UPROPERTY(Transient)
	FGutLocomotionAnimations Locomotion;

	/** Speed over the ground */
	float Speed = 0.0f;

	/** Which way it is going, in degrees to the right of the way it faces */
	float Direction = 0.0f;

	bool bBlocking = false;
	bool bKeepUpperBodyWhileMoving = true;

	/** Where in the idle to start and how fast to play it, for bodies that should not all breathe as one */
	float IdleStart = 0.0f;
	float IdleRate = 1.0f;

	/** Where in the block clip the guard is held still, in seconds: less than nought if the clip is a loop */
	float BlockHold = -1.0f;

	/** True if a new idle is begun at its beginning: one of a family of standing clips, each of which follows on from the last */
	bool bIdleFromStart = false;

	/** True for a fighter: its body is kept from jerking, and it steps round on the spot when it is turned. Somebody watching needs neither. */
	bool bFighter = false;

	/** Which way the fighter faces, in the world */
	float ActorYaw = 0.0f;

	/** True while the fighter stands its ground, free or with its guard up: its feet may stay where they are when it is turned */
	bool bHolds = false;

	/** Goes up each time something starts or stops on the animation slot: the body is carried over from what it was doing, in Smooth Time */
	int32 SmoothSerial = 0;
	float SmoothTime = 0.2f;

	/** Goes up each time the fighter is put somewhere else: nothing is carried over */
	int32 PlaceSerial = 0;

	/**
	 *  True while the body that is seen has its hands shut for it (round a handle, into fists, half shut when tied): the
	 *  fingers are then left as the skeleton has them at rest, whatever the clip does with them. The bought clips carry
	 *  their performers' fingers, and the shutting was laid over those: a hand already closed on a sword was closed again,
	 *  and came out with its fingers through the handle.
	 */
	bool bFingersShutForIt = false;

	/** A weapon gripped in the right fist: its two ends in the space of the hand's bone, and how thick it is. Kept out of the neck and head. */
	bool bWeaponLine = false;
	FVector WeaponBase = FVector::ZeroVector;
	FVector WeaponTip = FVector::ZeroVector;
	float WeaponRadius = 2.5f;

	/** How much further from the right hand the neck and the head of the body that is seen are than the fighting skeleton's own, in the mesh's space */
	FVector NeckShift = FVector::ZeroVector;
	FVector HeadShift = FVector::ZeroVector;

	/** How the right hand of the body that is seen is turned against the fighting skeleton's own, in that hand's space */
	FQuat HandTurn = FQuat::Identity;

	/** Which way that weapon points while standing ready and with the guard up, in the space of the body's own mesh. Zero for none. */
	FVector WeaponAim = FVector::ZeroVector;
	FVector BlockWeaponAim = FVector::ZeroVector;

	/**
	 *  For somebody whose wrists are tied behind their back: a clip with the arms held there, which the arms are taken
	 *  from whatever the legs and the back are doing. None for anybody whose hands are their own.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Bound;

	/** How far past the middle of the two each wrist is taken toward the other's side, in centimetres: see AGutFighter::BoundPast */
	float BoundPast = 2.0f;

	/**
	 *  True while the body stands on its feet on the ground: a foot that the clip has put down is then held where it was
	 *  put, in the world, for as long as the clip keeps it down, whatever carries the body meanwhile. See HoldFeet.
	 */
	bool bHoldFeet = false;

	/** True if its walking clips plant their own feet, as a performed walk does: nothing is held while it walks */
	bool bFeetFree = false;

	/** True if it carries its head up whatever its clips do with it: see LookToward */
	bool bHeadUp = false;

	/** Somewhere in the world for the head to be turned toward, and how much of the turning to do: none while a blow is being thrown or taken */
	bool bLook = false;
	FVector LookAt = FVector::ZeroVector;

	/** How far the limb that strikes is taken off the clip's own path to land its blow, in the world, and which: left arm, right arm, left leg, right leg */
	FVector BlowReach = FVector::ZeroVector;
	int32 BlowLimb = INDEX_NONE;

	/** How far each arm is brought in toward the body from where the clip hangs it, in degrees, left then right */
	float ArmsIn[2] = { 0.0f, 0.0f };

	/** True while the body is being taken along a clip its performer travelled in: the clip's own travel is left out of the pose, since the body itself is moved */
	bool bCarried = false;

	/** True if what plays on the animation slot is for the upper body only: the legs go on with whatever they were doing */
	bool bSlotUpperOnly = false;

	/** True while what plays on the slot is a gesture of the hands: the neck and the head are left as they were under it */
	bool bSlotKeepsHead = false;

	/** The height of the floor under the body, in the world */
	float FloorHeight = 0.0f;

	/** How fast the body is being carried over the ground by anything at all, its own legs or not, and which way, in the world */
	float Carried = 0.0f;
	FVector CarriedWay = FVector::ZeroVector;
};

/**
 *  Does the fighter's standing, walking, turning and guarding, and lays whatever plays on the animation slot over it.
 *  It is written out here instead of drawn as an animation graph so that it can be built, read and changed with the rest of the code.
 *
 *  The movement itself all comes from clips. What is worked out here is how they are joined:
 *    - a clip that starts or stops is taken up from wherever the body was, and at the speed it was going, and eased into
 *    - a fighter turned on the spot keeps its feet where they are and twists at the waist, until it is turned far enough to step round
 *    - a weapon in the fist is pointed where the stance holds it, and kept out of the fighter's own neck
 */
USTRUCT()
struct FGutAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FGutAnimInstanceProxy() = default;

	explicit FGutAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** The bones this needs by name, found once for the bones in use */
	struct FBones
	{
		FCompactPoseBoneIndex Root = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine[5] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
		FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Forearm = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Hand = FCompactPoseBoneIndex(INDEX_NONE);

		/** Each leg from the hip down, left then right: thigh, calf, foot */
		FCompactPoseBoneIndex Leg[2][3] = { { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) }, { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) } };

		/** The ball of each foot, left then right */
		FCompactPoseBoneIndex Ball[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };

		/** Each arm from the shoulder down, left then right: upper arm, forearm, hand */
		FCompactPoseBoneIndex Arm[2][3] = { { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) }, { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) } };
	};

	/** Fills a pose from a clip at a time, or with the reference pose if there is no clip */
	static void Sample(FPoseContext& Pose, const UAnimSequenceBase* Clip, float Time);

	/** How much each bone belongs to the upper body, for the bones in use now. Also finds the bones needed by name. */
	const TArray<float>& GetUpperBody(const FBoneContainer& Bones);

	/** Where a bone is in the space of the whole body */
	static FTransform InBody(const FCompactPose& Pose, FCompactPoseBoneIndex Bone);

	/** Turns the body round by what it has not yet stepped round, and the chest back the other way so that it faces where the fighter faces */
	void TurnBody(FPoseContext& Output) const;

	/** Points the weapon in the right fist where the stance holds it */
	void HoldWeapon(FPoseContext& Output);

	/** Turns the wrist until the weapon in the right fist is clear of the fighter's own neck and head */
	void ClearWeapon(FPoseContext& Output) const;

	/** Carries the body over from what it was doing when something else started */
	void CarryOver(FPoseContext& Output);

	/** Holds the arms behind the back, whatever the rest of the body is doing, and brings the wrists together there */
	void BindArms(FPoseContext& Output);

	/** Bends one arm at the elbow, on the side it is bent already, until its wrist is at a place in the space of the whole body */
	static void ReachTo(FCompactPose& Pose, const FCompactPoseBoneIndex Arm[3], const FVector& Target);

	/**
	 *  Holds each foot that the clip has put down where it was put, in the world, until the clip lifts it.
	 *
	 *  The clips walk and strike on the spot and the game moves the body, at a speed that is the game's own: wherever the
	 *  two disagree (a sidestep slower than its clip, the carry of a lunge, a shove from a blow) the feet slid over the
	 *  floor. A held foot does not. The leg is bent to it; when it would have to stretch too far the foot takes a step:
	 *  up, over and down ahead of the body, one foot at a time. A foot is always either planted or in the air.
	 */
	void HoldFeet(FPoseContext& Output, float DeltaTime);

	/**
	 *  Turns the head, with a little of the neck and the top of the back, toward what the body is looking at: whoever it is
	 *  fighting, or whoever has walked up to it. Nobody alive stares at one spot in front of them while somebody comes past.
	 */
	void LookToward(FPoseContext& Output, float DeltaTime);

	/** The turning clip for a body that many degrees behind where the fighter faces */
	const FGutTurnClip* ChooseTurn(float Offset) const;

	FGutAnimInputs Inputs;

	FName SlotName = FName("DefaultSlot");

	/** Each foot, left then right: whether it is held, where in the world, how far it is from where the clip has it, and where the clip had it last */
	bool bFootHeld[2] = { false, false };
	FVector FootHeldAt[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector FootOff[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector FootWas[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bFootKnown = false;
	int32 FeetPlaceSerial = 0;

	/** A step being taken to catch the body up: where from, how far through it is, how long it takes and how high it goes */
	bool bFootStepping[2] = { false, false };
	FVector FootStepFrom[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float FootStepTime[2] = { 0.0f, 0.0f };
	float FootStepLength[2] = { 0.15f, 0.15f };
	float FootStepLift[2] = { 5.0f, 5.0f };

	/** Which way each held foot was turned, in the world, when it was put down; how far it is now turned back from the way the clip has it, in degrees; and how far it was when its step began */
	FQuat FootHeldTurned[2] = { FQuat::Identity, FQuat::Identity };
	float FootTurn[2] = { 0.0f, 0.0f };
	float FootStepTurn[2] = { 0.0f, 0.0f };

	/** How long each held foot's body has been at rest */
	float FootRest[2] = { 0.0f, 0.0f };

	/** How far each foot is picked up off the floor while the clip slides it forward */
	float FootLift[2] = { 0.0f, 0.0f };

	/** Which of the head bone's own axes the face is on */
	FVector HeadFaces = FVector::RightVector;

	/** Which way the head is turned from straight ahead by looking, in the space of the whole body, and how much of it is on */
	FVector LookWay = FVector(0.0f, 1.0f, 0.0f);
	float LookAmount = 0.0f;

	// how far down the clip has had the face over the last second or so, and how far it is being lifted
	float HeadHangs = 0.0f;
	float HeadRaised = 0.0f;
	bool bHeadKnown = false;

	/** How high the fighting skeleton's ankle is off the floor when it stands in its reference pose, and the ball of its foot */
	float AnkleRest = 8.0f;
	float BallRest = 2.0f;

	/** Whether each foot is standing on the ball of it and not on its heel, and how fast the point it stands on was last going */
	bool bFootOnBall[2] = { false, false };
	float FootWent[2] = { 0.0f, 0.0f };

	/** The idle before the weapon changed, and how far the change to the new one has got */
	TObjectPtr<UAnimSequenceBase> PreviousIdle;
	TObjectPtr<UAnimSequenceBase> CurrentIdle;
	float IdleChange = 1.0f;

	float IdleTime = 0.0f;
	float BlockTime = 0.0f;

	/** Where the idle before had got to, which it goes on from while it is left, and how long the leaving of it takes */
	float PreviousIdleTime = 0.0f;
	float IdleChangeLength = 0.3f;

	/** How far through its step the fighter is, 0 to 1. Every walking clip is read at this, so their feet agree. */
	float StepPhase = 0.0f;
	float StepCount = 0.0f;

	/** How much of the pose is walking, how much of the walking is running, and how much each way: forward, right, back, left */
	float MoveAmount = 0.0f;
	float MoveSpeed = 0.0f;
	float RunAmount = 0.0f;
	float WayAmount[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

	float BlockAmount = 0.0f;
	float BlockSpeed = 0.0f;

	float SlotWeight = 0.0f;
	float SlotSourceWeight = 1.0f;
	float SlotTotalWeight = 0.0f;

	FGraphTraversalCounter SlotCounter;

	TArray<float> UpperBody;

	/** How much each bone belongs to an arm: all of it from the collarbone down, none of it anywhere else */
	TArray<float> Arms;
	TArray<float> Scratch;

	/** Every bone below a wrist */
	TArray<FCompactPoseBoneIndex> Fingers;
	uint16 UpperBodySerial = 0;
	bool bUpperBodyBuilt = false;

	FBones Named;

	/** Time since the pose was last worked out */
	float Elapsed = 0.0f;

	/** How far the body is turned from where the fighter faces, in degrees to the right, and how fast that is changing */
	float BodyYaw = 0.0f;
	float BodyYawSpeed = 0.0f;
	float LastActorYaw = 0.0f;
	bool bYawKnown = false;
	int32 PlaceSerial = 0;

	/** The step round being taken: its clip, how far through it is, how much turn it is taking out, and how much of it shows */
	FGutTurnClip Turn;
	float TurnTime = 0.0f;
	float TurnOwed = 0.0f;
	float TurnDone = 0.0f;
	float TurnAmount = 0.0f;
	bool bTurning = false;

	/** How long the body has stood a little off where the fighter faces */
	float AskewTime = 0.0f;

	/** The turn given to the wrist to hold the weapon, in the space of the whole body */
	FQuat HoldTurn = FQuat::Identity;

	/** How far the right hand is from where the stance has it */
	float WristFromStance = 0.0f;

	/** The last two poses that went out, and the time between them */
	TArray<FTransform> LastPose;
	TArray<FTransform> PoseBefore;
	float LastStep = 0.0f;
	int32 PosesKept = 0;
	uint16 PoseSerial = 0;

	/** What is being carried over: how far each bone was from where the new pose has it and how fast it was going, and for how long */
	TArray<FVector> CarriedPlace;
	TArray<FVector> CarriedPlaceSpeed;
	TArray<FVector> CarriedTurn;
	TArray<FVector> CarriedTurnSpeed;
	float CarryTime = 0.0f;
	float CarryRate = 0.0f;
	bool bCarrying = false;

	/** The pose as it was asked for last time, before anything was carried over onto it, and whether the speed of what was just begun is still to come off what is carried */
	TArray<FTransform> PoseAsked;
	bool bSpeedOwed = false;
	int32 SmoothSerial = 0;
};

/**
 *  The animation of the skeleton that does the fighting. See FGutAnimInstanceProxy.
 */
UCLASS(Transient, NotBlueprintable)
class UGutAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 *  For a body that is not a fighter, such as somebody watching from the deck: it stands in the given clip,
	 *  and whatever is played on the animation slot is laid over that and blends in and out.
	 */
	void SetIdle(UAnimSequenceBase* Idle, float StartTime = 0.0f, float PlayRate = 1.0f, bool bFollowsOn = false);

	/** For a body that is not a fighter: somewhere in the world for its head to be turned toward, or nowhere. A fighter's is found by itself. */
	void SetLook(bool bLook, const FVector& At = FVector::ZeroVector);

	/** Something is about to start or stop on the animation slot: carry the body over from what it is doing now, in this long */
	void SmoothOver(float Seconds);

	/** The fighter has been put somewhere else: carry nothing over */
	void Placed();

	const FGutAnimInputs& GetInputs() const { return Inputs; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FGutAnimInputs Inputs;

	FVector LastPlace = FVector::ZeroVector;
};
