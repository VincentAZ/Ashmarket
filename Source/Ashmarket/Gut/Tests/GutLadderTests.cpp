// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutArmorData.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationCommon.h"

/**
 *  Tests for the ladder, played in the real cage level with the Meat arc the content script builds:
 *
 *  Ashmarket.Gut.Ladder       the arc from the first card to the gate by way of a death and being spared: the card, the rack,
 *                             the betting window, the purse and the payout, carried injuries, the cutter, fighting a lost fight again
 *  Ashmarket.Gut.LadderWon    the arc won outright, the bill at the gate, the sums behind the odds, and what a crippled arm
 *                             leaves the player able to draw at the rack
 *
 *  Ashmarket.Gut.Ranks       the ranks above Meat, the crowd's call for a finish and what it pays, the pit boss taking his debt
 *                             out of purses, the cut of the gate, and the run being kept on disk
 *  Ashmarket.Gut.Armor        what armor does to a blow, buying it on the card, opponents that wear it, and sponsors' offers
 *
 *  Fights are ended by hand rather than fought: these tests are about what happens around the fights.
 */
namespace GutLadderTests
{
	using namespace GutTests;
	using FStep = FScriptCommand::FStep;

	/** Taps a key: down now, up a moment later */
	void Tap(TArray<FStep>& Steps, float Delay, const FKey& Key)
	{
		Steps.Add({ Delay, [Key](const FFight& Fight) { SendKey(Fight, Key, true); } });
		Steps.Add({ 0.1f, [Key](const FFight& Fight) { SendKey(Fight, Key, false); } });
	}

	/** Runs the steps given so far, then waits for the cage to be loaded afresh with the ladder still running */
	void RunThenWaitForNextFight(FAutomationTestBase* Test, TArray<FStep>& Steps)
	{
		TSharedRef<TWeakObjectPtr<AGutGameMode>> OldGameMode = MakeShared<TWeakObjectPtr<AGutGameMode>>();

		// the last step given is the one that asks for the new cage, so note the old one just before it
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
			[Test]() { Test->AddError(TEXT("The cage was not loaded again for the next part of the ladder")); return true; },
			30.0f));

		Steps.Reset();
	}

	/** Takes the player from the card to the bell by the shortest way: no cutter, the first weapon it can hold, no bet */
	void WalkToBell(const FFight& Fight)
	{
		Fight.GameMode->ConfirmCard();
		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();
	}

	const UGutLadderData* GetLadder(const FFight& Fight)
	{
		return Fight.GameMode->GetLadder();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLadderTest, "Ashmarket.Gut.Ladder", GutTests::Flags)

