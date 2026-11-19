// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GutCrowd.generated.h"

class UIKRetargeter;
class URetargetComponent;
class UAnimSequenceBase;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 *  The crowd on the deck round the cage, looking down through the lid: placeholder bodies standing in a ring.
 *  More of them come the bigger the player's name is, and some of them jump when something happens below.
 *
 *  Place it at the center of the cage floor. It is only for show, and there is none of it when the game runs with no renderer.
 */
class UAudioComponent;

UCLASS()
class AGutCrowd : public AActor
{
	GENERATED_BODY()

	/** Root component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

protected:

	/** Body used for every spectator. All of them are adults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TObjectPtr<USkeletalMesh> SpectatorMesh;

	/** Animation a spectator loops while it watches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** Animation a spectator plays once when it cheers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TObjectPtr<UAnimSequenceBase> CheerAnimation;

	/** Ways of standing and watching. Each spectator is dealt one. If empty, everybody uses the Idle Animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TArray<TObjectPtr<UAnimSequenceBase>> IdleAnimations;

	/** Ways of cheering. One is picked each time a spectator cheers. If empty, everybody uses the Cheer Animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TArray<TObjectPtr<UAnimSequenceBase>> CheerAnimations;

	/** Size of the cage floor the crowd stands round */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (Units = "cm"))
	FVector2D FloorSize = FVector2D(1400.0f, 1000.0f);

	/** Height of the deck above the cage floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float DeckHeight = 320.0f;

	/** How far back from the edge of the cage the nearest spectators stand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float EdgeDistance = 45.0f;

	/** How far back from the edge the crowd reaches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float Depth = 200.0f;

	/** Spectators who come to see a fighter nobody has heard of */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 500))
	int32 BaseAttendance = 14;

	/** More spectators for each point of the fighter's name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 100))
	float AttendancePerName = 2.5f;

	/** The deck holds no more than this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd", meta = (ClampMin = 0, ClampMax = 500))
	int32 MaxAttendance = 70;

	/** Name of the color parameter on the body materials */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	FName TintParameter = FName("Paint Tint");

	/**
	 *  People made elsewhere (MetaHumans) to stand on the deck in place of the placeholder figures, taken in turn.
	 *  Each is a whole character that follows a placeholder nobody sees. Leave empty for the placeholders.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TArray<TSoftClassPtr<AActor>> SpectatorClasses;

	/** How the placeholder's pose is carried over to those people */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TObjectPtr<UIKRetargeter> SpectatorRetargeter;

	/** Name of the skeletal mesh in such a person that the rest of it follows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	FName PersonBodyName = FName("Body");

	/** Names of the color parameters on their clothes, so that no two are dressed quite alike */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TArray<FName> ClothParameters = { FName("diffuse_color_1"), FName("diffuse_color_2") };

	/**
	 *  Where each of a crowd stands and the way each faces, in the level, for a crowd that stands where it is told to and
	 *  not round a cage: bidders in a square. With any, the crowd is these and nobody else, from the start.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gut|Crowd")
	TArray<FTransform> StandingPlaces;

	/** True once a crowd that was told where to stand has been stood there */
	bool bStoodWhereTold = false;
	int32 FramesWaited = 0;

	/** One spectator */
	struct FSpectator
	{
		TWeakObjectPtr<USkeletalMeshComponent> Body;

		/** The person seen in its place, if there is one, and what makes that person follow it */
		TWeakObjectPtr<AActor> Person;
		TWeakObjectPtr<URetargetComponent> Follow;

		/** Which of the people it is to be seen as, or none, and the color of that person's clothes */
		int32 PersonIndex = INDEX_NONE;
		FLinearColor Tint = FLinearColor::Black;

		/** Time left of the cheer it is in the middle of. Zero when it is only watching. */
		float CheerTimeLeft = 0.0f;

		/** Time until a cheer it has been asked for starts, so the deck does not jump as one */
		float CheerDelay = -1.0f;
	};

	TArray<FSpectator> Spectators;

	/** The noise of the crowd standing about, and how many of them it was started for */
	int32 Murmur = INDEX_NONE;

	int32 HeardSpectators = 0;

	/** A crowd yelling at a fight, laid under the fight while it lasts: one of them, and a second for a deck that is full */
	TArray<int32> FightNoise;

	/** True while that is being heard */
	bool bFightHeard = false;

	/** Time until somebody on the deck next shouts something, and how many more are to shout straight after */
	float ShoutIn = 0.0f;
	int32 ShoutsDue = 0;

	/** One of the crowd shouts something, heard from where it stands */
	void Shout();

	/** How many spectators are still waiting for their person to be fetched */
	int32 PeopleStillToCome = 0;

	/** Starts fetching the people in the background, if that has not been started */
	void FetchPeople();

	/** Puts the person a spectator is to be seen as in its place, if that person has been fetched. Returns true if it did. */
	bool GivePerson(FSpectator& Spectator);

	/** Keeps the people loaded once they have been fetched in the background */
	TSharedPtr<struct FStreamableHandle> PeopleLoading;

public:

	/** Constructor */
	AGutCrowd();

	/** Returns a way of standing or of cheering, picked at random from the list, or the single one if the list is empty */
	static UAnimSequenceBase* PickFrom(const TArray<TObjectPtr<UAnimSequenceBase>>& Several, UAnimSequenceBase* Single);

	/** Runs the cheers */
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Fills the deck for a fighter with the given name, in place of whoever was there */
	UFUNCTION(BlueprintCallable, Category="Gut|Crowd")
	void Populate(int32 FighterName);

	/** Makes the given share of the crowd cheer, each starting within the given time */
	UFUNCTION(BlueprintCallable, Category="Gut|Crowd")
	void Cheer(float Share, float WithinTime = 0.4f);

	/** Returns the number of spectators who would come to see a fighter with the given name */
	UFUNCTION(BlueprintPure, Category="Gut|Crowd")
	int32 GetAttendanceFor(int32 FighterName) const;

	/** Returns the number of spectators on the deck */
	UFUNCTION(BlueprintPure, Category="Gut|Crowd")
	int32 GetNumSpectators() const { return Spectators.Num(); }

	/** Returns true while the crowd is yelling at a fight that is going on */
	UFUNCTION(BlueprintPure, Category="Gut|Crowd")
	bool IsYelling() const { return bFightHeard; }

	/** Returns the number of spectators cheering, or about to */
	UFUNCTION(BlueprintPure, Category="Gut|Crowd")
	int32 GetNumCheering() const;

protected:

	/** Returns a place to stand on the deck, and the way to face from it */
	FVector PickStandingPlace(FRotator& OutFacing) const;
};
