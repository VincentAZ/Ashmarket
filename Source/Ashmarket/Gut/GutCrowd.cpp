// Ashmarket prototype: the Gut.

#include "GutCrowd.h"
#include "Components/AudioComponent.h"
#include "GutSound.h"
#include "Animation/AnimInstance.h"
#include "GutAnimInstance.h"
#include "GutGameMode.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LODSyncComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RetargetComponent.h"
#include "Retargeter/IKRetargeter.h"

namespace GutCrowdConstants
{
	/** The body mesh stands with its feet at its origin and faces along its right */
	const float MeshYawOffset = -90.0f;

	/** Spectators are adults of ordinary heights */
	const float MinScale = 0.95f;
	const float MaxScale = 1.08f;
}

AGutCrowd::AGutCrowd()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AGutCrowd::Tick(float DeltaTime)
{
	// the noise of people standing about, for as long as there are people: louder the more of them there are
	if (Spectators.Num() != HeardSpectators)
	{
		HeardSpectators = Spectators.Num();

		// a bed of them, that goes from one recording of a crowd to another and never round and round one
		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			Sounds->StopBed(Murmur, 1.0f);

			Murmur = HeardSpectators > 0 ? Sounds->StartBed(FName("crowd_bed"), FMath::Clamp(HeardSpectators / 40.0f, 0.3f, 1.2f), 1.5f) : INDEX_NONE;
		}
	}

	// a crowd that has been told where to stand is standing there from the start: from a moment after it, when the level
	// has been drawn a number of times and has less to do. (The frames a level opens in are when the town stands its people,
	// one to a frame, and that many bodies made in one frame have stopped the renderer dead.)
	if (!bStoodWhereTold && !StandingPlaces.IsEmpty() && ++FramesWaited > 45 && GetGameTimeSinceCreation() > 0.4f)
	{
		bStoodWhereTold = true;

		Populate(0);
	}

	// and while a fight is going on they yell at it: a crowd at a fight laid under it, a second one for a full deck, and
	// every so often one voice out of them with words in it. -GutNoShouts leaves the crowd as it was before it had any.
	static const bool bShouts = !FParse::Param(FCommandLine::Get(), TEXT("GutNoShouts"));

	const AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>();
	const bool bFight = bShouts && GameMode && GameMode->GetPhase() == EGutFightPhase::Fighting && !Spectators.IsEmpty();

	if (bFight != bFightHeard)
	{
		bFightHeard = bFight;

		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			for (const int32 Yelling : FightNoise)
			{
				// the roar at the end of a fight covers their going
				Sounds->StopBed(Yelling, 3.0f);
			}
		}

		FightNoise.Reset();

		if (UGutSoundSubsystem* Sounds = bFight ? UGutSoundSubsystem::Get(this) : nullptr)
		{
			// a full deck is two crowds laid one on the other, not one crowd turned up
			const float Size = FMath::Clamp(Spectators.Num() / 40.0f, 0.3f, 1.0f);

			for (int32 Layer = 0; Layer < (Spectators.Num() >= 30 ? 2 : 1); ++Layer)
			{
				if (const int32 Yelling = Sounds->StartBed(FName("crowd_fight"), Size * (Layer == 0 ? 1.0f : 0.7f), Layer == 0 ? 1.5f : 4.0f); Yelling != INDEX_NONE)
				{
					FightNoise.Add(Yelling);
				}
			}

			// the first of them has something to say almost at the bell
			ShoutIn = FMath::FRandRange(0.4f, 1.5f);
			ShoutsDue = 0;
		}
	}

	if (bFightHeard)
	{
		ShoutIn -= DeltaTime;

		if (ShoutIn <= 0.0f)
		{
			Shout();

			if (ShoutsDue > 0)
			{
				// set off by something that happened: one on top of another
				--ShoutsDue;

				ShoutIn = FMath::FRandRange(0.15f, 0.5f);
			}
			else
			{
				// the more of them there are, the less time goes by with nobody shouting
				ShoutIn = FMath::Clamp(90.0f / Spectators.Num(), 1.2f, 5.0f) * FMath::FRandRange(0.5f, 1.5f);
			}
		}
	}

	Super::Tick(DeltaTime);

	// the people come on a couple at a time as they are fetched, so the deck fills without the fight stalling for them
	if (PeopleStillToCome > 0)
	{
		int32 Arrived = 0;

		for (FSpectator& Spectator : Spectators)
		{
			if (Arrived >= 2)
			{
				break;
			}

			if (Spectator.PersonIndex != INDEX_NONE && !Spectator.Person.IsValid() && GivePerson(Spectator))
			{
				++Arrived;
				--PeopleStillToCome;
			}
		}
	}

	for (FSpectator& Spectator : Spectators)
	{
		USkeletalMeshComponent* Body = Spectator.Body.Get();

		if (!Body)
		{
			continue;
		}

		// a cheer that has been asked for starts when its turn comes
		if (Spectator.CheerDelay >= 0.0f)
		{
			Spectator.CheerDelay -= DeltaTime;

			if (UAnimSequenceBase* Cheering = Spectator.CheerDelay < 0.0f ? PickFrom(CheerAnimations, CheerAnimation) : nullptr)
			{
				const float Rate = FMath::FRandRange(0.9f, 1.15f);

				// laid over the watching and blended in and out of it, so nobody snaps from one to the other
				if (UAnimInstance* Driver = Body->GetAnimInstance())
				{
					Driver->PlaySlotAnimationAsDynamicMontage(Cheering, FName("DefaultSlot"), 0.25f, 0.4f, Rate);
				}

				Spectator.CheerTimeLeft = Cheering->GetPlayLength() / Rate;
			}
		}

		// and when it is over the spectator is back to watching
		if (Spectator.CheerTimeLeft > 0.0f)
		{
			Spectator.CheerTimeLeft = FMath::Max(0.0f, Spectator.CheerTimeLeft - DeltaTime);
		}
	}
}

