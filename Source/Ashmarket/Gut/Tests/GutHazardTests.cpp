// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GutFlood.h"
#include "GutHazard.h"
#include "GutLiveCable.h"
#include "GutOpponentController.h"
#include "GutSpikeWall.h"
#include "Tests/AutomationCommon.h"

/**
 *  Tests for the cage's hazards, played in the real cage level with the spikes and the cable placed by the content script:
 *
 *  Ashmarket.Gut.Hazards           the round timer switches the hazards on; each hurts, stuns and throws; the cable can be dodged
 *                                  across; the cage kills outright and credits the fighter who drove the victim in
 *  Ashmarket.Gut.HazardDeath       hazards that are switched off stay off; the cable kills the player and loses the fight
 *  Ashmarket.Gut.HazardAvoidance   the opponent walks round the live cable to reach the player, and never touches it
 *  Ashmarket.Gut.CageFinish        one of the cage's own, beaten in front of the spikes by another, is knocked onto them
 *  Ashmarket.Gut.Flood             the valve opens before round 4 and the water rises; over the ankles it slows everybody,
 *                                  shortens a dodge and carries the cable's current; switched off, the pool is dry again
 *
 *  The fight clock is moved on by hand to reach the later rounds, rather than waiting a minute for them.
 */
namespace GutHazardTests
{
	using namespace GutTests;

	/** The spikes sit in the wall at this Y, the face of the long wall opposite the rack. They cover X from -400 to 400. */
	const float SpikeWallY = -500.0f;

	/** The cable runs along this Y, from X -630 to -30 */
	const float CableY = 250.0f;
	const float CableX = -300.0f;

	template<typename T>
	T* FindHazard(const FFight& Fight)
	{
		for (AGutHazard* Hazard : Fight.GameMode->GetHazards())
		{
			if (T* Typed = Cast<T>(Hazard))
			{
				return Typed;
			}
		}

		return nullptr;
	}

	/** Returns true if the level has both hazards, and flags an error if not */
	bool HasHazards(FAutomationTestBase* Test, const FFight& Fight)
	{
		if (FindHazard<AGutSpikeWall>(Fight) && FindHazard<AGutLiveCable>(Fight))
		{
			return true;
		}

		Test->AddError(TEXT("The cage level is missing its spikes or its cable. Run Tools/build_gut_content.sh."));
		return false;
	}

	/** Moves the fight clock on to the given time after the bell */
	void SetFightTime(const FFight& Fight, float Time)
	{
		Fight.GameMode->AdvanceFightTime(Time - Fight.GameMode->GetFightTime());
	}

