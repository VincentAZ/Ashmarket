// Ashmarket: the cage fight.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/CharacterMovementComponent.h"
#include "GutLadderData.h"
#include "GutSound.h"

/**
 *  What a fighter's body decides.
 *
 *  Ashmarket.Gut.Bodies   a woman's blow carries less than a man's and comes sooner; she covers the floor faster and a
 *                         dodge costs her less; about three of every ten fighters on the ladders are women
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBodiesTest, "Ashmarket.Gut.Bodies", GutTests::Flags)

bool FGutBodiesTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float EnemyHealth = 0.0f;
		float Damage = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);

		// the same fighter, first with a man's body and then with a woman's
		Fight.Player->SetWoman(false);

		const FGutAttackDef Asked = *Fight.Player->GetNextLightAttack();
		const float Pace = Fight.Player->GetCharacterMovement()->MaxWalkSpeed;
		const float DodgeCost = Fight.Player->GetDodgeStaminaCost();

		Notes->EnemyHealth = Fight.Enemy->GetHealth();
		Notes->Damage = Asked.Damage;

		Fight.Player->SetWoman(true);

		TestTrue(TEXT("A woman covers the floor faster than a man"), Fight.Player->GetCharacterMovement()->MaxWalkSpeed > Pace * 1.03f);
		TestTrue(TEXT("A dodge costs her less"), Fight.Player->GetDodgeStaminaCost() < DodgeCost * 0.95f);
		TestTrue(TEXT("She swings when asked"), Fight.Player->TryLightAttack());
		TestTrue(TEXT("Her blow comes sooner than a man's"), Fight.Player->GetCurrentAttack().Windup < Asked.Windup * 0.97f);
		TestTrue(TEXT("And she is back from it sooner"), Fight.Player->GetCurrentAttack().Recovery < Asked.Recovery * 0.97f);
		TestTrue(TEXT("It carries less"), Fight.Player->GetCurrentAttack().Damage < Asked.Damage * 0.95f);
	} });

	Steps.Add({ 1.4f, [this, Notes](const FFight& Fight)
	{
		const float Lost = Notes->EnemyHealth - Fight.Enemy->GetHealth();

		TestTrue(TEXT("Her blow lands, and does less than a man's"), Lost > 0.0f && Lost < Notes->Damage * 0.95f);

		Fight.Player->SetWoman(false);

		// --- who is on the ladders ---

		UGutSoundSubsystem* Bodies = UGutSoundSubsystem::Get(Fight.Player);

		if (!TestNotNull(TEXT("The game knows which bodies are women's"), Bodies))
		{
			return;
		}

		int32 Fighters = 0;
		int32 Women = 0;

		for (const TCHAR* Path : { TEXT("/Game/Gut/Ladder/DA_Gut_MeatLadder"), TEXT("/Game/Gut/Ladder/DA_Gut_DogLadder"),
			TEXT("/Game/Gut/Ladder/DA_Gut_ButcherLadder"), TEXT("/Game/Gut/Ladder/DA_Gut_ChampionLadder") })
		{
			const UGutLadderData* Ladder = LoadObject<UGutLadderData>(nullptr, Path);

			if (!TestNotNull(Path, Ladder))
			{
				continue;
			}

			for (const FGutLadderRung& Rung : Ladder->Rungs)
			{
				if (!Rung.Body.IsNull())
				{
					++Fighters;
					Women += Bodies->VoiceOf(Rung.Body.GetAssetName()) == FName("woman") ? 1 : 0;
				}
			}
		}

		const float Share = Fighters > 0 ? static_cast<float>(Women) / Fighters : 0.0f;

		TestTrue(*FString::Printf(TEXT("About three in ten of the ladders' fighters are women (%d of %d)"), Women, Fighters), Fighters >= 10 && Share >= 0.22f && Share <= 0.36f);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif
