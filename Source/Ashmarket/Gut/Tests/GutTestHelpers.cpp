// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"
#include "GutGameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GutBrainController.h"
#include "GutHazard.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace GutTests
{
	const TCHAR* MapPath = TEXT("/Game/Gut/Lvl_Gut");

	FFight GetFight()
	{
		FFight Fight;

		if (UWorld* World = AutomationCommon::GetAnyGameWorld())
		{
			Fight.GameMode = World->GetAuthGameMode<AGutGameMode>();

			if (Fight.GameMode)
			{
				Fight.Player = Fight.GameMode->GetPlayerFighter();
				Fight.Enemy = Fight.GameMode->GetEnemyFighter();
			}
		}

		return Fight;
	}

	bool IsFightOver(const FFight& Fight)
	{
		return Fight.GameMode->GetPhase() == EGutFightPhase::PlayerWon || Fight.GameMode->GetPhase() == EGutFightPhase::PlayerLost;
	}

	void Place(AGutFighter* Fighter, float X, float Yaw)
	{
		Fighter->SetActorLocationAndRotation(FVector(X, 0.0f, FloorZ), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	void WalkToward(AGutFighter* Fighter, const AGutFighter* Target)
	{
		// the same room the opponent's brain gives a hazard
		const float Clearance = Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.0f;

		FVector Goal = Target->GetActorLocation();
		FVector Waypoint;

		if (AGutHazard::FindWayRound(Fighter, Fighter->GetActorLocation(), Goal, Clearance, Waypoint))
		{
			Goal = Waypoint;
		}

		Fighter->AddMovementInput((Goal - Fighter->GetActorLocation()).GetSafeNormal2D(), 1.0f);
	}

	void PlaceAt(AGutFighter* Fighter, float X, float Y, float Yaw)
	{
		Fighter->SetActorLocationAndRotation(FVector(X, Y, FloorZ), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	void PlaceFaceToFace(const FFight& Fight)
	{
		Place(Fight.Player, -70.0f, 0.0f);
		Place(Fight.Enemy, 70.0f, 180.0f);
	}

	void Screenshot(const FString& Name)
	{
		static TSet<FString> Taken;

		if (!FApp::CanEverRender() || Taken.Contains(Name))
		{
			return;
		}

		Taken.Add(Name);

		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("for-review/screenshots") / (Name + TEXT(".png")));

		FScreenshotRequest::RequestScreenshot(Path, true, false);
	}

	void WaitForNewFight(FAutomationTestBase* Test, TWeakObjectPtr<AGutGameMode> OldGameMode, bool bStayAtRack, bool bStayOnLadder)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[OldGameMode, bStayAtRack, bStayOnLadder]()
			{
				const FFight Fight = GetFight();

				if (!Fight.IsValid() || Fight.GameMode == OldGameMode.Get())
				{
					return false;
				}

				if (bStayOnLadder)
				{
					return true;
				}

				Fight.GameMode->EnterSandbox();

				if (!bStayAtRack)
				{
					Fight.GameMode->SkipRack();
				}

				return true;
			},
			[Test]() { Test->AddError(TEXT("The fighters never appeared in the cage")); return true; },
			30.0f));
	}

	UGutWeaponData* FindRackWeapon(const FFight& Fight, const FString& NamePart)
	{
		for (UGutWeaponData* Weapon : Fight.GameMode->GetRackWeapons())
		{
			if (Weapon && Weapon->GetName().Contains(NamePart))
			{
				return Weapon;
			}
		}

		return nullptr;
	}

	void PlaceAtGap(const FFight& Fight, float Gap)
	{
		// a gap is measured to the surface of the target
		Place(Fight.Player, -150.0f, 0.0f);
		Place(Fight.Enemy, -150.0f + Gap + Fight.Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius(), 180.0f);
	}

	bool OpenCage(FAutomationTestBase* Test, bool bStayAtRack)
	{
		const TWeakObjectPtr<AGutGameMode> OldGameMode = GetFight().GameMode;

		// the cage's tests were written for a cage that only ever opens itself again: a test of the street that asked for
		// the street as a place, and did not put that back, does not get to send them there
		if (const UWorld* World = AutomationCommon::GetAnyGameWorld())
		{
			if (UGutGameInstance* GameInstance = Cast<UGutGameInstance>(World->GetGameInstance()))
			{
				GameInstance->bStreetForced.Reset();

				// and each test finds the cage as it was first found: nobody dead from the test before, and nobody new
				GameInstance->Roster = FGutRosterState();
			}
		}

		if (!AutomationOpenMap(MapPath, true))
		{
			Test->AddError(TEXT("Could not open the cage level"));
			return false;
		}

		WaitForNewFight(Test, OldGameMode, bStayAtRack);

		return true;
	}

	void DisableEnemyBrain(FAutomationTestBase* Test, const FFight& Fight)
	{
		if (AGutBrainController* Brain = Cast<AGutBrainController>(Fight.Enemy->GetController()))
		{
			Brain->SetBrainEnabled(false);
		}
		else
		{
			Test->AddError(TEXT("The enemy is not controlled by a Gut Brain Controller"));
		}
	}

	AController* GiveBrain(AGutFighter* Fighter, TSubclassOf<AController> BrainClass)
	{
		AController* OldController = Fighter->GetController();
		AController* NewController = Fighter->GetWorld()->SpawnActor<AController>(BrainClass);

		if (!NewController)
		{
			return nullptr;
		}

		if (OldController)
		{
			OldController->UnPossess();

			// an AI brain without a fighter has no further use. A player controller stays, it owns the camera and the HUD.
			if (OldController->IsA<AAIController>())
			{
				OldController->Destroy();
			}
		}

		NewController->Possess(Fighter);

		return NewController;
	}

	void SendKey(const FFight& Fight, const FKey& Key, bool bPressed)
	{
		APlayerController* PlayerController = Cast<APlayerController>(Fight.Player->GetController());

		// somebody watching from the wall drives nobody, and still has the keys
		if (!PlayerController)
		{
			PlayerController = Fight.GameMode->GetWorld()->GetFirstPlayerController();
		}

		if (PlayerController)
		{
			PlayerController->InputKey(FInputKeyEventArgs(nullptr, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), Key, bPressed ? IE_Pressed : IE_Released, 0));
		}
	}

	bool FScriptCommand::Update()
	{
		const FFight Fight = GetFight();

		if (!Fight.IsValid())
		{
			Test->AddError(TEXT("The fight disappeared while a test script was running"));
			return true;
		}

		const float Now = Fight.GameMode->GetWorld()->GetTimeSeconds();

		// the clock starts again when the level reloads
		if (StepStartTime < 0.0f || Now < StepStartTime)
		{
			StepStartTime = Now;
		}

		// one step per frame at most, so every step sees the result of the one before
		if (Index < Steps.Num() && Now - StepStartTime >= Steps[Index].Delay)
		{
			Steps[Index].Action(Fight);

			++Index;
			StepStartTime = Now;
		}

		return Index >= Steps.Num();
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