int32 AGutCrowd::GetAttendanceFor(int32 FighterName) const
{
	// -GutCrowd=70 on the command line fills the deck to order, to see what a full house costs in frames
	int32 Ordered = 0;

	if (FParse::Value(FCommandLine::Get(), TEXT("GutCrowd="), Ordered))
	{
		return FMath::Max(0, Ordered);
	}

	return FMath::Clamp(BaseAttendance + FMath::RoundToInt32(AttendancePerName * FMath::Max(0, FighterName)), 0, MaxAttendance);
}

UAnimSequenceBase* AGutCrowd::PickFrom(const TArray<TObjectPtr<UAnimSequenceBase>>& Several, UAnimSequenceBase* Single)
{
	return Several.Num() > 0 ? Several[FMath::RandRange(0, Several.Num() - 1)].Get() : Single;
}

int32 AGutCrowd::GetNumCheering() const
{
	int32 Count = 0;

	for (const FSpectator& Spectator : Spectators)
	{
		Count += (Spectator.CheerDelay >= 0.0f || Spectator.CheerTimeLeft > 0.0f) ? 1 : 0;
	}

	return Count;
}

void AGutCrowd::Populate(int32 FighterName)
{
	for (const FSpectator& Spectator : Spectators)
	{
		if (URetargetComponent* Follow = Spectator.Follow.Get())
		{
			Follow->SetControlledMesh(nullptr);

			// through the base class: the component keeps its own way of going to itself
			static_cast<UActorComponent*>(Follow)->DestroyComponent();
		}

		if (AActor* Person = Spectator.Person.Get())
		{
			Person->Destroy();
		}

		if (USkeletalMeshComponent* Body = Spectator.Body.Get())
		{
			Body->DestroyComponent();
		}
	}

	Spectators.Reset();
	PeopleStillToCome = 0;

	// start anywhere on the list of people, so the same few are not always the first to turn up
	const int32 FirstPerson = SpectatorClasses.Num() > 0 ? FMath::RandRange(0, SpectatorClasses.Num() - 1) : 0;

	// nobody comes to a fight that nobody can see
	if (!SpectatorMesh || !FApp::CanEverRender())
	{
		return;
	}

	const int32 Attendance = StandingPlaces.IsEmpty() ? GetAttendanceFor(FighterName) : StandingPlaces.Num();

	for (int32 i = 0; i < Attendance; ++i)
	{
		USkeletalMeshComponent* Body = NewObject<USkeletalMeshComponent>(this);

		Body->SetSkeletalMeshAsset(SpectatorMesh);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetupAttachment(RootComponent);

		FRotator Facing = FRotator::ZeroRotator;
		const FVector Place = StandingPlaces.IsEmpty() ? PickStandingPlace(Facing) : FVector::ZeroVector;

		Body->SetRelativeLocationAndRotation(Place, Facing + FRotator(0.0f, GutCrowdConstants::MeshYawOffset, 0.0f));
		Body->SetRelativeScale3D(FVector(FMath::FRandRange(GutCrowdConstants::MinScale, GutCrowdConstants::MaxScale)));

		Body->RegisterComponent();

		// told where to stand: that place in the level, whatever the crowd's own place is
		if (StandingPlaces.IsValidIndex(i))
		{
			Body->SetWorldLocationAndRotation(StandingPlaces[i].GetLocation(), StandingPlaces[i].Rotator() + FRotator(0.0f, GutCrowdConstants::MeshYawOffset, 0.0f));
		}

		// dull clothes, no two quite alike
		const float Shade = FMath::FRandRange(0.03f, 0.3f);
		const FLinearColor Tint(Shade * FMath::FRandRange(0.8f, 1.3f), Shade * FMath::FRandRange(0.7f, 1.1f), Shade * FMath::FRandRange(0.6f, 1.0f));

		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = Body->CreateAndSetMaterialInstanceDynamic(Slot))
			{
				Material->SetVectorParameterValue(TintParameter, Tint);
			}
		}

		// everybody stands their own way and shifts their weight in their own time
		// dealt round, from a different start each night, so that the ways of standing are shared out evenly
		// and the same three are not standing side by side every time
		static const int32 FirstDeal = FMath::RandRange(0, 1000);

		UAnimSequenceBase* Watching = IdleAnimations.Num() > 0 ? IdleAnimations[(FirstDeal + (i * 3)) % IdleAnimations.Num()].Get() : IdleAnimation.Get();

		if (Watching)
		{
			Body->SetAnimInstanceClass(UGutAnimInstance::StaticClass());

			if (UGutAnimInstance* Driver = Cast<UGutAnimInstance>(Body->GetAnimInstance()))
			{
				Driver->SetIdle(Watching, FMath::FRandRange(0.0f, Watching->GetPlayLength()), FMath::FRandRange(0.85f, 1.15f));
			}
		}

		FSpectator Spectator;
		Spectator.Body = Body;

		// a real person is seen in the placeholder's place as soon as that person has been fetched
		if (SpectatorRetargeter && SpectatorClasses.Num() > 0)
		{
			Spectator.PersonIndex = (FirstPerson + i) % SpectatorClasses.Num();
			Spectator.Tint = Tint;

			Body->SetRelativeScale3D(FVector::OneVector);
			Body->SetVisibility(false, false);
			Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

			++PeopleStillToCome;
		}

		Spectators.Add(Spectator);
	}

	FetchPeople();
}

