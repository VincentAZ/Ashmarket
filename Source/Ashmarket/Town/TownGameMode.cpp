// Ashmarket: the town outside the cage.

#include "TownGameMode.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GutFighter.h"
#include "EngineUtils.h"
#include "GutArmorData.h"
#include "GutCrowd.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutSound.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TownHUD.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTown, Log, All);

namespace TownConstants
{
	/** How long the words of a shut door stay up */
	const float NoticeTime = 3.5f;

	/** The colours of a built person's clothes, as their materials name them */
	const FName ClothParameters[] = { FName("diffuse_color_1"), FName("diffuse_color_2") };

	/** The parts of a built person that are not clothes */
	const FName BodyPart("Body");
	const FName FacePart("Face");
}

ATownGameMode::ATownGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// the player in the street is the player of the cage: the same fighter, with the same hands on the same keys
	static ConstructorHelpers::FClassFinder<APawn> Player(TEXT("/Game/Gut/Blueprints/BP_GutPlayer"));
	static ConstructorHelpers::FClassFinder<APlayerController> Hands(TEXT("/Game/Gut/Blueprints/BP_GutPlayerController"));

	if (Player.Succeeded())
	{
		DefaultPawnClass = Player.Class;
	}

	if (Hands.Succeeded())
	{
		PlayerControllerClass = Hands.Class;
	}

	HUDClass = ATownHUD::StaticClass();

	Place = TSoftObjectPtr<UTownPlaceData>(FSoftObjectPath(TEXT("/Game/Town/DA_Town_PoolStreet.DA_Town_PoolStreet")));
	PersonClass = TSoftClassPtr<AGutFighter>(FSoftObjectPath(TEXT("/Game/Gut/Blueprints/BP_GutDummy.BP_GutDummy_C")));
	FirstLadder = TSoftObjectPtr<UGutLadderData>(FSoftObjectPath(TEXT("/Game/Gut/Ladder/DA_Gut_MeatLadder.DA_Gut_MeatLadder")));
}

void ATownGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// each place has its list beside its level: Lvl_BlockSquare is described by DA_Town_BlockSquare
	FString Short = FPackageName::GetShortName(UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));

	if (Short.RemoveFromStart(TEXT("Lvl_")))
	{
		const FString Listed = FString::Printf(TEXT("/Game/Town/DA_Town_%s"), *Short);

		if (FPackageName::DoesPackageExist(Listed))
		{
			Place = TSoftObjectPtr<UTownPlaceData>(FSoftObjectPath(FString::Printf(TEXT("%s.DA_Town_%s"), *Listed, *Short)));
		}
	}

	PlaceData = Place.LoadSynchronous();
	LadderData = FirstLadder.LoadSynchronous();

	if (!PlaceData)
	{
		UE_LOG(LogTown, Warning, TEXT("The street has no list of who and what is in it (%s): it is empty and has no edges"), *Place.ToString());
	}
}

UGutGameInstance* ATownGameMode::GetGutGameInstance() const
{
	return Cast<UGutGameInstance>(GetGameInstance());
}

AGutFighter* ATownGameMode::GetWalker() const
{
	return Cast<AGutFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ATownGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// -GutFrameRate on the command line writes the frame rate to the log every five seconds of real time, as it does in the cage
	static const bool bLogFrameRate = FParse::Param(FCommandLine::Get(), TEXT("GutFrameRate"));

	if (bLogFrameRate)
	{
		static double Since = FPlatformTime::Seconds();
		static int32 Frames = 0;
		static double Slowest = 0.0;
		static double LastFrame = FPlatformTime::Seconds();

		const double Now = FPlatformTime::Seconds();

		Slowest = FMath::Max(Slowest, Now - LastFrame);
		LastFrame = Now;
		++Frames;

		// -GutProfileAt=40 has the engine write where one frame's time on the graphics card went, that many seconds in
		static float ProfileAt = 0.0f;
		static bool bProfileRead = false;

		if (!bProfileRead)
		{
			bProfileRead = true;
			FParse::Value(FCommandLine::Get(), TEXT("GutProfileAt="), ProfileAt);
		}

		if (ProfileAt > 0.0f && GetWorld()->GetRealTimeSeconds() >= ProfileAt)
		{
			ProfileAt = 0.0f;
			GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
		}

		if (Now - Since >= 5.0)
		{
			UE_LOG(LogTown, Log, TEXT("Frame rate: %.0f a second, slowest frame %.0f ms"), Frames / (Now - Since), Slowest * 1000.0);

			Since = Now;
			Frames = 0;
			Slowest = 0.0;
		}
	}

	if (!bSettled)
	{
		Settle();
		return;
	}

	SettledFor += DeltaTime;

	UpdateCalls(DeltaTime);
	UpdateLed(DeltaTime);
	UpdateBeats(DeltaTime);

	if (AGutFighter* Walker = GetWalker())
	{
		KeepOnGround(Walker);
		FindReach(Walker);

		// an open way to another place is gone through by walking up to it
		if (PlaceData && SettledFor > 1.0f && !IsTalking() && !bShopping && !bStoppedMenu)
		{
			for (const FTownDoor& Door : PlaceData->Doors)
			{
				if (Door.bWalkedThrough && Door.Deed == ETownDeed::Go && Door.IsEntered(Walker->GetActorLocation(), WayReach))
				{
					Do(Door.Deed, &Door);
					return;
				}
			}
		}

		// while they talk the camera comes round to look past the player at whoever is speaking, wherever it was before,
		// and stays there while the player is at their counter
		const int32 Faced = IsTalking() ? TalkingTo : (bShopping ? ShopKeeper : INDEX_NONE);

		if (AController* Eyes = Walker->GetController(); Eyes && People.IsValidIndex(Faced) && People[Faced])
		{
			const FVector Toward = People[Faced]->GetActorLocation() - Walker->GetActorLocation();

			if (!Toward.IsNearlyZero())
			{
				const FRotator Wanted(TalkCameraPitch, Toward.Rotation().Yaw + TalkCameraYaw, 0.0f);

				Eyes->SetControlRotation(FMath::RInterpTo(Eyes->GetControlRotation(), Wanted, DeltaTime, TalkCameraTurnSpeed));
			}
		}
	}
}

