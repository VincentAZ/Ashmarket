// Copyright Epic Games, Inc. All Rights Reserved.

#include "GutClearOp.h"
#include "GutFighter.h"
#include "GutCrowd.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutClearOp)

namespace GutClear
{
	double Power(double Value, double N)
	{
		return FMath::Pow(FMath::Abs(Value), N);
	}

	/** How far out a place is on an outline of half-size (A, B) that is squarer than an ellipse: 1 on it, less inside it */
	double GetShare(const FVector2D& At, double A, double B, double N)
	{
		return FMath::Pow(Power(At.X / A, N) + Power(At.Y / B, N), 1.0 / N);
	}

	/**
	 *  The way out of a slice of the trunk for a place that is in it, in the slice's own flat. Anchor is a place further up
	 *  the same limb. Where the limb comes into the body says which face it has come in through, and the place is taken out
	 *  through that face: sideways through a side, straight forward through a front, straight back through a back. Never the
	 *  nearest way, which for wrists tied behind a narrow back was out through the belly.
	 */
	FVector2D GetWayOut(const FVector2D& At, const FVector2D& Anchor, double A, double B, double N, bool bShortest, bool bOutTheBack)
	{
		const double Share = GetShare(At, A, B, N);

		if (Share >= 1.0)
		{
			return FVector2D::ZeroVector;
		}

		// Wrists that are tied behind a back are behind it: straight back, however far in they are. On a narrow or a deep
		// body the clip has them past the middle of it, and any reckoning of the nearest way took them out by the belly.
		if (bOutTheBack)
		{
			const double X = FMath::Clamp(At.X, -A * 0.999, A * 0.999);

			return FVector2D(X, -B * FMath::Pow(FMath::Max(1.0 - Power(X / A, N), 0.0), 1.0 / N)) - At;
		}

		// Something long that is held is taken out the short way, whichever face that is through. A blade drawn back past a
		// flank comes into the body by the front of it, and taken out by the front it would have to cross the whole belly:
		// it is the flank it has to be turned out of.
		if (bShortest)
		{
			const double X = FMath::Clamp(At.X, -A * 0.999, A * 0.999);
			const double Y = FMath::Clamp(At.Y, -B * 0.999, B * 0.999);

			const FVector2D InDepth(X, ((FMath::Abs(At.Y) > 0.5 ? At.Y : Anchor.Y) >= 0.0 ? 1.0 : -1.0) * B * FMath::Pow(FMath::Max(1.0 - Power(X / A, N), 0.0), 1.0 / N));
			const FVector2D Sideways(((FMath::Abs(At.X) > 0.5 ? At.X : Anchor.X) >= 0.0 ? 1.0 : -1.0) * A * FMath::Pow(FMath::Max(1.0 - Power(Y / B, N), 0.0), 1.0 / N), Y);

			const double BySide = FMath::Pow(FVector2D::Distance(Sideways, At), 4.0);
			const double ByDepth = FMath::Pow(FVector2D::Distance(InDepth, At), 4.0);

			FVector2D Target = (BySide + ByDepth) > 1.e-9 ? FMath::Lerp(Sideways, InDepth, BySide / (BySide + ByDepth)) : Sideways;

			if (const double TargetShare = GetShare(Target, A, B, N); TargetShare > 0.001 && TargetShare < 1.0)
			{
				Target /= TargetShare;
			}

			return Target - At;
		}

		const double AnchorShare = GetShare(Anchor, A, B, N);

		FVector2D Entry;

		if (AnchorShare > 1.0)
		{
			FVector2D In = At;
			FVector2D Out = Anchor;

			for (int32 i = 0; i < 12; ++i)
			{
				const FVector2D Half = (In + Out) * 0.5;

				(GetShare(Half, A, B, N) < 1.0 ? In : Out) = Half;
			}

			Entry = Out;
		}
		else if (Share > 0.1)
		{
			// The limb did not come in through the outline: it came in over the top (a club hung down a back from a hand
			// above the head) or it is in to the shoulder. Then where the place itself is says which face is nearest it.
			Entry = At / Share;
		}
		else if (AnchorShare > 0.1)
		{
			Entry = Anchor / AnchorShare;
		}
		else
		{
			Entry = FVector2D(A, 0.0);
		}

		// how much of the way in was through the front or the back, and not through a side
		const double Depth = FMath::Clamp(Power(Entry.Y / B, N), 0.0, 1.0);

		const double X = FMath::Clamp(At.X, -A * 0.999, A * 0.999);
		const double Y = FMath::Clamp(At.Y, -B * 0.999, B * 0.999);

		const FVector2D InDepth(X, (Entry.Y >= 0.0 ? 1.0 : -1.0) * B * FMath::Pow(FMath::Max(1.0 - Power(X / A, N), 0.0), 1.0 / N));
		const FVector2D Sideways((Entry.X >= 0.0 ? 1.0 : -1.0) * A * FMath::Pow(FMath::Max(1.0 - Power(Y / B, N), 0.0), 1.0 / N), Y);

		FVector2D Target = FMath::Lerp(Sideways, InDepth, FMath::SmoothStep(0.25, 0.75, Depth));

		// between the two it is inside the outline again: back onto it
		if (const double TargetShare = GetShare(Target, A, B, N); TargetShare > 0.001 && TargetShare < 1.0)
		{
			Target /= TargetShare;
		}

		return Target - At;
	}

	/** Where the elbow goes for a wrist at a place: both lengths kept, bent toward a given side. The wrist is brought within reach. */
	FVector SolveElbow(const FVector& Shoulder, FVector& InOutWrist, double UpperLength, double LowerLength, const FVector& Toward)
	{
		FVector Line = InOutWrist - Shoulder;
		double Reach = Line.Size();

		if (Reach < 0.01)
		{
			Line = FVector::ForwardVector;
			Reach = 0.01;
		}

		Line /= Reach;

		Reach = FMath::Clamp(Reach, FMath::Abs(UpperLength - LowerLength) + 0.5, (UpperLength + LowerLength) * 0.997);

		InOutWrist = Shoulder + (Line * Reach);

		const double Along = (FMath::Square(UpperLength) - FMath::Square(LowerLength) + FMath::Square(Reach)) / (2.0 * Reach);
		const double Off = FMath::Sqrt(FMath::Max(FMath::Square(UpperLength) - FMath::Square(Along), 0.0));

		FVector Bend = (Toward - Shoulder) - (Line * FVector::DotProduct(Toward - Shoulder, Line));

		if (!Bend.Normalize())
		{
			Bend = FVector::CrossProduct(Line, FVector::UpVector).GetSafeNormal();

			if (Bend.IsNearlyZero())
			{
				Bend = FVector::RightVector;
			}
		}

		return Shoulder + (Line * Along) + (Bend * Off);
	}
}

void FGutClearOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	FIKRetargetOpBase::CopyStructProperties(FGutClearOpSettings::StaticStruct(), InSettingsToCopyFrom, this, TArray<FName>());
}

bool FGutClearOp::Initialize(const FIKRetargetProcessor& InProcessor, const FRetargetSkeleton& InSourceSkeleton, const FTargetSkeleton& InTargetSkeleton, const FIKRetargetOpBase* InParentOp, FIKRigLogger& Log)
{
	static const FName Arms[2][4] = { { FName("upperarm_l"), FName("lowerarm_l"), FName("hand_l"), FName("middle_01_l") }, { FName("upperarm_r"), FName("lowerarm_r"), FName("hand_r"), FName("middle_01_r") } };
	static const FName Legs[2][2] = { { FName("thigh_l"), FName("calf_l") }, { FName("thigh_r"), FName("calf_r") } };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			Arm[Side][i] = InTargetSkeleton.FindBoneIndexByName(Arms[Side][i]);
		}

		for (int32 i = 0; i < 2; ++i)
		{
			Leg[Side][i] = InTargetSkeleton.FindBoneIndexByName(Legs[Side][i]);
		}
	}

	// the performer's own hands: how far apart the clip has them is told from them, whatever body wears it
	PerformerHand[0] = InSourceSkeleton.FindBoneIndexByName(FName("hand_l"));
	PerformerHand[1] = InSourceSkeleton.FindBoneIndexByName(FName("hand_r"));

	Head[0] = InTargetSkeleton.FindBoneIndexByName(FName("neck_02"));
	Head[1] = InTargetSkeleton.FindBoneIndexByName(FName("head"));

	BoneNames = InTargetSkeleton.BoneNames;

	Shape = FGutClearShape();
	SectionBones.Reset();

	// a body without these arms is left as it comes: the op still counts as ready
	bIsInitialized = true;

	return true;
}

void FGutClearOp::AnimGraphPreUpdateMainThread(USkeletalMeshComponent& SourceMeshComponent, USkeletalMeshComponent& TargetMeshComponent)
{
	AGutFighter* Fighter = Cast<AGutFighter>(SourceMeshComponent.GetOwner());

	Shape.bOn = Fighter && Fighter->GetClearShape(Shape);

	// Somebody in a crowd has the same done for them, from the shape of the body they are seen as. They had nothing:
	// a clip's arms hang by its performer's sides, and worn by a broad body in a coat they stood out from it, shoulders
	// up, like somebody carrying two buckets ("I still see artifacts", 24 November). -GutNoCrowdArms is without.
	if (!Fighter && SourceMeshComponent.GetOwner() && SourceMeshComponent.GetOwner()->IsA<AGutCrowd>())
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoCrowdArms"));

		Shape.bOn = !bOff && AGutFighter::MeasureShapeOf(TargetMeshComponent.GetOwner(), &TargetMeshComponent, Shape);
	}

	SectionBones.Reset();

	if (Shape.bOn)
	{
		for (const FGutClearShape::FSection& Section : Shape.Trunk)
		{
			SectionBones.Add(BoneNames.IndexOfByKey(Section.Bone));
		}
	}
}

