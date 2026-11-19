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

	/** The turning clip for a body that many degrees behind where the fighter faces */
	const FGutTurnClip* ChooseTurn(float Offset) const;

	FGutAnimInputs Inputs;

	FName SlotName = FName("DefaultSlot");

	/** The idle before the weapon changed, and how far the change to the new one has got */
	TObjectPtr<UAnimSequenceBase> PreviousIdle;
	TObjectPtr<UAnimSequenceBase> CurrentIdle;
	float IdleChange = 1.0f;

	float IdleTime = 0.0f;
	float BlockTime = 0.0f;

	/** How far through its step the fighter is, 0 to 1. Every walking clip is read at this, so their feet agree. */
	float StepPhase = 0.0f;

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
	void SetIdle(UAnimSequenceBase* Idle, float StartTime = 0.0f, float PlayRate = 1.0f);

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