void ATownGameMode::Settle()
{
	AGutFighter* Walker = GetWalker();

	if (!Walker || !Walker->HasActorBegunPlay())
	{
		return;
	}

	if (!bWalkerSettled)
	{
		// nobody carries a weapon or wears a collar in the street, and nobody is on their guard
		Walker->EquipWeapon(nullptr);
		Walker->SetCollared(false);
		Walker->SetAtEase(true);

		// somebody who has come out of the cage is stood where they went into it from, and somebody who has walked here from
		// another place where that way comes in. Anybody else stands where the level starts its player.
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			const FName Here(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));

			TOptional<FTransform> Stood;

			if (GameInstance->TownLevel == Here && GameInstance->TownStood.IsSet())
			{
				Stood = GameInstance->TownStood;
			}
			else if (const FTownArrival* Way = PlaceData ? PlaceData->Arrivals.FindByPredicate([GameInstance](const FTownArrival& Arrival) { return Arrival.Name == GameInstance->TownArrive; }) : nullptr)
			{
				Stood = FTransform(FRotator(0.0f, Way->Facing, 0.0f), Way->Place + FVector(0.0f, 0.0f, Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f));
			}

			if (Stood.IsSet())
			{
				Walker->SetActorLocationAndRotation(Stood->GetLocation(), FRotator(0.0f, Stood->Rotator().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

				if (AController* Eyes = Walker->GetController())
				{
					Eyes->SetControlRotation(FRotator(Eyes->GetControlRotation().Pitch, Stood->Rotator().Yaw, 0.0f));
				}
			}

			GameInstance->TownLevel = Here;
			GameInstance->TownStood.Reset();
			GameInstance->TownArrive = NAME_None;
		}

		bWalkerSettled = true;
	}

	UClass* Kind = PersonClass.LoadSynchronous();

	// the people are stood one to a frame, from a few frames after the level opens. (All of a place's people made in the
	// frame it opens in, ten of them in Block Square, stopped the renderer dead two frames later, every time.)
	if (PlaceData && Kind && People.Num() < PlaceData->People.Num())
	{
		if (++FramesSettling <= 4)
		{
			return;
		}

		const float HalfHeight = Kind->GetDefaultObject<AGutFighter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		// -TownPeople=3 stands only the first so many, and -TownNoCollars leaves the collars off: for finding out what a place costs
		int32 Most = MAX_int32;
		FParse::Value(FCommandLine::Get(), TEXT("TownPeople="), Most);

		const bool bCollars = !FParse::Param(FCommandLine::Get(), TEXT("TownNoCollars"));

		{
			const FTownPerson& Person = PlaceData->People[People.Num()];

			if (People.Num() >= Most)
			{
				People.Add(nullptr);
				return;
			}

			FVector Feet = Person.Place;

			// somebody up on something stands on it: whatever is under them there
			if (Person.bStandsOnWhatIsThere)
			{
				FCollisionQueryParams Looking(FName("TownStanding"), true, Walker);
				FHitResult Under;

				if (GetWorld()->LineTraceSingleByChannel(Under, Feet + FVector(0.0f, 0.0f, 230.0f), Feet - FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, Looking))
				{
					UE_LOG(LogTown, Log, TEXT("Street: under %s is %s, %.0f cm up"), *Person.Name, *GetNameSafe(Under.GetActor()), Under.Location.Z - Feet.Z);

					Feet.Z = Under.Location.Z;
				}
			}

			// where they stand is where their feet are
			const FTransform Where(FRotator(0.0f, Person.Facing, 0.0f), Feet + FVector(0.0f, 0.0f, HalfHeight));

			AGutFighter* Body = GetWorld()->SpawnActorDeferred<AGutFighter>(Kind, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (!Body)
			{
				People.Add(nullptr);
				return;
			}

			// the body they are seen as is named before they begin, so that they are never anybody else first
			if (!Person.Body.IsNull())
			{
				Body->SetBodyClass(Person.Body);
			}

			if (Person.Idle)
			{
				Body->SetAtEaseIdle(Person.Idle);
			}

			UGameplayStatics::FinishSpawningActor(Body, Where);

			Body->EquipWeapon(nullptr);
			Body->SetCollared(Person.bCollared && bCollars);
			Body->SetAtEase(true);

			// somebody stood up on something stays where they were stood: a truck's deck is not ground to whatever moves a
			// fighter about, which would walk them off the end of it
			if (Person.bStandsOnWhatIsThere)
			{
				Body->GetCharacterMovement()->DisableMovement();
			}

			// somebody with a round to walk is walked along it, from its first stop to its second
			if (!Person.Beat.IsEmpty())
			{
				Body->GetCharacterMovement()->DisableMovement();

				FTownBeat Round;
				Round.Person = People.Num();
				Round.Stop = Person.Beat.Num() > 1 ? 1 : 0;
				Round.Wait = Person.Beat[0].Wait;

				Beats.Add(Round);
			}

			// their clothes are the clothes everybody has, in their own colour
			if (const AActor* Seen = Body->GetBodyActor())
			{
				for (UActorComponent* Component : Seen->GetComponents())
				{
					USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

					if (!Part || Part->GetFName() == TownConstants::BodyPart || Part->GetFName() == TownConstants::FacePart)
					{
						continue;
					}

					for (int32 i = 0; i < Part->GetNumMaterials(); ++i)
					{
						if (UMaterialInstanceDynamic* Cloth = Part->CreateAndSetMaterialInstanceDynamic(i))
						{
							for (const FName& Parameter : TownConstants::ClothParameters)
							{
								Cloth->SetVectorParameterValue(Parameter, Person.Cloth);
							}
						}
					}
				}
			}

			People.Add(Body);

			UE_LOG(LogTown, Log, TEXT("Street: %s stands at %s"), *Person.Name, *Feet.ToCompactString());
		}

		// the next of them next frame
		return;
	}

	if (const UGutGameInstance* GameInstance = GetGutGameInstance())
	{
		GameInstance->ApplyLoudness(this);
	}

	bSettled = true;

	UE_LOG(LogTown, Log, TEXT("Street: %s, %d people, %d doors"), PlaceData ? *PlaceData->Name : TEXT("nowhere"), People.Num(), PlaceData ? PlaceData->Doors.Num() : 0);
}

void ATownGameMode::KeepOnGround(AGutFighter* Walker) const
{
	if (!PlaceData || PlaceData->Ground.IsEmpty())
	{
		return;
	}

	const FVector Here = Walker->GetActorLocation();
	const FVector2D Flat(Here.X, Here.Y);

	FVector2D Kept = Flat;

	// round whatever stands there: clear of each thing by the nearest way, and again for a corner where two of them meet
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FTownBlock& Block : PlaceData->Blocks)
		{
			Kept = Block.KeepClear(Kept, WalkerRadius);
		}
	}

	// and on the ground: off every patch of it, back to the nearest edge
	FVector2D Nearest = Kept;
	float NearestDistance = TNumericLimits<float>::Max();

	for (const FBox2D& Patch : PlaceData->Ground)
	{
		if (Patch.IsInside(Kept))
		{
			Nearest = Kept;
			break;
		}

		const FVector2D Closest = Patch.GetClosestPointTo(Kept);
		const float Distance = FVector2D::DistSquared(Closest, Kept);

		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Closest;
		}
	}

	if (!Nearest.Equals(Flat, 0.01f))
	{
		Walker->SetActorLocation(FVector(Nearest.X, Nearest.Y, Here.Z), false, nullptr, ETeleportType::None);
	}
}

