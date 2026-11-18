// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GutArmorData.h"
#include "GutLadderData.h"
#include "Misc/App.h"
#include "Tests/TownTestHelpers.h"

/**
 *  Main Street south, from Block Square down to the Gate Yard.
 *
 *  Ashmarket.Town.MainStreet    the street climbs and whoever walks it stays on it; its edges are things that stand there;
 *                               the people who walk a round walk it; the armorers' counter sells a Butcher armor for water
 *                               in hand and nobody else anything; the way up to the square and back.
 *  Ashmarket.Look.MainStreet    fixed views of the street saved as pictures, then three as it is played. Not part of a test run.
 *  Ashmarket.Probe.MainStreet   a measurement: stood, jogged down the street and stood again, for reading the frame rate
 *                               with -GutFrameRate. Not part of a test run.
 */
namespace MainStreetTests
{
	const TCHAR* MapPath = TEXT("/Game/Town/Lvl_MainStreet");

	/** A place on the map (x east, y north, in metres) as the level has it */
	FVector2D OnMap(float X, float Y)
	{
		return FVector2D((Y - 208.0f) * 100.0f, (X - 95.0f) * 100.0f);
	}

	FVector InLevel(const FVector& OnTheMap)
	{
		return FVector((OnTheMap.Y - 208.0f) * 100.0f, (OnTheMap.X - 95.0f) * 100.0f, (OnTheMap.Z - 3.0f) * 100.0f);
	}

	bool IsIn(const TCHAR* Name)
	{
		return TownTests::IsStreetReady() && TownTests::GetTown()->GetPlace() && TownTests::GetTown()->GetPlace()->Name == Name;
	}

	/** How far the player's feet are above the ground the place says is under them */
	float FeetOverGround()
	{
		const ATownGameMode* Town = TownTests::GetTown();
		const AGutFighter* Walker = Town->GetWalker();

		return Walker->GetActorLocation().Z - Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Town->GetPlace()->GetGroundZ(Walker->GetActorLocation().X);
	}

	int32 Find(const TCHAR* Name)
	{
		return TownTests::GetTown()->GetPlace()->People.IndexOfByPredicate([Name](const FTownPerson& Person) { return Person.Name == Name; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownMainStreetTest, "Ashmarket.Town.MainStreet", GutTests::Flags)

bool FTownMainStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace MainStreetTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	struct FNotes
	{
		FVector Last = FVector::ZeroVector;
		float Still = 0.0f;
		float WalkTime = 0.0f;

		/** Where the first of the people who walk a round was when the street opened */
		int32 Rounder = INDEX_NONE;
		FVector RounderWas = FVector::ZeroVector;

		/** The fighter's run as it was found, put back at the end */
		bool bLadderWas = false;
		FGutLadderState LadderWas;
		FGutLadderState RungStartWas;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	const auto InStreet = []() { return IsIn(TEXT("Main Street")); };
	const auto InSquare = []() { return IsIn(TEXT("Block Square")); };

	// a line down the road that nobody's round is on
	const FVector2D Came = OnMap(1.95f, 88.0f);

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this, Notes, Came]()
	{
		const ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		TestEqual(TEXT("Everybody the list names is in the street"), Town->GetPeople().Num(), Place->People.Num());
		TestTrue(TEXT("There are people to talk to"), Place->People.Num() >= 8);

		// the street climbs northward, three in a hundred, and the place knows it
		const float Foot = Place->GetGroundZ(OnMap(0.0f, 42.0f).X);
		const float Head = Place->GetGroundZ(OnMap(0.0f, 127.0f).X);

		TestTrue(*FString::Printf(TEXT("The head of the street is two to three metres above its foot (%.0f cm)"), Head - Foot), Head - Foot > 200.0f && Head - Foot < 300.0f);
		TestTrue(*FString::Printf(TEXT("The player stands on the street where it comes in (%.0f cm over it)"), FeetOverGround()), FMath::Abs(FeetOverGround()) < 15.0f);

		TestTrue(*FString::Printf(TEXT("Some of the street's people walk a round (%d)"), Town->GetBeatCount()), Town->GetBeatCount() >= 4);

		for (int32 i = 0; i < Place->People.Num(); ++i)
		{
			if (!Place->People[i].Beat.IsEmpty() && Town->GetPeople().IsValidIndex(i) && Town->GetPeople()[i])
			{
				Notes->Rounder = i;
				Notes->RounderWas = Town->GetPeople()[i]->GetActorLocation();
				break;
			}
		}

		GutTests::Screenshot(TEXT("80_main_street"));

		StandAt(Came, 180.0f);
		Notes->WalkTime = 0.0f;
		Notes->Still = 0.0f;
	}, InStreet });

