// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutGameInstance.h"
#include "Kismet/GameplayStatics.h"

/**
 *  The menu that stops the game.
 *
 *  Ashmarket.Gut.Stopped    up, the game stands still and no other screen listens; its lines; how loud; the way out to the
 *                           street only where there is one; down, the game goes on
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutStoppedTest, "Ashmarket.Gut.Stopped", GutTests::Flags)

bool FGutStoppedTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	// at the rack of a single fight
	if (!OpenCage(this, true))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		TestFalse(TEXT("The game is going"), GameMode->IsStopped() || UGameplayStatics::IsGamePaused(GameMode));

		GameMode->SetStopped(true);

		TestTrue(TEXT("Stopped, everything stands still"), GameMode->IsStopped() && UGameplayStatics::IsGamePaused(GameMode));

		const TArray<FString> Lines = GameMode->GetStoppedLines();

		TestTrue(TEXT("At the rack of a single fight there is a way out to the street"), Lines.Num() == 4 && Lines[0] == TEXT("Go on") && Lines[2] == TEXT("Out to the street") && Lines[3] == TEXT("Leave the game"));

		// the screen underneath does not listen
		const int32 Choice = GameMode->GetPlayerChoice();

		GameMode->HandleMenuInput(EGutMenuInput::Right);

		TestEqual(TEXT("The rack takes nothing while the game is stopped"), GameMode->GetPlayerChoice(), Choice);

		// how loud
		GameMode->HandleStoppedInput(EGutMenuInput::Down);

		TestEqual(TEXT("Down goes to the next line"), GameMode->GetStoppedChoice(), 1);

		const int32 Loud = GameInstance->Loudness;

		GameMode->HandleStoppedInput(EGutMenuInput::Confirm);

		TestEqual(TEXT("Each press on the sound makes it a third quieter"), GameInstance->Loudness, (Loud + 3) % 4);
		TestTrue(TEXT("And the line says so"), GameMode->GetStoppedLines()[1] != Lines[1]);

		for (int32 i = 0; i < 3; ++i)
		{
			GameMode->HandleStoppedInput(EGutMenuInput::Confirm);
		}

		TestEqual(TEXT("Four presses bring it round to where it was"), GameInstance->Loudness, Loud);
		TestTrue(TEXT("It is still stopped"), GameMode->IsStopped());

		// for the picture: the menu left up, with time allowed to pass so that the test can go on to take it
		GameMode->HandleStoppedInput(EGutMenuInput::Up);

		UGameplayStatics::SetGamePaused(GameMode, false);
	} });

	Steps.Add({ 0.4f, [this](const FFight& Fight) { Screenshot(TEXT("69_stopped")); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		TestEqual(TEXT("Up from the second line is the first"), GameMode->GetStoppedChoice(), 0);

		GameMode->HandleStoppedInput(EGutMenuInput::Confirm);

		TestFalse(TEXT("Go on, and the game goes on"), GameMode->IsStopped() || UGameplayStatics::IsGamePaused(GameMode));

		// in a fight there is no way out to the street: only on, quieter, or out of the game
		DisableEnemyBrain(this, Fight);

		GameMode->SkipRack();
		GameMode->StartFight();
		GameMode->SetStopped(true);

		const TArray<FString> Lines = GameMode->GetStoppedLines();

		TestTrue(TEXT("In a fight the menu has no way out to the street"), Lines.Num() == 3 && !Lines.Contains(TEXT("Out to the street")));

		const float Health = Fight.Player->GetHealth();

		GameMode->SetStopped(false);

		TestTrue(TEXT("And the fight is as it was left"), !UGameplayStatics::IsGamePaused(GameMode) && Fight.Player->GetHealth() == Health && GameMode->GetPhase() == EGutFightPhase::Fighting);
	} });

	// and by the keys themselves, as somebody playing would: Escape stops it, S and Enter work the menu, Escape lets it go on.
	// While it is stopped the cage's clock stands still, so each step here waits on what it expects and not on the clock.
	Steps.Add({ 0.3f, [](const FFight& Fight)
	{
		SendKey(Fight, EKeys::Escape, true);
		SendKey(Fight, EKeys::Escape, false);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	auto Until = [this](TFunction<bool(const FFight&)> Reached, const TCHAR* Failure)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[Reached]() { const FFight Fight = GetFight(); return Fight.IsValid() && Reached(Fight); },
			[this, Failure]() { AddError(Failure); return true; },
			8.0f));
	};

	auto Press = [this](const FKey& Key)
	{
		TArray<FStep> One;

		One.Add({ 0.0f, [Key](const FFight& Fight)
		{
			SendKey(Fight, Key, true);
			SendKey(Fight, Key, false);
		} });

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(One)));
	};

	TSharedRef<int32> Loud = MakeShared<int32>(3);

	Until([Loud](const FFight& Fight) { *Loud = Fight.GameMode->GetGameInstance<UGutGameInstance>()->Loudness; return Fight.GameMode->IsStopped(); }, TEXT("The Escape key did not stop the game"));

	Press(EKeys::S);

	Until([](const FFight& Fight) { return Fight.GameMode->GetStoppedChoice() == 1; }, TEXT("The S key did not move down the stopped menu"));

	Press(EKeys::Enter);

	Until([Loud](const FFight& Fight) { return Fight.GameMode->GetGameInstance<UGutGameInstance>()->Loudness != *Loud; }, TEXT("The Enter key did not change how loud the game is"));

	Press(EKeys::Escape);

	Until([](const FFight& Fight) { return !Fight.GameMode->IsStopped() && !UGameplayStatics::IsGamePaused(Fight.GameMode); }, TEXT("The Escape key did not let the game go on"));

	// whatever happened, the game is left going and as loud as it was, for the tests that come after
	TArray<FStep> Last;

	Last.Add({ 0.0f, [Loud](const FFight& Fight)
	{
		Fight.GameMode->SetStopped(false);
		Fight.GameMode->GetGameInstance<UGutGameInstance>()->Loudness = 3;
		Fight.GameMode->ApplyLoudness();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Last)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
