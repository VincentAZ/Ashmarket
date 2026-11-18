// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "Misc/CommandLine.h"

/**
 *  Not a test of anything that can fail: stables run by stand-ins, to see what each way of owning comes to.
 *
 *  Ashmarket.Balance.Stable    -GutBalanceFights=N days for each of four owners, three runs each, the same Block and the same
 *                              bills for all: one who feeds, patches, trains, promises tags and keeps the promise; one who buys
 *                              cheap, dopes, sends them in hurt, breaks the promise and sells the dead to the carts; one who
 *                              buys a single fighter and teaches it everything before it fights; and one who owns nobody and
 *                              only signs people with tags. Tools/run_gut_balance.sh 30 Stable runs it.
 */
namespace GutStableBalance
{
	using namespace GutTests;

	enum class EOwner : uint8 { Kind, Hard, Trainer, Signer, Count };

	const TCHAR* Names[] = { TEXT("Kind"), TEXT("Hard"), TEXT("Trainer"), TEXT("Signer") };

	struct FTally
	{
		int32 Fights = 0;
		int32 Won = 0;
		float Book = 0.0f;
		int32 LayDowns = 0;
		int32 Abandoned = 0;
		int32 Bought = 0;
		int32 Signed = 0;
		int32 Trainings = 0;
		int32 Doses = 0;
		int32 TagsGiven = 0;
		int32 TagsDenied = 0;
		int32 Lost = 0;
		int32 Buried = 0;
		int32 BodiesSold = 0;
		int32 IdleDays = 0;
		float WaterAtEnd = 0.0f;
		float DebtAtEnd = 0.0f;
		float Purses = 0.0f;
		int32 InTheYardAtEnd = 0;
		int32 NameAtEnd = 0;
		float Seconds = 0.0f;
	};

	int32 Taught(const FGutStableFighter& Fighter)
	{
		return Fighter.Strength + Fighter.Speed + Fighter.Nerve;
	}

	class FDaysCommand : public IAutomationLatentCommand
	{
	public:

		FDaysCommand(FAutomationTestBase* InTest, int32 InDays)
			: Test(InTest)
			, Days(InDays)
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

			// a cage just opened: nothing has been done in it yet
			if (LastGameMode != GameMode)
			{
				LastGameMode = GameMode;
				bRecorded = false;
			}

			// a new owner comes to the yard
			if (!bStarted)
			{
				bStarted = true;

				GameMode->EnterStable(2000 + Run);
				return false;
			}

			if (!GameMode->IsAtStable())
			{
				return false;
			}

			FTally& Tally = Tallies[static_cast<int32>(Owner)];
			const FGutStableState& State = GameMode->GetStableState();

			switch (GameMode->GetPhase())
			{
			case EGutFightPhase::Card:

				if (State.Day > Days)
				{
					NextOwner(Tally, State);
					return Finished();
				}

				ActAtCard(GameMode, Tally);
				break;

			case EGutFightPhase::Betting:
				GameMode->ConfirmBet();
				break;

			case EGutFightPhase::Intro:
				GameMode->StartFight();
				break;

			case EGutFightPhase::Fighting:

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
				const FGutStableSettlement& Result = GameMode->GetStableSettlement();

				if (!bRecorded && Result.bSettled)
				{
					bRecorded = true;

					++Tally.Fights;
					Tally.Won += Result.bWon ? 1 : 0;
					Tally.LayDowns += Result.bLayDown ? 1 : 0;
					Tally.Book += BookChance;
					Tally.Seconds += GameMode->GetFightTime();
				}

				if (GameMode->CanRestart())
				{
					GameMode->RequestRestart();
				}
				break;
			}

			default:
				break;
			}

			return false;
		}

	private:

		/** Does the next thing this kind of owner would do at the card. One thing a call: whatever it changes is looked at afresh the next time. */
		void ActAtCard(AGutGameMode* GameMode, FTally& Tally)
		{
			const FGutStableState& State = GameMode->GetStableState();

			const bool bHard = Owner == EOwner::Hard;

			// the dead first
			if (State.bBodyWaiting)
			{
				if (bHard)
				{
					GameMode->SendBodyTo(EGutBodyGoesTo::Grill);
				}
				else if (!GameMode->SendBodyTo(EGutBodyGoesTo::Ground))
				{
					// nothing in hand to bury it with
					GameMode->SendBodyTo(EGutBodyGoesTo::Parts);
				}

				return;
			}

			// tags that are due
			for (int32 i = 0; i < State.Fighters.Num(); ++i)
			{
				if (!GameMode->IsTagDue(i))
				{
					continue;
				}

				if (bHard)
				{
					if (GameMode->BreakPromise(i))
					{
						++Tally.TagsDenied;
						return;
					}
				}
				else if (GameMode->GiveTag(i))
				{
					++Tally.TagsGiven;
					return;
				}
			}

			// the Block, while there is room this owner means to fill
			const int32 Wanted = Owner == EOwner::Trainer ? 1 : (bHard ? 3 : 2);

			if (State.Fighters.Num() < Wanted)
			{
				TArray<FGutStableFighter> Offers;
				GameMode->GetBlockOffers(Offers);

				int32 Pick = INDEX_NONE;

				for (int32 i = 0; i < Offers.Num(); ++i)
				{
					if (State.BoughtToday.Contains(i))
					{
						continue;
					}

					const FGutStableFighter& Offer = Offers[i];

					// who this owner will have at all
					if (Offer.bOwned ? Owner == EOwner::Signer : (bHard || Owner == EOwner::Trainer))
					{
						continue;
					}

					// and can pay for with a little left over to feed it
					if (Offer.Price > State.Water - 8.0f)
					{
						continue;
					}

					if (Pick == INDEX_NONE)
					{
						Pick = i;
					}
					else if (Offers[Pick].bOwned && Offer.bOwned)
					{
						// the hard owner buys the cheapest, the others the one that knows most
						const bool bBetter = bHard ? Offer.Price < Offers[Pick].Price : Taught(Offer) > Taught(Offers[Pick]);

						Pick = bBetter ? i : Pick;
					}
				}

				if (Pick != INDEX_NONE && GameMode->BuyFighter(Pick))
				{
					++(Offers[Pick].bOwned ? Tally.Bought : Tally.Signed);
					return;
				}
			}

			// promises: everybody in a collar is told it can win its way out. Only one kind of owner means it.
			for (int32 i = 0; i < State.Fighters.Num(); ++i)
			{
				if (State.Fighters[i].bOwned && !State.Fighters[i].bPromisedTag && !State.Fighters[i].bBetrayed && GameMode->PromiseTag(i))
				{
					return;
				}
			}

			// the cutter, for those who use him
			if (!bHard)
			{
				for (int32 i = 0; i < State.Fighters.Num(); ++i)
				{
					if (State.Fighters[i].IsCrippled() && State.Water >= GameMode->GetStablePatchCost() + 4.0f && GameMode->PatchFighter(i))
					{
						return;
					}
				}
			}

			// who goes in today: the one that knows most, and for all but the hard owner, one that is whole
			int32 Best = INDEX_NONE;

			for (int32 i = 0; i < State.Fighters.Num(); ++i)
			{
				if (State.Fighters[i].IsCrippled() && !bHard)
				{
					continue;
				}

				if (Best == INDEX_NONE || Taught(State.Fighters[i]) > Taught(State.Fighters[Best]))
				{
					Best = i;
				}
			}

			if (Best == INDEX_NONE)
			{
				++Tally.IdleDays;

				GameMode->RestDay();
				return;
			}

			const FGutStableFighter& Fighter = State.Fighters[Best];

			// training: the trainer until there is nothing left to teach, the kind owner every other day
			const bool bTrains = (Owner == EOwner::Trainer && Taught(Fighter) < 3 * GameMode->GetStableMostTaught())
				|| (Owner == EOwner::Kind && State.Day % 2 == 0 && Taught(Fighter) < 3 * GameMode->GetStableMostTaught());

			if (bTrains && State.Water >= GameMode->GetTrainingCost() + 6.0f)
			{
				// whatever it knows least
				const EGutTraining What = (Fighter.Nerve <= Fighter.Strength && Fighter.Nerve <= Fighter.Speed) ? EGutTraining::Nerve
					: (Fighter.Strength <= Fighter.Speed ? EGutTraining::Strength : EGutTraining::Speed);

				if (GameMode->TrainFighter(Best, What))
				{
					++Tally.Trainings;
					return;
				}
			}

			if (bHard && !Fighter.bDoped && GameMode->DopeFighter(Best))
			{
				++Tally.Doses;
				return;
			}

			TArray<int32> Against;
			GameMode->GetStableOffers(Against);

			if (Against.IsEmpty())
			{
				++Tally.IdleDays;

				GameMode->RestDay();
				return;
			}

			// the hard owner takes the richest purse on the bill, the others the fight they are likeliest to walk out of
			if (GameMode->MakeMatch(Best, bHard ? Against.Last() : Against[0]))
			{
				BookChance = GameMode->GetStableBookChance();
			}
			else
			{
				++Tally.IdleDays;

				GameMode->RestDay();
			}
		}

		/** Notes how the owner that has just finished stands, and brings on the next run or the next kind of owner */
		void NextOwner(FTally& Tally, const FGutStableState& State)
		{
			Tally.WaterAtEnd += State.Water;
			Tally.DebtAtEnd += State.Debt;
			Tally.Purses += State.Purses;
			Tally.Lost += State.FightersLost;
			Tally.Buried += State.Buried;
			Tally.BodiesSold += State.BodiesSold;
			Tally.InTheYardAtEnd += State.Fighters.Num();
			Tally.NameAtEnd += State.Name;

			bStarted = false;

			if (++Run >= Runs)
			{
				Run = 0;
				Owner = static_cast<EOwner>(static_cast<int32>(Owner) + 1);
			}
		}

		/** Prints the summary once every kind of owner has had its days. Returns true when there is nothing left to do. */
		bool Finished()
		{
			if (Owner != EOwner::Count)
			{
				return false;
			}

			for (int32 i = 0; i < static_cast<int32>(EOwner::Count); ++i)
			{
				const FTally& Tally = Tallies[i];
				const float Fights = FMath::Max(Tally.Fights, 1);

				UE_LOG(LogGut, Display, TEXT("BALANCE Stable %s SUMMARY: %d runs of %d days. Bought %d, signed %d, trained %d times, doped %d times. ")
					TEXT("%d fights of %.0f s on average, %d abandoned, won %d (%.0f%%; the book said %.0f%%), %d lay down. ")
					TEXT("Lost %d fighters in all (dead or gone in the night), buried %d, sold %d bodies. Tags handed over %d, refused %d. Days with nobody to send in %d. ")
					TEXT("Ends a run with %.1f in the yard, a name of %.1f, purses of %.0f L, holding %.0f L and owing %.0f L on average."),
					Names[i], Runs, Days, Tally.Bought, Tally.Signed, Tally.Trainings, Tally.Doses,
					Tally.Fights, Tally.Seconds / Fights, Tally.Abandoned, Tally.Won, 100.0f * Tally.Won / Fights, 100.0f * Tally.Book / Fights, Tally.LayDowns,
					Tally.Lost, Tally.Buried, Tally.BodiesSold, Tally.TagsGiven, Tally.TagsDenied, Tally.IdleDays,
					static_cast<float>(Tally.InTheYardAtEnd) / Runs, static_cast<float>(Tally.NameAtEnd) / Runs, Tally.Purses / Runs, Tally.WaterAtEnd / Runs, Tally.DebtAtEnd / Runs);
			}

			// the cage is left for single fights, as the other tests expect to find it
			if (const FFight Fight = GetFight(); Fight.IsValid())
			{
				Fight.GameMode->GetGameInstance<UGutGameInstance>()->bStable = false;
			}

			return true;
		}

		FAutomationTestBase* Test;
		int32 Days;

		/** Each kind of owner runs a stable this many times, with a different Block and different bills each time */
		int32 Runs = 3;
		int32 Run = 0;

		EOwner Owner = EOwner::Kind;
		FTally Tallies[static_cast<int32>(EOwner::Count)];

		TWeakObjectPtr<AGutGameMode> LastGameMode;
		bool bStarted = false;
		bool bRecorded = false;
		float BookChance = 0.5f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceStableTest, "Ashmarket.Balance.Stable", GutTests::Flags)

bool FGutBalanceStableTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;

	if (!OpenCage(this))
	{
		return false;
	}

	int32 Days = 30;

	FParse::Value(FCommandLine::Get(), TEXT("GutBalanceFights="), Days);

	ADD_LATENT_AUTOMATION_COMMAND(GutStableBalance::FDaysCommand(this, FMath::Max(Days, 1)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