void AGutCrowd::BeginPlay()
{
	Super::BeginPlay();

	// while the card and the rack are on screen
	FetchPeople();

}

void AGutCrowd::FetchPeople()
{
	// fetch the people in the background, so the fight does not stall on them
	if (PeopleLoading.IsValid() || !SpectatorRetargeter || !FApp::CanEverRender())
	{
		return;
	}

	TArray<FSoftObjectPath> People;

	for (const TSoftClassPtr<AActor>& Person : SpectatorClasses)
	{
		if (!Person.IsNull())
		{
			People.Add(Person.ToSoftObjectPath());
		}
	}

	if (People.Num() > 0)
	{
		PeopleLoading = UAssetManager::GetStreamableManager().RequestAsyncLoad(People);
	}
}

bool AGutCrowd::GivePerson(FSpectator& Spectator)
{
	USkeletalMeshComponent* Body = Spectator.Body.Get();

	if (!Body || Spectator.Person.IsValid())
	{
		return false;
	}

	// not fetched yet: the placeholder waits, unseen
	UClass* PersonClass = SpectatorClasses.IsValidIndex(Spectator.PersonIndex) ? SpectatorClasses[Spectator.PersonIndex].Get() : nullptr;

	if (PersonClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Person = GetWorld()->SpawnActor<AActor>(PersonClass, Body->GetComponentTransform(), Params);
		USkeletalMeshComponent* PersonBody = nullptr;

		if (Person)
		{
			Person->SetActorEnableCollision(false);
			Person->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			for (UActorComponent* Component : Person->GetComponents())
			{
				USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

				if (!Part)
				{
					continue;
				}

				if (Part->GetFName() == PersonBodyName)
				{
					PersonBody = Part;
				}
				else
				{
					// the clothes: each piece in the colour it was made in, a shade darker or lighter than on whoever else
					// on the deck is the same person. What takes no dye, a bought outfit, is left as it is.
					const float Shade = FMath::Lerp(0.7f, 1.2f, FMath::Frac((Spectator.Tint.R * 37.1f) + (Spectator.Tint.G * 91.7f) + (Spectator.Tint.B * 53.3f)));

					for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
					{
						const UMaterialInterface* Worn = Part->GetMaterial(Slot);

						FLinearColor Now;

						if (ClothParameters.IsEmpty() || !Worn || !Worn->GetVectorParameterValue(FHashedMaterialParameterInfo(ClothParameters[0]), Now))
						{
							continue;
						}

						UMaterialInstanceDynamic* Cloth = Part->CreateAndSetMaterialInstanceDynamic(Slot);

						for (const FName& Parameter : ClothParameters)
						{
							if (Cloth && Cloth->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Now))
							{
								Cloth->SetVectorParameterValue(Parameter, FLinearColor(Now.R * Shade, Now.G * Shade, Now.B * Shade, Now.A));
							}
						}
					}
				}
			}
		}

		if (PersonBody)
		{
			// a face in the crowd is too far off to be worth working its muscles, or drawing at its finest:
			// the head rides on the body, everything is drawn one step down, and nobody is posed more often than they are seen to move
			for (UActorComponent* Component : Person->GetComponents())
			{
				if (USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component))
				{
					if (Part != PersonBody)
					{
						Part->SetAnimInstanceClass(nullptr);
						Part->SetLeaderPoseComponent(PersonBody);
					}

					Part->bEnableUpdateRateOptimizations = true;
				}
				else if (ULODSyncComponent* Sync = Cast<ULODSyncComponent>(Component))
				{
					Sync->MinLOD = 1;
				}
			}

			PersonBody->AddTickPrerequisiteComponent(Body);

			URetargetComponent* Follow = NewObject<URetargetComponent>(this);

			// only this one person follows this one placeholder
			Follow->bForceOtherMeshesToFollowControlledMesh = false;
			Follow->RegisterComponent();
			Follow->SetControlledMesh(PersonBody);
			Follow->SetSourcePerformerMesh(Body);
			Follow->SetRetargetAsset(SpectatorRetargeter);

			Spectator.Person = Person;
			Spectator.Follow = Follow;
		}
		else if (Person)
		{
			Person->Destroy();
		}
	}

	return Spectator.Person.IsValid();
}

void AGutCrowd::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PeopleLoading.Reset();

	// the people spawned to be seen go when the crowd goes
	for (const FSpectator& Spectator : Spectators)
	{
		if (AActor* Person = Spectator.Person.Get())
		{
			Person->Destroy();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGutCrowd::Cheer(float Share, float WithinTime)
{
	// heard as well as seen: a few voices for a blow, the lot of them for a death, and a bigger crowd is a louder one
	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && !Spectators.IsEmpty())
	{
		static const FName Roar(TEXT("crowd_roar"));
		static const FName Shout(TEXT("crowd_cheer"));
		static const FName Few(TEXT("crowd_react"));

		const float Size = FMath::Clamp(Spectators.Num() / 40.0f, 0.3f, 1.2f);

		Sounds->Play(Share >= 0.9f ? Roar : (Share >= 0.25f ? Shout : Few), GetActorLocation(), Size * FMath::Lerp(0.7f, 1.0f, FMath::Clamp(Share, 0.0f, 1.0f)));
	}

	// and a blow that gets a cheer gets something shouted after it, by one of them or by several
	if (bFightHeard && Share >= 0.25f)
	{
		ShoutsDue = FMath::Min(ShoutsDue + (Share >= 0.5f ? 2 : 1), 3);
		ShoutIn = FMath::Min(ShoutIn, FMath::FRandRange(0.2f, 0.6f));
	}

	for (FSpectator& Spectator : Spectators)
	{
		// one cheer at a time
		if (Spectator.CheerDelay >= 0.0f || Spectator.CheerTimeLeft > 0.0f)
		{
			continue;
		}

		if (FMath::FRand() < Share)
		{
			Spectator.CheerDelay = FMath::FRandRange(0.0f, FMath::Max(WithinTime, 0.0f));
		}
	}
}

void AGutCrowd::Shout()
{
	UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this);

	if (!Sounds || Spectators.IsEmpty())
	{
		return;
	}

	// whoever it is, it comes from where they stand, at the height of a head
	const FSpectator& Who = Spectators[FMath::RandRange(0, Spectators.Num() - 1)];

	if (const USkeletalMeshComponent* Body = Who.Body.Get())
	{
		Sounds->Play(FName("crowd_shout"), Body->GetComponentLocation() + FVector(0.0f, 0.0f, 160.0f), FMath::FRandRange(0.6f, 1.0f));
	}
}

