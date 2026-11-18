// Ashmarket: the cage fight.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutFighter.h"
#include "GutGameMode.h"
#include "InputCoreTypes.h"
#include "Tests/GutTestHelpers.h"

/**
 *  Not tests: measurements, for finding out what the game does with what a player's hands do. Not part of a test run.
 *
 *  Ashmarket.Probe.Mash    hammers the light attack through the player's own keys, eight seconds at an opponent out of reach
 *                          and eight at one in reach that does nothing. Run it with -GutStateLog and read when each swing began:
 *                          GUT_ARGS=-GutStateLog GUT_TESTS=Ashmarket.Probe.Mash Tools/run_gut_tests.sh
 *                          With -GutProbeBrain as well the opponent keeps its brain and fights back.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutProbeMashTest, "Ashmarket.Probe.Mash", GutTests::Flags)

bool FGutProbeMashTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FScriptCommand::FStep> Steps;

	// -GutProbeBrain leaves the opponent its brain and hammers at it for twenty seconds from where the bell leaves them
	if (FParse::Param(FCommandLine::Get(), TEXT("GutProbeBrain")))
	{
		for (float Time = 0.0f; Time < 20.0f; Time += 0.16f)
		{
			Steps.Add({ 0.08f, [](const FFight& Fight)
			{
				WalkToward(Fight.Player, Fight.Enemy);
				SendKey(Fight, EKeys::LeftMouseButton, true);
			} });
			Steps.Add({ 0.08f, [](const FFight& Fight) { SendKey(Fight, EKeys::LeftMouseButton, false); } });
		}

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

		return true;
	}

	Steps.Add({ 1.0f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);
		PlaceAtGap(Fight, 400.0f);

		UE_LOG(LogTemp, Log, TEXT("Mash: out of reach, from %.3f"), Fight.Player->GetWorld()->GetTimeSeconds());
	} });

	// a press every sixth of a second, held for half of it: somebody hammering a mouse button
	const auto Hammer = [&Steps](float Seconds)
	{
		for (float Time = 0.0f; Time < Seconds; Time += 0.16f)
		{
			Steps.Add({ 0.08f, [](const FFight& Fight) { SendKey(Fight, EKeys::LeftMouseButton, true); } });
			Steps.Add({ 0.08f, [](const FFight& Fight) { SendKey(Fight, EKeys::LeftMouseButton, false); } });
		}
	};

	Hammer(8.0f);

	Steps.Add({ 3.0f, [](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		UE_LOG(LogTemp, Log, TEXT("Mash: in reach, from %.3f"), Fight.Player->GetWorld()->GetTimeSeconds());
	} });

	Hammer(8.0f);

	Steps.Add({ 1.0f, [](const FFight& Fight) {} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif
