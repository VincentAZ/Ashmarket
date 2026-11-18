// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/PlayerController.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "GutWeaponData.h"

/**
 *  The Gut entered as an owner.
 *
 *  Ashmarket.Gut.Stable    days in the yard: the Block, buying and signing, training, dope and what it costs later, a fight won and
 *                          its purse, a fighter sent in hurt, a promise of a tag kept and one broken, a fighter that lies down,
 *                          and one that dies
 */
namespace GutStableTests
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	/** Runs the steps given so far, then waits for the cage to be loaded afresh */
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
			[Test]() { Test->AddError(TEXT("The cage was not loaded again for the next day in the yard")); return true; },
			30.0f));

		Steps.Reset();
	}

	/** The place in the stable of the first fighter that is owned, or signed */
	int32 FindFighter(const FGutStableState& State, bool bOwned)
	{
		return State.Fighters.IndexOfByPredicate([bOwned](const FGutStableFighter& Fighter) { return Fighter.bOwned == bOwned; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutStableTest, "Ashmarket.Gut.Stable", GutTests::Flags)

bool FGutStableTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutStableTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	struct FNotes
	{
		float Water = 0.0f;
		float ReturnPerLitre = 0.0f;
		int32 Against = -1;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// --- somebody comes to the cage as an owner ---

	Steps.Add({ 0.3f, [](const FFight& Fight) { Fight.GameMode->EnterStable(7); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutStableState& State = GameMode->GetStableState();

		TestTrue(TEXT("The cage is run for an owner"), GameMode->IsAtStable() && !GameMode->IsAtWindow() && !GameMode->IsOnLadder());
		TestEqual(TEXT("The day opens on the yard"), GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("It is the first day"), State.Day, 1);
		TestEqual(TEXT("The yard is empty"), State.Fighters.Num(), 0);
		TestTrue(TEXT("The owner drives nobody"), GameMode->GetWorld()->GetFirstPlayerController()->GetPawn() == nullptr);

		// with nobody in the yard the lines are the Block and letting the day go by
		TestEqual(TEXT("Two lines on an empty yard's card"), GameMode->GetCardItems().Num(), 2);

		TArray<FGutStableFighter> Offers;
		GameMode->GetBlockOffers(Offers);

		if (!TestEqual(TEXT("Three people are at the Block"), Offers.Num(), 3))
		{
			return;
		}

		TestTrue(TEXT("Two are for sale and have a price"), Offers[0].bOwned && Offers[1].bOwned && Offers[0].Price > 0.0f && Offers[1].Price > 0.0f);
		TestTrue(TEXT("The third has a tag, and signs for a wage"), !Offers[2].bOwned && Offers[2].Price == 0.0f && Offers[2].Wage > 0.0f);
		TestFalse(TEXT("Everybody at the Block has a name"), Offers[0].FighterName.IsEmpty() || Offers[1].FighterName.IsEmpty() || Offers[2].FighterName.IsEmpty());
		TestFalse(TEXT("And somewhere they came from"), Offers[0].Billing.IsEmpty() || Offers[2].Billing.IsEmpty());

		// through the card, as a player would: to the Block, and buy the first
		GameMode->ActivateCardSelection();

		TestEqual(TEXT("The first line of an empty yard leads to the Block"), GameMode->GetStablePage(), EGutStablePage::Block);

		Notes->Water = State.Water;

		GameMode->ActivateCardSelection();

		TestEqual(TEXT("The first at the Block is bought"), State.Fighters.Num(), 1);
		TestEqual(TEXT("And paid for"), State.Water, Notes->Water - Offers[0].Price, 0.01f);
		TestTrue(TEXT("It is owned"), State.Fighters[0].bOwned);

		Screenshot(TEXT("45_stable_block"));
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutStableState& State = GameMode->GetStableState();

		TArray<FGutStableFighter> Offers;
		GameMode->GetBlockOffers(Offers);

		// the one with a tag signs for nothing down
		TestTrue(TEXT("The one with a tag signs"), GameMode->BuyFighter(2));
		TestEqual(TEXT("For nothing down"), State.Water, Notes->Water - Offers[0].Price, 0.01f);
		TestFalse(TEXT("Nobody is bought twice"), GameMode->BuyFighter(2));
		TestEqual(TEXT("Two in the yard"), State.Fighters.Num(), 2);

		// back to the yard with the key that goes back
		GameMode->HandleMenuInput(EGutMenuInput::Left);

		TestEqual(TEXT("Back in the yard"), GameMode->GetStablePage(), EGutStablePage::Yard);
		TestEqual(TEXT("The yard now has a line for each fighter, the Block, the bill and the day"), GameMode->GetCardItems().Num(), 5);

		Screenshot(TEXT("46_stable_yard"));
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutStableState& State = GameMode->GetStableState();

		// --- training takes water and the day ---

		const int32 Owned = FindFighter(State, true);
		const int32 Strength = State.Fighters[Owned].Strength;
		const float Before = State.Water;
		const float Due = (GameMode->GetFeedPerDay() * 2.0f) + State.Fighters[FindFighter(State, false)].Wage;

		TestTrue(TEXT("The owned fighter is trained"), GameMode->TrainFighter(Owned, EGutTraining::Strength));
		TestEqual(TEXT("It is stronger"), State.Fighters[Owned].Strength, Strength + 1);
		TestEqual(TEXT("A day has gone by"), State.Day, 2);
		TestEqual(TEXT("Training, two mouths and a wage are paid"), State.Water, Before - GameMode->GetTrainingCost() - Due, 0.01f);

		// --- a promise eases what a collared fighter thinks of its owner ---

		const float Grudge = State.Fighters[Owned].Resentment;

		TestTrue(TEXT("It is promised its tag"), GameMode->PromiseTag(Owned));
		TestTrue(TEXT("And thinks a little better of the hand that feeds it"), State.Fighters[Owned].Resentment < Grudge);
		TestFalse(TEXT("Somebody with a tag cannot be promised one"), GameMode->PromiseTag(FindFighter(State, false)));
		TestFalse(TEXT("The tag is not due before it is earned"), GameMode->IsTagDue(Owned) || GameMode->GiveTag(Owned));

		// --- dope: an edge now ---

		TestTrue(TEXT("It is doped"), GameMode->DopeFighter(Owned));
		TestTrue(TEXT("Which starts a habit and is not forgotten"), State.Fighters[Owned].bDoped && State.Fighters[Owned].Habit > 0.0f && State.Fighters[Owned].Resentment > Grudge - 15.0f);

		// --- the bill ---

		TArray<int32> Against;
		GameMode->GetStableOffers(Against);

		if (!TestEqual(TEXT("Three fights are on offer"), Against.Num(), 3))
		{
			return;
		}

		Notes->Against = Against[0];
		Notes->Water = State.Water;

		const FGutLadderRung* Rung = GameMode->FindRung(Against[0]);

		TestTrue(TEXT("The owner's purse is more than the fighter's own"), Rung && GameMode->GetStablePurse(Against[0]) > Rung->Purse);

		TestTrue(TEXT("The fight is made"), GameMode->MakeMatch(Owned, Against[0]));
		TestEqual(TEXT("And goes to the betting"), GameMode->GetPhase(), EGutFightPhase::Betting);
		TestEqual(TEXT("The stable's fighter is in the first corner"), Fight.Player->GetDisplayName().ToString(), State.Fighters[Owned].FighterName.ToString());
		TestEqual(TEXT("The pit boss's is in the second"), Fight.Enemy->GetDisplayName().ToString(), Rung->OpponentName.ToString());
		TestTrue(TEXT("Both are fought by the cage's brain"), Cast<AGutOpponentController>(Fight.Player->GetController()) && Cast<AGutOpponentController>(Fight.Enemy->GetController()));
		TestTrue(TEXT("A trained, doped fighter hits harder than the attack says"), Fight.Player->GetDamageScale() > 1.0f);
		TestTrue(TEXT("An owned fighter wears a collar"), Fight.Player->IsCollared());

		Screenshot(TEXT("47_stable_betting"));
	} });

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;

		const float Step = GameMode->GetLadder()->BetStep;

		GameMode->SetBet(EGutBetSide::Player, Step);

		Notes->ReturnPerLitre = GameMode->GetReturnPerLitre(EGutBetSide::Player, Step);

		GameMode->ConfirmBet();
		GameMode->StartFight();

		// the stable's fighter loses an arm on the way to winning
		Fight.Player->SetLimbHealth(EGutLimb::LeftArm, 0.0f);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 0.5f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutStableState& State = GameMode->GetStableState();
		const FGutStableSettlement& Result = GameMode->GetStableSettlement();
		const float Step = GameMode->GetLadder()->BetStep;
		const int32 Owned = FindFighter(State, true);

		TestTrue(TEXT("The fight is settled as a win"), Result.bSettled && Result.bWon);
		TestEqual(TEXT("The whole purse is the owner's"), Result.FightersShare, 0.0f);
		TestEqual(TEXT("The purse is what was offered"), Result.Purse, GameMode->GetStablePurse(Notes->Against) - (0.05f * GameMode->FindRung(Notes->Against)->Purse), 1.0f);
		TestEqual(TEXT("The window pays the owner like anybody else"), Result.Return, Step * Notes->ReturnPerLitre, 0.01f);
		TestTrue(TEXT("The dead do not fight again"), State.Dead.Contains(Notes->Against));
		TestEqual(TEXT("The fighter has a win"), State.Fighters[Owned].Wins, 1);
		TestTrue(TEXT("And carries its arm out as it is"), State.Fighters[Owned].IsCrippled());
		TestFalse(TEXT("The dope has worn off"), State.Fighters[Owned].bDoped);
		TestEqual(TEXT("The stable has a name"), State.Name, 1);
		TestEqual(TEXT("The fight took the day"), State.Day, 3);

		Screenshot(TEXT("48_stable_won"));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- back in the yard: the cutter, a tag earned and given, and one broken ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();
		FGutStableState& State = GameInstance->Stable;

		TestEqual(TEXT("Back in the yard"), GameMode->GetPhase(), EGutFightPhase::Card);

		const int32 Owned = FindFighter(State, true);
		const float Grudge = State.Fighters[Owned].Resentment;

		TestTrue(TEXT("Doc Stitch sees to the arm"), GameMode->PatchFighter(Owned));
		TestFalse(TEXT("It is whole again"), State.Fighters[Owned].IsCrippled());
		TestTrue(TEXT("And that is remembered kindly"), State.Fighters[Owned].Resentment < Grudge);

		// the wins it was promised its tag for, as if it had fought them
		State.Fighters[Owned].Wins = State.Fighters[Owned].WinsAtPromise + GameMode->GetWinsForTag();
		State.Fighters[Owned].Resentment = 10.0f;
		State.Water = 200.0f;

		TestTrue(TEXT("The tag is due"), GameMode->IsTagDue(Owned));

		const FString Who = State.Fighters[Owned].FighterName.ToString();

		TestTrue(TEXT("It is bought and handed over"), GameMode->GiveTag(Owned));
		TestEqual(TEXT("At the Pump office's price"), State.Water, 200.0f - GameMode->GetTagPrice(), 0.01f);

		const int32 Freed = State.Fighters.IndexOfByPredicate([&Who](const FGutStableFighter& Fighter) { return Fighter.FighterName.ToString() == Who; });

		TestTrue(TEXT("Treated well enough, a freed fighter stays on for a wage"), Freed != INDEX_NONE && !State.Fighters[Freed].bOwned && State.Fighters[Freed].Wage > 0.0f);

		// --- a promise broken ---

		TArray<FGutStableFighter> Offers;
		GameMode->GetBlockOffers(Offers);

		TestTrue(TEXT("Two more are bought"), GameMode->BuyFighter(0) && GameMode->BuyFighter(1));

		const int32 First = State.Fighters.Num() - 2;
		const int32 Second = State.Fighters.Num() - 1;

		TestTrue(TEXT("One is promised its tag"), GameMode->PromiseTag(First));

		State.Fighters[First].Wins = State.Fighters[First].WinsAtPromise + GameMode->GetWinsForTag();

		const float OthersGrudge = State.Fighters[Second].Resentment;

		TestTrue(TEXT("And told there will be none"), GameMode->BreakPromise(First));
		TestTrue(TEXT("It does not forget"), State.Fighters[First].bBetrayed && State.Fighters[First].Resentment >= 50.0f);
		TestTrue(TEXT("Nor does anybody else in a collar"), State.Fighters[Second].Resentment > OthersGrudge);
		TestFalse(TEXT("It will not be promised anything again"), GameMode->PromiseTag(First));

		// --- sent in hating its owner, it lies down ---

		State.Fighters[First].Resentment = 90.0f;

		TArray<int32> Against;
		GameMode->GetStableOffers(Against);

		GameMode->SetNextStableRoll(0.0f);

		const int32 InTheYard = State.Fighters.Num();

		TestTrue(TEXT("It is sent in all the same"), GameMode->MakeMatch(First, Against[0]));

		const AGutOpponentController* Brain = Cast<AGutOpponentController>(Fight.Player->GetController());

		TestTrue(TEXT("It has no intention of fighting"), Brain && Brain->GetTuning().AttackChance < 0.1f && Brain->GetTuning().BlockLightChance == 0.0f);

		GameMode->ConfirmBet();
		GameMode->StartFight();

		Fight.Player->Kill(Fight.Enemy, FVector(1.0f, 0.0f, 0.0f));

		const FGutStableSettlement& Result = GameMode->GetStableSettlement();

		TestTrue(TEXT("It is dead, and it meant to be"), Result.bSettled && !Result.bWon && Result.bLayDown);
		TestEqual(TEXT("There is no purse"), Result.Purse, 0.0f);
		TestEqual(TEXT("And one fewer in the yard"), State.Fighters.Num(), InTheYard - 1);
		TestFalse(TEXT("Its opponent is still on the bills"), State.Dead.Contains(Against[0]));

		Screenshot(TEXT("49_stable_lay_down"));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the dead fighter is the owner's to see to ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		const FGutStableState& State = GameMode->GetStableState();

		TestTrue(TEXT("The dead fighter wore a collar, so its body is the owner's"), State.bBodyWaiting && !State.BodyOf.IsEmpty());
		TestEqual(TEXT("And nothing else is on the card until it is seen to"), GameMode->GetStablePage(), EGutStablePage::Body);
		TestEqual(TEXT("Three places it can go"), GameMode->GetCardItems().Num(), 3);
		TestTrue(TEXT("Doc Stitch and the grill carts pay, and the ground costs"), GameMode->GetBodyPrice(EGutBodyGoesTo::Parts) > 0.0f
			&& GameMode->GetBodyPrice(EGutBodyGoesTo::Grill) > 0.0f && GameMode->GetBodyPrice(EGutBodyGoesTo::Ground) < 0.0f);

		Screenshot(TEXT("53_stable_cold_room"));
	} });

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		FGutStableState& State = GameMode->GetGameInstance<UGutGameInstance>()->Stable;

		const int32 Owned = FindFighter(State, true);

		if (!TestTrue(TEXT("Somebody in a collar is left to see where it goes"), Owned != INDEX_NONE))
		{
			return;
		}

		// first the carts, on a copy of the day, to see what the yard makes of it
		const FGutStableState Before = State;

		TestTrue(TEXT("It can go to the grill carts"), GameMode->SendBodyTo(EGutBodyGoesTo::Grill));
		TestEqual(TEXT("Who pay for it"), State.Water, Before.Water + GameMode->GetBodyPrice(EGutBodyGoesTo::Grill), 0.01f);
		TestTrue(TEXT("And the yard does not forgive it"), State.Fighters[Owned].Resentment > Before.Fighters[Owned].Resentment);
		TestFalse(TEXT("It cannot be sold twice"), GameMode->SendBodyTo(EGutBodyGoesTo::Parts));

		State = Before;

		// then what the card's first line does: the ground
		GameMode->ActivateCardSelection();

		TestFalse(TEXT("It is seen to"), State.bBodyWaiting);
		TestEqual(TEXT("It is in the ground"), State.Buried, 1);
		TestEqual(TEXT("Which the owner paid for"), State.Water, Before.Water + GameMode->GetBodyPrice(EGutBodyGoesTo::Ground), 0.01f);
		TestTrue(TEXT("And the yard thinks the better of it"), State.Fighters[Owned].Resentment < Before.Fighters[Owned].Resentment);
		TestEqual(TEXT("The card is the yard's again"), GameMode->GetStablePage(), EGutStablePage::Yard);
	} });

	// --- hunger, and a fighter gone in the night ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		AGutGameMode* GameMode = Fight.GameMode;
		UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();
		FGutStableState& State = GameInstance->Stable;

		const int32 Owned = FindFighter(State, true);
		const int32 Signed = FindFighter(State, false);

		if (!TestTrue(TEXT("There is still somebody owned and somebody signed in the yard"), Owned != INDEX_NONE && Signed != INDEX_NONE))
		{
			return;
		}

		// nothing in hand: nobody eats and nobody is paid
		State.Water = 0.0f;
		State.Fighters[Owned].Resentment = 90.0f;

		const int32 InTheYard = State.Fighters.Num();
		const int32 SignedInTheYard = State.Fighters.FilterByPredicate([](const FGutStableFighter& Fighter) { return !Fighter.bOwned; }).Num();

		GameMode->RestDay();

		TestTrue(TEXT("What could not be paid is written down"), State.Debt > 0.0f);
		TestEqual(TEXT("Nobody with a tag stays unpaid"), State.Fighters.FilterByPredicate([](const FGutStableFighter& Fighter) { return !Fighter.bOwned; }).Num(), 0);
		TestTrue(TEXT("And the one that hated its owner most is gone in the night"), State.Fighters.Num() <= InTheYard - SignedInTheYard - 1);

		// back to single fights, so that whatever test comes next finds the cage as it expects it
		GameInstance->bStable = false;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
