// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "GutClearOp.generated.h"

/**
 *  The body somebody has as it is dressed, and what they hold, for keeping their own arms, hands and weapon out of it.
 *  Measured by the fighter from the body standing at rest (AGutFighter::MeasureTrunk) and handed to the op each frame.
 */
struct FGutClearShape
{
	/** A level slice of the trunk, in the space of the spine bone it starts at */
	struct FSection
	{
		FName Bone;
		float Height = 0.0f;
		float Below = 0.0f;
		FVector Along = FVector::ZeroVector;
		FVector Across = FVector::ZeroVector;
		FVector Front = FVector::ZeroVector;
		FVector2D Middle = FVector2D::ZeroVector;
		FVector2D Half = FVector2D::ZeroVector;

		/** How far an arm may lie into it, in cm: flesh and a shirt give under an arm that rests on them, a vest or a coat does not */
		float Give = 0.0f;
	};

	TArray<FSection, TInlineAllocator<6>> Trunk;

	/** How thick each upper arm, forearm and thigh is with what is worn on it, as a radius: left, right */
	float UpperArm[2] = { 4.5f, 4.5f };
	float Forearm[2] = { 3.4f, 3.4f };
	float Thigh[2] = { 8.5f, 8.5f };

	/** What is held in each hand: places along it in the hand bone's own space, each with how thick it is there (W) */
	TArray<FVector4, TInlineAllocator<8>> Held[2];

	/** Wrists that are tied: the two hands are moved as one */
	bool bBound = false;

	/**
	 *  How much an arm that hangs is brought in to the side of the body that wears the clip, 0 to 1: all of it for somebody
	 *  standing at ease with nothing in their hands. The performer of a clip is as wide as they are, and on a narrower
	 *  body their arms hang out from the hips like a doll's.
	 */
	float HangIn = 0.0f;

	/** How far from what the body wears a hanging wrist is left, in cm */
	float HangGap = 2.5f;

	bool bOn = false;
};

/** That body where it is now: each slice of the trunk, the thighs and the head set down in one space, to ask of it how far in a place is */
struct FGutClearBody
{
	const FGutClearShape* Shape = nullptr;

	/** Where the spine bone of each slice is, slice for slice */
	TArray<FTransform, TInlineAllocator<6>> Spine;

	FVector Hip[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector Knee[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bLegs = false;

	/** The middle of the head, for what is held: a blade is kept out of its holder's own head */
	FVector Head = FVector::ZeroVector;
	bool bHead = false;

	double Clearance = 0.5;
	double Squareness = 2.6;

	/**
	 *  How far a place of a given thickness has to move to be out of the trunk and the thighs (and, for something held, the
	 *  head); zero if it is out. Anchor is a place further up the same limb: where the limb comes into the body is the way out.
	 */
	FVector GetPush(const FVector& Place, double Thickness, const FVector& Anchor, bool bHeld, bool bOutTheBack = false) const;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Keep Clear Of Own Body Settings"))
struct FGutClearOpSettings : public FIKRetargetOpSettingsBase
{
	GENERATED_BODY()

	/** How much air is left between a limb and what the body wears, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Op Settings")
	double Clearance = 0.5;

	/** How square a slice of the trunk is taken to be: 2 is an ellipse, more is nearer a box with rounded corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Op Settings", meta = (ClampMin = "2.0", ClampMax = "6.0"))
	double Squareness = 2.6;

	/** The furthest a wrist is moved from where the clip has it, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Op Settings")
	double MostMoved = 26.0;

	/** The furthest something held is turned in the hand to keep its far end out of its holder, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Op Settings")
	double MostTurned = 55.0;

	virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) override;
};

/**
 *  Keeps somebody's own arms, hands and what they hold out of their own body as it is dressed.
 *
 *  A clip is performed by one body and worn by another: thicker, thinner, in a coat, in a stiff vest. Carried over joint by
 *  joint, a hand that rested on the performer's hip is inside this one's, wrists tied behind a narrow back are in it, and a
 *  club hanging by a thigh hangs through it. This runs last, on the body that is seen: each upper arm is turned out of the
 *  trunk's side, and each wrist is moved until the forearm, the hand and whatever is in it are outside the trunk and the
 *  thighs. It is worked out afresh every frame from where the clip has the arm, with nothing carried from the frame
 *  before: what stood here on the morning of 23 November added to an angle whenever an arm was in and never took it away,
 *  and the yard hand's elbows were at 29 degrees within a minute ("still some cuts", image 6).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Keep Clear Of Own Body"))
struct FGutClearOp : public FIKRetargetOpBase
{
	GENERATED_BODY()

	virtual bool Initialize(const FIKRetargetProcessor& InProcessor, const FRetargetSkeleton& InSourceSkeleton, const FTargetSkeleton& InTargetSkeleton, const FIKRetargetOpBase* InParentOp, FIKRigLogger& Log) override;

	virtual void Run(FIKRetargetProcessor& InProcessor, const double InDeltaTime, const TArray<FTransform>& InSourceGlobalPose, TArray<FTransform>& OutTargetGlobalPose) override;

	virtual void AnimGraphPreUpdateMainThread(USkeletalMeshComponent& SourceMeshComponent, USkeletalMeshComponent& TargetMeshComponent) override;

	virtual FIKRetargetOpSettingsBase* GetSettings() override { return &Settings; }

	virtual const UScriptStruct* GetSettingsType() const override { return FGutClearOpSettings::StaticStruct(); }

	virtual const UScriptStruct* GetType() const override { return FGutClearOp::StaticStruct(); }

	UPROPERTY()
	FGutClearOpSettings Settings;

private:

	/** How far an upper arm has to go out sideways to lie against the trunk's side and not in it, and which way that is */
	double GetUpperArmSunk(const FGutClearBody& Body, const FVector& Shoulder, const FVector& Elbow, int32 Side, FVector& OutWay) const;

	FGutClearShape Shape;

	/** Bones of the body that is seen: the spine each slice is fixed to, and upper arm, forearm, hand and knuckle; thigh and calf */
	TArray<int32, TInlineAllocator<6>> SectionBones;
	int32 Arm[2][4] = { { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE }, { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE } };
	int32 Leg[2][2] = { { INDEX_NONE, INDEX_NONE }, { INDEX_NONE, INDEX_NONE } };
	int32 Head[2] = { INDEX_NONE, INDEX_NONE };

	/** The hands of whoever performed the clip, in the skeleton the clip is played on */
	int32 PerformerHand[2] = { INDEX_NONE, INDEX_NONE };

	TArray<FName> BoneNames;
};
