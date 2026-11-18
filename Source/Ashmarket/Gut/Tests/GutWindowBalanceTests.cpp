// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "Misc/CommandLine.h"

/**
 *  Not a test of anything that can fail: nights at the betting window played out by stand-ins, to see what the window is worth.
 *
 *  Ashmarket.Balance.Window    -GutBalanceFights=N nights for each of four gamblers: one who only watches (so the book can be
 *                              held against what really happens), one who puts a step on the book's favourite every fight,
 *                              one who backs the other fighter, and one who pays the favourite to go down and backs the other.
 *                              Tools/run_gut_balance.sh 10 Window runs it. A fixer that is caught fights with the cage's own brain.
 */
namespace GutWindowBalance
{
	using namespace GutTests;

	enum class EGambler : uint8 { Watcher, Favourite, Underdog, Briber, Drugger, Count };

	const TCHAR* Names[] = { TEXT("Watcher"), TEXT("Favourite"), TEXT("Underdog"), TEXT("Briber"), TEXT("Drugger") };

	struct FTally
	{
		int32 Fights = 0;
		int32 FavouriteWon = 0;
		float BookOnFavourite = 0.0f;
		float BookOnWinner = 0.0f;
		int32 Bets = 0;
		int32 BetsWon = 0;
		int32 Fixes = 0;
		int32 FixesWorked = 0;
		int32 Caught = 0;
		int32 OwnFights = 0;
		int32 OwnFightsLost = 0;
		int32 Abandoned = 0;
		int32 StakedAgain = 0;
		float WaterAtEnd = 0.0f;
		float DebtAtEnd = 0.0f;
		float Seconds = 0.0f;
	};

	class FNightsCommand : public IAutomationLatentCommand
	{
	public:

		FNightsCommand(FAutomationTestBase* InTest, int32 InNights)
			: Test(InTest)
			, Nights(InNights)
		{
		}

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return false;
			}

			AGutGameMode* GameMode = Fight.GameMode;
			UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

			// a cage just opened: nothing has been done in it yet
			if (LastGameMode != GameMode)
			{
				LastGameMode = GameMode;
				bRecorded = false;
				bBrainGiven = false;
			}

			// a new gambler comes to the window
			if (!bStarted)
			{
				bStarted = true;

				GameMode->EnterWindow(1000 + (static_cast<int32>(Gambler) * 0) + Run);
				return false;
			}

			if (!GameMode->IsAtWindow() && !GameMode->IsGamblersOwnFight())
			{
				return false;
			}

			FTally& Tally = Tallies[static_cast<int32>(Gambler)];
			const FGutWindowState& State = GameMode->GetWindowState();

			switch (GameMode->GetPhase())
			{
			case EGutFightPhase::Card:
			{
				BookChance = GameMode->GetWindowBookChance();

				const int32 Favourite = BookChance >= 0.5f ? 0 : 1;

				// what this gambler arranges, if it can pay for it
				if (Gambler == EGambler::Briber)
				{
					GameMode->SetWindowFix(EGutFix::Bribe, Favourite);
				}
				else if (Gambler == EGambler::Drugger)
				{
					GameMode->SetWindowFix(EGutFix::DrugBucket, Favourite);
				}

				// the way on to the window is the card's first line
				for (int32 i = 0; i < 8; ++i)
				{
					GameMode->MoveCardSelection(-1);
				}

				GameMode->ActivateCardSelection();
				break;
			}

			case EGutFightPhase::Betting:
			{
				const int32 Favourite = BookChance >= 0.5f ? 0 : 1;
				const float Step = GameMode->GetLadder()->BetStep;

				int32 Back = -1;

				switch (Gambler)
				{
				case EGambler::Favourite:
					Back = Favourite;
					break;

				case EGambler::Underdog:
					Back = 1 - Favourite;
					break;

				case EGambler::Briber:
				case EGambler::Drugger:
					// only with something arranged: otherwise it keeps its water
					Back = GameMode->GetWindowFix() != EGutFix::None ? 1 - Favourite : -1;
					break;

				default:
					break;
				}

				if (Back >= 0)
				{
					// a fixer puts on what it dares, the others a step
					const bool bFixer = Gambler == EGambler::Briber || Gambler == EGambler::Drugger;

					GameMode->SetBet(Back == 0 ? EGutBetSide::Player : EGutBetSide::Opponent, bFixer ? Step * 4.0f : Step);
				}

				GameMode->ConfirmBet();
				break;
			}

			case EGutFightPhase::Intro:
				GameMode->StartFight();
				break;

			case EGutFightPhase::Fighting:

				// a fixer in the cage is fought by the cage's own brain, as the stand-in for a player
				if (GameMode->IsGamblersOwnFight() && !bBrainGiven)
				{
					bBrainGiven = true;

					if (UClass* BrainClass = Fight.Enemy->GetController() ? Fight.Enemy->GetController()->GetClass() : nullptr)
					{
						GiveBrain(Fight.Player, BrainClass);
					}
				}

				// a fight that will not end is ended
				if (GameMode->GetFightTime() > 300.0f)
				{
					++Tally.Abandoned;

					(Fight.Player->GetHealth() <= Fight.Enemy->GetHealth() ? Fight.Player : Fight.Enemy)->Kill(nullptr, FVector(1.0f, 0.0f, 0.0f));
				}
				break;

			case EGutFightPhase::PlayerWon:
			case EGutFightPhase::PlayerLost:
			{
				const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();

				if (!bRecorded && Result.bSettled)
				{
					bRecorded = true;

					if (Result.bOwnFight)
					{
						++Tally.OwnFights;
						Tally.OwnFightsLost += Result.bSurvived ? 0 : 1;

						// a dead gambler's night is over: the next one comes
						if (!Result.bSurvived)
						{
							NextGambler(Tally, State);
							return Finished();
						}
					}
					else
					{
						const int32 Favourite = BookChance >= 0.5f ? 0 : 1;

						++Tally.Fights;
						Tally.Seconds += GameMode->GetFightTime();
						Tally.FavouriteWon += Result.Winner == Favourite ? 1 : 0;
						Tally.BookOnFavourite += FMath::Max(BookChance, 1.0f - BookChance);
						Tally.BookOnWinner += Result.Winner == 0 ? BookChance : 1.0f - BookChance;

						if (Result.Backed >= 0)
						{
							++Tally.Bets;
							Tally.BetsWon += (Result.Backed == Result.Winner && !Result.bCaught) ? 1 : 0;
						}

						if (Result.Fix != EGutFix::None)
						{
							++Tally.Fixes;
							Tally.FixesWorked += Result.Winner != Result.FixedCorner ? 1 : 0;
							Tally.Caught += Result.bCaught ? 1 : 0;
						}

						Tally.StakedAgain += Result.bStakedAgain ? 1 : 0;
					}
				}

				if (GameMode->CanRestart())
				{
					GameMode->RequestRestart();
				}
				break;
			}

			case EGutFightPhase::Gate:

				if (State.Night >= Nights)
				{
					NextGambler(Tally, State);
					return Finished();
				}

				GameMode->HandleMenuInput(EGutMenuInput::Confirm);
				break;

			default:
				break;
			}

			return false;
		}

	private:

		/** Notes how the gambler that has just finished stands, and brings on the next run or the next kind of gambler */
		void NextGambler(FTally& Tally, const FGutWindowState& State)
		{
			Tally.WaterAtEnd += State.Water;
			Tally.DebtAtEnd += State.Debt;

			bStarted = false;

			if (++Run >= Runs)
			{
				Run = 0;
				Gambler = static_cast<EGambler>(static_cast<int32>(Gambler) + 1);
			}
		}

		/** Prints the summary once every kind of gambler has had its nights. Returns true when there is nothing left to do. */
		bool Finished()
		{
			if (Gambler != EGambler::Count)
			{
				return false;
			}

			for (int32 i = 0; i < static_cast<int32>(EGambler::Count); ++i)
			{
				const FTally& Tally = Tallies[i];
				const float Fights = FMath::Max(Tally.Fights, 1);

				UE_LOG(LogGut, Display, TEXT("BALANCE Window %s SUMMARY: %d runs of %d nights, %d fights of %.0f s on average, %d abandoned. The book's favourite won %.0f%% (the book said %.0f%%). ")
					TEXT("Bets %d, won %d (%.0f%%). Fixes %d, worked %d (%.0f%%), noticed %d (%.0f%%). In the cage %d times, died there %d. Staked again by the lender %d times. ")
					TEXT("Ends a run holding %.0f L and owing %.0f L on average."),
					Names[i], Runs, Nights, Tally.Fights, Tally.Seconds / Fights, Tally.Abandoned,
					100.0f * Tally.FavouriteWon / Fights, 100.0f * Tally.BookOnFavourite / Fights,
					Tally.Bets, Tally.BetsWon, 100.0f * Tally.BetsWon / FMath::Max(Tally.Bets, 1),
					Tally.Fixes, Tally.FixesWorked, 100.0f * Tally.FixesWorked / FMath::Max(Tally.Fixes, 1), Tally.Caught, 100.0f * Tally.Caught / FMath::Max(Tally.Fixes, 1),
					Tally.OwnFights, Tally.OwnFightsLost, Tally.StakedAgain,
					Tally.WaterAtEnd / Runs, Tally.DebtAtEnd / Runs);
			}

			// the cage is left for single fights, as the other tests expect to find it
			if (const FFight Fight = GetFight(); Fight.IsValid())
			{
				Fight.GameMode->GetGameInstance<UGutGameInstance>()->bWindow = false;
			}

			return true;
		}

		FAutomationTestBase* Test;
		int32 Nights;

		/** Each kind of gambler comes to the window this many times, to a different run of bills each time */
		int32 Runs = 3;
		int32 Run = 0;

		EGambler Gambler = EGambler::Watcher;
		FTally Tallies[static_cast<int32>(EGambler::Count)];

		TWeakObjectPtr<AGutGameMode> LastGameMode;
		bool bStarted = false;
		bool bRecorded = false;
		bool bBrainGiven = false;
		float BookChance = 0.5f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceWindowTest, "Ashmarket.Balance.Window", GutTests::Flags)

bool FGutBalanceWindowTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;

	if (!OpenCage(this))
	{
		return false;
	}

	int32 Nights = 3;

	FParse::Value(FCommandLine::Get(), TEXT("GutBalanceFights="), Nights);

	ADD_LATENT_AUTOMATION_COMMAND(GutWindowBalance::FNightsCommand(this, FMath::Max(Nights, 1)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
