// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Engine/DataAsset.h"
#include "GutOpponentController.h"
#include "GutBeast.generated.h"

class UAnimSequenceBase;
class UGutWeaponData;
class USkeletalMesh;

/** What an animal's body is doing, which decides the clip it is in */
UENUM(BlueprintType)
enum class EGutBeastPose : uint8
{
	Stand,
	Move,
	Walk,
	Strike,
	Leap,
	Maul,
	Hurt,
	Dead
};

/**
 *  An animal that can be put in the cage (design document, chapter 7: "fighter against animal", and "released dogs" among
 *  the hazards). In the rules it is a fighter like any other: it has health, it is struck and it strikes, and the cage's
 *  brain runs it. What is different is here: its body and how that moves, what it bites with, and how it fights.
 *
 *  The body is a model with a skeleton and clips of its own, made outside the game. The game only chooses which clip.
 */
UCLASS(BlueprintType)
class UGutBeastData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** What the HUD calls it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Body")
	TObjectPtr<USkeletalMesh> Mesh;

	/** Size of the model in the cage, and how it is turned so that it faces the way its fighter faces */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Body", meta = (ClampMin = 0.01))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Body")
	FRotator Turn = FRotator::ZeroRotator;

	/**
	 *  Coats, so that two of them in the cage are not the same dog twice: each is a colour its own coat is multiplied by,
	 *  dealt in turn. The model's material has to take a "BaseColorFactor", as one brought in from a glTF file does.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Body")
	TArray<FLinearColor> Coats;

	/** Standing its ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Stand;

	/** Going somewhere fast, and the speed over the ground its legs cover when the clip is played as it was made */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 1, Units = "cm/s"))
	float MoveClipSpeed = 400.0f;

	/** Going somewhere slowly: the same for a walk, and the speed under which it walks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 1, Units = "cm/s"))
	float WalkClipSpeed = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 1, Units = "cm/s"))
	float WalkUnderSpeed = 200.0f;

	/** A bite, and the time in the clip at which the jaws close */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Strike;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 0, Units = "s"))
	float StrikeImpactTime = 0.3f;

	/** Going for somebody from further off, and the time in the clip at which it lands on them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Leap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 0, Units = "s"))
	float LeapImpactTime = 0.4f;

	/** At somebody who is down or as good as: played round and round for as long as it takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Maul;

	/** Struck */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Hurt;

	/** Killed: it ends on the floor and stays as it ended. And the time in the clip it is begun at, if not at its start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion")
	TObjectPtr<UAnimSequenceBase> Death;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Motion", meta = (ClampMin = 0, Units = "s"))
	float DeathStartTime = 0.0f;

	/** What it bites with: the attacks, their reach and what they cost whoever they land on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Fight")
	TObjectPtr<UGutWeaponData> Bite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Fight", meta = (ClampMin = 1))
	float Health = 80.0f;

	/** How fast it is on its feet, against a fighter's own speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Fight", meta = (ClampMin = 0.1, ClampMax = 3))
	float SpeedScale = 1.25f;

	/** How the cage's brain fights for it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Fight")
	FGutOpponentTuning Tuning;

	/** What its sounds are filed under: its snarl as it strikes is effort_<voice>, its yelp pain_<voice>, its last death_<voice> */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Sound")
	FName Voice = FName("dog");

	/** The sound it makes now and again while it is alive and loose, and how long between */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Sound")
	FName IdleSound = FName("dog_growl");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beast|Sound", meta = (ClampMin = 0.2, Units = "s"))
	float IdleSoundGap = 3.0f;
};

/** What the game tells an animal's body each frame: the clip it is in, where in it, and when that last changed */
USTRUCT()
struct FGutBeastAnimInputs
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<const UAnimSequenceBase> Clip;

	float Time = 0.0f;

	/** Goes up by one each time the body is put into a different clip, or the same one from its start */
	int32 Change = 0;

	/** How long the body takes to get from what it was doing into the clip */
	float BlendTime = 0.15f;
};

/** Reads the clip the game chose and, for a moment after each change, eases into it from the pose the body was in */
USTRUCT()
struct FGutBeastAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FGutBeastAnimInstanceProxy() = default;

	explicit FGutBeastAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	FGutBeastAnimInputs Inputs;

	/** The clip the body was in before the last change, and where in it the body was left */
	const UAnimSequenceBase* Before = nullptr;
	float BeforeTime = 0.0f;

	const UAnimSequenceBase* Latest = nullptr;
	float LatestTime = 0.0f;

	int32 SeenChange = 0;

	/** From 0, just changed, to 1, wholly in the new clip */
	float Arrived = 1.0f;
};

/**
 *  The animation of an animal in the cage. It has no graph: the fighter that the animal is tells it which clip and where
 *  in it, and this joins one clip to the next. See AGutFighter::UpdateBeast.
 */
UCLASS(Transient, NotBlueprintable)
class UGutBeastAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** Puts the body in a clip at a time in it. Asking for a different clip, or bRestart, eases into it from the present pose. */
	void Show(const UAnimSequenceBase* Clip, float Time, bool bRestart = false, float BlendTime = 0.15f);

	const FGutBeastAnimInputs& GetInputs() const { return Inputs; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FGutBeastAnimInputs Inputs;
};