	/** Points the player's camera at a spot on the floor, for a screenshot */
	void LookAt(const FFight& Fight, const FVector& Point)
	{
		if (AController* Controller = Fight.Player->GetController())
		{
			FVector ToPoint = Point - Fight.Player->GetActorLocation();
			ToPoint.Z = -150.0f;

			Controller->SetControlRotation(ToPoint.Rotation());
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutHazardsTest, "Ashmarket.Gut.Hazards", GutTests::Flags)

bool FGutHazardsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutHazardTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	// health the fighters are expected to have, worked out from the hazards' own numbers
	struct FExpected
	{
		float EnemyHealth = 0.0f;
		float PlayerHealth = 0.0f;
		float ThrownFromY = 0.0f;
	};

	TSharedRef<FExpected> Expected = MakeShared<FExpected>();
	TSharedRef<bool> bReady = MakeShared<bool>(false);

	// --- before the bell, and round 1: nothing is live ---

	Steps.Add({ 0.2f, [this, Expected, bReady](const FFight& Fight)
	{
		*bReady = HasHazards(this, Fight);

		if (!*bReady)
		{
			return;
		}

		TestEqual(TEXT("The spikes are off before the bell"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Off);
		TestEqual(TEXT("The cable is off before the bell"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Off);

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		TestEqual(TEXT("The fight starts in round 1"), Fight.GameMode->GetRound(), 1);
		TestEqual(TEXT("The spikes are off in round 1"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Off);

		Expected->EnemyHealth = Fight.Enemy->GetMaxHealth();
		Expected->PlayerHealth = Fight.Player->GetMaxHealth();

		PlaceAt(Fight.Player, -300.0f, -100.0f, 0.0f);
	} });

	// --- the spikes warn at the end of round 1, and the warning does not hurt ---

	Steps.Add({ 0.1f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(2) - 2.0f);

		TestEqual(TEXT("Still round 1"), Fight.GameMode->GetRound(), 1);
		TestEqual(TEXT("The spikes warn before their round"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Warning);
		TestEqual(TEXT("The cable is still off"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Off);

		// right up against the spikes
		PlaceAt(Fight.Enemy, 100.0f, SpikeWallY + 75.0f, 90.0f);

		TestTrue(TEXT("The enemy is touching the spikes"), FindHazard<AGutSpikeWall>(Fight)->IsTouching(Fight.Enemy));
	} });

	Steps.Add({ 0.3f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("Spikes that are only warning do not hurt"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);

		// on into round 2
		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(2) + 0.5f);

		TestEqual(TEXT("Round 2"), Fight.GameMode->GetRound(), 2);
		TestEqual(TEXT("The spikes are live in round 2"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Live);
		TestEqual(TEXT("The cable is still off in round 2"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Off);

		Expected->ThrownFromY = Fight.Enemy->GetActorLocation().Y;
	} });

	// --- the live spikes hurt, stun and throw ---

	Steps.Add({ 0.1f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		const AGutSpikeWall* Spikes = FindHazard<AGutSpikeWall>(Fight);

		Expected->EnemyHealth -= Spikes->GetDamage();

		TestEqual(TEXT("The spikes hurt a fighter that touches them"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
		TestEqual(TEXT("The hurt is counted"), Fight.Enemy->GetHazardHits(), 1);
		TestEqual(TEXT("The spikes stun"), Fight.Enemy->GetFighterState(), EGutFighterState::Stunned);
		TestEqual(TEXT("The player, nowhere near them, is not hurt"), Fight.Player->GetHealth(), Expected->PlayerHealth);

		LookAt(Fight, FVector(100.0f, SpikeWallY, 0.0f));
	} });

	Steps.Add({ 0.4f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestTrue(TEXT("The spikes throw the fighter off the wall"), Fight.Enemy->GetActorLocation().Y > Expected->ThrownFromY + 100.0f);
		TestFalse(TEXT("Thrown clear, it is no longer touching them"), FindHazard<AGutSpikeWall>(Fight)->IsTouching(Fight.Enemy));
		TestTrue(TEXT("The spikes are all the way out"), FindHazard<AGutSpikeWall>(Fight)->GetExtent() >= 1.0f);

		Screenshot(TEXT("18_spikes_live"));

		// straight back onto them, still inside the cooldown
		PlaceAt(Fight.Enemy, 100.0f, SpikeWallY + 75.0f, 90.0f);
	} });

	Steps.Add({ 0.2f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("The spikes do not hurt the same fighter twice in quick succession"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);

		// keep it there until the cooldown has run out
		PlaceAt(Fight.Enemy, 100.0f, SpikeWallY + 75.0f, 90.0f);
	} });

	Steps.Add({ 0.6f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		Expected->EnemyHealth -= FindHazard<AGutSpikeWall>(Fight)->GetDamage();

		TestEqual(TEXT("Once the cooldown has passed the spikes hurt again"), Fight.Enemy->GetHealth(), Expected->EnemyHealth);
		TestEqual(TEXT("Both hurts are counted"), Fight.Enemy->GetHazardHits(), 2);

		PlaceAt(Fight.Enemy, 300.0f, 0.0f, 180.0f);
	} });

	// --- the cable warns at the end of round 2; a dodge carries the player across it once it is live ---

	Steps.Add({ 1.0f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(3) - 2.0f);

		TestEqual(TEXT("The cable warns before its round"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Warning);
		TestEqual(TEXT("The spikes stay live"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Live);

		// on the cable while it is only warning
		PlaceAt(Fight.Player, CableX, CableY, -90.0f);

		TestTrue(TEXT("The player is touching the cable"), FindHazard<AGutLiveCable>(Fight)->IsTouching(Fight.Player));
	} });

	Steps.Add({ 0.3f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("A cable that is only warning does not hurt"), Fight.Player->GetHealth(), Expected->PlayerHealth);

		// step off it, just to the rack side, then switch it on
		PlaceAt(Fight.Player, CableX, CableY + 80.0f, -90.0f);

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(3) + 0.5f);

		TestEqual(TEXT("Round 3"), Fight.GameMode->GetRound(), 3);
		TestEqual(TEXT("The cable is live in round 3"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Live);
		TestFalse(TEXT("The player is clear of the cable"), FindHazard<AGutLiveCable>(Fight)->IsTouching(Fight.Player));
	} });

