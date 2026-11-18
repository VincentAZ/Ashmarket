// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/PlayerController.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"

/**
 *  The street outside the Gut.
 *
 *  Ashmarket.Gut.Door    the three ways in and the single fight; each way in leads where it says and has a way back out;
 *                        a way that has been used before can be gone back into as it stood, or started over
 */
namespace GutDoorTests
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
			[Test]() { Test->AddError(TEXT("The cage was not loaded again after a door was gone through")); return true; },
			30.0f));

		Steps.Reset();
	}

	/** Puts the choice on the line given and takes it, as a player would with the keys */
	bool Take(const FFight& Fight, int32 DoorLine)
	{
		const TArray<FGutCardItem> Items = Fight.GameMode->GetCardItems();
		const int32 Wanted = Items.IndexOfByKey(FGutCardItem{ EGutCardItemType::Door, DoorLine });

		if (Wanted == INDEX_NONE)
		{
			return false;
		}

		// up to the top, then down to it
		Fight.GameMode->MoveCardSelection(-Items.Num());
		Fight.GameMode->MoveCardSelection(Wanted);
		Fight.GameMode->ActivateCardSelection();

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutDoorTest, "Ashmarket.Gut.Door", GutTests::Flags)

bool FGutDoorTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutDoorTests;

	// at the rack of a single fight, which is one of the places the street is reached from
	if (!OpenCage(this, true))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		UGutGameInstance* GameInstance = Fight.GameMode->GetGameInstance<UGutGameInstance>();

		// nobody has been here before
		GameInstance->bLadderStarted = false;
		GameInstance->Stable = FGutStableState();
		GameInstance->Window = FGutWindowState();

		TestFalse(TEXT("A cage opened straight into a fight has no street behind it"), Fight.GameMode->CameByDoor() || Fight.GameMode->IsAtDoor());

		Fight.GameMode->GoToDoor();
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- outside ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("The player is outside"), GameMode->IsAtDoor());
		TestFalse(TEXT("Which is not on the ladder, at the window or in the yard"), GameMode->IsOnLadder() || GameMode->IsAtWindow() || GameMode->IsAtStable() || GameMode->IsGamblersOwnFight());
		TestEqual(TEXT("The ways in are on a card"), GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("Three ways in and a single fight"), GameMode->GetCardItems().Num(), 4);
		TestTrue(TEXT("Nobody is driven"), GameMode->GetWorld()->GetFirstPlayerController()->GetPawn() == nullptr);

		Screenshot(TEXT("51_door"));
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight) { TestTrue(TEXT("There is a way in for an owner"), Take(Fight, GutDoorLines::Own)); } });

	RunThenWaitForNextFight(this, Steps);

	// --- in as an owner, and out again ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		TestTrue(TEXT("It leads to the yard"), GameMode->IsAtStable() && !GameMode->IsAtDoor());
		TestTrue(TEXT("Which remembers the street"), GameMode->CameByDoor());
		TestEqual(TEXT("An empty yard's card has the Block, the day and the gate"), GameMode->GetCardItems().Num(), 3);

		// something to know the yard by
		GameInstance->Stable.Water = 33.0f;

		TestTrue(TEXT("There is a way back out"), Take(Fight, GutDoorLines::Leave));
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("Outside again"), GameMode->IsAtDoor());
		TestEqual(TEXT("The yard is still there, and can be started over"), GameMode->GetCardItems().Num(), 5);
		TestEqual(TEXT("It is as it was left"), GameMode->GetGameInstance<UGutGameInstance>()->Stable.Water, 33.0f, 0.01f);

		Screenshot(TEXT("52_door_with_a_yard"));
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight) { TestTrue(TEXT("It can be started over"), Take(Fight, GutDoorLines::Afresh + GutDoorLines::Own)); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("In the yard"), GameMode->IsAtStable());
		TestTrue(TEXT("A new one: the first day, and what an owner comes with"), GameMode->GetStableState().Day == 1 && GameMode->GetStableState().Water != 33.0f && GameMode->GetStableState().Water > 0.0f);

		TestTrue(TEXT("Out again"), Take(Fight, GutDoorLines::Leave));
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- in to bet ---

	Steps.Add({ 0.3f, [this](const FFight& Fight) { TestTrue(TEXT("There is a way in for a gambler"), Take(Fight, GutDoorLines::Bet)); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("It leads to the window"), GameMode->IsAtWindow() && GameMode->GetWindowState().bStarted);
		TestEqual(TEXT("The card has the six fixes, the stairs down and the window"), GameMode->GetCardItems().Num(), 8);
		TestTrue(TEXT("Back down the stairs"), Take(Fight, GutDoorLines::Leave));
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- in to fight ---

	Steps.Add({ 0.3f, [this](const FFight& Fight) { TestTrue(TEXT("There is a way in for a fighter"), Take(Fight, GutDoorLines::Fight)); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("It leads to the ladder"), GameMode->IsOnLadder() && GameMode->GetGameInstance<UGutGameInstance>()->bLadderStarted);
		TestTrue(TEXT("From the first fight"), GameMode->GetLadderState().Rank == 0 && GameMode->GetLadderState().Rung == 0);
		TestTrue(TEXT("The player is in the cage"), GameMode->GetWorld()->GetFirstPlayerController()->GetPawn() == Fight.Player);
		TestTrue(TEXT("And can walk back out from the card"), Take(Fight, GutDoorLines::Leave));
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- a single fight ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("All three have been used, and each can be started over"), Fight.GameMode->GetCardItems().Num(), 7);
		TestTrue(TEXT("There is a single fight to be had"), Take(Fight, GutDoorLines::Spar));
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestTrue(TEXT("It is a fight with no ladder under it"), !GameMode->IsOnLadder() && !GameMode->IsAtDoor() && GameMode->GetPhase() == EGutFightPhase::Choosing);

		// down at the rack is the way back out
		GameMode->HandleMenuInput(EGutMenuInput::Down);
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		UGutGameInstance* GameInstance = Fight.GameMode->GetGameInstance<UGutGameInstance>();

		TestTrue(TEXT("Outside, the way it was come from"), Fight.GameMode->IsAtDoor());

		// back to single fights with nothing behind them, so that whatever test comes next finds the cage as it expects it
		GameInstance->bLadderStarted = false;
		GameInstance->Stable = FGutStableState();
		GameInstance->Window = FGutWindowState();

		Fight.GameMode->EnterSandbox();
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("And the street is forgotten"), Fight.GameMode->IsAtDoor() || Fight.GameMode->CameByDoor());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