void ATownGameMode::FindReach(const AGutFighter* Walker)
{
	PersonInReach = INDEX_NONE;
	DoorInReach = INDEX_NONE;

	if (!PlaceData)
	{
		return;
	}

	const FVector Here = Walker->GetActorLocation();

	float Nearest = Reach;

	for (int32 i = 0; i < PlaceData->People.Num(); ++i)
	{
		const float Distance = FVector::Dist2D(Here, GetPersonPlace(i));

		// somebody with nothing to say is not offered to be talked to
		if (Distance < Nearest && People.IsValidIndex(i) && People[i] && !PlaceData->People[i].Lines.IsEmpty())
		{
			Nearest = Distance;
			PersonInReach = i;
		}
	}

	// somebody in reach comes before a door in reach: the door will still be there
	if (PersonInReach != INDEX_NONE)
	{
		return;
	}

	for (int32 i = 0; i < PlaceData->Doors.Num(); ++i)
	{
		const float Distance = FVector::Dist2D(Here, PlaceData->Doors[i].Place);

		// an open way is walked through, not used
		if (Distance < Nearest && !PlaceData->Doors[i].bWalkedThrough)
		{
			Nearest = Distance;
			DoorInReach = i;
		}
	}
}

void ATownGameMode::Interact()
{
	if (!bSettled || bStoppedMenu || !PlaceData)
	{
		return;
	}

	if (bShopping)
	{
		// at a counter the same key takes what the player is on
		HandleMenuInput(EGutMenuInput::Confirm);
	}
	else if (IsTalking())
	{
		AdvanceTalk();
	}
	else if (PlaceData->People.IsValidIndex(PersonInReach))
	{
		BeginTalk(PersonInReach);
	}
	else if (PlaceData->Doors.IsValidIndex(DoorInReach))
	{
		const FTownDoor& Door = PlaceData->Doors[DoorInReach];

		if (Door.Deed == ETownDeed::None)
		{
			Notice = Door.Shut;
			NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::NoticeTime;
		}
		else
		{
			Do(Door.Deed, &Door);
		}
	}
}

void ATownGameMode::BeginTalk(int32 Person)
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person) || PlaceData->People[Person].Lines.IsEmpty())
	{
		return;
	}

	TalkingTo = Person;
	TalkLine = 0;
	TalkChoice = 0;
	Dealt.Reset();

	// whoever is talking stands still to do it, and looks at who they are talking to
	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	if (AGutFighter* Walker = GetWalker())
	{
		const FVector Toward = GetPersonPlace(Person) - Walker->GetActorLocation();

		if (!Toward.IsNearlyZero())
		{
			Walker->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));
		}
	}

	UE_LOG(LogTown, Log, TEXT("Street: talking to %s"), *PlaceData->People[Person].Name);
}

void ATownGameMode::EndTalk()
{
	TalkingTo = INDEX_NONE;
	TalkLine = 0;
	TalkChoice = 0;

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->ResetIgnoreMoveInput();
	}
}

void ATownGameMode::AdvanceTalk()
{
	if (!IsTalking())
	{
		return;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	if (!Lines.IsValidIndex(TalkLine))
	{
		EndTalk();
		return;
	}

	const FTownLine& Line = Lines[TalkLine];

	int32 Next = Line.Next;

	if (Line.Answers.IsValidIndex(TalkChoice))
	{
		const FTownAnswer& Answer = Line.Answers[TalkChoice];

		if (Answer.Deed == ETownDeed::Borrow || Answer.Deed == ETownDeed::Repay)
		{
			// done where they stand, and the talk goes on with what it came to
			Do(Answer.Deed);
		}
		else if (Answer.Deed == ETownDeed::Armor)
		{
			// to their counter: whoever was being talked to keeps it
			const int32 Keeper = TalkingTo;

			EndTalk();
			OpenShop(Keeper);
			return;
		}
		else if (Answer.Deed != ETownDeed::None)
		{
			EndTalk();
			Do(Answer.Deed);
			return;
		}

		Next = Answer.Next;
	}

	if (Lines.IsValidIndex(Next))
	{
		TalkLine = Next;
		TalkChoice = 0;
	}
	else
	{
		EndTalk();
	}
}

void ATownGameMode::HandleMenuInput(EGutMenuInput Input)
{
	if (bStoppedMenu)
	{
		if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			StoppedChoice = (StoppedChoice + (Input == EGutMenuInput::Up ? TownStoppedLines::Count - 1 : 1)) % TownStoppedLines::Count;
			return;
		}

		if (Input != EGutMenuInput::Confirm)
		{
			return;
		}

		UGutGameInstance* GameInstance = GetGutGameInstance();

		switch (StoppedChoice)
		{
		case TownStoppedLines::GoOn:
			SetStopped(false);
			break;

		case TownStoppedLines::Sound:

			// each press a third quieter, and round again, as in the cage
			if (GameInstance)
			{
				GameInstance->Loudness = (GameInstance->Loudness + 3) % 4;
				GameInstance->KeepLoudness();
				GameInstance->ApplyLoudness(this);
			}
			break;

		default:

			// nothing is lost: a run is written down in the cage, and nothing is kept of a walk in the street
			SetStopped(false);

			UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
			break;
		}

		return;
	}

	if (bShopping)
	{
		// the pieces, and under them the line that leaves
		const int32 Pieces = GetShopLines().Num();

		if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			ShopChoice = (ShopChoice + (Input == EGutMenuInput::Up ? Pieces : 1)) % (Pieces + 1);
		}
		else if (Input == EGutMenuInput::Confirm)
		{
			if (ShopChoice >= Pieces)
			{
				CloseShop();
			}
			else
			{
				BuyAtShop(ShopChoice);
			}
		}

		return;
	}

	if (!IsTalking())
	{
		return;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;
	const int32 Answers = Lines.IsValidIndex(TalkLine) ? Lines[TalkLine].Answers.Num() : 0;

	if (Input == EGutMenuInput::Confirm)
	{
		AdvanceTalk();
	}
	else if (Answers > 1 && (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down))
	{
		TalkChoice = (TalkChoice + (Input == EGutMenuInput::Up ? Answers - 1 : 1)) % Answers;
	}
}

void ATownGameMode::Do(ETownDeed Deed, const FTownDoor* Door)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || Deed == ETownDeed::None)
	{
		return;
	}

	if (Deed == ETownDeed::Borrow || Deed == ETownDeed::Repay)
	{
		Lend(Deed == ETownDeed::Borrow);
		return;
	}

	if (Deed == ETownDeed::Armor)
	{
		OpenShop(INDEX_NONE);
		return;
	}

	if (Deed == ETownDeed::Go)
	{
		if (!Door || Door->To.IsNone() || !FPackageName::DoesPackageExist(Door->To.ToString()))
		{
			UE_LOG(LogTown, Warning, TEXT("Street: a way that leads nowhere (%s)"), Door ? *Door->To.ToString() : TEXT("no door"));
			return;
		}

		// on foot to the next place, arriving by the way that leads from here
		GameInstance->TownArrive = Door->Arrive;
		GameInstance->TownStood.Reset();

		UE_LOG(LogTown, Log, TEXT("Street: on to %s by %s"), *Door->To.ToString(), *Door->Arrive.ToString());

		UGameplayStatics::OpenLevel(this, Door->To, true);
		return;
	}

	// the cage's way out leads back to where the player is standing now
	if (const AGutFighter* Walker = GetWalker())
	{
		GameInstance->TownStood = FTransform(Walker->GetActorRotation(), Walker->GetActorLocation());
	}

	// the cage is opened as whatever the player is going into it as, and remembers that they came from the street
	GameInstance->bAtDoor = false;
	GameInstance->bCameByDoor = true;
	GameInstance->bSandbox = Deed == ETownDeed::Spar;
	GameInstance->bStable = Deed == ETownDeed::Own || Deed == ETownDeed::Buy;
	GameInstance->bStableAtBlock = Deed == ETownDeed::Buy;
	GameInstance->bWindow = Deed == ETownDeed::Bet;

	static const TCHAR* Ways[] = { TEXT("nowhere"), TEXT("fight"), TEXT("fight, from the start"), TEXT("spar"), TEXT("bet"), TEXT("own"), TEXT("another place"), TEXT("buy at the Block") };

	UE_LOG(LogTown, Log, TEXT("Street: in to %s"), Ways[FMath::Min<int32>(static_cast<uint8>(Deed), UE_ARRAY_COUNT(Ways) - 1)]);

	UGameplayStatics::OpenLevel(this, CageLevel, true, Deed == ETownDeed::FightAfresh ? TEXT("NewRun=1") : TEXT(""));
}

void ATownGameMode::Lend(bool bBorrow)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return;
	}

	// her book is the window's: what a gambler holds is what she lends against, and what the window pays out is what she is paid from
	FGutWindowState& Page = GameInstance->Window;

	if (!Page.bStarted)
	{
		Dealt = TEXT("I have no page for you. Put water on a fight at Chalk's window first, and then I will know your face.");
	}
	else if (bBorrow)
	{
		if (Page.Debt + LoanOwed > LoanLimit)
		{
			Dealt = FString::Printf(TEXT("No. %.0f litres is as much as I let anybody owe me, and you know where debts are paid here."), LoanLimit);
		}
		else
		{
			Page.Water += LoanGiven;
			Page.Debt += LoanOwed;

			Dealt = FString::Printf(TEXT("%.0f litres counted out. %.0f written down."), LoanGiven, LoanOwed);

			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(FName("cash"), FVector::ZeroVector);
			}
		}
	}
	else
	{
		const float Paid = FMath::Min(Page.Water, Page.Debt);

		if (Page.Debt <= 0.0f)
		{
			Dealt = TEXT("Your page is clean. I take nothing I am not owed.");
		}
		else if (Paid <= 0.0f)
		{
			Dealt = TEXT("With what? You are holding nothing.");
		}
		else
		{
			Page.Water -= Paid;
			Page.Debt -= Paid;

			Dealt = FString::Printf(TEXT("%.0f litres, struck off."), Paid);

			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(FName("cash"), FVector::ZeroVector);
			}
		}
	}

	UE_LOG(LogTown, Log, TEXT("Street: the lender: %s Holding %.0f L, owing %.0f L"), *Dealt, Page.Water, Page.Debt);

	GameInstance->SaveRun();
}

FString ATownGameMode::Told(const FString& Words) const
{
	if (!Words.Contains(TEXT("{")))
	{
		return Words;
	}

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const FGutWindowState Page = GameInstance ? GameInstance->Window : FGutWindowState();

	FString Result = Words;

	Result.ReplaceInline(TEXT("{held}"), *FString::Printf(TEXT("%.0f"), Page.Water));
	Result.ReplaceInline(TEXT("{owed}"), *FString::Printf(TEXT("%.0f"), Page.Debt));
	Result.ReplaceInline(TEXT("{dealt}"), *Dealt);

	return Result.TrimStartAndEnd();
}

void ATownGameMode::UpdateCalls(float DeltaTime)
{
	if (!PlaceData || PlaceData->Calls.IsEmpty())
	{
		return;
	}

	CallTime += DeltaTime;

	if (!PlaceData->Calls.IsValidIndex(CallIndex) || CallTime >= PlaceData->Calls[CallIndex].Seconds)
	{
		CallIndex = (CallIndex + 1) % PlaceData->Calls.Num();
		CallTime = 0.0f;

		BeginCall(PlaceData->Calls[CallIndex]);
	}
}

void ATownGameMode::SkipToCall(int32 Index)
{
	if (PlaceData && PlaceData->Calls.IsValidIndex(Index))
	{
		CallIndex = (Index + PlaceData->Calls.Num() - 1) % PlaceData->Calls.Num();
		CallTime = PlaceData->Calls[CallIndex].Seconds;
	}
}

void ATownGameMode::BeginCall(const FTownCall& Call)
{
	// a lot knocked down gets something out of whoever is standing there
	if (Call.bStirsTheCrowd)
	{
		for (TActorIterator<AGutCrowd> It(GetWorld()); It; ++It)
		{
			It->Cheer(0.12f, 1.2f);
		}
	}

	// and is walked off, down the way it came up
	if (Call.Sells > 0)
	{
		SellLot(Call.Sells);
	}

	if (Call.bBringsUp)
	{
		BringLotsUp();
	}
}

FVector ATownGameMode::GetKeptPlace(int32 Back) const
{
	const TArray<FVector>& Way = PlaceData->LotWay;

	// the file runs back from where the way begins, away from where it goes
	return Way[0] + ((Way[0] - Way[1]).GetSafeNormal2D() * (LotFileGap * Back));
}

void ATownGameMode::SellLot(int32 Lot)
{
	if (!PlaceData || PlaceData->LotWay.Num() < 2)
	{
		return;
	}

	const int32 Who = PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; });

	if (!People.IsValidIndex(Who) || !People[Who] || KeptLots.Contains(Who))
	{
		return;
	}

	const FTownPerson& Person = PlaceData->People[Who];
	const TArray<FVector>& Way = PlaceData->LotWay;

	FTownLed Walk;
	Walk.Person = Who;
	Walk.Wait = LotSoldWait;

	// back off its mark, along behind the others to the head of the deck, down the way and to the end of the file
	Walk.Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));

	for (int32 i = Way.Num() - 1; i >= 0; --i)
	{
		Walk.Way.Add(Way[i]);
	}

	// the first to be sold goes to the far end of the file and the last stops at its head, so nobody walks through anybody
	const int32 Lots = PlaceData->People.FilterByPredicate([](const FTownPerson& Other) { return Other.Lot > 0; }).Num();
	const int32 Back = Lots - 1 - KeptLots.Num();

	if (Back > 0)
	{
		Walk.Way.Add(GetKeptPlace(Back));
	}

	// and waits looking the way it will come back
	Walk.Facing = (Way[1] - Way[0]).Rotation().Yaw;

	Led.RemoveAll([Who](const FTownLed& One) { return One.Person == Who; });
	Led.Add(MoveTemp(Walk));
	KeptLots.Add(Who);

	UE_LOG(LogTown, Log, TEXT("Sale: %s is knocked down and walked off, %d kept"), *Person.Name, KeptLots.Num());
}

void ATownGameMode::BringLotsUp()
{
	if (!PlaceData || PlaceData->LotWay.Num() < 2)
	{
		return;
	}

	const TArray<FVector>& Way = PlaceData->LotWay;

	// the head of the file first, which is whoever was walked off last
	for (int32 Order = 0; Order < KeptLots.Num(); ++Order)
	{
		const int32 Who = KeptLots[KeptLots.Num() - 1 - Order];

		if (!People.IsValidIndex(Who) || !People[Who])
		{
			continue;
		}

		const FTownPerson& Person = PlaceData->People[Who];
		const FVector Here = People[Who]->GetActorLocation();

		// from wherever on the way it has got to: somebody still going down it turns round where they are
		int32 Nearest = 0;

		for (int32 i = 1; i < Way.Num(); ++i)
		{
			if (FVector::DistSquared2D(Way[i], Here) < FVector::DistSquared2D(Way[Nearest], Here))
			{
				Nearest = i;
			}
		}

		FTownLed Walk;
		Walk.Person = Who;
		Walk.Wait = LotFileTime * Order;
		Walk.Facing = Person.Facing;

		for (int32 i = Nearest; i < Way.Num(); ++i)
		{
			Walk.Way.Add(Way[i]);
		}

		// along behind the marks to its own, and onto it
		Walk.Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));
		Walk.Way.Add(Person.Place);

		Led.RemoveAll([Who](const FTownLed& One) { return One.Person == Who; });
		Led.Add(MoveTemp(Walk));
	}

	UE_LOG(LogTown, Log, TEXT("Sale: %d lots are brought up"), KeptLots.Num());

	KeptLots.Reset();
}

void ATownGameMode::UpdateLed(float DeltaTime)
{
	if (Led.IsEmpty())
	{
		return;
	}

	// what is under each of them is the deck, the ramp or the ground, and never one of the others
	FCollisionQueryParams Looking(FName("TownLed"), true, GetWalker());

	for (const AGutFighter* Body : People)
	{
		if (Body)
		{
			Looking.AddIgnoredActor(Body);
			Looking.AddIgnoredActor(Body->GetBodyActor());
		}
	}

	for (int32 i = Led.Num() - 1; i >= 0; --i)
	{
		FTownLed& One = Led[i];
		AGutFighter* Body = People.IsValidIndex(One.Person) ? People[One.Person].Get() : nullptr;

		if (!Body)
		{
			Led.RemoveAt(i);
			continue;
		}

		if (One.Wait > 0.0f)
		{
			One.Wait -= DeltaTime;
			continue;
		}

		const FRotator Facing = Body->GetActorRotation();

		// there: it turns to face the way it is to stand, and is done
		if (One.Way.IsEmpty())
		{
			Body->SetLedVelocity(FVector::ZeroVector);
			Body->SetActorRotation(FMath::RInterpConstantTo(Facing, FRotator(0.0f, One.Facing, 0.0f), DeltaTime, LotTurnSpeed));

			if (FMath::Abs(FRotator::NormalizeAxis(Body->GetActorRotation().Yaw - One.Facing)) < 1.0f)
			{
				Led.RemoveAt(i);
			}

			continue;
		}

		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float OwnPace = Body->GetCharacterMovement()->MaxWalkSpeed;
		const float Pace = (OwnPace > 1.0f ? OwnPace : 150.0f) * LotPace;

		FVector Feet = Body->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
		FVector2D Flat(Feet.X, Feet.Y);
		FVector2D Going = FVector2D::ZeroVector;

		// as far as a frame's walking goes, through as many of the way's places as that passes
		float Left = Pace * DeltaTime;

		while (Left > 0.0f && !One.Way.IsEmpty())
		{
			const FVector2D To(One.Way[0].X, One.Way[0].Y);
			const float Distance = FVector2D::Distance(Flat, To);

			if (Distance > UE_KINDA_SMALL_NUMBER)
			{
				Going = (To - Flat) / Distance;
			}

			if (Distance <= Left)
			{
				Left -= Distance;
				Flat = To;
				One.Way.RemoveAt(0);
			}
			else
			{
				Flat += Going * Left;
				Left = 0.0f;
			}
		}

		// on whatever is under them there: the deck, the ramp as it falls, the ground. A foot is longer than the gap between
		// two of a ramp's boards, so it is the highest of what is under the middle of them, a little ahead and a little behind.
		float Highest = -TNumericLimits<float>::Max();

		for (float Along : { 0.0f, 14.0f, -14.0f })
		{
			const FVector2D At = Flat + (Going * Along);

			FHitResult Under;

			if (GetWorld()->LineTraceSingleByChannel(Under, FVector(At.X, At.Y, Feet.Z + 80.0f), FVector(At.X, At.Y, Feet.Z - 120.0f), ECC_Visibility, Looking))
			{
				Highest = FMath::Max(Highest, Under.Location.Z);
			}
		}

		if (Highest > -TNumericLimits<float>::Max())
		{
			Feet.Z = FMath::FInterpTo(Feet.Z, Highest, DeltaTime, 14.0f);
		}

		FRotator Turned = Facing;

		if (!Going.IsNearlyZero())
		{
			Turned = FMath::RInterpConstantTo(Facing, FRotator(0.0f, FVector(Going.X, Going.Y, 0.0f).Rotation().Yaw, 0.0f), DeltaTime, LotTurnSpeed);
		}

		Body->SetActorLocationAndRotation(FVector(Flat.X, Flat.Y, Feet.Z + HalfHeight), Turned, false, nullptr, ETeleportType::None);
		Body->SetLedVelocity(FVector(Going.X, Going.Y, 0.0f) * Pace);
	}
}

FVector ATownGameMode::GetPersonPlace(int32 Person) const
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person))
	{
		return FVector::ZeroVector;
	}

	// somebody who walks a round is wherever it has taken them
	if (!PlaceData->People[Person].Beat.IsEmpty() && People.IsValidIndex(Person) && People[Person])
	{
		return People[Person]->GetActorLocation();
	}

	return PlaceData->People[Person].Place;
}

void ATownGameMode::UpdateBeats(float DeltaTime)
{
	if (Beats.IsEmpty() || !PlaceData)
	{
		return;
	}

	const AGutFighter* Walker = GetWalker();

	for (FTownBeat& One : Beats)
	{
		AGutFighter* Body = People.IsValidIndex(One.Person) ? People[One.Person].Get() : nullptr;

		if (!Body || !PlaceData->People.IsValidIndex(One.Person) || !PlaceData->People[One.Person].Beat.IsValidIndex(One.Stop))
		{
			continue;
		}

		const FTownPerson& Person = PlaceData->People[One.Person];
		const int32 Stops = Person.Beat.Num();

		const FRotator Facing = Body->GetActorRotation();
		const FVector Here = Body->GetActorLocation();

		const auto TurnTo = [this, Body, &Facing, DeltaTime](float Yaw)
		{
			Body->SetActorRotation(FMath::RInterpConstantTo(Facing, FRotator(0.0f, Yaw, 0.0f), DeltaTime, LotTurnSpeed));
		};

		// spoken to, or kept at their counter: they stand and look at whoever it is
		if (Walker && (TalkingTo == One.Person || (bShopping && ShopKeeper == One.Person)))
		{
			Body->SetLedVelocity(FVector::ZeroVector);

			if (const FVector Toward = Walker->GetActorLocation() - Here; !Toward.IsNearlyZero())
			{
				TurnTo(Toward.Rotation().Yaw);
			}

			continue;
		}

		// standing at a stop, looking at what they stopped for
		if (One.Wait > 0.0f)
		{
			One.Wait -= DeltaTime;

			Body->SetLedVelocity(FVector::ZeroVector);

			if (const float Look = Person.Beat[(One.Stop + Stops - 1) % Stops].Facing; Look >= 0.0f)
			{
				TurnTo(Look);
			}

			continue;
		}

		const FTownBeatStop& Stop = Person.Beat[One.Stop];

		FVector2D Flat(Here.X, Here.Y);

		const FVector2D To(Stop.Place.X, Stop.Place.Y);
		const float Distance = FVector2D::Distance(Flat, To);
		const FVector2D Going = Distance > UE_KINDA_SMALL_NUMBER ? (To - Flat) / Distance : FVector2D::ZeroVector;

		// somebody in their way: they stand until the way is clear, as people do
		if (Walker)
		{
			const FVector2D Off(Walker->GetActorLocation().X - Here.X, Walker->GetActorLocation().Y - Here.Y);

			if (Off.Size() < BeatRoom && FVector2D::DotProduct(Off.GetSafeNormal(), Going) > 0.3f)
			{
				Body->SetLedVelocity(FVector::ZeroVector);
				continue;
			}
		}

		const float OwnPace = Body->GetCharacterMovement()->MaxWalkSpeed;
		const float Pace = (OwnPace > 1.0f ? OwnPace : 150.0f) * Person.Pace;
		const float Step = Pace * DeltaTime;

		if (Distance <= Step)
		{
			// there: they stand as long as the stop says, and then go on to the next
			Flat = To;

			One.Wait = Stop.Wait;
			One.Stop = (One.Stop + 1) % Stops;
		}
		else
		{
			Flat += Going * Step;
		}

		// on the ground, which the place knows the height of: what can be felt under anybody is only there where it is drawn
		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		FRotator Turned = Facing;

		if (!Going.IsNearlyZero())
		{
			Turned = FMath::RInterpConstantTo(Facing, FRotator(0.0f, FVector(Going.X, Going.Y, 0.0f).Rotation().Yaw, 0.0f), DeltaTime, LotTurnSpeed);
		}

		Body->SetActorLocationAndRotation(FVector(Flat.X, Flat.Y, PlaceData->GetGroundZ(Flat.X) + HalfHeight), Turned, false, nullptr, ETeleportType::None);
		Body->SetLedVelocity(FVector(Going.X, Going.Y, 0.0f) * Pace);
	}
}

