// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 *  The story told in pictures.
 *
 *  Ashmarket.Gut.Story    nothing is told in a run nobody watches; told, each part has the screen to itself, the confirm key
 *                         shows the next picture and nothing else is taken, and after the last the screen underneath is back
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutStoryTest, "Ashmarket.Gut.Story", GutTests::Flags)

bool FGutStoryTest::RunTest(const FString& Parameters)
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

		GameMode->SetStoriesEnabled(false);
		GameMode->TellStory(EGutStory::Sold);

		TestFalse(TEXT("Nothing is told when stories are off"), GameMode->IsTellingStory());

		GameMode->SetStoriesEnabled(true);
		GameMode->TellStory(EGutStory::None);

		TestFalse(TEXT("And there is no story of nothing"), GameMode->IsTellingStory());

		GameMode->TellStory(EGutStory::Sold);

		if (!TestTrue(TEXT("How the player was sold is told in pictures"), GameMode->IsTellingStory() && GameMode->GetStoryLength() == 4 && GameMode->GetStoryIndex() == 0))
		{
			return;
		}

		TestTrue(TEXT("Each picture has words under it"), GameMode->GetStoryPanel() && GameMode->GetStoryPanel()->Words.Num() > 0 && !GameMode->GetStoryPanel()->Picture.IsNone());

		const EGutFightPhase Before = GameMode->GetPhase();
		const int32 Choice = GameMode->GetPlayerChoice();

		GameMode->HandleMenuInput(EGutMenuInput::Left);
		GameMode->HandleMenuInput(EGutMenuInput::Up);

		TestTrue(TEXT("While it is told, the screen underneath takes nothing"), GameMode->GetPhase() == Before && GameMode->GetPlayerChoice() == Choice && GameMode->GetStoryIndex() == 0);
	} });

	for (int32 i = 0; i < 4; ++i)
	{
		Steps.Add({ 0.4f, [this, i](const FFight& Fight) { Screenshot(FString::Printf(TEXT("63_story_sold_%d"), i + 1)); } });

		Steps.Add({ 0.2f, [this, i](const FFight& Fight)
		{
			Fight.GameMode->HandleMenuInput(EGutMenuInput::Confirm);

			if (i < 3)
			{
				TestEqual(TEXT("The confirm key shows the next picture"), Fight.GameMode->GetStoryIndex(), i + 1);
			}
			else
			{
				TestFalse(TEXT("After the last, the story is over"), Fight.GameMode->IsTellingStory());
			}
		} });
	}

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->TellStory(EGutStory::Tagged);

		TestTrue(TEXT("The gate has its picture"), Fight.GameMode->IsTellingStory() && Fight.GameMode->GetStoryLength() == 1);
	} });

	Steps.Add({ 0.4f, [this](const FFight& Fight) { Screenshot(TEXT("64_story_tagged")); } });

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->AdvanceStory();
		Fight.GameMode->TellStory(EGutStory::Champion);

		TestTrue(TEXT("And so has the Champion's end"), Fight.GameMode->IsTellingStory() && Fight.GameMode->GetStoryLength() == 1);
	} });

	Steps.Add({ 0.4f, [this](const FFight& Fight) { Screenshot(TEXT("65_story_champion")); } });

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->AdvanceStory();
		Fight.GameMode->TellStory(EGutStory::Owner);

		TestTrue(TEXT("An owner's first day has two"), Fight.GameMode->IsTellingStory() && Fight.GameMode->GetStoryLength() == 2);

		Fight.GameMode->AdvanceStory();
	} });

	Steps.Add({ 0.4f, [this](const FFight& Fight) { Screenshot(TEXT("66_story_owner")); } });

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->AdvanceStory();
		Fight.GameMode->TellStory(EGutStory::Gambler);

		TestTrue(TEXT("And a gambler's first night one"), Fight.GameMode->IsTellingStory() && Fight.GameMode->GetStoryLength() == 1);
	} });

	Steps.Add({ 0.4f, [this](const FFight& Fight) { Screenshot(TEXT("67_story_gambler")); } });

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->HandleMenuInput(EGutMenuInput::Confirm);

		TestFalse(TEXT("The screen underneath is back"), Fight.GameMode->IsTellingStory());

		// as every other test finds it
		Fight.GameMode->SetStoriesEnabled(false);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