	Steps.Add({ 0.1f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("A fighter clear of the cable is not hurt"), Fight.Player->GetHealth(), Expected->PlayerHealth);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("The player dodges across the cable"), Fight.Player->TryDodge(FVector(0.0f, -1.0f, 0.0f)));
	} });

	Steps.Add({ 0.6f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestTrue(TEXT("The dodge took the player across the cable"), Fight.Player->GetActorLocation().Y < CableY - 60.0f);
		TestEqual(TEXT("A dodge across the live cable does not touch it"), Fight.Player->GetHealth(), Expected->PlayerHealth);
		TestEqual(TEXT("Nothing is counted"), Fight.Player->GetHazardHits(), 0);

		LookAt(Fight, FVector(CableX, CableY, 0.0f));

		// now stand on it
		PlaceAt(Fight.Player, CableX, CableY, -90.0f);

		Expected->ThrownFromY = CableY;
	} });

	Steps.Add({ 0.1f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		Expected->PlayerHealth -= FindHazard<AGutLiveCable>(Fight)->GetDamage();

		TestEqual(TEXT("The live cable hurts a fighter that touches it"), Fight.Player->GetHealth(), Expected->PlayerHealth);
		TestEqual(TEXT("The live cable stuns"), Fight.Player->GetFighterState(), EGutFighterState::Stunned);
		TestEqual(TEXT("The hurt is counted"), Fight.Player->GetHazardHits(), 1);
	} });

	Steps.Add({ 0.6f, [this, Expected, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestTrue(TEXT("The cable throws the fighter off it"), FMath::Abs(Fight.Player->GetActorLocation().Y - Expected->ThrownFromY) > 80.0f);
		TestEqual(TEXT("The cable's stun outlasts any blow's"), Fight.Player->GetFighterState(), EGutFighterState::Stunned);

		Screenshot(TEXT("19_cable_live"));
	} });

	// --- the cage kills outright, and the fighter who drove the victim into it gets the credit ---

	Steps.Add({ 1.0f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("The stun has worn off"), Fight.Player->GetFighterState(), EGutFighterState::Idle);

		// the enemy stands a step off the spikes, facing the player, with too little health left to take a blow and the spikes both
		PlaceAt(Fight.Enemy, 100.0f, SpikeWallY + 110.0f, 90.0f);
		PlaceAt(Fight.Player, 100.0f, SpikeWallY + 250.0f, -90.0f);

		Fight.Enemy->SetHealth(Fight.Player->GetNextLightAttack()->Damage + 10.0f);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestFalse(TEXT("The enemy is not yet touching the spikes"), FindHazard<AGutSpikeWall>(Fight)->IsTouching(Fight.Enemy));
		TestTrue(TEXT("The player swings"), Fight.Player->TryLightAttack());

		LookAt(Fight, FVector(100.0f, SpikeWallY, 0.0f));
	} });

	Steps.Add({ 0.8f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestFalse(TEXT("Knocked onto the spikes with little health left, the enemy dies"), Fight.Enemy->IsAlive());
		TestTrue(TEXT("The spikes killed it"), Fight.Enemy->GetKillingHazard() == FindHazard<AGutSpikeWall>(Fight));
		TestTrue(TEXT("The player, who knocked it there, is credited"), Fight.Enemy->GetKiller() == Fight.Player);
		TestEqual(TEXT("A death to the cage ends the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
	} });

	Steps.Add({ 0.5f, [](const FFight& Fight)
	{
		Screenshot(TEXT("20_cage_kill"));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutHazardDeathTest, "Ashmarket.Gut.HazardDeath", GutTests::Flags)

bool FGutHazardDeathTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutHazardTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;
	TSharedRef<bool> bReady = MakeShared<bool>(false);

	// --- switched off, the hazards stay off whatever the round ---

	Steps.Add({ 0.2f, [this, bReady](const FFight& Fight)
	{
		*bReady = HasHazards(this, Fight);

		if (!*bReady)
		{
			return;
		}

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();
		Fight.GameMode->SetHazardsEnabled(false);

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(3) + 5.0f);

		TestEqual(TEXT("Switched off, the spikes stay off in round 3"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Off);
		TestEqual(TEXT("Switched off, the cable stays off in round 3"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Off);

		PlaceAt(Fight.Player, CableX, CableY, 0.0f);
		PlaceAt(Fight.Enemy, 300.0f, 0.0f, 180.0f);
	} });

	Steps.Add({ 0.3f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestEqual(TEXT("A switched off cable does not hurt"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());

		// a fighter on its last legs, standing on the cable as it comes on
		Fight.Player->SetHealth(5.0f);
		Fight.GameMode->SetHazardsEnabled(true);

		TestEqual(TEXT("Switched back on, the cable is live at once in its round"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Live);

		LookAt(Fight, FVector(CableX + 200.0f, CableY, 0.0f));
	} });

	Steps.Add({ 0.3f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestFalse(TEXT("The cable kills the player outright"), Fight.Player->IsAlive());
		TestTrue(TEXT("The cable killed the player"), Fight.Player->GetKillingHazard() == FindHazard<AGutLiveCable>(Fight));
		TestTrue(TEXT("Nobody drove the player onto it, so nobody is credited"), Fight.Player->GetKiller() == nullptr);
		TestEqual(TEXT("The player's death to the cage loses the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
	} });

	Steps.Add({ 1.0f, [](const FFight& Fight)
	{
		Screenshot(TEXT("21_cable_kills_player"));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutCageFinishTest, "Ashmarket.Gut.CageFinish", GutTests::Flags)

bool FGutCageFinishTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutHazardTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;
	TSharedRef<bool> bReady = MakeShared<bool>(false);

	// two of the cage's own, one of them beaten two metres in front of the spikes while they are out
	Steps.Add({ 0.2f, [this, bReady](const FFight& Fight)
	{
		*bReady = HasHazards(this, Fight);

		AGutOpponentController* Brain = Cast<AGutOpponentController>(Fight.Enemy->GetController());

		if (!*bReady || !TestNotNull(TEXT("The enemy has the cage's brain"), Brain))
		{
			*bReady = false;
			return;
		}

		// the one who is to be beaten is the cage's too, and stands still: the cage is only let finish its own
		if (AGutBrainController* Still = Cast<AGutBrainController>(GiveBrain(Fight.Player, Brain->GetClass())))
		{
			Still->SetBrainEnabled(false);
		}

		// this one lets the cage do it whenever the cage can
		FGutOpponentTuning Tuning = Brain->GetTuning();

		Tuning.CageFinishChance = 1.0f;
		Tuning.MinFinishDelay = 0.1f;
		Tuning.MaxFinishDelay = 0.1f;

		Brain->SetTuning(Tuning);

		Fight.GameMode->StartFight();

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(2) + 1.0f);

		TestEqual(TEXT("The spikes are out"), FindHazard<AGutSpikeWall>(Fight)->GetHazardState(), EGutHazardState::Live);

		// its back to the spikes, two metres off them, and the other in front of it
		PlaceAt(Fight.Player, 0.0f, SpikeWallY + 245.0f, 90.0f);
		PlaceAt(Fight.Enemy, 0.0f, SpikeWallY + 420.0f, -90.0f);

		Fight.Player->SetHealth(1.0f);

		// a light blow is all it takes to beat it, and does not carry it far
		if (const FGutAttackDef* Blow = Fight.Enemy->GetNextLightAttack())
		{
			Fight.Player->ReceiveAttack(Fight.Enemy, *Blow);
		}

		TestTrue(TEXT("It is beaten and still standing"), Fight.Player->IsStaggered());
		TestFalse(TEXT("And is not on the spikes yet"), FindHazard<AGutSpikeWall>(Fight)->IsTouching(Fight.Player));
	} });

	Steps.Add({ 4.0f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestFalse(TEXT("The beaten fighter is dead"), Fight.Player->IsAlive());
		TestTrue(TEXT("It was sent reeling by a blow thrown to drive it"), Fight.Player->GetTimesDriven() >= 1);
		TestTrue(TEXT("The spikes killed it"), Fight.Player->GetKillingHazard() == FindHazard<AGutSpikeWall>(Fight));
		TestFalse(TEXT("And no finisher did"), Fight.Player->WasFinished());
		TestTrue(TEXT("Whoever drove it there is credited"), Fight.Player->GetKiller() == Fight.Enemy);
		TestTrue(TEXT("The one who did it is alive and unhurt by the spikes"), Fight.Enemy->IsAlive() && Fight.Enemy->GetHazardHits() == 0);
	} });

	Steps.Add({ 0.5f, [](const FFight& Fight)
	{
		Screenshot(TEXT("22_driven_onto_the_spikes"));
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutHazardAvoidanceTest, "Ashmarket.Gut.HazardAvoidance", GutTests::Flags)

bool FGutHazardAvoidanceTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutHazardTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;
	TSharedRef<bool> bReady = MakeShared<bool>(false);

	// the player stands still on the far side of the live cable. The straight way to it is across the cable.
	Steps.Add({ 0.2f, [this, bReady](const FFight& Fight)
	{
		*bReady = HasHazards(this, Fight);

		if (!*bReady)
		{
			return;
		}

		Fight.GameMode->StartFight();

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(3) + 1.0f);

		TestEqual(TEXT("The cable is live"), FindHazard<AGutLiveCable>(Fight)->GetHazardState(), EGutHazardState::Live);

		PlaceAt(Fight.Player, -400.0f, CableY + 170.0f, -90.0f);
		PlaceAt(Fight.Enemy, -400.0f, CableY - 190.0f, 90.0f);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	// it has to walk the length of the cable and back, and then it attacks when it chooses to. Give it time.
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
		[bReady]()
		{
			const FFight Fight = GetFight();

			return !*bReady || !Fight.IsValid() || IsFightOver(Fight) || Fight.Enemy->GetTally().Swings > 0;
		},
		[this]() { AddError(TEXT("The opponent did not get round the live cable and attack the player within 40 seconds")); return true; },
		40.0f));

	TArray<FStep> Verdict;

	Verdict.Add({ 0.0f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		TestTrue(TEXT("The opponent reaches the player and attacks"), Fight.Enemy->GetTally().Swings > 0);
		TestTrue(TEXT("It is on the player's side of the cable"), Fight.Enemy->GetActorLocation().Y > CableY);
		TestEqual(TEXT("The opponent never touched the live cable on the way"), Fight.Enemy->GetHazardHits(), 0);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Verdict)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFloodTest, "Ashmarket.Gut.Flood", GutTests::Flags)

bool FGutFloodTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutHazardTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;
	TSharedRef<bool> bReady = MakeShared<bool>(false);
	TSharedRef<float> DrySpeed = MakeShared<float>(0.0f);

	// a spot beside the cable: clear of it on dry tile, inside what its current reaches through water
	const FVector BesideCable(CableX, CableY + 90.0f, 0.0f);

	Steps.Add({ 0.2f, [this, bReady, DrySpeed, BesideCable](const FFight& Fight)
	{
		AGutFlood* Flood = FindHazard<AGutFlood>(Fight);
		AGutLiveCable* Cable = FindHazard<AGutLiveCable>(Fight);

		*bReady = HasHazards(this, Fight) && Flood;

		if (!Flood)
		{
			AddError(TEXT("The cage level has no flood. Run Tools/place_gut_flood.sh."));
		}

		if (!*bReady)
		{
			return;
		}

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		// the player out in the open, and the other one standing beside the cable, clear of it
		PlaceAt(Fight.Player, 300.0f, -200.0f, 0.0f);
		PlaceAt(Fight.Enemy, BesideCable.X, BesideCable.Y, 180.0f);

		*DrySpeed = Fight.Player->GetCharacterMovement()->MaxWalkSpeed;

		FVector Away;

		TestEqual(TEXT("The pool starts dry"), Flood->GetWaterLevel(), 0.0f);
		TestEqual(TEXT("And the valve shut"), Flood->GetHazardState(), EGutHazardState::Off);
		TestFalse(TEXT("Nobody wades on dry tile"), Fight.Player->IsWading() || Fight.Enemy->IsWading());
		TestTrue(TEXT("Beside the cable is clear of it on dry tile"), Cable->GetDangerDistance(BesideCable, Away) > Fight.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius());

		// the valve is opened a little before its round
		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(Flood->GetActivationRound()) - 0.5f);
	} });

	Steps.Add({ 0.3f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		AGutFlood* Flood = FindHazard<AGutFlood>(Fight);

		TestEqual(TEXT("The water is on its way before the round it is for"), Flood->GetHazardState(), EGutHazardState::Warning);
		TestTrue(TEXT("It is coming up"), Flood->GetWaterLevel() > 0.0f && Flood->GetWaterLevel() < Flood->GetDepth() * 0.5f);
		TestFalse(TEXT("Under the ankles nobody is slowed yet"), Fight.Player->IsWading());

		SetFightTime(Fight, Fight.GameMode->GetRoundStartTime(Flood->GetActivationRound()) + 0.1f);
	} });

	Steps.Add({ 5.0f, [this, bReady, DrySpeed, BesideCable](const FFight& Fight)
	{
		if (!*bReady) { return; }

		AGutFlood* Flood = FindHazard<AGutFlood>(Fight);
		AGutLiveCable* Cable = FindHazard<AGutLiveCable>(Fight);

		FVector Away;

		TestEqual(TEXT("The valve is open"), Flood->GetHazardState(), EGutHazardState::Live);
		TestTrue(TEXT("The water is over the ankles"), Flood->GetWaterLevel() >= Flood->GetDepth() * 0.5f);
		TestTrue(TEXT("Everybody wades"), Fight.Player->IsWading() && Fight.Enemy->IsWading());
		TestTrue(TEXT("And is slower for it"), Fight.Player->GetCharacterMovement()->MaxWalkSpeed < *DrySpeed * 0.9f);
		TestTrue(TEXT("The cable's current carries through the water"), Cable->IsLive() && Cable->GetDangerDistance(BesideCable, Away) < Fight.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius());
		TestEqual(TEXT("The water itself hurts nobody"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
		TestTrue(TEXT("Whoever stood where the tile was safe a minute ago is hurt by the cable without touching it"), Fight.Enemy->GetHealth() < Fight.Enemy->GetMaxHealth());

		LookAt(Fight, FVector(0.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 0.4f, [this, bReady](const FFight& Fight)
	{
		if (!*bReady) { return; }

		Screenshot(TEXT("57_flooded"));
	} });

	Steps.Add({ 4.0f, [this, bReady, DrySpeed, BesideCable](const FFight& Fight)
	{
		if (!*bReady) { return; }

		AGutFlood* Flood = FindHazard<AGutFlood>(Fight);
		AGutLiveCable* Cable = FindHazard<AGutLiveCable>(Fight);

		FVector Away;

		TestEqual(TEXT("The water stops at its depth"), Flood->GetWaterLevel(), Flood->GetDepth(), 0.01f);

		// the hazards switched off: the next fight starts in a dry pool
		Fight.GameMode->SetHazardsEnabled(false);

		TestEqual(TEXT("Switched off, the pool is dry"), Flood->GetWaterLevel(), 0.0f);
		TestFalse(TEXT("Nobody wades"), Fight.Player->IsWading() || Fight.Enemy->IsWading());
		TestEqual(TEXT("And everybody has their feet back"), Fight.Player->GetCharacterMovement()->MaxWalkSpeed, *DrySpeed, 0.5f);
		TestTrue(TEXT("The cable reaches no further than itself"), Cable->GetDangerDistance(BesideCable, Away) > Fight.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius());
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