bool FGutLadderTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLadderTests;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float ReturnPerLitre = 0.0f;
		float Water = 0.0f;
		float WaterAtRungStart = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	TArray<FStep> Steps;

	// --- the ladder starts at its first card ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestFalse(TEXT("Every other test runs the cage as single fights"), Fight.GameMode->IsOnLadder());
		TestNotNull(TEXT("The game mode has a ladder"), GetLadder(Fight));

		Fight.GameMode->StartNewLadder();
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		TestTrue(TEXT("The cage is running the ladder"), Fight.GameMode->IsOnLadder());
		TestEqual(TEXT("A ladder fight opens on its card"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("The Meat arc is three fights"), Ladder->Rungs.Num(), 3);
		TestEqual(TEXT("The player is on the first"), Fight.GameMode->GetLadderState().Rung, 0);
		TestEqual(TEXT("The player starts with the ladder's starting water"), Fight.GameMode->GetLadderState().Water, Ladder->StartingWater);
		TestFalse(TEXT("With nothing crippled the cutter has nothing to do"), Fight.GameMode->PatchLimb());
		TestFalse(TEXT("Nobody swings at the card"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.8f, [](const FFight& Fight) { Screenshot(TEXT("22_ladder_card")); } });

	// --- left mouse: to the rack, where the opponent has become this rung's ---

	Tap(Steps, 0.1f, EKeys::LeftMouseButton);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		const FGutLadderRung& Rung = GetLadder(Fight)->Rungs[0];

		TestEqual(TEXT("Left mouse leaves the card for the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestEqual(TEXT("The opponent has the rung's name"), Fight.Enemy->GetDisplayName().ToString(), Rung.OpponentName.ToString());
		TestEqual(TEXT("The opponent has the rung's health"), Fight.Enemy->GetHealth(), Rung.Health);
		TestTrue(TEXT("The opponent has drawn the rung's weapon"), Fight.Enemy->GetWeapon() == Rung.Weapon);
	} });

	// on a ladder the opponent's draw cannot be changed
	Tap(Steps, 0.1f, EKeys::W);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("W does not change the opponent's weapon on a ladder"), Fight.Enemy->GetWeapon() == GetLadder(Fight)->Rungs[0].Weapon);
	} });

	Tap(Steps, 0.1f, EKeys::D);
	Tap(Steps, 0.2f, EKeys::LeftMouseButton);

	// --- the betting window ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		TestEqual(TEXT("The betting window comes after the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Betting);
		TestTrue(TEXT("The player kept the weapon it drew"), Fight.Player->GetWeapon() == Fight.GameMode->GetRackWeapons()[1]);
		TestEqual(TEXT("No bet is down to begin with"), Fight.GameMode->GetBetSide(), EGutBetSide::None);
		TestEqual(TEXT("With no water the window takes what the slate allows"), Fight.GameMode->GetMaxStake(), Ladder->SlateLimit);

		TestEqual(TEXT("The book's figure is the ladder's for these weapons"), Fight.GameMode->GetBookChance(),
			Ladder->GetBookChance(0, Fight.Player->GetWeapon(), Fight.Enemy->GetWeapon(), 0));
	} });

	Tap(Steps, 0.1f, EKeys::D);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("D moves the bet onto the player"), Fight.GameMode->GetBetSide(), EGutBetSide::Player);
		TestEqual(TEXT("The first bet is the smallest"), Fight.GameMode->GetBetStake(), GetLadder(Fight)->BetStep);
	} });

	Tap(Steps, 0.1f, EKeys::W);

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		TestEqual(TEXT("W raises the stake a litre"), Fight.GameMode->GetBetStake(), Ladder->BetStep + 1.0f);

		// and twice the usual bet goes on, for what follows
		Fight.GameMode->SetBet(EGutBetSide::Player, Ladder->BetStep * 2.0f);

		TestTrue(TEXT("The odds move with the money: a bigger stake brings back less for each litre"),
			Fight.GameMode->GetReturnPerLitre(EGutBetSide::Player, Ladder->BetStep * 2.0f) < Fight.GameMode->GetReturnPerLitre(EGutBetSide::Player, Ladder->BetStep));

		TestTrue(TEXT("A litre on the side the book fancies less brings back more"),
			(Fight.GameMode->GetBookChance() < 0.5f) == (Fight.GameMode->GetReturnPerLitre(EGutBetSide::Player, 0.0f) > Fight.GameMode->GetReturnPerLitre(EGutBetSide::Opponent, 0.0f)));

		Notes->ReturnPerLitre = Fight.GameMode->GetReturnPerLitre(EGutBetSide::Player, Fight.GameMode->GetBetStake());

		Screenshot(TEXT("23_betting_window"));
	} });

	Tap(Steps, 0.3f, EKeys::LeftMouseButton);

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		TestEqual(TEXT("Left mouse places the bet and leaves for the intro"), Fight.GameMode->GetPhase(), EGutFightPhase::Intro);
		TestEqual(TEXT("The stake comes out of the player's water, onto the slate if need be"), Fight.GameMode->GetLadderState().Water, Ladder->StartingWater - (Ladder->BetStep * 2.0f));

		// --- the fight, cut short: the player takes two injuries and the opponent dies ---

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		Fight.Player->SetLimbHealth(EGutLimb::LeftArm, 0.0f);
		Fight.Player->SetLimbHealth(EGutLimb::RightLeg, 12.0f);

		Notes->Water = Fight.GameMode->GetLadderState().Water;

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();
		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		const float Stake = Ladder->BetStep * 2.0f;

		TestEqual(TEXT("The opponent's death wins the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The fight is settled"), Settlement.bSettled && Settlement.bPlayerWon);
		TestEqual(TEXT("The winner is paid the rung's purse"), Settlement.Purse, Ladder->Rungs[0].Purse);
		TestEqual(TEXT("The window pays the stake back at the odds it was placed at"), Settlement.BetReturn, Stake * Notes->ReturnPerLitre, 0.01f);
		TestEqual(TEXT("Purse and payout are in the player's hand"), State.Water, Notes->Water + Ladder->Rungs[0].Purse + (Stake * Notes->ReturnPerLitre), 0.01f);
		TestEqual(TEXT("The player has moved up a rung"), State.Rung, 1);
		TestFalse(TEXT("There are fights still to come"), State.bFinished);
		TestTrue(TEXT("The player's injuries are carried"), State.bInjuriesCarried);
		TestEqual(TEXT("The crippled arm is carried"), State.LimbHealth[static_cast<int32>(EGutLimb::LeftArm)], 0.0f);
		TestEqual(TEXT("The hurt leg is carried"), State.LimbHealth[static_cast<int32>(EGutLimb::RightLeg)], 12.0f);

		Notes->WaterAtRungStart = State.Water;
	} });

	Steps.Add({ 1.0f, [](const FFight& Fight) { Screenshot(TEXT("24_ladder_payout")); } });

	// --- R: on to the next fight, where the cutter is waiting ---

	Steps.Add({ 1.5f, [](const FFight& Fight) { SendKey(Fight, EKeys::R, true); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		SendKey(Fight, EKeys::R, false);

		TestEqual(TEXT("The next fight opens on its card"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("It is the second fight"), Fight.GameMode->GetLadderState().Rung, 1);
		TestFalse(TEXT("The injuries are not on the fighter until it leaves the card"), Fight.Player->IsLimbCrippled(EGutLimb::LeftArm));
	} });

	Steps.Add({ 0.8f, [](const FFight& Fight) { Screenshot(TEXT("25_ladder_card_injured")); } });

	// the card's lines are chosen with W and S. The cutter's is the one above the way out.
	Tap(Steps, 0.1f, EKeys::W);

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		TestTrue(TEXT("W moves the choice up to the cutter's line"), Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Patch);
		TestEqual(TEXT("Choosing a line does nothing by itself"), Fight.GameMode->GetLadderState().Water, Notes->WaterAtRungStart, 0.01f);
	} });

	Tap(Steps, 0.1f, EKeys::LeftMouseButton);

	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("Left mouse on the cutter's line pays the cutter"), State.Water, Notes->WaterAtRungStart - Ladder->PatchPrice, 0.01f);
		TestEqual(TEXT("The fighters are still at the card"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);
		TestTrue(TEXT("With nothing left to patch the choice is back on the way out"), Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Rack);
		TestEqual(TEXT("The patched limb comes back with a share of its health"), State.LimbHealth[static_cast<int32>(EGutLimb::LeftArm)], Fight.Player->GetMaxLimbHealth() * Ladder->PatchedLimbShare);
		TestEqual(TEXT("A limb that is hurt but working is left as it is"), State.LimbHealth[static_cast<int32>(EGutLimb::RightLeg)], 12.0f);
		TestFalse(TEXT("With nothing left crippled the cutter has nothing more to do"), Fight.GameMode->PatchLimb());
	} });

	Tap(Steps, 0.1f, EKeys::LeftMouseButton);

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderRung& Rung = Ladder->Rungs[1];

		TestEqual(TEXT("Left mouse leaves the card for the rack"), Fight.GameMode->GetPhase(), EGutFightPhase::Choosing);
		TestEqual(TEXT("The player's patched arm is as the cutter left it"), Fight.Player->GetLimbHealth(EGutLimb::LeftArm), Fight.Player->GetMaxLimbHealth() * Ladder->PatchedLimbShare);
		TestEqual(TEXT("The player's hurt leg is as the last fight left it"), Fight.Player->GetLimbHealth(EGutLimb::RightLeg), 12.0f);
		TestEqual(TEXT("The player's health is not carried: it starts each fight whole"), Fight.Player->GetHealth(), Fight.Player->GetMaxHealth());
		TestEqual(TEXT("The second opponent has its own name"), Fight.Enemy->GetDisplayName().ToString(), Rung.OpponentName.ToString());
		TestTrue(TEXT("The second opponent has its own weapon"), Fight.Enemy->GetWeapon() == Rung.Weapon);

		// --- a death is not the end: the fight is fought again, as the player came to it ---

		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		Fight.Player->Kill(Fight.Enemy, FVector(-1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The player's death loses the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestFalse(TEXT("A dead fighter is paid nothing"), Fight.GameMode->GetSettlement().Purse > 0.0f);
		TestEqual(TEXT("A dead fighter does not move up"), Fight.GameMode->GetLadderState().Rung, 1);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("After a death the same fight opens on its card again"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("It is still the second fight"), State.Rung, 1);
		TestEqual(TEXT("The player's water is what it came to the fight with"), State.Water, Notes->WaterAtRungStart, 0.01f);
		TestEqual(TEXT("The cutter's work is undone with it"), State.LimbHealth[static_cast<int32>(EGutLimb::LeftArm)], 0.0f);

		// win it this time
		WalkToBell(Fight);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("The player is on to the last fight"), Fight.GameMode->GetLadderState().Rung, 2);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the last opponent will not finish the player, and a bet against yourself pays ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderRung& Rung = Ladder->Rungs[2];

		TestTrue(TEXT("The last opponent of the Meat arc spares the player"), Rung.bSparesPlayer);

		Fight.GameMode->ConfirmCard();
		Fight.GameMode->ConfirmChoice();

		Fight.GameMode->SetBet(EGutBetSide::Opponent, Ladder->BetStep);

		Notes->ReturnPerLitre = Fight.GameMode->GetReturnPerLitre(EGutBetSide::Opponent, Ladder->BetStep);

		Fight.GameMode->ConfirmBet();

		Notes->Water = Fight.GameMode->GetLadderState().Water;

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		// one touch from beaten
		Fight.Player->SetHealth(1.0f);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("The opponent swings"), Fight.Enemy->TryLightAttack());
	} });

	Steps.Add({ 0.8f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();
		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		TestEqual(TEXT("A beaten player has lost"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestTrue(TEXT("But it is alive"), Fight.Player->IsAlive());
		TestTrue(TEXT("And is going down onto its knees, to stay there"), Fight.Player->HasYielded());
		TestTrue(TEXT("It was spared"), Settlement.bSettled && Settlement.bSpared && !Settlement.bPlayerWon);
		TestEqual(TEXT("There is no purse for losing"), Settlement.Purse, 0.0f);
		TestEqual(TEXT("The bet against itself pays"), Settlement.BetReturn, Ladder->BetStep * Notes->ReturnPerLitre, 0.01f);
		TestEqual(TEXT("The payout is in the player's hand"), State.Water, Notes->Water + (Ladder->BetStep * Notes->ReturnPerLitre), 0.01f);
		TestTrue(TEXT("The ladder is over"), State.bFinished && State.bSpared);

		Notes->Water = State.Water;
	} });

	// long enough for the beaten one to be down and the other to have let its hands drop
	Steps.Add({ 3.6f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("Somebody spared is still alive and still down a few seconds on, with no second wind"), Fight.Player->IsAlive() && Fight.Player->IsStaggered() && Fight.Player->HasYielded());

		Screenshot(TEXT("26_spared"));
	} });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the gate ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("After the last fight the cage opens on the gate"), Fight.GameMode->GetPhase(), EGutFightPhase::Gate);
		TestEqual(TEXT("The player's water is what it left the last fight with"), State.Water, Notes->Water, 0.01f);
		TestTrue(TEXT("Being spared adds to the bill"), Ladder->GetGateBill(State.Water, true) > Ladder->GetGateBill(State.Water, false) || Ladder->GetGateBill(State.Water, false) > Ladder->RoomAndBoard);
		TestTrue(TEXT("The player leaves owing at least the minimum"), Ladder->GetGateDebt(State.Water, true) >= Ladder->MinimumDebt);
		TestFalse(TEXT("Nobody swings at the gate"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 0.8f, [](const FFight& Fight) { Screenshot(TEXT("27_gate_spared")); } });

	// left mouse: the bill is settled and the player comes back as a Dog
	Steps.Add({ 0.2f, [this, Notes](const FFight& Fight)
	{
		Notes->ReturnPerLitre = GetLadder(Fight)->GetGateDebt(Fight.GameMode->GetLadderState().Water, true);

		SendKey(Fight, EKeys::LeftMouseButton, true);
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		SendKey(Fight, EKeys::LeftMouseButton, false);

		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("Left mouse at the gate goes on to the next rank's first card"), Fight.GameMode->GetPhase(), EGutFightPhase::Card);
		TestEqual(TEXT("The player is at the second rank"), State.Rank, 1);
		TestEqual(TEXT("On its first fight"), State.Rung, 0);
		TestEqual(TEXT("The player's water went against the bill"), State.Water, 0.0f);
		TestEqual(TEXT("What was left of the bill is owed"), State.Debt, Notes->ReturnPerLitre, 0.01f);
		TestTrue(TEXT("Injuries come along to the next rank"), State.bInjuriesCarried);
		TestFalse(TEXT("The new rank has fights to come"), State.bFinished);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLadderWonTest, "Ashmarket.Gut.LadderWon", GutTests::Flags)

bool FGutLadderWonTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLadderTests;

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	// --- the sums, on the ladder asset itself ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		if (!TestNotNull(TEXT("The game mode has a ladder"), Ladder))
		{
			return;
		}

		const UGutWeaponData* Club = FindRackWeapon(Fight, TEXT("Club"));
		const UGutWeaponData* Chain = FindRackWeapon(Fight, TEXT("Chain"));

		// the book
		const float Even = Ladder->GetBookChance(0, Club, Club, 0);

		TestEqual(TEXT("With the same weapon on both sides the book's figure is the rung's own"), Even, Ladder->Rungs[0].BookChance);
		TestTrue(TEXT("The book knows the chain beats the club"), Ladder->GetBookChance(0, Chain, Club, 0) > Even && Ladder->GetBookChance(0, Club, Chain, 0) < Even);
		TestTrue(TEXT("The book marks a crippled fighter down"), Ladder->GetBookChance(0, Club, Club, 1) < Even);
		TestTrue(TEXT("The book marks an unarmed fighter down"), Ladder->GetBookChance(0, nullptr, Club, 0) < Even);
		TestTrue(TEXT("The book thinks less of the player with each rung"), Ladder->Rungs[2].BookChance < Ladder->Rungs[0].BookChance);

		// the pot: everything staked, less the house's cut, shared among the winners
		const float Chance = 0.25f;
		const float Stake = 10.0f;
		const float Expected = ((Ladder->CrowdPool + Stake) * (1.0f - Ladder->HouseTake)) / ((Ladder->CrowdPool * Chance) + Stake);

		TestEqual(TEXT("A litre on the player brings back its share of the pot"), Ladder->GetReturnPerLitre(Chance, EGutBetSide::Player, Stake), Expected, 0.001f);
		TestTrue(TEXT("The long shot pays more than the favourite"), Ladder->GetReturnPerLitre(Chance, EGutBetSide::Player, 0.0f) > Ladder->GetReturnPerLitre(Chance, EGutBetSide::Opponent, 0.0f));
		TestTrue(TEXT("The house's cut means backing both sides loses"), (1.0f / Ladder->GetReturnPerLitre(Chance, EGutBetSide::Player, 0.0f)) + (1.0f / Ladder->GetReturnPerLitre(Chance, EGutBetSide::Opponent, 0.0f)) > 1.0f);
		TestEqual(TEXT("No bet brings back nothing"), Ladder->GetReturnPerLitre(Chance, EGutBetSide::None, Stake), 0.0f);

		// the bill
		TestEqual(TEXT("With nothing in hand the debt is the bill"), Ladder->GetGateDebt(0.0f, false), Ladder->RoomAndBoard);
		TestEqual(TEXT("Water in hand goes against the bill"), Ladder->GetGateDebt(30.0f, false), Ladder->RoomAndBoard - 30.0f);
		TestEqual(TEXT("Water owed on the slate is added to it"), Ladder->GetGateDebt(-15.0f, false), Ladder->RoomAndBoard + 15.0f);
		TestEqual(TEXT("However much the player holds it leaves owing the minimum"), Ladder->GetGateDebt(Ladder->RoomAndBoard * 5.0f, false), Ladder->MinimumDebt);
		TestEqual(TEXT("Being spared costs the penalty"), Ladder->GetGateDebt(0.0f, true), Ladder->RoomAndBoard + Ladder->SparedPenalty);

		Fight.GameMode->StartNewLadder();
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- first fight: won, at the cost of the weapon arm ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		WalkToBell(Fight);

		Fight.Player->SetLimbHealth(EGutLimb::RightArm, 0.0f);

		TestFalse(TEXT("A crippled weapon arm drops the weapon"), Fight.Player->HasWeapon());

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- second fight: with no right arm, only the shield can be drawn ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const UGutWeaponData* Shield = FindRackWeapon(Fight, TEXT("Shield"));
		const UGutWeaponData* Club = FindRackWeapon(Fight, TEXT("Club"));

		Fight.GameMode->ConfirmCard();

		TestTrue(TEXT("The crippled weapon arm is carried into the next fight"), Fight.Player->IsLimbCrippled(EGutLimb::RightArm));
		TestFalse(TEXT("The player cannot hold the club"), Fight.GameMode->CanPlayerHold(Club));
		TestTrue(TEXT("The player can still carry the shield, on the other arm"), Fight.GameMode->CanPlayerHold(Shield));
		TestTrue(TEXT("The rack starts the player on the one weapon it can hold"), Fight.Player->GetWeapon() == Shield);

		Fight.GameMode->CyclePlayerChoice(1);

		TestTrue(TEXT("Moving along the rack skips what the player cannot hold"), Fight.Player->GetWeapon() == Shield);

		Screenshot(TEXT("28_rack_one_arm"));
	} });

	Steps.Add({ 0.5f, [this](const FFight& Fight)
	{
		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		// and the other arm goes in this fight
		Fight.Player->SetLimbHealth(EGutLimb::LeftArm, 0.0f);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- last fight: with no arms the player comes to it bare handed, and wins it anyway ---

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);

		Fight.GameMode->ConfirmCard();

		TestFalse(TEXT("With both arms crippled the player holds nothing"), Fight.Player->HasWeapon());
		TestNotNull(TEXT("But it still has its fists"), Fight.Player->GetNextLightAttack());

		Fight.GameMode->ConfirmChoice();

		TestEqual(TEXT("The book marks down both arms and the empty hands"), Fight.GameMode->GetBookChance(),
			Ladder->GetBookChance(2, nullptr, Fight.Enemy->GetWeapon(), 2));

		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestTrue(TEXT("Three wins finish the ladder"), State.bFinished && !State.bSpared);
		TestEqual(TEXT("The three purses are in hand"), State.Water, Ladder->StartingWater + Ladder->Rungs[0].Purse + Ladder->Rungs[1].Purse + Ladder->Rungs[2].Purse);
		TestEqual(TEXT("And counted"), State.Purses, Ladder->Rungs[0].Purse + Ladder->Rungs[1].Purse + Ladder->Rungs[2].Purse);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const UGutLadderData* Ladder = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("The cage opens on the gate"), Fight.GameMode->GetPhase(), EGutFightPhase::Gate);
		TestEqual(TEXT("The player leaves owing the bill less its water"), Ladder->GetGateDebt(State.Water, false), FMath::Max(Ladder->MinimumDebt, Ladder->RoomAndBoard - State.Water));
	} });

	Steps.Add({ 0.8f, [](const FFight& Fight) { Screenshot(TEXT("29_gate_won")); } });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutRanksTest, "Ashmarket.Gut.Ranks", GutTests::Flags)

