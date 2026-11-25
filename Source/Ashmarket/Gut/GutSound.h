// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "GutTypes.h"
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
 *  A kind of sound that lies under a place for as long as anybody is in it: the night, a crowd standing about, a market.
 *  It is never one recording going round and round. One of the kind's recordings is played for a while, then another
 *  is faded in over it as it fades out, and never the same one twice running.
 */
USTRUCT()
struct FGutSoundBed
{
	GENERATED_BODY()

	FName Kind;

	/** How loud whoever started it asked for, against the kind's own loudness */
	float Volume = 1.0f;

	/** The recording that is being heard now */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Playing;

	/** When, by the world's clock, the next recording is brought in */
	double ChangeAt = 0.0;

	/** How many recordings it has gone through: what a test counts */
	int32 Changes = 0;

	bool bStopped = false;

	/** How loud it is at full, and how much of that is heard now: a crowd's murmur is heard by how near the crowd is */
	float Base = 1.0f;
	float Share = 1.0f;
};

/**
 *  Plays the Gut's sounds. Anything in the cage asks for a sound by name and says where it is.
 */
UCLASS()
class UGutSoundSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	static UGutSoundSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Moves each bed on to its next recording when its time has come */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGutSoundSubsystem, STATGROUP_Tickables); }

	/**
	 *  Starts a bed of a kind, heard everywhere, and returns the number to stop it or change its loudness by (less than
	 *  nought if the kind has no recordings). It goes from one recording of the kind to another for as long as it lasts.
	 */
	int32 StartBed(FName Kind, float Volume = 1.0f, float FadeIn = 2.0f);

	void StopBed(int32 Bed, float FadeOut = 1.5f);

	/** How much of its full loudness a bed is heard at from now on: 1 as it was started */
	void SetBedShare(int32 Bed, float Share);

	/** How sounds that come from a place fall off with distance, for whoever plays one of their own from a place: a voice */
	USoundAttenuation* GetAttenuation();

	/** How many recordings a bed has gone through so far, or nought */
	int32 GetBedChanges(int32 Bed) const { return Beds.IsValidIndex(Bed) ? Beds[Bed].Changes : 0; }

	/** For the tests: brings every bed's next change forward to now */
	void HurryBeds();

	/** Plays one recording of a kind, once, at a place. Volume and pitch are multiplied onto the kind's own. */
	void Play(FName Kind, const FVector& Where, float Volume = 1.0f, float Pitch = 1.0f, uint32 Whose = 0);

	/** Plays one recording of a kind over and over until the returned component is stopped. Attached to a component if one is given. */
	UAudioComponent* Loop(FName Kind, const FVector& Where, float Volume = 1.0f, USceneComponent* AttachTo = nullptr);

	/** "woman" or "man", for building the name of a voice's sounds, from the name of a body's class */
	FName VoiceOf(const FString& BodyClassName);

	/** He or she, for whoever is seen as this body. A man where nothing is known. */
	static FGutWho WhoIs(const UObject* WorldContextObject, const TSoftClassPtr<AActor>& Body);

	bool Has(FName Kind);

	/** The kind a bare foot makes coming down here: on the pool's tile, or on the ground outside it */
	FName GetFootfall() const { return Footfall; }

	/** How many times a kind has been played since the level began: what a test listens with */
	int32 GetTimesPlayed(FName Kind) const { return TimesPlayed.FindRef(Kind); }

	/**
	 *  The night's far sounds, for a place that is walked about in: gunfire a long way off, and now and then somebody
	 *  screaming (23 November: "add gun shots and sounds of screaming. But random... you decide the % that makes it real and
	 *  not hollywood"). Not on a beat and not often: see UpdateStrays for how often and why.
	 */
	void SetStrays(bool bOn) { bStrays = bOn; }

	/** For the tests: the next far sound is due now */
	void HurryStrays();

private:

	/** The kind's set and one of its recordings, or null if there is none or it was asked for too soon after the last */
	/** @param Whose  not zero for a voice: whose it is. Each owner has about half of a kind's recordings to themselves, so that two people do not make the same sounds. */
	USoundBase* Choose(FName Kind, const FGutSoundSet*& OutSet, bool bCountGap, uint32 Whose = 0);

	/** The next recording for a bed of this kind: dealt so that each is heard once, in any order, before any is heard again, across every place of a sitting */
	USoundBase* ChooseForBed(FName Kind, const FGutSoundSet*& OutSet);

	UGutSoundData* GetData();

	UPROPERTY(Transient)
	TObjectPtr<UGutSoundData> Data;

	bool bLookedForData = false;

	TMap<FName, int32> TimesPlayed;

	FName Footfall = FName("step");

	/** Brings in the bed's next recording, fading the one before it out under it */
	void ChangeBed(FGutSoundBed& Bed, float Fade);

	UPROPERTY(Transient)
	TArray<FGutSoundBed> Beds;

	TMap<FName, int32> LastChosen;
	TMap<FName, double> LastPlayed;

	/** Plays what far sounds have come due, and says when the next are */
	void UpdateStrays(float DeltaTime);

	bool bStrays = false;

	/** Far sounds that follow one already heard: a second shot, an answer, a scream after it. When (in the level's time), what, how loud and from where. */
	struct FStrayDue
	{
		double At = 0.0;
		FName Kind;
		float Volume = 1.0f;
		FVector Way = FVector::ForwardVector;
	};

	TArray<FStrayDue> StraysDue;
};
