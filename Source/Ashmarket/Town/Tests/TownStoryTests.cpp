// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/TownTestHelpers.h"

/**
 *  The town's story: its marks, what is asked of them and what changes them, and the work that is made of them.
 *
 *  Ashmarket.Town.Marks      what is asked of the marks and what is done to them, with no place needed but one to stand in
 *  Ashmarket.Town.TheCut     the Wardens' second piece of work: taken from Rook, twelve litres from each of two bars
 *                            on Rotgut Row, Tarp Nine found robbed, who robbed it learned from Mash and got back from
 *                            him at the gate, and Rook told; then the other ways it can be counted to him
 *  Ashmarket.Town.Rope       the Wardens' third piece of work: taken from Captain Voss in the Velvet Tin's court, the
 *                            Guildmaster paid to say where Ilya Brandt is, Brandt walked to the gate and handed to Rook,
 *                            and who stands under the gantry afterwards; then the other ways it can end, and the way in
 *                            among the sleepers
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

		// (which he now has to give: it is left for another night)
		Town->HandleBack();

		TestFalse(TEXT("And can be left"), Town->IsTalking());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownTheCutTest, "Ashmarket.Town.TheCut", GutTests::Flags)

bool FTownTheCutTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(StoryTests::MapPath, true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TSharedRef<FKept> Kept = MakeShared<FKept>();

	const auto In = [](const TCHAR* Name)
	{
		return [Name]() { return IsStreetReady() && GetTown()->GetPlace() && GetTown()->GetPlace()->Name == Name; };
	};

	// walks to another place of the town, arriving by one of its ways in
	const auto GoTo = [](const TCHAR* Level, const TCHAR* Arrive)
	{
		UGutGameInstance* Run = GetRun();

		Run->TownArrive = FName(Arrive);
		Run->TownStood.Reset();

		UGameplayStatics::OpenLevel(GetTown(), FName(Level), true);
	};

	// stands the player in reach of somebody, a little way off toward a side of them
	const auto StandWith = [](const TCHAR* Name, const FVector2D& Off)
	{
		const ATownGameMode* Town = GetTown();
		const int32 Who = Find(Name);

		if (Who != INDEX_NONE)
		{
			const FVector Theirs = Town->GetPlace()->People[Who].Place;

			StandOn(FVector2D(Theirs.X + Off.X, Theirs.Y + Off.Y), FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw);
		}

		return Who;
	};

	TArray<FScript::FStep> Steps;

	// somebody who has stood the gate shift, with 20 L
	Steps.Add({ 0.5f, [Kept, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		Run->bStreetForced = true;
		Run->Story = FTownStoryState();
		Run->bLadderStarted = true;
		Run->Ladder.Water = 20.0f;

		Town->Apply(TEXT("w1=100 wynn=2 st.wardens=10"));

		StandWith(TEXT("Sergeant Rook"), FVector2D(-185.0f, 0.0f));
	}, In(TEXT("Main Street")) });

	Steps.Add({ 1.0f, [this, GoTo]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("With the shift stood, Rook has the Row's cut to be collected (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("The Row's cut is due")));
		TestTrue(TEXT("Where the Row is can be asked"), Say(TEXT("Where is the Row")) && Town->GetWords().Contains(TEXT("under the overpass")));
		TestTrue(TEXT("It is taken"), Say(TEXT("I will collect it")) && Town->GetMark(FName("w2")) == 2);

		Town->Interact();

		TestFalse(TEXT("And the talk ends"), Town->IsTalking());

		GoTo(TEXT("/Game/Town/Lvl_RotgutRow"), TEXT("from_pool_street"));
	} });

	// the Row: The Sump pays
	Steps.Add({ 0.8f, [this, StandWith]()
	{
		const ATownGameMode* Town = GetTown();

		TestTrue(TEXT("The three who keep the bars are on the Row"), Find(TEXT("Big Lottie")) != INDEX_NONE && Find(TEXT("Fen")) != INDEX_NONE && Find(TEXT("Mash")) != INDEX_NONE
			&& Town->GetPeople()[Find(TEXT("Big Lottie"))] && Town->GetPeople()[Find(TEXT("Fen"))] && Town->GetPeople()[Find(TEXT("Mash"))]);
		TestTrue(TEXT("And nobody has Tarp Nine but Fen"), !Town->GetPeople()[Find(TEXT("A new keeper"))]);

		StandWith(TEXT("Big Lottie"), FVector2D(0.0f, 140.0f));
	}, In(TEXT("Rotgut Row")) });

	Steps.Add({ 1.0f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(TEXT("Big Lottie, up on the lip, is in reach of somebody in the furrow"), Town->GetPersonInReach(), Find(TEXT("Big Lottie")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("She has the cut ready (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("post's cut")));

		GutTests::Screenshot(TEXT("57_big_lottie_and_the_cut"));
	} });

	Steps.Add({ 0.4f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		TestTrue(TEXT("Twelve from The Sump"), Say(TEXT("Twelve")) && Town->GetMark(FName("w2.sump")) == 1 && Town->GetMark(FName("w2.cut")) == 12);
		TestEqual(TEXT("It is the post's water, and not the player's"), Town->GetMark(FName("water")), 20);

		Town->Interact();

		StandWith(TEXT("Mash"), FVector2D(-170.0f, 0.0f));
	} });

	// Detention pays; Tarp Nine has been robbed
	Steps.Add({ 1.0f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("Twelve from Detention"), Say(TEXT("Twelve")) && Town->GetMark(FName("w2.det")) == 1 && Town->GetMark(FName("w2.cut")) == 24);

		Town->Interact();

		StandWith(TEXT("Fen"), FVector2D(0.0f, 140.0f));
	} });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("Fen has been robbed (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("took the tin")) && Town->GetMark(FName("w2.fen")) == 1);
		TestEqual(TEXT("There are four things to do about it"), Town->GetAnswers().Num(), 4);

		GutTests::Screenshot(TEXT("57_fen_has_been_robbed"));
	} });

	Steps.Add({ 0.4f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		TestTrue(TEXT("Who took it can be asked"), Say(TEXT("Who took it")) && Town->GetMark(FName("w2.asked")) == 1 && Town->GetWords().Contains(TEXT("Ask Mash")));

		Town->HandleBack();

		TestFalse(TEXT("And Fen left, with nothing settled"), Town->IsTalking() || Town->GetMark(FName("w2.nine")) != 0);

		StandWith(TEXT("Mash"), FVector2D(-170.0f, 0.0f));
	} });

	// Mash saw who; he is at the gate
	Steps.Add({ 1.0f, [this, GoTo]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("Mash says who it was (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("A vest with plates in it")) && Town->GetMark(FName("w2.knows")) == 1);
		TestEqual(TEXT("It is written down"), Run->Story.Journal.Num(), 2);

		Town->Interact();

		GoTo(TEXT("/Game/Town/Lvl_MainStreet"), TEXT("from_square"));
	} });

	Steps.Add({ 0.8f, [StandWith]()
	{
		StandWith(TEXT("Lane Warden"), FVector2D(180.0f, 0.0f));
	}, In(TEXT("Main Street")) });

	Steps.Add({ 1.0f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(TEXT("The Warden on the wagon lane is in reach"), Town->GetPersonInReach(), Find(TEXT("Lane Warden")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("He knows why he is being spoken to (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("Fen's tin")));
		TestTrue(TEXT("He hands it over"), Say(TEXT("Hand over the twelve")) && Town->GetMark(FName("w2.nine")) == 3 && Town->GetMark(FName("w2.cut")) == 36);

		Town->Interact();

		StandWith(TEXT("Sergeant Rook"), FVector2D(-185.0f, 0.0f));
	} });

	// the count: Rook is told whose vest it came out of
	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(TEXT("Rook asks for the count"), Town->GetWords().Contains(TEXT("Count")));
		TestEqual(TEXT("There are three things that could be told him"), Town->GetAnswers().Num(), 3);
		TestTrue(TEXT("He is told who"), Say(TEXT("your own man's vest")) && Town->GetWords().Contains(TEXT("Off my own shift")));
		TestEqual(TEXT("He pays the walk"), Town->GetMark(FName("water")), 65);
		TestEqual(TEXT("And thinks the better of who told him"), Town->GetMark(FName("st.wardens")), 25);
		TestTrue(TEXT("The work is done, and a coat lent"), Town->GetMark(FName("w2")) == 100 && Town->GetMark(FName("coat")) == 1);
		TestTrue(TEXT("The Warden who took it is walked off the lane"), Town->GetMark(FName("lanewarden.sold")) == 1 && Town->IsPersonLed(Find(TEXT("Lane Warden"))));

		GutTests::Screenshot(TEXT("58_rook_is_told_who"));
	} });

	// the other ways it can be counted to him
	struct FEnd
	{
		const TCHAR* Name;
		const TCHAR* Stood;
		int32 Throw;
		const TCHAR* Told;
		const TCHAR* Says;
		int32 Held;
		int32 Wardens;
		const TCHAR* Then;
	};

	static const FEnd Ends[] =
	{
		{ TEXT("all three paid"), TEXT("w2.nine=2"), 3, TEXT("All three paid"), TEXT("a coat off the peg"), 45, 20, TEXT("coat") },
		{ TEXT("Tarp Nine short"), TEXT(""), 3, TEXT("Twenty-four"), TEXT("a new keeper"), 30, 15, TEXT("fen.gone") },
		{ TEXT("skimmed, and found out"), TEXT("w2.nine=1"), 1, TEXT("all short"), TEXT("a runner up the Row"), 0, -5, TEXT("w2.kept") },
		{ TEXT("skimmed, and not found out"), TEXT("w2.nine=1"), 2, TEXT("all short"), TEXT("Short weeks happen"), 60, 20, TEXT("w2.kept") },
	};

	for (const FEnd& End : Ends)
	{
		Steps.Add({ 0.6f, [this, End]()
		{
			ATownGameMode* Town = GetTown();
			UGutGameInstance* Run = GetRun();

			if (Town->IsTalking())
			{
				Town->Interact();
			}

			Run->Story = FTownStoryState();
			Run->Ladder.Water = 0.0f;

			Town->Apply(FString(TEXT("w1=100 st.wardens=10 w2=2 w2.sump=1 w2.det=1 w2.fen=1 ")) + End.Stood);
			Town->Thrown = End.Throw;

			Town->Tick(0.0f);
			Town->Interact();

			TestTrue(*FString::Printf(TEXT("%s: he asks for the count"), End.Name), Town->GetWords().Contains(TEXT("Count")));
			TestTrue(*FString::Printf(TEXT("%s: it can be told him so"), End.Name), Say(End.Told));

			FString Last;

			for (int32 i = 0; i < 6 && Town->IsTalking(); ++i)
			{
				Last = Town->GetWords();
				Town->Interact();
			}

			TestFalse(*FString::Printf(TEXT("%s: the talk comes to an end"), End.Name), Town->IsTalking());
			TestTrue(*FString::Printf(TEXT("%s: the last thing he says is the right one (%s)"), End.Name, *Last), Last.Contains(End.Says));
			TestEqual(*FString::Printf(TEXT("%s: what the player holds"), End.Name), Town->GetMark(FName("water")), End.Held);
			TestEqual(*FString::Printf(TEXT("%s: where the Wardens stand"), End.Name), Town->GetMark(FName("st.wardens")), End.Wardens);
			TestTrue(*FString::Printf(TEXT("%s: the work is done, and %s is so"), End.Name, End.Then), Town->GetMark(FName("w2")) == 100 && Town->GetMark(FName(End.Then)) == 1);

			Town->Thrown.Reset();
		} });
	}

	// found out, he has no more work for whoever counted his water short to his face
	Steps.Add({ 0.5f, [this, GoTo]()
	{
		ATownGameMode* Town = GetTown();

		Town->Apply(TEXT("st.wardens=-5"));
		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("Rook has nothing for somebody he caught (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("counted my water short")));

		Town->Interact();

		// and where Tarp Nine was short, it has changed hands
		Town->Apply(TEXT("fen.gone=1"));

		GoTo(TEXT("/Game/Town/Lvl_RotgutRow"), TEXT("from_pool_street"));
	} });

	Steps.Add({ 1.0f, [this, Kept]()
	{
		const ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		TestTrue(TEXT("Fen is gone from Tarp Nine, and somebody else has it"), !Town->GetPeople()[Find(TEXT("Fen"))] && Town->GetPeople()[Find(TEXT("A new keeper"))] != nullptr);

		GutTests::Screenshot(TEXT("59_rotgut_row"));

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	}, In(TEXT("Rotgut Row")) });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownRopeTest, "Ashmarket.Town.Rope", GutTests::Flags)

bool FTownRopeTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_VelvetTin"), true))
	{
		AddError(TEXT("Could not open the Velvet Tin's level"));
		return false;
	}

	TSharedRef<FKept> Kept = MakeShared<FKept>();

	const auto In = [](const TCHAR* Name)
	{
		return [Name]() { return IsStreetReady() && GetTown()->GetPlace() && GetTown()->GetPlace()->Name == Name; };
	};

	const auto GoTo = [](const TCHAR* Level, const TCHAR* Arrive)
	{
		UGutGameInstance* Run = GetRun();

		Run->TownArrive = FName(Arrive);
		Run->TownStood.Reset();

		UGameplayStatics::OpenLevel(GetTown(), FName(Level), true);
	};

	const auto StandWith = [](const TCHAR* Name, const FVector2D& Off)
	{
		const ATownGameMode* Town = GetTown();
		const int32 Who = Find(Name);

		if (Who != INDEX_NONE)
		{
			const FVector Theirs = Town->GetPlace()->People[Who].Place;

			StandOn(FVector2D(Theirs.X + Off.X, Theirs.Y + Off.Y), FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw);
		}

		return Who;
	};

	TArray<FScript::FStep> Steps;

	// somebody who has done the Wardens' first two pieces of work, with 60 L
	Steps.Add({ 0.5f, [this, Kept, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		Run->bStreetForced = true;
		Run->Story = FTownStoryState();
		Run->bLadderStarted = true;
		Run->Ladder.Water = 60.0f;

		TestTrue(TEXT("Madam Corazon and Captain Voss are in the court"), Find(TEXT("Madam Corazon")) != INDEX_NONE && Find(TEXT("Captain Voss")) != INDEX_NONE
			&& Town->GetPeople()[Find(TEXT("Madam Corazon"))] && Town->GetPeople()[Find(TEXT("Captain Voss"))]);

		StandWith(TEXT("Captain Voss"), FVector2D(-150.0f, 90.0f));
	}, In(TEXT("The Velvet Tin")) });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(TEXT("The Captain is in reach at the back table"), Town->GetPersonInReach(), Find(TEXT("Captain Voss")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("To somebody the Wardens do not know, the Captain has nothing (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("nothing I am selling")));

		Town->Interact();

		// with the Row's cut brought in
		Town->Apply(TEXT("w1=100 w2=100 st.wardens=20"));
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("With the first two done, the Captain has Brandt to be brought in (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("Ilya Brandt")));

		GutTests::Screenshot(TEXT("60_captain_voss_at_the_back_table"));
	} });

	Steps.Add({ 0.4f, [this, GoTo]()
	{
		ATownGameMode* Town = GetTown();

		TestTrue(TEXT("Why he did it can be asked, and is not answered"), Say(TEXT("What did he tap it for")) && Town->GetWords().Contains(TEXT("did not ask")));
		TestTrue(TEXT("The work is taken"), Say(TEXT("I will bring him")) && Town->GetMark(FName("w3")) == 2);

		Town->Interact();

		TestFalse(TEXT("And the talk ends"), Town->IsTalking());

		GoTo(TEXT("/Game/Town/Lvl_ScrappersYard"), TEXT("from_pool_street"));
	} });

	// the yard: nobody has heard the name, until the Guildmaster is paid
	Steps.Add({ 0.8f, [this, StandWith]()
	{
		const ATownGameMode* Town = GetTown();

		TestTrue(TEXT("The Guildmaster and the crew boss are in the yard"), Town->GetPeople()[Find(TEXT("Guildmaster Okafor-Reyes"))] && Town->GetPeople()[Find(TEXT("Pike"))]);
		TestTrue(TEXT("And Brandt is not to be seen"), !Town->GetPeople()[Find(TEXT("Ilya Brandt"))]);

		StandWith(TEXT("Guildmaster Okafor-Reyes"), FVector2D(-175.0f, 0.0f));
	}, In(TEXT("The Scrappers' Yard")) });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(TEXT("The Guildmaster is in reach"), Town->GetPersonInReach(), Find(TEXT("Guildmaster Okafor-Reyes")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("The Guildmaster names a price (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("Forty litres")));
		TestEqual(TEXT("There are four things to say to it"), Town->GetAnswers().Num(), 4);

		GutTests::Screenshot(TEXT("61_the_guildmaster_names_a_price"));
	} });

	Steps.Add({ 0.4f, [this, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		TestTrue(TEXT("Why he did it is answered here"), Say(TEXT("Why did he do it")) && Town->GetWords().Contains(TEXT("fever")));
		TestTrue(TEXT("Forty is paid"), Say(TEXT("Forty, for the crew")) && Town->GetMark(FName("water")) == 20 && Town->GetMark(FName("w3.where")) == 1 && Town->GetMark(FName("w3.how")) == 1);
		TestEqual(TEXT("The Guild thinks a little the less of it"), Town->GetMark(FName("st.scrappers")), -5);
		TestTrue(TEXT("And where he is is said"), Town->GetWords().Contains(TEXT("behind the tarp")));

		Town->Interact();

		TestFalse(TEXT("The talk ends"), Town->IsTalking());
		TestTrue(TEXT("Brandt is there to be found now"), Town->GetPeople()[Find(TEXT("Ilya Brandt"))] != nullptr);

		StandWith(TEXT("Ilya Brandt"), FVector2D(-150.0f, 0.0f));
	} });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(TEXT("Brandt is in reach behind the tarp"), Town->GetPersonInReach(), Find(TEXT("Ilya Brandt")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("He says what he did (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("two litres out of forty thousand")));
		TestEqual(TEXT("There are three things to do with him"), Town->GetAnswers().Num(), 3);

		GutTests::Screenshot(TEXT("62_ilya_brandt_behind_the_tarp"));
	} });

	Steps.Add({ 0.4f, [this, GoTo]()
	{
		ATownGameMode* Town = GetTown();

		TestTrue(TEXT("He is walked to the gate"), Say(TEXT("Walk in front of me")) && Town->GetMark(FName("w3.have")) == 1);
		TestTrue(TEXT("And walks off out of the yard"), Town->IsPersonLed(Find(TEXT("Ilya Brandt"))));

		Town->Interact();

		TestFalse(TEXT("With no more to be said to him"), Town->IsTalking());

		GoTo(TEXT("/Game/Town/Lvl_MainStreet"), TEXT("from_square"));
	} });

	// the gate: he is handed over
	Steps.Add({ 0.8f, [this, StandWith]()
	{
		const ATownGameMode* Town = GetTown();

		TestTrue(TEXT("Brandt is at the tally desk, brought in"), Town->GetPeople()[Find(TEXT("Ilya Brandt"))] != nullptr);
		TestTrue(TEXT("And nobody is under the gantry"), !Town->GetPeople()[Find(TEXT("Maren Brandt"))]);

		StandWith(TEXT("Sergeant Rook"), FVector2D(-185.0f, 0.0f));
	}, In(TEXT("Main Street")) });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);
		Town->Interact();

		TestTrue(*FString::Printf(TEXT("Rook takes him (%s)"), *Town->GetWords()), Town->GetWords().Contains(TEXT("Up the stair with him")));
		TestEqual(TEXT("Fifty is counted out"), Town->GetMark(FName("water")), 70);
		TestEqual(TEXT("The Wardens think the better of it"), Town->GetMark(FName("st.wardens")), 35);
		TestEqual(TEXT("And so does the Pump"), Town->GetMark(FName("st.pump")), 5);
		TestTrue(TEXT("He is to hang, and the work is done"), Town->GetMark(FName("brandt")) == 1 && Town->GetMark(FName("w3")) == 100);

		GutTests::Screenshot(TEXT("63_rook_takes_brandt"));
	} });

	Steps.Add({ 0.4f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Interact();

		TestFalse(TEXT("The talk ends"), Town->IsTalking());
		TestTrue(TEXT("Maren Brandt is under the gantry"), Town->GetPeople()[Find(TEXT("Maren Brandt"))] != nullptr);

		// and the gantry can be looked up at, now that there is somebody on it
		StandOn(OnMap(-0.8f, 14.1f), 180.0f);
	} });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		// (she is nearer than the door is: stepped a little off her, the door is what is in reach)
		if (Town->GetPersonInReach() == INDEX_NONE)
		{
			TestTrue(*FString::Printf(TEXT("The gantry is offered to be looked up at (%s)"), *Town->GetPrompt()), Town->GetPrompt().Contains(TEXT("gantry")));

			Town->Interact();

			TestTrue(*FString::Printf(TEXT("And what is on it is said (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("TAPPED THE LINE")));
		}
		else
		{
			TestEqual(TEXT("Maren Brandt is who is in reach under the gantry"), Town->GetPersonInReach(), Find(TEXT("Maren Brandt")));

			Town->Interact();

			TestTrue(TEXT("She says who is up there"), Town->GetWords().Contains(TEXT("That is Ilya")));

			Town->Interact();
		}

		GutTests::Screenshot(TEXT("64_under_the_gantry"));
	} });

	// the other ways it can be told to Rook
	struct FEnd
	{
		const TCHAR* Name;
		const TCHAR* Stood;
		int32 Throw;
		const TCHAR* Told;
		const TCHAR* Says;
		int32 Held;
		int32 Wardens;
	};

	static const FEnd Ends[] =
	{
		{ TEXT("let go, said dead, believed"), TEXT("brandt=2"), 2, TEXT("Brandt is dead"), TEXT("Dead is dead"), 50, 35 },
		{ TEXT("let go, said dead, not believed"), TEXT("brandt=2"), 1, TEXT("Brandt is dead"), TEXT("he is not dead"), 0, 10 },
		{ TEXT("let go, and said so"), TEXT("brandt=2"), 3, TEXT("I let him go"), TEXT("fifty litres go with him"), 0, 10 },
		{ TEXT("sold at the Block"), TEXT("brandt=3"), 3, TEXT(""), TEXT("not Mother Ledger's to sell"), 0, 10 },
	};

	for (const FEnd& End : Ends)
	{
		Steps.Add({ 0.6f, [this, End, StandWith]()
		{
			ATownGameMode* Town = GetTown();
			UGutGameInstance* Run = GetRun();

			if (Town->IsTalking())
			{
				Town->HandleBack();
			}

			Run->Story = FTownStoryState();
			Run->Ladder.Water = 0.0f;

			Town->Apply(FString(TEXT("w1=100 w2=100 st.wardens=20 w3=2 w3.where=1 ")) + End.Stood);
			Town->Thrown = End.Throw;

			StandWith(TEXT("Sergeant Rook"), FVector2D(-185.0f, 0.0f));

			Town->Tick(0.0f);
			Town->Interact();

			if (FCString::Strlen(End.Told) > 0)
			{
				TestTrue(*FString::Printf(TEXT("%s: it can be told him so"), End.Name), Say(End.Told));
			}

			FString Last;

			for (int32 i = 0; i < 6 && Town->IsTalking(); ++i)
			{
				Last = Town->GetWords();
				Town->Interact();
			}

			TestFalse(*FString::Printf(TEXT("%s: the talk comes to an end"), End.Name), Town->IsTalking());
			TestTrue(*FString::Printf(TEXT("%s: the last thing he says is the right one (%s)"), End.Name, *Last), Last.Contains(End.Says));
			TestEqual(*FString::Printf(TEXT("%s: what the player holds"), End.Name), Town->GetMark(FName("water")), End.Held);
			TestEqual(*FString::Printf(TEXT("%s: where the Wardens stand"), End.Name), Town->GetMark(FName("st.wardens")), End.Wardens);
			TestEqual(*FString::Printf(TEXT("%s: the work is over"), End.Name), Town->GetMark(FName("w3")), 100);

			Town->Thrown.Reset();
		} });
	}

	// and the way in among the sleepers, which is there only while he is being looked for
	Steps.Add({ 0.5f, [GoTo]()
	{
		GetRun()->Story = FTownStoryState();

		GoTo(TEXT("/Game/Town/Lvl_ScrappersYard"), TEXT("from_pool_street"));
	} });

	Steps.Add({ 0.8f, []()
	{
		StandOn(OnMap(135.7f, 206.2f), 0.0f);
	}, In(TEXT("The Scrappers' Yard")) });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestFalse(*FString::Printf(TEXT("With nobody being looked for, there is no going in under the tarp (%s)"), *Town->GetPrompt()), Town->GetPrompt().Contains(TEXT("under the tarp")));

		Town->Apply(TEXT("w3=2"));
		Town->Thrown = 1;
		Town->Tick(0.0f);

		TestTrue(*FString::Printf(TEXT("While he is being looked for, there is (%s)"), *Town->GetPrompt()), Town->GetPrompt().Contains(TEXT("under the tarp")));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("What is found is said (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("Eight people asleep")));
		TestTrue(TEXT("He is found, and somebody woke"), Town->GetMark(FName("w3.where")) == 1 && Town->GetMark(FName("w3.how")) == 4 && Town->GetMark(FName("st.scrappers")) == -15);
		TestTrue(TEXT("And he is there to be spoken to"), Town->GetPeople()[Find(TEXT("Ilya Brandt"))] != nullptr);

		Town->Thrown.Reset();
	} });

	Steps.Add({ 0.3f, [Kept]()
	{
		UGutGameInstance* Run = GetRun();

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.ArmorForOwned   armor is bought for a fighter the player owns (25 November: "so I personally have to up my
 *                                 rank via me fighting?? what if I want to buy armor for my slaves?"). Somebody who owns
 *                                 a fighter and fights nowhere themselves buys a vest at Needle's counter: it is in the
 *                                 bag; given from the bag it is on the fighter who walks with them; a second piece for
 *                                 the same part takes its place and the first comes back; and the cage knows the piece
 *                                 by its name, to put it on whoever has it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownArmorForOwnedTest, "Ashmarket.Town.ArmorForOwned", GutTests::Flags)

bool FTownArmorForOwnedTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	struct FWas
	{
		bool bLadder = false;
		FGutStableState Stable;
		TArray<FGutBagItem> Bag;
	};

	TSharedRef<FWas> Was = MakeShared<FWas>();

	// the bag's line for armor, and giving it: the bag opened, the line gone to, and asked for
	const auto Give = [this]()
	{
		ATownGameMode* Town = GetTown();

		if (!Town->IsBagOpen())
		{
			Town->ToggleBag();
		}

		const TArray<FTownBagLine> Lines = Town->GetBagLines();
		const int32 Armor = Lines.IndexOfByPredicate([](const FTownBagLine& Line) { return Line.Part == TEXT("ARMOR"); });

		if (!TestTrue(TEXT("The bag has armor in it, under its own heading"), Lines.IsValidIndex(Armor)))
		{
			return;
		}

		TestTrue(*FString::Printf(TEXT("And it can be given (%s)"), *Lines[Armor].Why), Lines[Armor].bCanUse && Lines[Armor].Verb == TEXT("give it"));

		for (int32 i = 0; i < 40 && Town->GetBagChoice() != Armor; ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);
		Town->ToggleBag();
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this, Was, Give]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Was->bLadder = Run->bLadderStarted;
		Was->Stable = Run->Stable;
		Was->Bag = Run->Bag;

		// somebody who owns one fighter, who is walking with them, and has never fought
		Run->bLadderStarted = false;
		Run->Bag.Reset();
		Run->Stable = FGutStableState();
		Run->Stable.bStarted = true;
		Run->Stable.Water = 60.0f;

		FGutStableFighter Mine;

		Mine.FighterName = FText::FromString(TEXT("Tally"));
		Mine.bOwned = true;
		Mine.bInTow = true;

		Run->Stable.Fighters.Add(Mine);

		const int32 Needle = Find(TEXT("Needle"));

		if (!TestTrue(TEXT("Needle is on Main Street"), Needle != INDEX_NONE))
		{
			return;
		}

		Town->OpenShop(Needle);

		TArray<FTownShopLine> Lines = Town->GetShopLines();

		if (!TestTrue(TEXT("There is armor on the counter"), Lines.Num() >= 4))
		{
			return;
		}

		TestTrue(*FString::Printf(TEXT("An owner who does not fight is told where it goes, and is not turned away (%s)"), *Town->GetBagSaid()), Town->GetBagSaid().Contains(TEXT("goes in your bag")));
		TestTrue(*FString::Printf(TEXT("And can buy the first piece with 60 L (%s)"), *Lines[0].Why), Lines[0].bCanBuy);

		const float Held = Town->GetShopWater();

		TestEqual(TEXT("What the counter counts as held is what the owner holds"), Held, 60.0f);
		TestTrue(TEXT("It is bought"), Town->BuyAtShop(0));
		TestEqual(TEXT("And paid for"), Town->GetShopWater(), Held - Lines[0].Price);
		TestTrue(TEXT("And is in the bag"), Run->Bag.Num() == 1 && Run->Bag[0].Count == 1);

		const FName First = Run->Bag.IsEmpty() ? NAME_None : Run->Bag[0].Good;

		Town->CloseShop();

		Give();

		TestTrue(TEXT("Given, it is on the fighter who walks with the owner"), Run->Stable.Fighters[0].Armor.Contains(First));
		TestEqual(TEXT("And out of the bag"), Run->GetBagCount(First), 0);

		// another piece for the same part of the body: it takes the first one's place
		Town->OpenShop(Needle);

		Lines = Town->GetShopLines();

		int32 Same = INDEX_NONE;

		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			if (Lines[i].Detail.Left(5) == Lines[0].Detail.Left(5) && Lines[i].bCanBuy)
			{
				Same = i;
				break;
			}
		}

		if (TestTrue(TEXT("There is a second piece for the same part that 40 L will buy"), Same != INDEX_NONE) && TestTrue(TEXT("Which is bought"), Town->BuyAtShop(Same)))
		{
			Town->CloseShop();

			Give();

			TestEqual(TEXT("The fighter has one piece on, the second"), Run->Stable.Fighters[0].Armor.Num(), 1);
			TestFalse(TEXT("Not the first"), Run->Stable.Fighters[0].Armor.Contains(First));
			TestEqual(TEXT("And the first is back in the bag"), Run->GetBagCount(First), 1);
		}

		Town->CloseShop();

		// what the cage lets in
		const AGutGameMode* Rules = GetDefault<AGutGameMode>(LoadClass<AGutGameMode>(nullptr, TEXT("/Game/Gut/Blueprints/BP_GutGameMode.BP_GutGameMode_C")));

		if (!TestNotNull(TEXT("The cage's rules are there to be asked"), Rules))
		{
			return;
		}

		TestTrue(TEXT("And the piece is found by its name, to be put on in the cage"), Rules->FindArmorPiece(First) != nullptr);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.3f, [Was]()
	{
		UGutGameInstance* Run = GetRun();

		Run->bLadderStarted = Was->bLadder;
		Run->Stable = Was->Stable;
		Run->Bag = Was->Bag;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.Rack   the yard is where things are kept (25 November: "the yard is for what? ... food I store there?").
 *                        At the rack in the yard, bread and tea go out of the bag onto it and come off it again, one at a
 *                        time, and the rack remembers what it holds.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownRackTest, "Ashmarket.Town.Rack", GutTests::Flags)

bool FTownRackTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	// an owner with one fighter standing in the yard, and bread and tea in the bag: before the yard is opened, which stands them
	struct FWas
	{
		bool bLadder = false;
		FGutStableState Stable;
		TArray<FGutBagItem> Bag;
		TArray<FGutBagItem> Rack;
	};

	TSharedRef<FWas> Was = MakeShared<FWas>();

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_Yard"), true))
	{
		AddError(TEXT("Could not open the yard's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.6f, [this, Was]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Was->bLadder = Run->bLadderStarted;
		Was->Stable = Run->Stable;
		Was->Bag = Run->Bag;
		Was->Rack = Run->Rack;

		Run->bLadderStarted = false;
		Run->Stable = FGutStableState();
		Run->Stable.bStarted = true;
		Run->Stable.Water = 20.0f;
		Run->Bag.Reset();
		Run->Rack.Reset();
		Run->AddToBag(FName("bread"), 2);
		Run->AddToBag(FName("tea"), 1);

		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Door = Place ? Place->Doors.IndexOfByPredicate([](const FTownDoor& One) { return One.Deed == ETownDeed::Rack; }) : INDEX_NONE;

		if (!TestTrue(TEXT("The yard has a rack to go to"), Door != INDEX_NONE))
		{
			return;
		}

		StandOn(FVector2D(Place->Doors[Door].Place.X - 90.0f, Place->Doors[Door].Place.Y), 0.0f);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->Tick(0.0f);

		TestTrue(*FString::Printf(TEXT("Standing at it, the rack is what is offered (%s)"), *Town->GetPrompt()), Town->GetPrompt().Contains(TEXT("rack")));

		Town->Interact();

		if (!TestTrue(TEXT("Its page opens"), Town->IsAtYardPage()))
		{
			return;
		}

		TArray<FTownShopLine> Lines = Town->GetShopLines();

		TestEqual(TEXT("With nothing on it, there are the two kinds of thing in the bag to put on it"), Lines.Num(), 2);
		TestTrue(*FString::Printf(TEXT("The first is the bread (%s)"), Lines.IsEmpty() ? TEXT("") : *Lines[0].Name), !Lines.IsEmpty() && Lines[0].Name.Contains(TEXT("Put on the rack")) && Lines[0].Name.Contains(TEXT("bread")));

		// one loaf onto the rack
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("One loaf is on the rack"), Run->GetRackCount(FName("bread")), 1);
		TestEqual(TEXT("And one is left in the bag"), Run->GetBagCount(FName("bread")), 1);

		Lines = Town->GetShopLines();

		TestTrue(*FString::Printf(TEXT("What is on the rack is listed first, to be taken off (%s)"), Lines.IsEmpty() ? TEXT("") : *Lines[0].Name),
			Lines.Num() == 3 && Lines[0].Name.Contains(TEXT("Take off the rack")) && Lines[0].Name.Contains(TEXT("bread")));

		// the tea too: the third line now
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("The tea is on the rack"), Run->GetRackCount(FName("tea")), 1);
		TestEqual(TEXT("And out of the bag"), Run->GetBagCount(FName("tea")), 0);

		// and they are there to be seen: a loaf and a kettle on its shelves ("I see nothing on the rack", 25 November)
		TestEqual(TEXT("What is on the rack stands on its shelves"), Town->GetRackThings().Num(), 2);

		for (const AActor* Thing : Town->GetRackThings())
		{
			const FVector At = Thing ? Thing->GetActorLocation() : FVector::ZeroVector;
			const float Up = At.Z - Town->GetPlace()->GetGroundZ(At.X);

			TestTrue(*FString::Printf(TEXT("%.0f cm off the ground, on a shelf, within the rack"), Up), Thing && Up > 30.0f && Up < 170.0f);
		}

		// and the loaf back off it: the first line
		Town->HandleMenuInput(EGutMenuInput::Up);
		Town->HandleMenuInput(EGutMenuInput::Up);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("The loaf is off the rack"), Run->GetRackCount(FName("bread")), 0);
		TestEqual(TEXT("And both are in the bag again"), Run->GetBagCount(FName("bread")), 2);
		TestTrue(*FString::Printf(TEXT("What was done is said (%s)"), *Town->GetBagSaid()), Town->GetBagSaid().Contains(TEXT("off the rack")));

		GutTests::Screenshot(TEXT("72_the_rack"));
	} });

	Steps.Add({ 0.5f, [this, Was]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->HandleBack();

		TestFalse(TEXT("Leaving shuts the page"), Town->IsAtYardPage());
		TestEqual(TEXT("With the loaf taken back, the kettle is what is left on the shelves"), Town->GetRackThings().Num(), 1);

		GutTests::Screenshot(TEXT("72b_on_the_rack"));

		Run->bLadderStarted = Was->bLadder;
		Run->Stable = Was->Stable;
		Run->Bag = Was->Bag;
		Run->Rack = Was->Rack;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.Tabs   the Last Call's first piece of work: taken from Captain Dolores on the Apron, the three tabs got
 *                        out of the three who owe them (one leaned on, one met half way, one threatened), counted onto
 *                        her table, and the board read afterwards; then the two other ways it can come out: paid out of
 *                        the player's own water, and the names sold to Mother Ledger.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownTabsTest, "Ashmarket.Town.Tabs", GutTests::Flags)

bool FTownTabsTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_LastCall"), true))
	{
		AddError(TEXT("Could not open the Last Call's level"));
		return false;
	}

	TSharedRef<FKept> Kept = MakeShared<FKept>();

	// stood by somebody, so far to their north and east (the level's X and Y), facing them
	const auto StandWith = [](const TCHAR* Name, const FVector2D& Off)
	{
		const ATownGameMode* Town = GetTown();
		const int32 Who = Find(Name);

		if (Who != INDEX_NONE)
		{
			const FVector Theirs = Town->GetPlace()->People[Who].Place;

			StandOn(FVector2D(Theirs.X + Off.X, Theirs.Y + Off.Y), FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw);
		}

		return Who;
	};

	// talks to whoever is in reach, says the one thing, and hears them out
	const auto Ask = [this](const TCHAR* Of, const TCHAR* Hears, const TCHAR* Says)
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		TestEqual(*FString::Printf(TEXT("%s is in reach"), Of), Town->GetPersonInReach(), Find(Of));

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("%s says the thing (%s)"), Of, *Town->GetWords()), Town->GetWords().Contains(Hears));

		if (FCString::Strlen(Says) > 0)
		{
			TestTrue(*FString::Printf(TEXT("%s can be told \"%s\""), Of, Says), Say(Says));
		}

		for (int32 i = 0; i < 4 && Town->IsTalking(); ++i)
		{
			Town->Interact();
		}

		if (Town->IsTalking())
		{
			Town->HandleBack();
		}
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [this, Kept, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Kept->bLadder = Run->bLadderStarted;
		Kept->Ladder = Run->Ladder;
		Kept->RungStart = Run->LadderAtRungStart;
		Kept->Story = Run->Story;
		Kept->Bag = Run->Bag;

		Run->bStreetForced = true;
		Run->Story = FTownStoryState();
		Run->bLadderStarted = true;
		Run->Ladder.Water = 10.0f;

		for (const TCHAR* Name : { TEXT("Captain Dolores"), TEXT("A Warden corporal"), TEXT("A winch hand"), TEXT("A Chainmen collector") })
		{
			TestTrue(*FString::Printf(TEXT("%s is on the Apron"), Name), Find(Name) != INDEX_NONE && Town->GetPeople()[Find(Name)] != nullptr);
		}

		StandWith(TEXT("Captain Dolores"), FVector2D(0.0f, 205.0f));
	}, []() { return IsStreetReady() && GetTown()->GetPlace() && GetTown()->GetPlace()->Name == TEXT("The Last Call"); } });

	Steps.Add({ 1.0f, [this, Ask, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Ask(TEXT("Captain Dolores"), TEXT("I have a job for you"), TEXT("I will get it"));

		TestEqual(TEXT("The work is taken"), Town->GetMark(FName("d1")), 2);

		StandWith(TEXT("A Warden corporal"), FVector2D(0.0f, 150.0f));
	} });

	Steps.Add({ 0.8f, [this, Ask, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Ask(TEXT("A Warden corporal"), TEXT("whistle"), TEXT("Twelve litres. Now."));

		TestTrue(TEXT("Leaned on, the corporal pays twelve and is the more afraid"), Town->GetMark(FName("tab.w")) == 1 && Town->GetMark(FName("water")) == 22 && Town->GetMark(FName("fear")) == 1);

		StandWith(TEXT("A winch hand"), FVector2D(0.0f, 150.0f));
	} });

	Steps.Add({ 0.8f, [this, Ask, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Ask(TEXT("A winch hand"), TEXT("Nine"), TEXT("Give me five"));

		TestTrue(TEXT("Met half way, the winch hand gives five"), Town->GetMark(FName("tab.g")) == 1 && Town->GetMark(FName("water")) == 27);
		TestEqual(TEXT("And the Guild thinks the better of it"), Town->GetMark(FName("st.scrappers")), 2);

		StandWith(TEXT("A Chainmen collector"), FVector2D(0.0f, -150.0f));
	} });

	Steps.Add({ 0.8f, [this, Ask, StandWith]()
	{
		ATownGameMode* Town = GetTown();

		Ask(TEXT("A Chainmen collector"), TEXT("I am not collected from"), TEXT("Thirteen, or"));

		TestTrue(TEXT("The collector pays last"), Town->GetMark(FName("tab.c")) == 1 && Town->GetMark(FName("water")) == 40);
		TestEqual(TEXT("And the Chainmen remember it"), Town->GetMark(FName("st.chainmen")), -5);

		StandWith(TEXT("Captain Dolores"), FVector2D(0.0f, 205.0f));
	} });

	Steps.Add({ 0.8f, [this, Ask]()
	{
		ATownGameMode* Town = GetTown();

		Ask(TEXT("Captain Dolores"), TEXT("All three"), TEXT("Thirty-four, Captain"));

		TestEqual(TEXT("Thirty-four goes onto her table and eight comes back"), Town->GetMark(FName("water")), 14);
		TestEqual(TEXT("The work is done"), Town->GetMark(FName("d1")), 100);

		Ask(TEXT("Captain Dolores"), TEXT("Seat-belt sign is off"), TEXT(""));

		// the board, which stands by her: a little along from her table
		StandOn(OnMap(-37.5f, 262.0f), 270.0f);
	} });

	Steps.Add({ 0.8f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->Tick(0.0f);

		if (Town->GetPersonInReach() == INDEX_NONE)
		{
			TestTrue(*FString::Printf(TEXT("The board is offered to be read (%s)"), *Town->GetPrompt()), Town->GetPrompt().Contains(TEXT("jobs board")));

			Town->Interact();

			TestTrue(*FString::Printf(TEXT("With her work done it shows the good cards (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("good cards are out")));

			Town->Apply(TEXT("bounty=20"));
			Town->Tick(0.0f);
			Town->Interact();

			TestTrue(*FString::Printf(TEXT("And a bounty against the player is on it (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("TWENTY LITRES")));
		}
		else
		{
			AddError(TEXT("Somebody is nearer than the jobs board where the test stands to read it"));
		}

		GutTests::Screenshot(TEXT("75_the_jobs_board"));
	} });

	// paid out of the player's own water
	Steps.Add({ 0.6f, [this, Ask, StandWith]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Run->Story = FTownStoryState();
		Run->Ladder.Water = 40.0f;

		Town->Apply(TEXT("d1=2"));

		StandWith(TEXT("Captain Dolores"), FVector2D(0.0f, 205.0f));

		Ask(TEXT("Captain Dolores"), TEXT("not brought me my thirty-four"), TEXT("I will cover it myself"));

		TestTrue(TEXT("Covered, it costs thirty-four and pays eight, and she trusts the more"), Town->GetMark(FName("water")) == 14 && Town->GetMark(FName("trust")) == 43 && Town->GetMark(FName("d1")) == 100);

		// and the names sold to the book
		Run->Story = FTownStoryState();

		Town->Apply(TEXT("d1=2 d1.sold=1 tab.w=2 tab.g=2 tab.c=2"));

		Ask(TEXT("Captain Dolores"), TEXT("sold my regulars"), TEXT(""));

		TestTrue(TEXT("She knows, and it is over between them"), Town->GetMark(FName("d1")) == 100 && Town->GetMark(FName("dolores")) == -1);

		Ask(TEXT("Captain Dolores"), TEXT("Seat-belt sign is on for you"), TEXT(""));

		GutTests::Screenshot(TEXT("76_captain_dolores"));
	} });

	Steps.Add({ 0.4f, [Kept]()
	{
		UGutGameInstance* Run = GetRun();

		Run->bLadderStarted = Kept->bLadder;
		Run->Ladder = Kept->Ladder;
		Run->LadderAtRungStart = Kept->RungStart;
		Run->Story = Kept->Story;
		Run->Bag = Kept->Bag;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.NoteBook   Mother Ledger's note has her book on it (25 November: "THE NOTE is incomplete - I need to see
 *                            what I borrowed and what I owe and what I paid back - transaction by transaction on the
 *                            sheet"). Two loans and a payment at her table are three rows on the note in the bag, each
 *                            with what it did to the debt and what was owed after; more rows than a page holds are turned
 *                            through, and it opens on the last; a debt from before the book was kept is its first row.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownNoteBookTest, "Ashmarket.Town.NoteBook", GutTests::Flags)

bool FTownNoteBookTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	struct FWas
	{
		FGutWindowState Window;
		TArray<FGutDebtLine> Book;
	};

	TSharedRef<FWas> Was = MakeShared<FWas>();

	const auto Note = []()
	{
		const TArray<FTownBagLine> Lines = GetTown()->GetBagLines();

		return Lines.IndexOfByPredicate([](const FTownBagLine& Line) { return Line.Kind == ETownBagKind::Note; });
	};

	const auto StandByHer = []()
	{
		const int32 Lender = Find(TEXT("Mother Ledger"));

		if (Lender != INDEX_NONE)
		{
			const FVector Hers = GetTown()->GetPlace()->People[Lender].Place;

			StandOn(FVector2D(Hers.X, Hers.Y - 170.0f), 90.0f);
		}

		return Lender;
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [this, Was, StandByHer]()
	{
		UGutGameInstance* Run = GetRun();

		Was->Window = Run->Window;
		Was->Book = Run->DebtBook;

		Run->Window = FGutWindowState();
		Run->Window.bStarted = true;
		Run->Window.Water = 10.0f;
		Run->DebtBook.Reset();

		TestTrue(TEXT("Mother Ledger is on the square"), StandByHer() != INDEX_NONE);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 1.0f, [this, Note]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->Tick(0.0f);

		TestTrue(TEXT("With nothing owed there is no note in the bag"), Note() == INDEX_NONE);

		Town->Interact();

		// borrowed twice, and then all that is held paid back
		Town->HandleMenuInput(EGutMenuInput::Confirm);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Two loans are two lines in her book"), Run->DebtBook.Num(), 2);
		TestEqual(TEXT("And seventy is owed"), Run->Window.Debt, 70.0f);

		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Sixty paid leaves ten"), Run->Window.Debt, 10.0f);
		TestEqual(TEXT("And is a third line"), Run->DebtBook.Num(), 3);

		Town->HandleBack();

		const TArray<FTownBagLine> Lines = Town->GetBagLines();
		const int32 Paper = Note();

		if (!TestTrue(TEXT("The note is in the bag"), Lines.IsValidIndex(Paper)) || !TestEqual(TEXT("With a row for each"), Lines[Paper].Sheet.Num(), 3))
		{
			return;
		}

		const TArray<FTownSheetRow>& Rows = Lines[Paper].Sheet;

		TestEqual(TEXT("What is owed is said outright"), Lines[Paper].Count, FString(TEXT("you owe 10 L")));
		TestTrue(*FString::Printf(TEXT("The first row is a loan, with what was counted out (%s)"), *Rows[0].What), Rows[0].What.Contains(TEXT("lent")) && Rows[0].What.Contains(TEXT("25 L counted out")));
		TestTrue(*FString::Printf(TEXT("Written as thirty-five, and thirty-five owed after it (%s, %s)"), *Rows[0].Sum, *Rows[0].Owed), Rows[0].Sum == TEXT("+35") && Rows[0].Owed == TEXT("35"));
		TestTrue(TEXT("The second leaves seventy owed"), Rows[1].Sum == TEXT("+35") && Rows[1].Owed == TEXT("70"));
		TestTrue(*FString::Printf(TEXT("The third is the payment, and leaves ten (%s, %s, %s)"), *Rows[2].What, *Rows[2].Sum, *Rows[2].Owed),
			Rows[2].What.Contains(TEXT("paid")) && Rows[2].Sum == TEXT("-60") && Rows[2].Owed == TEXT("10"));

		// more than a page of it: a night of small sums kept at the window
		for (int32 i = 0; i < 14; ++i)
		{
			Run->Window.Debt += 2.0f;
			Run->WriteDebt(i % 3 == 0 ? TEXT("kept at the window, of 54 L won") : TEXT("lent at the window"), i % 3 == 0 ? -27.0f : 25.0f, i % 3 == 0 ? -27.0f : 35.0f, Run->Window.Debt);
		}

		Town->ToggleBag();

		for (int32 i = 0; i < 40 && Town->GetBagChoice() != Note(); ++i)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		TestEqual(TEXT("The bag is open on the note"), Town->GetBagChoice(), Note());
		TestEqual(TEXT("Seventeen rows are two pages"), ATownGameMode::GetSheetPages(17), 2);
		TestEqual(TEXT("It opens on the last, where what is owed now is"), Town->GetSheetPage(17), 1);

		Town->HandleMenuInput(EGutMenuInput::Right);

		TestEqual(TEXT("There is no page after the last"), Town->GetSheetPage(17), 1);

		GutTests::Screenshot(TEXT("70_the_note_last_page"));
	} });

	Steps.Add({ 0.5f, [this]()
	{
		ATownGameMode* Town = GetTown();

		Town->HandleMenuInput(EGutMenuInput::Left);

		TestEqual(TEXT("It is turned back to the first"), Town->GetSheetPage(17), 0);

		Town->HandleMenuInput(EGutMenuInput::Left);

		TestEqual(TEXT("And no further"), Town->GetSheetPage(17), 0);

		GutTests::Screenshot(TEXT("71_the_note_first_page"));
	} });

	// a debt from before the book was kept
	Steps.Add({ 0.5f, [this, Note, StandByHer]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		Town->ToggleBag();

		Run->DebtBook.Reset();
		Run->Window.Debt = 67.0f;
		Run->Window.Water = 7.0f;

		TArray<FTownBagLine> Lines = Town->GetBagLines();

		TestTrue(TEXT("An old debt with no book has the one row"), Lines.IsValidIndex(Note()) && Lines[Note()].Sheet.Num() == 1 && Lines[Note()].Sheet[0].Owed == TEXT("67"));

		StandByHer();

		Town->Tick(0.0f);
		Town->Interact();
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);
		Town->HandleBack();

		Lines = Town->GetBagLines();

		TestEqual(TEXT("Seven paid off it leaves sixty"), Run->Window.Debt, 60.0f);
		TestTrue(TEXT("And the book begins with what was owed before, and then the payment"), Lines.IsValidIndex(Note()) && Lines[Note()].Sheet.Num() == 2
			&& Lines[Note()].Sheet[0].Owed == TEXT("67") && Lines[Note()].Sheet[1].Sum == TEXT("-7") && Lines[Note()].Sheet[1].Owed == TEXT("60"));
	} });

	Steps.Add({ 0.2f, [Was]()
	{
		UGutGameInstance* Run = GetRun();

		Run->Window = Was->Window;
		Run->DebtBook = Was->Book;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

namespace StoryTests
{
	/** Where a picture is taken from and what it looks at, in the map's own metres (x east, y north, z up) */
	struct FView
	{
		const TCHAR* Name;
		FVector From;
		FVector At;
		float FieldOfView;
	};

	/** Opens a place and saves fixed views of it as pictures: -GutLookOnly= keeps those whose name has it */
	static bool Look(FAutomationTestBase* Test, const TCHAR* Level, TConstArrayView<FView> Views)
	{
		using namespace TownTests;

		if (!AutomationOpenMap(Level, true))
		{
			Test->AddError(FString::Printf(TEXT("Could not open %s"), Level));
			return false;
		}

		FString Only;
		FParse::Value(FCommandLine::Get(), TEXT("GutLookOnly="), Only);

		TSharedRef<TWeakObjectPtr<ACameraActor>> Camera = MakeShared<TWeakObjectPtr<ACameraActor>>();

		TArray<FScript::FStep> Steps;

		Steps.Add({ 10.0f, [Camera]()
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
				const auto InLevel = [](const FVector& OnTheMap) { return FVector((OnTheMap.Y - 208.0f) * 100.0f, (OnTheMap.X - 95.0f) * 100.0f, (OnTheMap.Z - 3.0f) * 100.0f); };

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

		ADD_LATENT_AUTOMATION_COMMAND(FScript(Test, MoveTemp(Steps)));

		return true;
	}
}


/**
 *  Ashmarket.Probe.Tow   Not a test: a look. Somebody the player owns walks behind them into a place, and is photographed
 *                        from in front, from close and from further off, over the first seconds: to catch what a
 *                        follower's body does that a fighter's in the cage does not (25 November: bright green forearms
 *                        and legs on a follower beside Rook). -GutProbeBody=Isaiah is whose body; -GutProbePlace=MainStreet
 *                        the place; -GutProbeHurt gives it the hurts of a fight; -GutProbeArmor has it wear the first
 *                        piece on the counter. Rendered runs only.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownProbeTowTest, "Ashmarket.Probe.Tow", GutTests::Flags)

bool FTownProbeTowTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	FString Body = TEXT("Isaiah");
	FString Place = TEXT("MainStreet");

	FParse::Value(FCommandLine::Get(), TEXT("GutProbeBody="), Body);
	FParse::Value(FCommandLine::Get(), TEXT("GutProbePlace="), Place);

	const bool bHurt = FParse::Param(FCommandLine::Get(), TEXT("GutProbeHurt"));

	// the owner and whoever is in tow are made before the place is: it stands them as it opens
	{
		UGutGameInstance* Run = Cast<UGutGameInstance>(GEngine->GetWorldContexts().IsEmpty() ? nullptr : GEngine->GetWorldContexts()[0].OwningGameInstance.Get());

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UGutGameInstance* One = Cast<UGutGameInstance>(Context.OwningGameInstance))
			{
				Run = One;
			}
		}

		if (!Run)
		{
			AddError(TEXT("There is no run to put an owner in"));
			return false;
		}

		Run->bLadderStarted = false;
		Run->Stable = FGutStableState();
		Run->Stable.bStarted = true;
		Run->Stable.Water = 30.0f;

		FGutStableFighter Mine;

		Mine.FighterName = FText::FromString(TEXT("Crow"));
		Mine.Body = TSoftClassPtr<AActor>(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), *Body, *Body, *Body)));
		Mine.bOwned = false;
		Mine.bInTow = true;

		if (bHurt)
		{
			Mine.LimbHealth[0] = 12.0f;
			Mine.LimbHealth[2] = 9.0f;
			Mine.LimbHealth[3] = 15.0f;
		}

		// -GutProbeArmor=DA_Gut_Armor_RubberVest,DA_Gut_Armor_RubberGreaves has it wear those pieces
		FString Pieces;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutProbeArmor="), Pieces, false))
		{
			TArray<FString> Names;
			Pieces.ParseIntoArray(Names, TEXT(","));

			for (const FString& Name : Names)
			{
				Mine.Armor.Add(FName(*Name));
			}
		}

		Run->Stable.Fighters.Add(Mine);
	}

	if (!AutomationOpenMap(FString::Printf(TEXT("/Game/Town/Lvl_%s"), *Place), true))
	{
		AddError(TEXT("Could not open the place"));
		return false;
	}

	TSharedRef<TWeakObjectPtr<ACameraActor>> Camera = MakeShared<TWeakObjectPtr<ACameraActor>>();

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.2f, [Camera]()
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

	struct FShot
	{
		float After;
		float Off;
		float Side;
		const TCHAR* Name;
	};

	static const FShot Shots[] =
	{
		{ 0.3f, 260.0f, 60.0f, TEXT("tow_1_at_once_close") },
		{ 1.2f, 600.0f, 200.0f, TEXT("tow_2_soon_far") },
		{ 2.5f, 260.0f, -80.0f, TEXT("tow_3_after_3s_close") },
		{ 4.0f, 900.0f, 0.0f, TEXT("tow_4_after_7s_far") },
		{ 6.0f, 240.0f, 240.0f, TEXT("tow_5_after_13s_side") },
	};

	for (const FShot& Shot : Shots)
	{
		Steps.Add({ Shot.After, [this, Camera, Shot]()
		{
			const ATownGameMode* Town = GetTown();
			const AGutFighter* Towed = Town->GetTowBody(0);

			if (!TestNotNull(TEXT("Somebody is in tow"), Towed) || !Camera->IsValid())
			{
				return;
			}

			const FVector Middle = Towed->GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);
			const FVector From = Middle + (Towed->GetActorForwardVector() * Shot.Off) + (Towed->GetActorRightVector() * Shot.Side) + FVector(0.0f, 0.0f, 40.0f);

			Camera->Get()->SetActorLocationAndRotation(From, (Middle - From).Rotation());
			Camera->Get()->GetCameraComponent()->SetFieldOfView(Shot.Off > 500.0f ? 30.0f : 50.0f);
			Camera->Get()->GetCameraComponent()->SetConstraintAspectRatio(false);
		} });

		Steps.Add({ 0.5f, [Shot]() { GutTests::Screenshot(Shot.Name); } });
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Look.RotgutRow, .VelvetTin, .ScrappersYard   fixed views of a place saved as pictures. Not part of a test run.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookRotgutRowTest, "Ashmarket.Look.RotgutRow", GutTests::Flags)

bool FTownLookRotgutRowTest::RunTest(const FString& Parameters)
{
	using StoryTests::FView;

	static const FView Views[] =
	{
		{ TEXT("row_01_coming_down_the_cut"), FVector(-38.6f, 294.0f, 10.7f), FVector(-46.0f, 300.0f, 10.6f), 70.0f },
		{ TEXT("row_02_up_the_furrow"), FVector(-45.0f, 293.6f, 10.7f), FVector(-45.0f, 330.0f, 11.0f), 68.0f },
		{ TEXT("row_03_the_sump"), FVector(-44.2f, 298.4f, 10.7f), FVector(-49.6f, 302.6f, 11.4f), 66.0f },
		{ TEXT("row_04_under_the_wing"), FVector(-45.6f, 312.0f, 10.7f), FVector(-44.6f, 300.0f, 13.0f), 72.0f },
		{ TEXT("row_05_tarp_nine"), FVector(-44.0f, 325.6f, 10.7f), FVector(-49.6f, 329.6f, 11.4f), 62.0f },
		{ TEXT("row_06_detention"), FVector(-45.4f, 326.0f, 10.7f), FVector(-45.0f, 339.0f, 11.0f), 64.0f },
		{ TEXT("row_07_back_down_to_the_tail"), FVector(-45.2f, 322.0f, 10.7f), FVector(-45.0f, 288.0f, 12.5f), 66.0f },
		{ TEXT("row_08_east_stalls"), FVector(-46.6f, 305.0f, 10.7f), FVector(-40.2f, 309.0f, 11.3f), 66.0f },
		{ TEXT("row_09_from_above"), FVector(-45.0f, 314.0f, 58.0f), FVector(-44.9f, 314.5f, 9.0f), 62.0f },
		{ TEXT("row_10_from_the_west_bank"), FVector(-62.0f, 300.0f, 15.0f), FVector(-45.0f, 312.0f, 11.0f), 62.0f },
		{ TEXT("row_11_the_tail_from_the_furrow"), FVector(-44.4f, 297.4f, 10.7f), FVector(-45.0f, 286.0f, 14.5f), 80.0f },
		{ TEXT("row_12_the_tail_from_the_east_bank"), FVector(-28.0f, 300.0f, 13.0f), FVector(-45.0f, 281.0f, 14.0f), 70.0f },
	};

	return StoryTests::Look(this, TEXT("/Game/Town/Lvl_RotgutRow"), Views);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookVelvetTinTest, "Ashmarket.Look.VelvetTin", GutTests::Flags)

bool FTownLookVelvetTinTest::RunTest(const FString& Parameters)
{
	using StoryTests::FView;

	static const FView Views[] =
	{
		{ TEXT("tin_01_in_at_the_curtain"), FVector(-86.0f, 189.6f, 7.7f), FVector(-97.0f, 206.0f, 7.6f), 72.0f },
		{ TEXT("tin_02_the_trailers"), FVector(-96.0f, 194.0f, 7.7f), FVector(-96.0f, 209.0f, 7.6f), 78.0f },
		{ TEXT("tin_03_madam_corazon"), FVector(-95.2f, 203.4f, 7.6f), FVector(-96.0f, 206.3f, 7.5f), 60.0f },
		{ TEXT("tin_04_the_back_table"), FVector(-108.6f, 202.2f, 7.6f), FVector(-112.0f, 205.0f, 7.3f), 62.0f },
		{ TEXT("tin_05_back_to_the_gate"), FVector(-104.0f, 205.0f, 7.7f), FVector(-86.0f, 188.0f, 7.4f), 66.0f },
		{ TEXT("tin_06_the_court_from_the_west"), FVector(-114.6f, 192.0f, 7.7f), FVector(-92.0f, 202.0f, 7.4f), 70.0f },
		{ TEXT("tin_07_from_above"), FVector(-98.0f, 199.0f, 46.0f), FVector(-97.9f, 200.0f, 6.0f), 62.0f },
		{ TEXT("tin_08_from_the_south_east"), FVector(-74.0f, 180.0f, 16.0f), FVector(-98.0f, 202.0f, 7.0f), 60.0f },
	};

	return StoryTests::Look(this, TEXT("/Game/Town/Lvl_VelvetTin"), Views);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookLastCallTest, "Ashmarket.Look.LastCall", GutTests::Flags)

bool FTownLookLastCallTest::RunTest(const FString& Parameters)
{
	using StoryTests::FView;

	static const FView Views[] =
	{
		{ TEXT("call_01_in_round_the_tail"), FVector(-37.0f, 275.4f, 11.9f), FVector(-36.0f, 255.0f, 11.6f), 75.0f },
		{ TEXT("call_02_the_captains_table"), FVector(-35.4f, 260.0f, 11.8f), FVector(-39.5f, 258.6f, 11.6f), 62.0f },
		{ TEXT("call_03_the_wreck_from_the_apron"), FVector(-24.0f, 258.0f, 11.9f), FVector(-45.0f, 254.0f, 13.0f), 85.0f },
		{ TEXT("call_04_the_fire"), FVector(-28.6f, 266.6f, 11.8f), FVector(-33.6f, 261.6f, 11.2f), 70.0f },
		{ TEXT("call_05_south_tables"), FVector(-32.0f, 255.0f, 11.8f), FVector(-34.0f, 246.0f, 11.2f), 72.0f },
		{ TEXT("call_06_the_board"), FVector(-36.2f, 262.6f, 11.8f), FVector(-39.2f, 261.4f, 11.4f), 60.0f },
		{ TEXT("call_07_from_above"), FVector(-33.0f, 258.0f, 60.0f), FVector(-32.9f, 259.0f, 10.0f), 62.0f },
		{ TEXT("call_08_from_the_south_east"), FVector(-8.0f, 228.0f, 24.0f), FVector(-38.0f, 260.0f, 11.0f), 60.0f },
	};

	return StoryTests::Look(this, TEXT("/Game/Town/Lvl_LastCall"), Views);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookScrappersYardTest, "Ashmarket.Look.ScrappersYard", GutTests::Flags)

bool FTownLookScrappersYardTest::RunTest(const FString& Parameters)
{
	using StoryTests::FView;

	static const FView Views[] =
	{
		{ TEXT("yard_s_01_in_at_the_gate"), FVector(113.6f, 176.2f, 6.95f), FVector(127.0f, 198.0f, 7.0f), 72.0f },
		{ TEXT("yard_s_02_the_weigh_house"), FVector(124.0f, 190.6f, 6.95f), FVector(127.2f, 199.0f, 7.4f), 66.0f },
		{ TEXT("yard_s_03_the_guildmaster"), FVector(124.2f, 194.6f, 6.9f), FVector(125.6f, 197.3f, 6.8f), 60.0f },
		{ TEXT("yard_s_04_the_crew"), FVector(126.4f, 201.0f, 6.95f), FVector(133.4f, 208.0f, 6.6f), 68.0f },
		{ TEXT("yard_s_05_behind_the_tarp"), FVector(134.6f, 206.0f, 6.95f), FVector(136.3f, 210.4f, 6.6f), 70.0f },
		{ TEXT("yard_s_06_plate_bay"), FVector(120.0f, 201.6f, 6.95f), FVector(120.3f, 209.6f, 6.6f), 70.0f },
		{ TEXT("yard_s_07_back_to_the_gate"), FVector(133.0f, 203.0f, 6.95f), FVector(113.0f, 177.0f, 6.6f), 66.0f },
		{ TEXT("yard_s_08_from_above"), FVector(125.0f, 193.0f, 52.0f), FVector(125.1f, 194.0f, 5.0f), 62.0f },
		{ TEXT("yard_s_09_from_the_south_west"), FVector(106.0f, 166.0f, 17.0f), FVector(127.0f, 197.0f, 6.0f), 60.0f },
	};

	return StoryTests::Look(this, TEXT("/Game/Town/Lvl_ScrappersYard"), Views);
}

#endif

/**
 *  Ashmarket.Town.Bump   walking into somebody: the player is stood in front of one of Main Street's Wardens and walks at
 *                        him. He says something, steps aside onto open ground, the Wardens think a point less of the
 *                        player (once, however many times it is done), and he goes back to his mark when the player
 *                        has gone. ("bumping into people should shove them out of the way", 25 November.)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownBumpTest, "Ashmarket.Town.Bump", GutTests::Flags)

bool FTownBumpTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	struct FNotes
	{
		FTownStoryState Story;
		int32 Who = INDEX_NONE;
		int32 Stood = 0;
		FVector2D From = FVector2D::ZeroVector;
		FVector2D Toward = FVector2D::ZeroVector;
		float Walked = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.8f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		const UTownPlaceData* Place = Town->GetPlace();

		Notes->Story = GetRun()->Story;

		// a Warden who can be walked up to: open ground a body's reach from him on some side
		for (int32 i = 0; i < Place->People.Num() && Notes->Who == INDEX_NONE; ++i)
		{
			const FTownPerson& Person = Place->People[i];

			if (Person.Of != FName("wardens") || Person.Bumped.IsEmpty() || !Person.Beat.IsEmpty() || !Town->GetPeople().IsValidIndex(i) || !Town->GetPeople()[i])
			{
				continue;
			}

			const FVector2D At(Person.Place.X, Person.Place.Y);

			for (const FVector2D& Side : { FVector2D(1.0f, 0.0f), FVector2D(-1.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, -1.0f) })
			{
				if (Town->IsOpenGround(At + (Side * 130.0f)) && Town->IsOpenGround(At + (Side * 100.0f)) && Town->IsOpenGround(At - (Side * 90.0f)))
				{
					Notes->Who = i;
					Notes->From = At + (Side * 130.0f);
					Notes->Toward = Side * -1.0f;
					break;
				}
			}
		}

		if (!TestTrue(TEXT("There is a Warden on Main Street who can be walked up to"), Notes->Who != INDEX_NONE))
		{
			return;
		}

		Notes->Stood = Town->GetMark(FName("st.wardens"));

		StandOn(Notes->From, FVector(Notes->Toward.X, Notes->Toward.Y, 0.0f).Rotation().Yaw);
	}, []() { return IsStreetReady(); } });

	// walks at him until he says something, or for three seconds
	Steps.Add({ 0.1f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();

		if (Notes->Who == INDEX_NONE)
		{
			return;
		}

		const FTownPerson& Person = Town->GetPlace()->People[Notes->Who];
		const FTownCall* Bark = Town->GetBark();

		if (TestNotNull(TEXT("Walked into, he says something"), Bark))
		{
			TestTrue(*FString::Printf(TEXT("It is one of his own lines, under his name (%s: %s)"), *Bark->Speaker, *Bark->Words),
				Bark->Speaker == Person.Name && Person.Bumped.ContainsByPredicate([Bark](const FTownBark& One) { return One.Words == Bark->Words; }));
			TestTrue(TEXT("And it is what is shown as called over the place while he says it"), Town->GetCall() == Bark);
		}

		TestEqual(TEXT("The Wardens think a point less of whoever shoves one of theirs"), Town->GetMark(FName("st.wardens")), Notes->Stood - 1);
		TestTrue(*FString::Printf(TEXT("And the player is told so (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(Person.Name));
		TestTrue(TEXT("He steps aside"), Town->IsAside(Notes->Who) && Town->IsPersonLed(Notes->Who));

		GutTests::Screenshot(TEXT("73_walked_into"));
	}, [Notes]()
	{
		ATownGameMode* Town = GetTown();

		if (Notes->Who == INDEX_NONE)
		{
			return true;
		}

		Town->GetWalker()->AddMovementInput(FVector(Notes->Toward.X, Notes->Toward.Y, 0.0f), 1.0f);

		Notes->Walked += Town->GetWorld()->GetDeltaSeconds();

		return Town->GetBark() != nullptr || Notes->Walked >= 3.0f;
	} });

	Steps.Add({ 2.0f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();

		if (Notes->Who == INDEX_NONE)
		{
			return;
		}

		const FTownPerson& Person = Town->GetPlace()->People[Notes->Who];
		const float Off = FVector::Dist2D(Town->GetPeople()[Notes->Who]->GetActorLocation(), Person.Place);

		TestTrue(*FString::Printf(TEXT("He has stepped aside from where he stood (%.0f cm)"), Off), Off > 50.0f && Off < 110.0f);
		TestTrue(*FString::Printf(TEXT("Onto open ground")), Town->IsOpenGround(FVector2D(Town->GetPeople()[Notes->Who]->GetActorLocation())));
		TestTrue(TEXT("And where he is to be spoken to is where he now stands"), FVector::Dist2D(Town->GetPersonStood(Notes->Who), Town->GetPeople()[Notes->Who]->GetActorLocation()) < 1.0f);

		GutTests::Screenshot(TEXT("74_stepped_aside"));

		// walked into again: something said, nothing more lost
		TestTrue(TEXT("He can be walked into again"), Town->Bump(Notes->Who));
		TestEqual(TEXT("And the Wardens do not count it twice on one visit"), Town->GetMark(FName("st.wardens")), Notes->Stood - 1);

		// and the player goes off up the street
		StandOn(Notes->From - (Notes->Toward * 600.0f), 0.0f);
	}, [Notes]() { return Notes->Who == INDEX_NONE || !GetTown()->IsPersonLed(Notes->Who); } });

	Steps.Add({ 0.3f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();

		if (Notes->Who != INDEX_NONE)
		{
			const FTownPerson& Person = Town->GetPlace()->People[Notes->Who];
			const AGutFighter* Body = Town->GetPeople()[Notes->Who];

			TestTrue(*FString::Printf(TEXT("When the player has gone he is back on his mark (%.0f cm off it)"), FVector::Dist2D(Body->GetActorLocation(), Person.Place)),
				FVector::Dist2D(Body->GetActorLocation(), Person.Place) < 8.0f && !Town->IsAside(Notes->Who));
			TestTrue(TEXT("Facing as he did"), FMath::Abs(FRotator::NormalizeAxis(Body->GetActorRotation().Yaw - Person.Facing)) < 3.0f);
		}

		GetRun()->Story = Notes->Story;
	}, [Notes]()
	{
		const ATownGameMode* Town = GetTown();

		// (he goes back when whoever shoved him is well gone, and three seconds after the last of it)
		return Notes->Who == INDEX_NONE || (!Town->IsAside(Notes->Who) && !Town->IsPersonLed(Notes->Who));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Look.Kept   pictures of what is kept and carried: the yard's rack with one of everything on it, and the bag
 *                        at the player's hip from the front, the side and behind. Asserts only that they are there.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLookKeptTest, "Ashmarket.Look.Kept", GutTests::Flags)

bool FTownLookKeptTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	struct FNotes
	{
		TArray<FGutBagItem> Rack;
		TWeakObjectPtr<ACameraActor> Camera;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_Yard"), true))
	{
		AddError(TEXT("Could not open the yard's level"));
		return false;
	}

	const auto LookFrom = [Notes](const FVector& From, const FVector& At, float FieldOfView)
	{
		if (ACameraActor* Seen = Notes->Camera.Get())
		{
			Seen->SetActorLocationAndRotation(From, (At - From).Rotation());
			Seen->GetCameraComponent()->SetFieldOfView(FieldOfView);
			Seen->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 1.0f, [this, Notes, LookFrom]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		UWorld* World = Town->GetWorld();

		Notes->Rack = Run->Rack;
		Run->Rack.Reset();

		for (const TCHAR* Good : { TEXT("bread"), TEXT("tea"), TEXT("splint"), TEXT("DA_Gut_Armor_RubberVest"), TEXT("flask"), TEXT("smokes"), TEXT("scrap") })
		{
			Run->AddToRack(FName(Good), FCString::Strcmp(Good, TEXT("bread")) == 0 ? 2 : 1);
		}

		Town->ShowRackNow();

		TestTrue(*FString::Printf(TEXT("What is kept is on the shelves (%d things)"), Town->GetRackThings().Num()), Town->GetRackThings().Num() >= 5);

		Notes->Camera = World->SpawnActor<ACameraActor>();

		if (APlayerController* Eyes = World->GetFirstPlayerController())
		{
			Eyes->SetViewTarget(Notes->Camera.Get());

			if (AHUD* Words = Eyes->GetHUD())
			{
				Words->bShowHUD = false;
			}
		}

		// the rack, from where somebody stands to use it and a little to one side
		FVector Middle = FVector::ZeroVector;

		for (const AActor* Thing : Town->GetRackThings())
		{
			Middle += Thing->GetActorLocation() / Town->GetRackThings().Num();
		}

		const UTownPlaceData* Place = Town->GetPlace();
		const int32 Door = Place->Doors.IndexOfByPredicate([](const FTownDoor& One) { return One.Deed == ETownDeed::Rack; });
		const FVector Stood = Place->Doors.IsValidIndex(Door) ? Place->Doors[Door].Place : Middle;
		const FVector Out = (FVector(Stood.X, Stood.Y, Middle.Z) - Middle).GetSafeNormal2D();

		// the player out of the way of it
		StandOn(FVector2D(Stood.X, Stood.Y) + (FVector2D(Out.X, Out.Y) * 260.0f) + (FVector2D(-Out.Y, Out.X) * 260.0f), 0.0f);

		LookFrom(Middle + (Out * 190.0f) + (FVector(-Out.Y, Out.X, 0.0f) * 60.0f) + FVector(0.0f, 0.0f, 45.0f), Middle, 60.0f);
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.6f, []() { GutTests::Screenshot(TEXT("kept_01_the_rack")); } });

	const TPair<const TCHAR*, FVector> Sides[] =
	{
		{ TEXT("kept_02_bag_from_the_front"), FVector(210.0f, -60.0f, 10.0f) },
		{ TEXT("kept_03_bag_from_its_side"), FVector(0.0f, -200.0f, 0.0f) },
		{ TEXT("kept_04_bag_from_behind"), FVector(-200.0f, -80.0f, 20.0f) },
		{ TEXT("kept_05_bag_close"), FVector(60.0f, -110.0f, -10.0f) },
	};

	for (const TPair<const TCHAR*, FVector>& Side : Sides)
	{
		Steps.Add({ 0.5f, [this, Side, LookFrom]()
		{
			const AGutFighter* Walker = GetTown()->GetWalker();

			TestTrue(TEXT("The player's bag is on them"), Walker->BagMesh && Walker->BagMesh->IsVisible() && Walker->BagMesh->GetStaticMesh());

			const FVector Hips = Walker->GetActorLocation() + FVector(0.0f, 0.0f, 5.0f);

			LookFrom(Hips + Walker->GetActorTransform().TransformVector(Side.Value), Hips, FCString::Strstr(Side.Key, TEXT("close")) ? 50.0f : 44.0f);
		} });

		Steps.Add({ 0.5f, [Side]() { GutTests::Screenshot(Side.Key); } });
	}

	Steps.Add({ 0.1f, [Notes]()
	{
		GetRun()->Rack = Notes->Rack;
		GetTown()->ShowRackNow();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.Taking  somebody with no tag: one of Main Street's two nameless walkers is walked up to, has something
 *                         to say and can be gone for; the run then knows who is being taken and under which mark. Then
 *                         the three ways it can come out are settled as a place opens: taken (the player's, in tow, an
 *                         owner now), dead, and got away with some of the player's water.
 *  Ashmarket.Gut.Taking   the fight itself, in the dead end: both bare-handed, and over the moment whoever is being
 *                         taken is beaten down to the share of their health they give up at, alive.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownTakingTest, "Ashmarket.Town.Taking", GutTests::Flags)

bool FTownTakingTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	struct FNotes
	{
		FTownStoryState Story;
		FGutStableState Stable;
		FGutTaking Asked;
		int32 Who = INDEX_NONE;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.8f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		Notes->Story = Run->Story;
		Notes->Stable = Run->Stable;

		Run->Stable = FGutStableState();
		Run->Taking = FGutTaking();

		Notes->Who = Place->People.IndexOfByPredicate([Town](const FTownPerson& Person) { return !Person.Taken.IsNone() && Town->GetMark(Person.Taken) == 0; });

		if (!TestTrue(TEXT("Somebody on Main Street has no tag"), Notes->Who != INDEX_NONE && Town->GetPeople().IsValidIndex(Notes->Who) && Town->GetPeople()[Notes->Who]))
		{
			Notes->Who = INDEX_NONE;
			return;
		}

		const FVector At = Town->GetPeople()[Notes->Who]->GetActorLocation();

		StandOn(FVector2D(At.X - 105.0f, At.Y), 0.0f);

		Town->bWaysHeld = true;
		Town->Tick(0.0f);

		TestEqual(TEXT("Whoever it is can be walked up to"), Town->GetPersonInReach(), Notes->Who);

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("They have something to say (%s)"), *Town->GetWords()), Town->IsTalking() && !Town->GetWords().IsEmpty());
		TestTrue(TEXT("And can be gone for, or left"), Town->GetAnswers().Num() == 2 && Town->GetAnswers()[0].Contains(TEXT("Take hold")));
		TestTrue(TEXT("Going for them"), Say(TEXT("Take hold")));

		TestTrue(TEXT("The run knows somebody is being taken"), Run->Taking.bOn && Run->Taking.Outcome == 0);
		TestEqual(TEXT("Under the mark that says they are gone from where they stood"), Run->Taking.Mark, Place->People[Notes->Who].Taken);
		TestTrue(*FString::Printf(TEXT("Who has a name out of the cage's own (%s) and not much in them (%d, %d, %d, %d)"), *Run->Taking.Who.FighterName.ToString(),
			Run->Taking.Who.Strength, Run->Taking.Who.Speed, Run->Taking.Who.Nerve, Run->Taking.Who.Cunning),
			!Run->Taking.Who.FighterName.IsEmpty() && Run->Taking.Who.Strength <= 2 && Run->Taking.Who.Speed <= 2 && Run->Taking.Who.bOwned);
		TestTrue(TEXT("It is fought as a fight of its own: nobody's ladder, bet or yard"), Run->bSandbox && !Run->bStable && !Run->bWindow && !Run->bAtDoor);
		TestFalse(TEXT("Nothing is settled before it is fought"), Run->Stable.bStarted);

		Notes->Asked = Run->Taking;

		// beaten down and taken: settled as the place opens again
		Run->Taking.Outcome = 1;
		Run->bSandbox = false;
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.2f, [this]() { AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true); } });

	Steps.Add({ 0.8f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->Who == INDEX_NONE)
		{
			return;
		}

		TestFalse(TEXT("Settled, nobody is being taken any more"), Run->Taking.bOn);
		TestTrue(TEXT("Taken, they are the player's, who is an owner now"), Run->Stable.bStarted && Run->Stable.Fighters.Num() == 1 && Run->Stable.Fighters[0].bOwned);
		TestTrue(TEXT("And behind the player, not yet brought to the yard"), Run->Stable.Fighters.Num() == 1 && Run->Stable.Fighters[0].bInTow && Town->GetTowCount() == 1);
		TestTrue(TEXT("With their wrists tied"), Town->GetTowBody(0) && Town->GetTowBody(0)->IsBound());
		TestEqual(TEXT("They are gone from where they stood"), Town->GetMark(Notes->Asked.Mark), 1);
		TestTrue(TEXT("Nobody stands there"), !Town->GetPeople().IsValidIndex(Notes->Who) || !Town->GetPeople()[Notes->Who]);
		TestTrue(*FString::Printf(TEXT("The player is told (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("is yours")) && Town->GetNotice().Contains(TEXT("yard")));

		GutTests::Screenshot(TEXT("82_taken_and_in_tow"));

		// and the other two ways: dead, and got away
		Run->Stable = FGutStableState();
		Run->Story = Notes->Story;

		Run->Taking = Notes->Asked;
		Run->Taking.Outcome = 2;
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.2f, [this]() { AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true); } });

	Steps.Add({ 0.6f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->Who != INDEX_NONE)
		{
			TestTrue(TEXT("Dead, nobody is anybody's"), !Run->Stable.bStarted && Run->Stable.Fighters.IsEmpty() && Town->GetTowCount() == 0);
			TestTrue(*FString::Printf(TEXT("And the player is told so (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("dead")));
			TestEqual(TEXT("They are gone from where they stood all the same"), Town->GetMark(Notes->Asked.Mark), 1);
		}

		Run->Story = Notes->Story;
		Run->Stable = Notes->Stable;
		Run->Taking = FGutTaking();
	}, []() { return IsStreetReady(); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutTakingTest, "Ashmarket.Gut.Taking", GutTests::Flags)

bool FGutTakingTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	UGutGameInstance* Run = GetRun();

	if (!Run)
	{
		AddError(TEXT("No run to take anybody in"));
		return false;
	}

	Run->Taking = FGutTaking();
	Run->Taking.bOn = true;
	Run->Taking.Mark = FName("took.test");
	Run->Taking.Who.FighterName = FText::FromString(TEXT("Gristle"));
	Run->Taking.Who.Strength = 1;
	Run->Taking.Who.Speed = 1;
	Run->Taking.Who.bOwned = true;

	Run->bAtDoor = false;
	Run->bSandbox = true;
	Run->bStable = false;
	Run->bWindow = false;

	if (!AutomationOpenMap(TEXT("/Game/Gut/Lvl_Alley"), true))
	{
		AddError(TEXT("Could not open the dead end"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.3f, [this]()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		if (!TestTrue(TEXT("The dead end has the cage's rules and two in it"), Fight.IsValid()))
		{
			return;
		}

		TestTrue(TEXT("It is a taking"), Fight.GameMode->IsTaking() && !Fight.GameMode->IsOnLadder());
		TestTrue(TEXT("Nobody has anything in their hands"), !Fight.Player->GetWeapon() && !Fight.Enemy->GetWeapon());
		TestTrue(*FString::Printf(TEXT("Whoever is being taken has the name they were given (%s)"), *Fight.Enemy->GetDisplayName().ToString()), Fight.Enemy->GetDisplayName().ToString() == TEXT("Gristle"));
		TestEqual(TEXT("Nothing is decided"), Fight.GameMode->GetTakingOutcome(), 0);

		GutTests::Screenshot(TEXT("80_the_dead_end"));

		// beaten down past where they give up, and not dead
		Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth() * (Fight.GameMode->TakenAtHealth - 0.05f));
	}, []()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		return Fight.IsValid() && Fight.GameMode->GetPhase() == EGutFightPhase::Fighting;
	} });

	Steps.Add({ 0.4f, [this]()
	{
		const GutTests::FFight Fight = GutTests::GetFight();
		UGutGameInstance* Run = GetRun();

		if (Fight.IsValid())
		{
			TestEqual(TEXT("Beaten down, they are taken"), Fight.GameMode->GetTakingOutcome(), 1);
			TestTrue(TEXT("Alive"), Fight.Enemy->IsAlive());
			TestTrue(TEXT("And it is over"), Fight.GameMode->GetPhase() == EGutFightPhase::PlayerWon);

			GutTests::Screenshot(TEXT("81_beaten_down"));
		}

		// (nobody is walked back to a town from a test)
		Run->Taking = FGutTaking();
		Run->bSandbox = false;
	}, []()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		return !Fight.IsValid() || Fight.GameMode->GetTakingOutcome() != 0;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.TakingThrough  a taking from one end to the other, with nothing held shut: somebody on Main Street is
 *                                gone for, the dead end opens with the two of them in it, they are beaten down, the town
 *                                opens again where the player stood, and they are behind the player with their wrists
 *                                tied. The parts are each tested in Ashmarket.Town.Taking and Ashmarket.Gut.Taking: this
 *                                is the joins between them.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownTakingThroughTest, "Ashmarket.Town.TakingThrough", GutTests::Flags)

bool FTownTakingThroughTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	struct FNotes
	{
		FTownStoryState Story;
		FGutStableState Stable;
		FName Mark;
		FVector Stood = FVector::ZeroVector;
		bool bAsked = false;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_MainStreet"), true))
	{
		AddError(TEXT("Could not open Main Street's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.8f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		Notes->Story = Run->Story;
		Notes->Stable = Run->Stable;

		Run->Stable = FGutStableState();
		Run->Taking = FGutTaking();

		const int32 Who = Place->People.IndexOfByPredicate([Town](const FTownPerson& Person) { return !Person.Taken.IsNone() && Town->GetMark(Person.Taken) == 0; });

		if (!TestTrue(TEXT("Somebody on Main Street has no tag"), Who != INDEX_NONE && Town->GetPeople().IsValidIndex(Who) && Town->GetPeople()[Who]))
		{
			return;
		}

		Notes->Mark = Place->People[Who].Taken;

		const FVector At = Town->GetPeople()[Who]->GetActorLocation();

		StandOn(FVector2D(At.X - 105.0f, At.Y), 0.0f);

		Town->Tick(0.0f);
		Town->Interact();

		Notes->Stood = Town->GetWalker()->GetActorLocation();
		Notes->bAsked = TestTrue(TEXT("Going for them"), Town->IsTalking() && Say(TEXT("Take hold")));
	}, []() { return IsStreetReady(); } });

	// the dead end, with the two of them in it, at the bell
	Steps.Add({ 0.5f, [this, Notes]()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		if (!Notes->bAsked || !TestTrue(TEXT("The dead end opened with the two of them in it"), Fight.IsValid() && Fight.GameMode->IsTaking()))
		{
			return;
		}

		TestTrue(*FString::Printf(TEXT("It is fought in the dead end (%s)"), *Fight.GameMode->GetWorld()->GetMapName()), Fight.GameMode->GetWorld()->GetMapName().Contains(TEXT("Alley")));
		TestTrue(TEXT("Bare hands on both sides, and no collar on the player"), !Fight.Player->GetWeapon() && !Fight.Enemy->GetWeapon() && !Fight.Player->IsCollared());

		GutTests::Screenshot(TEXT("83_through_the_dead_end"));

		Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth() * (Fight.GameMode->TakenAtHealth - 0.05f));
	}, [Notes]()
	{
		const GutTests::FFight Fight = GutTests::GetFight();

		return !Notes->bAsked || (Fight.IsValid() && Fight.GameMode->GetPhase() == EGutFightPhase::Fighting);
	} });

	// back in the town, by itself, when the last picture of it has had its time
	Steps.Add({ 1.2f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->bAsked && TestNotNull(TEXT("The town opened again"), Town))
		{
			TestTrue(*FString::Printf(TEXT("It is Main Street the player comes back to (%s)"), *Town->GetPlace()->Name), Town->GetPlace()->Name == TEXT("Main Street"));
			TestTrue(*FString::Printf(TEXT("Where they stood (%.0f cm from it)"), FVector::Dist2D(Town->GetWalker()->GetActorLocation(), Notes->Stood)),
				FVector::Dist2D(Town->GetWalker()->GetActorLocation(), Notes->Stood) < 150.0f);
			TestTrue(TEXT("Whoever was taken is the player's and behind them, wrists tied"), Run->Stable.Fighters.Num() == 1 && Run->Stable.Fighters[0].bInTow && Town->GetTowCount() == 1
				&& Town->GetTowBody(0) && Town->GetTowBody(0)->IsBound());
			TestEqual(TEXT("And gone from where they stood"), Town->GetMark(Notes->Mark), 1);
			TestTrue(*FString::Printf(TEXT("The player is told (%s)"), *Town->GetNotice()), Town->GetNotice().Contains(TEXT("is yours")));
			TestFalse(TEXT("Nobody is being taken any more, and the cage is nobody's"), Run->Taking.bOn || Run->bSandbox);

			GutTests::Screenshot(TEXT("84_through_back_in_the_street"));
		}

		Run->Story = Notes->Story;
		Run->Stable = Notes->Stable;
		Run->Taking = FGutTaking();
	}, [Notes]() { return !Notes->bAsked || IsStreetReady(); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.SellAtBlock  somebody the player owns and has behind them is sold at the clerk's table on Block Square:
 *                              the answer is there only while somebody is in tow to sell, the table lists them with what
 *                              the Block gives, and sold they are gone from behind the player and out of the stable.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownSellAtBlockTest, "Ashmarket.Town.SellAtBlock", GutTests::Flags)

bool FTownSellAtBlockTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;
	using namespace StoryTests;

	struct FNotes
	{
		FGutStableState Stable;
		FTownStoryState Story;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// an owner with one taken person behind them, before the square is opened, which stands them there
	if (UGutGameInstance* Run = GetRun())
	{
		Notes->Stable = Run->Stable;
		Notes->Story = Run->Story;

		Run->Stable = FGutStableState();
		Run->Stable.bStarted = true;
		Run->Stable.Water = 5.0f;

		FGutStableFighter Taken;
		Taken.FighterName = FText::FromString(TEXT("Nettle"));
		Taken.Strength = 2;
		Taken.Speed = 1;
		Taken.bOwned = true;
		Taken.bInTow = true;
		Taken.Price = 40.0f;

		Run->Stable.Fighters.Add(Taken);
		Run->Taking = FGutTaking();
	}

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	TArray<FScript::FStep> Steps;

	Steps.Add({ 1.0f, [this]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		TestEqual(TEXT("Somebody is behind the player, and theirs to sell"), Town->GetTowToSell(), 1);

		const int32 Clerk = Find(TEXT("Margin"));

		if (!TestTrue(TEXT("The clerk is at her table"), Clerk != INDEX_NONE))
		{
			return;
		}

		const FTownPerson& Margin = Place->People[Clerk];
		const FVector Before = Margin.Place + (FRotator(0.0f, Margin.Facing, 0.0f).Vector() * 115.0f);

		StandOn(FVector2D(Before.X, Before.Y), Margin.Facing + 180.0f);

		Town->Tick(0.0f);

		TestEqual(TEXT("The clerk is in reach"), Town->GetPersonInReach(), Clerk);

		Town->Interact();

		TestTrue(*FString::Printf(TEXT("With somebody in tow, selling is one of the things to say (%d answers)"), Town->GetAnswers().Num()),
			Town->GetAnswers().ContainsByPredicate([](const FString& Answer) { return Answer.Contains(TEXT("somebody to sell")); }));
		TestTrue(TEXT("Saying it"), Say(TEXT("somebody to sell")));
		TestTrue(TEXT("Opens the table, and not the book"), Town->IsShopping() && Town->IsAtYardPage() && !Town->IsAtBook());

		const TArray<FTownShopLine> Lines = Town->GetShopLines();

		TestTrue(*FString::Printf(TEXT("Whoever is in tow is on it, with what the Block gives (%s, %s)"), Lines.IsEmpty() ? TEXT("") : *Lines[0].Name, Lines.IsEmpty() ? TEXT("") : *Lines[0].PriceText),
			Lines.Num() == 1 && Lines[0].Name.Contains(TEXT("Nettle")) && Lines[0].PriceText == TEXT("+32 L") && Lines[0].bCanBuy);

		GutTests::Screenshot(TEXT("85_somebody_to_sell"));

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("Sold, they are out of the stable"), Run->Stable.Fighters.IsEmpty());
		TestEqual(TEXT("And the water is the player's, the Chainmen's fifth taken"), Run->Stable.Water, 37.0f);
		TestEqual(TEXT("Nobody is behind the player"), Town->GetTowCount(), 0);
		TestTrue(*FString::Printf(TEXT("It is said (%s)"), *Town->GetShopSaid()), Town->GetShopSaid().Contains(TEXT("Nettle")) && Town->GetShopSaid().Contains(TEXT("32 L")));
		TestTrue(TEXT("And there is nobody left to sell"), Town->GetShopLines().IsEmpty());

		Town->HandleBack();
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.4f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();

		const int32 Clerk = Find(TEXT("Margin"));

		Town->Tick(0.0f);

		if (Town->GetPersonInReach() == Clerk)
		{
			Town->Interact();

			TestFalse(TEXT("With nobody in tow, there is nothing to say about selling"),
				Town->GetAnswers().ContainsByPredicate([](const FString& Answer) { return Answer.Contains(TEXT("somebody to sell")); }));

			Town->HandleBack();
		}

		GetRun()->Stable = Notes->Stable;
		GetRun()->Story = Notes->Story;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}