bool FGutRanksTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLadderTests;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float Debt = 0.0f;
		int32 Name = 0;
		EGutFinisherStyle Call = EGutFinisherStyle::Clean;
		FGutLadderState Saved;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	auto GetInstance = [](const FFight& Fight) { return Cast<UGutGameInstance>(Fight.GameMode->GetGameInstance()); };

	TArray<FStep> Steps;

	// --- the ladder is four ranks, each reached through the one before ---

	Steps.Add({ 0.2f, [this](const FFight& Fight)
	{
		Fight.GameMode->StartNewLadder();
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, GetInstance](const FFight& Fight)
	{
		const UGutLadderData* Meat = GetLadder(Fight);

		if (!TestNotNull(TEXT("The game mode has a ladder"), Meat))
		{
			return;
		}

		const UGutLadderData* Dog = Meat->NextLadder;
		const UGutLadderData* Butcher = Dog ? Dog->NextLadder.Get() : nullptr;
		const UGutLadderData* Champion = Butcher ? Butcher->NextLadder.Get() : nullptr;

		if (!TestNotNull(TEXT("Dog follows Meat"), Dog) || !TestNotNull(TEXT("Butcher follows Dog"), Butcher) || !TestNotNull(TEXT("Champion follows Butcher"), Champion))
		{
			return;
		}

		TestEqual(TEXT("Meat is three fights"), Meat->Rungs.Num(), 3);
		TestEqual(TEXT("Dog is five fights"), Dog->Rungs.Num(), 5);
		TestEqual(TEXT("Butcher is five fights"), Butcher->Rungs.Num(), 5);
		TestEqual(TEXT("Champion is one fight"), Champion->Rungs.Num(), 1);
		TestNull(TEXT("Nothing follows Champion"), Champion->NextLadder.Get());
		TestTrue(TEXT("Only Meat ends with a bill"), Meat->bBillAtGate && !Dog->bBillAtGate && !Butcher->bBillAtGate && !Champion->bBillAtGate);
		TestTrue(TEXT("After Meat the pit boss takes his debt out of the purses"), Meat->DebtShareOfPurse == 0.0f && Dog->DebtShareOfPurse > 0.0f);
		TestTrue(TEXT("The cut of the gate grows with the player's name"), Champion->GetGateCut(10) > Champion->GetGateCut(0));

		// to the Dog rank by the short way: at the Meat gate, three wins behind it
		UGutGameInstance* GameInstance = GetInstance(Fight);

		GameInstance->Ladder.Rung = 3;
		GameInstance->Ladder.Water = 30.0f;
		GameInstance->Ladder.Name = 3;
		GameInstance->Ladder.bFinished = true;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;

		Fight.GameMode->EnterSandbox();
		Fight.GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		GameInstance->bSandbox = false;
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		TestEqual(TEXT("With the last Meat fight behind it the cage opens on the gate"), Fight.GameMode->GetPhase(), EGutFightPhase::Gate);

		Notes->Debt = GetLadder(Fight)->GetGateDebt(30.0f, false);

		Fight.GameMode->LeaveGate();
	} });

	RunThenWaitForNextFight(this, Steps);

	// --- a Dog fight: the crowd calls for a finish, and the pit boss takes his share ---

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Dog = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("The ladder in play is now the Dog rank"), Dog->RankName.ToString(), FString(TEXT("Dog")));
		TestEqual(TEXT("The debt from the gate came along"), State.Debt, Notes->Debt, 0.01f);
		TestEqual(TEXT("The player's name came along"), State.Name, 3);

		WalkToBell(Fight);

		DisableEnemyBrain(this, Fight);

		TestTrue(TEXT("The crowd calls for a finish"), Fight.GameMode->IsCrowdCalling());

		Notes->Call = Fight.GameMode->GetCrowdCall();
		Notes->Name = State.Name;

		// one touch from staggering
		Fight.Enemy->SetHealth(1.0f);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this, Notes](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy is staggered"), Fight.Enemy->IsStaggered());
		TestTrue(TEXT("The player gives the crowd the finish it called for"), Fight.Player->TryFinisher(Notes->Call));
	} });

	Steps.Add({ 3.0f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Dog = GetLadder(Fight);
		const FGutLadderState& State = Fight.GameMode->GetLadderState();
		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		const float Purse = Dog->Rungs[0].Purse;
		const float Kept = FMath::Min(Notes->Debt, Purse * Dog->DebtShareOfPurse);

		TestEqual(TEXT("The finisher wins the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The enemy died to a finisher of the style called for"), Fight.Enemy->WasFinished() && Fight.Enemy->GetKillingFinisherStyle() == Notes->Call);
		TestTrue(TEXT("The crowd's call was matched"), Settlement.bCrowdCallMatched);
		TestEqual(TEXT("The crowd pays its bonus"), Settlement.CrowdBonus, Dog->CrowdCallBonus);
		TestEqual(TEXT("The win and the finish both make the player's name"), State.Name, Notes->Name + Dog->NameForWin + Dog->NameForCrowdCall);
		TestEqual(TEXT("The pit boss keeps his share of the purse"), Settlement.DebtPaid, Kept, 0.01f);
		TestEqual(TEXT("The debt goes down by it"), State.Debt, Notes->Debt - Kept, 0.01f);
		TestEqual(TEXT("The player holds the rest of the purse and the bonus"), State.Water, Purse - Kept + Dog->CrowdCallBonus, 0.01f);
		TestEqual(TEXT("Dogs get no cut of the gate"), Settlement.GateCut, 0.0f);

		Screenshot(TEXT("30_crowd_call_paid"));
	} });

	// --- the run is kept on disk ---

	Steps.Add({ 0.5f, [this, Notes, GetInstance](const FFight& Fight)
	{
		UGutGameInstance* GameInstance = GetInstance(Fight);

		TestFalse(TEXT("Automated runs do not save"), GameInstance->bSaveEnabled);

		// switch saving on, in a slot of the test's own
		GameInstance->SaveSlotName = TEXT("GutAutomation");
		GameInstance->bSaveEnabled = true;

		TestTrue(TEXT("The run is written to disk"), GameInstance->SaveRun());
		TestTrue(TEXT("There is a run on disk"), GameInstance->HasSavedRun());

		Notes->Saved = GameInstance->LadderAtRungStart;

		// lose track of it, then read it back
		GameInstance->Ladder = FGutLadderState();

		TestTrue(TEXT("The run is read back"), GameInstance->LoadRun());

		const FGutLadderState& State = GameInstance->Ladder;

		TestEqual(TEXT("The rank comes back"), State.Rank, Notes->Saved.Rank);
		TestEqual(TEXT("The fight comes back"), State.Rung, Notes->Saved.Rung);
		TestEqual(TEXT("The water comes back"), State.Water, Notes->Saved.Water);
		TestEqual(TEXT("The debt comes back"), State.Debt, Notes->Saved.Debt);
		TestEqual(TEXT("The name comes back"), State.Name, Notes->Saved.Name);
		TestEqual(TEXT("It is the run as it stood after the last fight was paid"), State.Rung, 1);

		// starting again forgets it
		GameInstance->ResetLadder(0.0f);

		TestFalse(TEXT("Starting the ladder again forgets the run on disk"), GameInstance->HasSavedRun());

		GameInstance->bSaveEnabled = false;
		GameInstance->SaveSlotName = TEXT("GutRun");

		// --- on to the top of the ladder by the short way: the Champion's fight, with a name ---

		GameInstance->Ladder = Notes->Saved;
		GameInstance->Ladder.Rank = 3;
		GameInstance->Ladder.Rung = 0;
		GameInstance->Ladder.Debt = 0.0f;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;

		Notes->Name = GameInstance->Ladder.Name;
	} });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, Notes](const FFight& Fight)
	{
		const UGutLadderData* Champion = GetLadder(Fight);

		TestEqual(TEXT("The ladder in play is the Champion's"), Champion->RankName.ToString(), FString(TEXT("Champion")));

		WalkToBell(Fight);

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		TestEqual(TEXT("The Champion's fight pays a cut of the gate that counts the player's name"), Settlement.GateCut, Champion->GetGateCut(Notes->Name));
		TestFalse(TEXT("A kill that is not a finisher does not match the crowd's call"), Settlement.bCrowdCallMatched);
		TestTrue(TEXT("One win finishes the rank"), Fight.GameMode->GetLadderState().bFinished);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestEqual(TEXT("The cage opens on the gate"), Fight.GameMode->GetPhase(), EGutFightPhase::Gate);
		TestEqual(TEXT("There is no bill at the top"), GetLadder(Fight)->bBillAtGate, false);
	} });

	Steps.Add({ 0.8f, [](const FFight& Fight)
	{
		Screenshot(TEXT("31_gate_champion"));
	} });

	Steps.Add({ 0.3f, [](const FFight& Fight) { Fight.GameMode->LeaveGate(); } });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("From the top of the ladder the only way on is back to the bottom"), State.Rank, 0);
		TestEqual(TEXT("At the first fight"), State.Rung, 0);
		TestEqual(TEXT("With no name"), State.Name, 0);
		TestFalse(TEXT("And whole"), State.bInjuriesCarried);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutArmorTest, "Ashmarket.Gut.Armor", GutTests::Flags)