double FGutClearOp::GetUpperArmSunk(const FGutClearBody& Body, const FVector& Shoulder, const FVector& Elbow, int32 Side, FVector& OutWay) const
{
	// most of the way down it, and the elbow: against the shoulder an arm is where it is
	const FVector Places[] = { FMath::Lerp(Shoulder, Elbow, 0.62), Elbow };
	const double Counts[] = { 1.0 / 0.62, 1.0 };

	double Sunk = 0.0;

	for (int32 i = 0; i < Shape.Trunk.Num(); ++i)
	{
		const FGutClearShape::FSection& Section = Shape.Trunk[i];
		const FTransform& Spine = Body.Spine[i];

		// which side of this slice the arm belongs on
		const double Own = FVector::DotProduct(Spine.InverseTransformPosition(Shoulder), Section.Across) - Section.Middle.X >= 0.0 ? 1.0 : -1.0;

		const double Wide = Section.Half.X + Shape.UpperArm[Side];
		const double Deep = Section.Half.Y + Shape.UpperArm[Side];

		for (int32 Which = 0; Which < 2; ++Which)
		{
			const FVector Local = Spine.InverseTransformPosition(Places[Which]);
			const double Up = FVector::DotProduct(Local, Section.Along);

			if (Up < -Section.Below - 1.0 || Up > Section.Height + 1.0)
			{
				continue;
			}

			const double Out = (FVector::DotProduct(Local, Section.Across) - Section.Middle.X) * Own;
			const double Fore = FVector::DotProduct(Local, Section.Front) - Section.Middle.Y;

			// in front of the body or behind it altogether: that is the forearm's business
			if (FMath::Abs(Fore) >= Deep)
			{
				continue;
			}

			// how far out the arm has to be there to lie against the body's side, and how far short of that it is
			const double Needed = Wide * FMath::Pow(FMath::Max(1.0 - GutClear::Power(Fore / Deep, Settings.Squareness), 0.0), 1.0 / Settings.Squareness);
			const double Short = ((Needed - Out) + Settings.Clearance - Section.Give) * Counts[Which];

			if (Short > Sunk)
			{
				Sunk = Short;
				OutWay = Spine.TransformVectorNoScale(Section.Across).GetSafeNormal() * Own;
			}
		}
	}

	return Sunk;
}

