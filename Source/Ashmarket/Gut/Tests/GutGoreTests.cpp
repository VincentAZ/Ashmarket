// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EngineUtils.h"
#include "GutCrowd.h"
#include "GutSound.h"
#include "GutGore.h"
#include "GutPlayerCharacter.h"
#include "Misc/App.h"

/**
 *  Tests for what the design document leaves until the fight works: blood, severed limbs and the crowd.
 *
 *  Ashmarket.Gut.Gore     blows that land leave blood on the floor and blocked ones do not; the dead lie in a pool;
 *                         a finisher that cuts takes off what its weapon says it does, and one that does not leaves the body whole
 *  Ashmarket.Gut.Crowd    the deck fills by the fighter's name, and the crowd cheers when it is given something to cheer
 */
namespace GutGoreTests
{
	using namespace GutTests;

	int32 CountBlood(const FFight& Fight)
	{
		const UGutGoreSubsystem* Gore = Fight.GameMode->GetWorld()->GetSubsystem<UGutGoreSubsystem>();

		return Gore ? Gore->GetNumBloodDecals() : -1;
	}

	int32 CountGibs(const FFight& Fight)
	{
		int32 Count = 0;

		for (TActorIterator<AGutGib> It(Fight.GameMode->GetWorld()); It; ++It)
		{
			++Count;
		}

		return Count;
	}