bool FGutArmorTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLadderTests;

	if (!OpenCage(this))
	{
		return false;
	}

	struct FNotes
	{
		float Health = 0.0f;
		float Limb = 0.0f;
		float Water = 0.0f;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	auto GetInstance = [](const FFight& Fight) { return Cast<UGutGameInstance>(Fight.GameMode->GetGameInstance()); };

	// the rank that sells armor and has sponsors: the third
	auto GetButcher = [](const FFight& Fight) -> const UGutLadderData*
	{
		const UGutLadderData* Ladder = Fight.GameMode->GetLadder();

		while (Ladder && Ladder->ArmorShop.IsEmpty())
		{
			Ladder = Ladder->NextLadder;
		}

		return Ladder;
	};

	auto FindPiece = [GetButcher](const FFight& Fight, EGutArmorSlot Slot, bool bAgainstBlades) -> UGutArmorData*
	{
		const UGutLadderData* Butcher = GetButcher(Fight);

		for (UGutArmorData* Piece : Butcher ? Butcher->ArmorShop : TArray<TObjectPtr<UGutArmorData>>())
		{
			if (Piece && Piece->Slot == Slot && (Piece->BladedScale < Piece->BluntScale) == bAgainstBlades)
			{
				return Piece;
			}
		}

		return nullptr;
	};

	TArray<FStep> Steps;

	// --- what armor does to a blow, in a single fight ---

	Steps.Add({ 0.2f, [this, Notes, GetButcher, FindPiece](const FFight& Fight)
	{
		if (!TestNotNull(TEXT("A rank of the ladder sells armor"), GetButcher(Fight)))
		{
			return;
		}

		UGutArmorData* RubberSleeves = FindPiece(Fight, EGutArmorSlot::Arms, false);
		UGutArmorData* SignBracers = FindPiece(Fight, EGutArmorSlot::Arms, true);

		if (!TestNotNull(TEXT("There are tire-rubber sleeves"), RubberSleeves) || !TestNotNull(TEXT("There are road-sign bracers"), SignBracers))
		{
			return;
		}

		TestTrue(TEXT("Tire rubber is made against blunt blows"), RubberSleeves->BluntScale < RubberSleeves->BladedScale);
		TestTrue(TEXT("Road sign is made against blades"), SignBracers->BladedScale < SignBracers->BluntScale);

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		// the club's opening swing is a blunt blow to the left arm
		Fight.Enemy->WearArmor(RubberSleeves);

		TestTrue(TEXT("The enemy wears the sleeves on its arms"), Fight.Enemy->GetArmorOn(EGutArmorSlot::Arms) == RubberSleeves);
		TestNull(TEXT("And nothing on its body"), Fight.Enemy->GetArmorOn(EGutArmorSlot::Torso));

		const FGutAttackDef* Swing = Fight.Player->GetNextLightAttack();

		TestEqual(TEXT("The club is blunt"), Swing->DamageType, EGutDamageType::Blunt);

		Notes->Health = Fight.Enemy->GetHealth() - (Swing->Damage * RubberSleeves->BluntScale);
		Notes->Limb = Fight.Enemy->GetMaxLimbHealth() - (Swing->Damage * Swing->LimbDamageScale * RubberSleeves->BluntScale);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this, Notes, FindPiece](const FFight& Fight)
	{
		TestEqual(TEXT("Armor on the arm takes its share off the health damage of a blow that lands there"), Fight.Enemy->GetHealth(), Notes->Health, 0.01f);
		TestEqual(TEXT("And off the damage to the limb"), Fight.Enemy->GetLimbHealth(EGutLimb::LeftArm), Notes->Limb, 0.01f);

		Screenshot(TEXT("32_armor"));

		// the heavy lands on the body, where the enemy wears nothing
		const FGutAttackDef& Heavy = Fight.Player->GetHeavyAttack();

		Notes->Health = Fight.Enemy->GetHealth() - Heavy.Damage;

		PlaceFaceToFace(Fight);

		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

		TestTrue(TEXT("Heavy attack starts"), Fight.Player->TryHeavyAttack());
	} });

	Steps.Add({ 2.2f, [this, Notes, FindPiece](const FFight& Fight)
	{
		TestEqual(TEXT("Armor on the arms does nothing for a blow to the body"), Fight.Enemy->GetHealth(), Notes->Health, 0.01f);

		// a second piece for the same part of the body goes on in place of the first
		UGutArmorData* SignBracers = FindPiece(Fight, EGutArmorSlot::Arms, true);

		Fight.Enemy->WearArmor(SignBracers);

		TestTrue(TEXT("A second piece for the arms replaces the first"), Fight.Enemy->GetArmorOn(EGutArmorSlot::Arms) == SignBracers && Fight.Enemy->GetArmor().Num() == 1);

		// the machete's opening cut is a bladed blow to the left arm
		UGutWeaponData* Machete = FindRackWeapon(Fight, TEXT("Machete"));

		if (!TestNotNull(TEXT("There is a machete on the rack"), Machete))
		{
			return;
		}

		Fight.Player->EquipWeapon(Machete);
		Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
		Fight.Enemy->SetHealth(Fight.Enemy->GetMaxHealth());

		const FGutAttackDef& Cut = Machete->LightAttacks[0];

		TestEqual(TEXT("The machete is bladed"), Cut.DamageType, EGutDamageType::Bladed);

		Notes->Health = Fight.Enemy->GetHealth() - (Cut.Damage * SignBracers->BladedScale);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.2f, [this, Notes, GetInstance](const FFight& Fight)
	{
		TestEqual(TEXT("Road sign takes its share off a cut"), Fight.Enemy->GetHealth(), Notes->Health, 0.01f);

		// --- on the ladder, at the rank that sells armor and has sponsors ---

		Fight.GameMode->StartNewLadder();
	} });

	RunThenWaitForNextFight(this, Steps);

	Steps.Add({ 0.3f, [this, GetInstance](const FFight& Fight)
	{
		TestEqual(TEXT("At Meat rank the card has nothing on it but the way out"), Fight.GameMode->GetCardItems().Num(), 1);
		TestNull(TEXT("And no sponsor"), Fight.GameMode->GetSponsorOffer());

		// to Butcher rank by the short way, with water to spend
		UGutGameInstance* GameInstance = GetInstance(Fight);

		GameInstance->Ladder.Rank = 2;
		GameInstance->Ladder.Water = 60.0f;
		GameInstance->LadderAtRungStart = GameInstance->Ladder;

		Fight.GameMode->EnterSandbox();
		Fight.GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		GameInstance->bSandbox = false;
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- first Butcher fight: buy armor, take the slow-kill offer, and meet it ---

	Steps.Add({ 0.3f, [this, Notes, FindPiece](const FFight& Fight)
	{
		const UGutLadderData* Butcher = Fight.GameMode->GetLadder();
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestFalse(TEXT("This rank sells armor"), Butcher->ArmorShop.IsEmpty());
		TestEqual(TEXT("The card lists the sponsor, every piece of armor, Dust, Hush and the way out"), Fight.GameMode->GetCardItems().Num(), 1 + Butcher->ArmorShop.Num() + 2 + 1);
		TestTrue(TEXT("The choice starts on the way out"), Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Rack);

		const FGutSponsor* Sponsor = Fight.GameMode->GetSponsorOffer();

		if (!TestNotNull(TEXT("A sponsor makes an offer"), Sponsor))
		{
			return;
		}

		TestEqual(TEXT("The first offer is for a slow kill"), Sponsor->Demand, EGutSponsorDemand::KillSlowly);

		// buy a vest, then think better of it and buy the other one
		const UGutArmorData* RubberVest = FindPiece(Fight, EGutArmorSlot::Torso, false);
		const UGutArmorData* SignPlate = FindPiece(Fight, EGutArmorSlot::Torso, true);

		TestTrue(TEXT("The player buys the tire-rubber vest"), Fight.GameMode->BuyArmor(Butcher->ArmorShop.IndexOfByKey(RubberVest)));
		TestEqual(TEXT("It is paid for"), State.Water, 60.0f - RubberVest->Price);
		TestTrue(TEXT("And worn"), Fight.GameMode->IsArmorWorn(RubberVest));
		TestFalse(TEXT("It cannot be bought twice"), Fight.GameMode->BuyArmor(Butcher->ArmorShop.IndexOfByKey(RubberVest)));

		TestTrue(TEXT("The player buys the road-sign breastplate"), Fight.GameMode->BuyArmor(Butcher->ArmorShop.IndexOfByKey(SignPlate)));
		TestTrue(TEXT("It goes on in place of the vest"), Fight.GameMode->IsArmorWorn(SignPlate) && !Fight.GameMode->IsArmorWorn(RubberVest));
		TestEqual(TEXT("Both were paid for"), State.Water, 60.0f - RubberVest->Price - SignPlate->Price);
		TestEqual(TEXT("And counted"), State.ArmorSpend, RubberVest->Price + SignPlate->Price);
	} });

	// the sponsor's line is the first: up from the way out past Dust, Hush and every piece of armor
	for (int32 i = 0; i < 9; ++i)
	{
		Tap(Steps, 0.1f, EKeys::W);
	}

	Tap(Steps, 0.2f, EKeys::LeftMouseButton);

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("W walks the choice up to the sponsor's line"), Fight.GameMode->GetCardSelection().Type == EGutCardItemType::Sponsor);
		TestTrue(TEXT("Left mouse takes the offer"), Fight.GameMode->IsSponsorAccepted());

		Screenshot(TEXT("33_card_sponsor_and_armor"));
	} });

	Steps.Add({ 0.5f, [this, Notes, FindPiece](const FFight& Fight)
	{
		const UGutLadderData* Butcher = Fight.GameMode->GetLadder();

		Notes->Water = Fight.GameMode->GetLadderState().Water;

		Fight.GameMode->ConfirmCard();

		TestTrue(TEXT("The player goes to the rack wearing what it bought"), Fight.Player->GetArmorOn(EGutArmorSlot::Torso) == FindPiece(Fight, EGutArmorSlot::Torso, true));
		TestEqual(TEXT("The opponent wears its rung's armor"), Fight.Enemy->GetArmor().Num(), Butcher->Rungs[0].Armor.Num());
		TestTrue(TEXT("Opponents at this rank are armored"), Fight.Enemy->GetArmor().Num() > 0);

		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		DisableEnemyBrain(this, Fight);

		Fight.Enemy->SetHealth(1.0f);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("Light attack starts"), Fight.Player->TryLightAttack());
	} });

	Steps.Add({ 1.0f, [this](const FFight& Fight)
	{
		TestTrue(TEXT("The enemy is staggered"), Fight.Enemy->IsStaggered());
		TestTrue(TEXT("The player finishes it the slow way"), Fight.Player->TryFinisher(EGutFinisherStyle::Slow));
	} });

	Steps.Add({ 3.0f, [this, Notes](const FFight& Fight)
	{
		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();
		const FGutSponsor* Sponsor = &Fight.GameMode->GetLadder()->Sponsors[0];

		TestEqual(TEXT("The slow finisher wins the fight"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerWon);
		TestTrue(TEXT("The fight was sponsored"), Settlement.bSponsored);
		TestEqual(TEXT("The sponsor pays for the slow kill it asked for"), Settlement.SponsorPay, Sponsor->Pay);
		TestTrue(TEXT("The sponsor's water is in the player's hand"), Fight.GameMode->GetLadderState().Water >= Notes->Water + Sponsor->Pay);
	} });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- second Butcher fight: the offer is to wear colors, which means no armor ---

	Steps.Add({ 0.3f, [this, FindPiece](const FFight& Fight)
	{
		const FGutSponsor* Sponsor = Fight.GameMode->GetSponsorOffer();

		if (!TestNotNull(TEXT("A sponsor makes an offer"), Sponsor))
		{
			return;
		}

		TestEqual(TEXT("The second offer is to wear colors"), Sponsor->Demand, EGutSponsorDemand::WearColors);
		TestTrue(TEXT("The armor bought for the last fight is still owned"), Fight.GameMode->IsArmorWorn(FindPiece(Fight, EGutArmorSlot::Torso, true)));

		Fight.GameMode->SetSponsorAccepted(true);
		Fight.GameMode->ConfirmCard();

		TestEqual(TEXT("In a sponsor's colors the player wears no armor"), Fight.Player->GetArmor().Num(), 0);

		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		Screenshot(TEXT("34_sponsor_colors"));

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		TestEqual(TEXT("Any win in the colors is paid for"), Fight.GameMode->GetSettlement().SponsorPay, Sponsor->Pay);
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- third: the window pays to lose. The player goes down, is left alive and paid, and the fight is still to be won ---

	TSharedRef<FGutLadderState> Before = MakeShared<FGutLadderState>();

	Steps.Add({ 0.3f, [this, FindPiece, Before](const FFight& Fight)
	{
		const FGutSponsor* Sponsor = Fight.GameMode->GetSponsorOffer();

		if (!TestNotNull(TEXT("A sponsor makes an offer"), Sponsor))
		{
			return;
		}

		TestEqual(TEXT("The third offer is to lose"), Sponsor->Demand, EGutSponsorDemand::ThrowFight);

		*Before = Fight.GameMode->GetLadderState();

		Fight.GameMode->SetSponsorAccepted(true);
		Fight.GameMode->ConfirmCard();

		TestTrue(TEXT("Out of the colors, the armor is back on"), Fight.Player->GetArmorOn(EGutArmorSlot::Torso) == FindPiece(Fight, EGutArmorSlot::Torso, true));
		TestTrue(TEXT("The fight is one the player has been paid to lose"), Fight.GameMode->IsThrowingFight());

		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		// one touch from beaten
		Fight.Player->SetHealth(1.0f);

		PlaceFaceToFace(Fight);

		TestTrue(TEXT("The opponent swings"), Fight.Enemy->TryLightAttack());
	} });

	Steps.Add({ 0.8f, [this, Before](const FFight& Fight)
	{
		const FGutSponsor* Sponsor = Fight.GameMode->GetSponsorOffer();
		const FGutLadderState& State = Fight.GameMode->GetLadderState();
		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		TestEqual(TEXT("A player that goes down has lost"), Fight.GameMode->GetPhase(), EGutFightPhase::PlayerLost);
		TestTrue(TEXT("But it is alive, as arranged"), Fight.Player->IsAlive());
		TestTrue(TEXT("The fight was thrown, not spared out of kindness"), Settlement.bSettled && Settlement.bThrown && !Settlement.bSpared && !Settlement.bPlayerWon);
		TestEqual(TEXT("The window pays for the loss it bought"), Settlement.SponsorPay, Sponsor ? Sponsor->Pay : -1.0f);
		TestEqual(TEXT("The water is in the player's hand"), State.Water, Before->Water + Settlement.SponsorPay, 0.01f);
		TestEqual(TEXT("There is no purse for losing"), Settlement.Purse, 0.0f);
		TestTrue(TEXT("Going down easy costs Name"), Settlement.NameLost > 0 && State.Name == Before->Name - Settlement.NameLost);
		TestEqual(TEXT("The same fight is still to be won"), State.Rung, Before->Rung);
		TestFalse(TEXT("And the ladder is not over"), State.bFinished);

		Screenshot(TEXT("39_thrown"));
	} });

	Steps.Add({ 2.5f, [](const FFight& Fight) { Fight.GameMode->RequestRestart(); } });

	RunThenWaitForNextFight(this, Steps);

	// --- the same fight again, the same offer. This time the player takes the water and wins: it keeps the win and owes the water ---

	Steps.Add({ 0.3f, [this, Before](const FFight& Fight)
	{
		const FGutSponsor* Sponsor = Fight.GameMode->GetSponsorOffer();
		const FGutLadderState& State = Fight.GameMode->GetLadderState();

		TestEqual(TEXT("Back at the same fight"), State.Rung, Before->Rung);
		TestTrue(TEXT("With the window's water still in hand"), State.Water > Before->Water);

		if (!TestTrue(TEXT("The window makes the same offer"), Sponsor && Sponsor->Demand == EGutSponsorDemand::ThrowFight))
		{
			return;
		}

		const float Debt = State.Debt;

		Fight.GameMode->SetSponsorAccepted(true);
		Fight.GameMode->ConfirmCard();
		Fight.GameMode->ConfirmChoice();
		Fight.GameMode->ConfirmBet();
		Fight.GameMode->StartFight();

		Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));

		const FGutSettlement& Settlement = Fight.GameMode->GetSettlement();

		TestTrue(TEXT("The player won a fight it was paid to lose"), Settlement.bPlayerWon && Settlement.bSponsored && !Settlement.bThrown);
		TestEqual(TEXT("The window pays nothing for a win"), Settlement.SponsorPay, 0.0f);
		TestEqual(TEXT("And what it paid the other corner is the player's to make good"), Settlement.SponsorOwed, Sponsor->Pay);
		TestEqual(TEXT("It goes on the debt"), Fight.GameMode->GetLadderState().Debt, Debt + Settlement.SponsorOwed - Settlement.DebtPaid, 0.01f);
		TestEqual(TEXT("The ladder moves on"), Fight.GameMode->GetLadderState().Rung, Before->Rung + 1);
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
