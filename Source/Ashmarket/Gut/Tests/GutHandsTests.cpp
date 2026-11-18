// Ashmarket: the cage fight.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutWeaponData.h"

/**
 *  What the player's hands get for what they do.
 *
 *  Ashmarket.Gut.Hands    a blow asked for without the stamina for it still comes, late and weak; a shield strikes with
 *                         the shield while the guard is held, and jabs when it is not, and the guard comes back up after
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutHandsTest, "Ashmarket.Gut.Hands", GutTests::Flags)

bool FGutHandsTest::RunTest(const FString& Parameters)
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

	// --- with nothing left, the blow still comes ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);

		const FGutAttackDef Asked = *Fight.Player->GetNextLightAttack();

		Notes->EnemyHealth = Fight.Enemy->GetHealth();
		Notes->Damage = Asked.Damage;

		Fight.Player->SetStamina(0.0f);

		TestTrue(TEXT("A fighter with no stamina still swings when asked"), Fight.Player->TryLightAttack());
		TestEqual(TEXT("And is winding up"), Fight.Player->GetFighterState(), EGutFighterState::Windup);
		TestTrue(TEXT("The blow is slower to come than the same blow fresh"), Fight.Player->GetCurrentAttack().Windup > Asked.Windup * 1.2f);
		TestTrue(TEXT("And slower to get back from"), Fight.Player->GetCurrentAttack().Recovery > Asked.Recovery * 1.1f);
		TestTrue(TEXT("The screen is told why"), Fight.Player->GetTimeSinceStaminaFail() < 0.1f);
	} });

	Steps.Add({ 1.6f, [this, Notes](const FFight& Fight)
	{
		const float Lost = Notes->EnemyHealth - Fight.Enemy->GetHealth();

		TestTrue(TEXT("It lands, and does less than the same blow fresh"), Lost > 0.0f && Lost < Notes->Damage * 0.8f);

		// --- the shield: fist with the guard down, the shield itself with the guard up ---

		UGutWeaponData* Shield = FindRackWeapon(Fight, TEXT("Shield"));

		if (!TestNotNull(TEXT("The rack has a shield"), Shield))
		{
			return;
		}

		Fight.Player->EquipWeapon(Shield);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("The shield has a blow of its own from behind the guard"), Shield->LightAttacks.IsValidIndex(Shield->GuardedLightAttack));
		TestEqual(TEXT("With the guard down a shield carrier leads with the fist"), Fight.Player->GetNextLightAttack()->Label, Shield->LightAttacks[0].Label);

		Fight.Player->SetBlocking(true);
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const UGutWeaponData* Shield = Fight.Player->GetWeapon();

		if (!Shield || !Shield->LightAttacks.IsValidIndex(Shield->GuardedLightAttack))
		{
			return;
		}

		TestTrue(TEXT("The guard is up"), Fight.Player->IsBlocking());
		TestTrue(TEXT("From behind the guard the shield carrier can still strike"), Fight.Player->TryLightAttack());
		TestEqual(TEXT("And strikes with the shield"), Fight.Player->GetCurrentAttack().Label, Shield->LightAttacks[Shield->GuardedLightAttack].Label);
	} });

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("With the guard still held it comes back up after the blow"), Fight.Player->IsBlocking());

		Fight.Player->SetBlocking(false);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif
