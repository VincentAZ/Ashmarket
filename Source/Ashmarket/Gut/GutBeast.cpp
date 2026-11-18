// Ashmarket prototype: the Gut.

#include "GutBeast.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutBeast)

void UGutBeastAnimInstance::Show(const UAnimSequenceBase* Clip, float Time, bool bRestart, float BlendTime)
{
	if (Clip != Inputs.Clip || bRestart)
	{
		++Inputs.Change;

		Inputs.BlendTime = BlendTime;
	}

	Inputs.Clip = Clip;
	Inputs.Time = Time;
}

FAnimInstanceProxy* UGutBeastAnimInstance::CreateAnimInstanceProxy()
{
	return new FGutBeastAnimInstanceProxy(this);
}

void FGutBeastAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UGutBeastAnimInstance* Instance = Cast<UGutBeastAnimInstance>(InAnimInstance);

	if (!Instance)
	{
		return;
	}

	Inputs = Instance->GetInputs();

	if (Inputs.Change != SeenChange)
	{
		SeenChange = Inputs.Change;

		// the body is left where it was in the old clip, and the new one comes in over it
		Before = Latest;
		BeforeTime = LatestTime;

		Arrived = Before ? 0.0f : 1.0f;
	}
	else if (Arrived < 1.0f)
	{
		Arrived = FMath::Min(1.0f, Arrived + (DeltaSeconds / FMath::Max(Inputs.BlendTime, 0.01f)));
	}

	Latest = Inputs.Clip;
	LatestTime = Inputs.Time;
}

bool FGutBeastAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!Latest)
	{
		Output.ResetToRefPose();
		return true;
	}

	{
		FAnimationPoseData Data(Output);

		Latest->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(LatestTime), false, {}, true));
	}

	if (Arrived < 1.0f && Before)
	{
		FPoseContext Old(Output);
		FPoseContext New(Output);

		New = Output;

		{
			FAnimationPoseData Data(Old);

			Before->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(BeforeTime), false, {}, true));
		}

		// eased at both ends, as something with weight leaves one thing for another
		const float Amount = FMath::SmoothStep(0.0f, 1.0f, Arrived);

		const FAnimationPoseData OldData(Old);
		const FAnimationPoseData NewData(New);

		FAnimationPoseData Result(Output);

		FAnimationRuntime::BlendTwoPosesTogether(OldData, NewData, 1.0f - Amount, Result);
	}

	return true;
}
