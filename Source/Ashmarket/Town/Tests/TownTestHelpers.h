// Ashmarket: the town outside the cage.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "Tests/AutomationCommon.h"
#include "Tests/GutTestHelpers.h"
#include "TownGameMode.h"

/** What the tests of every place in the town share: finding the place and the player in it, and running a script of steps */
namespace TownTests
{
	inline const TCHAR* MapPath = TEXT("/Game/Town/Lvl_PoolStreet");

	inline ATownGameMode* GetTown()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();

		return World ? World->GetAuthGameMode<ATownGameMode>() : nullptr;
	}

	inline UGutGameInstance* GetRun()
	{
		const UWorld* World = AutomationCommon::GetAnyGameWorld();

		return World ? Cast<UGutGameInstance>(World->GetGameInstance()) : nullptr;
	}

	/** True once the street is open with everybody stood in it */
	inline bool IsStreetReady()
	{
		const ATownGameMode* Town = GetTown();

		return Town && Town->IsSettled() && Town->GetWalker();
	}

	/** Stands the player at a place in the level, on the ground it is already on, facing along a yaw */
	inline void StandAt(const FVector2D& Where, float Yaw = 0.0f)
	{
		if (AGutFighter* Walker = GetTown() ? GetTown()->GetWalker() : nullptr)
		{
			Walker->SetActorLocationAndRotation(FVector(Where.X, Where.Y, Walker->GetActorLocation().Z), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	/** Stands the player a little way off something at a place, on the side the street is on */
	inline void StandBy(const FVector& Thing, const FVector2D& Off)
	{
		StandAt(FVector2D(Thing.X + Off.X, Thing.Y + Off.Y), FVector(-Off.X, -Off.Y, 0.0f).Rotation().Yaw);
	}

	/**
	 *  Steps run one after another. Each waits for its own condition (if it has one) and then for its delay, in the game's
	 *  own time, so the script keeps step with the game however fast the machine is and whichever level is open.
	 */
	class FScript : public IAutomationLatentCommand
	{
	public:

		struct FStep
		{
			float Delay = 0.0f;
			TFunction<void()> Do;

			/** Waited for before the delay starts. Run every frame until it is true, so it can also do something each frame. */
			TFunction<bool()> Until;
		};

		FScript(FAutomationTestBase* InTest, TArray<FStep> InSteps)
			: Test(InTest), Steps(MoveTemp(InSteps))
		{
		}

		virtual bool Update() override
		{
			if (!Steps.IsValidIndex(Index))
			{
				return true;
			}

			const UWorld* World = AutomationCommon::GetAnyGameWorld();

			if (!World || !World->HasBegunPlay())
			{
				return GiveUpIfLate(TEXT("no level was open"));
			}

			FStep& Step = Steps[Index];

			if (Step.Until && !bConditionMet)
			{
				if (!Step.Until())
				{
					return GiveUpIfLate(TEXT("what it was waiting for never happened"));
				}

				bConditionMet = true;
			}

			Waited += World->GetDeltaSeconds();

			if (Waited < Step.Delay)
			{
				return false;
			}

			if (Step.Do)
			{
				Step.Do();
			}

			++Index;
			Waited = 0.0f;
			bConditionMet = false;
			StepStarted = FPlatformTime::Seconds();

			return !Steps.IsValidIndex(Index);
		}

	private:

		/** A step that has not got anywhere in a minute of the clock on the wall fails the test and ends the script */
		bool GiveUpIfLate(const TCHAR* Why)
		{
			if (FPlatformTime::Seconds() - StepStarted < 60.0)
			{
				return false;
			}

			Test->AddError(FString::Printf(TEXT("Step %d of the script was given up on: %s"), Index + 1, Why));

			return true;
		}

		FAutomationTestBase* Test = nullptr;
		TArray<FStep> Steps;
		int32 Index = 0;
		float Waited = 0.0f;
		bool bConditionMet = false;
		double StepStarted = FPlatformTime::Seconds();
	};

	inline bool OpenStreet(FAutomationTestBase* Test)
	{
		if (!AutomationOpenMap(MapPath, true))
		{
			Test->AddError(TEXT("Could not open the street's level"));
			return false;
		}

		return true;
	}
}

#endif
