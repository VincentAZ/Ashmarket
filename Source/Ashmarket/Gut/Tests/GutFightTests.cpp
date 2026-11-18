// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GutDroppedWeapon.h"
#include "GutDummyController.h"
#include "GutOpponentController.h"
#include "GutPlayerCharacter.h"
#include "GutWeaponData.h"
#include "GutWeaponRack.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationCommon.h"

/**
 *  Tests for the Gut, all played in the real cage level:
 *
 *  Ashmarket.Gut.Mechanics          each fighter mechanic in isolation, with the enemy's brain switched off
 *  Ashmarket.Gut.Controls           simulated key presses reach the fighter through the real input assets
 *  Ashmarket.Gut.DummyKillsPlayer   a player who never touches the controls dies to the dummy brain
 *  Ashmarket.Gut.PlayerKillsDummy   a simple scripted player beats the dummy brain, then restarts the fight
 *  Ashmarket.Gut.Opponent           the opponent brain approaches, attacks, blocks, dodges, hits back and retreats
 *  Ashmarket.Gut.Limbs              limbs take damage and cripple: the weapon drops, legs slow, the guard costs more
 *  Ashmarket.Gut.Finishers          a beaten fighter staggers, gets a second wind if left, and dies to a finisher
 *  Ashmarket.Gut.Rack               weapons are drawn at the rack with the real inputs, and a rematch offers the same two again
 *  Ashmarket.Gut.Weapons            the four weapons differ in reach, speed, damage and guard, in their data and in the cage
 *
 *  Every test but the one about the rack leaves it at once with the weapons the fighters start with: a club each.
 *
 *  Run them with Tools/run_gut_tests.sh. They need a game world, so they run with -game, not in the editor.
 *  The opponent's balance is measured separately, over many fights, in GutBalanceTests.cpp.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutMechanicsTest, "Ashmarket.Gut.Mechanics", GutTests::Flags)

bool FGutMechanicsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	// the numbers the fighters are expected to show, worked out from their own attack data as the script goes
	struct FExpected
	{
		float EnemyHealth = 0.0f;
		float PlayerHealth = 0.0f;
		float EnemyStamina = 0.0f;
	};

	TSharedRef<FExpected> Expected = MakeShared<FExpected>();

	// --- before the bell ---

	Steps.Add({ 0.2f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("The fight starts in the intro"), Fight.GameMode->GetPhase(), EGutFightPhase::Intro);
		TestEqual(TEXT("Player starts at full health"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
		TestEqual(TEXT("Player starts at full stamina"), Fight.Player->GetStamina(), Fight.Player->GetMaxStamina());
		TestFalse(TEXT("No attack before the bell"), Fight.Player->TryLightAttack());
		TestFalse(TEXT("No dodge before the bell"), Fight.Player->TryDodge(FVector::ZeroVector));

		// this test drives both fighters by hand
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		TestEqual(TEXT("Start Fight rings the bell"), Fight.GameMode->GetPhase(), EGutFightPhase::Fighting);

		Expected->EnemyHealth = Fight.Enemy->GetMaxHealth();
		Expected->PlayerHealth = Fight.Player->GetMaxHealth();
	} });

	// --- a light attack that hits nothing: costs stamina, which comes back ---

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		Place(Fight.Player, -400.0f, 0.0f);
		Place(Fight.Enemy, 400.0f, 180.0f);

		const float Cost = Fight.Player->GetNextLightAttack()->StaminaCost;

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
		TestEqual(TEXT("Light attack is in its windup"), Fight.Player->GetFighterState(), EGutFighterState::Windup);
		TestEqual(TEXT("Light attack costs stamina"), Fight.Player->GetStamina(), Fight.Player->GetMaxStamina() - Cost);
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("Attack ends back in idle"), Fight.Player->GetFighterState(), EGutFighterState::Idle);
		TestEqual(TEXT("A miss does no damage"), Fight.Enemy->GetHealth(), Fight.Enemy->GetMaxHealth());
	} });

	Steps.Add({ 2.0f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("Stamina regenerates to full"), Fight.Player->GetStamina(), Fight.Player->GetMaxStamina());
	} });

	// --- a light attack that connects: damage and a stun ---

	Steps.Add({ 0.0f, [this, Expected](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Expected->EnemyHealth -= Fight.Player->GetNextLightAttack()->Damage;

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.35f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("Light attack lands as a clean hit"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Hit);
		TestEqual(TEXT("Light attack does its damage"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
		TestEqual(TEXT("A clean hit stuns"), Fight.Enemy->GetFighterState(), EGutFighterState::Stunned);
	} });

	// --- a blocked attack: no damage, stamina instead ---

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The stun wears off"), Fight.Enemy->GetFighterState(), EGutFighterState::Idle);

		PlaceFaceToFace(Fight);

		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
		Fight.Enemy->SetBlocking(true);
	} });

	Steps.Add({ 0.2f, [this, Expected](const FFight& Fight)
	{
		TestTrue(TEXT("The guard goes up"), Fight.Enemy->IsBlocking());

		Expected->EnemyStamina = Fight.Enemy->GetMaxStamina() - Fight.Player->GetNextLightAttack()->BlockStaminaDamage;

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.35f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("The guard stops a frontal attack"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("A blocked attack does no damage"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
		TestEqual(TEXT("A blocked attack costs the blocker stamina"), Fight.Enemy->GetStamina(), Expected->EnemyStamina);
		TestTrue(TEXT("The guard stays up"), Fight.Enemy->IsBlocking());
	} });

	// --- the guard does not cover the back ---

	Steps.Add({ 1.5f, [this, Expected](const FFight& Fight)
	{
		// with nobody to track, the guard stays pointed where it is put
		Fight.Enemy->SetOpponent(nullptr);

		Place(Fight.Player, -70.0f, 0.0f);
		Place(Fight.Enemy, 70.0f, 0.0f);

		Expected->EnemyHealth -= Fight.Player->GetNextLightAttack()->Damage;

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.35f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("An attack from behind gets past the guard"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Hit);
		TestEqual(TEXT("An attack from behind does its damage"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);

		Fight.Enemy->SetOpponent(Fight.Player);
	} });

	// --- a guard with no stamina behind it breaks ---

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(5.0f);

		TestTrue(TEXT("The guard is still up"), Fight.Enemy->IsBlocking());
		TestTrue(TEXT("Heavy attack starts"), Fight.Player->TryHeavyAttack());
	} });

	Steps.Add({ 0.4f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("Heavy attack is still winding up"), Fight.Player->GetFighterState(), EGutFighterState::Windup);
		TestEqual(TEXT("Nothing has landed yet"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
	} });

	Steps.Add({ 0.5f, [this, Expected](const FFight& Fight)
	{
		// the guard break share is 0.5 by default
		Expected->EnemyHealth -= Fight.Player->GetHeavyAttack().Damage * 0.5f;

		TestEqual(TEXT("A guard without stamina breaks"), Fight.Enemy->GetLastHitResult(), EGutHitResult::GuardBroken);
		TestEqual(TEXT("A broken guard lets half the damage through"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
		TestEqual(TEXT("A broken guard empties stamina"), Fight.Enemy->GetStamina(), 0.0f);
		TestEqual(TEXT("A broken guard stuns"), Fight.Enemy->GetFighterState(), EGutFighterState::Stunned);

		Fight.Enemy->SetBlocking(false);
	} });

	// --- a dodge lets an attack pass through ---

	Steps.Add({ 2.0f, [this](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());

		TestTrue(TEXT("The enemy's light attack starts"), Fight.Enemy->TryLightAttack());
	} });

	Steps.Add({ 0.12f, [this](const FFight& Fight)
	{
		// dodge into the enemy, so the player is certainly still in reach when the blow arrives
		TestTrue(TEXT("Dodge starts"), Fight.Player->TryDodge(FVector(1.0f, 0.0f, 0.0f)));
		TestEqual(TEXT("Dodge costs stamina"), Fight.Player->GetStamina(), Fight.Player->GetMaxStamina() - Fight.Player->GetDodgeStaminaCost());
	} });

	Steps.Add({ 0.6f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The attack passes through the dodge"), Fight.Player->GetLastHitResult(), EGutHitResult::Dodged);
		TestEqual(TEXT("A dodged attack does no damage"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
	} });

	// --- an unguarded fighter takes the hit, and the light chain steps on ---

	Steps.Add({ 1.0f, [this, Expected](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Expected->PlayerHealth -= Fight.Enemy->GetNextLightAttack()->Damage;

		TestTrue(TEXT("The enemy's light attack starts"), Fight.Enemy->TryLightAttack());
	} });

	Steps.Add({ 0.35f, [this, Expected](const FFight& Fight)
	{
		TestEqual(TEXT("The enemy's attack lands on an open player"), Fight.Player->GetLastHitResult(), EGutHitResult::Hit);
		TestEqual(TEXT("The player takes the damage"), Fight.Player->GetHealth(), Expected->PlayerHealth);
	} });

	// --- death ends the fight ---

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetHealth(5.0f);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	// long enough for the attack to be over, so the next press is not just buffered
	Steps.Add({ 0.9f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy staggers at zero health instead of dying"), Fight.Enemy->IsAlive() && Fight.Enemy->IsStaggered());
		TestEqual(TEXT("A stagger does not end the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::Fighting);

		// the same input that attacks now finishes
		TestTrue(TEXT("Light attack on a staggered enemy starts a finisher"), Fight.Player->TryLightAttack());
		TestEqual(TEXT("The player is finishing"), Fight.Player->GetFighterState(), EGutFighterState::Finishing);
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("The finisher kills"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The enemy's death wins the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The player is alive"), Fight.Player->IsAlive());
		TestFalse(TEXT("No attacks after the fight"), Fight.Player->TryLightAttack());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutControlsTest, "Ashmarket.Gut.Controls", GutTests::Flags)

bool FGutControlsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	// where the player stood before a step that should move it
	TSharedRef<FVector> Mark = MakeShared<FVector>(FVector::ZeroVector);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestNotNull(TEXT("The player's fighter is controlled by a player controller"), Cast<APlayerController>(Fight.Player->GetController()));

		// keep the dummy out of it, and far enough away that nothing connects
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		Place(Fight.Player, -300.0f, 0.0f);
		Place(Fight.Enemy, 500.0f, 180.0f);
	} });

	// --- left mouse: light attack ---

	Steps.Add({ 0.2f, [](const FFight& Fight) { SendKey(Fight, EKeys::LeftMouseButton, true); } });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::LeftMouseButton, false);

		TestEqual(TEXT("Left mouse starts an attack"), Fight.Player->GetFighterState(), EGutFighterState::Windup);
		TestEqual(TEXT("Left mouse attacks light"), Fight.Player->GetCurrentAttackType(), EGutAttackType::Light);
	} });

	// --- F: heavy attack ---

	Steps.Add({ 1.2f, [](const FFight& Fight) { SendKey(Fight, EKeys::F, true); } });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::F, false);

		TestEqual(TEXT("F starts an attack"), Fight.Player->GetFighterState(), EGutFighterState::Windup);
		TestEqual(TEXT("F attacks heavy"), Fight.Player->GetCurrentAttackType(), EGutAttackType::Heavy);
	} });

	// --- right mouse: block while held ---

	Steps.Add({ 2.0f, [](const FFight& Fight) { SendKey(Fight, EKeys::RightMouseButton, true); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("Holding right mouse raises the guard"), Fight.Player->IsBlocking());

		SendKey(Fight, EKeys::RightMouseButton, false);
	} });

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("Releasing right mouse lowers the guard"), Fight.Player->IsBlocking());
	} });

	// --- space with no direction held: step back ---

	Steps.Add({ 0.1f, [Mark](const FFight& Fight)
	{
		*Mark = Fight.Player->GetActorLocation();

		SendKey(Fight, EKeys::SpaceBar, true);
	} });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::SpaceBar, false);

		TestEqual(TEXT("Space starts a dodge"), Fight.Player->GetFighterState(), EGutFighterState::Dodging);
	} });

	Steps.Add({ 0.8f, [this, Mark](const FFight& Fight)
	{
		// the player faces along +X, so straight back is -X
		TestTrue(TEXT("A dodge with no direction held steps back"), Fight.Player->GetActorLocation().X < Mark->X - 200.0f);
	} });

	// --- Tab: lock-on camera ---

	Steps.Add({ 0.0f, [this](const FFight& Fight)
	{
		const AGutPlayerCharacter* PlayerCharacter = Cast<AGutPlayerCharacter>(Fight.Player);

		if (TestNotNull(TEXT("The player's fighter is a Gut Player Character"), PlayerCharacter))
		{
			TestTrue(TEXT("The camera locks on by itself at the bell"), PlayerCharacter->IsLockedOn());
		}

		SendKey(Fight, EKeys::Tab, true);
	} });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::Tab, false);

		TestFalse(TEXT("Tab lets the camera go"), CastChecked<AGutPlayerCharacter>(Fight.Player)->IsLockedOn());
	} });

	Steps.Add({ 0.1f, [](const FFight& Fight) { SendKey(Fight, EKeys::Tab, true); } });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::Tab, false);

		TestTrue(TEXT("Tab locks the camera on again"), CastChecked<AGutPlayerCharacter>(Fight.Player)->IsLockedOn());

		// move the enemy round to the player's left, a quarter turn away
		const FVector PlayerLocation = Fight.Player->GetActorLocation();

		Fight.Enemy->SetActorLocation(PlayerLocation + FVector(0.0f, -400.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	} });

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		const float YawToEnemy = (Fight.Enemy->GetActorLocation() - Fight.Player->GetActorLocation()).Rotation().Yaw;
		const float CameraYaw = Fight.Player->GetController()->GetControlRotation().Yaw;

		TestTrue(TEXT("The locked camera swings round to the enemy"), FMath::Abs(FMath::FindDeltaAngleDegrees(CameraYaw, YawToEnemy)) < 8.0f);
		TestTrue(TEXT("The locked fighter turns to face the enemy"), FMath::Abs(FMath::FindDeltaAngleDegrees(Fight.Player->GetActorRotation().Yaw, YawToEnemy)) < 8.0f);

		// and back in front, for the steps that follow
		Fight.Enemy->SetActorLocation(Fight.Player->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	} });

	// --- W: walk forward ---

	Steps.Add({ 1.5f, [](const FFight& Fight) {} });

	Steps.Add({ 0.0f, [Mark](const FFight& Fight)
	{
		*Mark = Fight.Player->GetActorLocation();

		SendKey(Fight, EKeys::W, true);
	} });

	Steps.Add({ 0.6f, [this, Mark](const FFight& Fight)
	{
		SendKey(Fight, EKeys::W, false);

		TestTrue(TEXT("Holding W walks forward"), Fight.Player->GetActorLocation().X > Mark->X + 100.0f);
	} });

	// --- R: fight again, but only once the fight is over ---

	TSharedRef<TWeakObjectPtr<AGutGameMode>> FinishedGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("The fight cannot be restarted while it is running"), Fight.GameMode->CanRestart());

		// the cage kills the dummy
		Fight.Enemy->Kill(nullptr, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("A fighter killed by the cage still ends the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
	} });

	Steps.Add({ 2.0f, [FinishedGameMode](const FFight& Fight)
	{
		*FinishedGameMode = Fight.GameMode;

		SendKey(Fight, EKeys::R, true);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[FinishedGameMode]() { const FFight Fight = GetFight(); return Fight.IsValid() && Fight.GameMode != FinishedGameMode->Get(); },
		[this]() { AddError(TEXT("Pressing R after the fight did not start a new one")); return true; },
		30.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutDummyKillsPlayerTest, "Ashmarket.Gut.DummyKillsPlayer", GutTests::Flags)

bool FGutDummyKillsPlayerTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	// this test is about the dummy, which no longer stands in the cage by default
	TArray<FStep> Setup;

	Setup.Add({ 0.1f, [this](const FFight& Fight)
	{
		TestNotNull(TEXT("The enemy takes the dummy brain"), GiveBrain(Fight.Enemy, AGutDummyController::StaticClass()));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Setup)));

	// the player never touches the controls. Wait through the intro and the whole fight.
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[]() { const FFight Fight = GetFight(); return Fight.IsValid() && IsFightOver(Fight); },
		[this]() { AddError(TEXT("The dummy did not finish an idle player within two minutes")); return true; },
		120.0f));

	TArray<FStep> Steps;

	Steps.Add({ 0.0f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("An idle player loses"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestFalse(TEXT("The player is dead"), Fight.Player->IsAlive());
		TestTrue(TEXT("The dummy is alive"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The dummy was never touched"), Fight.Enemy->GetHealth(), Fight.Enemy->GetMaxHealth());

		AddInfo(FString::Printf(TEXT("The dummy killed an idle player %.1f seconds after the level started"), Fight.GameMode->GetWorld()->GetTimeSeconds()));
	} });

	Steps.Add({ 2.0f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The fight can be restarted after a loss"), Fight.GameMode->CanRestart());

		Screenshot(TEXT("06_player_lost"));
	} });

	// give the screenshot a moment to be written
	Steps.Add({ 0.5f, [](const FFight& Fight) {} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutPlayerKillsDummyTest, "Ashmarket.Gut.PlayerKillsDummy", GutTests::Flags)

bool FGutPlayerKillsDummyTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Intro;

	// this test is about the dummy, which no longer stands in the cage by default
	Intro.Add({ 0.1f, [this](const FFight& Fight)
	{
		TestNotNull(TEXT("The enemy takes the dummy brain"), GiveBrain(Fight.Enemy, AGutDummyController::StaticClass()));
	} });

	Intro.Add({ 0.9f, [](const FFight& Fight) { Screenshot(TEXT("01_intro")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Intro)));

	// true from the moment the player defends against an attack until it answers with a heavy
	TSharedRef<bool> bOwedHeavy = MakeShared<bool>(false);

	// play the player's side with the same calls the input bindings make:
	// walk in, guard against lights, dodge heavies, hit back when it is safe
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[bOwedHeavy]()
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return true;
			}

			if (Fight.GameMode->GetPhase() == EGutFightPhase::Intro)
			{
				return false;
			}

			if (IsFightOver(Fight))
			{
				return true;
			}

			AGutFighter* Player = Fight.Player;
			AGutFighter* Enemy = Fight.Enemy;

			const float Gap = Player->GetGapTo(Enemy);
			const FVector ToEnemy = (Enemy->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();

			const bool bEnemySwinging = Enemy->GetFighterState() == EGutFighterState::Windup || Enemy->GetFighterState() == EGutFighterState::Active;
			const bool bThreatened = bEnemySwinging && Gap <= Enemy->GetCurrentAttack().Reach + 80.0f;
			const bool bHeavyIncoming = bThreatened && Enemy->GetCurrentAttackType() == EGutAttackType::Heavy;

			// sidestep a heavy just before it lands
			if (bHeavyIncoming && Enemy->GetFighterState() == EGutFighterState::Windup && Enemy->GetStateTime() >= Enemy->GetCurrentAttack().Windup - 0.12f)
			{
				Player->SetBlocking(false);

				if (Player->TryDodge(FVector::CrossProduct(FVector::UpVector, ToEnemy)))
				{
					*bOwedHeavy = true;

					Screenshot(TEXT("04_dodge"));
				}

				return false;
			}

			// guard against everything else
			Player->SetBlocking(bThreatened && !bHeavyIncoming);

			if (bThreatened)
			{
				*bOwedHeavy = true;

				if (Player->IsBlocking() && Enemy->GetFighterState() == EGutFighterState::Active)
				{
					Screenshot(TEXT("03_block"));
				}

				return false;
			}

			if (Player->GetFighterState() == EGutFighterState::Active)
			{
				Screenshot(Player->GetCurrentAttackType() == EGutAttackType::Heavy ? TEXT("05_heavy_attack") : TEXT("02_light_attack"));
			}

			const FGutAttackDef* NextLight = Player->GetNextLightAttack();

			if (Gap > NextLight->Reach * 0.8f)
			{
				// walk in
				WalkToward(Player, Enemy);
			}
			else if (Player->GetFighterState() == EGutFighterState::Idle)
			{
				// the dummy has just swung and will not swing again for a while: time for one heavy.
				// Otherwise keep the light chain going while stamina lasts.
				if (*bOwedHeavy && Player->GetStamina() >= 50.0f)
				{
					*bOwedHeavy = !Player->TryHeavyAttack();
				}
				else if (Player->GetStamina() >= 35.0f)
				{
					Player->TryLightAttack();
				}
			}

			return false;
		},
		[this]() { AddError(TEXT("The scripted player did not finish the fight within three minutes")); return true; },
		180.0f));

	TArray<FStep> Steps;

	Steps.Add({ 0.0f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The scripted player wins"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The player is alive"), Fight.Player->IsAlive());
		TestFalse(TEXT("The dummy is dead"), Fight.Enemy->IsAlive());

		AddInfo(FString::Printf(TEXT("The scripted player won %.1f seconds after the level started, with %.0f health left"),
			Fight.GameMode->GetWorld()->GetTimeSeconds(), Fight.Player->GetHealth()));

		Fight.Player->SetBlocking(false);
	} });

	Steps.Add({ 2.0f, [this](const FFight& Fight)
	{
		Screenshot(TEXT("07_player_won"));

		TestTrue(TEXT("The fight can be restarted after a win"), Fight.GameMode->CanRestart());
	} });

	// fight again
	TSharedRef<TWeakObjectPtr<AGutGameMode>> FinishedGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

	Steps.Add({ 0.5f, [FinishedGameMode](const FFight& Fight)
	{
		*FinishedGameMode = Fight.GameMode;

		Fight.GameMode->RequestRestart();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[FinishedGameMode]() { const FFight Fight = GetFight(); return Fight.IsValid() && Fight.GameMode != FinishedGameMode->Get(); },
		[this]() { AddError(TEXT("The fighters never appeared after a restart")); return true; },
		30.0f));

	TArray<FStep> AfterRestart;

	AfterRestart.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("A restart goes back to the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestTrue(TEXT("The dummy is alive again"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The dummy is back to full health"), Fight.Enemy->GetHealth(), Fight.Enemy->GetMaxHealth());
		TestEqual(TEXT("The player is back to full health"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(AfterRestart)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutOpponentTest, "Ashmarket.Gut.Opponent", GutTests::Flags)

bool FGutOpponentTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	// what the script needs to remember from one step to the next
	struct FNotes
	{
		/** Tuning that makes the opponent do nothing of its own accord, so each behaviour can be switched on alone */
		FGutOpponentTuning Passive;

		float Gap = 0.0f;
		float Health = 0.0f;
		FVector Location = FVector::ZeroVector;

		/** Furthest the opponent has been seen from Location */
		float Travel = 0.0f;

		/** If true the opponent has been seen mid dodge */
		bool bSawDodge = false;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	auto GetBrain = [](const FFight& Fight) { return Cast<AGutOpponentController>(Fight.Enemy->GetController()); };

	// 1 if the opponent is facing the player squarely, -1 if it has its back turned
	auto GetFacing = [](const FFight& Fight)
	{
		const FVector ToPlayer = (Fight.Player->GetActorLocation() - Fight.Enemy->GetActorLocation()).GetSafeNormal2D();

		return static_cast<float>(FVector::DotProduct(Fight.Enemy->GetActorForwardVector(), ToPlayer));
	};

	// stands the fighters mid cage, with the opponent inside the player's heavy attack range but not so close that it feels crowded
	auto StandPlayerInHeavyRange = [GetBrain](const FFight& Fight)
	{
		// just outside the distance the opponent treats as crowding. GetGapTo measures to the surface, so add the capsule radius back.
		const float Distance = GetBrain(Fight)->GetHoldGapMin(Fight.Player) + 12.0f + 35.0f;

		Place(Fight.Player, -Distance * 0.5f, 0.0f);
		Place(Fight.Enemy, Distance * 0.5f, 180.0f);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
	};

	// undoes whatever the last scenario did to the player
	auto RestorePlayer = [](const FFight& Fight)
	{
		Fight.Player->SetHealth(Fight.Player->GetMaxHealth());

		for (const EGutLimb Limb : { EGutLimb::LeftArm, EGutLimb::RightArm, EGutLimb::LeftLeg, EGutLimb::RightLeg })
		{
			Fight.Player->SetLimbHealth(Limb, Fight.Player->GetMaxLimbHealth());
		}
	};

	TArray<FStep> Steps;

	// --- approach, then hold just outside reach ---

	Steps.Add({ 0.2f, [this, Notes, GetBrain](const FFight& Fight)
	{
		AGutOpponentController* Brain = GetBrain(Fight);

		if (!TestNotNull(TEXT("The fighter in the cage is run by the opponent brain"), Brain))
		{
			return;
		}

		Notes->Passive = Brain->GetTuning();
		Notes->Passive.MinDecisionInterval = 0.3f;
		Notes->Passive.MaxDecisionInterval = 0.3f;
		Notes->Passive.AttackChance = 0.0f;
		Notes->Passive.CrowdedAttackChance = 0.0f;
		Notes->Passive.GuardChance = 0.0f;
		Notes->Passive.CounterChance = 0.0f;
		Notes->Passive.PunishChance = 0.0f;
		Notes->Passive.BlockLightChance = 0.0f;
		Notes->Passive.BlockHeavyChance = 0.0f;
		Notes->Passive.DodgeHeavyChance = 0.0f;
		Notes->Passive.HeavyChance = 0.0f;
		Notes->Passive.MinReactionTime = 0.2f;
		Notes->Passive.MaxReactionTime = 0.2f;

		// when the player crowds it, a passive opponent covers up. Keep that short, so its guard is down again soon after the player steps back.
		Notes->Passive.MinGuardTime = 0.2f;
		Notes->Passive.MaxGuardTime = 0.2f;

		Brain->SetTuning(Notes->Passive);

		Notes->Gap = Fight.Enemy->GetGapTo(Fight.Player);

		Fight.GameMode->StartFight();
	} });

	Steps.Add({ 0.4f, [this, Notes, GetBrain](const FFight& Fight)
	{
		TestEqual(TEXT("The opponent starts by approaching"), GetBrain(Fight)->GetBrainState(), EGutOpponentState::Approach);
		TestTrue(TEXT("Approaching closes the distance"), Fight.Enemy->GetGapTo(Fight.Player) < Notes->Gap - 50.0f);
	} });

	Steps.Add({ 3.0f, [this, Notes, GetBrain](const FFight& Fight)
	{
		const float Gap = Fight.Enemy->GetGapTo(Fight.Player);

		TestEqual(TEXT("The opponent stops short and holds"), GetBrain(Fight)->GetBrainState(), EGutOpponentState::Hold);
		TestTrue(TEXT("It holds just outside the player's light attack reach"),
			Gap >= GetBrain(Fight)->GetHoldGapMin(Fight.Player) - 15.0f && Gap <= GetBrain(Fight)->GetHoldGapMax(Fight.Player) + 15.0f);

		Notes->Location = Fight.Enemy->GetActorLocation();
		Notes->Travel = 0.0f;

		// swing the camera off to one side, so the screenshot shows the opponent rather than the player's back
		if (AController* PlayerController = Fight.Player->GetController())
		{
			const float YawToEnemy = (Fight.Enemy->GetActorLocation() - Fight.Player->GetActorLocation()).Rotation().Yaw;

			PlayerController->SetControlRotation(FRotator(-15.0f, YawToEnemy + 35.0f, 0.0f));
		}
	} });

	// the opponent turns round now and then as it circles, so look more than once to see that it has moved
	auto NoteTravel = [Notes](const FFight& Fight)
	{
		Notes->Travel = FMath::Max(Notes->Travel, static_cast<float>(FVector::Dist2D(Fight.Enemy->GetActorLocation(), Notes->Location)));
	};

	Steps.Add({ 0.5f, NoteTravel });
	Steps.Add({ 0.5f, NoteTravel });

	Steps.Add({ 0.5f, [this, Notes, GetFacing, NoteTravel, GetBrain](const FFight& Fight)
	{
		NoteTravel(Fight);

		const float Gap = Fight.Enemy->GetGapTo(Fight.Player);

		TestTrue(TEXT("It circles while it holds"), Notes->Travel > 50.0f);
		TestTrue(TEXT("It keeps its distance while circling"),
			Gap >= GetBrain(Fight)->GetHoldGapMin(Fight.Player) - 15.0f && Gap <= GetBrain(Fight)->GetHoldGapMax(Fight.Player) + 15.0f);
		TestTrue(TEXT("It faces the player while circling"), GetFacing(Fight) > 0.9f);
		TestEqual(TEXT("A passive opponent has not touched the player"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());

		Screenshot(TEXT("08_opponent_circles"));
	} });

	// --- attack ---

	Steps.Add({ 0.0f, [Notes, GetBrain](const FFight& Fight)
	{
		FGutOpponentTuning Tuning = Notes->Passive;
		Tuning.AttackChance = 1.0f;

		GetBrain(Fight)->SetTuning(Tuning);
	} });

	Steps.Add({ 1.6f, [this, Notes, GetBrain](const FFight& Fight)
	{
		TestTrue(TEXT("The opponent steps in and lands an attack on an idle player"), Fight.Player->GetHealth() < Fight.Player->GetMaxHealth());

		GetBrain(Fight)->SetTuning(Notes->Passive);
	} });

	// --- block on reaction ---

	Steps.Add({ 2.5f, [StandPlayerInHeavyRange](const FFight& Fight) { StandPlayerInHeavyRange(Fight); } });

	Steps.Add({ 0.8f, [this, Notes, GetBrain, StandPlayerInHeavyRange, RestorePlayer](const FFight& Fight)
	{
		FGutOpponentTuning Tuning = Notes->Passive;
		Tuning.BlockHeavyChance = 1.0f;

		GetBrain(Fight)->SetTuning(Tuning);

		StandPlayerInHeavyRange(Fight);
		RestorePlayer(Fight);

		Notes->Health = Fight.Enemy->GetHealth();

		TestFalse(TEXT("The opponent's guard is down before the swing"), Fight.Enemy->IsBlocking());
		TestTrue(TEXT("The player's heavy attack starts"), Fight.Player->TryHeavyAttack());
	} });

	Steps.Add({ 0.45f, [this, GetBrain](const FFight& Fight)
	{
		TestTrue(TEXT("The opponent sees the heavy coming and raises its guard"), Fight.Enemy->IsBlocking());
		TestEqual(TEXT("Its brain is guarding"), GetBrain(Fight)->GetBrainState(), EGutOpponentState::Guard);

		Screenshot(TEXT("09_opponent_blocks"));
	} });

	Steps.Add({ 0.6f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The heavy is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("Blocking costs the opponent no health"), Fight.Enemy->GetHealth(), Notes->Health);
	} });

	// --- dodge on reaction ---

	Steps.Add({ 1.5f, [StandPlayerInHeavyRange](const FFight& Fight) { StandPlayerInHeavyRange(Fight); } });

	Steps.Add({ 0.8f, [this, Notes, GetBrain, StandPlayerInHeavyRange](const FFight& Fight)
	{
		FGutOpponentTuning Tuning = Notes->Passive;
		Tuning.DodgeHeavyChance = 1.0f;

		GetBrain(Fight)->SetTuning(Tuning);

		StandPlayerInHeavyRange(Fight);

		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());

		TestFalse(TEXT("The opponent's guard is down before the swing"), Fight.Enemy->IsBlocking());
		TestTrue(TEXT("The player's heavy attack starts"), Fight.Player->TryHeavyAttack());
	} });

	// the dodge is timed off the heavy's windup, which depends on what the player is holding, so look more than once
	auto NoteDodge = [Notes](const FFight& Fight)
	{
		Notes->bSawDodge |= Fight.Enemy->GetFighterState() == EGutFighterState::Dodging;
	};

	Steps.Add({ 0.6f, NoteDodge });
	Steps.Add({ 0.1f, NoteDodge });
	Steps.Add({ 0.1f, NoteDodge });

	Steps.Add({ 0.4f, [this, Notes](const FFight& Fight)
	{
		TestTrue(TEXT("The opponent dodges just before the heavy lands"), Notes->bSawDodge);
		TestEqual(TEXT("Dodging costs the opponent no health"), Fight.Enemy->GetHealth(), Notes->Health);
	} });

	// --- hit back after a block ---

	Steps.Add({ 1.5f, [StandPlayerInHeavyRange](const FFight& Fight) { StandPlayerInHeavyRange(Fight); } });

	Steps.Add({ 0.8f, [this, Notes, GetBrain, StandPlayerInHeavyRange](const FFight& Fight)
	{
		FGutOpponentTuning Tuning = Notes->Passive;
		Tuning.BlockHeavyChance = 1.0f;
		Tuning.CounterChance = 1.0f;

		GetBrain(Fight)->SetTuning(Tuning);

		StandPlayerInHeavyRange(Fight);

		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());

		TestFalse(TEXT("The opponent's guard is down before the swing"), Fight.Enemy->IsBlocking());

		TestEqual(TEXT("The player is unhurt before the exchange"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
		TestTrue(TEXT("The player's heavy attack starts"), Fight.Player->TryHeavyAttack());
	} });

	Steps.Add({ 1.7f, [this, Notes, GetBrain](const FFight& Fight)
	{
		TestEqual(TEXT("The heavy is blocked again"), Fight.Enemy->GetHealth(), Notes->Health);
		TestTrue(TEXT("The opponent hits back straight after the block"), Fight.Player->GetHealth() < Fight.Player->GetMaxHealth());

		GetBrain(Fight)->SetTuning(Notes->Passive);
	} });

	// --- retreat when out of stamina, and come back ---

	Steps.Add({ 2.0f, [Notes](const FFight& Fight)
	{
		// mid cage, so there is room to back off
		Place(Fight.Player, -150.0f, 0.0f);
		Place(Fight.Enemy, 100.0f, 180.0f);

		Fight.Enemy->SetStamina(5.0f);

		Notes->Gap = Fight.Enemy->GetGapTo(Fight.Player);
	} });

	Steps.Add({ 0.5f, [this, GetBrain](const FFight& Fight)
	{
		TestEqual(TEXT("An opponent with no stamina retreats"), GetBrain(Fight)->GetBrainState(), EGutOpponentState::Retreat);
	} });

	Steps.Add({ 1.0f, [this, Notes, GetFacing](const FFight& Fight)
	{
		UE_LOG(LogGut, Display, TEXT("Retreat: the gap went from %.0f to %.0f cm"), Notes->Gap, Fight.Enemy->GetGapTo(Fight.Player));

		TestTrue(TEXT("Retreating opens the distance"), Fight.Enemy->GetGapTo(Fight.Player) > Notes->Gap + 100.0f);
		TestTrue(TEXT("It backs off without turning its back"), GetFacing(Fight) > 0.9f);

		Screenshot(TEXT("10_opponent_retreats"));
	} });

	Steps.Add({ 3.5f, [this, Notes, GetBrain](const FFight& Fight)
	{
		TestTrue(TEXT("It gets its stamina back"), Fight.Enemy->GetStamina() >= Notes->Passive.RecoveredStamina);
		TestNotEqual(TEXT("It stops retreating once it has"), GetBrain(Fight)->GetBrainState(), EGutOpponentState::Retreat);
		TestTrue(TEXT("It comes back to the player"), Fight.Enemy->GetGapTo(Fight.Player) <= GetBrain(Fight)->GetHoldGapMax(Fight.Player) + 60.0f);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLimbsTest, "Ashmarket.Gut.Limbs", GutTests::Flags)

bool FGutLimbsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float LeftArm = 0.0f;
		float RightArm = 0.0f;
		float WalkSpeed = 0.0f;
		float Stamina = 0.0f;
		float Damage = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	auto CountDroppedWeapons = [](const FFight& Fight)
	{
		int32 Count = 0;

		for (TActorIterator<AGutDroppedWeapon> It(Fight.GameMode->GetWorld()); It; ++It)
		{
			++Count;
		}

		return Count;
	};

	TArray<FStep> Steps;

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		TestTrue(TEXT("The enemy starts the fight armed"), Fight.Enemy->HasWeapon());
		TestEqual(TEXT("Limbs start whole"), Fight.Enemy->GetLimbHealth(EGutLimb::RightArm), Fight.Enemy->GetMaxLimbHealth());
	} });

	// --- an attacker standing off to one side hits the limb on that side ---

	Steps.Add({ 0.1f, [this, Notes](const FFight& Fight)
	{
		// the enemy faces down the cage toward -X, so its right hand side is toward -Y. The player stands there.
		Place(Fight.Enemy, 70.0f, 180.0f);

		Fight.Player->SetActorLocationAndRotation(FVector(70.0f, -140.0f, FloorZ), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

		const FGutAttackDef* Attack = Fight.Player->GetNextLightAttack();

		TestEqual(TEXT("The opening light attack lands on the left arm from in front"), Attack->HitZone, EGutHitZone::LeftArm);

		Notes->Damage = Attack->Damage * Attack->LimbDamageScale;

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("From the enemy's right, the same attack lands on its right arm"), Fight.Enemy->GetLimbHealth(EGutLimb::RightArm), Fight.Enemy->GetMaxLimbHealth() - Notes->Damage);
		TestEqual(TEXT("The left arm is untouched"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm), Fight.Enemy->GetMaxLimbHealth());
		TestFalse(TEXT("One hit does not cripple an arm"), Fight.Enemy->IsLimbCrippled(EGutLimb::RightArm));
	} });

	// --- a blocked attack does no limb damage ---

	Steps.Add({ 0.5f, [](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
		Fight.Enemy->SetBlocking(true);
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		Notes->LeftArm = Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm);
		Notes->RightArm = Fight.Enemy->GetLimbHealth(EGutLimb::RightArm);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The attack is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("A blocked attack does not hurt the left arm"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm), Notes->LeftArm);
		TestEqual(TEXT("A blocked attack does not hurt the right arm"), Fight.Enemy->GetLimbHealth(EGutLimb::RightArm), Notes->RightArm);

		Fight.Enemy->SetBlocking(false);
	} });

	// --- two light attacks in a chain: the first lands on the left arm, the second cripples the weapon arm ---

	Steps.Add({ 0.5f, [this](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("First light attack starts"), Fight.Player->TryLightAttack());
	} });

	// pressed again during the first attack's recovery, the way a player chains
	Steps.Add({ 0.6f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The first attack is recovering"), Fight.Player->GetFighterState(), EGutFighterState::Recovery);

		Fight.Player->TryLightAttack();
	} });

	Steps.Add({ 0.25f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The second attack of the chain lands on the right arm from in front"), Fight.Player->GetCurrentAttack().HitZone, EGutHitZone::RightArm);
		TestTrue(TEXT("The second attack is under way"), Fight.Player->IsAttacking());
	} });

	Steps.Add({ 0.8f, [this, Notes, CountDroppedWeapons](const FFight& Fight)
	{
		TestTrue(TEXT("The first attack hurt the left arm"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm) < Notes->LeftArm);
		TestTrue(TEXT("The second attack crippled the right arm"), Fight.Enemy->IsLimbCrippled(EGutLimb::RightArm));
		TestFalse(TEXT("A crippled weapon arm drops the weapon"), Fight.Enemy->HasWeapon());
		TestEqual(TEXT("The dropped weapon is lying in the cage"), CountDroppedWeapons(Fight), 1);
		TestTrue(TEXT("The enemy is alive and still in the fight"), Fight.Enemy->IsAlive() && !Fight.Enemy->IsStaggered());
		TestTrue(TEXT("The player still has its weapon"), Fight.Player->HasWeapon());

		Screenshot(TEXT("11_weapon_dropped"));
	} });

	Steps.Add({ 0.1f, [this](const FFight& Fight)
	{
		// disarmed, the enemy fights on with its fists
		const FGutAttackDef* EnemyAttack = Fight.Enemy->GetNextLightAttack();
		const FGutAttackDef* PlayerAttack = Fight.Player->GetNextLightAttack();

		if (TestNotNull(TEXT("A disarmed fighter can still attack"), EnemyAttack))
		{
			TestTrue(TEXT("Bare hands reach less far than the weapon"), EnemyAttack->Reach < PlayerAttack->Reach);
		}

		// and cannot pick a weapon up again with that arm
		TestFalse(TEXT("A crippled weapon arm cannot hold a weapon"), Fight.Enemy->EquipWeapon(const_cast<UGutWeaponData*>(Fight.Player->GetArmament())));
	} });

	// --- each crippled arm makes blocking cost more ---

	Steps.Add({ 1.0f, [Notes](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
		Fight.Enemy->SetBlocking(true);
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		// one arm is crippled, and the penalty is a quarter of the cost again for each by default
		Notes->Stamina = Fight.Enemy->GetMaxStamina() - (Fight.Player->GetNextLightAttack()->BlockStaminaDamage * 1.25f);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.45f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The attack is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("Blocking with a crippled arm costs a quarter as much again"), Fight.Enemy->GetStamina(), Notes->Stamina);

		Fight.Enemy->SetBlocking(false);
	} });

	// --- crippled legs slow the fighter ---

	Steps.Add({ 1.0f, [this, Notes](const FFight& Fight)
	{
		Notes->WalkSpeed = Fight.Enemy->GetCharacterMovement()->MaxWalkSpeed;

		TestTrue(TEXT("The enemy can walk"), Notes->WalkSpeed > 0.0f);

		Fight.Enemy->SetLimbHealth(EGutLimb::LeftLeg, 0.0f);

		// 0.7 is the default slowdown for each crippled leg
		TestEqual(TEXT("One crippled leg slows the fighter"), Fight.Enemy->GetCharacterMovement()->MaxWalkSpeed, Notes->WalkSpeed * 0.7f, 0.5f);

		Fight.Enemy->SetLimbHealth(EGutLimb::RightLeg, 0.0f);

		TestEqual(TEXT("Two crippled legs slow it more"), Fight.Enemy->GetCharacterMovement()->MaxWalkSpeed, Notes->WalkSpeed * 0.49f, 0.5f);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFinishersTest, "Ashmarket.Gut.Finishers", GutTests::Flags)

bool FGutFinishersTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float FirstStagger = 0.0f;
		bool bSawPlayerStaggered = false;
		bool bSawEnemyFinishing = false;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	TArray<FStep> Steps;

	// --- a beaten fighter staggers instead of dying ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);

		Fight.Enemy->SetHealth(5.0f);

		TestFalse(TEXT("A fighter that is not staggered cannot be finished"), Fight.Player->CanFinish(Fight.Enemy));
		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.45f, [this, Notes](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy staggers at zero health"), Fight.Enemy->IsStaggered());
		TestTrue(TEXT("A staggered fighter is still alive"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("It has no health left"), Fight.Enemy->GetHealth(), 0.0f);
		TestEqual(TEXT("The fight goes on"), Fight.GameMode->GetPhase(), EGutFightPhase::Fighting);

		Notes->FirstStagger = Fight.Enemy->GetStaggerTimeRemaining();

		TestTrue(TEXT("The stagger is on a clock"), Notes->FirstStagger > 3.0f);

		Screenshot(TEXT("12_staggered"));
	} });

	// --- an ordinary attack cannot end it ---

	Steps.Add({ 0.5f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("An ordinary attack does nothing to a staggered fighter"), Fight.Enemy->ReceiveAttack(Fight.Player, Fight.Player->GetHeavyAttack()), EGutHitResult::Ignored);
		TestTrue(TEXT("It is still staggered"), Fight.Enemy->IsStaggered());

		// nor can a finisher from across the cage
		Place(Fight.Player, -400.0f, 0.0f);

		TestFalse(TEXT("A staggered fighter out of reach cannot be finished"), Fight.Player->CanFinish(Fight.Enemy));
		TestFalse(TEXT("So the finisher does not start"), Fight.Player->TryFinisher(EGutFinisherStyle::Clean));
	} });

	// --- left alone, it gets a second wind ---

	Steps.Add({ 3.6f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("The stagger runs out"), Fight.Enemy->IsStaggered());
		TestEqual(TEXT("The fighter is free again"), Fight.Enemy->GetFighterState(), EGutFighterState::Idle);

		// 15 is the default second wind
		TestEqual(TEXT("It comes back with a little health"), Fight.Enemy->GetHealth(), 15.0f);
	} });

	// --- beaten again, the stagger is shorter, and a heavy attack finishes it the slow way ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetHealth(5.0f);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.45f, [this, Notes](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy staggers again"), Fight.Enemy->IsStaggered());
		TestTrue(TEXT("The second stagger is shorter than the first"), Fight.Enemy->GetStaggerTimeRemaining() < Notes->FirstStagger - 0.5f);
	} });

	// wait for the attack to finish, so the heavy input is not just buffered
	Steps.Add({ 0.5f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The player can finish a staggered enemy in reach"), Fight.Player->CanFinish(Fight.Enemy));
		TestTrue(TEXT("Heavy attack on a staggered enemy starts a finisher"), Fight.Player->TryHeavyAttack());
		TestEqual(TEXT("The player is finishing"), Fight.Player->GetFighterState(), EGutFighterState::Finishing);
		TestEqual(TEXT("The heavy input is the slow finisher"), Fight.Player->GetCurrentFinisher().Label, Fight.Player->GetArmament()->SlowFinisher.Label);
		TestTrue(TEXT("The enemy is being finished"), Fight.Enemy->IsBeingFinished());
		TestFalse(TEXT("One finisher at a time"), Fight.Player->CanFinish(Fight.Enemy));
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The slow finisher takes its time"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The player is still finishing"), Fight.Player->GetFighterState(), EGutFighterState::Finishing);

		Screenshot(TEXT("13_finisher"));
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("The finisher kills"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The kill wins the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The player is alive"), Fight.Player->IsAlive());
	} });

	// --- the other way round: the opponent's brain finishes a staggered player ---

	TSharedRef<TWeakObjectPtr<AGutGameMode>> FinishedGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

	Steps.Add({ 1.5f, [FinishedGameMode](const FFight& Fight)
	{
		*FinishedGameMode = Fight.GameMode;

		Fight.GameMode->RequestRestart();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[FinishedGameMode]() { const FFight Fight = GetFight(); return Fight.IsValid() && Fight.GameMode != FinishedGameMode->Get(); },
		[this]() { AddError(TEXT("The fight did not restart")); return true; },
		30.0f));

	TArray<FStep> Setup;

	Setup.Add({ 0.2f, [this](const FFight& Fight)
	{
		// one touch from staggering, and standing still
		Fight.GameMode->SkipRack();
		Fight.GameMode->StartFight();

		Fight.Player->SetHealth(1.0f);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Setup)));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[Notes]()
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return true;
			}

			Notes->bSawPlayerStaggered |= Fight.Player->IsStaggered();
			Notes->bSawEnemyFinishing |= Fight.Enemy->GetFighterState() == EGutFighterState::Finishing;

			return IsFightOver(Fight);
		},
		[this]() { AddError(TEXT("The opponent did not finish a player with one health within two minutes")); return true; },
		120.0f));

	TArray<FStep> Verdict;

	Verdict.Add({ 0.0f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The opponent wins"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestTrue(TEXT("The player staggered before it died"), Notes->bSawPlayerStaggered);
		TestTrue(TEXT("The opponent finished it"), Notes->bSawEnemyFinishing);
		TestFalse(TEXT("The player is dead"), Fight.Player->IsAlive());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Verdict)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutRackTest, "Ashmarket.Gut.Rack", GutTests::Flags)

