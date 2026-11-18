// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutBrainController.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"

/**
 *  The pit boss's special cards.
 *
 *  Ashmarket.Gut.Special    one against three: who is on which side, who fights whom, a death that does not end it and the one
 *                           that does, nobody hurt by its own side; two on two: the second on each side, a second's death;
 *                           the player's death ending either at once; and the card on the ladder, with its purse
 */
namespace GutSpecialTests
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
			[Test]() { Test->AddError(TEXT("The cage was not loaded again for the next fight")); return true; },
			30.0f));

		Steps.Reset();
	}

	/** Nobody in the cage thinks for itself: the test moves them */
	void StopEveryBrain(const FFight& Fight)
	{
		TArray<AGutFighter*> Everybody = { Fight.Player, Fight.Enemy };

		for (AGutFighter* Extra : Fight.GameMode->GetExtraFighters())
		{
			Everybody.Add(Extra);
		}

		for (AGutFighter* Fighter : Everybody)
		{
			if (AGutBrainController* Brain = Cast<AGutBrainController>(Fighter->GetController()))
			{
				Brain->SetBrainEnabled(false);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutSpecialTest, "Ashmarket.Gut.Special", GutTests::Flags)

bool FGutSpecialTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutSpecialTests;

	// at the rack of a single fight
	if (!OpenCage(this, true))
	{
		return false;
	}

	TArray<FStep> Steps;

	// --- one against three ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestEqual(TEXT("A fight of two has nobody else in it"), GameMode->GetExtraFighters().Num(), 0);
		TestEqual(TEXT("And no sides"), Fight.Player->GetSide(), static_cast<int32>(INDEX_NONE));

		GameMode->BeginSpecial(EGutSpecialCard::OneAgainstThree);

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestEqual(TEXT("Two more are brought in"), Extras.Num(), 2))
		{
			return;
		}

		TestEqual(TEXT("The player stands alone"), GameMode->CountLiving(0), 1);
		TestEqual(TEXT("Against three"), GameMode->CountLiving(1), 3);
		TestTrue(TEXT("Each of the three has a brain of its own"), Cast<AGutBrainController>(Extras[0]->GetController()) && Cast<AGutBrainController>(Extras[1]->GetController()));
		TestTrue(TEXT("And is after the player"), Extras[0]->GetOpponent() == Fight.Player && Extras[1]->GetOpponent() == Fight.Player && Fight.Enemy->GetOpponent() == Fight.Player);
		TestFalse(TEXT("Nobody swings before the bell"), Extras[0]->IsCombatEnabled());

		GameMode->BeginSpecial(EGutSpecialCard::TwoOnTwo);

		TestEqual(TEXT("A card cannot be made special twice"), Extras.Num(), 2);

		StopEveryBrain(Fight);

		GameMode->SkipRack();
		GameMode->StartFight();

		TestTrue(TEXT("The bell is for all of them"), Extras[0]->IsCombatEnabled() && Extras[1]->IsCombatEnabled());
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("54_one_against_three")); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (Extras.Num() != 2)
		{
			return;
		}

		// two of the three side by side: one swings, and the other is in its arc
		PlaceAt(Extras[0], 300.0f, 300.0f, 0.0f);
		PlaceAt(Extras[1], 390.0f, 300.0f, 180.0f);

		Extras[0]->SetOpponent(Extras[1]);
		Extras[0]->SetStamina(Extras[0]->GetMaxStamina());

		TestTrue(TEXT("One of the three swings with another of them in the way"), Extras[0]->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (Extras.Num() != 2)
		{
			return;
		}

		TestEqual(TEXT("Nobody is hurt by its own side"), Extras[1]->GetHealth(), Extras[1]->GetMaxHealth(), 0.01f);
		TestTrue(TEXT("And it goes back to the one it is here for"), Extras[0]->GetOpponent() == Fight.Player);

		// the one on the card dies first
		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("One death does not end it"), GameMode->GetPhase(), EGutFightPhase::Fighting);
		TestEqual(TEXT("Two are left against the player"), GameMode->CountLiving(1), 2);
		TestTrue(TEXT("The player turns to one who is still alive"), Fight.Player->GetOpponent() && Fight.Player->GetOpponent()->IsAlive() && Fight.Player->GetOpponent()->GetSide() == 1);

		Extras[0]->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("Nor does the second"), GameMode->GetPhase(), EGutFightPhase::Fighting);
		TestTrue(TEXT("The player turns to the last"), Fight.Player->GetOpponent() == Extras[1]);

		Extras[1]->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The third does"), GameMode->GetPhase(), EGutFightPhase::PlayerWon);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- two on two ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		GameMode->EnterSandbox();
		GameMode->BeginSpecial(EGutSpecialCard::TwoOnTwo);

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestEqual(TEXT("A second for each side"), Extras.Num(), 2))
		{
			return;
		}

		TestTrue(TEXT("Two on two"), GameMode->CountLiving(0) == 2 && GameMode->CountLiving(1) == 2);

		AGutFighter* Theirs = Extras[0]->GetSide() == 1 ? Extras[0].Get() : Extras[1].Get();
		AGutFighter* Ours = Extras[0]->GetSide() == 0 ? Extras[0].Get() : Extras[1].Get();

		TestTrue(TEXT("The player's second is after one of the other two"), Ours->GetOpponent() && Ours->GetOpponent()->GetSide() == 1);
		TestTrue(TEXT("And theirs after one of these two"), Theirs->GetOpponent() && Theirs->GetOpponent()->GetSide() == 0);

		StopEveryBrain(Fight);

		GameMode->SkipRack();
		GameMode->StartFight();
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("55_two_on_two")); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (Extras.Num() != 2)
		{
			return;
		}

		AGutFighter* Theirs = Extras[0]->GetSide() == 1 ? Extras[0].Get() : Extras[1].Get();
		AGutFighter* Ours = Extras[0]->GetSide() == 0 ? Extras[0].Get() : Extras[1].Get();

		Ours->Kill(Fight.Enemy, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The player's second dying does not end it"), GameMode->GetPhase(), EGutFightPhase::Fighting);
		TestTrue(TEXT("Both of the others are now after the player"), Fight.Enemy->GetOpponent() == Fight.Player && Theirs->GetOpponent() == Fight.Player);

		Fight.Player->Kill(Theirs, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The player dying ends it at once, whoever is left"), GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestFalse(TEXT("And the fighting stops"), Theirs->IsCombatEnabled());

		// --- on the ladder: to Dog rank's second fight by the short way ---

		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		GameInstance->bLadderStarted = true;
		GameInstance->Ladder = FGutLadderState();
		GameInstance->Ladder.Rank = 1;
		GameInstance->Ladder.Rung = 1;
		GameInstance->Ladder.Water = 50.0f;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;
		GameInstance->bSandbox = false;
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	TSharedRef<float> Purse = MakeShared<float>(0.0f);

	Steps.Add({ 0.3f, [this, Purse](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		if (!TestTrue(TEXT("On the ladder, at Dog rank's second fight"), GameMode->IsOnLadder() && GameMode->GetLadderState().Rank == 1 && GameMode->GetLadderState().Rung == 1))
		{
			return;
		}

		TestEqual(TEXT("The pit boss has a card of two on two"), GameMode->GetSpecialOffer(), EGutSpecialCard::TwoOnTwo);
		TestTrue(TEXT("It is a line on the card"), GameMode->GetCardItems().Contains(FGutCardItem{ EGutCardItemType::Special, 0 }));
		TestFalse(TEXT("It is not taken unless it is taken"), GameMode->IsSpecialAccepted());

		GameMode->SetSpecialAccepted(true);

		TestTrue(TEXT("It is taken"), GameMode->IsSpecialAccepted());

		*Purse = GameMode->GetRung()->Purse;
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight) { Screenshot(TEXT("56_card_special")); } });

	Steps.Add({ 0.3f, [this, Purse](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		if (!GameMode->IsOnLadder())
		{
			return;
		}

		GameMode->ConfirmCard();

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestEqual(TEXT("The seconds are brought in when the card is left"), Extras.Num(), 2))
		{
			return;
		}

		TestFalse(TEXT("They have names"), Extras[0]->GetDisplayName().IsEmpty() || Extras[1]->GetDisplayName().IsEmpty());

		StopEveryBrain(Fight);

		GameMode->ConfirmChoice();
		GameMode->ConfirmBet();
		GameMode->StartFight();

		AGutFighter* Theirs = Extras[0]->GetSide() == 1 ? Extras[0].Get() : Extras[1].Get();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestFalse(TEXT("The fight is not settled while its second stands"), GameMode->GetSettlement().bSettled);

		Theirs->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestTrue(TEXT("It is settled as a win when both are down"), GameMode->GetSettlement().bSettled && GameMode->GetSettlement().bPlayerWon);
		TestEqual(TEXT("At the special card's purse"), GameMode->GetSettlement().Purse, FMath::RoundToFloat(*Purse * GameMode->GetSpecialPurseScale(EGutSpecialCard::TwoOnTwo)), 0.01f);
		TestEqual(TEXT("The ladder moves on"), GameMode->GetLadderState().Rung, 2);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		TestEqual(TEXT("The third fight is against the dogs, if the pit boss has any"), GameMode->GetSpecialOffer(), GameMode->GetDog() ? EGutSpecialCard::Animal : EGutSpecialCard::None);
		TestEqual(TEXT("And nobody extra in the cage"), GameMode->GetExtraFighters().Num(), 0);

		GameInstance->Ladder.Rung = 3;

		TestEqual(TEXT("The fourth is one against three"), GameMode->GetSpecialOffer(), EGutSpecialCard::OneAgainstThree);

		GameInstance->Ladder.Rank = 0;

		TestEqual(TEXT("Meat is not offered any"), GameMode->GetSpecialOffer(), EGutSpecialCard::None);

		// back to single fights with nothing behind them, so that whatever test comes next finds the cage as it expects it
		GameInstance->Ladder = FGutLadderState();
		GameInstance->bLadderStarted = false;

		GameMode->EnterSandbox();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