	/** Turns the player's camera the given way, for a screenshot. The locked camera is let go first, or it would turn straight back. */
	void SetCamera(const FFight& Fight, const FRotator& Rotation)
	{
		if (AGutPlayerCharacter* Player = Cast<AGutPlayerCharacter>(Fight.Player))
		{
			Player->SetLockedOn(false);
		}

		if (AController* Controller = Fight.Player->GetController())
		{
			Controller->SetControlRotation(Rotation);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutGoreTest, "Ashmarket.Gut.Gore", GutTests::Flags)

bool FGutGoreTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutGoreTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TSharedRef<int32> Blood = MakeShared<int32>(0);

	TArray<FStep> Steps;

	// --- blood where blows land ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		TestEqual(TEXT("The floor is clean at the bell"), CountBlood(Fight), 0);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this, Blood](const FFight& Fight)
	{
		*Blood = CountBlood(Fight);

		TestTrue(TEXT("A blow that lands leaves blood on the floor"), *Blood > 0);
		TestEqual(TEXT("And is one blow that drew blood"), Fight.Enemy->GetTimesBloodDrawn(), 1);

		// the same blow on a raised guard
		PlaceFaceToFace(Fight);

		Fight.Enemy->SetBlocking(true);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this, Blood](const FFight& Fight)
	{
		TestEqual(TEXT("The attack is blocked"), Fight.Enemy->GetLastHitResult(), EGutHitResult::Blocked);
		// not by what is on the floor: the drops the first blow threw are still coming down, and what it opened drips
		TestEqual(TEXT("A blocked blow draws no blood"), Fight.Enemy->GetTimesBloodDrawn(), 1);
		TestTrue(TEXT("And what is on the floor has not gone"), CountBlood(Fight) >= *Blood);

		Fight.Enemy->SetBlocking(false);

		// --- a finisher that does not cut leaves the body whole ---

		Fight.Enemy->SetHealth(1.0f);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy is staggered"), Fight.Enemy->IsStaggered());
		TestFalse(TEXT("Nothing can be cut off a fighter that is still alive"), Fight.Enemy->SeverLimb(FName("head"), FVector::ForwardVector));
		TestTrue(TEXT("The player finishes it with the club"), Fight.Player->TryFinisher(EGutFinisherStyle::Clean));
	} });

	Steps.Add({ 1.5f, [this, Blood](const FFight& Fight)
	{
		TestFalse(TEXT("The enemy is dead"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("The club leaves the body whole"), Fight.Enemy->GetNumSevered(), 0);
		TestEqual(TEXT("And nothing lying on the floor"), CountGibs(Fight), 0);
		TestTrue(TEXT("The dead lie in a pool"), CountBlood(Fight) > *Blood);

		SetCamera(Fight, FRotator(-35.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 3.0f, [](const FFight& Fight) { Screenshot(TEXT("35_blood")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	// --- a finisher that cuts: the machete's slow one, in a fresh cage ---

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() { OpenCage(this); return true; }));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		TArray<FStep> Cut;

		Cut.Add({ 0.3f, [this](const FFight& Fight)
		{
			UGutWeaponData* Machete = FindRackWeapon(Fight, TEXT("Machete"));

			if (!TestNotNull(TEXT("There is a machete on the rack"), Machete))
			{
				return;
			}

			TestTrue(TEXT("The machete's slow finisher cuts"), Machete->SlowFinisher.SeverBones.Num() > 0);
			TestEqual(TEXT("Its clean one does not"), Machete->CleanFinisher.SeverBones.Num(), 0);

			DisableEnemyBrain(this, Fight);

			Fight.GameMode->StartFight();

			Fight.Player->EquipWeapon(Machete);
			Fight.Enemy->SetHealth(1.0f);

			PlaceFaceToFace(Fight);

			TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
		} });

		Cut.Add({ 1.0f, [this](const FFight& Fight)
		{
			TestTrue(TEXT("The enemy is staggered"), Fight.Enemy->IsStaggered());
			TestTrue(TEXT("The player finishes it the slow way"), Fight.Player->TryFinisher(EGutFinisherStyle::Slow));
		} });

		Cut.Add({ 3.0f, [this](const FFight& Fight)
		{
			const UGutWeaponData* Machete = FindRackWeapon(Fight, TEXT("Machete"));

			if (!Machete)
			{
				return;
			}

			TestFalse(TEXT("The enemy is dead"), Fight.Enemy->IsAlive());
			TestEqual(TEXT("The body is cut through wherever the finisher says"), Fight.Enemy->GetNumSevered(), Machete->SlowFinisher.SeverBones.Num());

			for (const FName& Bone : Machete->SlowFinisher.SeverBones)
			{
				TestTrue(FString::Printf(TEXT("The body is cut through at %s"), *Bone.ToString()), Fight.Enemy->IsSevered(Bone));
			}

			TestEqual(TEXT("What came off is lying on the floor"), CountGibs(Fight), Machete->SlowFinisher.SeverBones.Num());
			TestFalse(TEXT("Nothing comes off twice"), Fight.Enemy->SeverLimb(Machete->SlowFinisher.SeverBones[0], FVector::ForwardVector));
			TestEqual(TEXT("The player is whole"), Fight.Player->GetNumSevered(), 0);

			SetCamera(Fight, FRotator(-35.0f, 0.0f, 0.0f));
		} });

		Cut.Add({ 2.5f, [](const FFight& Fight) { Screenshot(TEXT("36_dismembered")); } });

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Cut)));

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutCrowdTest, "Ashmarket.Gut.Crowd", GutTests::Flags)

bool FGutCrowdTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutGoreTests;
	using FStep = FScriptCommand::FStep;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutCrowd* Crowd = Fight.GameMode->GetCrowd();

		if (!TestNotNull(TEXT("There is a crowd on the deck"), Crowd))
		{
			return;
		}

		TestTrue(TEXT("Somebody comes to see a fighter nobody has heard of"), Crowd->GetAttendanceFor(0) > 0);
		TestTrue(TEXT("More come to see a fighter with a name"), Crowd->GetAttendanceFor(10) > Crowd->GetAttendanceFor(0));
		TestEqual(TEXT("The deck only holds so many"), Crowd->GetAttendanceFor(100000), Crowd->GetAttendanceFor(100001));

		// the crowd is only for show: with no renderer nobody comes
		const int32 Expected = FApp::CanEverRender() ? Crowd->GetAttendanceFor(0) : 0;

		TestEqual(TEXT("In a single fight the crowd is the one for a fighter with no name"), Crowd->GetNumSpectators(), Expected);
		TestEqual(TEXT("Nobody is cheering before the bell"), Crowd->GetNumCheering(), 0);

		Crowd->Populate(12);

		TestEqual(TEXT("The deck fills for a fighter with a name"), Crowd->GetNumSpectators(), FApp::CanEverRender() ? Crowd->GetAttendanceFor(12) : 0);

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();
	} });

	// look up at them, once the camera has locked on at the bell and can be let go again
	Steps.Add({ 0.3f, [](const FFight& Fight) { SetCamera(Fight, FRotator(24.0f, 25.0f, 0.0f)); } });

	Steps.Add({ 1.0f, [](const FFight& Fight) { Screenshot(TEXT("37_crowd")); } });

	// a death is what they came for
	Steps.Add({ 0.6f, [this](const FFight& Fight)
	{
		// while it lasts they yell at it, and somebody among them has had something to say by now
		if (const AGutCrowd* Crowd = Fight.GameMode->GetCrowd(); Crowd && Crowd->GetNumSpectators() > 0)
		{
			UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(Crowd);

			TestTrue(TEXT("The crowd yells at a fight that is going on"), Crowd->IsYelling());
			TestTrue(TEXT("And one of them has shouted something"), !Sounds || !Sounds->Has(FName("crowd_shout")) || Sounds->GetTimesPlayed(FName("crowd_shout")) > 0);
		}

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		if (const AGutCrowd* Crowd = Fight.GameMode->GetCrowd())
		{
			TestEqual(TEXT("The whole deck cheers a death"), Crowd->GetNumCheering(), Crowd->GetNumSpectators());
		}
	} });

	Steps.Add({ 0.6f, [](const FFight& Fight) { Screenshot(TEXT("38_crowd_cheers")); } });

	Steps.Add({ 4.0f, [this](const FFight& Fight)
	{
		if (const AGutCrowd* Crowd = Fight.GameMode->GetCrowd())
		{
			TestEqual(TEXT("And then goes back to watching"), Crowd->GetNumCheering(), 0);
			TestFalse(TEXT("With nothing left to yell at"), Crowd->IsYelling());
		}
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
