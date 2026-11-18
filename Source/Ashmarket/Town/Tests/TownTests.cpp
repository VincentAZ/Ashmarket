// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GutStableTypes.h"
#include "GutCrowd.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GutGameInstance.h"
#include "GutSound.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformNonRealtime.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationCommon.h"
#include "Tests/GutTestHelpers.h"
#include "TownGameMode.h"

/**
 *  Tests of the street outside the cage. They play in the street's own level, and where they go through a door, in the cage's.
 *
 *  Ashmarket.Town.Street   the level opens with the player at ease in it and the people in their places; walking and
 *                          jogging cover ground; the edge of the ground holds.
 *  Ashmarket.Town.Talk     somebody in reach can be spoken to, answered and left; a shut door says so; the game can be stopped.
 *  Ashmarket.Town.Doors    the ways in open the cage as what they say, and the cage's way out opens the street.
 *  Ashmarket.Town.Square   Block Square on a sale night: who stands where, the lender, the clerk, and the walk to Pool Street and back.
 *  Ashmarket.Look.Square   fixed views of the square saved as pictures. Not part of a test run.
 *  Ashmarket.Film.Sale     the sale filmed: a lot walked off the Block, the rest after it, and all six brought back up.
 *  Ashmarket.Probe.Bet, .Feet, .Square, .Deck   measurements, not tests: the window's card by the keys themselves, what the
 *                          feet do, the square's frame rate, how high what stands in the square is along a line. Not part of a test run.
 *  Ashmarket.Look.Street   fixed views of the street saved as pictures, with nothing drawn over them, then three as it is
 *                          played: walking, in reach of somebody, and talking. Not part of a test run.
 */
namespace TownTests
{
	const TCHAR* MapPath = TEXT("/Game/Town/Lvl_PoolStreet");

	ATownGameMode* GetTown()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();