bool FGutRackTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	// this test is about the rack, so the fighters stay at it
	if (!OpenCage(this, true))
	{
		return false;
	}

	struct FNotes
	{
		int32 EnemyChoice = 0;
		FVector Mark = FVector::ZeroVector;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// taps a key: down now, up a moment later
	auto Tap = [](TArray<FStep>& Steps, float Delay, const FKey& Key)
	{
		Steps.Add({ Delay, [Key](const FFight& Fight) { SendKey(Fight, Key, true); } });
		Steps.Add({ 0.1f, [Key](const FFight& Fight) { SendKey(Fight, Key, false); } });
	};

	TArray<FStep> Steps;

	// --- the fight opens at the rack, with a weapon already drawn for each fighter ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const TArray<TObjectPtr<UGutWeaponData>>& Weapons = Fight.GameMode->GetRackWeapons();

		TestEqual(TEXT("The fight starts at the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestEqual(TEXT("There are four weapons on the rack"), Weapons.Num(), 4);

		if (Weapons.Num() != 4)
		{
			return;
		}

		TestEqual(TEXT("The player starts on the first weapon"), Fight.GameMode->GetPlayerChoice(), 0);
		TestTrue(TEXT("The player is holding the weapon it has drawn"), Fight.Player->GetArmament() == Weapons[0]);
		TestTrue(TEXT("The opponent is holding the weapon it has drawn"), Fight.Enemy->GetArmament() == Weapons[Fight.GameMode->GetEnemyChoice()]);
		TestFalse(TEXT("Nobody swings at the rack"), Fight.Player->TryLightAttack());

		if (TestNotNull(TEXT("There is a rack on the wall"), Fight.GameMode->GetRack()))
		{
			TestFalse(TEXT("A drawn weapon is off the board"), Fight.GameMode->GetRack()->IsOnBoard(Weapons[0]));
		}

		Notes->EnemyChoice = Fight.GameMode->GetEnemyChoice();
		Notes->Mark = Fight.Player->GetActorLocation();
	} });

	// once the light in the cage has settled
	Steps.Add({ 1.0f, [](const FFight& Fight) { Screenshot(TEXT("14_rack")); } });

	// --- D: the next weapon ---

	Tap(Steps, 0.3f, EKeys::D);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("D draws the next weapon"), Fight.GameMode->GetPlayerChoice(), 1);
		TestTrue(TEXT("The player is holding it"), Fight.Player->GetArmament() == Fight.GameMode->GetRackWeapons()[1]);
		TestTrue(TEXT("The first weapon is back on the board"),
			Fight.GameMode->GetEnemyChoice() == 0 || Fight.GameMode->GetRack()->IsOnBoard(Fight.GameMode->GetRackWeapons()[0]));
	} });

	// --- A: the one before, wrapping round the end of the rack ---

	Tap(Steps, 0.1f, EKeys::A);
	Tap(Steps, 0.1f, EKeys::A);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("A twice wraps round to the last weapon"), Fight.GameMode->GetPlayerChoice(), 3);
		TestTrue(TEXT("The player is holding it"), Fight.Player->GetArmament() == Fight.GameMode->GetRackWeapons()[3]);

		Screenshot(TEXT("15_rack_shield"));
	} });

	// --- W: change what the opponent draws ---

	Tap(Steps, 0.1f, EKeys::W);

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("W moves the opponent's draw along"), Fight.GameMode->GetEnemyChoice(), (Notes->EnemyChoice + 1) % 4);
		TestTrue(TEXT("The opponent is holding it"), Fight.Enemy->GetArmament() == Fight.GameMode->GetRackWeapons()[Fight.GameMode->GetEnemyChoice()]);

		Notes->EnemyChoice = Fight.GameMode->GetEnemyChoice();
	} });

	// --- the keys that choose weapons do not also walk the fighter ---

	Steps.Add({ 0.1f, [](const FFight& Fight) { SendKey(Fight, EKeys::S, true); } });

	Steps.Add({ 0.4f, [this, Notes](const FFight& Fight)
	{
		SendKey(Fight, EKeys::S, false);

		TestTrue(TEXT("The fighter stands still at the rack"), FVector::Dist2D(Fight.Player->GetActorLocation(), Notes->Mark) < 5.0f);
	} });

	// --- left mouse: take it ---

	Tap(Steps, 0.1f, EKeys::LeftMouseButton);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("Left mouse leaves the rack for the intro"), Fight.GameMode->GetPhase(), EGutFightPhase::Intro);
		TestTrue(TEXT("The player keeps the weapon it drew"), Fight.Player->GetArmament() == Fight.GameMode->GetRackWeapons()[3]);
		TestFalse(TEXT("Still nobody swings before the bell"), Fight.Player->TryLightAttack());
	} });

	// the rack keys no longer choose anything
	Tap(Steps, 0.1f, EKeys::D);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The draw cannot be changed after leaving the rack"), Fight.GameMode->GetPlayerChoice(), 3);
	} });

	// --- the bell rings by itself, and the fighter walks again ---

	Steps.Add({ 3.0f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The bell rings after the intro"), Fight.GameMode->GetPhase(), EGutFightPhase::Fighting);

		DisableEnemyBrain(this, Fight);

		Notes->Mark = Fight.Player->GetActorLocation();

		SendKey(Fight, EKeys::S, true);
	} });

	Steps.Add({ 0.5f, [this, Notes](const FFight& Fight)
	{
		SendKey(Fight, EKeys::S, false);

		TestTrue(TEXT("The fighter walks again once the fight is on"), FVector::Dist2D(Fight.Player->GetActorLocation(), Notes->Mark) > 30.0f);
	} });

	// --- a rematch comes back to the rack with the same two weapons drawn ---

	TSharedRef<TWeakObjectPtr<AGutGameMode>> FinishedGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

	Steps.Add({ 0.2f, [](const FFight& Fight)
	{
		Fight.Enemy->Kill(nullptr, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 2.0f, [FinishedGameMode](const FFight& Fight)
	{
		*FinishedGameMode = Fight.GameMode;

		Fight.GameMode->RequestRestart();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[FinishedGameMode]() { const FFight Fight = GetFight(); return Fight.IsValid() && Fight.GameMode != FinishedGameMode->Get(); },
		[this]() { AddError(TEXT("The fighters never appeared after a restart")); return true; },
		30.0f));

	TArray<FStep> AfterRestart;

	AfterRestart.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("A rematch starts at the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestEqual(TEXT("The player's last weapon is drawn again"), Fight.GameMode->GetPlayerChoice(), 3);
		TestEqual(TEXT("The opponent's last weapon is drawn again"), Fight.GameMode->GetEnemyChoice(), Notes->EnemyChoice);
		TestTrue(TEXT("The player is holding it"), Fight.Player->GetArmament() == Fight.GameMode->GetRackWeapons()[3]);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(AfterRestart)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutWeaponsTest, "Ashmarket.Gut.Weapons", GutTests::Flags)

bool FGutWeaponsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float Health = 0.0f;
		float Stamina = 0.0f;
		float Limb = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// the scenarios below wear the enemy's limbs down, and each needs them whole to start with
	auto HealLimbs = [](AGutFighter* Fighter)
	{
		for (const EGutLimb Limb : { EGutLimb::LeftArm, EGutLimb::RightArm, EGutLimb::LeftLeg, EGutLimb::RightLeg })
		{
			Fighter->SetLimbHealth(Limb, Fighter->GetMaxLimbHealth());
		}
	};

	TArray<FStep> Steps;

	// --- the data: four weapons, no two alike in reach, speed or damage ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		const TArray<TObjectPtr<UGutWeaponData>>& Weapons = Fight.GameMode->GetRackWeapons();

		TestEqual(TEXT("There are four weapons on the rack"), Weapons.Num(), 4);

		for (int32 i = 0; i < Weapons.Num(); ++i)
		{
			const UGutWeaponData* Weapon = Weapons[i];
			const FString Name = Weapon->GetName();

			TestTrue(Name + TEXT(" has light attacks"), Weapon->LightAttacks.Num() > 0);
			TestNotNull(Name + TEXT(" has a shape"), Weapon->Mesh.Get());
			TestFalse(Name + TEXT(" has a name"), Weapon->DisplayName.IsEmpty());
			TestFalse(Name + TEXT(" has a description"), Weapon->Description.IsEmpty());

			for (int32 j = i + 1; j < Weapons.Num(); ++j)
			{
				const UGutWeaponData* Other = Weapons[j];
				const FString Pair = Name + TEXT(" and ") + Other->GetName();

				TestTrue(Pair + TEXT(" differ in reach"), FMath::Abs(Weapon->GetLightReach() - Other->GetLightReach()) >= 10.0f);
				TestTrue(Pair + TEXT(" differ in speed"), FMath::Abs(Weapon->GetLightTime() - Other->GetLightTime()) >= 0.05f);
				TestTrue(Pair + TEXT(" differ in damage"), FMath::Abs(Weapon->GetLightDamage() - Other->GetLightDamage()) >= 1.0f
					|| FMath::Abs(Weapon->HeavyAttack.Damage - Other->HeavyAttack.Damage) >= 2.0f);
			}

			// hammering at a guard must tire the attacker first, whatever the two are holding
			for (const UGutWeaponData* Guard : Weapons)
			{
				for (const FGutAttackDef& Attack : Weapon->LightAttacks)
				{
					TestTrue(FString::Printf(TEXT("Blocking %s's %s with %s costs less than throwing it"), *Name, *Attack.Label.ToString(), *Guard->GetName()),
						Attack.BlockStaminaDamage * Guard->BlockCostScale < Attack.StaminaCost);
				}
			}
		}
	} });

	// --- in the cage: every weapon lands inside its reach and misses outside it ---

	for (int32 Index = 0; Index < 4; ++Index)
	{
		Steps.Add({ 0.8f, [this, Notes, Index](const FFight& Fight)
		{
			UGutWeaponData* Weapon = Fight.GameMode->GetRackWeapons().IsValidIndex(Index) ? Fight.GameMode->GetRackWeapons()[Index].Get() : nullptr;

			if (!TestNotNull(TEXT("The weapon is on the rack"), Weapon))
			{
				return;
			}

			TestTrue(TEXT("The player takes the weapon"), Fight.Player->EquipWeapon(Weapon));

			Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
			Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth());

			// just inside the reach of its opening attack
			PlaceAtGap(Fight, Weapon->LightAttacks[0].Reach - 10.0f);

			Notes->Health = Fight.Enemy->GetHealth() - Weapon->LightAttacks[0].Damage;

			TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
		} });

		Steps.Add({ 1.2f, [this, Notes, Index](const FFight& Fight)
		{
			const UGutWeaponData* Weapon = Fight.Player->GetArmament();
			const FGutAttackDef& Opener = Weapon->LightAttacks[0];

			TestEqual(Weapon->GetName() + TEXT(" lands inside its reach"), Fight.Enemy->GetHealth(), Notes->Health);

			// further away than the attack can reach even with its step forward
			PlaceAtGap(Fight, Opener.Reach + Opener.LungeDistance + 30.0f);

			Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

			TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
		} });

		Steps.Add({ 1.2f, [this, Notes](const FFight& Fight)
		{
			TestEqual(Fight.Player->GetArmament()->GetName() + TEXT(" misses outside its reach"), Fight.Enemy->GetHealth(), Notes->Health);
		} });
	}

	// --- the chain hits from where the club cannot ---

	Steps.Add({ 0.8f, [this, Notes, HealLimbs](const FFight& Fight)
	{
		HealLimbs(Fight.Enemy);

		const UGutWeaponData* Club = FindRackWeapon(Fight, TEXT("Club"));
		UGutWeaponData* Chain = FindRackWeapon(Fight, TEXT("Chain"));

		if (!TestNotNull(TEXT("There is a club on the rack"), Club) || !TestNotNull(TEXT("There is a chain on the rack"), Chain))
		{
			return;
		}

		const FGutAttackDef& Swing = Club->LightAttacks[0];

		// out of the club's reach, step and all
		PlaceAtGap(Fight, Swing.Reach + Swing.LungeDistance + 15.0f);

		TestTrue(TEXT("That is inside the chain's reach"), Swing.Reach + Swing.LungeDistance + 15.0f < Chain->LightAttacks[0].Reach);

		Fight.Player->EquipWeapon(Chain);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth());

		Notes->Health = Fight.Enemy->GetMaxHealth() - Chain->LightAttacks[0].Damage;
		Notes->Limb = Fight.Enemy->GetMaxLimbHealth() - (Chain->LightAttacks[0].Damage * Chain->LightAttacks[0].LimbDamageScale);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	// as the lash lands
	Steps.Add({ 0.42f, [](const FFight& Fight) { Screenshot(TEXT("16_chain_reach")); } });

	Steps.Add({ 0.8f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The chain lands from outside the club's reach"), Fight.Enemy->GetHealth(), Notes->Health);
		TestEqual(TEXT("The chain's opening lash lands on a leg"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftLeg), Notes->Limb);
	} });

	// --- the machete's cuts do more to a limb than to the body, and two on the same arm cripple it ---

	Steps.Add({ 0.5f, [this, Notes, HealLimbs](const FFight& Fight)
	{
		HealLimbs(Fight.Enemy);

		UGutWeaponData* Machete = FindRackWeapon(Fight, TEXT("Machete"));

		if (!TestNotNull(TEXT("There is a machete on the rack"), Machete))
		{
			return;
		}

		Fight.Player->EquipWeapon(Machete);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth());

		PlaceFaceToFace(Fight);

		const FGutAttackDef& Cut = Machete->LightAttacks[0];

		Notes->Limb = Fight.Enemy->GetMaxLimbHealth() - (Cut.Damage * Cut.LimbDamageScale);

		TestTrue(TEXT("A cut does more to a limb than to the body"), Cut.LimbDamageScale > 1.0f);
		TestTrue(TEXT("Two cuts are enough to cripple an arm"), Cut.Damage * Cut.LimbDamageScale * 2.0f >= Fight.Enemy->GetMaxLimbHealth());
		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The machete's opening cut lands on the left arm"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm), Notes->Limb);
		TestFalse(TEXT("One cut does not cripple it"), Fight.Enemy->IsLimbCrippled(EGutLimb::LeftArm));

		PlaceFaceToFace(Fight);

		// after a pause the chain starts again from its opening cut
		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("A second cut cripples the arm"), Fight.Enemy->IsLimbCrippled(EGutLimb::LeftArm));
		TestTrue(TEXT("The other arm still holds the weapon"), Fight.Enemy->HasWeapon());
	} });

	// --- the shield guards best and the chain worst ---

	Steps.Add({ 1.0f, [this, Notes, HealLimbs](const FFight& Fight)
	{
		UGutWeaponData* Club = FindRackWeapon(Fight, TEXT("Club"));
		UGutWeaponData* ShieldWeapon = FindRackWeapon(Fight, TEXT("Shield"));
		UGutWeaponData* Chain = FindRackWeapon(Fight, TEXT("Chain"));

		if (!TestNotNull(TEXT("There is a shield on the rack"), ShieldWeapon) || !TestNotNull(TEXT("There is a chain on the rack"), Chain))
		{
			return;
		}

		TestTrue(TEXT("The shield guards better than anything else on the rack"), ShieldWeapon->BlockCostScale < 1.0f);
		TestTrue(TEXT("The chain guards worse than anything else on the rack"), Chain->BlockCostScale > 1.0f);

		// the enemy gets its arm back, and the player swings a club at a raised shield
		HealLimbs(Fight.Enemy);

		TestTrue(TEXT("The player takes the club"), Fight.Player->EquipWeapon(Club));
		TestTrue(TEXT("The enemy takes the shield"), Fight.Enemy->EquipWeapon(ShieldWeapon));

		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
		Fight.Enemy->SetBlocking(true);

		Notes->Stamina = Fight.Enemy->GetMaxStamina() - (Fight.Player->GetNextLightAttack()->BlockStaminaDamage * ShieldWeapon->BlockCostScale);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.45f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The attack is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("Blocking behind the shield costs less"), Fight.Enemy->GetStamina(), Notes->Stamina);

		Screenshot(TEXT("17_shield_block"));

		Fight.Enemy->SetBlocking(false);
	} });

	// the same swing against a raised chain
	Steps.Add({ 1.5f, [this, Notes](const FFight& Fight)
	{
		UGutWeaponData* Chain = FindRackWeapon(Fight, TEXT("Chain"));

		TestTrue(TEXT("The enemy takes the chain"), Fight.Enemy->EquipWeapon(Chain));

		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());
		Fight.Enemy->SetBlocking(true);

		Notes->Stamina = Fight.Enemy->GetMaxStamina() - (Fight.Player->GetNextLightAttack()->BlockStaminaDamage * Chain->BlockCostScale);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.45f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("The attack is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		TestEqual(TEXT("Blocking behind the chain costs more"), Fight.Enemy->GetStamina(), Notes->Stamina);

		Fight.Enemy->SetBlocking(false);
	} });

	// the shield is carried on the other arm, and takes most of what is aimed at it
	Steps.Add({ 1.5f, [this, Notes](const FFight& Fight)
	{
		UGutWeaponData* ShieldWeapon = FindRackWeapon(Fight, TEXT("Shield"));

		TestTrue(TEXT("The enemy takes the shield"), Fight.Enemy->EquipWeapon(ShieldWeapon));
		TestEqual(TEXT("The shield is carried on the left arm"), Fight.Enemy->GetArmFor(ShieldWeapon), EGutLimb::LeftArm);

		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		// the club's opening swing lands on the left arm
		const FGutAttackDef* Swing = Fight.Player->GetNextLightAttack();

		TestEqual(TEXT("The club's opening swing lands on the left arm"), Swing->HitZone, EGutHitZone::LeftArm);
		TestTrue(TEXT("The shield shelters its arm"), ShieldWeapon->HeldArmDamageScale < 1.0f);

		Notes->Limb = Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm) - (Swing->Damage * Swing->LimbDamageScale * ShieldWeapon->HeldArmDamageScale);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this, Notes](const FFight& Fight)
	{
		UGutWeaponData* ShieldWeapon = FindRackWeapon(Fight, TEXT("Shield"));

		TestEqual(TEXT("The attack lands"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Hit);
		TestEqual(TEXT("The arm behind the shield takes a share of the limb damage"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm), Notes->Limb, 0.01f);

		Fight.Enemy->SetLimbHealth(EGutLimb::RightArm, 0.0f);

		TestTrue(TEXT("A crippled right arm does not drop the shield"), Fight.Enemy->HasWeapon());

		Fight.Enemy->SetLimbHealth(EGutLimb::LeftArm, 0.0f);

		TestFalse(TEXT("A crippled left arm does"), Fight.Enemy->HasWeapon());
		TestFalse(TEXT("And cannot pick it up again"), Fight.Enemy->EquipWeapon(ShieldWeapon));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