	// east, south and west from the middle of the road until something is in the way: each time it is something that stands
	// there. (North is the way up to the square, which is gone through further on.)
	static const FVector Ways[] = { FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f) };
	static const TCHAR* WayNames[] = { TEXT("east"), TEXT("south"), TEXT("west") };

	for (int32 Way = 0; Way < 3; ++Way)
	{
		Steps.Add({ 0.0f, [this, Notes, Way, Came]()
		{
			const ATownGameMode* Town = GetTown();
			const FVector Here = Town->GetWalker()->GetActorLocation();

			float Nearest = TNumericLimits<float>::Max();

			for (const FTownBlock& Block : Town->GetPlace()->Blocks)
			{
				Nearest = FMath::Min(Nearest, Block.GetDistance(FVector2D(Here.X, Here.Y)));
			}

			for (const AGutFighter* Person : Town->GetPeople())
			{
				Nearest = FMath::Min(Nearest, Person ? FVector::Dist2D(Person->GetActorLocation(), Here) - 40.0f : Nearest);
			}

			TestTrue(*FString::Printf(TEXT("Going %s, what stops the walker is something standing there (the nearest thing is %.0f cm off)"), WayNames[Way], Nearest),
				Nearest < Town->GetWalkerRadius() + 12.0f);

			// down at the foot of the street the walker has come down with it
			if (Way == 1)
			{
				TestTrue(*FString::Printf(TEXT("At the foot of the street the walker is still on it (%.0f cm over it)"), FeetOverGround()), FMath::Abs(FeetOverGround()) < 15.0f);
				TestTrue(TEXT("And has come most of the way down it"), Here.X < OnMap(0.0f, 50.0f).X);
			}

			StandAt(Came, 180.0f);
			Notes->WalkTime = 0.0f;
			Notes->Still = 0.0f;
		}, [Notes, Way]()
		{
			AGutFighter* Walker = GetTown()->GetWalker();
			const float Step = Walker->GetWorld()->GetDeltaSeconds();

			Walker->SetJogging(true);
			Walker->AddMovementInput(Ways[Way], 1.0f);

			Notes->Still = FVector::Dist2D(Walker->GetActorLocation(), Notes->Last) < 40.0f * Step ? Notes->Still + Step : 0.0f;
			Notes->Last = Walker->GetActorLocation();
			Notes->WalkTime += Step;

			return Notes->Still >= 0.5f && Notes->WalkTime > 1.0f;
		} });
	}

	// whoever walks a round has got somewhere on it by now, and is on the street's own ground wherever that is
	Steps.Add({ 0.2f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		Town->GetWalker()->SetJogging(false);

		if (!TestTrue(TEXT("Somebody walks a round"), Town->GetPeople().IsValidIndex(Notes->Rounder) && Town->GetPeople()[Notes->Rounder]))
		{
			return;
		}

		const AGutFighter* Body = Town->GetPeople()[Notes->Rounder];
		const float Gone = FVector::Dist2D(Body->GetActorLocation(), Notes->RounderWas);
		const float Feet = Body->GetActorLocation().Z - Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		TestTrue(*FString::Printf(TEXT("%s has walked on along their round (%.0f cm)"), *Place->People[Notes->Rounder].Name, Gone), Gone > 300.0f);
		TestTrue(*FString::Printf(TEXT("And is on the street's ground where they have got to (%.0f cm over it)"), Feet - Place->GetGroundZ(Body->GetActorLocation().X)),
			FMath::Abs(Feet - Place->GetGroundZ(Body->GetActorLocation().X)) < 3.0f);

		// somebody on a round is talked to wherever they have got to, and stands while they are. (From their west: the
		// other Warden walks on their east.)
		StandAt(FVector2D(Body->GetActorLocation().X, Body->GetActorLocation().Y - 150.0f), 90.0f);

		Town->Tick(0.0f);

		TestEqual(TEXT("Stood beside them, it is them the player is offered"), Town->GetPersonInReach(), Notes->Rounder);

		Town->Interact();

		TestTrue(TEXT("And they can be talked to"), Town->IsTalking());

		Notes->RounderWas = Body->GetActorLocation();
	} });

	Steps.Add({ 1.5f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		const AGutFighter* Body = Town->GetPeople().IsValidIndex(Notes->Rounder) ? Town->GetPeople()[Notes->Rounder].Get() : nullptr;

		if (Body)
		{
			TestTrue(TEXT("Somebody being talked to stands where they are"), FVector::Dist2D(Body->GetActorLocation(), Notes->RounderWas) < 5.0f);
		}

		Town->Interact();

		TestFalse(TEXT("The talk is over"), Town->IsTalking());
	} });

	// the armorers' counter. First as nobody's fighter: everything is on it and none of it can be had.
	Steps.Add({ 0.3f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Needle = Find(TEXT("Needle"));

		Notes->bLadderWas = Run->bLadderStarted;
		Notes->LadderWas = Run->Ladder;
		Notes->RungStartWas = Run->LadderAtRungStart;

		Run->bLadderStarted = false;

		if (!TestTrue(TEXT("Needle is at the counter"), Place->People.IsValidIndex(Needle)))
		{
			return;
		}

		// on the street side of the counter
		StandBy(Place->People[Needle].Place, FVector2D(0.0f, 200.0f));

		Town->Tick(0.0f);

		TestEqual(TEXT("Needle is in reach across the counter"), Town->GetPersonInReach(), Needle);

		Town->Interact();

		TestTrue(TEXT("Needle talks"), Town->IsTalking());

		// the first thing that can be said back is to see what is on the counter
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestFalse(TEXT("The talk gives way to the counter"), Town->IsTalking());
		TestTrue(TEXT("The counter is open"), Town->IsShopping());
		TestEqual(TEXT("Six pieces are on it"), Town->GetShopLines().Num(), 6);
		TestFalse(TEXT("Somebody with no fighter is told why none of it is theirs to buy"), Town->GetShopSaid().IsEmpty());

		for (const FTownShopLine& Line : Town->GetShopLines())
		{
			TestFalse(*FString::Printf(TEXT("Nobody's fighter cannot buy the %s"), *Line.Name), Line.bCanBuy);
		}

		TestFalse(TEXT("And taking one buys nothing"), Town->BuyAtShop(0));

		// a fighter who has only got as far as Dog rank is told the rank it is sold from
		Run->ResetLadder(60.0f);
		Run->PendingStory = EGutStory::None;
		Run->Ladder.Rank = 1;
		Run->LadderAtRungStart = Run->Ladder;

		Town->CloseShop();
		Town->OpenShop(Needle);

		TestTrue(TEXT("A Dog is told armor is sold from Butcher rank"), Town->GetShopSaid().Contains(TEXT("Butcher")));
		TestFalse(TEXT("And cannot buy it, whatever it holds"), Town->BuyAtShop(0));
		TestEqual(TEXT("And still holds what it held"), Run->Ladder.Water, 60.0f);

		// a Butcher with sixty litres
		Run->Ladder.Rank = 2;
		Run->LadderAtRungStart = Run->Ladder;

		Town->CloseShop();
		Town->OpenShop(Needle);

		TestTrue(TEXT("A Butcher is told nothing: the prices are on the counter"), Town->GetShopSaid().IsEmpty());
		TestTrue(TEXT("And can buy the first piece on it"), Town->GetShopLines().Num() == 6 && Town->GetShopLines()[0].bCanBuy);
	}, InStreet });

	// the picture is of the frame after it is asked for: the last thing this step does
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("81_main_street_counter")); } });

	Steps.Add({ 0.3f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		const TArray<FTownShopLine> Lines = Town->GetShopLines();

		if (Lines.Num() != 6)
		{
			return;
		}

		// the vest: twenty litres, by the key that takes what the player is on
		const float Vest = Lines[0].Price;

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("The first piece is paid for out of the fighter's own water"), Run->Ladder.Water, 60.0f - Vest);
		TestEqual(TEXT("And is the one piece it owns"), Run->Ladder.Armor.Num(), 1);
		TestTrue(TEXT("The fight it is waiting for is come to wearing it"), Run->LadderAtRungStart.Armor == Run->Ladder.Armor && Run->LadderAtRungStart.Water == Run->Ladder.Water);
		TestTrue(TEXT("The counter shows it worn"), Town->GetShopLines()[0].bWorn && !Town->GetShopLines()[0].bCanBuy);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("It cannot be bought twice"), Run->Ladder.Water, 60.0f - Vest);

		// the breastplate goes on in place of the vest: one piece to a part of the body
		const int32 Plate = Lines.IndexOfByPredicate([&Lines](const FTownShopLine& Line) { return &Line != &Lines[0] && Line.Detail.StartsWith(TEXT("body")); });

		if (!TestTrue(TEXT("There is a second piece for the body"), Plate != INDEX_NONE))
		{
			return;
		}

		for (int32 i = 0; i < Plate; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		TestEqual(TEXT("Down the counter to it"), Town->GetShopChoice(), Plate);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("It is paid for too"), Run->Ladder.Water, 60.0f - Vest - Lines[Plate].Price);
		TestEqual(TEXT("And goes on in place of the vest"), Run->Ladder.Armor.Num(), 1);
		TestTrue(TEXT("The vest is no longer worn"), !Town->GetShopLines()[0].bWorn && Town->GetShopLines()[Plate].bWorn);

		// what is left will not stretch to anything else, and nothing here goes on a slate
		const float Left = Run->Ladder.Water;

		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (!Town->GetShopLines()[i].bWorn && Lines[i].Price > Left)
			{
				TestFalse(*FString::Printf(TEXT("With %.0f L, the %s (%.0f L) cannot be had"), Left, *Lines[i].Name, Lines[i].Price), Town->BuyAtShop(i));
			}
		}

		TestEqual(TEXT("Nothing went on a slate"), Run->Ladder.Water, Left);
		TestTrue(TEXT("And the fighter is told what it is short of"), Town->GetShopSaid().Contains(TEXT("slate")) || Left >= 15.0f);

		// the line under the pieces leaves
		while (Town->GetShopChoice() != Lines.Num())
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestFalse(TEXT("The last line leaves the counter"), Town->IsShopping());

		// up to the head of the street, short of its mouth
		StandAt(OnMap(0.4f, 124.5f), 0.0f);
	} });

	// the head of the street is as wide as the street, and walking into it anywhere is going up to the square
	Steps.Add({ 0.8f, [this]()
	{
		const ATownGameMode* Town = GetTown();
		const FVector Here = Town->GetWalker()->GetActorLocation();
		const FVector2D Arrived = OnMap(0.5f, 133.2f);

		TestTrue(*FString::Printf(TEXT("Up Main Street is the square, at its south side (%.0f cm from where that way comes in)"), FVector2D::Distance(FVector2D(Here.X, Here.Y), Arrived)),
			FVector2D::Distance(FVector2D(Here.X, Here.Y), Arrived) < 250.0f);
	}, [InSquare]()
	{
		if (InSquare())
		{
			return true;
		}

		if (ATownGameMode* Town = GetTown(); Town && Town->GetWalker() && IsIn(TEXT("Main Street")))
		{
			Town->GetWalker()->AddMovementInput(FVector(1.0f, 0.0f, 0.0f), 1.0f);
		}

		return false;
	} });

	// and south out of the square is Main Street again
	Steps.Add({ 0.8f, [this, Notes]()
	{
		const ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const FVector Here = Town->GetWalker()->GetActorLocation();
		const FVector2D Arrived = OnMap(0.5f, 123.5f);

		TestTrue(*FString::Printf(TEXT("Down out of the square is Main Street, at its head (%.0f cm from where that way comes in)"), FVector2D::Distance(FVector2D(Here.X, Here.Y), Arrived)),
			FVector2D::Distance(FVector2D(Here.X, Here.Y), Arrived) < 250.0f);

		// whatever runs after this in the same sitting gets the fighter it would have had
		Run->bLadderStarted = Notes->bLadderWas;
		Run->Ladder = Notes->LadderWas;
		Run->LadderAtRungStart = Notes->RungStartWas;
		Run->PendingStory = EGutStory::None;
		Run->TownLevel = NAME_None;
		Run->TownStood.Reset();
	}, [InStreet]()
	{
		if (InStreet())
		{
			return true;
		}

		if (ATownGameMode* Town = GetTown(); Town && Town->GetWalker() && IsIn(TEXT("Block Square")))
		{
			Town->GetWalker()->AddMovementInput(FVector(-1.0f, 0.0f, 0.0f), 1.0f);
		}

		return false;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Not a test: fixed views of Main Street saved as pictures, with nothing drawn over them, and then three as it is played.
 *  Ashmarket.Look.MainStreet. Not part of a test run.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookMainStreetTest, "Ashmarket.Look.MainStreet", GutTests::Flags)

bool FTownLookMainStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace MainStreetTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	// where each picture is taken from and what it looks at, in the map's own metres (x east, y north, z up)
	struct FView
	{
		const TCHAR* Name;
		FVector From;
		FVector At;
		float FieldOfView;
	};

	static const FView Views[] =
	{
		{ TEXT("main_01_down_the_street"), FVector(-1.8f, 126.2f, 5.8f), FVector(0.0f, 60.0f, 2.6f), 68.0f },
		{ TEXT("main_02_hide_and_plate"), FVector(-1.6f, 99.5f, 4.6f), FVector(-7.0f, 94.6f, 4.0f), 62.0f },
		{ TEXT("main_03_the_counter"), FVector(-3.8f, 95.6f, 4.5f), FVector(-7.4f, 95.0f, 4.1f), 55.0f },
		{ TEXT("main_04_tooth_and_trigger"), FVector(0.8f, 57.6f, 3.5f), FVector(6.6f, 62.4f, 3.0f), 62.0f },
		{ TEXT("main_05_the_food_stall"), FVector(-1.6f, 76.4f, 4.2f), FVector(-6.5f, 72.0f, 3.3f), 58.0f },
		{ TEXT("main_06_the_scrap_table"), FVector(1.6f, 80.0f, 4.4f), FVector(6.6f, 84.2f, 3.4f), 58.0f },
		{ TEXT("main_07_dice_and_the_cart"), FVector(1.5f, 111.0f, 5.2f), FVector(-3.0f, 103.0f, 4.0f), 70.0f },
		{ TEXT("main_08_the_barrier_and_the_dam"), FVector(0.6f, 52.0f, 3.4f), FVector(0.0f, 10.0f, 5.5f), 65.0f },
		{ TEXT("main_09_up_the_street"), FVector(-0.6f, 43.5f, 2.9f), FVector(0.0f, 110.0f, 5.4f), 65.0f },
		{ TEXT("main_10_pen_lane"), FVector(2.5f, 113.5f, 5.3f), FVector(8.0f, 117.1f, 5.0f), 60.0f },
		{ TEXT("main_11_the_alley"), FVector(-2.5f, 62.6f, 3.6f), FVector(-8.0f, 59.8f, 3.3f), 60.0f },
		{ TEXT("main_12_from_above"), FVector(0.0f, 84.0f, 62.0f), FVector(0.3f, 84.6f, 2.5f), 62.0f },
		{ TEXT("main_13_into_the_square"), FVector(0.5f, 116.0f, 5.4f), FVector(2.0f, 150.0f, 6.5f), 60.0f },
	};

	TSharedRef<TWeakObjectPtr<ACameraActor>> Camera = MakeShared<TWeakObjectPtr<ACameraActor>>();

	TArray<FScript::FStep> Steps;

	// the people are fetched in the background: wait for them, and for the textures, before the first picture
	Steps.Add({ 14.0f, [Camera]()
	{
		ATownGameMode* Town = GetTown();
		UWorld* World = Town->GetWorld();

		*Camera = World->SpawnActor<ACameraActor>();

		if (APlayerController* Eyes = World->GetFirstPlayerController())
		{
			Eyes->SetViewTarget(Camera->Get());

			if (AHUD* Words = Eyes->GetHUD())
			{
				Words->bShowHUD = false;
			}
		}
	}, []() { return IsStreetReady(); } });

	for (const FView& View : Views)
	{
		Steps.Add({ 0.1f, [Camera, View]()
		{
			if (ACameraActor* Seen = Camera->Get())
			{
				const FVector From = InLevel(View.From);

				Seen->SetActorLocationAndRotation(From, (InLevel(View.At) - From).Rotation());
				Seen->GetCameraComponent()->SetFieldOfView(View.FieldOfView);
				Seen->GetCameraComponent()->SetConstraintAspectRatio(false);
			}
		} });

		Steps.Add({ 1.2f, [View]() { GutTests::Screenshot(View.Name); } });
	}

	// and as it is played: where the player comes in, in reach of Needle, and at the counter as a Butcher
	Steps.Add({ 0.5f, []()
	{
		ATownGameMode* Town = GetTown();

		if (APlayerController* Eyes = Town->GetWorld()->GetFirstPlayerController())
		{
			Eyes->SetViewTarget(Town->GetWalker());

			if (AHUD* Words = Eyes->GetHUD())
			{
				Words->bShowHUD = true;
			}
		}
	} });

	Steps.Add({ 1.5f, []() { GutTests::Screenshot(TEXT("main_14_as_played_arriving")); } });

	Steps.Add({ 0.3f, []()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Needle = Find(TEXT("Needle"));

		if (Needle != INDEX_NONE)
		{
			GetRun()->ResetLadder(60.0f);
			GetRun()->PendingStory = EGutStory::None;
			GetRun()->Ladder.Rank = 2;
			GetRun()->LadderAtRungStart = GetRun()->Ladder;

			StandBy(Place->People[Needle].Place, FVector2D(0.0f, 200.0f));

			if (AController* Eyes = Town->GetWalker()->GetController())
			{
				Eyes->SetControlRotation(FRotator(-8.0f, -90.0f, 0.0f));
			}
		}
	} });

	Steps.Add({ 1.5f, []() { GutTests::Screenshot(TEXT("main_15_as_played_in_reach")); } });
	Steps.Add({ 0.3f, []() { GetTown()->Interact(); } });
	Steps.Add({ 2.5f, []() { GutTests::Screenshot(TEXT("main_16_as_played_needle")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_17_as_played_the_counter")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_18_as_played_bought")); } });

	Steps.Add({ 0.3f, []()
	{
		GetTown()->CloseShop();

		GetRun()->bLadderStarted = false;
		GetRun()->Ladder = FGutLadderState();
		GetRun()->LadderAtRungStart = FGutLadderState();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Not a test: a measurement. Stands at the head of Main Street, jogs down it to the barrier and stands again, for reading
 *  the frame rate with -GutFrameRate. Ashmarket.Probe.MainStreet.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeMainStreetTest, "Ashmarket.Probe.MainStreet", GutTests::Flags)

bool FTownProbeMainStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace MainStreetTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;
	TSharedRef<float> Clock = MakeShared<float>(0.0f);

	const auto Go = [&Steps, Clock](const TCHAR* What, float Seconds, const FVector& Way)
	{
		Steps.Add({ 0.0f, [What]() { UE_LOG(LogGut, Log, TEXT("Main Street: end of %s"), What); }, [Clock, Seconds, Way]()
		{
			AGutFighter* Walker = GetTown()->GetWalker();

			if (!Way.IsZero())
			{
				Walker->SetJogging(true);
				Walker->AddMovementInput(Way, 1.0f);
			}

			*Clock += Walker->GetWorld()->GetDeltaSeconds();

			if (*Clock < Seconds)
			{
				return false;
			}

			*Clock = 0.0f;

			return true;
		} });
	};

	// the people standing about are fetched in the background: give them time to come
	Steps.Add({ 12.0f, []()
	{
		UE_LOG(LogGut, Log, TEXT("Main Street: everybody is here"));

		// on a line down the road that nobody's round is on
		StandAt(OnMap(1.95f, 123.5f), 180.0f);

		if (AController* Eyes = GetTown()->GetWalker()->GetController())
		{
			Eyes->SetControlRotation(FRotator(-6.0f, 180.0f, 0.0f));
		}
	}, &IsStreetReady });

	Go(TEXT("standing at the head of the street"), 10.0f, FVector::ZeroVector);
	Go(TEXT("jogging down it"), 22.0f, FVector(-1.0f, 0.0f, 0.0f));
	Go(TEXT("standing at the barrier, looking at the dam"), 10.0f, FVector::ZeroVector);

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

#endif
