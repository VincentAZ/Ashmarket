// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/PlayerController.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "GutWeaponData.h"

/**
 *  The Gut entered as a gambler.
 *
 *  Ashmarket.Gut.Window    a night at the betting window: the card for a fight between two of the cage's own, a bet that wins,
 *                          a fix that gets by, a fix that is noticed and the fight in the cage that follows, and the night's reckoning
 *  Ashmarket.Gut.WindowKeys the same card and window worked by the keys themselves, as somebody playing would: the watcher drives
 *                          nobody, so its keys reach the game by a different road from a fighter's
 */
namespace GutWindowTests
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	/** Runs the steps given so far, then waits for the cage to be loaded afresh */
	void RunThenWaitForNextFight(FAutomationTestBase* Test, TArray<FStep>& Steps)
	{
		TSharedRef<TWeakObjectPtr<AGutGameMode>> OldGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

		if (Steps.Num() > 0)
		{
			TFunction<void(const FFight&)> Last = MoveTemp(Steps.Last().Action);

			Steps.Last().Action = [OldGameMode, Last](const FFight& Fight)
			{
				*OldGameMode = Fight.GameMode;

				Last(Fight);
			};
		}

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(Test, MoveTemp(Steps)));

		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[OldGameMode]() { const FFight Fight = GetFight(); return OldGameMode->IsExplicitlyNull() ? false : (Fight.IsValid() && Fight.GameMode != OldGameMode->Get()); },
			[Test]() { Test->AddError(TEXT("The cage was not loaded again for the next fight on the bill")); return true; },
			30.0f));

		Steps.Reset();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutWindowTest, "Ashmarket.Gut.Window", GutTests::Flags)

bool FGutWindowTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutWindowTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	struct FNotes
	{
		float Water = 0.0f;
		float ReturnPerLitre = 0.0f;
		FString Loser;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// --- somebody comes to the cage to bet ---

	Steps.Add({ 0.3f, [](const FFight& Fight) { Fight.GameMode->EnterWindow(11); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		TestTrue(TEXT("The cage is at the window"), GameMode->IsAtWindow());
		TestFalse(TEXT("And not on the ladder"), GameMode->IsOnLadder());
		TestEqual(TEXT("A night opens on the card of its first fight"), GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("It is the first night"), State.Night, 1);
		TestEqual(TEXT("And the first fight"), State.Fight, 0);
		TestTrue(TEXT("The gambler has water"), State.Water > 0.0f);

		const FGutLadderRung* First = GameMode->GetWindowRung(0);
		const FGutLadderRung* Second = GameMode->GetWindowRung(1);

		if (!TestTrue(TEXT("Two fighters are on the card"), First && Second && First != Second))
		{
			return;
		}

		TestEqual(TEXT("The fighter in the first corner is the one on the card"), Fight.Player->GetDisplayName().ToString(), First->OpponentName.ToString());
		TestEqual(TEXT("And the one in the second"), Fight.Enemy->GetDisplayName().ToString(), Second->OpponentName.ToString());
		TestTrue(TEXT("Each holds what its rung says"), Fight.Player->GetWeapon() == First->Weapon && Fight.Enemy->GetWeapon() == Second->Weapon);
		TestTrue(TEXT("Both are fought by the cage's own brain"), Cast<AGutOpponentController>(Fight.Player->GetController()) && Cast<AGutOpponentController>(Fight.Enemy->GetController()));

		const APlayerController* Viewer = GameMode->GetWorld()->GetFirstPlayerController();

		TestTrue(TEXT("The player drives neither of them"), Viewer && Viewer->GetPawn() == nullptr);

		const float Chance = GameMode->GetWindowBookChance();

		TestTrue(TEXT("The book calls no fight a certainty"), Chance > 0.1f && Chance < 0.9f);

		// the card's lines: the way on, and three ways of fixing it for each corner
		TestEqual(TEXT("Seven lines on the card"), GameMode->GetCardItems().Num(), 7);

		// each name is written on the side of the screen its fighter stands on, as whoever is watching sees them
		if (TestTrue(TEXT("The gambler has a view"), Viewer && Viewer->PlayerCameraManager))
		{
			const FVector ScreenRight = FRotationMatrix(Viewer->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
			const float Across = FVector::DotProduct(Fight.Player->GetActorLocation() - Fight.Enemy->GetActorLocation(), ScreenRight);

			TestTrue(TEXT("The two are seen side by side"), FMath::Abs(Across) > 100.0f);
			TestEqual(TEXT("The first corner's name is on the side it stands on"), GameMode->IsFirstCornerOnRight(), Across > 0.0f);
		}

		Screenshot(TEXT("40_window_card"));
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		Notes->Water = State.Water;

		// no fix this time: straight to the window
		GameMode->ConfirmCard();

		TestEqual(TEXT("The ladder's way off the card does nothing here"), GameMode->GetPhase(), EGutFightPhase::Card);

		for (int32 i = 0; i < 8; ++i)
		{
			GameMode->MoveCardSelection(-1);
		}

		GameMode->ActivateCardSelection();

		TestEqual(TEXT("The first line leads to the betting"), GameMode->GetPhase(), EGutFightPhase::Betting);

		// the odds shorten as the water goes on
		const float Step = GameMode->GetLadder()->BetStep;

		TestTrue(TEXT("A bigger stake brings back less for each litre"), GameMode->GetReturnPerLitre(EGutBetSide::Player, Step * 4.0f) < GameMode->GetReturnPerLitre(EGutBetSide::Player, Step));
		TestTrue(TEXT("The window takes no more than the gambler holds"), GameMode->GetMaxStake() <= State.Water);

		// how much is the gambler's to say, to the litre, before it is on anybody and after
		TestEqual(TEXT("The window takes all the gambler holds, to the litre"), GameMode->GetMaxStake(), FMath::Min(GameMode->GetLadder()->MaxBet, FMath::FloorToFloat(State.Water)));
		TestEqual(TEXT("The stake begins at the usual bet"), GameMode->GetBetWanted(), Step);

		GameMode->HandleMenuInput(EGutMenuInput::Up);
		GameMode->HandleMenuInput(EGutMenuInput::Up);

		TestEqual(TEXT("Up twice is two litres more, with nobody chosen yet"), GameMode->GetBetWanted(), Step + 2.0f);
		TestEqual(TEXT("And nothing is on anybody"), GameMode->GetBetSide(), EGutBetSide::None);

		// right is the one standing on the right, left the one on the left, and no bet is between them
		const EGutBetSide OnRight = GameMode->IsFirstCornerOnRight() ? EGutBetSide::Player : EGutBetSide::Opponent;
		const EGutBetSide OnLeft = GameMode->IsFirstCornerOnRight() ? EGutBetSide::Opponent : EGutBetSide::Player;

		GameMode->HandleMenuInput(EGutMenuInput::Right);
		TestEqual(TEXT("Right bets on the one on the right"), GameMode->GetBetSide(), OnRight);
		TestEqual(TEXT("With the stake that was set"), GameMode->GetBetStake(), Step + 2.0f);

		GameMode->HandleMenuInput(EGutMenuInput::Down);
		TestEqual(TEXT("Down is a litre less"), GameMode->GetBetStake(), Step + 1.0f);

		GameMode->HandleMenuInput(EGutMenuInput::Left);
		TestEqual(TEXT("Left from there is no bet"), GameMode->GetBetSide(), EGutBetSide::None);
		TestEqual(TEXT("And the stake is kept for whoever is chosen next"), GameMode->GetBetWanted(), Step + 1.0f);

		GameMode->HandleMenuInput(EGutMenuInput::Left);
		TestEqual(TEXT("And left again is the one on the left"), GameMode->GetBetSide(), OnLeft);
		TestEqual(TEXT("With that stake"), GameMode->GetBetStake(), Step + 1.0f);

		for (int32 i = 0; i < 200; ++i)
		{
			GameMode->HandleMenuInput(EGutMenuInput::Up);
		}

		TestEqual(TEXT("Up stops at everything the window will take"), GameMode->GetBetStake(), GameMode->GetMaxStake());

		for (int32 i = 0; i < 200; ++i)
		{
			GameMode->HandleMenuInput(EGutMenuInput::Down);
		}

		TestEqual(TEXT("And down at a litre"), GameMode->GetBetStake(), 1.0f);

		GameMode->SetBet(EGutBetSide::Player, Step * 2.0f);

		Notes->ReturnPerLitre = GameMode->GetReturnPerLitre(EGutBetSide::Player, Step * 2.0f);

		Screenshot(TEXT("41_window_betting"));
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();
		const float Step = GameMode->GetLadder()->BetStep;

		GameMode->ConfirmBet();

		TestEqual(TEXT("The stake is out of the gambler's hand"), State.Water, Notes->Water - (Step * 2.0f), 0.01f);

		GameMode->StartFight();

		TestEqual(TEXT("They fight"), GameMode->GetPhase(), EGutFightPhase::Fighting);

		// the two of them along the top, and neither of them is "you"
		Fight.Player->SetCombatEnabled(false);
		Fight.Enemy->SetCombatEnabled(false);
	} });

	// a picture is of the frame after it is asked for, so each is the last thing its step does
	Steps.Add({ 1.3f, [this](const FFight& Fight) { Screenshot(TEXT("41_window_watching")); } });

	Steps.Add({ 0.2f, [](const FFight& Fight)
	{
		// the second corner is beaten to its knees
		Fight.Enemy->SetCombatEnabled(true);
		Fight.Enemy->SetHealth(1.0f);
		Fight.Enemy->ReceiveAttack(Fight.Player, Fight.Player->GetHeavyAttack());
	} });

	Steps.Add({ 0.6f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The second corner is staggered"), Fight.Enemy->IsStaggered());

		Screenshot(TEXT("41_window_staggered"));
	} });

	Steps.Add({ 0.2f, [Notes](const FFight& Fight)
	{
		// the one that was backed wins
		Notes->Loser = Fight.Enemy->GetDisplayName().ToString();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 0.5f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();
		const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();
		const float Step = GameMode->GetLadder()->BetStep;

		TestTrue(TEXT("The fight is settled"), Result.bSettled && !Result.bOwnFight);
		TestEqual(TEXT("The first corner is still standing"), Result.Winner, 0);
		TestEqual(TEXT("The window pays at the odds that were posted"), Result.Return, Step * 2.0f * Notes->ReturnPerLitre, 0.01f);
		TestEqual(TEXT("The water is in the gambler's hand"), State.Water, Notes->Water - (Step * 2.0f) + Result.Return, 0.01f);
		TestEqual(TEXT("The bill moves on"), State.Fight, 1);
		TestEqual(TEXT("And the one on the floor is off the roster"), State.Dead.Num(), 1);
		TestEqual(TEXT("The card still names the fight that was fought"), GameMode->GetWindowRung(1)->OpponentName.ToString(), Notes->Loser);

		Screenshot(TEXT("42_window_paid"));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the second fight: something in a water bucket, and nobody notices ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		TestEqual(TEXT("The second fight is on the card"), State.Fight, 1);
		TestTrue(TEXT("Neither of them is the one who died"), GameMode->GetWindowRung(0)->OpponentName.ToString() != Notes->Loser && GameMode->GetWindowRung(1)->OpponentName.ToString() != Notes->Loser);

		Notes->Water = State.Water;

		const float Cost = GameMode->GetFixCost(EGutFix::DrugBucket, 1);

		GameMode->SetWindowFix(EGutFix::DrugBucket, 1);

		TestEqual(TEXT("The bucket is seen to"), GameMode->GetWindowFix(), EGutFix::DrugBucket);

		const AGutOpponentController* Brain = Cast<AGutOpponentController>(Fight.Enemy->GetController());
		const float ReactionBefore = Brain ? Brain->GetTuning().MaxReactionTime : 0.0f;

		// to the window
		for (int32 i = 0; i < 8; ++i)
		{
			GameMode->MoveCardSelection(-1);
		}

		GameMode->ActivateCardSelection();

		TestEqual(TEXT("It is paid for at once"), State.Water, Notes->Water - Cost, 0.01f);
		TestTrue(TEXT("The drugged fighter is slower to react"), Brain && Brain->GetTuning().MaxReactionTime > ReactionBefore);

		GameMode->SetBet(EGutBetSide::Player, GameMode->GetLadder()->BetStep);
		GameMode->ConfirmBet();
		GameMode->StartFight();

		// the roll comes up high: nobody notices
		GameMode->SetNextWindowRoll(0.99f);

		// and the gambler owes the lender a little, to see who is standing at the window when it pays
		GameMode->GetGameInstance<UGutGameInstance>()->Window.Debt = 2.0f;

		const float Held = State.Water;

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();

		TestTrue(TEXT("Every debt gets paid: the lender takes what is owed out of the window's payment"), Result.DebtPaid == 2.0f && State.Debt == 0.0f);
		TestEqual(TEXT("And the gambler gets the rest"), State.Water, Held + Result.Return - Result.DebtPaid, 0.01f);
		TestTrue(TEXT("A fix that gets by is paid like any other bet"), Result.bSettled && !Result.bCaught && Result.Return > 0.0f);
		TestTrue(TEXT("But the gambler is looked at a little harder"), State.Heat > 0.0f);
		TestFalse(TEXT("And is not for the cage"), State.bCaught);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the third: a weapon is seen to, it comes apart, and this time it is noticed ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		// whichever of them is armed
		const int32 Corner = Fight.Enemy->GetWeapon() ? 1 : 0;
		AGutFighter* Fixed = Corner == 1 ? Fight.Enemy : Fight.Player;
		AGutFighter* Other = Corner == 1 ? Fight.Player : Fight.Enemy;

		if (!TestNotNull(TEXT("One of them holds a weapon"), Fixed->GetWeapon()))
		{
			return;
		}

		GameMode->SetWindowFix(EGutFix::TamperRack, Corner);

		for (int32 i = 0; i < 8; ++i)
		{
			GameMode->MoveCardSelection(-1);
		}

		GameMode->ActivateCardSelection();

		TestTrue(TEXT("The weapon has been seen to"), Fixed->IsWeaponSabotaged());

		GameMode->SetBet(Corner == 1 ? EGutBetSide::Player : EGutBetSide::Opponent, GameMode->GetLadder()->BetStep);
		GameMode->ConfirmBet();

		Notes->Water = State.Water;

		GameMode->StartFight();

		// the two brains are switched off so that the one blow below is the only one
		for (AGutFighter* Fighter : { Fixed, Other })
		{
			if (AController* Brain = Fighter->GetController())
			{
				Brain->SetActorTickEnabled(false);
			}
		}

		Fixed->SetActorLocationAndRotation(FVector(-70.0f, 0.0f, Fixed->GetActorLocation().Z), FRotator(0.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Other->SetActorLocationAndRotation(FVector(70.0f, 0.0f, Other->GetActorLocation().Z), FRotator(0.0f, 180.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

		TestTrue(TEXT("The fighter with the seen-to weapon swings"), Fixed->TryLightAttack());
	} });

	Steps.Add({ 0.8f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		const int32 Corner = GameMode->GetWindowFixCorner();
		AGutFighter* Fixed = Corner == 1 ? Fight.Enemy : Fight.Player;
		AGutFighter* Other = Corner == 1 ? Fight.Player : Fight.Enemy;

		TestNull(TEXT("The weapon came apart at its first blow"), Fixed->GetWeapon());

		// the roll comes up low: it is noticed
		GameMode->SetNextWindowRoll(0.0f);

		Fixed->Kill(Other, FVector(1.0f, 0.0f, 0.0f));

		const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();

		TestTrue(TEXT("The fix was noticed"), Result.bSettled && Result.bCaught);
		TestEqual(TEXT("A noticed fix pays nothing, though the bet won"), Result.Return, 0.0f);
		TestTrue(TEXT("There is a fine"), Result.Fine > 0.0f && FMath::IsNearlyEqual(State.Water, Notes->Water - Result.Fine, 0.01f));
		TestTrue(TEXT("And the gambler is for the cage"), State.bCaught && GameMode->IsGamblersOwnFight());

		Screenshot(TEXT("43_window_caught"));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the fixer's own fight ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();
		const APlayerController* Viewer = GameMode->GetWorld()->GetFirstPlayerController();

		TestTrue(TEXT("This fight is the gambler's own"), GameMode->IsGamblersOwnFight() && !GameMode->IsAtWindow());
		TestTrue(TEXT("The player drives its own fighter again"), Viewer && Viewer->GetPawn() == Fight.Player);
		TestNull(TEXT("With bare hands"), Fight.Player->GetWeapon());
		TestTrue(TEXT("There is no card and no rack: straight to the bell"), GameMode->GetPhase() == EGutFightPhase::Intro || GameMode->GetPhase() == EGutFightPhase::Fighting);

		Notes->Water = State.Water;

		// whatever it owes the lender when it goes in
		GameMode->GetGameInstance<UGutGameInstance>()->Window.Debt = 30.0f;

		GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();

		TestTrue(TEXT("The cage's verdict is final: walking out alive clears what was owed"), Result.DebtCleared == 30.0f && State.Debt == 0.0f);
		TestTrue(TEXT("A fixer that walks out has paid"), Result.bSettled && Result.bOwnFight && Result.bSurvived);
		TestFalse(TEXT("The window is open to it again"), State.bCaught);
		TestEqual(TEXT("With what it held"), State.Water, Notes->Water, 0.01f);
		TestEqual(TEXT("And tonight's bill where it was left"), State.Fight, 3);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the last fight of the night, with nothing on it, and the reckoning ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("Back at the window"), GameMode->IsAtWindow() && GameMode->GetPhase() == EGutFightPhase::Card);

		for (int32 i = 0; i < 8; ++i)
		{
			GameMode->MoveCardSelection(-1);
		}

		GameMode->ActivateCardSelection();
		GameMode->ConfirmBet();
		GameMode->StartFight();

		Fight.Player->Kill(Fight.Enemy, FVector(1.0f, 0.0f, 0.0f));

		const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();

		TestTrue(TEXT("With nothing on it, nothing changes hands"), Result.bSettled && Result.Backed < 0 && Result.Return == 0.0f);
		TestEqual(TEXT("The second corner is still standing"), Result.Winner, 1);
		TestTrue(TEXT("The night's bill is fought out"), GameMode->IsWindowNightOver());
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		TestEqual(TEXT("After the last fight comes the reckoning"), GameMode->GetPhase(), EGutFightPhase::Gate);
		TestTrue(TEXT("It knows what was staked and what came back"), State.Staked > 0.0f && State.Returned > 0.0f && State.FixSpend > 0.0f && State.Fines > 0.0f);

		Screenshot(TEXT("44_window_night"));

		Notes->Water = State.Water;
	} });

	// where there is a street to go back down to, the reckoning offers it beside tomorrow night's bill
	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		UGutGameInstance* Run = Cast<UGutGameInstance>(Fight.GameMode->GetGameInstance());

		TestFalse(TEXT("With no street there is only tomorrow night"), Fight.GameMode->CanLeaveForStreet());

		Run->bStreetForced = true;

		TestTrue(TEXT("With one, it is offered"), Fight.GameMode->CanLeaveForStreet());

		Fight.GameMode->HandleMenuInput(EGutMenuInput::Down);

		TestTrue(TEXT("Down moves the arrow to the street"), Fight.GameMode->IsLeaveChosen());

		Screenshot(TEXT("44_window_night_or_the_street"));
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		UGutGameInstance* Run = Cast<UGutGameInstance>(Fight.GameMode->GetGameInstance());

		Fight.GameMode->HandleMenuInput(EGutMenuInput::Up);

		TestFalse(TEXT("And up moves it back"), Fight.GameMode->IsLeaveChosen());

		Run->bStreetForced.Reset();
	} });

	Steps.Add({ 0.3f, [](const FFight& Fight)
	{
		Fight.GameMode->HandleMenuInput(EGutMenuInput::Confirm);
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutWindowState& State = GameMode->GetWindowState();

		TestEqual(TEXT("A new night"), State.Night, 2);
		TestEqual(TEXT("With a new bill"), State.Fight, 0);
		TestEqual(TEXT("The water comes along"), State.Water, Notes->Water, 0.01f);
		TestEqual(TEXT("Last night's sums do not"), State.Staked, 0.0f);
		TestEqual(TEXT("The dead stay dead"), State.Dead.Num(), 4);
		TestEqual(TEXT("And the card for its first fight is up"), GameMode->GetPhase(), EGutFightPhase::Card);

		// back to single fights, so that whatever test comes next finds the cage as it expects it
		GameMode->GetGameInstance<UGutGameInstance>()->bWindow = false;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutWindowKeysTest, "Ashmarket.Gut.WindowKeys", GutTests::Flags)

bool FGutWindowKeysTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutWindowTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [](const FFight& Fight) { Fight.GameMode->EnterWindow(11); } });

	RunThenWaitForNextFight(this, Steps);

	auto Until = [this](TFunction<bool(const FFight&)> Reached, const TCHAR* Failure)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[Reached]() { const FFight Fight = GetFight(); return Fight.IsValid() && Reached(Fight); },
			[this, Failure]() { AddError(Failure); return true; },
			6.0f));
	};

	auto Press = [this](const FKey& Key)
	{
		TArray<FStep> One;

		One.Add({ 0.3f, [Key](const FFight& Fight) { SendKey(Fight, Key, true); } });
		One.Add({ 0.1f, [Key](const FFight& Fight) { SendKey(Fight, Key, false); } });

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(One)));
	};

	Until([](const FFight& Fight) { return Fight.GameMode->GetPhase() == EGutFightPhase::Card && !Fight.GameMode->IsTellingStory(); }, TEXT("The window's card did not come up"));

	// the choice rests on the way on, which is the first line. Down is a way of helping the fight, and up is back.
	Press(EKeys::S);

	Until([](const FFight& Fight) { return Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Fix; }, TEXT("The S key did not move down the window's card"));

	// taking that line arranges it, and puts the choice back on the way on: the next press is the bet
	Press(EKeys::SpaceBar);

	Until([](const FFight& Fight) { return Fight.GameMode->GetWindowFix() != EGutFix::None && Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Rack; },
		TEXT("The space bar did not arrange the line it was on and put the choice back on the way to the betting"));

	// each of the keys the card names goes on from it, and each of them is tried from a card of its own
	Press(EKeys::Enter);

	Until([](const FFight& Fight) { return Fight.GameMode->GetPhase() == EGutFightPhase::Betting; }, TEXT("The Enter key did not go on from the window's card to the betting"));

	Press(EKeys::D);

	Until([](const FFight& Fight) { return Fight.GameMode->GetBetSide() != EGutBetSide::None; }, TEXT("The D key did not choose a side to bet on"));

	Press(EKeys::W);

	Until([](const FFight& Fight) { return Fight.GameMode->GetBetStake() > Fight.GameMode->GetLadder()->BetStep; }, TEXT("The W key did not raise the stake"));

	Press(EKeys::LeftMouseButton);

	Until([](const FFight& Fight) { return Fight.GameMode->GetPhase() == EGutFightPhase::Intro || Fight.GameMode->GetPhase() == EGutFightPhase::Fighting; }, TEXT("The left mouse button did not place the bet and let the fight begin"));

	// back to single fights, so that whatever test comes next finds the cage as it expects it
	TArray<FStep> Last;

	Last.Add({ 0.0f, [](const FFight& Fight) { Fight.GameMode->GetGameInstance<UGutGameInstance>()->bWindow = false; } });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Last)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
