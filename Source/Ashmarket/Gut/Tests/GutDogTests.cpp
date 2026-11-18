// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "GutBeast.h"
#include "GutBrainController.h"
#include "GutDogs.h"
#include "GutGameInstance.h"
#include "GutHazard.h"
#include "GutLadderData.h"

/**
 *  The pit boss's dogs.
 *
 *  Ashmarket.Gut.Dogs    the card against the dogs: the fighter on the card becomes one and another comes in beside it, each
 *                        an animal's body with no collar, weapon or limbs; a blow that beats a dog kills it; a dog bites, and
 *                        finishes somebody it has beaten; killing them all wins. Then the dogs let in on a fight between two
 *                        people: on nobody's side, their deaths deciding nothing; the thing that lets them in, and its round;
 *                        the card on the ladder; and somebody on the ladder who brings a dog of its own.
 */
namespace GutDogTests
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	void RunThenWaitForNextFight(FAutomationTestBase* Test, TArray<FStep>& Steps)
	{
		TSharedRef<TWeakObjectPtr<AGutGameMode>> OldGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

		if (Steps.Num() > 0)
		{
			TFunction<void(const FFight&)> Last = MoveTemp(Steps.Last().Action);

			Steps.Last().Action = [OldGameMode, Last](const FFight& Fight)
			{
				*OldGameMode = Fight.GameMode;

				Last(Fight);
			};
		}

		ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(Test, MoveTemp(Steps)));

		ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand(
			[OldGameMode]() { const FFight Fight = GetFight(); return OldGameMode->IsExplicitlyNull() ? false : (Fight.IsValid() && Fight.GameMode != OldGameMode->Get()); },
			[Test]() { Test->AddError(TEXT("The cage was not loaded again for the next fight")); return true; },
			30.0f));

		Steps.Reset();
	}

	void StopEveryBrain(const FFight& Fight)
	{
		TArray<AGutFighter*> Everybody = { Fight.Player, Fight.Enemy };

		for (AGutFighter* Extra : Fight.GameMode->GetExtraFighters())
		{
			Everybody.Add(Extra);
		}

		for (AGutFighter* Fighter : Everybody)
		{
			if (AGutBrainController* Brain = Cast<AGutBrainController>(Fighter->GetController()))
			{
				Brain->SetBrainEnabled(false);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutDogsTest, "Ashmarket.Gut.Dogs", GutTests::Flags)

bool FGutDogsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutDogTests;

	// at the rack of a single fight
	if (!OpenCage(this, true))
	{
		return false;
	}

	TArray<FStep> Steps;

	// --- the card against the dogs ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const UGutBeastData* Dog = GameMode->GetDog();

		if (!TestNotNull(TEXT("The pit boss has dogs"), Dog))
		{
			return;
		}

		TestFalse(TEXT("The one on the card is a person until the card is taken"), Fight.Enemy->IsBeast());

		GameMode->BeginSpecial(EGutSpecialCard::Animal);

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestEqual(TEXT("One more dog is brought in beside the first"), Extras.Num(), GameMode->GetDogsOnCard() - 1) || Extras.Num() < 1)
		{
			return;
		}

		TestTrue(TEXT("What stood on the card is a dog now, and so is the other"), Fight.Enemy->IsBeast() && Extras[0]->IsBeast());
		TestEqual(TEXT("The player stands alone"), GameMode->CountLiving(0), 1);
		TestEqual(TEXT("Against the dogs"), GameMode->CountLiving(1), GameMode->GetDogsOnCard());
		TestTrue(TEXT("Each has an animal's body"), Fight.Enemy->GetBeastBody() && Fight.Enemy->GetBeastBody()->GetSkeletalMeshAsset() == Dog->Mesh && Extras[0]->GetBeastBody() != nullptr);
		TestFalse(TEXT("And no person's body under it"), Fight.Enemy->HasBody() || Extras[0]->HasBody());
		TestEqual(TEXT("A dog has a dog's health"), Fight.Enemy->GetMaxHealth(), Dog->Health, 0.01f);
		TestEqual(TEXT("So has the other"), Extras[0]->GetHealth(), Dog->Health, 0.01f);
		TestTrue(TEXT("It bites: that is its weapon"), Fight.Enemy->GetArmament() == Dog->Bite && Extras[0]->GetArmament() == Dog->Bite);
		TestFalse(TEXT("It wears no collar of the Gut's"), Fight.Enemy->IsCollared());
		TestTrue(TEXT("Each has a brain, and is after the player"), Cast<AGutBrainController>(Extras[0]->GetController()) && Extras[0]->GetOpponent() == Fight.Player && Fight.Enemy->GetOpponent() == Fight.Player);
		TestFalse(TEXT("A dog cannot be handed a weapon"), Fight.Enemy->EquipWeapon(FindRackWeapon(Fight, TEXT("Club"))));

		StopEveryBrain(Fight);

		GameMode->SkipRack();
		GameMode->StartFight();

		TestTrue(TEXT("The bell is for the dogs too"), Fight.Enemy->IsCombatEnabled() && Extras[0]->IsCombatEnabled());
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("58_against_the_dogs")); } });

	// a blow lands on a dog
	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		if (!Fight.Enemy->IsBeast())
		{
			return;
		}

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 95.0f, 0.0f, 180.0f);

		Fight.Player->SetOpponent(Fight.Enemy);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("The player swings at a dog"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		if (!Fight.Enemy->IsBeast())
		{
			return;
		}

		TestTrue(TEXT("The blow costs the dog health"), Fight.Enemy->GetHealth() < Fight.Enemy->GetMaxHealth());
		TestTrue(TEXT("And it is still alive"), Fight.Enemy->IsAlive());

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			TestFalse(TEXT("A dog has no limb that is crippled apart from the rest of it"), Fight.Enemy->IsLimbCrippled(static_cast<EGutLimb>(i)));
		}

		// the next one is the last
		Fight.Enemy->SetHealth(1.0f);

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 95.0f, 0.0f, 180.0f);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("The player swings again"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!Fight.Enemy->IsBeast() || Extras.Num() < 1)
		{
			return;
		}

		TestFalse(TEXT("A dog that is beaten is dead: it does not stand there to be finished"), Fight.Enemy->IsAlive());
		TestEqual(TEXT("One dog dead does not end it"), GameMode->GetPhase(), EGutFightPhase::Fighting);
		TestTrue(TEXT("The player turns to the other"), Fight.Player->GetOpponent() == Extras[0]);

		// the other dog bites
		PlaceAt(Fight.Player, 0.0f, 300.0f, 0.0f);
		PlaceAt(Extras[0], 90.0f, 300.0f, 180.0f);

		Extras[0]->SetStamina(Extras[0]->GetMaxStamina());

		TestTrue(TEXT("A dog bites"), Extras[0]->TryLightAttack());
	} });

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (Extras.Num() < 1)
		{
			return;
		}

		TestTrue(TEXT("The bite costs the player health"), Fight.Player->GetHealth() < Fight.Player->GetMaxHealth());

		// and the last dog dies: the card is won
		Extras[0]->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("With both dogs dead the card is won"), GameMode->GetPhase(), EGutFightPhase::PlayerWon);
	} });

	Steps.Add({ 1.5f, [this](const FFight& Fight) { Screenshot(TEXT("59_dogs_dead")); } });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- a dog finishes somebody it has beaten ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		GameMode->EnterSandbox();
		GameMode->BeginSpecial(EGutSpecialCard::Animal);

		StopEveryBrain(Fight);

		GameMode->SkipRack();
		GameMode->StartFight();

		if (!Fight.Enemy->IsBeast())
		{
			return;
		}

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 90.0f, 0.0f, 180.0f);

		Fight.Player->SetHealth(1.0f);
		Fight.Enemy->SetOpponent(Fight.Player);
		Fight.Enemy->SetStamina(Fight.Enemy->GetMaxStamina());

		TestTrue(TEXT("A dog bites somebody on their last legs"), Fight.Enemy->TryLightAttack());
	} });

	Steps.Add({ 1.5f, [this](const FFight& Fight)
	{
		if (!Fight.Enemy->IsBeast())
		{
			return;
		}

		TestTrue(TEXT("A person beaten by a dog reels as from any blow"), Fight.Player->IsStaggered());
		TestTrue(TEXT("And the dog can finish it"), Fight.Enemy->CanFinish(Fight.Player) && Fight.Enemy->TryFinisher(EGutFinisherStyle::Clean));
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("60_dog_finishes")); } });

	Steps.Add({ 5.0f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("It does"), Fight.Player->IsAlive());
		TestEqual(TEXT("And the player has lost"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
	} });

	Steps.Add({ 1.0f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the dogs let in on a fight between two people ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		GameMode->EnterSandbox();

		// the thing that lets them in is there, set for its round, and switched off
		const AGutDogs* Kennel = nullptr;

		for (const AGutHazard* Hazard : GameMode->GetHazards())
		{
			if (const AGutDogs* Found = Cast<AGutDogs>(Hazard))
			{
				Kennel = Found;
			}
		}

		if (TestNotNull(TEXT("The cage has a way to let the dogs in"), Kennel))
		{
			TestEqual(TEXT("In the round the pit boss has set"), Kennel->GetActivationRound(), GameMode->GetDogsRound());
			TestEqual(TEXT("Shut until then"), Kennel->GetHazardState(), EGutHazardState::Off);
		}

		GameMode->LooseDogs(2);

		TestFalse(TEXT("No dogs before the bell"), GameMode->AreDogsLoose());

		StopEveryBrain(Fight);

		GameMode->SkipRack();
		GameMode->StartFight();

		PlaceAt(Fight.Player, -100.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 100.0f, 0.0f, 180.0f);

		GameMode->LooseDogs(2);

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestTrue(TEXT("Two dogs are let in"), GameMode->AreDogsLoose() && Extras.Num() == 2))
		{
			return;
		}

		TestTrue(TEXT("They are dogs"), Extras[0]->IsBeast() && Extras[1]->IsBeast());
		TestTrue(TEXT("On nobody's side"), Extras[0]->GetSide() == GutDogsSide && Extras[1]->GetSide() == GutDogsSide && Fight.Player->GetSide() == 0 && Fight.Enemy->GetSide() == 1);
		TestTrue(TEXT("Loose at once"), Extras[0]->IsCombatEnabled() && Extras[1]->IsCombatEnabled());
		TestTrue(TEXT("Each after one of the two people"), (Extras[0]->GetOpponent() == Fight.Player || Extras[0]->GetOpponent() == Fight.Enemy) && (Extras[1]->GetOpponent() == Fight.Player || Extras[1]->GetOpponent() == Fight.Enemy));
		TestTrue(TEXT("They came in away from the fighting"), FVector::Dist2D(Extras[0]->GetActorLocation(), Fight.Player->GetActorLocation()) > 350.0f);

		StopEveryBrain(Fight);
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("61_dogs_loose")); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (Extras.Num() != 2)
		{
			return;
		}

		Extras[0]->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("Killing a dog decides nothing"), GameMode->GetPhase(), EGutFightPhase::Fighting);
		TestFalse(TEXT("And settles nothing"), GameMode->GetSettlement().bSettled);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The other fighter dying is what wins it, with a dog still loose"), GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The dog that is left is alive"), Extras[1]->IsAlive());
		TestFalse(TEXT("And is called off"), Extras[1]->IsCombatEnabled());

		// --- on the ladder: to Dog rank's third fight by the short way ---

		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		GameInstance->bLadderStarted = true;
		GameInstance->Ladder = FGutLadderState();
		GameInstance->Ladder.Rank = 1;
		GameInstance->Ladder.Rung = 2;
		GameInstance->Ladder.Water = 50.0f;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;
		GameInstance->bSandbox = false;
	} });

	Steps.Add({ 2.0f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	TSharedRef<float> Purse = MakeShared<float>(0.0f);

	Steps.Add({ 0.3f, [this, Purse](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		if (!TestTrue(TEXT("On the ladder, at Dog rank's third fight"), GameMode->IsOnLadder() && GameMode->GetLadderState().Rank == 1 && GameMode->GetLadderState().Rung == 2))
		{
			return;
		}

		TestEqual(TEXT("The pit boss has a card against the dogs"), GameMode->GetSpecialOffer(), EGutSpecialCard::Animal);
		TestTrue(TEXT("It is a line on the card"), GameMode->GetCardItems().Contains(FGutCardItem{ EGutCardItemType::Special, 0 }));

		GameMode->SetSpecialAccepted(true);

		*Purse = GameMode->GetRung()->Purse;
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight) { Screenshot(TEXT("62_card_dogs")); } });

	Steps.Add({ 0.3f, [this, Purse](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		if (!GameMode->IsOnLadder())
		{
			return;
		}

		GameMode->ConfirmCard();

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestTrue(TEXT("The dogs are brought in when the card is left"), Fight.Enemy->IsBeast() && Extras.Num() == GameMode->GetDogsOnCard() - 1))
		{
			return;
		}

		StopEveryBrain(Fight);

		GameMode->ConfirmChoice();
		GameMode->ConfirmBet();
		GameMode->StartFight();

		TestTrue(TEXT("The one on the card is still a dog at the bell, with a dog's health and no weapon"), Fight.Enemy->IsBeast() && Fight.Enemy->GetMaxHealth() == GameMode->GetDog()->Health && Fight.Enemy->GetArmament() == GameMode->GetDog()->Bite);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		for (AGutFighter* Extra : Extras)
		{
			TestFalse(TEXT("The fight is not settled while a dog stands"), GameMode->GetSettlement().bSettled);

			Extra->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
		}

		TestTrue(TEXT("It is settled as a win when they are all down"), GameMode->GetSettlement().bSettled && GameMode->GetSettlement().bPlayerWon);
		TestEqual(TEXT("At the card's purse"), GameMode->GetSettlement().Purse, FMath::RoundToFloat(*Purse * GameMode->GetSpecialPurseScale(EGutSpecialCard::Animal)), 0.01f);
		TestEqual(TEXT("The ladder moves on"), GameMode->GetLadderState().Rung, 3);

		// --- next, somebody with a dog of its own: the last fight of Dog rank, by the short way ---

		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		GameInstance->Ladder.Rung = 4;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;
	} });

	Steps.Add({ 2.0f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		const FGutLadderRung* Rung = GameMode->GetRung();

		if (!TestTrue(TEXT("On the ladder, at the last fight of Dog rank"), GameMode->IsOnLadder() && GameMode->GetLadderState().Rank == 1 && GameMode->GetLadderState().Rung == 4 && Rung))
		{
			return;
		}

		TestTrue(TEXT("The one on the card has a dog of its own"), Rung->Dogs > 0);
		TestEqual(TEXT("No card is offered for it"), GameMode->GetSpecialOffer(), EGutSpecialCard::None);
		TestEqual(TEXT("Nobody is brought in before the card is left"), GameMode->GetExtraFighters().Num(), 0);

		GameMode->ConfirmCard();

		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!TestEqual(TEXT("Its dogs come in with it"), Extras.Num(), Rung->Dogs))
		{
			return;
		}

		TestTrue(TEXT("A dog, on its master's side"), Extras[0]->IsBeast() && Extras[0]->GetSide() == 1 && Fight.Enemy->GetSide() == 1 && !Fight.Enemy->IsBeast());
		TestEqual(TEXT("The master is who the card says, at the health the card's rung says"), Fight.Enemy->GetMaxHealth(), Rung->Health, 0.01f);

		StopEveryBrain(Fight);

		GameMode->ConfirmChoice();
		GameMode->ConfirmBet();
		GameMode->StartFight();
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight) { Screenshot(TEXT("68_kennel_master")); } });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const TArray<TObjectPtr<AGutFighter>>& Extras = GameMode->GetExtraFighters();

		if (!GameMode->IsOnLadder() || Extras.Num() < 1)
		{
			return;
		}

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestFalse(TEXT("With the master dead the fight is not over while its dog lives"), GameMode->GetSettlement().bSettled);
		TestTrue(TEXT("The player turns to the dog"), Fight.Player->GetOpponent() == Extras[0]);

		for (AGutFighter* Extra : Extras)
		{
			Extra->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
		}

		TestTrue(TEXT("With the dog dead too it is won, at the rung's own purse"), GameMode->GetSettlement().bSettled && GameMode->GetSettlement().bPlayerWon
			&& FMath::IsNearlyEqual(GameMode->GetSettlement().Purse, GameMode->GetRung() ? GameMode->GetRung()->Purse : -1.0f, 0.01f));
	} });

	Steps.Add({ 2.0f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

		GameInstance->Ladder.Rank = 0;
		GameInstance->Ladder.Rung = 2;

		TestEqual(TEXT("Meat is not put in against the dogs"), GameMode->GetSpecialOffer(), EGutSpecialCard::None);

		// back to single fights with nothing behind them, so that whatever test comes next finds the cage as it expects it
		GameInstance->Ladder = FGutLadderState();
		GameInstance->bLadderStarted = false;

		GameMode->EnterSandbox();
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