		return World ? World->GetAuthGameMode<ATownGameMode>() : nullptr;
	}

	UGutGameInstance* GetRun()
	{
		const UWorld* World = AutomationCommon::GetAnyGameWorld();

		return World ? Cast<UGutGameInstance>(World->GetGameInstance()) : nullptr;
	}

	/** True once the street is open with everybody stood in it */
	bool IsStreetReady()
	{
		const ATownGameMode* Town = GetTown();

		return Town && Town->IsSettled() && Town->GetWalker();
	}

	/** Stands the player at a place in the level, on the ground it is already on, facing along a yaw */
	void StandAt(const FVector2D& Where, float Yaw = 0.0f)
	{
		if (AGutFighter* Walker = GetTown() ? GetTown()->GetWalker() : nullptr)
		{
			Walker->SetActorLocationAndRotation(FVector(Where.X, Where.Y, Walker->GetActorLocation().Z), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	/** Stands the player a little way off something at a place, on the side the street is on */
	void StandBy(const FVector& Thing, const FVector2D& Off)
	{
		StandAt(FVector2D(Thing.X + Off.X, Thing.Y + Off.Y), FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw);
	}

	/**
	 *  Steps run one after another. Each waits for its own condition (if it has one) and then for its delay, in the game's
	 *  own time, so the script keeps step with the game however fast the machine is and whichever level is open.
	 */
	class FScript : public IAutomationLatentCommand
	{
	public:

		struct FStep
		{
			float Delay = 0.0f;
			TFunction<void()> Do;

			/** Waited for before the delay starts. Run every frame until it is true, so it can also do something each frame. */
			TFunction<bool()> Until;
		};

		FScript(FAutomationTestBase* InTest, TArray<FStep> InSteps)
			: Test(InTest), Steps(MoveTemp(InSteps))
		{
		}

		virtual bool Update() override
		{
			if (!Steps.IsValidIndex(Index))
			{
				return true;
			}

			const UWorld* World = AutomationCommon::GetAnyGameWorld();

			if (!World || !World->HasBegunPlay())
			{
				return GiveUpIfLate(TEXT("no level was open"));
			}

			FStep& Step = Steps[Index];

			if (Step.Until && !bConditionMet)
			{
				if (!Step.Until())
				{
					return GiveUpIfLate(TEXT("what it was waiting for never happened"));
				}

				bConditionMet = true;
			}

			Waited += World->GetDeltaSeconds();

			if (Waited < Step.Delay)
			{
				return false;
			}

			if (Step.Do)
			{
				Step.Do();
			}

			++Index;
			Waited = 0.0f;
			bConditionMet = false;
			StepStarted = FPlatformTime::Seconds();

			return !Steps.IsValidIndex(Index);
		}

	private:

		/** A step that has not got anywhere in a minute of the clock on the wall fails the test and ends the script */
		bool GiveUpIfLate(const TCHAR* Why)
		{
			if (FPlatformTime::Seconds() - StepStarted < 60.0)
			{
				return false;
			}

			Test->AddError(FString::Printf(TEXT("Step %d of the script was given up on: %s"), Index + 1, Why));

			return true;
		}

		FAutomationTestBase* Test = nullptr;
		TArray<FStep> Steps;
		int32 Index = 0;
		float Waited = 0.0f;
		bool bConditionMet = false;
		double StepStarted = FPlatformTime::Seconds();
	};

	bool OpenStreet(FAutomationTestBase* Test)
	{
		if (!AutomationOpenMap(MapPath, true))
		{
			Test->AddError(TEXT("Could not open the street's level"));
			return false;
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownStreetTest, "Ashmarket.Town.Street", GutTests::Flags)

bool FTownStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	struct FNotes
	{
		FVector Start = FVector::ZeroVector;
		float WalkTime = 0.0f;

		/** How many footfalls had been heard when the walker last set off */
		int32 Footfalls = 0;

		/** Where the player came into the street, where it was a frame ago, and how long it has been standing still */
		FVector Came = FVector::ZeroVector;
		FVector Last = FVector::ZeroVector;
		float Still = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [this, Notes]()
	{
		const ATownGameMode* Town = GetTown();
		AGutFighter* Walker = Town->GetWalker();
		const UTownPlaceData* Place = Town->GetPlace();

		TestNotNull(TEXT("The street has a list of who and what is in it"), Place);

		if (!Place)
		{
			return;
		}

		TestTrue(TEXT("The player is at ease"), Walker->IsAtEase());
		TestNull(TEXT("And carries nothing"), Walker->GetWeapon());
		TestTrue(TEXT("And cannot be made to fight"), !Walker->TryLightAttack());
		TestEqual(TEXT("Everybody the list names is standing in the street"), Town->GetPeople().Num(), Place->People.Num());
		TestTrue(TEXT("There is somebody to talk to"), Place->People.Num() >= 3);
		TestTrue(TEXT("There are ways out"), Place->Doors.Num() >= 3);

		for (int32 i = 0; i < Town->GetPeople().Num(); ++i)
		{
			const AGutFighter* Person = Town->GetPeople()[i];

			TestNotNull(*FString::Printf(TEXT("%s is there"), *Place->People[i].Name), Person);

			if (Person)
			{
				TestTrue(*FString::Printf(TEXT("%s is at ease"), *Place->People[i].Name), Person->IsAtEase());
				const FVector Off = Person->GetActorLocation() - Place->People[i].Place;

				TestTrue(*FString::Printf(TEXT("%s stands where the list says (%.0f north and %.0f east of it)"), *Place->People[i].Name, Off.X, Off.Y), Off.Size2D() < 60.0f);
			}
		}

		Notes->Start = Walker->GetActorLocation();
		Notes->Came = Notes->Start;
		Notes->Footfalls = UGutSoundSubsystem::Get(Walker)->GetTimesPlayed(UGutSoundSubsystem::Get(Walker)->GetFootfall());

		GutTests::Screenshot(TEXT("70_street"));
	}, []() { return IsStreetReady(); } });

	// west along the street at a walk for four seconds: about six metres
	Steps.Add({ 0.0f, [this, Notes]()
	{
		const AGutFighter* Walker = GetTown()->GetWalker();
		const float Covered = Notes->Start.Y - Walker->GetActorLocation().Y;

		TestTrue(*FString::Printf(TEXT("Four seconds at a walk covers four to seven metres (covered %.0f cm)"), Covered), Covered > 400.0f && Covered < 700.0f);
		TestTrue(TEXT("And the walker faces the way it is going"), FMath::Abs(FMath::FindDeltaAngleDegrees(Walker->GetActorRotation().Yaw, -90.0f)) < 25.0f);

		// feet are only there to be heard where there is a body to have them
		const int32 Heard = UGutSoundSubsystem::Get(Walker)->GetTimesPlayed(UGutSoundSubsystem::Get(Walker)->GetFootfall());

		if (Walker->HasBody())
		{
			// a footfall to a step where the body is drawn. Where nothing is drawn the body is not posed as often as it is
			// seen to move, and all that can be asked is that some are heard.
			const int32 Least = FApp::CanEverRender() ? 7 : 3;

			TestTrue(*FString::Printf(TEXT("Its feet are heard on the street as it walks (%d footfalls in four seconds)"), Heard - Notes->Footfalls), Heard - Notes->Footfalls >= Least && Heard - Notes->Footfalls <= 10);
		}

		Notes->Footfalls = Heard;
		Notes->Start = Walker->GetActorLocation();
		Notes->WalkTime = 0.0f;
	}, [Notes]()
	{
		AGutFighter* Walker = GetTown()->GetWalker();

		Walker->AddMovementInput(FVector(0.0f, -1.0f, 0.0f), 1.0f);

		Notes->WalkTime += Walker->GetWorld()->GetDeltaSeconds();

		return Notes->WalkTime >= 4.0f;
	} });

	// and standing still for a second after it: a walker that has stopped is not heard taking another step
	Steps.Add({ 1.0f, [this, Notes]()
	{
		const AGutFighter* Walker = GetTown()->GetWalker();
		const int32 Heard = UGutSoundSubsystem::Get(Walker)->GetTimesPlayed(UGutSoundSubsystem::Get(Walker)->GetFootfall());

		TestEqual(TEXT("No footfall is heard from a walker that has stopped"), Heard, Notes->Footfalls);

		Notes->Start = Walker->GetActorLocation();
	} });

	// and at a jog
	Steps.Add({ 0.0f, [this, Notes]()
	{
		AGutFighter* Walker = GetTown()->GetWalker();
		const float Covered = Notes->Start.Y - Walker->GetActorLocation().Y;

		TestTrue(*FString::Printf(TEXT("Four seconds at a jog covers eleven to fifteen metres (covered %.0f cm)"), Covered), Covered > 1100.0f && Covered < 1500.0f);

		const int32 Heard = UGutSoundSubsystem::Get(Walker)->GetTimesPlayed(UGutSoundSubsystem::Get(Walker)->GetFootfall());

		if (Walker->HasBody())
		{
			TestTrue(*FString::Printf(TEXT("And as it jogs (%d footfalls in four seconds)"), Heard - Notes->Footfalls), Heard - Notes->Footfalls >= (FApp::CanEverRender() ? 10 : 4) && Heard - Notes->Footfalls <= 13);
		}

		Walker->SetJogging(false);

		// put down off the end of the street, the walker is brought back onto it
		StandAt(FVector2D(Walker->GetActorLocation().X, -12000.0f));
	}, [Notes]()
	{
		AGutFighter* Walker = GetTown()->GetWalker();

		Walker->SetJogging(true);
		Walker->AddMovementInput(FVector(0.0f, -1.0f, 0.0f), 1.0f);

		Notes->WalkTime += Walker->GetWorld()->GetDeltaSeconds();

		return Notes->WalkTime >= 4.0f;
	} });

	Steps.Add({ 0.3f, [this, Notes]()
	{
		const ATownGameMode* Town = GetTown();
		const FVector Here = Town->GetWalker()->GetActorLocation();

		bool bOnGround = false;

		for (const FBox2D& Patch : Town->GetPlace()->Ground)
		{
			bOnGround |= Patch.ExpandBy(1.0f).IsInside(FVector2D(Here.X, Here.Y));
		}

		TestTrue(TEXT("The edge of the ground holds: nobody walks off the end of the street"), bOnGround);
		TestTrue(TEXT("The place knows what stands on its ground"), Town->GetPlace()->Blocks.Num() > 20);

		// back to where the street is come into, for walking at what stands along it
		StandAt(FVector2D(Notes->Came.X, Notes->Came.Y));
		Notes->WalkTime = 0.0f;
	} });

	// north, east, south and west from there until something is in the way: each time it is something that stands there.
	// Nobody is stopped by nothing.
	static const FVector Ways[] = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f) };
	static const TCHAR* WayNames[] = { TEXT("north"), TEXT("east"), TEXT("south"), TEXT("west") };

	for (int32 Way = 0; Way < 4; ++Way)
	{
		Steps.Add({ 0.0f, [this, Notes, Way]()
		{
			const ATownGameMode* Town = GetTown();
			const FVector Here = Town->GetWalker()->GetActorLocation();

			float Nearest = TNumericLimits<float>::Max();

			for (const FTownBlock& Block : Town->GetPlace()->Blocks)
			{
				Nearest = FMath::Min(Nearest, Block.GetDistance(FVector2D(Here.X, Here.Y)));
			}

			// the people of the street stop a walker too
			for (const AGutFighter* Person : Town->GetPeople())
			{
				Nearest = FMath::Min(Nearest, Person ? FVector::Dist2D(Person->GetActorLocation(), Here) - 40.0f : Nearest);
			}

			TestTrue(*FString::Printf(TEXT("Going %s, what stops the walker is something standing there (the nearest thing is %.0f cm off)"), WayNames[Way], Nearest),
				Nearest < Town->GetWalkerRadius() + 12.0f);

			StandAt(FVector2D(Notes->Came.X, Notes->Came.Y));
			Notes->WalkTime = 0.0f;
			Notes->Still = 0.0f;
		}, [Notes, Way]()
		{
			AGutFighter* Walker = GetTown()->GetWalker();
			const float Step = Walker->GetWorld()->GetDeltaSeconds();

			Walker->SetJogging(true);
			Walker->AddMovementInput(Ways[Way], 1.0f);

			// stopped is not having moved for half a second
			Notes->Still = FVector::Dist2D(Walker->GetActorLocation(), Notes->Last) < 40.0f * Step ? Notes->Still + Step : 0.0f;
			Notes->Last = Walker->GetActorLocation();
			Notes->WalkTime += Step;

			return Notes->Still >= 0.5f && Notes->WalkTime > 1.0f;
		} });
	}

	Steps.Add({ 0.1f, []()
	{
		GetTown()->GetWalker()->SetJogging(false);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownTalkTest, "Ashmarket.Town.Talk", GutTests::Flags)

bool FTownTalkTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	TArray<FScript::FStep> Steps;

	// up to the first person on the list, who has something to be answered
	Steps.Add({ 0.5f, []()
	{
		StandBy(GetTown()->GetPlace()->People[0].Place, FVector2D(0.0f, -160.0f));
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.3f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const FTownPerson& Person = Town->GetPlace()->People[0];

		TestEqual(TEXT("Standing by somebody puts them in reach"), Town->GetPersonInReach(), 0);
		TestTrue(TEXT("And the player is offered a word with them"), Town->GetPrompt().Contains(Person.Name));

		Town->Interact();

		TestTrue(TEXT("The key starts the talk"), Town->IsTalking());
		TestEqual(TEXT("With them"), Town->GetSpeaker(), Person.Name);
		TestEqual(TEXT("At their first line"), Town->GetWords(), Person.Lines[0].Words);
		TestEqual(TEXT("With every answer they wait for"), Town->GetAnswers().Num(), Person.Lines[0].Answers.Num());
		TestTrue(TEXT("Nothing is offered while they are talking"), Town->GetPrompt().IsEmpty());
		TestTrue(TEXT("And the player stands still to listen"), UGameplayStatics::GetPlayerController(Town, 0)->IsMoveInputIgnored());
	} });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const FTownPerson& Person = Town->GetPlace()->People[0];

		GutTests::Screenshot(TEXT("71_street_talk"));

		// the answer that leads to another line and does nothing else, if they have one: the second, on the list as it stands
		int32 Asking = INDEX_NONE;

		for (int32 i = 0; i < Person.Lines[0].Answers.Num(); ++i)
		{
			if (Person.Lines[0].Answers[i].Deed == ETownDeed::None && Person.Lines[0].Answers[i].Next >= 0)
			{
				Asking = i;
				break;
			}
		}

		if (!TestTrue(TEXT("The first person has a question that can be asked"), Asking != INDEX_NONE))
		{
			return;
		}

		for (int32 i = 0; i < Asking; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		TestEqual(TEXT("Down moves along the answers"), Town->GetTalkChoice(), Asking);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("Asking keeps them talking"), Town->IsTalking());
		TestEqual(TEXT("And gets the line that answers it"), Town->GetWords(), Person.Lines[Person.Lines[0].Answers[Asking].Next].Words);

		// a line with nothing to answer ends when the player is ready
		Town->Interact();

		TestTrue(TEXT("A line with nothing after it ends the talk"), !Town->IsTalking());
		TestTrue(TEXT("And the player can walk again"), !UGameplayStatics::GetPlayerController(Town, 0)->IsMoveInputIgnored());

		// stopping the game, and starting it again from its own menu
		Town->SetStopped(true);

		TestTrue(TEXT("The game can be stopped in the street"), Town->IsStopped() && UGameplayStatics::IsGamePaused(Town));
		TestEqual(TEXT("With a menu"), Town->GetStoppedLines().Num(), TownStoppedLines::Count);

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("And its first line starts it again"), !Town->IsStopped() && !UGameplayStatics::IsGamePaused(Town));

		// a door that is shut: the first on the list that does nothing
		for (const FTownDoor& Door : Town->GetPlace()->Doors)
		{
			if (Door.Deed == ETownDeed::None)
			{
				StandAt(FVector2D(Door.Place.X, Door.Place.Y));
				break;
			}
		}
	} });

	Steps.Add({ 0.3f, [this]()
	{
		ATownGameMode* Town = GetTown();

		if (!TestTrue(TEXT("Standing at a door puts it in reach"), Town->GetPlace()->Doors.IsValidIndex(Town->GetDoorInReach())))
		{
			return;
		}

		const FTownDoor& Door = Town->GetPlace()->Doors[Town->GetDoorInReach()];

		TestEqual(TEXT("And the player is offered it"), Town->GetPrompt(), Door.Prompt);

		Town->Interact();

		TestEqual(TEXT("A shut door says why it is shut"), Town->GetNotice(), Door.Shut);
		TestNotNull(TEXT("And the player is still in the street"), GetTown());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownDoorsTest, "Ashmarket.Town.Doors", GutTests::Flags)

bool FTownDoorsTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	/** Talks to whoever has an answer that does a deed, and gives that answer */
	const auto Say = [this](ETownDeed Deed)
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		for (int32 Person = 0; Person < Place->People.Num(); ++Person)
		{
			if (Place->People[Person].Lines.IsEmpty())
			{
				continue;
			}

			const TArray<FTownAnswer>& Answers = Place->People[Person].Lines[0].Answers;

			for (int32 Answer = 0; Answer < Answers.Num(); ++Answer)
			{
				if (Answers[Answer].Deed != Deed)
				{
					continue;
				}

				// to their west, which for everybody in this street is ground or the nearest thing to it
				StandBy(Place->People[Person].Place, FVector2D(0.0f, -160.0f));

				// whoever stands off the ground is put back on it, and who is in reach is worked out, once a frame: now
				Town->Tick(0.0f);

				TestEqual(*FString::Printf(TEXT("%s is in reach"), *Place->People[Person].Name), Town->GetPersonInReach(), Person);

				Town->Interact();

				for (int32 i = 0; i < Answer; ++i)
				{
					Town->HandleMenuInput(EGutMenuInput::Down);
				}

				Town->HandleMenuInput(EGutMenuInput::Confirm);
				return;
			}
		}

		AddError(TEXT("Nobody in the street has that answer to give"));
	};

	const auto InCage = []()
	{
		return GutTests::GetFight().IsValid();
	};

	TArray<FScript::FStep> Steps;

	// a single fight
	Steps.Add({ 0.5f, [Say]()
	{
		// in a test the street is the old card unless the test asks for the place
		GetRun()->bStreetForced = true;

		Say(ETownDeed::Spar);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.5f, [this]()
	{
		const UGutGameInstance* Run = GetRun();
		const GutTests::FFight Fight = GutTests::GetFight();

		TestTrue(TEXT("Asking for a spar opens the cage for a single fight"), Run->bSandbox && !Run->bWindow && !Run->bStable && !Run->bAtDoor);
		TestTrue(TEXT("And the cage remembers that the player came from the street"), Run->bCameByDoor);
		TestEqual(TEXT("It opens at the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestTrue(TEXT("In the cage the player is on its guard again"), !Fight.Player->IsAtEase());

		// and the way out of the cage is the street
		Fight.GameMode->GoToDoor();
	}, InCage });

	// to bet
	Steps.Add({ 0.5f, [this, Say]()
	{
		TestNotNull(TEXT("The cage's way out to the street opens the street"), GetTown());

		Say(ETownDeed::Bet);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.5f, [this]()
	{
		const UGutGameInstance* Run = GetRun();
		const GutTests::FFight Fight = GutTests::GetFight();

		TestTrue(TEXT("Putting water on the card opens the cage at the window"), Run->bWindow && !Run->bSandbox && !Run->bStable);
		TestTrue(TEXT("Where the player watches and does not fight"), Fight.GameMode->IsAtWindow());

		// the fight is watched with no bet, and is over as soon as it begins
		for (int32 i = 0; i < 10 && Fight.GameMode->IsTellingStory(); ++i)
		{
			Fight.GameMode->HandleMenuInput(EGutMenuInput::Confirm);
		}

		for (int32 i = 0; i < 8; ++i)
		{
			Fight.GameMode->MoveCardSelection(-1);
		}

		Fight.GameMode->ActivateCardSelection();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		TestEqual(TEXT("They fight"), Fight.GameMode->GetPhase(), EGutFightPhase::Fighting);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	}, InCage });

	// and from the end of a fight that was watched, the street is one of the two ways on
	Steps.Add({ 0.4f, [this]()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		TestFalse(TEXT("The arrow begins on the next fight"), Fight.GameMode->IsLeaveChosen());

		Fight.GameMode->HandleMenuInput(EGutMenuInput::Down);

		TestTrue(TEXT("Down moves it to the street"), Fight.GameMode->IsLeaveChosen());

		GutTests::Screenshot(TEXT("45_window_next_or_the_street"));
	}, [InCage]() { return InCage() && GutTests::GetFight().GameMode->CanLeaveForStreet(); } });

	Steps.Add({ 0.4f, []() { GutTests::GetFight().GameMode->RequestRestart(); } });

	// as an owner
	Steps.Add({ 0.5f, [this, Say]()
	{
		TestNotNull(TEXT("Enter with the arrow on the street goes back down to it"), GetTown());
		TestEqual(TEXT("And the bill is one fight further on for the next time"), GetRun()->Window.Fight, 1);

		Say(ETownDeed::Own);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.5f, [this]()
	{
		const UGutGameInstance* Run = GetRun();
		const GutTests::FFight Fight = GutTests::GetFight();

		TestTrue(TEXT("An owner is walked round to the yard"), Run->bStable && !Run->bSandbox && !Run->bWindow);
		TestTrue(TEXT("Which is the cage as an owner has it"), Fight.GameMode->IsAtStable());

		Fight.GameMode->GoToDoor();
	}, InCage });

	// and to fight, by the door itself
	Steps.Add({ 0.5f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		for (const FTownDoor& Door : Place->Doors)
		{
			if (Door.Deed == ETownDeed::Fight)
			{
				StandAt(FVector2D(Door.Place.X - 60.0f, Door.Place.Y));

				Town->Tick(0.0f);

				TestTrue(TEXT("The fighters' door is in reach"), Place->Doors.IsValidIndex(Town->GetDoorInReach()) && Place->Doors[Town->GetDoorInReach()].Deed == ETownDeed::Fight);

				// whatever ran before this in the same sitting may have climbed the ladder: this is somebody at its foot
				GetRun()->Ladder = FGutLadderState();
				GetRun()->LadderAtRungStart = GetRun()->Ladder;

				Town->Interact();
				return;
			}
		}

		AddError(TEXT("The street has no door that leads to the fights"));
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.5f, [this]()
	{
		UGutGameInstance* Run = GetRun();
		const GutTests::FFight Fight = GutTests::GetFight();

		TestTrue(TEXT("The fighters' door opens the cage on the ladder"), !Run->bSandbox && !Run->bWindow && !Run->bStable && !Run->bAtDoor);
		TestEqual(TEXT("At the card"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);

		// to the end of Meat by the short way: three wins behind it, and the cage opened again, which is then at the gate
		Run->Ladder.Rung = 3;
		Run->Ladder.bFinished = true;
		Run->LadderAtRungStart = Run->Ladder;

		UGameplayStatics::OpenLevel(Fight.GameMode, FName(TEXT("/Game/Gut/Lvl_Gut")));
	}, InCage });

	Steps.Add({ 0.5f, []() { GutTests::GetFight().GameMode->LeaveGate(); }, [InCage]()
	{
		return InCage() && GutTests::GetFight().GameMode->GetPhase() == EGutFightPhase::Gate;
	} });

	Steps.Add({ 0.3f, [this]()
	{
		UGutGameInstance* Run = GetRun();

		TestEqual(TEXT("Out of the gate at the end of Meat the player is a Dog"), Run->Ladder.Rank, 1);
		TestNotNull(TEXT("And is standing in the street"), GetTown());

		// whatever else runs after this in the same sitting gets the street it would have had, and a ladder not yet climbed
		Run->Ladder = FGutLadderState();
		Run->LadderAtRungStart = Run->Ladder;
		Run->bStreetForced.Reset();
	}, []() { return IsStreetReady(); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookStreetTest, "Ashmarket.Look.Street", GutTests::Flags)

bool FTownLookStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	/** A view of the street: where the camera stands and what it looks at, in the level, and how wide it sees */
	struct FView
	{
		const TCHAR* Name;
		FVector From;
		FVector At;
		float FieldOfView;
	};

	// the level's north is +X and its east is +Y; the street's ground is at 225
	static const FView Views[] =
	{
		{ TEXT("street_01_from_the_door"), FVector(-3180.0f, -150.0f, 395.0f), FVector(-3280.0f, -2400.0f, 330.0f), 70.0f },
		{ TEXT("street_02_the_lockers"), FVector(-3560.0f, -700.0f, 400.0f), FVector(-2850.0f, 150.0f, 400.0f), 70.0f },
		{ TEXT("street_03_down_the_alley"), FVector(-3200.0f, -1700.0f, 400.0f), FVector(-1000.0f, -1650.0f, 380.0f), 65.0f },
		{ TEXT("street_04_the_window"), FVector(-1250.0f, -1790.0f, 430.0f), FVector(-1010.0f, -1360.0f, 400.0f), 70.0f },
		{ TEXT("street_05_the_main_street_end"), FVector(-3300.0f, -7400.0f, 400.0f), FVector(-3300.0f, -8500.0f, 380.0f), 65.0f },
		{ TEXT("street_06_from_above"), FVector(-5200.0f, -2500.0f, 2600.0f), FVector(-2600.0f, -600.0f, 225.0f), 60.0f },
		{ TEXT("street_10_the_row"), FVector(-3480.0f, -5500.0f, 400.0f), FVector(-2980.0f, -4000.0f, 380.0f), 70.0f },
		{ TEXT("street_11_the_alleys_west_side"), FVector(-1700.0f, -1540.0f, 420.0f), FVector(-1500.0f, -1940.0f, 330.0f), 75.0f },
		{ TEXT("street_12_the_scrappers_end"), FVector(-3300.0f, 900.0f, 400.0f), FVector(-3250.0f, 2300.0f, 380.0f), 70.0f },
		{ TEXT("street_13_the_hoarding"), FVector(-3150.0f, -2500.0f, 395.0f), FVector(-3625.0f, -3300.0f, 400.0f), 70.0f },
		{ TEXT("street_14_the_bills"), FVector(-3400.0f, -3700.0f, 395.0f), FVector(-3625.0f, -3800.0f, 410.0f), 70.0f },
		{ TEXT("street_15_the_fighters_door"), FVector(-3140.0f, -150.0f, 395.0f), FVector(-2800.0f, 0.0f, 340.0f), 65.0f },
		{ TEXT("street_16_the_public_gate"), FVector(-950.0f, -1750.0f, 464.0f), FVector(-640.0f, -1300.0f, 430.0f), 70.0f },
	};

	TArray<FScript::FStep> Steps;

	for (const FView& View : Views)
	{
		Steps.Add({ 0.3f, [View]()
		{
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;

			if (!Viewer)
			{
				return;
			}

			ACameraActor* Camera = World->SpawnActor<ACameraActor>(View.From, (View.At - View.From).Rotation());

			Camera->GetCameraComponent()->SetFieldOfView(View.FieldOfView);
			Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

			Viewer->bAutoManageActiveCameraTarget = false;
			Viewer->SetViewTarget(Camera);

			if (AHUD* HUD = Viewer->GetHUD())
			{
				HUD->bShowHUD = false;
			}
		}, []() { return IsStreetReady(); } });

		// long enough for the picture to settle after the cut
		Steps.Add({ 1.4f, [View]() { GutTests::Screenshot(View.Name); } });
	}

	// then the street as whoever plays it sees it: their own camera, and everything the screen draws over it
	Steps.Add({ 0.3f, []()
	{
		APlayerController* Viewer = AutomationCommon::GetAnyGameWorld()->GetFirstPlayerController();

		Viewer->bAutoManageActiveCameraTarget = true;
		Viewer->SetViewTarget(Viewer->GetPawn());

		if (AHUD* HUD = Viewer->GetHUD())
		{
			HUD->bShowHUD = true;
		}
	} });

	TSharedRef<float> Walked = MakeShared<float>(0.0f);

	Steps.Add({ 0.0f, []() { GutTests::Screenshot(TEXT("street_07_as_played_walking")); }, [Walked]()
	{
		AGutFighter* Walker = GetTown()->GetWalker();

		Walker->AddMovementInput(FVector(0.0f, -1.0f, 0.0f), 1.0f);
		*Walked += Walker->GetWorld()->GetDeltaSeconds();

		return *Walked > 2.2f;
	} });

	// by the second on the list, who keeps the door
	Steps.Add({ 0.6f, []()
	{
		const FVector2D Off(-40.0f, -170.0f);

		StandBy(GetTown()->GetPlace()->People[1].Place, Off);

		// having walked up to them, not been dropped beside them with the camera still looking down the street
		AutomationCommon::GetAnyGameWorld()->GetFirstPlayerController()->SetControlRotation(FRotator(-8.0f, FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw - 25.0f, 0.0f));
	} });
	Steps.Add({ 1.6f, []() { GutTests::Screenshot(TEXT("street_08_as_played_in_reach")); } });
	Steps.Add({ 0.5f, []() { GetTown()->Interact(); } });
	Steps.Add({ 1.8f, []() { GutTests::Screenshot(TEXT("street_09_as_played_talking")); } });
	Steps.Add({ 0.5f, []() {} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

namespace TownTests
{
	/**
	 *  Walks the player round the street as somebody playing would, and saves every other frame to Saved/GutFilm, with
	 *  what is heard. The picture is the game's own, with everything the game draws over it.
	 */
	class FFilmWalkCommand : public IAutomationLatentCommand
	{
	public:

		/** A place to walk to, in the map's own metres, and what is done on the way and on getting there */
		struct FLeg
		{
			float East = 0.0f;
			float North = 0.0f;
			bool bJog = false;

			/** Stood still for this long on getting there */
			float Wait = 0.0f;

			/** Where the camera looks while standing there, as a compass bearing. Below zero, it is left where it was. */
			float Look = -1.0f;
		};

		FFilmWalkCommand(FAutomationTestBase* InTest, TArray<FLeg> InLegs, bool bInSound)
			: Test(InTest), Legs(MoveTemp(InLegs)), bSound(bInSound)
		{
		}

		virtual bool Update() override
		{
			if (!IsStreetReady())
			{
				return false;
			}

			ATownGameMode* Town = GetTown();
			UWorld* World = Town->GetWorld();
			AGutFighter* Walker = Town->GetWalker();
			APlayerController* Viewer = World->GetFirstPlayerController();
			const float Step = World->GetDeltaSeconds();

			if (!Viewer)
			{
				return true;
			}

			if (!bStarted)
			{
				bStarted = true;

				// what is heard is kept from the same moment as what is seen, a frame's worth for each frame, as for a fight
				if (bSound && World->GetAudioDeviceRaw())
				{
					if (IConsoleVariable* Itself = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick")))
					{
						Itself->Set(0);
					}

					UAudioMixerBlueprintLibrary::StartRecordingOutput(World, 300.0f);

					bRecording = true;
				}
			}

			if (bRecording)
			{
				Audio::FMixerDevice* Mixer = static_cast<Audio::FMixerDevice*>(World->GetAudioDeviceRaw());
				Audio::IAudioMixerPlatformInterface* Platform = Mixer ? Mixer->GetAudioMixerPlatform() : nullptr;

				if (Platform && Platform->IsNonRealtime())
				{
					static_cast<Audio::FMixerPlatformNonRealtime*>(Platform)->RenderAudio(Step);
				}
			}

			Time += Step;

			// a moment standing where the game begins, then each leg in turn
			if (Time > 1.5f && Legs.IsValidIndex(Leg))
			{
				const FLeg& To = Legs[Leg];
				const FVector Here = Walker->GetActorLocation();

				// the map's metres are the level's centimetres from the pool's own corner: north is the level's X and east its Y
				const FVector There((To.North - 208.0f) * 100.0f, (To.East - 95.0f) * 100.0f, Here.Z);
				const FVector Way = (There - Here).GetSafeNormal2D();

				if (Waited <= 0.0f && FVector::Dist2D(Here, There) > 35.0f && LegTime < 30.0f)
				{
					LegTime += Step;

					Walker->SetJogging(To.bJog);
					Walker->AddMovementInput(Way, 1.0f);

					// the camera comes round behind, as a hand on the mouse brings it
					Turn(Viewer, Way.Rotation().Yaw, Step, 1.6f);
				}
				else
				{
					Walker->SetJogging(false);

					Waited += Step;

					if (To.Look >= 0.0f)
					{
						Turn(Viewer, To.Look, Step, 2.2f);
					}

					if (Waited >= To.Wait)
					{
						++Leg;
						Waited = 0.0f;
						LegTime = 0.0f;
					}
				}
			}

			// at the end of the last leg, whoever is in reach is spoken to, and heard out for two of their lines
			if (!Legs.IsValidIndex(Leg))
			{
				EndTime += Step;

				if (!Town->IsTalking() && !bSpoke && EndTime > 1.2f && !Town->GetPrompt().IsEmpty())
				{
					bSpoke = true;

					Town->Interact();
				}

				if (bSpoke && !bWentOn && EndTime > 6.5f && Town->GetAnswers().Num() <= 1)
				{
					bWentOn = true;

					Town->HandleMenuInput(EGutMenuInput::Confirm);
				}
			}

			if (Frame++ % 2 == 0)
			{
				// read straight from the game's picture, as the fight films are
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm") / FString::Printf(TEXT("frame_%05d.png"), Saved++)), false, false);
			}

			const bool bDone = EndTime > 12.0f || Time > 150.0f;

			if (bDone && bRecording)
			{
				bRecording = false;

				UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(World, EAudioRecordingExportType::WavFile, TEXT("sound"),
					FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm")));
			}

			if (bDone)
			{
				UE_LOG(LogTemp, Display, TEXT("[film] the walk: %d legs of %d walked, %s, %.0f seconds"), Leg, Legs.Num(), bSpoke ? TEXT("somebody spoken to") : TEXT("nobody spoken to"), Time);
			}

			return bDone;
		}

	private:

		static void Turn(APlayerController* Viewer, float Yaw, float Step, float Speed)
		{
			Viewer->SetControlRotation(FMath::RInterpTo(Viewer->GetControlRotation(), FRotator(-9.0f, Yaw, 0.0f), Step, Speed));
		}

		FAutomationTestBase* Test = nullptr;

		TArray<FLeg> Legs;

		bool bSound = false;
		bool bStarted = false;
		bool bRecording = false;
		bool bSpoke = false;
		bool bWentOn = false;

		int32 Leg = 0;
		int32 Frame = 0;
		int32 Saved = 0;

		float Time = 0.0f;
		float LegTime = 0.0f;
		float Waited = 0.0f;
		float EndTime = 0.0f;
	};
}

/**
 *  Ashmarket.Film.Street   the player walks from where the game begins to the fighters' door, back along the Hoarding, up
 *                          Cold Alley and to the window, and speaks to Chalk. Tools/film_town_walk.sh runs it and joins
 *                          the frames into for-review/walk_pool_street.mp4.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownFilmStreetTest, "Ashmarket.Film.Street", GutTests::Flags)

bool FTownFilmStreetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using FLeg = FFilmWalkCommand::FLeg;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenStreet(this))
	{
		return false;
	}

	TArray<FLeg> Legs;

	// to the fighters' door, and a look at it
	Legs.Add({ 94.4f, 177.7f, false, 2.5f, 15.0f });

	// west along the street, the Hoarding on the left hand, part of it at a jog
	Legs.Add({ 90.0f, 175.2f, false, 0.0f });
	Legs.Add({ 84.0f, 174.9f, true, 0.0f });
	Legs.Add({ 79.2f, 175.0f, false, 2.0f, 200.0f });

	// up Cold Alley
	Legs.Add({ 78.3f, 181.0f, false, 0.0f });
	Legs.Add({ 78.3f, 189.0f, true, 0.0f });
	Legs.Add({ 78.5f, 196.0f, false, 0.0f });

	// and to the window
	Legs.Add({ 79.9f, 198.15f, false, 0.5f, 90.0f });

	ADD_LATENT_AUTOMATION_COMMAND(FFilmWalkCommand(this, MoveTemp(Legs), FParse::Param(FCommandLine::Get(), TEXT("GutFilmSound"))));

	return true;
}

/**
 *  Films the sale from fixed places, as somebody standing in the square sees it: a lot knocked down and walked off, the
 *  rest after it, the file in the Holding Yard, and the six brought back up. A frame is saved for every second frame of
 *  the game, and the sound with it when asked.
 */
namespace TownTests
{
	class FFilmSaleCommand : public IAutomationLatentCommand
	{
	public:

		/** Where the camera stands from a moment on, in the map's own metres, and what it looks at */
		struct FShot
		{
			float From = 0.0f;
			FVector Stand = FVector::ZeroVector;
			FVector At = FVector::ZeroVector;
			float FieldOfView = 60.0f;
		};

		/** A moment at which a lot is knocked down. Nought brings them up. */
		struct FHammer
		{
			float When = 0.0f;
			int32 Lot = 0;
		};

		FFilmSaleCommand(TArray<FShot> InShots, TArray<FHammer> InHammers, float InLength, bool bInSound)
			: Shots(MoveTemp(InShots)), Hammers(MoveTemp(InHammers)), Length(InLength), bSound(bInSound)
		{
		}

		virtual bool Update() override
		{
			if (!IsStreetReady())
			{
				return false;
			}

			ATownGameMode* Town = GetTown();
			UWorld* World = Town->GetWorld();
			APlayerController* Viewer = World->GetFirstPlayerController();
			const UTownPlaceData* Place = Town->GetPlace();
			const float Step = World->GetDeltaSeconds();

			if (!Viewer || !Place)
			{
				return true;
			}

			// the bidders are fetched in the background, and the pictures on everything take a moment to come sharp
			if ((Settling += Step) < 14.0f)
			{
				return false;
			}

			if (!Camera.IsValid())
			{
				Camera = World->SpawnActor<ACameraActor>();

				if (!Camera.IsValid())
				{
					return true;
				}

				Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
				Viewer->SetViewTarget(Camera.Get());

				// what is heard is kept from the same moment as what is seen, a frame's worth for each frame, as for a fight
				if (bSound && World->GetAudioDeviceRaw())
				{
					if (IConsoleVariable* Itself = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick")))
					{
						Itself->Set(0);
					}

					UAudioMixerBlueprintLibrary::StartRecordingOutput(World, 300.0f);

					bRecording = true;
				}
			}

			if (bRecording)
			{
				Audio::FMixerDevice* Mixer = static_cast<Audio::FMixerDevice*>(World->GetAudioDeviceRaw());
				Audio::IAudioMixerPlatformInterface* Platform = Mixer ? Mixer->GetAudioMixerPlatform() : nullptr;

				if (Platform && Platform->IsNonRealtime())
				{
					static_cast<Audio::FMixerPlatformNonRealtime*>(Platform)->RenderAudio(Step);
				}
			}

			Time += Step;

			while (Shots.IsValidIndex(Shot) && Time >= Shots[Shot].From)
			{
				// the level's X is the map's north and its Y the map's east, from the cage's own middle
				const auto InLevel = [](const FVector& OnMap) { return FVector((OnMap.Y - 208.0f) * 100.0f, (OnMap.X - 95.0f) * 100.0f, (OnMap.Z - 3.0f) * 100.0f); };

				const FVector Stand = InLevel(Shots[Shot].Stand);

				Camera->SetActorLocationAndRotation(Stand, (InLevel(Shots[Shot].At) - Stand).Rotation());
				Camera->GetCameraComponent()->SetFieldOfView(Shots[Shot].FieldOfView);

				++Shot;
			}

			while (Hammers.IsValidIndex(Hammer) && Time >= Hammers[Hammer].When)
			{
				const int32 Lot = Hammers[Hammer].Lot;
				const int32 Call = Place->Calls.IndexOfByPredicate([Lot](const FTownCall& One) { return Lot > 0 ? One.Sells == Lot : One.bBringsUp; });

				Town->SkipToCall(Call);

				++Hammer;
			}

			if (Frame++ % 2 == 0)
			{
				// read straight from the game's picture, as the fight films are
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm") / FString::Printf(TEXT("frame_%05d.png"), Saved++)), false, false);
			}

			const bool bDone = Time > Length;

			if (bDone && bRecording)
			{
				bRecording = false;

				UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(World, EAudioRecordingExportType::WavFile, TEXT("sound"),
					FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm")));
			}

			if (bDone)
			{
				UE_LOG(LogTemp, Display, TEXT("[film] the sale: %d shots, %d frames, %.0f seconds, %d lots kept at the end"), Shot, Saved, Time, Town->GetLotsKept());
			}

			return bDone;
		}

	private:

		TArray<FShot> Shots;
		TArray<FHammer> Hammers;
		float Length = 60.0f;
		bool bSound = false;

		TWeakObjectPtr<ACameraActor> Camera;
		float Settling = 0.0f;
		float Time = 0.0f;
		int32 Shot = 0;
		int32 Hammer = 0;
		int32 Frame = 0;
		int32 Saved = 0;
		bool bRecording = false;
	};
}

/**
 *  Ashmarket.Film.Sale     the sale on the Block, from the crowd and from over the Holding Yard's fence. Not part of a test
 *                          run. GUT_FILM_TEST=Ashmarket.Film.Sale Tools/film_town_walk.sh sale_on_the_block
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownFilmSaleTest, "Ashmarket.Film.Sale", GutTests::Flags)

bool FTownFilmSaleTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using FShot = FFilmSaleCommand::FShot;
	using FHammer = FFilmSaleCommand::FHammer;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	const FVector Crowd(21.5f, 143.0f, 6.5f);
	// from over the clerk's end of the square, high enough to see the ramp and the yard over the fence and the truck beds
	const FVector Above(27.0f, 141.0f, 11.5f);
	const FVector Front(14.5f, 150.0f, 6.4f);

	TArray<FShot> Shots;

	// lot one is knocked down and walks behind the others to the head of the deck
	Shots.Add({ 0.0f, Crowd, FVector(30.0f, 148.6f, 6.9f), 50.0f });

	// down the ramp into the Holding Yard
	Shots.Add({ 7.5f, Above, FVector(34.0f, 150.1f, 5.0f), 42.0f });

	// the other five after it, from where the bidders stand
	Shots.Add({ 15.0f, Front, FVector(29.6f, 150.0f, 6.6f), 62.0f });

	// the file, kept
	Shots.Add({ 31.0f, Above, FVector(38.0f, 150.1f, 4.8f), 38.0f });

	// and brought back up
	Shots.Add({ 44.0f, Above, FVector(34.5f, 150.1f, 5.0f), 42.0f });
	Shots.Add({ 52.0f, Front, FVector(29.6f, 150.0f, 6.6f), 62.0f });

	TArray<FHammer> Hammers;

	Hammers.Add({ 1.0f, 1 });

	for (int32 Lot = 2; Lot <= 6; ++Lot)
	{
		Hammers.Add({ 15.0f + ((Lot - 2) * 2.2f), Lot });
	}

	// when the last of them is in the file
	Hammers.Add({ 36.5f, 0 });

	ADD_LATENT_AUTOMATION_COMMAND(FFilmSaleCommand(MoveTemp(Shots), MoveTemp(Hammers), 66.0f, FParse::Param(FCommandLine::Get(), TEXT("GutFilmSound"))));

	return true;
}

/**
 *  Not a test: a measurement. Goes in to bet from the street by the keys themselves, as somebody playing does, with the run
 *  that is on disk if the game was started without -GutNoSave, and says in the log what each key did at the window's card.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeBetTest, "Ashmarket.Probe.Bet", GutTests::Flags)

bool FTownProbeBetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	using FStep = FScript::FStep;

	const auto Key = [](const FKey& Which, bool bPressed)
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();

		if (APlayerController* Hands = World ? World->GetFirstPlayerController() : nullptr)
		{
			Hands->InputKey(FInputKeyEventArgs(nullptr, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), Which, bPressed ? IE_Pressed : IE_Released, 0));
		}
	};

	const auto Cage = []() -> AGutGameMode*
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();

		return World ? World->GetAuthGameMode<AGutGameMode>() : nullptr;
	};

	const auto Say = [Cage](const TCHAR* What)
	{
		if (const AGutGameMode* GameMode = Cage())
		{
			UE_LOG(LogTemp, Log, TEXT("Probe: %s: phase %s, the choice is on type %d line %d, story %d, stopped %d, paused %d"), What,
				*UEnum::GetDisplayValueAsText(GameMode->GetPhase()).ToString(), static_cast<int32>(GameMode->GetCardSelection().Type), GameMode->GetCardSelection().Index,
				GameMode->IsTellingStory() ? 1 : 0, GameMode->IsStopped() ? 1 : 0, UGameplayStatics::IsGamePaused(GameMode) ? 1 : 0);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Probe: %s: not in the cage"), What);
		}
	};

	TArray<FStep> Steps;

	Steps.Add({ 0.5f, []()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		const int32 Chalk = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Name == TEXT("Chalk"); });

		StandBy(Place->People[Chalk].Place, FVector2D(0.0f, -160.0f));
	}, &IsStreetReady });

	Steps.Add({ 0.5f, [Key]() { Key(EKeys::E, true); }, []() { return GetTown() && GetTown()->GetPersonInReach() != INDEX_NONE; } });
	Steps.Add({ 0.1f, [Key]() { Key(EKeys::E, false); } });

	// the first answer is the bet. The key is still down when the cage opens, as a finger's is.
	Steps.Add({ 0.5f, [Key]() { Key(EKeys::Enter, true); }, []() { return GetTown() && GetTown()->IsTalking(); } });
	Steps.Add({ 0.1f, [Key]() { Key(EKeys::Enter, false); } });

	Steps.Add({ 1.0f, [Say]() { Say(TEXT("at the card")); }, [Cage]() { return Cage() && Cage()->GetPhase() == EGutFightPhase::Card && Cage()->GetPlayerFighter(); } });

	Steps.Add({ 0.2f, []() { GutTests::Screenshot(TEXT("probe_0_card")); } });

	int32 Shot = 1;

	for (const FKey& Which : { EKeys::W, EKeys::W, EKeys::Enter, EKeys::S, EKeys::S, EKeys::Gamepad_FaceButton_Bottom, EKeys::D, EKeys::LeftMouseButton, EKeys::Enter })
	{
		Steps.Add({ 0.5f, [Key, Which]() { Key(Which, true); } });
		Steps.Add({ 0.1f, [Key, Which]() { Key(Which, false); } });
		Steps.Add({ 0.3f, [Say, Which, Shot]()
		{
			Say(*FString::Printf(TEXT("after %s"), *Which.ToString()));

			GutTests::Screenshot(FString::Printf(TEXT("probe_%d_after_%s"), Shot, *Which.ToString()));
		} });

		++Shot;
	}

	Steps.Add({ 6.0f, []() { GutTests::Screenshot(TEXT("probe_watching")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Not a test: a measurement. Walks, stops, walks, stops, jogs and stops along the street, for reading what the feet did with
 *  -GutFootLog: when each footfall sounded, and how long after the walker had stopped the last of them came.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeFeetTest, "Ashmarket.Probe.Feet", GutTests::Flags)

bool FTownProbeFeetTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!OpenStreet(this))
	{
		return false;
	}

	using FStep = FScript::FStep;

	TArray<FStep> Steps;

	TSharedRef<float> Clock = MakeShared<float>(0.0f);

	const auto Go = [&Steps, Clock](const TCHAR* What, float Seconds, bool bMove, bool bJog)
	{
		Steps.Add({ 0.0f, [What]()
		{
			const AGutFighter* Walker = GetTown()->GetWalker();

			UE_LOG(LogGut, Log, TEXT("Feet: end of %s at %.3f"), What, Walker->GetWorld()->GetTimeSeconds());
		}, [Clock, Seconds, bMove, bJog, What]()
		{
			AGutFighter* Walker = GetTown()->GetWalker();

			if (*Clock == 0.0f)
			{
				UE_LOG(LogGut, Log, TEXT("Feet: start of %s at %.3f"), What, Walker->GetWorld()->GetTimeSeconds());
			}

			Walker->SetJogging(bJog);

			if (bMove)
			{
				Walker->AddMovementInput(FVector(0.0f, -1.0f, 0.0f), 1.0f);
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

	Steps.Add({ 1.0f, nullptr, &IsStreetReady });

	Go(TEXT("standing"), 3.0f, false, false);
	Go(TEXT("walking"), 5.0f, true, false);
	Go(TEXT("standing"), 3.0f, false, false);
	Go(TEXT("walking"), 3.0f, true, false);
	Go(TEXT("standing"), 3.0f, false, false);
	Go(TEXT("jogging"), 4.0f, true, true);
	Go(TEXT("standing"), 3.0f, false, false);

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.Square   Block Square on a sale night: everybody is where the list puts them, the lots up on the Block; a
 *                          walker is stopped by what stands there; the lender lends and is paid; the clerk shows who is for
 *                          sale, what is bought is in the yard, and the way out of the yard is the square; Pool Street and
 *                          the square are a walk apart, both ways.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownSquareTest, "Ashmarket.Town.Square", GutTests::Flags)

bool FTownSquareTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	static const TCHAR* SquarePath = TEXT("/Game/Town/Lvl_BlockSquare");

	if (!AutomationOpenMap(SquarePath, true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	struct FNotes
	{
		FVector Came = FVector::ZeroVector;
		FVector Last = FVector::ZeroVector;
		float Still = 0.0f;
		float WalkTime = 0.0f;
		int32 Call = 0;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	const auto InSquare = []() { return IsStreetReady() && GetTown()->GetPlace() && GetTown()->GetPlace()->Name == TEXT("Block Square"); };
	const auto InStreet = []() { return IsStreetReady() && GetTown()->GetPlace() && GetTown()->GetPlace()->Name == TEXT("Pool Street"); };

	/** Stands the player by somebody, on the side the square is on, and returns which of the place's people they are */
	const auto StandWith = [](const TCHAR* Name)
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Who = Place->People.IndexOfByPredicate([Name](const FTownPerson& Person) { return Person.Name == Name; });

		if (Who != INDEX_NONE)
		{
			// to their west: everybody who deals in the square faces it from beside the Block
			StandBy(Place->People[Who].Place, FVector2D(0.0f, -170.0f));

			Town->Tick(0.0f);
		}

		return Who;
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this, Notes]()
	{
		const ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		TestEqual(TEXT("Everybody the list names is standing in the square"), Town->GetPeople().Num(), Place->People.Num());
		TestTrue(TEXT("Something is being called from the Block"), Town->GetCall() != nullptr);

		int32 Lots = 0;

		for (int32 i = 0; i < Place->People.Num(); ++i)
		{
			const AGutFighter* Body = Town->GetPeople().IsValidIndex(i) ? Town->GetPeople()[i].Get() : nullptr;

			if (!Body || !Place->People[i].bStandsOnWhatIsThere)
			{
				continue;
			}

			++Lots;

			const float Feet = Body->GetActorLocation().Z - Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

			// the trucks' own shape is what they stand on, and it is only there to be stood on where the trucks are drawn
			if (FApp::CanEverRender())
			{
				TestTrue(*FString::Printf(TEXT("%s stands up on the Block (%.0f cm over the ground)"), *Place->People[i].Name, Feet - Place->People[i].Place.Z), Feet - Place->People[i].Place.Z > 60.0f && Feet - Place->People[i].Place.Z < 175.0f);
			}

			TestTrue(*FString::Printf(TEXT("%s wears a collar"), *Place->People[i].Name), !Body->HasBody() || Body->IsCollared());
		}

		TestEqual(TEXT("Six lots are up"), Lots, 6);

		// the bidders are only there to be seen: with no renderer nobody comes
		if (FApp::CanEverRender())
		{
			int32 Standing = 0;

			for (TActorIterator<AGutCrowd> It(Town->GetWorld()); It; ++It)
			{
				Standing += It->GetNumSpectators();
			}

			TestTrue(*FString::Printf(TEXT("A crowd of bidders stands in the square (%d)"), Standing), Standing >= 40);
		}

		// from the open ground on the Main Street side, for walking at what stands round the square
		Notes->Came = FVector(Place->People[0].Place.X + 1050.0f, Place->People[0].Place.Y - 3100.0f, 0.0f);
		Notes->Call = Town->GetCallIndex();

		StandAt(FVector2D(Notes->Came.X, Notes->Came.Y));
	}, InSquare });

	static const FVector Ways[] = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f) };
	static const TCHAR* WayNames[] = { TEXT("north"), TEXT("east"), TEXT("south"), TEXT("west") };

	for (int32 Way = 0; Way < 4; ++Way)
	{
		Steps.Add({ 0.0f, [this, Notes, Way]()
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

			TestTrue(*FString::Printf(TEXT("Going %s across the square, what stops the walker is something standing there (the nearest thing is %.0f cm off)"), WayNames[Way], Nearest),
				Nearest < Town->GetWalkerRadius() + 12.0f);

			StandAt(FVector2D(Notes->Came.X, Notes->Came.Y));
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

	// a lot that is knocked down is walked off the Block, down the ramp, to where the sold are kept
	Steps.Add({ 0.2f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		Town->GetWalker()->SetJogging(false);

		TestTrue(TEXT("The sale has moved on while the player walked about"), Town->GetCallIndex() != Notes->Call);
		TestTrue(TEXT("The square has a way for its lots to come up by"), Place->LotWay.Num() >= 2);

		const int32 Sold = Place->Calls.IndexOfByPredicate([](const FTownCall& Call) { return Call.Sells == 1; });

		if (TestTrue(TEXT("A call knocks down lot one"), Sold != INDEX_NONE))
		{
			Town->SkipToCall(Sold);
		}
	} });

	Steps.Add({ 0.2f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Who = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Lot == 1; });

		if (!TestTrue(TEXT("Somebody is lot one"), Town->GetPeople().IsValidIndex(Who) && Town->GetPeople()[Who]) || Place->LotWay.Num() < 2)
		{
			return;
		}

		const AGutFighter* Body = Town->GetPeople()[Who];
		const int32 Lots = Place->People.FilterByPredicate([](const FTownPerson& Person) { return Person.Lot > 0; }).Num();

		// the first to go stands at the far end of the file, which runs back from where the way begins
		const FVector Wanted = Place->LotWay[0] + ((Place->LotWay[0] - Place->LotWay[1]).GetSafeNormal2D() * (85.0f * (Lots - 1)));
		const float Feet = Body->GetActorLocation().Z - Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		TestEqual(TEXT("One lot is kept"), Town->GetLotsKept(), 1);
		TestTrue(*FString::Printf(TEXT("Lot one has been walked to the end of the file (%.0f cm from it)"), FVector::Dist2D(Body->GetActorLocation(), Wanted)), FVector::Dist2D(Body->GetActorLocation(), Wanted) < 20.0f);
		TestTrue(TEXT("And has stopped walking"), Body->GetLedVelocity().IsNearlyZero());

		// what is under them is only there where it is drawn
		if (FApp::CanEverRender())
		{
			TestTrue(*FString::Printf(TEXT("Lot one is down off the Block (%.0f cm over the square)"), Feet - Place->People[Who].Place.Z), Feet - Place->People[Who].Place.Z < 20.0f);
		}

		const int32 Up = Place->Calls.IndexOfByPredicate([](const FTownCall& Call) { return Call.bBringsUp; });

		if (TestTrue(TEXT("A call brings the lots up"), Up != INDEX_NONE))
		{
			Town->SkipToCall(Up);
		}
	}, []() { return GetTown()->GetLotsKept() == 1 && GetTown()->GetLedCount() == 0; } });

	Steps.Add({ 0.2f, [this]()
	{
		const ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Who = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Lot == 1; });

		if (!Town->GetPeople().IsValidIndex(Who) || !Town->GetPeople()[Who])
		{
			return;
		}

		const AGutFighter* Body = Town->GetPeople()[Who];
		const float Feet = Body->GetActorLocation().Z - Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		TestTrue(*FString::Printf(TEXT("Lot one is back on its number (%.0f cm from it)"), FVector::Dist2D(Body->GetActorLocation(), Place->People[Who].Place)), FVector::Dist2D(Body->GetActorLocation(), Place->People[Who].Place) < 5.0f);
		TestTrue(TEXT("Facing the square again"), FMath::Abs(FRotator::NormalizeAxis(Body->GetActorRotation().Yaw - Place->People[Who].Facing)) < 3.0f);

		if (FApp::CanEverRender())
		{
			TestTrue(*FString::Printf(TEXT("And up on the Block (%.0f cm over the square)"), Feet - Place->People[Who].Place.Z), Feet - Place->People[Who].Place.Z > 130.0f && Feet - Place->People[Who].Place.Z < 175.0f);
		}
	}, []() { return GetTown()->GetLotsKept() == 0 && GetTown()->GetLedCount() == 0; } });

	// the lender, with a page to write on: ten litres in hand and nothing owed
	Steps.Add({ 0.3f, [this, Notes, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->GetWalker()->SetJogging(false);

		Run->Window = FGutWindowState();
		Run->Window.bStarted = true;
		Run->Window.Water = 10.0f;

		const int32 Lender = StandWith(TEXT("Mother Ledger"));

		TestEqual(TEXT("Mother Ledger is in reach at her table"), Town->GetPersonInReach(), Lender);

		Town->Interact();

		TestTrue(TEXT("She begins with the figures"), Town->IsTalking() && Town->GetWords().Contains(TEXT("0 litres owed")) && Town->GetWords().Contains(TEXT("holding 10")));

		// the first answer borrows
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("She counts out twenty-five"), Run->Window.Water, 35.0f);
		TestEqual(TEXT("And writes down thirty-five"), Run->Window.Debt, 35.0f);
		TestTrue(TEXT("And says so, with the page as it now stands"), Town->IsTalking() && Town->GetWords().Contains(TEXT("35 litres owed")));
		TestTrue(TEXT("What is owed her is on the player's purse"), Town->GetPurses().ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("Mother Ledger 35")); }));

		// the second pays back what the player holds
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Paying her back takes what is owed out of what is held"), Run->Window.Water, 0.0f);
		TestEqual(TEXT("And clears the page"), Run->Window.Debt, 0.0f);

		// the last answer leaves her
		for (int32 i = 0; i < 3; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestFalse(TEXT("Nothing tonight ends it"), Town->IsTalking());

		// she lends only so much
		Run->Window.Debt = 100.0f;

		Town->Interact();
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("She will not lend past what she lets anybody owe"), Run->Window.Debt, 100.0f);

		for (int32 i = 0; i < 3; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		Run->Window = FGutWindowState();
	} });

	// the clerk: who is for sale, and what is bought is in the yard
	Steps.Add({ 0.3f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		// in a test the street is the old card unless the test asks for the place
		Run->bStreetForced = true;
		Run->Stable = FGutStableState();

		const int32 Clerk = StandWith(TEXT("Margin"));

		TestEqual(TEXT("The clerk is in reach at the table"), Town->GetPersonInReach(), Clerk);

		Town->Interact();
		Town->HandleMenuInput(EGutMenuInput::Confirm);
	} });

	Steps.Add({ 0.5f, [this]()
	{
		const UGutGameInstance* Run = GetRun();
		const GutTests::FFight Fight = GutTests::GetFight();

		TestTrue(TEXT("Buying at the Block opens the cage as an owner has it"), Run->bStable && Fight.GameMode->IsAtStable());
		TestEqual(TEXT("At who is for sale today"), Fight.GameMode->GetStablePage(), EGutStablePage::Block);

		// the first on the list
		Fight.GameMode->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Whoever is bought is the owner's"), Run->Stable.Fighters.Num(), 1);

		// back a page is the yard, and they are in it
		Fight.GameMode->HandleMenuInput(EGutMenuInput::Left);

		TestEqual(TEXT("And is in the yard"), Fight.GameMode->GetStablePage(), EGutStablePage::Yard);

		Fight.GameMode->GoToDoor();
	}, []() { return GutTests::GetFight().IsValid() && GutTests::GetFight().GameMode->GetPhase() == EGutFightPhase::Card && !GutTests::GetFight().GameMode->IsTellingStory(); } });

	// the yard's way out is the square, at the clerk's table, and from there Pool Street is a walk up the lane
	Steps.Add({ 0.5f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Clerk = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Name == TEXT("Margin"); });

		TestTrue(TEXT("Out of the yard the player is back at the clerk's table"), Place->People.IsValidIndex(Clerk) && FVector::Dist2D(Town->GetWalker()->GetActorLocation(), Place->People[Clerk].Place) < 300.0f);

		for (const FTownDoor& Door : Place->Doors)
		{
			if (Door.bWalkedThrough)
			{
				StandAt(FVector2D(Door.Place.X - 40.0f, Door.Place.Y));
				return;
			}
		}

		AddError(TEXT("The square has no open way out of it"));
	}, InSquare });

	Steps.Add({ 0.5f, [this]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		TestTrue(TEXT("Up the lane is Pool Street, at its Main Street end"), Town->GetWalker()->GetActorLocation().Y < Place->People[0].Place.Y);

		for (const FTownDoor& Door : Place->Doors)
		{
			if (Door.bWalkedThrough)
			{
				StandAt(FVector2D(Door.Place.X + 40.0f, Door.Place.Y));
				return;
			}
		}

		AddError(TEXT("Pool Street has no open way down to the square"));
	}, InStreet });

	Steps.Add({ 0.5f, [this]()
	{
		UGutGameInstance* Run = GetRun();

		TestTrue(TEXT("And down the lane from Pool Street is the square again"), true);

		// whatever else runs after this in the same sitting gets the street it would have had, and nobody's yard or page
		Run->Stable = FGutStableState();
		Run->Window = FGutWindowState();
		Run->bStable = false;
		Run->bStreetForced.Reset();
		Run->TownLevel = NAME_None;
		Run->TownStood.Reset();
	}, InSquare });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Not a test: fixed views of Block Square saved as pictures, with nothing drawn over them, and then two as it is played.
 *  Ashmarket.Look.Square. Not part of a test run.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookSquareTest, "Ashmarket.Look.Square", GutTests::Flags)

