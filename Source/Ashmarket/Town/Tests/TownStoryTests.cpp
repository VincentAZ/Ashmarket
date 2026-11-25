// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/TownTestHelpers.h"

/**
 *  The town's story: its marks, what is asked of them and what changes them, and the work that is made of them.
 *
 *  Ashmarket.Town.Marks      what is asked of the marks and what is done to them, with no place needed but one to stand in
 *  Ashmarket.Town.GateDutyEnds   the other ways the shift can come out: she is held by the book, she is waved through
 *                            and the book finds it or does not, the tin is short, her water is taken and she is held anyway
 *  Ashmarket.Town.GateDuty   the Wardens' first piece of work, played through on the foot lane in the Gate Yard: taken
 *                            from Sergeant Rook, five read as they come up the tunnel, the one with the forged tag bought
 *                            a real one, the count taken back, and what the town remembers of it afterwards
 */
namespace StoryTests
{
	const TCHAR* MapPath = TEXT("/Game/Town/Lvl_MainStreet");

	/** A place on the map (x east, y north, in metres) as the level has it */
	FVector2D OnMap(float X, float Y)
	{
		return FVector2D((Y - 208.0f) * 100.0f, (X - 95.0f) * 100.0f);
	}

	/** Stands the player at a place in the level, on the ground the place has there, looking along a yaw */
	void StandOn(const FVector2D& Where, float Yaw)
	{
		const ATownGameMode* Town = TownTests::GetTown();
		AGutFighter* Walker = Town ? Town->GetWalker() : nullptr;

		if (!Walker || !Town->GetPlace())
		{
			return;
		}

		const float Z = Town->GetPlace()->GetGroundZ(Where.X) + Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f;

		Walker->SetActorLocationAndRotation(FVector(Where.X, Where.Y, Z), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

		if (AController* Eyes = Walker->GetController())
		{
			Eyes->SetControlRotation(FRotator(-6.0f, Yaw, 0.0f));
		}
	}

	/** One of the place's people by name, and by a word of when they are there, where several have one name */
	int32 Find(const TCHAR* Name, const TCHAR* When = TEXT(""))
	{
		return TownTests::GetTown()->GetPlace()->People.IndexOfByPredicate([Name, When](const FTownPerson& Person) { return Person.Name == Name && Person.When.Contains(When); });
	}

	bool IsStanding(int32 Person)
	{
		const ATownGameMode* Town = TownTests::GetTown();

		return Town && Town->GetPeople().IsValidIndex(Person) && Town->GetPeople()[Person] && Town->GetLedCount() >= 0 && !Town->IsPersonLed(Person);
	}

	/** Says the answer that has these words in it. False if it is not there to be said. */
	bool Say(const TCHAR* Words)
	{
		ATownGameMode* Town = TownTests::GetTown();
		const TArray<FString> Answers = Town->GetAnswers();
		const int32 Which = Answers.IndexOfByPredicate([Words](const FString& Answer) { return Answer.Contains(Words); });

		if (Which == INDEX_NONE)
		{
			return false;
		}

		while (Town->GetTalkChoice() != Which)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		return true;
	}

	/** The run as it was found, to be put back */
	struct FKept
	{
		bool bLadder = false;
		FGutLadderState Ladder;
		FGutLadderState RungStart;
		FTownStoryState Story;
		TArray<FGutBagItem> Bag;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownMarksTest, "Ashmarket.Town.Marks", GutTests::Flags)

bool FTownMarksTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(StoryTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TSharedRef<StoryTests::FKept> Kept = MakeShared<StoryTests::FKept>();

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [this, Kept]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		Run->Story = FTownStoryState();
		Run->bLadderStarted = true;
		Run->Ladder.Water = 40.0f;

		TestTrue(TEXT("The town's story is written down where the game reads it"), Town->GetStory() != nullptr && Town->GetStory()->Factions.Num() == 5);

		TestEqual(TEXT("A mark nobody has touched is nought"), Town->GetMark(FName("w9")), 0);
		TestEqual(TEXT("Trust begins where the story says"), Town->GetMark(FName("trust")), 40);
		TestEqual(TEXT("Water is what is held"), Town->GetMark(FName("water")), 40);

		TestTrue(TEXT("Nothing asked always holds"), Town->Holds(TEXT("")));
		TestTrue(TEXT("A mark at nought is not"), Town->Holds(TEXT("!w9")) && !Town->Holds(TEXT("w9")));
		TestTrue(TEXT("Figures are compared"), Town->Holds(TEXT("water>=40 water<41 trust=40 trust!=41 trust>39 trust<=40")));

		FString Lacking;

		TestTrue(TEXT("And what is lacking is named"), !Town->Holds(TEXT("trust=40 water>=41 w9"), &Lacking) && Lacking == TEXT("water>=41"));
		TestTrue(TEXT("Either of two will do"), Town->Holds(TEXT("w9=3|trust=40")) && !Town->Holds(TEXT("w9=3|trust=41")));

		const FString Told = Town->Apply(TEXT("st.wardens+10 st.freetags-5 water-30 w9=2 bag.splint+1 trust+2"));

		TestEqual(TEXT("Standing goes up"), Town->GetMark(FName("st.wardens")), 10);
		TestEqual(TEXT("And down"), Town->GetMark(FName("st.freetags")), -5);
		TestEqual(TEXT("Water is paid"), Town->GetMark(FName("water")), 10);
		TestEqual(TEXT("A mark is set"), Town->GetMark(FName("w9")), 2);
		TestEqual(TEXT("Something is put in the bag"), Run->GetBagCount(FName("splint")), Kept->Bag.ContainsByPredicate([](const FGutBagItem& Item) { return Item.Good == FName("splint"); }) ? Run->GetBagCount(FName("splint")) : 1);
		TestTrue(*FString::Printf(TEXT("The player is told who thinks what of it, and what was paid (%s)"), *Told), Told.Contains(TEXT("The Wardens +10")) && Told.Contains(TEXT("The Free Tags")) && Told.Contains(TEXT("30 L paid: you hold 10")));

		Town->Apply(TEXT("water-500 st.wardens+500 heat+9 fear-3"));

		TestEqual(TEXT("Water is never taken below nothing"), Town->GetMark(FName("water")), 0);
		TestEqual(TEXT("Standing stops at a hundred"), Town->GetMark(FName("st.wardens")), 100);
		TestEqual(TEXT("Heat stops at five"), Town->GetMark(FName("heat")), 5);
		TestEqual(TEXT("Fear stops at nought"), Town->GetMark(FName("fear")), 0);

		Town->Thrown = 3;
		Town->Apply(TEXT("w9.roll=?4"));
		Town->Thrown.Reset();

		TestEqual(TEXT("A throw can be fixed for a test"), Town->GetMark(FName("w9.roll")), 3);

		TestEqual(TEXT("Hunted at fifty under"), ATownGameMode::GetStandingWord(-50), FString(TEXT("hunted by them")));
		TestEqual(TEXT("Trusted at twenty"), ATownGameMode::GetStandingWord(20), FString(TEXT("trusted with work")));

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	}, []() { return IsStreetReady(); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownGateDutyTest, "Ashmarket.Town.GateDuty", GutTests::Flags)

bool FTownGateDutyTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(StoryTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TSharedRef<FKept> Kept = MakeShared<FKept>();

	// the head of the foot lane, looking down it at the tunnel
	const FVector2D LaneHead = OnMap(1.75f, 7.5f);

	TArray<FScript::FStep> Steps;

	// somebody with a fighter's 40 L, whom the town knows nothing about
	Steps.Add({ 0.5f, [this, Kept]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		Run->Story = FTownStoryState();
		Run->Bag.Reset();
		Run->bLadderStarted = true;
		Run->Ladder.Water = 40.0f;

		// nobody is found out by the book tonight
		Town->Thrown = 3;

		const int32 Rook = Find(TEXT("Sergeant Rook"));

		TestTrue(TEXT("Sergeant Rook is at the tally desk"), Town->GetPeople().IsValidIndex(Rook) && Town->GetPeople()[Rook] != nullptr);

		for (const TCHAR* Comer : { TEXT("A hauler"), TEXT("A woman with no litre"), TEXT("A collector"), TEXT("A cart hand") })
		{
			TestTrue(*FString::Printf(TEXT("%s is not in the yard before the shift"), Comer), Town->GetPeople().IsValidIndex(Find(Comer)) && Town->GetPeople()[Find(Comer)] == nullptr);
		}

		TestTrue(TEXT("Nor is Calla Wynn, anywhere"), !Town->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("w1.q=4"))] && !Town->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("wynn=2"))] && !Town->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("wynn=3"))]);

		const FVector Desk = Town->GetPlace()->People[Rook].Place;

		// across the tally desk from him
		StandOn(FVector2D(Desk.X - 185.0f, Desk.Y), 0.0f);
	}, []() { return IsStreetReady(); } });

	// (the camera follows a moment behind whoever is stood somewhere else: the pictures wait for it)
	Steps.Add({ 1.2f, [this]()
	{
		ATownGameMode* Town = GetTown();

		const int32 Rook = Find(TEXT("Sergeant Rook"));

		Town->Tick(0.0f);

		TestEqual(TEXT("Rook is who is in reach"), Town->GetPersonInReach(), Rook);

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("He offers the shift (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("reader short")));
		TestEqual(TEXT("And there are three things to say to it"), Town->GetAnswers().Num(), 3);

		GutTests::Screenshot(TEXT("50_rook_offers_the_shift"));
	} });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		TestTrue(TEXT("It can be asked about"), Say(TEXT("What is the work")));
		TestTrue(TEXT("And he says what it is"), Town->GetWords().Contains(TEXT("No tag is property")));
		TestEqual(TEXT("With the same three things still to say"), Town->GetAnswers().Num(), 3);

		TestTrue(TEXT("It can be taken"), Say(TEXT("I will stand it")));
		TestTrue(TEXT("And he says where to stand"), Town->GetWords().Contains(TEXT("Foot lane")));
		TestEqual(TEXT("The work is in hand"), Town->GetMark(FName("w1")), 2);
		TestEqual(TEXT("And is not offered again in the same breath"), Town->GetAnswers().Num(), 0);

		Town->Interact();

		TestFalse(TEXT("The talk ends there"), Town->IsTalking());
		TestTrue(TEXT("It is written down"), Run->Story.Journal.Num() == 1);
	} });

	// the first of them comes up the tunnel, and is read
	const auto At = [](const TCHAR* Name, const TCHAR* When = TEXT(""))
	{
		return [Name, When]() { return IsStreetReady() && IsStanding(Find(Name, When)); };
	};

	Steps.Add({ 0.4f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();

		StandOn(LaneHead, 180.0f);
		Town->Tick(0.0f);

		TestEqual(TEXT("The hauler is who is in reach at the head of the lane"), Town->GetPersonInReach(), Find(TEXT("A hauler")));

		GutTests::Screenshot(TEXT("51_the_first_up_the_lane"));

		Town->Interact();

		TestTrue(TEXT("What the tag looks like is written under what he says"), Town->GetTalkNote().Contains(TEXT("one clean blow")));
		TestTrue(TEXT("He is sent through"), Say(TEXT("One litre")));
		TestEqual(TEXT("A litre is in the tin"), Town->GetMark(FName("w1.toll")), 1);
		TestTrue(TEXT("And he walks on"), Town->IsPersonLed(Find(TEXT("A hauler"))));

		Town->Interact();

		TestFalse(TEXT("With nothing more to be said to him"), Town->IsTalking());
	}, At(TEXT("A hauler")) });

	// the second has no litre: it is put in for her
	Steps.Add({ 0.4f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();

		StandOn(LaneHead, 180.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestEqual(TEXT("There are three ways with somebody who cannot pay"), Town->GetAnswers().Num(), 3);
		TestTrue(TEXT("Her litre is paid for her"), Say(TEXT("I will put your litre in")));
		TestEqual(TEXT("Out of the reader's own water"), Town->GetMark(FName("water")), 39);
		TestEqual(TEXT("Which the town thinks the better of"), Town->GetMark(FName("trust")), 41);

		Town->Interact();
	}, At(TEXT("A woman with no litre")) });

	// the third does not mean to pay, and does
	Steps.Add({ 0.4f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();

		StandOn(LaneHead, 180.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("The collector is made to pay"), Say(TEXT("The post takes a litre")));
		TestEqual(TEXT("And the Chainmen think the less of whoever made him"), Town->GetMark(FName("st.chainmen")), -2);

		Town->Interact();
	}, At(TEXT("A collector")) });

	// the fourth is Calla Wynn, with a tag struck twice
	Steps.Add({ 0.4f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();

		StandOn(LaneHead, 180.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("The forgery is written under what she says"), Town->GetTalkNote().Contains(TEXT("forgery")));
		TestEqual(TEXT("There are five things to do about it"), Town->GetAnswers().Num(), 5);

		GutTests::Screenshot(TEXT("52_calla_wynn_and_a_tag_struck_twice"));
	}, At(TEXT("Calla Wynn"), TEXT("w1.q=4")) });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		// with too little, buying her a tag is refused and said why, and nothing is changed
		Run->Ladder.Water = 12.0f;

		TestTrue(TEXT("Buying her a tag can be asked for with too little"), Say(TEXT("I will buy you a real one")));
		TestTrue(*FString::Printf(TEXT("And it is said what it takes (%s)"), *Town->GetTalkNote()), Town->GetTalkNote().Contains(TEXT("That takes 30 L. You hold 12.")));
		TestEqual(TEXT("With nothing done"), Town->GetMark(FName("wynn")), 0);
		TestTrue(TEXT("And the talk still where it was"), Town->IsTalking() && Town->GetAnswers().Num() == 5);

		Run->Ladder.Water = 39.0f;

		TestTrue(TEXT("With enough, it is done"), Say(TEXT("I will buy you a real one")));
		TestEqual(TEXT("Thirty litres of the reader's own"), Town->GetMark(FName("water")), 9);
		TestEqual(TEXT("She is tagged"), Town->GetMark(FName("wynn")), 2);
		TestTrue(TEXT("And says so"), Town->GetWords().Contains(TEXT("Thirty litres")));
		TestTrue(*FString::Printf(TEXT("What was paid is written under it (%s)"), *Town->GetTalkNote()), Town->GetTalkNote().Contains(TEXT("30 L paid: you hold 9")));

		Town->Interact();

		TestFalse(TEXT("She goes on up the street"), Town->IsTalking());
	} });

	// and the last
	Steps.Add({ 0.4f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();

		StandOn(LaneHead, 180.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("The cart hand is sent through"), Say(TEXT("One litre")));
		TestEqual(TEXT("Five litres are in the tin"), Town->GetMark(FName("w1.toll")), 5);
		TestEqual(TEXT("And nobody is left to come"), Town->GetMark(FName("w1.q")), 6);

		Town->Interact();
	}, At(TEXT("A cart hand")) });

	// the count is taken to Rook, and he pays
	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		const int32 Rook = Find(TEXT("Sergeant Rook"));
		const FVector Desk = Town->GetPlace()->People[Rook].Place;

		StandOn(FVector2D(Desk.X - 185.0f, Desk.Y), 0.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("He asks for the count (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("Count")));
		TestEqual(TEXT("And there is one true thing to tell him"), Town->GetAnswers().Num(), 1);
		TestTrue(TEXT("The tin is right"), Say(TEXT("the tin is right")));
		TestEqual(TEXT("A fifth of it is the reader's"), Town->GetMark(FName("water")), 14);
		TestEqual(TEXT("Nothing is offered while he goes on"), Town->GetAnswers().Num(), 0);

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("He says what he makes of the tag that was bought (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("no line for that")));
		TestEqual(TEXT("And pays the shift"), Town->GetMark(FName("water")), 39);
		TestEqual(TEXT("The Wardens know the reader now"), Town->GetMark(FName("st.wardens")), 10);
		TestEqual(TEXT("The work is done"), Town->GetMark(FName("w1")), 100);

		GutTests::Screenshot(TEXT("53_rook_pays_the_shift"));
	} });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		const int32 Rook = Find(TEXT("Sergeant Rook"));
		const FVector Desk = Town->GetPlace()->People[Rook].Place;

		Town->Interact();

		TestFalse(TEXT("And that is the end of it"), Town->IsTalking());
		TestTrue(*FString::Printf(TEXT("What it came to stays up in the street (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("The Wardens +10")));

		// the bag has the work in it, how the player stands, and what it has done
		const TArray<FTownBagLine> Bag = Town->GetBagLines();

		const FTownBagLine* Work = Bag.FindByPredicate([](const FTownBagLine& Line) { return Line.Name == TEXT("Gate duty"); });
		const FTownBagLine* Standing = Bag.FindByPredicate([](const FTownBagLine& Line) { return Line.Name == TEXT("The Wardens"); });

		TestTrue(TEXT("The work is in the bag, done"), Work && Work->Count == TEXT("done") && !Work->About.IsEmpty() && Work->About[0].Contains(TEXT("stood the shift")));
		TestTrue(TEXT("And how the player stands with the Wardens"), Standing && Standing->Count == TEXT("+10") && Standing->Detail == TEXT("known to them"));
		TestTrue(TEXT("And the Chainmen, who were made to pay"), Bag.ContainsByPredicate([](const FTownBagLine& Line) { return Line.Name == TEXT("The Chainmen") && Line.Count == TEXT("-2"); }));
		TestTrue(*FString::Printf(TEXT("The player's own account has it all (%d lines)"), Run->Story.Journal.Num()), Run->Story.Journal.Num() == 3);

		// afterwards he has other words
		StandOn(FVector2D(Desk.X - 185.0f, Desk.Y), 0.0f);
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("Rook speaks of the next work"), Town->GetWords().Contains(TEXT("Row's cut")));

		Town->Interact();
	} });

	// the town remembers: she is on Main Street, tagged, and pays it back
	Steps.Add({ 1.0f, [this, Kept]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		const int32 Wynn = Find(TEXT("Calla Wynn"), TEXT("wynn=2"));

		TestTrue(TEXT("Calla Wynn is up the street, working"), Town->GetPeople().IsValidIndex(Wynn) && Town->GetPeople()[Wynn] != nullptr);
		TestTrue(TEXT("And is not held on the Block or hiding at a fire"), !Town->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("wynn=3"))]);

		if (Town->GetPeople().IsValidIndex(Wynn) && Town->GetPeople()[Wynn])
		{
			const FVector Hers = Town->GetPlace()->People[Wynn].Place;

			StandOn(FVector2D(Hers.X, Hers.Y - 150.0f), 90.0f);
			Town->Tick(0.0f);

			TestEqual(TEXT("She is who is in reach"), Town->GetPersonInReach(), Wynn);

			Town->Interact();

			TestTrue(TEXT("She has a splint for whoever bought her a name"), Run->GetBagCount(FName("splint")) == 1);

			Town->Interact();

			StandOn(FVector2D(Hers.X, Hers.Y - 150.0f), 90.0f);
			Town->Tick(0.0f);
			Town->Interact();

			TestTrue(TEXT("Once"), Run->GetBagCount(FName("splint")) == 1 && Town->GetWords().Contains(TEXT("before you go to the cutter")));

			Town->Interact();
		}

		Town->Thrown.Reset();

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownGateDutyEndsTest, "Ashmarket.Town.GateDutyEnds", GutTests::Flags)

