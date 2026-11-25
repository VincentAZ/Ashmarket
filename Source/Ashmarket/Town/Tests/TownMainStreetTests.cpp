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
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GutLadderData.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
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

		// what lights the street is seen to burn: the drums and the lanterns are fires, and the game finds each to make it waver
		TestTrue(*FString::Printf(TEXT("Fires burn in the street (%d)"), Town->GetFireCount()), Town->GetFireCount() >= 6);

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
 *  Ashmarket.Town.Market   The stalls on Main Street and the bag: bread, tea and a splint are bought with water in hand
 *                          and go into the bag; the bag opens anywhere and shuts by the back key; bread and tea are used
 *                          once before a fight, a splint only on a limb that is hurt and not broken through; nobody is
 *                          sold what they cannot pay for, and nothing is used up for nothing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownMarketTest, "Ashmarket.Town.Market", GutTests::Flags)

bool FTownMarketTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace MainStreetTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	const auto InStreet = []() { return IsIn(TEXT("Main Street")); };

	// goes up to somebody's stall by talking to them and giving the first answer
	const auto ToStall = [this](const TCHAR* Name)
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Who = Find(Name);

		if (!TestTrue(*FString::Printf(TEXT("%s is in the street"), Name), Place->People.IsValidIndex(Who)))
		{
			return false;
		}

		// on the road's side of them
		StandBy(Place->People[Who].Place, FVector2D(0.0f, Place->People[Who].Place.Y < OnMap(0.0f, 0.0f).Y ? 190.0f : -190.0f));

		Town->Tick(0.0f);

		TestEqual(*FString::Printf(TEXT("%s is in reach"), Name), Town->GetPersonInReach(), Who);

		Town->Interact();
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		return TestTrue(*FString::Printf(TEXT("Asking %s opens their stall"), Name), Town->IsShopping() && !Town->IsTalking() && Town->GetShopTitle().Contains(TEXT("STALL")));
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this, ToStall]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		// a fighter with ten litres, whole, and nothing in its bag
		Run->bStreetForced = true;
		Run->ResetLadder(10.0f);
		Run->PendingStory = EGutStory::None;
		Run->Bag.Reset();
		Run->Stable = FGutStableState();
		Run->Window = FGutWindowState();

		TestFalse(TEXT("The bag is shut to begin with"), Town->IsBagOpen());

		if (!ToStall(TEXT("Stallholder")))
		{
			return;
		}

		TArray<FTownShopLine> Lines = Town->GetShopLines();

		TestTrue(TEXT("The stall has bread on it, at a price, and it can be had"), Lines.Num() == 1 && Lines[0].Name.Contains(TEXT("bread")) && Lines[0].bCanBuy && Lines[0].Price == 2.0f);
		TestTrue(TEXT("The stall says whose water is being spent"), Town->GetShopPurse().Contains(TEXT("10 L")) && Town->GetShopPurse().Contains(TEXT("fighter")));

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Bread bought is bread in the bag"), Run->GetBagCount(FName("bread")), 1);
		TestEqual(TEXT("And paid for"), Run->Ladder.Water, 8.0f);
		TestEqual(TEXT("In what is written down too"), Run->LadderAtRungStart.Water, 8.0f);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("A second is a second"), Run->GetBagCount(FName("bread")), 2);

		Town->HandleBack();

		TestTrue(TEXT("The back key leaves the stall, and the player can walk"), !Town->IsShopping() && !UGameplayStatics::GetPlayerController(Town, 0)->IsMoveInputIgnored());

		// tea is a tenth of a litre, as she says
		if (ToStall(TEXT("Tea seller")))
		{
			Town->HandleMenuInput(EGutMenuInput::Confirm);

			TestEqual(TEXT("A cup of tea is in the bag"), Run->GetBagCount(FName("tea")), 1);
			TestTrue(*FString::Printf(TEXT("For a tenth of a litre (%.2f L left)"), Run->Ladder.Water), FMath::IsNearlyEqual(Run->Ladder.Water, 5.9f, 0.001f));

			Town->CloseShop();
		}

		// the scrap table: a splint is six, and there is not six left
		if (ToStall(TEXT("Scrap seller")))
		{
			Lines = Town->GetShopLines();

			TestTrue(TEXT("What cannot be paid for is marked so"), Lines.Num() == 1 && !Lines[0].bCanBuy && Lines[0].Why.Contains(TEXT("short")));
			TestFalse(TEXT("And is not sold"), Town->BuyAtShop(0));
			TestEqual(TEXT("Nothing went into the bag"), Run->GetBagCount(FName("splint")), 0);

			Run->Ladder.Water = 6.0f;
			Run->LadderAtRungStart.Water = 6.0f;

			TestTrue(TEXT("With six litres it is sold"), Town->BuyAtShop(0));
			TestEqual(TEXT("And is in the bag"), Run->GetBagCount(FName("splint")), 1);
			TestEqual(TEXT("And the water is gone"), Run->Ladder.Water, 0.0f);

			Town->CloseShop();
		}
	}, InStreet });

	// the bag by its own keys, as a player opens it: right on the pad's D-pad, and I
	const auto Key = [this](const FKey& Which, bool bPressed)
	{
		if (APlayerController* Hands = UGameplayStatics::GetPlayerController(GetTown(), 0))
		{
			Hands->InputKey(FInputKeyEventArgs(nullptr, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), Which, bPressed ? IE_Pressed : IE_Released, 0));
		}
	};

	Steps.Add({ 0.3f, [this, Key]()
	{
		TestFalse(TEXT("The bag starts shut"), GetTown()->IsBagOpen());

		Key(EKeys::Gamepad_DPad_Right, true);
	}, InStreet });

	Steps.Add({ 0.3f, [this, Key]()
	{
		TestTrue(TEXT("Right on the D-pad opens the bag"), GetTown()->IsBagOpen());

		Key(EKeys::Gamepad_DPad_Right, false);
		Key(EKeys::I, true);
	}, InStreet });

	Steps.Add({ 0.3f, [this, Key]()
	{
		TestFalse(TEXT("And I shuts it"), GetTown()->IsBagOpen());

		Key(EKeys::I, false);

		// and for whoever has found neither key, it is on the menu that stops the game
		ATownGameMode* Town = GetTown();

		Town->SetStopped(true);
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("The stopped menu's second line opens the bag and lets the game go on"), Town->IsBagOpen() && !Town->IsStopped());

		Town->ToggleBag();
	}, InStreet });

	Steps.Add({ 0.3f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		// the bag, wherever the player stands
		Town->ToggleBag();

		TestTrue(TEXT("The bag opens"), Town->IsBagOpen() && Town->GetShopTitle().Contains(TEXT("BAG")));

		TArray<FTownBagLine> Things = Town->GetBagLines();

		const auto Find = [&Things](ETownBagKind Kind, const TCHAR* Name)
		{
			return Things.IndexOfByPredicate([Kind, Name](const FTownBagLine& Thing) { return Thing.Kind == Kind && Thing.Name.Contains(Name); });
		};

		if (!TestEqual(TEXT("Water, three kinds of bought thing and the map are in it"), Things.Num(), 5))
		{
			return;
		}

		TestTrue(TEXT("Water first, how much, and whose"), Things[0].Kind == ETownBagKind::Water && Things[0].Count == TEXT("0 L") && Things[0].Name.Contains(TEXT("fighter")));
		TestTrue(TEXT("Then bread, two of them, to be eaten"), Things[1].Name.Contains(TEXT("bread")) && Things[1].Count.Contains(TEXT("2")) && Things[1].bCanUse && Things[1].Picture == FName("bag_bread"));
		TestTrue(TEXT("Tea can be drunk"), Things[2].bCanUse);
		TestTrue(TEXT("A splint is no use to somebody who is whole"), !Things[3].bCanUse && Things[3].Why.Contains(TEXT("nothing to bind")));
		TestTrue(TEXT("And the map, which can be opened"), Things[4].Kind == ETownBagKind::Map && Things[4].bCanUse);

		for (const FTownBagLine& Thing : Things)
		{
			const FString Picture = Thing.Picture.ToString();

			TestNotNull(*FString::Printf(TEXT("%s has a picture (%s)"), *Thing.Name, *Picture), LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/UI/%s.%s"), *Picture, *Picture), nullptr, LOAD_NoWarn | LOAD_Quiet));
		}

		// down to the bread, and eaten once
		Town->HandleMenuInput(EGutMenuInput::Down);

		TestTrue(TEXT("The key that says yes uses what the player is on"), Town->GetBagChoice() == 1 && Town->GetShopVerb().Contains(TEXT("use")));

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("Bread eaten is in the fighter for its next fight"), Run->Ladder.bFed && Run->LadderAtRungStart.bFed);
		TestEqual(TEXT("And out of the bag"), Run->GetBagCount(FName("bread")), 1);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("A second does nothing, and is kept"), Run->GetBagCount(FName("bread")), 1);

		// the splint: not on a whole fighter, nor on a limb broken through, and then on one that is only hurt
		TestFalse(TEXT("The splint is not used up on nothing"), Town->UseFromBag(3));
		TestEqual(TEXT("It is still in the bag"), Run->GetBagCount(FName("splint")), 1);

		const float Whole = Town->GetWalker()->GetMaxLimbHealth();

		Run->Ladder.bInjuriesCarried = true;

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			Run->Ladder.LimbHealth[i] = Whole;
		}

		Run->Ladder.LimbHealth[0] = 0.0f;

		TestFalse(TEXT("Nor on a limb that is broken through"), Town->UseFromBag(3));

		Run->Ladder.LimbHealth[3] = Whole * 0.4f;

		TestTrue(TEXT("On a hurt limb it is used"), Town->UseFromBag(3));
		TestTrue(*FString::Printf(TEXT("And gives back half of what was lost (%.0f of %.0f)"), Run->Ladder.LimbHealth[3], Whole), FMath::IsNearlyEqual(Run->Ladder.LimbHealth[3], Whole * 0.7f, 0.01f));
		TestEqual(TEXT("The broken one is as it was"), Run->Ladder.LimbHealth[0], 0.0f);
		TestEqual(TEXT("And the splint is gone"), Run->GetBagCount(FName("splint")), 0);
		TestEqual(TEXT("Four things are left"), Town->GetBagLines().Num(), 4);

		// somebody with no fight ahead, and nobody with them, keeps what they carry
		Run->bLadderStarted = false;

		Things = Town->GetBagLines();

		int32 Tea = Find(ETownBagKind::Good, TEXT("tea"));

		TestTrue(TEXT("With no fight ahead nothing in the bag is used"), Things.IsValidIndex(Tea) && !Things[Tea].bCanUse && Things[Tea].Why.Contains(TEXT("no fight")) && !Town->UseFromBag(Tea));
		TestEqual(TEXT("The tea is kept"), Run->GetBagCount(FName("tea")), 1);

		Run->bLadderStarted = true;

		// somebody bought off lot three, walking with the player: they are in the bag as their bill of sale
		FGutStableFighter Bought;

		Bought.FighterName = FText::FromString(TEXT("Tally"));
		Bought.bOwned = true;
		Bought.Price = 30.0f;
		Bought.Lot = 3;
		Bought.bInTow = true;
		Bought.Resentment = 20.0f;

		Run->Stable.bStarted = true;
		Run->Stable.Fighters.Add(Bought);

		Things = Town->GetBagLines();

		const int32 Paper = Find(ETownBagKind::Person, TEXT("Tally"));

		TestTrue(TEXT("Whoever is owned is in the bag, as the paper that says so"), Things.IsValidIndex(Paper) && Things[Paper].Picture == FName("bag_paper_lot3") && Things[Paper].Why.Contains(TEXT("with you")) && Things[Paper].Detail.Contains(TEXT("bill of sale")));
		TestNotNull(TEXT("Lot three's bill of sale has a picture"), LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/bag_paper_lot3.bag_paper_lot3"), nullptr, LOAD_NoWarn | LOAD_Quiet));
		TestFalse(TEXT("Nothing is done with a person from the bag"), Things.IsValidIndex(Paper) && Town->UseFromBag(Paper));

		// bread that two could have is asked about, with the player on whoever has not eaten
		const int32 Bread = Find(ETownBagKind::Good, TEXT("bread"));

		TestTrue(TEXT("The bread says it can be given"), Things.IsValidIndex(Bread) && Things[Bread].bCanUse && Things[Bread].Verb.Contains(TEXT("give")));
		TestFalse(TEXT("Nothing is done until it is said who it is for"), Town->UseFromBag(Bread));

		const TArray<FTownBagTaker> Takers = Town->GetBagTakers(FName("bread"));

		TestTrue(TEXT("It asks who it is for: the player, who has eaten, and Tally, who has not"), Town->IsBagAsking() && Takers.Num() == 2 && !Takers[0].bCan && Takers[1].bCan && Takers[1].Name == TEXT("Tally") && Town->GetBagAskChoice() == 1);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("Given, it is in them for their next fight, and out of the bag"), Run->Stable.Fighters[0].bFed && Run->GetBagCount(FName("bread")) == 0 && !Town->IsBagAsking() && Town->IsBagOpen());
		TestTrue(*FString::Printf(TEXT("And it is remembered (%.0f)"), Run->Stable.Fighters[0].Resentment), Run->Stable.Fighters[0].Resentment < 20.0f);

		// the back key, while it is asking, goes back to the bag and no further
		Things = Town->GetBagLines();
		Tea = Find(ETownBagKind::Good, TEXT("tea"));

		Town->UseFromBag(Tea);

		TestTrue(TEXT("Tea is asked about too"), Town->IsBagAsking());

		Town->HandleBack();

		TestTrue(TEXT("The back key leaves the asking, and the bag stays open with the tea in it"), Town->IsBagOpen() && !Town->IsBagAsking() && Run->GetBagCount(FName("tea")) == 1);

		// nothing can be handed to somebody who is in the yard
		Run->Stable.Fighters[0].bInTow = false;

		TestEqual(TEXT("With them in the yard there is only the player to have it"), Town->GetBagTakers(FName("tea")).Num(), 1);
		Things = Town->GetBagLines();

		const int32 Kept = Find(ETownBagKind::Person, TEXT("Tally"));

		TestTrue(TEXT("And their paper says where they are"), Things.IsValidIndex(Kept) && Things[Kept].Why.Contains(TEXT("yard")));

		// the map is in the bag, and opens from it

		TestTrue(TEXT("The map opens from the bag"), Town->UseFromBag(Find(ETownBagKind::Map, TEXT("Map"))) && !Town->IsBagOpen() && Town->IsMapUp());

		Town->HandleBack();

		TestTrue(TEXT("And the back key puts it away, and the player can walk"), !Town->IsMapUp() && !Town->IsShopping() && !UGameplayStatics::GetPlayerController(Town, 0)->IsMoveInputIgnored());

		Town->ToggleBag();
		Town->HandleBack();

		TestTrue(TEXT("The back key shuts the bag, and the player can walk"), !Town->IsBagOpen() && !Town->IsShopping() && !UGameplayStatics::GetPlayerController(Town, 0)->IsMoveInputIgnored());

		Town->ToggleBag();
		Town->ToggleBag();

		TestFalse(TEXT("And so does its own key"), Town->IsBagOpen());

		// whatever else runs after this gets nobody's fighter and an empty bag
		Run->Bag.Reset();
		Run->Stable = FGutStableState();
		Run->Ladder = FGutLadderState();
		Run->LadderAtRungStart = FGutLadderState();
		Run->bLadderStarted = false;
		Run->PendingStory = EGutStory::None;
		Run->bStreetForced.Reset();
		Run->TownLevel = NAME_None;
		Run->TownStood.Reset();
	}, InStreet });

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
		{ TEXT("main_02_hide_and_plate"), FVector(-0.6f, 104.5f, 4.9f), FVector(-8.6f, 96.0f, 4.9f), 68.0f },
		{ TEXT("main_03_the_counter"), FVector(-4.6f, 97.8f, 4.6f), FVector(-9.0f, 96.0f, 4.1f), 55.0f },
		{ TEXT("main_04_tooth_and_trigger"), FVector(-0.4f, 55.6f, 3.6f), FVector(6.2f, 62.6f, 3.2f), 66.0f },
		{ TEXT("main_05_the_food_stall"), FVector(-1.0f, 76.6f, 4.2f), FVector(-6.4f, 72.0f, 3.2f), 60.0f },
		{ TEXT("main_06_the_scrap_stall"), FVector(1.2f, 79.4f, 4.4f), FVector(6.4f, 84.2f, 3.5f), 60.0f },
		{ TEXT("main_07_dice_and_the_cart"), FVector(1.5f, 111.0f, 5.2f), FVector(-3.0f, 103.0f, 4.0f), 70.0f },
		{ TEXT("main_08_the_barrier_and_the_dam"), FVector(0.6f, 52.0f, 3.4f), FVector(0.0f, 6.0f, 8.5f), 72.0f },
		{ TEXT("main_09_up_the_street"), FVector(-0.6f, 43.5f, 2.9f), FVector(0.0f, 110.0f, 5.4f), 65.0f },
		{ TEXT("main_10_pen_lane"), FVector(2.5f, 113.5f, 5.3f), FVector(8.0f, 117.1f, 5.0f), 60.0f },
		{ TEXT("main_11_the_alley"), FVector(-2.5f, 62.6f, 3.6f), FVector(-8.0f, 59.8f, 3.3f), 60.0f },
		{ TEXT("main_12_from_above"), FVector(0.0f, 84.0f, 62.0f), FVector(0.3f, 84.6f, 2.5f), 62.0f },
		{ TEXT("main_13_into_the_square"), FVector(0.5f, 116.0f, 5.4f), FVector(2.0f, 150.0f, 6.5f), 60.0f },
		{ TEXT("main_13a_the_laws"), FVector(1.0f, 43.2f, 3.2f), FVector(0.0f, 0.0f, 11.5f), 62.0f },
		{ TEXT("main_13b_under_the_water_line"), FVector(-3.3f, 88.6f, 3.9f), FVector(-5.6f, 70.0f, 3.6f), 62.0f },
		{ TEXT("main_13c_a_fire"), FVector(2.4f, 79.0f, 3.9f), FVector(5.6f, 76.0f, 3.2f), 50.0f },
		{ TEXT("main_13d_work_lamps"), FVector(0.5f, 97.0f, 4.2f), FVector(5.6f, 92.5f, 6.4f), 58.0f },
		{ TEXT("main_13e_the_tea_stall"), FVector(2.2f, 105.0f, 4.9f), FVector(6.5f, 108.0f, 4.2f), 55.0f },
		{ TEXT("main_13f_thread_at_the_machine"), FVector(-4.2f, 97.4f, 4.4f), FVector(-7.4f, 100.2f, 4.0f), 55.0f },

		// the Gate Yard, opened on 25 November: -GutLookOnly=yard takes these alone
		{ TEXT("yard_01_down_into_the_yard"), FVector(0.4f, 39.0f, 3.0f), FVector(0.5f, 4.0f, 3.2f), 68.0f },
		{ TEXT("yard_02_the_tally_desk"), FVector(2.2f, 12.6f, 2.9f), FVector(6.0f, 7.4f, 2.4f), 62.0f },
		{ TEXT("yard_03_the_foot_lane_and_the_tunnel"), FVector(1.9f, 9.6f, 2.9f), FVector(1.75f, -10.0f, 2.2f), 60.0f },
		{ TEXT("yard_04_the_wagon_lane"), FVector(3.6f, 9.4f, 2.9f), FVector(-5.0f, 3.0f, 2.6f), 66.0f },
		{ TEXT("yard_05_up_from_the_gate"), FVector(-0.6f, 1.6f, 2.9f), FVector(0.0f, 40.0f, 4.6f), 70.0f },
		{ TEXT("yard_06_west_side"), FVector(4.0f, 22.0f, 2.9f), FVector(-9.0f, 20.0f, 2.6f), 70.0f },
		{ TEXT("yard_07_east_side"), FVector(-4.0f, 24.0f, 2.9f), FVector(9.5f, 26.0f, 2.8f), 70.0f },
		{ TEXT("yard_08_in_the_tunnel"), FVector(-0.8f, -2.0f, 2.9f), FVector(1.2f, -19.0f, 2.3f), 60.0f },
		{ TEXT("yard_09_from_above"), FVector(0.0f, 20.0f, 46.0f), FVector(0.2f, 20.4f, 2.0f), 62.0f },
		{ TEXT("yard_10_the_pipe_at_the_dam"), FVector(-1.6f, 7.0f, 3.2f), FVector(-5.2f, 0.0f, 3.8f), 62.0f },
		{ TEXT("yard_11_across_the_desk"), FVector(5.0f, 5.6f, 2.9f), FVector(5.3f, 9.4f, 2.6f), 60.0f },
		{ TEXT("yard_12_the_pipe_from_above"), FVector(-5.2f, 4.0f, 16.0f), FVector(-5.19f, 4.2f, 2.0f), 60.0f },
	};

	// only those whose names have this in them, if it is given
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("GutLookOnly="), Only);

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
		if (!Only.IsEmpty() && !FString(View.Name).Contains(Only))
		{
			continue;
		}

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

	// the bread stall, and then the bag with what was bought in it
	Steps.Add({ 0.3f, []()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Who = MainStreetTests::Find(TEXT("Stallholder"));

		Town->CloseShop();

		GetRun()->Ladder.Water = 12.0f;
		GetRun()->Bag.Reset();

		if (Place->People.IsValidIndex(Who))
		{
			StandBy(Place->People[Who].Place, FVector2D(0.0f, 190.0f));

			Town->Tick(0.0f);
			Town->Interact();
		}
	} });

	Steps.Add({ 2.0f, []() { GutTests::Screenshot(TEXT("main_19_as_played_the_stallholder")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_20_as_played_the_stall")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_21_as_played_bread_bought")); } });

	// the bag of somebody who fights, bets and owns: water three ways, what was bought, a bought woman walking with them, armor, the map and a debt
	Steps.Add({ 0.3f, []()
	{
		UGutGameInstance* Run = GetRun();

		GetTown()->HandleBack();

		Run->AddToBag(FName("tea"), 2);
		Run->AddToBag(FName("splint"), 1);

		Run->Window.bStarted = true;
		Run->Window.Water = 15.0f;
		Run->Window.Debt = 35.0f;

		FGutStableFighter Bought;

		Bought.FighterName = FText::FromString(TEXT("Tally"));
		Bought.Billing = FText::FromString(TEXT("A woman, twenty-four. Cooks, and can strip a pump."));
		Bought.bOwned = true;
		Bought.Price = 42.0f;
		Bought.Lot = 4;
		Bought.bInTow = true;
		Bought.Strength = 1;
		Bought.Speed = 2;
		Bought.Nerve = 1;

		Run->Stable.bStarted = true;
		Run->Stable.Water = 8.0f;
		Run->Stable.Fighters.Add(Bought);

		GetTown()->ToggleBag();
	} });

	const auto Down = [](int32 Times)
	{
		return [Times]()
		{
			for (int32 i = 0; i < Times; ++i)
			{
				GetTown()->HandleMenuInput(EGutMenuInput::Down);
			}
		};
	};

	Steps.Add({ 1.5f, []() { GutTests::Screenshot(TEXT("main_22_as_played_the_bag")); } });

	// past the three waters to the bread; asked who it is for; given to whoever is walking with the player
	Steps.Add({ 0.3f, Down(3) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_23_as_played_bag_bread")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 0.3f, Down(1) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_24_as_played_bag_who_for")); } });
	Steps.Add({ 0.3f, []() { GetTown()->HandleMenuInput(EGutMenuInput::Confirm); } });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_25_as_played_bag_given")); } });

	// the bread is gone: tea, splint, and then her bill of sale, the armor, the map, and what is owed
	Steps.Add({ 0.3f, Down(1) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_26_as_played_bag_splint")); } });
	Steps.Add({ 0.3f, Down(1) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_27_as_played_bag_bill_of_sale")); } });
	Steps.Add({ 0.3f, Down(1) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_28_as_played_bag_worn")); } });
	Steps.Add({ 0.3f, Down(2) });
	Steps.Add({ 1.0f, []() { GutTests::Screenshot(TEXT("main_29_as_played_bag_note")); } });

	Steps.Add({ 0.3f, []()
	{
		GetTown()->CloseShop();

		GetRun()->Bag.Reset();
		GetRun()->Stable = FGutStableState();
		GetRun()->Window = FGutWindowState();
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

/**
 *  Ashmarket.Town.Purse    one purse: what a fighter, somebody betting and an owner hold is one lot of water. Whatever any of
 *                          the three gains or spends is the player's, each of the three is kept at what the player holds, only
 *                          a fighter stands below nothing, and somebody who starts a new way starts it with what they hold.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownPurseTest, "Ashmarket.Town.Purse", GutTests::Flags)

bool FTownPurseTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		// a fighter with ten litres who has also bet, and holds twenty-five to bet with; no yard
		Run->ForgetPurse();
		Run->bStreetForced = true;
		Run->ResetLadder(10.0f);
		Run->PendingStory = EGutStory::None;
		Run->Stable = FGutStableState();
		Run->Window = FGutWindowState();
		Run->Window.bStarted = true;
		Run->Window.Water = 25.0f;

		TestFalse(TEXT("Under test the three are kept apart unless asked"), (Town->Tick(0.0f), Run->IsOnePurse()));

		Run->bSharesWaterInTests = true;
		Town->Tick(0.0f);

		TestTrue(TEXT("The first time, everything held is one purse"), Run->IsOnePurse() && FMath::IsNearlyEqual(Run->GetPurse(), 35.0f));
		TestTrue(TEXT("And each way holds it"), FMath::IsNearlyEqual(Run->Ladder.Water, 35.0f) && FMath::IsNearlyEqual(Run->Window.Water, 35.0f));
		TestTrue(TEXT("The street says what is held, once"), Town->GetPurses().Num() == 1 && Town->GetPurses()[0].Contains(TEXT("35")));

		// twenty won at the window, five spent as a fighter
		Run->Window.Water += 20.0f;
		Town->Tick(0.0f);
		Run->Ladder.Water -= 5.0f;
		Town->Tick(0.0f);

		TestTrue(TEXT("What one way wins and another spends is the same water"), FMath::IsNearlyEqual(Run->GetPurse(), 50.0f) && FMath::IsNearlyEqual(Run->Ladder.Water, 50.0f) && FMath::IsNearlyEqual(Run->Window.Water, 50.0f));

		// onto the pit boss's slate
		Run->Ladder.Water -= 62.0f;
		Town->Tick(0.0f);

		TestTrue(TEXT("Only a fighter stands below nothing"), FMath::IsNearlyEqual(Run->GetPurse(), -12.0f) && FMath::IsNearlyEqual(Run->Ladder.Water, -12.0f) && FMath::IsNearlyEqual(Run->Window.Water, 0.0f));

		Run->Window.Water += 30.0f;
		Town->Tick(0.0f);

		TestTrue(TEXT("And what is won pays the slate first"), FMath::IsNearlyEqual(Run->GetPurse(), 18.0f) && FMath::IsNearlyEqual(Run->Ladder.Water, 18.0f) && FMath::IsNearlyEqual(Run->Window.Water, 18.0f));

		TestTrue(TEXT("Somebody who starts a yard starts it with what they hold"), FMath::IsNearlyEqual(Run->GetStartingWater(70.0f), 18.0f));
		TestTrue(TEXT("The bag has one water in it"), Town->GetBagLines().FilterByPredicate([](const FTownBagLine& Line) { return Line.Kind == ETownBagKind::Water; }).Num() == 1);

		// and as it was
		Run->bSharesWaterInTests = false;
		Run->ForgetPurse();
		Run->bStreetForced.Reset();
		Run->Window = FGutWindowState();
		Run->ResetLadder(0.0f);
		Run->PendingStory = EGutStory::None;

		TestTrue(TEXT("With no purse, a yard starts with its own stake"), FMath::IsNearlyEqual(Run->GetStartingWater(70.0f), 70.0f));
	}, []() { return MainStreetTests::IsIn(TEXT("Main Street")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.Finds    what lies about Main Street is there to be picked up once: standing over one offers it, taking it puts
 *                          it in the bag and it is gone from the street for good, the flask's water can be poured into one's own,
 *                          and the scrap seller pays for scrap and for a tag.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownFindsTest, "Ashmarket.Town.Finds", GutTests::Flags)

bool FTownFindsTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace MainStreetTests;

	if (!AutomationOpenMap(MainStreetTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	// first, from four metres off, as somebody walking the street comes on one: what there is to see of it
	Steps.Add({ 0.6f, [this]()
	{
		const UTownPlaceData* Place = GetTown()->GetPlace();

		if (Place && !Place->Finds.IsEmpty())
		{
			GetRun()->Found.Reset();

			StandBy(Place->Finds[0].Place, FVector2D(0.0f, 400.0f));
		}
	} });

	Steps.Add({ 1.2f, []() { GutTests::Screenshot(TEXT("82_main_street_something_lying_there")); } });

	Steps.Add({ 0.2f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		// a fighter with ten litres and an empty bag, who has found nothing yet
		Run->bStreetForced = true;
		Run->ResetLadder(10.0f);
		Run->PendingStory = EGutStory::None;
		Run->Bag.Reset();
		Run->Stable = FGutStableState();
		Run->Window = FGutWindowState();

		if (!TestTrue(TEXT("Main Street has things lying about in its list"), Place && Place->Finds.Num() >= 3))
		{
			return;
		}

		// whichever of them is scrap, and whichever is the flask
		const int32 Scrap = Place->Finds.IndexOfByPredicate([](const FTownFind& Lying) { return Lying.Good == FName("scrap"); });
		const int32 Flask = Place->Finds.IndexOfByPredicate([](const FTownFind& Lying) { return Lying.Good == FName("flask"); });

		if (!TestTrue(TEXT("Scrap and a flask are among them"), Scrap != INDEX_NONE && Flask != INDEX_NONE))
		{
			return;
		}

		for (const int32 Which : { Scrap, Flask })
		{
			StandBy(Place->Finds[Which].Place, FVector2D(40.0f, 0.0f));
			Town->Tick(0.0f);

			if (!TestEqual(TEXT("Standing over a find puts it in reach"), Town->GetFindInReach(), Which))
			{
				return;
			}

			TestTrue(TEXT("And it is offered"), Town->GetPrompt().StartsWith(TEXT("Pick up")));

			Town->Interact();
			Town->Tick(0.0f);

			TestEqual(TEXT("Picked up, it is no longer in reach"), Town->GetFindInReach(), static_cast<int32>(INDEX_NONE));
		}

		TestTrue(TEXT("Both are in the bag"), Run->GetBagCount(FName("scrap")) == 1 && Run->GetBagCount(FName("flask")) == 1);
		TestEqual(TEXT("And both are remembered as found"), Run->Found.Num(), 2);

		// the flask: half a litre, poured into one's own
		const float Before = Run->Ladder.Water;

		Town->ToggleBag();

		const TArray<FTownBagLine> Lines = Town->GetBagLines();
		const int32 Line = Lines.IndexOfByPredicate([](const FTownBagLine& One) { return One.Good == FName("flask"); });

		TestTrue(TEXT("The flask is a line in the bag, and can be used"), Lines.IsValidIndex(Line) && Lines[Line].bCanUse);

		Town->UseFromBag(Line);

		TestTrue(TEXT("Its water is poured into the player's own"), FMath::IsNearlyEqual(Run->Ladder.Water, Before + 0.5f) && Run->GetBagCount(FName("flask")) == 0);

		Town->ToggleBag();

		// the scrap seller pays a litre for the scrap
		const int32 Seller = Find(TEXT("Scrap seller"));

		if (!TestTrue(TEXT("The scrap seller is in the street"), Place->People.IsValidIndex(Seller)))
		{
			return;
		}

		Town->OpenStall(Seller);

		const TArray<FTownShopLine> Stall = Town->GetShopLines();
		const int32 Sell = Stall.IndexOfByPredicate([](const FTownShopLine& One) { return One.Name.StartsWith(TEXT("Sell")); });

		if (TestTrue(TEXT("With scrap in the bag, the scrap seller's stall has a line to sell it"), Stall.IsValidIndex(Sell)))
		{
			const float Held = Run->Ladder.Water;

			TestTrue(TEXT("Selling it pays"), Town->BuyAtShop(Sell) && FMath::IsNearlyEqual(Run->Ladder.Water, Held + 1.0f) && Run->GetBagCount(FName("scrap")) == 0);
			TestFalse(TEXT("And there is nothing left to sell"), Town->GetShopLines().ContainsByPredicate([](const FTownShopLine& One) { return One.Name.StartsWith(TEXT("Sell")); }));
		}

		Town->HandleBack();

		// as it was
		Run->Found.Reset();
		Run->Bag.Reset();
		Run->bStreetForced.Reset();
		Run->ResetLadder(0.0f);
		Run->PendingStory = EGutStory::None;
	}, []() { return MainStreetTests::IsIn(TEXT("Main Street")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

#endif
