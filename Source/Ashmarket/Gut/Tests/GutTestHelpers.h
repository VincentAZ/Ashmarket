// Ashmarket prototype: the Gut.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutFighter.h"
#include "GutGameMode.h"
#include "GutWeaponData.h"

class AController;
struct FKey;

/** Shared by every Gut test. All of them play in the real cage level. */
namespace GutTests
{
	extern const TCHAR* MapPath;

	inline constexpr EAutomationTestFlags Flags = EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Height the fighters stand at on the cage floor */
	inline constexpr float FloorZ = 92.0f;

	/** The running fight, looked up fresh every time because levels reload under the tests */
	struct FFight
	{
		AGutGameMode* GameMode = nullptr;
		AGutFighter* Player = nullptr;
		AGutFighter* Enemy = nullptr;

		bool IsValid() const { return GameMode && Player && Enemy; }
	};

	FFight GetFight();

	bool IsFightOver(const FFight& Fight);

	/** Puts a fighter on the cage floor at X, facing along the given yaw */
	void Place(AGutFighter* Fighter, float X, float Yaw);

	/**
	 *  Walks a scripted fighter toward its opponent for this frame. If a hazard that is live or about to be lies between them
	 *  it walks toward the way round instead, as the opponent's brain does: a scripted player is not meant to be blind.
	 */
	void WalkToward(AGutFighter* Fighter, const AGutFighter* Target);

	/** Puts a fighter on the cage floor at X and Y, facing along the given yaw */
	void PlaceAt(AGutFighter* Fighter, float X, float Y, float Yaw);

	/** Puts the two fighters face to face, close enough for any attack to reach */
	void PlaceFaceToFace(const FFight& Fight);

	/** Saves a screenshot for review. Does nothing when the tests run without a renderer. */
	void Screenshot(const FString& Name);

	/**
	 *  Waits until a fight other than the given one is running. Loading a level takes a frame or more to replace the old one.
	 *  Unless told to stay at the rack, the fighters then leave it with the weapons they start with, so every test that is not
	 *  about the rack fights with the same weapons every time.
	 *  Unless told to stay on the ladder, the cage is switched to single fights first: only the ladder tests want the ladder.
	 */
	void WaitForNewFight(FAutomationTestBase* Test, TWeakObjectPtr<AGutGameMode> OldGameMode, bool bStayAtRack = false, bool bStayOnLadder = false);

	/** Loads the cage and waits for both fighters to be in it. See WaitForNewFight for the rack and the ladder. */
	bool OpenCage(FAutomationTestBase* Test, bool bStayAtRack = false);

	/** Returns the weapon on the rack whose asset name contains the given text */
	UGutWeaponData* FindRackWeapon(const FFight& Fight, const FString& NamePart);

	/** Puts the two fighters face to face with the given gap between the player's center and the enemy's surface */
	void PlaceAtGap(const FFight& Fight, float Gap);

	/** Switches the enemy's brain off, for tests that drive both fighters by hand */
	void DisableEnemyBrain(FAutomationTestBase* Test, const FFight& Fight);

	/** Replaces whatever controls a fighter with a new controller of the given class, and returns it */
	AController* GiveBrain(AGutFighter* Fighter, TSubclassOf<AController> BrainClass);

	/** Presses or releases a key on the player's controller, as if it came from the keyboard or mouse */
	void SendKey(const FFight& Fight, const FKey& Key, bool bPressed);

	/**
	 *  Runs a list of steps one after another, each after its own delay.
	 *  Delays are measured in game time, so the script stays in step with the fight however fast the machine is.
	 */
	class FScriptCommand : public IAutomationLatentCommand
	{
	public:

		struct FStep
		{
			float Delay = 0.0f;
			TFunction<void(const FFight&)> Action;
		};

		FScriptCommand(FAutomationTestBase* InTest, TArray<FStep> InSteps)
			: Test(InTest)
			, Steps(MoveTemp(InSteps))
		{
		}

		virtual bool Update() override;

	private:

		FAutomationTestBase* Test;
		TArray<FStep> Steps;
		int32 Index = 0;
		float StepStartTime = -1.0f;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