bool FTownGateDutyEndsTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(StoryTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TSharedRef<FKept> Kept = MakeShared<FKept>();

	const FVector2D LaneHead = OnMap(1.75f, 7.5f);

	// how one telling of the count to Rook comes out: the shift stood, the marks as given, a throw of the book's die
	struct FEnd
	{
		const TCHAR* Name;

		/** The marks as they stand when the last has come up */
		const TCHAR* Stood;

		int32 Throw;

		/** What Rook says last, what he pays in all, and where the Wardens stand afterwards */
		const TCHAR* Says;
		int32 Paid;
		int32 Wardens;
	};

	static const FEnd Ends[] =
	{
		{ TEXT("held by the book"), TEXT("wynn=1"), 3, TEXT("doubled stamp"), 38, 10 },
		{ TEXT("waved through, and the book does not find it"), TEXT("wynn=3"), 3, TEXT("Five and five"), 30, 10 },
		{ TEXT("waved through, and the book finds it"), TEXT("wynn=3"), 1, TEXT("I count heads"), 30, 5 },
		{ TEXT("held, with the tin short"), TEXT("wynn=4 w1.short=2"), 3, TEXT("doubled stamp"), 33, 10 },
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [Kept]()
	{
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		const ATownGameMode* Town = GetTown();
		const FVector Desk = Town->GetPlace()->People[Find(TEXT("Sergeant Rook"))].Place;

		StandOn(FVector2D(Desk.X - 185.0f, Desk.Y), 0.0f);
	}, []() { return IsStreetReady(); } });

	for (const FEnd& End : Ends)
	{
		Steps.Add({ 0.6f, [this, End]()
		{
			ATownGameMode* Town = GetTown();
			UGutGameInstance* Run = GetRun();

			Run->Story = FTownStoryState();
			Run->bLadderStarted = true;
			Run->Ladder.Water = 0.0f;

			Town->Apply(FString(TEXT("w1=2 w1.q=6 ")) + End.Stood);
			Town->Thrown = End.Throw;

			Town->Tick(0.0f);
			Town->Interact();

			TestTrue(*FString::Printf(TEXT("%s: he asks for the count"), End.Name), Town->GetWords().Contains(TEXT("Count")));
			TestEqual(*FString::Printf(TEXT("%s: there is one true thing to tell him"), End.Name), Town->GetAnswers().Num(), 1);

			// the count, and then he goes on for as long as he has anything to say
			Town->HandleMenuInput(EGutMenuInput::Confirm);

			FString Last;

			for (int32 i = 0; i < 6 && Town->IsTalking(); ++i)
			{
				Last = Town->GetWords();
				Town->Interact();
			}

			TestFalse(*FString::Printf(TEXT("%s: the talk comes to an end"), End.Name), Town->IsTalking());
			TestTrue(*FString::Printf(TEXT("%s: the last thing he says is the right one (%s)"), End.Name, *Last), Last.Contains(End.Says));
			TestEqual(*FString::Printf(TEXT("%s: what he pays"), End.Name), Town->GetMark(FName("water")), End.Paid);
			TestEqual(*FString::Printf(TEXT("%s: where the Wardens stand"), End.Name), Town->GetMark(FName("st.wardens")), End.Wardens);
			TestEqual(*FString::Printf(TEXT("%s: the work is done"), End.Name), Town->GetMark(FName("w1")), 100);
			TestEqual(*FString::Printf(TEXT("%s: one line of it is written down"), End.Name), Run->Story.Journal.Num(), 1);

			Town->Thrown.Reset();
		} });
	}

	// and on the lane itself: held by the book she is walked off to the post, and is gone; her water taken first, she can still be let through
	Steps.Add({ 0.5f, [this, LaneHead]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Run->Story = FTownStoryState();
		Run->Ladder.Water = 5.0f;

		Town->Apply(TEXT("w1=2 w1.q=4"));

		StandOn(LaneHead, 180.0f);
	} });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		const int32 Wynn = Find(TEXT("Calla Wynn"), TEXT("w1.q=4"));

		Town->Tick(0.0f);

		TestEqual(TEXT("She is who is in reach at the head of the lane"), Town->GetPersonInReach(), Wynn);

		Town->Interact();

		TestTrue(TEXT("Her water can be taken"), Say(TEXT("Twelve litres, you said")));
		TestEqual(TEXT("It is taken"), Town->GetMark(FName("water")), 17);
		TestEqual(TEXT("And the town thinks the less of whoever took it"), Town->GetMark(FName("trust")), 38);
		TestEqual(TEXT("Then there are two things left to do with her"), Town->GetAnswers().Num(), 2);

		GutTests::Screenshot(TEXT("55_her_water_taken"));

		TestTrue(TEXT("She can be held all the same"), Say(TEXT("No tag means property")));
		TestEqual(TEXT("She is held, robbed"), Town->GetMark(FName("wynn")), 4);
		TestEqual(TEXT("The Chainmen think the better of it"), Town->GetMark(FName("st.chainmen")), 5);
		TestEqual(TEXT("And the Free Tags the worse"), Town->GetMark(FName("st.freetags")), -5);
		TestTrue(TEXT("She is walked off to the post"), Town->IsPersonLed(Wynn));
		TestTrue(TEXT("And says what she thinks of it"), Town->GetWords().Contains(TEXT("of course")));
		TestEqual(TEXT("With nothing left to say to her"), Town->GetAnswers().Num(), 0);

		Town->Interact();

		TestFalse(TEXT("The talk is over"), Town->IsTalking());
		TestEqual(TEXT("Two lines of it are written down"), Run->Story.Journal.Num(), 2);
	}, []() { return IsStreetReady() && IsStanding(Find(TEXT("Calla Wynn"), TEXT("w1.q=4"))); } });

	// she is gone from the yard when she has been walked to the post
	Steps.Add({ 0.3f, [this, Kept]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		TestTrue(TEXT("She is gone from the lane"), Town->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("w1.q=4"))] == nullptr);

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	}, []() { return IsStreetReady() && GetTown()->GetPeople()[Find(TEXT("Calla Wynn"), TEXT("w1.q=4"))] == nullptr; } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

#endif