const UGutLadderData* ATownGameMode::GetFighterRank() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Rank = LadderData;

	// each rank is reached through the one before it
	for (int32 i = 0; Rank && GameInstance && GameInstance->bLadderStarted && i < GameInstance->Ladder.Rank && Rank->NextLadder; ++i)
	{
		Rank = Rank->NextLadder;
	}

	return Rank;
}

const UGutLadderData* ATownGameMode::GetShelfRank() const
{
	// what is on the counter is what the fighter's rank is sold, or, if that is nothing, what the first rank above it is
	const UGutLadderData* Rank = GetFighterRank();

	while (Rank && Rank->ArmorShop.IsEmpty())
	{
		Rank = Rank->NextLadder;
	}

	return Rank;
}

bool ATownGameMode::HasFighter() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance && GameInstance->bLadderStarted;
}

float ATownGameMode::GetShopWater() const
{
	return HasFighter() ? GetGutGameInstance()->Ladder.Water : 0.0f;
}

FString ATownGameMode::GetShopKeeper() const
{
	return (PlaceData && PlaceData->People.IsValidIndex(ShopKeeper)) ? PlaceData->People[ShopKeeper].Name : FString();
}

TArray<FTownShopLine> ATownGameMode::GetShopLines() const
{
	TArray<FTownShopLine> Lines;

	const UGutLadderData* Shelf = GetShelfRank();
	const UGutLadderData* Own = GetFighterRank();

	if (!Shelf)
	{
		return Lines;
	}

	const bool bFighter = HasFighter();
	const float Held = GetShopWater();

	static const TCHAR* Parts[] = { TEXT("body"), TEXT("arms"), TEXT("legs") };

	for (const UGutArmorData* Piece : Shelf->ArmorShop)
	{
		if (!Piece)
		{
			continue;
		}

		FTownShopLine Line;

		Line.Name = Piece->DisplayName.ToString();
		Line.Price = Piece->Price;

		// what it takes off a blow that lands on the part it covers
		Line.Detail = FString::Printf(TEXT("%s:  %.0f%% off a club, %.0f%% off a blade"), Parts[FMath::Clamp<int32>(static_cast<int32>(Piece->Slot), 0, 2)],
			(1.0f - Piece->BluntScale) * 100.0f, (1.0f - Piece->BladedScale) * 100.0f);

		Line.bWorn = bFighter && GetGutGameInstance()->Ladder.Armor.Contains(Piece->GetFName());

		if (!bFighter)
		{
			Line.Why = TEXT("no fighter");
		}
		else if (Own != Shelf)
		{
			Line.Why = FString::Printf(TEXT("%s rank"), *Shelf->RankName.ToString());
		}
		else if (Line.bWorn)
		{
			Line.Why = TEXT("worn");
		}
		else if (Held < Piece->Price)
		{
			Line.Why = FString::Printf(TEXT("%.0f L short"), Piece->Price - Held);
		}
		else
		{
			Line.bCanBuy = true;
		}

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

void ATownGameMode::OpenShop(int32 Keeper)
{
	const UGutLadderData* Shelf = GetShelfRank();
	const UGutLadderData* Own = GetFighterRank();

	ShopKeeper = Keeper;
	ShopChoice = 0;
	bShopping = true;

	// why nothing on it can be bought, if nothing can: said once, at the top
	if (!Shelf)
	{
		ShopSaid = TEXT("There is nothing on the counter.");
	}
	else if (!HasFighter())
	{
		ShopSaid = TEXT("This is ring armor, cut for the Gut's fighters, and you are not one yet: go in at the fighters' door first.");
	}
	else if (Own != Shelf)
	{
		ShopSaid = FString::Printf(TEXT("Armor is worn in the Gut from %s rank up, and sold to nobody below it. You are fighting at %s rank."), *Shelf->RankName.ToString(), Own ? *Own->RankName.ToString() : TEXT("no"));
	}
	else
	{
		ShopSaid.Reset();
	}

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogTown, Log, TEXT("Street: at %s's counter. %s"), *GetShopKeeper(), *ShopSaid);
}

void ATownGameMode::CloseShop()
{
	if (!bShopping)
	{
		return;
	}

	bShopping = false;
	ShopKeeper = INDEX_NONE;
	ShopSaid.Reset();

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->ResetIgnoreMoveInput();
	}
}