FVector FGutClearBody::GetPush(const FVector& Place, double Thickness, const FVector& Anchor, bool bHeld, bool bOutTheBack) const
{
	FVector Push = FVector::ZeroVector;
	double Furthest = 0.0;

	for (int32 i = 0; i < Shape->Trunk.Num() && i < Spine.Num(); ++i)
	{
		const FGutClearShape::FSection& Section = Shape->Trunk[i];
		const FTransform& Bone = Spine[i];
		const FVector Local = Bone.InverseTransformPosition(Place);
		const double Up = FVector::DotProduct(Local, Section.Along);

		// A slice ends and the body goes on a little past it, rounding off: below the hips, and above the top of the chest
		// where the shoulders and the root of the neck are. The outline is drawn in to nothing over that much, so that a
		// place just past the end is still taken all the way out of what there is of the body there.
		const double Past = FMath::Max(-Section.Below - Up, Up - Section.Height);
		const double Cap = (i == Shape->Trunk.Num() - 1 && Up > Section.Height) ? 9.0 : 4.0;

		if (Past >= Cap)
		{
			continue;
		}

		const double Taper = Past > 0.0 ? FMath::Sqrt(1.0 - FMath::Square(Past / Cap)) : 1.0;

		const double A = (Section.Half.X + Thickness + Clearance) * Taper;
		const double B = (Section.Half.Y + Thickness + Clearance) * Taper;

		if (A < 0.5 || B < 0.5)
		{
			continue;
		}

		const FVector AnchorLocal = Bone.InverseTransformPosition(Anchor);

		const FVector2D At(FVector::DotProduct(Local, Section.Across) - Section.Middle.X, FVector::DotProduct(Local, Section.Front) - Section.Middle.Y);
		const FVector2D AnchorAt(FVector::DotProduct(AnchorLocal, Section.Across) - Section.Middle.X, FVector::DotProduct(AnchorLocal, Section.Front) - Section.Middle.Y);

		const FVector2D Way = GutClear::GetWayOut(At, AnchorAt, A, B, Squareness, bHeld, bOutTheBack);

		if (Way.SizeSquared() > FMath::Square(Furthest))
		{
			Furthest = Way.Size();
			Push = Bone.TransformVectorNoScale((Section.Across * Way.X) + (Section.Front * Way.Y));
		}
	}

	// The thighs, each a round limb from hip to knee. A hand or a club that is in one comes out the way the limb came in.
	for (int32 Which = 0; Which < 2 && bLegs; ++Which)
	{
		const FVector& Top = Hip[Which];
		const FVector Line = Knee[Which] - Top;
		const double Length = Line.Size();

		if (Length < 1.0)
		{
			continue;
		}

		const FVector Along = Line / Length;
		const double Up = FVector::DotProduct(Place - Top, Along);

		// from a little below the hip joint, where the trunk's lowest slice already is, to the knee
		if (Up < 4.0 || Up > Length)
		{
			continue;
		}

		const double Round = Shape->Thigh[Which] + Thickness + Clearance;
		const FVector Off = (Place - Top) - (Along * Up);
		const double Out = Off.Size();

		if (Out >= Round)
		{
			continue;
		}

		// Which way out. Something that only grazes a thigh goes straight out of it, the short way: a club lying across the
		// front of a leg was being taken out past the far side of the leg, toward the hand, the whole thigh's width. Only
		// what is deep in it comes out the way the limb came in, toward a place further up the limb that is outside.
		const FVector AnchorOff = (Anchor - Top) - (Along * FVector::DotProduct(Anchor - Top, Along));
		const FVector Straight = Off.GetSafeNormal();

		FVector Way = Straight;

		if (AnchorOff.Size() > Round && !bHeld)
		{
			Way = FMath::Lerp(AnchorOff.GetSafeNormal(), Straight, FMath::SmoothStep(0.2, 0.55, Out / Round)).GetSafeNormal();

			if (Way.IsNearlyZero())
			{
				Way = Straight;
			}
		}

		if (Way.IsNearlyZero())
		{
			Way = FVector::CrossProduct(Along, FVector::ForwardVector).GetSafeNormal() * (Which == 0 ? 1.0 : -1.0);
		}

		// onto the round of the thigh along that way: from where it is, not from the middle
		const double Across = FVector::DotProduct(Off, Way);
		const double Beside = FMath::Sqrt(FMath::Max(FMath::Square(Out) - FMath::Square(Across), 0.0));
		const double Needed = FMath::Sqrt(FMath::Max(FMath::Square(Round) - FMath::Square(Beside), 0.0)) - Across;

		// it dies away toward the hip, where the trunk takes over
		const double Share = FMath::Clamp((Up - 4.0) / 6.0, 0.0, 1.0);

		if (Needed * Share > Furthest)
		{
			Furthest = Needed * Share;
			Push = Way * Furthest;
		}
	}

	// And the head, for something held: straight out from the middle of it.
	if (bHeld && bHead)
	{
		const double Round = 10.5 + Thickness + Clearance;
		const FVector Off = Place - Head;
		const double Out = Off.Size();

		if (Out < Round && Round - Out > Furthest)
		{
			const FVector AnchorOff = Anchor - Head;

			Furthest = Round - Out;
			Push = (Out > 1.0 ? Off / Out : (AnchorOff.GetSafeNormal().IsNearlyZero() ? FVector::ForwardVector : AnchorOff.GetSafeNormal())) * Furthest;
		}
	}

	return Push;
}