FVector AGutCrowd::PickStandingPlace(FRotator& OutFacing) const
{
	const float HalfX = FloorSize.X * 0.5f;
	const float HalfY = FloorSize.Y * 0.5f;

	// somewhere along the edge of the cage, by length of side, and some way back from it
	const float Along = FMath::FRandRange(0.0f, (FloorSize.X + FloorSize.Y) * 2.0f);
	const float Back = EdgeDistance + (FMath::Square(FMath::FRand()) * Depth);

	FVector Place;
	FVector Inward;

	if (Along < FloorSize.X)
	{
		Place = FVector(Along - HalfX, HalfY + Back, DeckHeight);
		Inward = FVector(0.0f, -1.0f, 0.0f);
	}
	else if (Along < FloorSize.X * 2.0f)
	{
		Place = FVector(Along - FloorSize.X - HalfX, -HalfY - Back, DeckHeight);
		Inward = FVector(0.0f, 1.0f, 0.0f);
	}
	else if (Along < (FloorSize.X * 2.0f) + FloorSize.Y)
	{
		Place = FVector(HalfX + Back, Along - (FloorSize.X * 2.0f) - HalfY, DeckHeight);
		Inward = FVector(-1.0f, 0.0f, 0.0f);
	}
	else
	{
		Place = FVector(-HalfX - Back, Along - (FloorSize.X * 2.0f) - FloorSize.Y - HalfY, DeckHeight);
		Inward = FVector(1.0f, 0.0f, 0.0f);
	}

	// looking down into the cage, more or less
	OutFacing = Inward.Rotation() + FRotator(0.0f, FMath::FRandRange(-25.0f, 25.0f), 0.0f);

	return Place;
}