bool ATownGameMode::BuyAtShop(int32 Line)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Shelf = GetShelfRank();

	const TArray<FTownShopLine> Lines = GetShopLines();

	if (!GameInstance || !Shelf || !Lines.IsValidIndex(Line) || !Shelf->ArmorShop.IsValidIndex(Line) || !Shelf->ArmorShop[Line])
	{
		return false;
	}

	const UGutArmorData* Piece = Shelf->ArmorShop[Line];

	if (!Lines[Line].bCanBuy)
	{
		// worn already, or short: said. Anything else was said when the counter was come to, and still stands.
		if (Lines[Line].bWorn && Lines[Line].Why == TEXT("worn"))
		{
			ShopSaid = FString::Printf(TEXT("You have the %s on already."), *Lines[Line].Name.ToLower());
		}
		else if (Lines[Line].Why.EndsWith(TEXT("short")))
		{
			ShopSaid = FString::Printf(TEXT("The %s is %.0f L and you hold %.0f. Nothing here goes on a slate."), *Lines[Line].Name.ToLower(), Piece->Price, GetShopWater());
		}

		return false;
	}

	// what it goes on in place of, to say so
	FString Off;

	for (const UGutArmorData* Other : Shelf->ArmorShop)
	{
		if (Other && Other != Piece && Other->Slot == Piece->Slot && GameInstance->Ladder.Armor.Contains(Other->GetFName()))
		{
			Off = Other->DisplayName.ToString().ToLower();
		}
	}

	// with water in hand: the armorers keep no slate
	if (!GameInstance->BuyArmor(Shelf, Line, 0.0f))
	{
		return false;
	}

	// the fight the fighter is waiting for is come to as it now is, wearing it and without the water
	GameInstance->LadderAtRungStart.Armor = GameInstance->Ladder.Armor;
	GameInstance->LadderAtRungStart.Water -= Piece->Price;
	GameInstance->LadderAtRungStart.ArmorSpend += Piece->Price;
	GameInstance->SaveRun();

	ShopSaid = Off.IsEmpty()
		? FString::Printf(TEXT("Bought: the %s, %.0f L. You will have it on in your next fight."), *Lines[Line].Name.ToLower(), Piece->Price)
		: FString::Printf(TEXT("Bought: the %s, %.0f L, in place of the %s."), *Lines[Line].Name.ToLower(), Piece->Price, *Off);

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

	return true;
}

const FTownCall* ATownGameMode::GetCall() const
{
	return (bSettled && PlaceData && PlaceData->Calls.IsValidIndex(CallIndex)) ? &PlaceData->Calls[CallIndex] : nullptr;
}

FString ATownGameMode::GetPrompt() const
{
	if (!bSettled || !PlaceData || IsTalking() || bShopping || bStoppedMenu)
	{
		return FString();
	}

	if (PlaceData->People.IsValidIndex(PersonInReach))
	{
		const FTownPerson& Person = PlaceData->People[PersonInReach];

		return Person.Prompt.IsEmpty() ? FString::Printf(TEXT("Talk to %s"), *Person.Name) : Person.Prompt;
	}

	if (PlaceData->Doors.IsValidIndex(DoorInReach))
	{
		return PlaceData->Doors[DoorInReach].Prompt;
	}

	return FString();
}

FString ATownGameMode::GetSpeaker() const
{
	return IsTalking() ? PlaceData->People[TalkingTo].Name : FString();
}

FString ATownGameMode::GetWords() const
{
	if (!IsTalking() || !PlaceData->People[TalkingTo].Lines.IsValidIndex(TalkLine))
	{
		return FString();
	}

	return Told(PlaceData->People[TalkingTo].Lines[TalkLine].Words);
}

TArray<FString> ATownGameMode::GetAnswers() const
{
	TArray<FString> Result;

	if (IsTalking() && PlaceData->People[TalkingTo].Lines.IsValidIndex(TalkLine))
	{
		for (const FTownAnswer& Answer : PlaceData->People[TalkingTo].Lines[TalkLine].Answers)
		{
			Result.Add(Told(Answer.Words));
		}
	}

	return Result;
}

FString ATownGameMode::GetNotice() const
{
	return (!Notice.IsEmpty() && GetWorld()->GetTimeSeconds() < NoticeUntil) ? Notice : FString();
}

TArray<FString> ATownGameMode::GetPurses() const
{
	TArray<FString> Lines;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return Lines;
	}

	// one way in, and the line is what it always was. More than one, and each says whose water it is.
	const int32 Ways = (GameInstance->bLadderStarted ? 1 : 0) + (GameInstance->Window.bStarted ? 1 : 0) + (GameInstance->Stable.bStarted ? 1 : 0);

	auto Line = [&Lines, Ways](const TCHAR* As, float Water, float Debt, const TCHAR* Whom)
	{
		FString Text = Ways > 1 ? FString::Printf(TEXT("%s:  %.0f L in hand"), As, Water) : FString::Printf(TEXT("%.0f L in hand"), Water);

		if (Debt > 0.5f)
		{
			Text += FString::Printf(TEXT("   \u00B7   owing %s%.0f L"), Whom, Debt);
		}

		Lines.Add(Text);
	};

	if (GameInstance->bLadderStarted)
	{
		Line(TEXT("Fighting"), GameInstance->Ladder.Water, GameInstance->Ladder.Debt, Ways > 1 ? TEXT("the pit boss ") : TEXT(""));
	}

	if (GameInstance->Window.bStarted)
	{
		Line(TEXT("Betting"), GameInstance->Window.Water, GameInstance->Window.Debt, TEXT("Mother Ledger "));
	}

	if (GameInstance->Stable.bStarted)
	{
		Line(TEXT("Owning"), GameInstance->Stable.Water, GameInstance->Stable.Debt, TEXT(""));
	}

	return Lines;
}

void ATownGameMode::SetStopped(bool bStopped)
{
	if (bStopped == bStoppedMenu)
	{
		return;
	}

	bStoppedMenu = bStopped;
	StoppedChoice = 0;

	UGameplayStatics::SetGamePaused(this, bStoppedMenu);
}

TArray<FString> ATownGameMode::GetStoppedLines() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	static const TCHAR* Loud[] = { TEXT("off"), TEXT("a third"), TEXT("two thirds"), TEXT("full") };

	return { TEXT("Go on"), FString::Printf(TEXT("Sound: %s"), Loud[FMath::Clamp(GameInstance ? GameInstance->Loudness : 3, 0, 3)]), TEXT("Leave the game") };
}