bool FTownLookSquareTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
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
		{ TEXT("square_01_from_the_lane"), FVector(5.2f, 168.6f, 6.3f), FVector(24.0f, 150.0f, 5.6f), 68.0f },
		{ TEXT("square_02_the_block"), FVector(14.5f, 150.0f, 6.4f), FVector(29.6f, 150.0f, 6.6f), 62.0f },
		{ TEXT("square_03_the_clerk"), FVector(22.6f, 141.4f, 6.2f), FVector(27.4f, 143.8f, 5.6f), 55.0f },
		{ TEXT("square_04_the_lender"), FVector(22.9f, 158.9f, 6.2f), FVector(27.6f, 156.5f, 5.6f), 55.0f },
		{ TEXT("square_05_from_the_block"), FVector(29.0f, 147.0f, 8.4f), FVector(8.0f, 152.0f, 5.0f), 75.0f },
		{ TEXT("square_06_the_main_street_end"), FVector(8.0f, 142.0f, 6.3f), FVector(10.0f, 130.5f, 5.4f), 65.0f },
		{ TEXT("square_07_the_west_side"), FVector(10.0f, 152.0f, 6.3f), FVector(-8.0f, 146.0f, 5.8f), 65.0f },
		{ TEXT("square_08_from_above"), FVector(11.0f, 148.0f, 46.0f), FVector(11.5f, 148.6f, 4.5f), 62.0f },
		{ TEXT("square_09_a_lot"), FVector(26.2f, 149.4f, 6.5f), FVector(29.6f, 149.6f, 7.0f), 45.0f },
		{ TEXT("square_13_the_gap_in_the_rope"), FVector(13.5f, 150.6f, 6.3f), FVector(24.0f, 150.4f, 5.6f), 60.0f },
		{ TEXT("square_14_the_counting_house"), FVector(24.5f, 160.6f, 6.4f), FVector(37.0f, 164.0f, 7.6f), 60.0f },
		{ TEXT("square_15_the_holding_yard"), FVector(24.0f, 138.5f, 6.4f), FVector(36.0f, 144.0f, 5.6f), 55.0f },
		{ TEXT("square_16_the_table_and_the_scale"), FVector(24.3f, 143.0f, 6.0f), FVector(27.2f, 142.6f, 5.2f), 60.0f },
		{ TEXT("square_17_the_lot_board"), FVector(26.2f, 145.2f, 6.3f), FVector(29.6f, 154.3f, 6.7f), 40.0f },
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
				// the level's X is the map's north and its Y the map's east, from the cage's own middle
				const auto InLevel = [](const FVector& OnMap) { return FVector((OnMap.Y - 208.0f) * 100.0f, (OnMap.X - 95.0f) * 100.0f, (OnMap.Z - 3.0f) * 100.0f); };

				const FVector From = InLevel(View.From);

				Seen->SetActorLocationAndRotation(From, (InLevel(View.At) - From).Rotation());
				Seen->GetCameraComponent()->SetFieldOfView(View.FieldOfView);
				Seen->GetCameraComponent()->SetConstraintAspectRatio(false);
			}
		} });

		Steps.Add({ 1.2f, [View]() { GutTests::Screenshot(View.Name); } });
	}

	// and as it is played: where the player comes in, and at the lender's table
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

	Steps.Add({ 1.5f, []() { GutTests::Screenshot(TEXT("square_10_as_played_arriving")); } });

	Steps.Add({ 0.3f, []()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Lender = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Name == TEXT("Mother Ledger"); });

		if (Lender != INDEX_NONE)
		{
			GetRun()->Window.bStarted = true;
			GetRun()->Window.Water = 15.0f;
			GetRun()->Window.Debt = 35.0f;

			StandBy(Place->People[Lender].Place, FVector2D(0.0f, -170.0f));

			if (AController* Eyes = Town->GetWalker()->GetController())
			{
				Eyes->SetControlRotation(FRotator(-8.0f, 90.0f, 0.0f));
			}
		}
	} });

	Steps.Add({ 1.5f, []() { GutTests::Screenshot(TEXT("square_11_as_played_in_reach")); } });
	Steps.Add({ 0.3f, []() { GetTown()->Interact(); } });
	Steps.Add({ 2.5f, []() { GutTests::Screenshot(TEXT("square_12_as_played_the_lender")); } });

	Steps.Add({ 0.3f, []()
	{
		ATownGameMode* Town = GetTown();

		for (int32 i = 0; i < 3; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		GetRun()->Window = FGutWindowState();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeDeckTest, "Ashmarket.Probe.Deck", GutTests::Flags)

/**
 *  How high whatever stands in Block Square is along a line, to lay a way over it: -TownProbeLine=x0,y0,x1,y1 in the map's
 *  own metres, every 20 cm. The models are only there to be measured where they are drawn, so it wants a rendered run.
 */
bool FTownProbeDeckTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 3.0f, []()
	{
		const ATownGameMode* Town = GetTown();
		UWorld* World = Town->GetWorld();

		// the same lines every time unless others are asked for: the way up the ramp, and four along the deck
		TArray<FString> Lines = { TEXT("27.5,150.1,44,150.1"), TEXT("29.0,144,29.0,156"), TEXT("29.6,144,29.6,156"), TEXT("30.3,144,30.3,156"), TEXT("27.5,149.5,44,149.5"), TEXT("27.5,150.7,44,150.7") };

		FString Asked;

		if (FParse::Value(FCommandLine::Get(), TEXT("TownProbeLine="), Asked, false))
		{
			Lines = { Asked };
		}

		FCollisionQueryParams Looking(FName("TownProbe"), true, Town->GetWalker());

		for (const AGutFighter* Body : Town->GetPeople())
		{
			if (Body)
			{
				Looking.AddIgnoredActor(Body);
				Looking.AddIgnoredActor(Body->GetBodyActor());
			}
		}

		// the square's ground, and the map's own corner, as Tools/town_block_square.json has them
		const float Ground = 150.0f;

		for (const FString& Line : Lines)
		{
			TArray<FString> Numbers;
			Line.ParseIntoArray(Numbers, TEXT(","));

			if (Numbers.Num() != 4)
			{
				continue;
			}

			const FVector2D From(FCString::Atof(*Numbers[0]), FCString::Atof(*Numbers[1]));
			const FVector2D To(FCString::Atof(*Numbers[2]), FCString::Atof(*Numbers[3]));
			const int32 Count = FMath::CeilToInt32(FVector2D::Distance(From, To) / 0.2f);

			FString Heights;

			for (int32 i = 0; i <= Count; ++i)
			{
				const FVector2D Map = FMath::Lerp(From, To, static_cast<float>(i) / FMath::Max(Count, 1));
				const FVector Top((Map.Y - 208.0f) * 100.0f, (Map.X - 95.0f) * 100.0f, Ground + 260.0f);

				FHitResult Under;

				Heights += World->LineTraceSingleByChannel(Under, Top, Top - FVector(0.0f, 0.0f, 330.0f), ECC_Visibility, Looking)
					? FString::Printf(TEXT(" %.0f"), Under.Location.Z - Ground)
					: FString(TEXT(" -"));
			}

			UE_LOG(LogGut, Log, TEXT("Deck: from %s to %s, every 20 cm, cm over the square:%s"), *From.ToString(), *To.ToString(), *Heights);
		}
	}, &IsStreetReady });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Not a test: a measurement. Stands in Block Square with the sale going on, walks into the crowd and back out, and stands
 *  again, forty seconds in all, for reading the frame rate with -GutFrameRate. Ashmarket.Probe.Square.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeSquareTest, "Ashmarket.Probe.Square", GutTests::Flags)

bool FTownProbeSquareTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;
	TSharedRef<float> Clock = MakeShared<float>(0.0f);

	const auto Go = [&Steps, Clock](const TCHAR* What, float Seconds, const FVector& Way)
	{
		Steps.Add({ 0.0f, [What]() { UE_LOG(LogGut, Log, TEXT("Square: end of %s"), What); }, [Clock, Seconds, Way]()
		{
			AGutFighter* Walker = GetTown()->GetWalker();

			if (!Way.IsZero())
			{
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

	// the bidders are fetched in the background: give them time to come
	Steps.Add({ 12.0f, []() { UE_LOG(LogGut, Log, TEXT("Square: everybody is here")); }, &IsStreetReady });

	Go(TEXT("standing where the lane comes in"), 10.0f, FVector::ZeroVector);
	Go(TEXT("walking south-east into the crowd"), 10.0f, FVector(-0.75f, 0.66f, 0.0f));
	Go(TEXT("standing in the crowd, looking at the Block"), 10.0f, FVector::ZeroVector);
	Go(TEXT("walking back"), 8.0f, FVector(0.75f, -0.66f, 0.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

#endif