void FGutClearOp::Run(FIKRetargetProcessor& InProcessor, const double InDeltaTime, const TArray<FTransform>& InSourceGlobalPose, TArray<FTransform>& OutTargetGlobalPose)
{
	if (!Shape.bOn || Shape.Trunk.IsEmpty() || SectionBones.Num() != Shape.Trunk.Num())
	{
		return;
	}

	const FRetargetSkeleton& Skeleton = InProcessor.GetSkeleton(ERetargetSourceOrTarget::Target);
	TArray<FTransform>& Pose = OutTargetGlobalPose;

	// the body where the clip has it: nothing here moves the spine, the legs or the head
	FGutClearBody Body;

	Body.Shape = &Shape;
	Body.Clearance = Settings.Clearance;
	Body.Squareness = Settings.Squareness;

	for (const int32 Bone : SectionBones)
	{
		if (!Pose.IsValidIndex(Bone))
		{
			return;
		}

		Body.Spine.Add(Pose[Bone]);
	}

	Body.bLegs = Pose.IsValidIndex(Leg[0][0]) && Pose.IsValidIndex(Leg[0][1]) && Pose.IsValidIndex(Leg[1][0]) && Pose.IsValidIndex(Leg[1][1]);

	for (int32 Side = 0; Side < 2 && Body.bLegs; ++Side)
	{
		Body.Hip[Side] = Pose[Leg[Side][0]].GetLocation();
		Body.Knee[Side] = Pose[Leg[Side][1]].GetLocation();
	}

	// the head bone is at the foot of the skull: its middle is a hand's breadth further along the neck's own line
	if (Pose.IsValidIndex(Head[0]) && Pose.IsValidIndex(Head[1]))
	{
		Body.bHead = true;
		Body.Head = Pose[Head[1]].GetLocation() + ((Pose[Head[1]].GetLocation() - Pose[Head[0]].GetLocation()).GetSafeNormal() * 9.0);
	}

	struct FSolved
	{
		bool bHas = false;
		FVector Shoulder, Elbow, Wrist;
		FVector ElbowNow, WristNow;
		FTransform Hand;
		FQuat HandTurn = FQuat::Identity;
		FVector Moved = FVector::ZeroVector;
		double Upper = 0.0, Lower = 0.0;
	};

	FSolved Arms[2];

	// Hands that are together stay together: tied, or clasped. How much they are, from how far apart the clip has them.
	double Together = 0.0;

	// Where each wrist is brought to, for hands the clip has together and this body has apart
	FVector Closed[2] = { FVector::ZeroVector, FVector::ZeroVector };
	double Close = 0.0;

	if (Pose.IsValidIndex(Arm[0][2]) && Pose.IsValidIndex(Arm[1][2]))
	{
		const FVector Left = Pose[Arm[0][2]].GetLocation();
		const FVector Right = Pose[Arm[1][2]].GetLocation();
		const double Worn = FVector::Dist(Left, Right);

		// By the performer's hands, where they are known. Told from the body that wears the clip, hands clasped in front
		// of a narrow performer were "apart" on a broad one in a vest: each was then taken out of the trunk its own way,
		// one to each thigh, and the Warden stood with his hands in his groin ("is this stance unnatural?", 24 November).
		const double Performed = (InSourceGlobalPose.IsValidIndex(PerformerHand[0]) && InSourceGlobalPose.IsValidIndex(PerformerHand[1]))
			? FVector::Dist(InSourceGlobalPose[PerformerHand[0]].GetLocation(), InSourceGlobalPose[PerformerHand[1]].GetLocation()) : Worn;

		// Only hands that are on each other: one laid over the other, a hand's breadth between the wrists. Arms that are
		// folded have their wrists a forearm apart and are no pair: taken for one, they were pushed out from the chest
		// together and stood crossed in the air in front of it.
		const double Clasped = 1.0 - FMath::SmoothStep(7.0, 12.0, Performed);

		Together = Shape.bBound ? 1.0 : FMath::Max(1.0 - FMath::SmoothStep(11.0, 18.0, Worn), Clasped);

		// and they are brought as near each other as the performer had them, about the place between them
		if (!Shape.bBound && Clasped > 0.0 && Worn > Performed + 0.5 && Worn > 1.0)
		{
			const FVector Between = (Left + Right) * 0.5;
			const FVector Across = (Right - Left) / Worn;

			Closed[0] = Between - (Across * (Performed * 0.5));
			Closed[1] = Between + (Across * (Performed * 0.5));
			Close = Clasped;
		}

		// -GutClearLog: how far apart the clip has the hands and this body has them, now and then
		static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("GutClearLog"));
		static double Logged = 0.0;

		if (bLog && FPlatformTime::Seconds() - Logged > 2.0 && Performed < 30.0)
		{
			Logged = FPlatformTime::Seconds();

			UE_LOG(LogGut, Log, TEXT("[hands] the clip has them %.1f cm apart and this body %.1f: together %.2f, brought together %.2f"), Performed, Worn, Together, Close);
		}
	}

	// the furthest something held is turned in the hand
	const double MostTurned = FMath::DegreesToRadians(Settings.MostTurned);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		FSolved& One = Arms[Side];

		if (!Pose.IsValidIndex(Arm[Side][0]) || !Pose.IsValidIndex(Arm[Side][1]) || !Pose.IsValidIndex(Arm[Side][2]))
		{
			continue;
		}

		One.bHas = true;
		One.Shoulder = Pose[Arm[Side][0]].GetLocation();
		One.Elbow = Pose[Arm[Side][1]].GetLocation();
		One.Wrist = Pose[Arm[Side][2]].GetLocation();
		One.Hand = Pose[Arm[Side][2]];
		One.Upper = FVector::Dist(One.Shoulder, One.Elbow);
		One.Lower = FVector::Dist(One.Elbow, One.Wrist);
		One.ElbowNow = One.Elbow;
		One.WristNow = One.Wrist;

		if (One.Upper < 1.0 || One.Lower < 1.0)
		{
			One.bHas = false;
			continue;
		}

		if (Close > 0.0)
		{
			One.WristNow = FMath::Lerp(One.Wrist, Closed[Side], Close);
			One.ElbowNow = GutClear::SolveElbow(One.Shoulder, One.WristNow, One.Upper, One.Lower, One.Elbow);
		}

		// An arm that hangs is first brought in to the side of this body: turned at the shoulder until the wrist is a
		// finger's breadth from the hip or the thigh as they are dressed. Worked out from where the clip has the arm,
		// every frame: what did this before was a second correction that measured the result of this one and chased it.
		if (Shape.HangIn > 0.01f && Together <= 0.0 && Body.bLegs && !Body.Spine.IsEmpty())
		{
			const FGutClearShape::FSection& Hips = Shape.Trunk[0];
			const FTransform& Pelvis = Body.Spine[0];

			const FVector Local = Pelvis.InverseTransformPosition(One.Wrist);
			const double Up = FVector::DotProduct(Local, Hips.Along);
			const double Thick = FMath::Min(Shape.Forearm[Side], 3.2f);

			// hanging: the wrist no higher than the hips, and the less as it comes up to them, so that an arm lifted
			// into a gesture leaves its place by the hip as it goes and does not jump out of it
			const double Low = FMath::SmoothStep(0.0, 1.0, FMath::Clamp((6.0 - Up) / 16.0, 0.0, 1.0));

			if (Low > 0.0)
			{
				const FVector2D At(FVector::DotProduct(Local, Hips.Across) - Hips.Middle.X, FVector::DotProduct(Local, Hips.Front) - Hips.Middle.Y);

				const double A = Hips.Half.X + Thick;
				const double B = Hips.Half.Y + Thick;

				double Gap = UE_BIG_NUMBER;

				// beside the hips, as far down as the hips are measured
				if (Up > -Hips.Below - 2.0 && FMath::Abs(At.Y) < B * 0.97)
				{
					Gap = FMath::Abs(At.X) - (A * FMath::Pow(FMath::Max(1.0 - GutClear::Power(At.Y / B, Settings.Squareness), 0.0), 1.0 / Settings.Squareness));
				}

				// and beside its own thigh
				const FVector Thigh = Body.Knee[Side] - Body.Hip[Side];
				const double Long = Thigh.Size();

				if (Long > 1.0)
				{
					const double Down = FMath::Clamp(FVector::DotProduct(One.Wrist - Body.Hip[Side], Thigh / Long), 0.0, Long);
					const FVector Nearest = Body.Hip[Side] + ((Thigh / Long) * Down);
					const FVector Off = One.Wrist - Nearest;

					// only its distance out to the side counts: a hand in front of a thigh is not hanging beside it
					const FVector Sideways = Pelvis.TransformVectorNoScale(Hips.Across).GetSafeNormal() * (At.X >= 0.0 ? 1.0 : -1.0);

					if (FVector::DotProduct(Off.GetSafeNormal(), Sideways) > 0.6)
					{
						Gap = FMath::Min(Gap, Off.Size() - (Shape.Thigh[Side] + Thick));
					}
				}

				if (Gap > Shape.HangGap && Gap < 45.0)
				{
					// and the less the further out it is held: an arm held well away from the side is held there on purpose
					const double Near = FMath::SmoothStep(0.0, 1.0, FMath::Clamp((45.0 - Gap) / 12.0, 0.0, 1.0));

					const FVector Inward = Pelvis.TransformVectorNoScale(Hips.Across).GetSafeNormal() * (At.X >= 0.0 ? -1.0 : 1.0);
					const FVector Hung = One.Wrist + (Inward * ((Gap - Shape.HangGap) * Shape.HangIn * Low * Near));

					FQuat Turn = FQuat::FindBetweenVectors(One.Wrist - One.Shoulder, Hung - One.Shoulder);

					if (Turn.GetAngle() > FMath::DegreesToRadians(28.0))
					{
						Turn = FQuat(Turn.GetRotationAxis(), FMath::DegreesToRadians(28.0));
					}

					One.ElbowNow = One.Shoulder + Turn.RotateVector(One.Elbow - One.Shoulder);
					One.WristNow = One.Shoulder + Turn.RotateVector(One.Wrist - One.Shoulder);
					One.HandTurn = Turn;
				}
			}
		}

		// The upper arm, out of the trunk's side. A bent arm is turned about the line from its shoulder to its wrist: the
		// elbow goes out and the hand stays where it is. A straight arm has no elbow to turn out, and is lifted away from
		// the side at the shoulder, hand and all.
		FVector Way = FVector::ZeroVector;

		const FVector ElbowWas = One.ElbowNow;
		const FVector WristWas = One.WristNow;

		if (const double Sunk = GetUpperArmSunk(Body, One.Shoulder, ElbowWas, Side, Way); Sunk > 0.0)
		{
			const FVector Out = One.Shoulder + ((ElbowWas + (Way * Sunk) - One.Shoulder).GetSafeNormal() * One.Upper);
			const FQuat Lift = FQuat::FindBetweenVectors(ElbowWas - One.Shoulder, Out - One.Shoulder);

			const FVector Hangs = (WristWas - One.Shoulder).GetSafeNormal();
			const FVector ToElbow = ElbowWas - One.Shoulder;
			// A hand that is with the other hand stays where it is however straight the arm: lifted away at the shoulder,
			// two wrists tied behind a broad back were swung out to the hips with the rope left hanging between them.
			const double Bent = FMath::Max(FMath::Clamp(((ToElbow - (Hangs * FVector::DotProduct(ToElbow, Hangs))).Size() - 4.0) / 6.0, 0.0, 1.0), Together);

			One.HandTurn = (FQuat::Slerp(Lift, FQuat::Identity, Bent) * One.HandTurn).GetNormalized();
			One.WristNow = FMath::Lerp(One.Shoulder + Lift.RotateVector(WristWas - One.Shoulder), WristWas, Bent);
			One.ElbowNow = GutClear::SolveElbow(One.Shoulder, One.WristNow, One.Upper, One.Lower, Out);
		}

		// The forearm, the hand and what is in it, out of the trunk and the thighs, looked at a few times over. The wrist is
		// moved by whichever of forearm, wrist and knuckles is furthest in. Something long that is held is turned in the
		// hand first, about the wrist, the way anybody keeps the end of a club out of their own back: moving the whole arm
		// until the far end of it was clear took the hand a forearm's length from where the clip had it.
		const FVector Before = One.WristNow;
		const FVector Knuckle = Pose.IsValidIndex(Arm[Side][3]) ? One.Hand.InverseTransformPositionNoScale(Pose[Arm[Side][3]].GetLocation()) : FVector::ZeroVector;

		double Turned = 0.0;

		for (int32 Pass = 0; Pass < 9; ++Pass)
		{
			const FQuat HandTurned = (One.HandTurn * One.Hand.GetRotation()).GetNormalized();

			FVector Most = FVector::ZeroVector;

			const auto Try = [&Most](const FVector& Push, double Counts)
			{
				if ((Push * Counts).SizeSquared() > Most.SizeSquared())
				{
					Most = Push * Counts;
				}
			};

			// the middle of the forearm moves about half as far as the wrist does
			Try(Body.GetPush(FMath::Lerp(One.ElbowNow, One.WristNow, 0.55), Shape.Forearm[Side], One.ElbowNow, false), 1.0 / 0.55);
			Try(Body.GetPush(One.WristNow, FMath::Min(Shape.Forearm[Side], 3.2f), One.ElbowNow, false), 1.0);
			Try(Body.GetPush(One.WristNow + HandTurned.RotateVector(Knuckle), 2.2, One.ElbowNow, false), 1.0);

			// What is held: the part of it by the hand goes where the hand goes. The far part is turned out, and it is the
			// place that asks for the biggest turn that is answered, a part of the way each time.
			FVector Lever = FVector::ZeroVector;
			FVector Swing = FVector::ZeroVector;
			double Asked = 0.0;

			for (const FVector4& Held : Shape.Held[Side])
			{
				const FVector Off = HandTurned.RotateVector(FVector(Held));
				const FVector Push = Body.GetPush(One.WristNow + Off, Held.W, One.WristNow, true);
				const double Far = Off.Size();

				if (Far < 15.0 || Turned >= MostTurned - 0.001)
				{
					Try(Push, 1.0);
					continue;
				}

				const FVector Sideways = Push - (Off * (FVector::DotProduct(Push, Off) / FMath::Square(Far)));
				const double Angle = FMath::Asin(FMath::Clamp(Sideways.Size() / Far, 0.0, 1.0));

				if (Angle > Asked)
				{
					Asked = Angle;
					Swing = Sideways;
					Lever = Off;
				}
			}

			if (Most.SizeSquared() < FMath::Square(0.05) && Asked < 0.002)
			{
				break;
			}

			// turned in the hand, and the hand moved, both in the one look
			if (Asked >= 0.002)
			{
				const FVector Axis = FVector::CrossProduct(Lever, Swing).GetSafeNormal();
				const double Angle = FMath::Min3(Asked * 1.05, FMath::DegreesToRadians(25.0), MostTurned - Turned);

				if (!Axis.IsNearlyZero() && Angle > 0.0)
				{
					One.HandTurn = (FQuat(Axis, Angle) * One.HandTurn).GetNormalized();
					Turned += Angle;
				}
			}

			if (Most.SizeSquared() < FMath::Square(0.05))
			{
				continue;
			}

			One.WristNow += Most;

			if (FVector::DistSquared(One.WristNow, Before) > FMath::Square(Settings.MostMoved))
			{
				One.WristNow = Before + ((One.WristNow - Before).GetSafeNormal() * Settings.MostMoved);
			}

			One.ElbowNow = GutClear::SolveElbow(One.Shoulder, One.WristNow, One.Upper, One.Lower, One.ElbowNow);
		}

		One.Moved = One.WristNow - Before;
	}

	// Hands that are together stay together: tied, or clasped. Each on its own would be taken its own way out (two wrists
	// tied behind a back, one out by each hip), so the pair is moved as one thing: as thick as a wrist and half as wide
	// again as they are apart, from where the clip has it, looked at from between the elbows. Tied, it goes out the back.
	if (Arms[0].bHas && Arms[1].bHas)
	{
		const double Apart = FVector::Dist(Arms[0].Wrist, Arms[1].Wrist);

		if (Together > 0.0)
		{
			const FVector Between = (Arms[0].Wrist + Arms[1].Wrist) * 0.5;
			const FVector Elbows = (Arms[0].ElbowNow + Arms[1].ElbowNow) * 0.5;
			const double Thick = FMath::Min(Shape.Forearm[0], 3.2f) + FMath::Min(Apart * 0.25, 4.0);

			FVector Both = FVector::ZeroVector;
			FVector At = Between;

			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				const FVector Push = Body.GetPush(At, Thick, Elbows, false, Shape.bBound);

				if (Push.SizeSquared() < FMath::Square(0.05))
				{
					break;
				}

				At += Push;
				Both += Push;
			}

			Both = Both.GetClampedToMaxSize(Settings.MostMoved);

			for (FSolved& One : Arms)
			{
				// from where the upper arm's turn left the wrist, the pair's way and not its own
				const FVector Own = One.WristNow - One.Moved;

				One.WristNow = FMath::Lerp(One.WristNow, Own + Both, Together);
				One.ElbowNow = GutClear::SolveElbow(One.Shoulder, One.WristNow, One.Upper, One.Lower, One.ElbowNow);
			}
		}
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		FSolved& One = Arms[Side];

		if (!One.bHas)
		{
			continue;
		}

		// the wrist moved, the elbow may be back in the trunk's side: out again, about the line to where the wrist is now
		FVector Way = FVector::ZeroVector;

		if (const double Sunk = GetUpperArmSunk(Body, One.Shoulder, One.ElbowNow, Side, Way); Sunk > 0.0)
		{
			One.ElbowNow = GutClear::SolveElbow(One.Shoulder, One.WristNow, One.Upper, One.Lower, One.ElbowNow + (Way * Sunk * 1.5));
		}

		if (FVector::DistSquared(One.ElbowNow, One.Elbow) < FMath::Square(0.02) && FVector::DistSquared(One.WristNow, One.Wrist) < FMath::Square(0.02) && One.HandTurn.GetAngle() < 0.002)
		{
			continue;
		}

		// the upper arm turned to its elbow, the forearm to its wrist, and the hand set down turned as it was
		FTransform Upper = Pose[Arm[Side][0]];

		Upper.SetRotation((FQuat::FindBetweenVectors(One.Elbow - One.Shoulder, One.ElbowNow - One.Shoulder) * Upper.GetRotation()).GetNormalized());

		Skeleton.SetGlobalTransformAndUpdateChildren(Arm[Side][0], Upper, Pose);

		FTransform Lower = Pose[Arm[Side][1]];

		Lower.SetRotation((FQuat::FindBetweenVectors(Pose[Arm[Side][2]].GetLocation() - Lower.GetLocation(), One.WristNow - One.ElbowNow) * Lower.GetRotation()).GetNormalized());
		Lower.SetLocation(One.ElbowNow);

		Skeleton.SetGlobalTransformAndUpdateChildren(Arm[Side][1], Lower, Pose);

		FTransform Hand = Pose[Arm[Side][2]];

		Hand.SetRotation((One.HandTurn * One.Hand.GetRotation()).GetNormalized());
		Hand.SetLocation(One.WristNow);

		Skeleton.SetGlobalTransformAndUpdateChildren(Arm[Side][2], Hand, Pose);
	}
}
