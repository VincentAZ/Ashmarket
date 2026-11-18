// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutGameInstance.h"
#include "GutLadderData.h"

/**
 *  Dust and Hush, which are legal in the ring.
 *
 *  Ashmarket.Gut.Drugs    not sold to Meat; sold at the card from Dog rank; what a dose of each does in the fight; the habit a dose
 *                         leaves and how a fight without one eases it; going without once the habit has hold; and Dust on a full habit
 */
namespace GutDrugTests
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

	/** Leaves the card, the rack and the window behind, rings the bell and has the player win at once */
	void WinTheFight(const FFight& Fight)
	{
		Fight.GameMode->ConfirmCard();
		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutDrugsTest, "Ashmarket.Gut.Drugs", GutTests::Flags)

bool FGutDrugsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutDrugTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	TSharedRef<float> WholeHealth = MakeShared<float>(0.0f);

	Steps.Add({ 0.3f, [](const FFight& Fight) { Fight.GameMode->StartNewLadder(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, WholeHealth](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		TestFalse(TEXT("Meat is sold nothing"), GameMode->AreDrugsSold());
		TestFalse(TEXT("And cannot take anything"), GameMode->TakeDrug(EGutDrug::Dust));

		*WholeHealth = Fight.Player->GetMaxHealth();

		// to Dog rank by the short way, with water to spend
		GameInstance->Ladder.Rank = 1;
		GameInstance->Ladder.Water = 100.0f;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;

		GameMode->EnterSandbox();
		GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		GameInstance->bSandbox = false;
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- a dose of each ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutLadderState& State = GameMode->GetLadderState();

		TestTrue(TEXT("From Dog rank both are sold at the card"), GameMode->AreDrugsSold());

		int32 Lines = 0;

		for (const FGutCardItem& Item : GameMode->GetCardItems())
		{
			Lines += Item.Type == EGutCardItemType::Drug ? 1 : 0;
		}

		TestEqual(TEXT("A line for each"), Lines, 2);

		const float Water = State.Water;

		TestTrue(TEXT("Dust is taken"), GameMode->TakeDrug(EGutDrug::Dust));
		TestTrue(TEXT("And Hush"), GameMode->TakeDrug(EGutDrug::Hush));
		TestFalse(TEXT("One dose of each to a fight"), GameMode->TakeDrug(EGutDrug::Dust));
		TestEqual(TEXT("Both are paid for"), State.Water, Water - GameMode->GetDrugPrice(EGutDrug::Dust) - GameMode->GetDrugPrice(EGutDrug::Hush), 0.01f);
		TestTrue(TEXT("Each leaves the start of a habit, Dust the bigger"), GameMode->GetDrugHabit(EGutDrug::Dust) > GameMode->GetDrugHabit(EGutDrug::Hush) && GameMode->GetDrugHabit(EGutDrug::Hush) > 0.0f);

		Screenshot(TEXT("50_card_dust_and_hush"));
	} });

	Steps.Add({ 0.3f, [this, WholeHealth](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		GameMode->ConfirmCard();

		TestTrue(TEXT("On Dust a blow lands harder than the attack says"), Fight.Player->GetDamageScale() > 1.0f);
		TestEqual(TEXT("And the fighter is whole"), Fight.Player->GetMaxHealth(), *WholeHealth);

		GameMode->ConfirmChoice();
		GameMode->ConfirmBet();
		GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestFalse(TEXT("A dose is for one fight"), GameMode->IsDrugTaken(EGutDrug::Dust) || GameMode->IsDrugTaken(EGutDrug::Hush));
		TestTrue(TEXT("The habit outlasts it"), GameMode->GetDrugHabit(EGutDrug::Dust) > 0.0f);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- going without, once the habit has hold ---

	Steps.Add({ 0.3f, [this, WholeHealth](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		// as if it had been taking both for a while
		GameInstance->Ladder.Habit[static_cast<int32>(EGutDrug::Dust)] = 0.7f;
		GameInstance->Ladder.Habit[static_cast<int32>(EGutDrug::Hush)] = 0.7f;

		TestTrue(TEXT("Without a dose the habit is felt"), GameMode->IsInWithdrawal(EGutDrug::Dust) && GameMode->IsInWithdrawal(EGutDrug::Hush));

		GameMode->ConfirmCard();

		TestTrue(TEXT("Without Dust a blow lands lighter"), Fight.Player->GetDamageScale() < 1.0f);
		TestEqual(TEXT("Without Hush the fighter is half itself"), Fight.Player->GetMaxHealth(), *WholeHealth * 0.5f, 0.5f);

		GameMode->ConfirmChoice();
		GameMode->ConfirmBet();
		GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestTrue(TEXT("A fight without a dose takes a little of the habit off"), GameMode->GetDrugHabit(EGutDrug::Dust) < 0.7f && GameMode->GetDrugHabit(EGutDrug::Hush) < 0.7f);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- Dust on a full habit ---

	Steps.Add({ 0.3f, [this, WholeHealth](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		GameInstance->Ladder.Habit[static_cast<int32>(EGutDrug::Dust)] = 1.0f;
		GameInstance->Ladder.Habit[static_cast<int32>(EGutDrug::Hush)] = 0.0f;

		TestEqual(TEXT("The heart is sound"), GameMode->GetLadderState().HeartDamage, 0.0f);
		TestTrue(TEXT("Dust is taken on a full habit"), GameMode->TakeDrug(EGutDrug::Dust));
		TestTrue(TEXT("And the heart pays"), GameMode->GetLadderState().HeartDamage > 0.0f);

		GameMode->ConfirmCard();

		TestEqual(TEXT("For good"), Fight.Player->GetMaxHealth(), *WholeHealth - GameMode->GetLadderState().HeartDamage, 0.5f);

		// back to single fights, so that whatever test comes next finds the cage as it expects it
		GameMode->EnterSandbox();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
