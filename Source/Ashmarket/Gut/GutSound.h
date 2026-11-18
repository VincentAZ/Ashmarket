// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "GutSound.generated.h"

class UAudioComponent;
class UReverbEffect;
class USoundAttenuation;
class USoundBase;

/** Several recordings of one kind of sound. One of them is played each time, never the same one twice running. */
USTRUCT(BlueprintType)
struct FGutSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** How loud this kind is against the rest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta = (ClampMin = 0, ClampMax = 4))
	float Volume = 1.0f;

	/** Each time it is played a little lower or higher than recorded, between these, so that the same recording does not sound the same */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta = (ClampMin = 0.25, ClampMax = 4))
	float LowPitch = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta = (ClampMin = 0.25, ClampMax = 4))
	float HighPitch = 1.05f;

	/** If true it is not in any one place: it is heard the same wherever the listener stands. For the night, the crowd as a whole, the cards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound")
	bool bEverywhere = false;

	/** The least time between two of this kind, so that something that happens many times in a moment does not rattle like a machine */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound", meta = (ClampMin = 0, Units = "s"))
	float LeastGap = 0.0f;
};

/**
 *  Every sound the Gut makes, by the name the game asks for it by. All of them are recordings, brought in by Tools/import_gut_sound.sh.
 *  A name with no recordings is silence: the game runs the same with or without sound.
 */
UCLASS(BlueprintType)
class UGutSoundData : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TMap<FName, FGutSoundSet> Sets;

	/** How a sound that is somewhere in the cage fades with distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** The ring of the place: an empty tiled pool, roofed with wire, open to the night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<UReverbEffect> Reverb;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (ClampMin = 0, ClampMax = 2))
	float ReverbVolume = 0.6f;

	/** The bodies that have a woman's voice, by the name of the character's Blueprint. Every other body has a man's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TSet<FName> WomenBodies;

	/** Kinds that play without end from the moment the cage opens, such as the night and the room */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TArray<FName> Always;
};

/**
 *  Plays the Gut's sounds. Anything in the cage asks for a sound by name and says where it is.
 */
UCLASS()
class UGutSoundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UGutSoundSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Plays one recording of a kind, once, at a place. Volume and pitch are multiplied onto the kind's own. */
	void Play(FName Kind, const FVector& Where, float Volume = 1.0f, float Pitch = 1.0f);

	/** Plays one recording of a kind over and over until the returned component is stopped. Attached to a component if one is given. */
	UAudioComponent* Loop(FName Kind, const FVector& Where, float Volume = 1.0f, USceneComponent* AttachTo = nullptr);

	/** "woman" or "man", for building the name of a voice's sounds, from the name of a body's class */
	FName VoiceOf(const FString& BodyClassName);

	bool Has(FName Kind);

	/** The kind a bare foot makes coming down here: on the pool's tile, or on the ground outside it */
	FName GetFootfall() const { return Footfall; }

	/** How many times a kind has been played since the level began: what a test listens with */
	int32 GetTimesPlayed(FName Kind) const { return TimesPlayed.FindRef(Kind); }

private:

	/** The kind's set and one of its recordings, or null if there is none or it was asked for too soon after the last */
	USoundBase* Choose(FName Kind, const FGutSoundSet*& OutSet, bool bCountGap);

	UGutSoundData* GetData();

	UPROPERTY(Transient)
	TObjectPtr<UGutSoundData> Data;

	bool bLookedForData = false;

	TMap<FName, int32> TimesPlayed;

	FName Footfall = FName("step");

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> AlwaysPlaying;

	TMap<FName, int32> LastChosen;
	TMap<FName, double> LastPlayed;
};
