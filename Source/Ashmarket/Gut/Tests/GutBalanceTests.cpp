// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GutArmorData.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/**
 *  Measures how fights against the opponent brain go, over many fights in a row:
 *
 *  Ashmarket.Balance.Opponent.Mirror   the opponent brain on both sides. The stand-in for an evenly matched player.
 *  Ashmarket.Balance.Opponent.Masher   a player who walks in and hammers light attack, and never defends
 *  Ashmarket.Balance.Opponent.Sharp    a player with perfect reactions: guards every light, dodges every heavy, always hits back,
 *                                      and breaks a guard that has no stamina behind it
 *  Ashmarket.Balance.Weapons.Pair      the opponent brain on both sides, one weapon against another, and how the fight was fought
 *  Ashmarket.Balance.Ladder.Arc        the opponent brain as the player, sent up the ladder over and over: how far it gets, and what it owes
 *
 *  Run them with Tools/run_gut_balance.sh, which runs the game on a fixed timestep with no frame limit, many times faster than real time.
 *  To try different opponent tuning without touching the Blueprint, pass -GutTune= with a comma separated list, for example
 *  -GutTune=AttackChance=0.4,MaxDecisionInterval=1.8. The names are the fields of FGutOpponentTuning.
 *  -GutHealth=120 does the same for both fighters' health.
 *  -GutNoHazards switches the cage's hazards off, to measure the fight on its own as it was before they existed.
 *  -GutWeapons=Chain,Shield gives the player's fighter the first weapon and the opponent the second, in any of these matchups.
 *  Each is any part of the name of a weapon asset on the rack. Without it both fighters start with a club.
 *  Tools/run_gut_weapons.sh plays every pair of weapons against each other this way and prints the table.
 *  Only the mirror match has targets it must hit: milestone 2 asks for a fight of 60 to 90 seconds that can go either way.
 *  The other two are there to show how the opponent copes with the extremes.
 */
namespace GutBalance
{
	using namespace GutTests;

	/** A fight that runs this long is abandoned and counted as a failure of the opponent to finish */
	const float MaxFightTime = 300.0f;

	/** Fights per matchup unless -GutBalanceFights= says otherwise */
	const int32 DefaultFights = 40;

	/** How one fighter fought in one fight */
	struct FSideRecord
	{
		FGutFighterTally Tally;
		int32 LimbsLost = 0;
		bool bDisarmed = false;

		/** Times the cage's hazards hurt this fighter */
		int32 HazardHits = 0;
	};

	struct FFightRecord
	{
		bool bPlayerWon = false;
		bool bTimedOut = false;
		float Duration = 0.0f;
		float WinnerHealth = 0.0f;

		/** If true the loser was killed by the cage, not finished */
		bool bByCage = false;

		/** If true the loser was killed by the cage straight after a blow from the winner drove it there */
		bool bCageCredited = false;

		/** Average distance between the fighters while the fight ran */
		float MeanGap = 0.0f;

		FSideRecord Player;
		FSideRecord Enemy;
	};

	struct FSummary
	{
		int32 Fights = 0;
		int32 PlayerWins = 0;
		int32 Timeouts = 0;
		float MedianDuration = 0.0f;
		float ShortDuration = 0.0f;
		float LongDuration = 0.0f;
		float InTargetShare = 0.0f;
		float MeanWinnerHealth = 0.0f;
		int32 CageKills = 0;
		int32 CageCredited = 0;
	};

	struct FMatchup
	{
		/** Name used in the results */
		FString Name;

		/** Called once per fight, before the bell */
		TFunction<void(const FFight&)> Setup;

		/** If true, how the fights were fought is reported as well as how they ended */
		bool bReportStyle = false;

		/** Called every frame of the fight, to play the player's side. Optional. */
		TFunction<void(const FFight&)> Drive;

		/** Called once with the results, to check them against any targets */
		TFunction<void(const FSummary&)> Check;
	};

	int32 GetNumFights()
	{
		int32 NumFights = DefaultFights;

		FParse::Value(FCommandLine::Get(), TEXT("GutBalanceFights="), NumFights);

		return FMath::Max(1, NumFights);
	}

	/** Applies any -GutTune= overrides from the command line to a fighter's brain, if it has the opponent brain */
	void ApplyTuningOverrides(FAutomationTestBase* Test, const AGutFighter* Fighter)
	{
		AGutOpponentController* Brain = Cast<AGutOpponentController>(Fighter->GetController());

		FString Overrides;

		if (!Brain || !FParse::Value(FCommandLine::Get(), TEXT("GutTune="), Overrides, false))
		{
			return;
		}

		FGutOpponentTuning Tuning = Brain->GetTuning();

		TArray<FString> Pairs;
		Overrides.ParseIntoArray(Pairs, TEXT(","));

		for (const FString& Pair : Pairs)
		{
			FString Name;
			FString Value;

			const FProperty* Property = Pair.Split(TEXT("="), &Name, &Value) ? FGutOpponentTuning::StaticStruct()->FindPropertyByName(FName(*Name)) : nullptr;

			if (!Property || !Property->ImportText_InContainer(*Value, &Tuning, nullptr, PPF_None))
			{
				Test->AddError(FString::Printf(TEXT("Could not apply the tuning override '%s'"), *Pair));
			}
		}

		Brain->SetTuning(Tuning);
	}

	/** Applies any -GutHealth= override from the command line, to try a different amount of health without touching the Blueprints */
	void ApplyHealthOverride(AGutFighter* Fighter)
	{
		float MaxHealth = 0.0f;

		if (!FParse::Value(FCommandLine::Get(), TEXT("GutHealth="), MaxHealth) || MaxHealth <= 0.0f)
		{
			return;
		}

		if (const FFloatProperty* Property = FindFProperty<FFloatProperty>(AGutFighter::StaticClass(), TEXT("MaxHealth")))
		{
			Property->SetPropertyValue_InContainer(Fighter, MaxHealth);

			Fighter->SetHealth(MaxHealth);
		}
	}

	/**
	 *  Tries a weapon's numbers without writing them into its asset: -GutWeaponTune=Chain:Damage=1.1,Light.Windup=0.9
	 *  multiplies those fields of every attack (or only the light ones, or only the heavy one) of the weapon whose name has
	 *  "Chain" in it, in this process's memory only. The names are the fields of FGutAttackDef.
	 */
	void TuneWeapon(FAutomationTestBase* Test, UGutWeaponData* Weapon)
	{
		static TSet<TWeakObjectPtr<UGutWeaponData>> Tuned;

		FString Tune;
		FString NamePart;
		FString Fields;

		if (!Weapon || Tuned.Contains(Weapon) || !FParse::Value(FCommandLine::Get(), TEXT("GutWeaponTune="), Tune, false)
			|| !Tune.Split(TEXT(":"), &NamePart, &Fields) || !Weapon->GetName().Contains(NamePart))
		{
			return;
		}

		Tuned.Add(Weapon);

		TArray<FString> Pairs;
		Fields.ParseIntoArray(Pairs, TEXT(","));

		for (const FString& Pair : Pairs)
		{
			FString Name;
			FString Value;

			if (!Pair.Split(TEXT("="), &Name, &Value))
			{
				continue;
			}

			const bool bLightOnly = Name.RemoveFromStart(TEXT("Light."));
			const bool bHeavyOnly = Name.RemoveFromStart(TEXT("Heavy."));

			const FFloatProperty* Property = CastField<FFloatProperty>(FGutAttackDef::StaticStruct()->FindPropertyByName(FName(*Name)));

			if (!Property)
			{
				Test->AddError(FString::Printf(TEXT("-GutWeaponTune: an attack has no number called %s"), *Name));
				continue;
			}

			const float Scale = FCString::Atof(*Value);

			if (!bHeavyOnly)
			{
				for (FGutAttackDef& Attack : Weapon->LightAttacks)
				{
					Property->SetPropertyValue_InContainer(&Attack, Property->GetPropertyValue_InContainer(&Attack) * Scale);
				}
			}

			if (!bLightOnly)
			{
				Property->SetPropertyValue_InContainer(&Weapon->HeavyAttack, Property->GetPropertyValue_InContainer(&Weapon->HeavyAttack) * Scale);
			}
		}

		UE_LOG(LogGut, Display, TEXT("BALANCE %s is tried with %s"), *Weapon->GetName(), *Fields);
	}

	/** Returns the two weapon names given with -GutWeapons=, or false if there were none */
	bool GetWeaponOverrides(FString& OutPlayerWeapon, FString& OutEnemyWeapon)
	{
		FString Names;

		return FParse::Value(FCommandLine::Get(), TEXT("GutWeapons="), Names, false) && Names.Split(TEXT(","), &OutPlayerWeapon, &OutEnemyWeapon);
	}

	/**
	 *  Takes the fighters away from the rack with known weapons: the two named with -GutWeapons=, or the ones they start with.
	 *  Left alone, the opponent would draw at random and no two runs would measure the same thing.
	 */
	void LeaveRack(FAutomationTestBase* Test, const FFight& Fight)
	{
		Fight.GameMode->SkipRack();

		if (FParse::Param(FCommandLine::Get(), TEXT("GutNoHazards")))
		{
			Fight.GameMode->SetHazardsEnabled(false);
		}

		FString PlayerName;
		FString EnemyName;

		if (!GetWeaponOverrides(PlayerName, EnemyName))
		{
			return;
		}

		UGutWeaponData* PlayerWeapon = FindRackWeapon(Fight, PlayerName);
		UGutWeaponData* EnemyWeapon = FindRackWeapon(Fight, EnemyName);

		if (!PlayerWeapon || !EnemyWeapon)
		{
			Test->AddError(FString::Printf(TEXT("-GutWeapons=%s,%s does not name two weapons on the rack"), *PlayerName, *EnemyName));
			return;
		}

		TuneWeapon(Test, PlayerWeapon);
		TuneWeapon(Test, EnemyWeapon);

		Fight.Player->EquipWeapon(PlayerWeapon);
		Fight.Enemy->EquipWeapon(EnemyWeapon);

		// -GutWoman=Player, Enemy or Both: whose body is a woman's, for measuring what that is worth
		FString Women;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutWoman="), Women))
		{
			Fight.Player->SetWoman(Women == TEXT("Player") || Women == TEXT("Both"));
			Fight.Enemy->SetWoman(Women == TEXT("Enemy") || Women == TEXT("Both"));
		}
	}

	/** Notes how one fighter fought, at the end of a fight */
	FSideRecord RecordSide(const AGutFighter* Fighter, const UGutWeaponData* StartedWith)
	{
		FSideRecord Side;
		Side.Tally = Fighter->GetTally();

		for (const EGutLimb Limb : { EGutLimb::LeftArm, EGutLimb::RightArm, EGutLimb::LeftLeg, EGutLimb::RightLeg })
		{
			Side.LimbsLost += Fighter->IsLimbCrippled(Limb) ? 1 : 0;
		}

		// every dead fighter lets go of its weapon, so look at the arm that held it instead
		Side.bDisarmed = StartedWith && Fighter->IsLimbCrippled(Fighter->GetArmFor(StartedWith));

		Side.HazardHits = Fighter->GetHazardHits();

		return Side;
	}

	/** Plays one matchup over and over and reports how the fights went */
	class FRunMatchupCommand : public IAutomationLatentCommand
	{
	public:

		FRunMatchupCommand(FAutomationTestBase* InTest, FMatchup InMatchup)
			: Test(InTest)
			, Matchup(MoveTemp(InMatchup))
			, NumFights(GetNumFights())
		{
		}

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			// between levels
			if (!Fight.IsValid())
			{
				return false;
			}

			// a fresh cage: set the fighters up and ring the bell
			if (Fight.GameMode != CurrentGameMode.Get())
			{
				CurrentGameMode = Fight.GameMode;
				bRecorded = false;
				bNextRequested = false;

				LeaveRack(Test, Fight);

				PlayerWeapon = Fight.Player->GetArmament();
				EnemyWeapon = Fight.Enemy->GetArmament();

				GapTotal = 0.0f;
				GapSamples = 0;

				if (Matchup.Setup)
				{
					Matchup.Setup(Fight);
				}

				ApplyTuningOverrides(Test, Fight.Player);
				ApplyTuningOverrides(Test, Fight.Enemy);

				ApplyHealthOverride(Fight.Player);
				ApplyHealthOverride(Fight.Enemy);

				Fight.GameMode->StartFight();

				return false;
			}

			if (!IsFightOver(Fight))
			{
				if (Matchup.Drive)
				{
					Matchup.Drive(Fight);
				}

				GapTotal += Fight.Player->GetGapTo(Fight.Enemy);
				++GapSamples;

				// nobody is going to win this one
				if (!bNextRequested && Fight.GameMode->GetFightTime() > MaxFightTime)
				{
					FFightRecord Record;
					Record.bTimedOut = true;
					Record.Duration = MaxFightTime;

					Records.Add(Record);

					// say where it got stuck, so the reason can be found
					auto Describe = [](const AGutFighter* Fighter)
					{
						const AGutOpponentController* Brain = Cast<AGutOpponentController>(Fighter->GetController());

						return FString::Printf(TEXT("at %.0f, %.0f with %.0f health and %.0f stamina, %s, brain %s"),
							Fighter->GetActorLocation().X, Fighter->GetActorLocation().Y, Fighter->GetHealth(), Fighter->GetStamina(),
							*UEnum::GetDisplayValueAsText(Fighter->GetFighterState()).ToString(),
							Brain ? *UEnum::GetDisplayValueAsText(Brain->GetBrainState()).ToString() : TEXT("none"));
					};

					UE_LOG(LogGut, Display, TEXT("BALANCE %s fight %d: abandoned. Player %s. Opponent %s."),
						*Matchup.Name, Records.Num(), *Describe(Fight.Player), *Describe(Fight.Enemy));

					if (Records.Num() >= NumFights)
					{
						Finish();
						return true;
					}

					bNextRequested = true;

					UGameplayStatics::OpenLevel(Fight.GameMode, FName(MapPath));
				}

				return false;
			}

			if (!bRecorded)
			{
				bRecorded = true;

				FFightRecord Record;
				Record.bPlayerWon = Fight.GameMode->GetPhase() == EGutFightPhase::PlayerWon;
				Record.Duration = Fight.GameMode->GetFightTime();
				Record.WinnerHealth = Record.bPlayerWon ? Fight.Player->GetHealth() : Fight.Enemy->GetHealth();
				Record.MeanGap = GapSamples > 0 ? GapTotal / GapSamples : 0.0f;
				Record.Player = RecordSide(Fight.Player, PlayerWeapon.Get());
				Record.Enemy = RecordSide(Fight.Enemy, EnemyWeapon.Get());

				const AGutFighter* Loser = Record.bPlayerWon ? Fight.Enemy : Fight.Player;

				Record.bByCage = Loser->GetKillingHazard() != nullptr;
				Record.bCageCredited = Record.bByCage && Loser->GetKiller() != nullptr;

				Records.Add(Record);

				UE_LOG(LogGut, Display, TEXT("BALANCE %s fight %d: %s won in %.1f s with %.0f health%s"),
					*Matchup.Name, Records.Num(), Record.bPlayerWon ? TEXT("player") : TEXT("opponent"), Record.Duration, Record.WinnerHealth,
					Record.bByCage ? TEXT(", the loser killed by the cage") : TEXT(""));

				if (Records.Num() >= NumFights)
				{
					Finish();
					return true;
				}
			}

			if (!bNextRequested && Fight.GameMode->CanRestart())
			{
				bNextRequested = true;

				Fight.GameMode->RequestRestart();
			}

			return false;
		}

	private:

		void Finish()
		{
			FSummary Summary;
			Summary.Fights = Records.Num();

			TArray<float> Durations;
			float WinnerHealthTotal = 0.0f;
			int32 InTarget = 0;

			for (const FFightRecord& Record : Records)
			{
				Durations.Add(Record.Duration);

				Summary.PlayerWins += Record.bPlayerWon ? 1 : 0;
				Summary.Timeouts += Record.bTimedOut ? 1 : 0;

				WinnerHealthTotal += Record.WinnerHealth;
				InTarget += (Record.Duration >= 60.0f && Record.Duration <= 90.0f) ? 1 : 0;

				Summary.CageKills += Record.bByCage ? 1 : 0;
				Summary.CageCredited += Record.bCageCredited ? 1 : 0;
			}

			Durations.Sort();

			// the middle fight, and the fights a tenth of the way in from either end
			Summary.MedianDuration = Durations[Durations.Num() / 2];
			Summary.ShortDuration = Durations[Durations.Num() / 10];
			Summary.LongDuration = Durations[(Durations.Num() * 9) / 10];
			Summary.InTargetShare = static_cast<float>(InTarget) / Summary.Fights;
			Summary.MeanWinnerHealth = WinnerHealthTotal / Summary.Fights;

			const FString Text = FString::Printf(
				TEXT("BALANCE %s SUMMARY: %d fights, player won %d (%.0f%%), median %.0f s, middle 80%% of fights %.0f to %.0f s, %.0f%% inside 60 to 90 s, winner's health %.0f on average, %d abandoned, %d ended by the cage (%d of them just after a blow drove the loser there)"),
				*Matchup.Name, Summary.Fights, Summary.PlayerWins, 100.0f * Summary.PlayerWins / Summary.Fights,
				Summary.MedianDuration, Summary.ShortDuration, Summary.LongDuration, 100.0f * Summary.InTargetShare, Summary.MeanWinnerHealth, Summary.Timeouts,
				Summary.CageKills, Summary.CageCredited);

			UE_LOG(LogGut, Display, TEXT("%s"), *Text);

			Test->AddInfo(Text);

			if (Matchup.bReportStyle)
			{
				ReportStyle();
			}

			if (Matchup.Check)
			{
				Matchup.Check(Summary);
			}
		}

		/** Reports how the fights were fought: the distance they were fought at, and what each side's attacks came to */
		void ReportStyle()
		{
			float GapTotalAll = 0.0f;
			float Minutes = 0.0f;

			for (const FFightRecord& Record : Records)
			{
				GapTotalAll += Record.MeanGap;
				Minutes += Record.Duration / 60.0f;
			}

			auto DescribeSide = [this, Minutes](const TCHAR* Name, FSideRecord FFightRecord::* Side)
			{
				FGutFighterTally Total;
				int32 LimbsLost = 0;
				int32 Disarmed = 0;
				int32 HazardHits = 0;

				for (const FFightRecord& Record : Records)
				{
					const FSideRecord& Fought = Record.*Side;

					HazardHits += Fought.HazardHits;

					Total.Swings += Fought.Tally.Swings;
					Total.Hits += Fought.Tally.Hits;
					Total.Blocked += Fought.Tally.Blocked;
					Total.GuardBreaks += Fought.Tally.GuardBreaks;
					Total.Dodged += Fought.Tally.Dodged;

					LimbsLost += Fought.LimbsLost;
					Disarmed += Fought.bDisarmed ? 1 : 0;
				}

				const float Swings = FMath::Max(1, Total.Swings);

				return FString::Printf(TEXT("%s %.0f swings a minute, %.0f%% hit, %.0f%% blocked, %.2f guard breaks a fight, lost %.2f limbs a fight, disarmed in %.0f%% of fights, hurt by the cage %.2f times a fight"),
					Name, Total.Swings / FMath::Max(Minutes, 0.01f), 100.0f * Total.Hits / Swings, 100.0f * Total.Blocked / Swings,
					static_cast<float>(Total.GuardBreaks) / Records.Num(), static_cast<float>(LimbsLost) / Records.Num(), 100.0f * Disarmed / Records.Num(),
					static_cast<float>(HazardHits) / Records.Num());
			};

			const FString Text = FString::Printf(TEXT("BALANCE %s STYLE: fought %.0f cm apart on average; %s; %s"),
				*Matchup.Name, GapTotalAll / Records.Num(), *DescribeSide(TEXT("player"), &FFightRecord::Player), *DescribeSide(TEXT("opponent"), &FFightRecord::Enemy));

			UE_LOG(LogGut, Display, TEXT("%s"), *Text);

			Test->AddInfo(Text);
		}

		FAutomationTestBase* Test;
		FMatchup Matchup;
		int32 NumFights;

		/** What each fighter held at the bell of the current fight */
		TWeakObjectPtr<const UGutWeaponData> PlayerWeapon;
		TWeakObjectPtr<const UGutWeaponData> EnemyWeapon;

		/** Running total of the distance between the fighters in the current fight */
		float GapTotal = 0.0f;
		int32 GapSamples = 0;

		TWeakObjectPtr<AGutGameMode> CurrentGameMode;
		TArray<FFightRecord> Records;
		bool bRecorded = false;
		bool bNextRequested = false;
	};

	bool RunMatchup(FAutomationTestBase* Test, FMatchup Matchup)
	{
		if (!OpenCage(Test))
		{
			return false;
		}

		ADD_LATENT_AUTOMATION_COMMAND(FRunMatchupCommand(Test, MoveTemp(Matchup)));

		return true;
	}
}

namespace GutBalance
{
	/** Returns a setup that gives the player's fighter a second copy of the opponent's own brain */
	TFunction<void(const FFight&)> MakeMirrorSetup(FAutomationTestBase* Test)
	{
		TSharedRef<int32> FightNumber = MakeShared<int32>(0);

		return [Test, FightNumber](const FFight& Fight)
		{
			UClass* BrainClass = Fight.Enemy->GetController() ? Fight.Enemy->GetController()->GetClass() : nullptr;

			if (!BrainClass || !GiveBrain(Fight.Player, BrainClass))
			{
				Test->AddError(TEXT("Could not give the player's fighter the opponent's brain"));
				return;
			}

			// the two brains take their turns in the order they were made. The fighter rules are written so that the order
			// should not matter, but when it did, it was worth ten wins in a hundred. Swap the order every other fight,
			// so that if it ever matters again it cannot lean the results, and the per-fight log will show it.
			if ((*FightNumber)++ % 2 == 1)
			{
				GiveBrain(Fight.Enemy, BrainClass);
			}
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceMirrorTest, "Ashmarket.Balance.Opponent.Mirror", GutTests::Flags)

bool FGutBalanceMirrorTest::RunTest(const FString& Parameters)
{
	using namespace GutBalance;

	FMatchup Matchup;
	Matchup.Name = TEXT("Mirror");
	Matchup.Setup = MakeMirrorSetup(this);

	Matchup.Check = [this](const FSummary& Summary)
	{
		TestTrue(TEXT("The middle fight between two opponents lasts 60 to 90 seconds"), Summary.MedianDuration >= 60.0f && Summary.MedianDuration <= 90.0f);
		TestEqual(TEXT("Every fight between two opponents ends"), Summary.Timeouts, 0);

		// with the same brain on both sides, a lopsided result would mean the cage itself favours one corner
		const float PlayerShare = static_cast<float>(Summary.PlayerWins) / Summary.Fights;

		TestTrue(TEXT("A fight between two opponents can go either way"), PlayerShare >= 0.25f && PlayerShare <= 0.75f);
	};

	return RunMatchup(this, MoveTemp(Matchup));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceMasherTest, "Ashmarket.Balance.Opponent.Masher", GutTests::Flags)

bool FGutBalanceMasherTest::RunTest(const FString& Parameters)
{
	using namespace GutBalance;

	FMatchup Matchup;
	Matchup.Name = TEXT("Masher");

	// walk in and hammer light attack. Never guard, never dodge, never rest.
	Matchup.Drive = [](const FFight& Fight)
	{
		AGutFighter* Player = Fight.Player;

		if (Player->GetFighterState() != EGutFighterState::Idle)
		{
			return;
		}

		const FGutAttackDef* NextLight = Player->GetNextLightAttack();

		if (Player->GetGapTo(Fight.Enemy) > NextLight->Reach * 0.9f)
		{
			WalkToward(Player, Fight.Enemy);
		}
		else
		{
			Player->TryLightAttack();
		}
	};

	return RunMatchup(this, MoveTemp(Matchup));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceSharpTest, "Ashmarket.Balance.Opponent.Sharp", GutTests::Flags)

bool FGutBalanceSharpTest::RunTest(const FString& Parameters)
{
	using namespace GutBalance;

	FMatchup Matchup;
	Matchup.Name = TEXT("Sharp");

	// true from the moment the player defends against an attack until it answers with a heavy
	TSharedRef<bool> bOwedHeavy = MakeShared<bool>(false);

	Matchup.Setup = [bOwedHeavy](const FFight& Fight)
	{
		*bOwedHeavy = false;
	};

	// the scripted player that beat the dummy: reacts on the same frame, which no person can
	Matchup.Drive = [bOwedHeavy](const FFight& Fight)
	{
		AGutFighter* Player = Fight.Player;
		AGutFighter* Enemy = Fight.Enemy;

		const float Gap = Player->GetGapTo(Enemy);
		const FVector ToEnemy = (Enemy->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();

		const bool bEnemySwinging = Enemy->GetFighterState() == EGutFighterState::Windup || Enemy->GetFighterState() == EGutFighterState::Active;
		const bool bThreatened = bEnemySwinging && Gap <= Enemy->GetCurrentAttack().Reach + Enemy->GetCurrentAttack().LungeDistance + 80.0f;
		const bool bHeavyIncoming = bThreatened && Enemy->GetCurrentAttackType() == EGutAttackType::Heavy;

		// sidestep a heavy just before it lands
		if (bHeavyIncoming && Enemy->GetFighterState() == EGutFighterState::Windup && Enemy->GetStateTime() >= Enemy->GetCurrentAttack().Windup - 0.12f)
		{
			Player->SetBlocking(false);

			if (Player->TryDodge(FVector::CrossProduct(FVector::UpVector, ToEnemy)))
			{
				*bOwedHeavy = true;
			}

			return;
		}

		// guard against everything else
		Player->SetBlocking(bThreatened && !bHeavyIncoming);

		if (bThreatened)
		{
			*bOwedHeavy = true;
			return;
		}

		const FGutAttackDef* NextLight = Player->GetNextLightAttack();

		if (Gap > NextLight->Reach * 0.8f)
		{
			WalkToward(Player, Enemy);
		}
		else if (Player->GetFighterState() == EGutFighterState::Idle)
		{
			// a heavy is no answer to a guard that can shrug it off, which a shield's can
			const bool bHeavyBitesGuard = Enemy->GetBlockCost(Player->GetHeavyAttack()) > Enemy->GetMaxStamina() * 0.35f;

			// the enemy is hiding behind a guard it has not the stamina to hold against a heavy: break it.
			// Lights would only be blocked, so hold them back and save up for the heavy.
			const bool bGuardCanBeBroken = Enemy->IsBlocking() && Enemy->GetStamina() < Enemy->GetBlockCost(Player->GetHeavyAttack());

			if (bGuardCanBeBroken)
			{
				if (Player->GetStamina() >= Player->GetHeavyAttack().StaminaCost)
				{
					Player->TryHeavyAttack();
				}
			}
			else if (*bOwedHeavy && bHeavyBitesGuard && Player->GetStamina() >= 50.0f)
			{
				*bOwedHeavy = !Player->TryHeavyAttack();
			}
			else if (Player->GetStamina() >= 35.0f)
			{
				Player->TryLightAttack();
			}
		}
	};

	return RunMatchup(this, MoveTemp(Matchup));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceWeaponPairTest, "Ashmarket.Balance.Weapons.Pair", GutTests::Flags)

bool FGutBalanceWeaponPairTest::RunTest(const FString& Parameters)
{
	using namespace GutBalance;

	FString PlayerWeapon = TEXT("Club");
	FString EnemyWeapon = TEXT("Club");

	GetWeaponOverrides(PlayerWeapon, EnemyWeapon);

	// the same brain on both sides, so that whatever differs between two pairs is down to the weapons
	FMatchup Matchup;
	Matchup.Name = FString::Printf(TEXT("%s v %s"), *PlayerWeapon, *EnemyWeapon);
	Matchup.Setup = MakeMirrorSetup(this);
	Matchup.bReportStyle = true;

	// how the pairs compare is for whoever reads the table. All that is asked of each pair here is that its fights end.
	Matchup.Check = [this](const FSummary& Summary)
	{
		TestEqual(TEXT("Every fight ends"), Summary.Timeouts, 0);
	};

	return RunMatchup(this, MoveTemp(Matchup));
}

namespace GutBalance
{
	/**
	 *  Sends a stand-in for the player up the ladder over and over, through the same card, rack and betting window a player uses.
	 *  The stand-in is the opponent's own brain with its ordinary tuning. In each fight it draws the weapon the book fancies most
	 *  against the opponent's, has the cutter patch whatever it can pay for without the slate, takes any sponsor's offer,
	 *  buys the armor made against the opponent's weapon when it can pay for it, and stakes the smallest bet on itself.
	 *  A stand-in that dies starts the ladder again from the bottom: this measures the climb, not the retry.
	 *
	 *  By default a run ends at the gate after the first rank, which is the Meat arc. -GutFullLadder sends it on up through every rank.
	 *  -GutRetry has a stand-in that dies fight the same fight again, as a player would, so that every fight of the ladder gets measured.
	 */
	class FRunArcsCommand : public IAutomationLatentCommand
	{
	public:

		FRunArcsCommand(FAutomationTestBase* InTest)
			: Test(InTest)
			, NumArcs(GetNumFights())
			, bFullLadder(FParse::Param(FCommandLine::Get(), TEXT("GutFullLadder")))
			, bTakeSpecial(FParse::Param(FCommandLine::Get(), TEXT("GutTakeSpecial")))
			, bRetry(FParse::Param(FCommandLine::Get(), TEXT("GutRetry")))
		{
			// -GutLadderAt=2,4 fights one fight of the ladder over and over (the rank and the fight, both from 0),
			// coming to it whole each time with -GutLadderWater= to spend: to measure one fight without climbing to it
			FString At;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutLadderAt="), At, false))
			{
				FString Rank, Rung;

				if (At.Split(TEXT(","), &Rank, &Rung))
				{
					AtRank = FCString::Atoi(*Rank);
					AtRung = FCString::Atoi(*Rung);
				}
			}

			FParse::Value(FCommandLine::Get(), TEXT("GutLadderWater="), AtWater);
		}

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return false;
			}

			AGutGameMode* GameMode = Fight.GameMode;

			// a fresh cage
			if (GameMode != CurrentGameMode.Get())
			{
				CurrentGameMode = GameMode;
				bRecorded = false;
				bNextRequested = false;

				// the cage was opened for single fights, as every test opens it. The first thing to do is start the ladder.
				if (!bLadderStarted)
				{
					bLadderStarted = true;
					bNextRequested = true;

					GameMode->StartNewLadder();
				}

				return false;
			}

			if (bNextRequested || !GameMode->IsOnLadder())
			{
				return false;
			}

			const UGutLadderData* Ladder = GameMode->GetLadder();
			const FGutLadderState& State = GameMode->GetLadderState();

			// one fight asked for: the stand-in is put in front of it, whole, however the last one went
			if (AtRank >= 0 && GameMode->GetPhase() == EGutFightPhase::Card && !bRecorded && !bPlacedAt)
			{
				UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

				GameInstance->Ladder = FGutLadderState();
				GameInstance->Ladder.Rank = AtRank;
				GameInstance->Ladder.Rung = AtRung;
				GameInstance->Ladder.Water = AtWater;
				GameInstance->LadderAtRungStart = GameInstance->Ladder;

				bPlacedAt = true;

				// the cage was opened as whatever came before. Open it again as this fight.
				bNextRequested = true;

				UGameplayStatics::OpenLevel(GameMode, FName(*UGameplayStatics::GetCurrentLevelName(GameMode)), true);
				return false;
			}

			switch (GameMode->GetPhase())
			{
			case EGutFightPhase::Card:
			{
				// the cutter, for as long as there is water to pay with
				while (State.Water >= Ladder->PatchPrice && GameMode->PatchLimb())
				{
				}

				// and on the slate if that is what it takes to have an arm to hold a weapon with
				while (State.bInjuriesCarried && State.LimbHealth[static_cast<int32>(EGutLimb::LeftArm)] <= 0.0f && State.LimbHealth[static_cast<int32>(EGutLimb::RightArm)] <= 0.0f && GameMode->PatchLimb())
				{
				}

				// it takes every offer but the one to lose: it is here to climb
				const FGutSponsor* Offer = GameMode->GetSponsorOffer();

				GameMode->SetSponsorAccepted(Offer && Offer->Demand != EGutSponsorDemand::ThrowFight);

				// the pit boss's special cards are left alone unless -GutTakeSpecial is given: then every one offered is taken
				GameMode->SetSpecialAccepted(bTakeSpecial);

				// armor made against what the opponent is carrying, body first, while there is water for it
				const FGutLadderRung* Rung = GameMode->GetRung();
				const bool bBladed = Rung && Rung->Weapon && Rung->Weapon->LightAttacks.Num() > 0 && Rung->Weapon->LightAttacks[0].DamageType == EGutDamageType::Bladed;

				for (const EGutArmorSlot Slot : { EGutArmorSlot::Torso, EGutArmorSlot::Arms, EGutArmorSlot::Legs })
				{
					for (int32 i = 0; i < Ladder->ArmorShop.Num(); ++i)
					{
						const UGutArmorData* Piece = Ladder->ArmorShop[i];

						if (Piece && Piece->Slot == Slot && (Piece->BladedScale < Piece->BluntScale) == bBladed && State.Water >= Piece->Price)
						{
							GameMode->BuyArmor(i);
						}
					}
				}

				Current = GetStats(State.Rank, State.Rung, Ladder);

				GameMode->ConfirmCard();

				// measuring one fight: -GutTune= and -GutHealth= change the fighter on the card, and -GutRungNoArmor strips it,
				// to try another version of it without rebuilding the ladder
				if (AtRank >= 0)
				{
					ApplyTuningOverrides(Test, Fight.Enemy);
					ApplyHealthOverride(Fight.Enemy);

					if (FParse::Param(FCommandLine::Get(), TEXT("GutRungNoArmor")))
					{
						Fight.Enemy->ClearArmor();
					}

					// -GutEnemyHealth=90 gives the fighter on the card that much and leaves the stand-in as it is
					float EnemyHealth = 0.0f;

					if (FParse::Value(FCommandLine::Get(), TEXT("GutEnemyHealth="), EnemyHealth) && EnemyHealth > 0.0f && !Fight.Enemy->IsBeast())
					{
						Fight.Enemy->SetMaxHealth(EnemyHealth);
					}
				}
				break;
			}

			case EGutFightPhase::Choosing:
			{
				// draw whatever the book fancies most against the opponent's weapon, of what the stand-in can hold
				const TArray<TObjectPtr<UGutWeaponData>>& Weapons = GameMode->GetRackWeapons();

				int32 Best = INDEX_NONE;
				float BestChance = 0.0f;

				// what the opponent is wearing takes off each kind of blow: somebody who can read the card does not take a club to rubber
				const FGutLadderRung* Facing = GameMode->GetRung();

				float BluntGetsThrough = 1.0f;
				float BladedGetsThrough = 1.0f;

				if (Facing && Facing->Armor.Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("GutBlindToArmor")))
				{
					float Blunt = 0.0f;
					float Bladed = 0.0f;

					for (const UGutArmorData* Piece : Facing->Armor)
					{
						Blunt += Piece ? Piece->BluntScale : 1.0f;
						Bladed += Piece ? Piece->BladedScale : 1.0f;
					}

					// three places a piece can be worn: what is bare lets everything through
					BluntGetsThrough = (Blunt + FMath::Max(0, 3 - Facing->Armor.Num())) / FMath::Max(3, Facing->Armor.Num());
					BladedGetsThrough = (Bladed + FMath::Max(0, 3 - Facing->Armor.Num())) / FMath::Max(3, Facing->Armor.Num());
				}

				for (int32 i = 0; i < Weapons.Num(); ++i)
				{
					const bool bCuts = Weapons[i] && Weapons[i]->LightAttacks.Num() > 0 && Weapons[i]->LightAttacks[0].DamageType == EGutDamageType::Bladed;

					// the book's odds on the two weapons, marked down hard for a blow the opponent is dressed against: measured,
					// armor that stops three tenths of a blow costs far more than three tenths of the fights
					const float Through = bCuts ? BladedGetsThrough : BluntGetsThrough;
					const float Chance = Ladder->GetBookChance(State.Rung, Weapons[i], Fight.Enemy->GetWeapon(), 0) * FMath::Pow(Through, 3.0f);

					if (GameMode->CanPlayerHold(Weapons[i]) && Chance > BestChance)
					{
						Best = i;
						BestChance = Chance;
					}
				}

				// -GutStandInWeapon=Machete makes it draw that whatever the book thinks, to measure a fight weapon by weapon
				FString Forced;

				if (FParse::Value(FCommandLine::Get(), TEXT("GutStandInWeapon="), Forced))
				{
					for (int32 i = 0; i < Weapons.Num(); ++i)
					{
						if (Weapons[i] && Weapons[i]->GetName().Contains(Forced) && GameMode->CanPlayerHold(Weapons[i]))
						{
							Best = i;
						}
					}
				}

				for (int32 i = 0; i < Weapons.Num() && Best != INDEX_NONE && GameMode->GetPlayerChoice() != Best; ++i)
				{
					GameMode->CyclePlayerChoice(1);
				}

				GameMode->ConfirmChoice();
				break;
			}

			case EGutFightPhase::Betting:
			{
				GameMode->SetBet(EGutBetSide::Player, Ladder->BetStep);
				GameMode->ConfirmBet();

				// the stand-in gets a brain of the opponent's kind, with that kind's ordinary tuning
				UClass* BrainClass = Fight.Enemy->GetController() ? Fight.Enemy->GetController()->GetClass() : nullptr;

				if (!BrainClass || !GiveBrain(Fight.Player, BrainClass))
				{
					Test->AddError(TEXT("Could not give the player's fighter the opponent's brain"));
					return true;
				}

				ApplyHealthOverride(Fight.Player);

				if (FParse::Param(FCommandLine::Get(), TEXT("GutNoHazards")))
				{
					GameMode->SetHazardsEnabled(false);
				}

				Current->Crippled += Fight.Player->GetCrippledLimbCount();
				Current->Unarmed += Fight.Player->HasWeapon() ? 0 : 1;

				GameMode->StartFight();
				break;
			}

			case EGutFightPhase::Fighting:

				// nobody is going to win this one
				if (GameMode->GetFightTime() > MaxFightTime)
				{
					++Abandoned;

					UE_LOG(LogGut, Display, TEXT("BALANCE Ladder arc %d: %s abandoned"), Arcs + 1, *Current->Name);

					return EndArc(GameMode);
				}
				break;

			case EGutFightPhase::PlayerWon:
			case EGutFightPhase::PlayerLost:
			{
				const FGutSettlement& Settlement = GameMode->GetSettlement();

				if (!bRecorded)
				{
					bRecorded = true;

					++Current->Fought;
					Current->Won += Settlement.bPlayerWon ? 1 : 0;
					Current->Spared += Settlement.bSpared ? 1 : 0;
					Current->CrowdPaid += Settlement.bCrowdCallMatched ? 1 : 0;
					Current->SponsorPaid += Settlement.SponsorPay > 0.0f ? 1 : 0;
					Current->Durations.Add(GameMode->GetFightTime());

					UE_LOG(LogGut, Display, TEXT("BALANCE Ladder arc %d: %s %s in %.1f s"), Arcs + 1, *Current->Name,
						Settlement.bPlayerWon ? TEXT("won") : (Settlement.bSpared ? TEXT("lost, spared") : TEXT("lost, dead")), GameMode->GetFightTime());

					// one fight asked for: each time it is fought is a run, and the next cage is put back in front of it
					if (AtRank >= 0)
					{
						bPlacedAt = false;

						if (++Arcs >= NumArcs)
						{
							Finish();
							return true;
						}
					}

					// dead: this run at the ladder ends here, unless it is one that fights a lost fight again as a player would
					if (!Settlement.bPlayerWon && !Settlement.bSpared && !bRetry)
					{
						return EndArc(GameMode);
					}
				}

				if (GameMode->CanRestart())
				{
					bNextRequested = true;

					GameMode->RequestRestart();
				}
				break;
			}

			case EGutFightPhase::Gate:

				// the end of the Meat arc is worth its own count: what the stand-in held, and what it owed
				if (State.Rank == 0)
				{
					++AtMeatGate;

					MeatGateWater += State.Water;
					MeatGateDebt += Ladder->GetGateDebt(State.Water, State.bSpared);
				}

				if (bFullLadder && Ladder->NextLadder)
				{
					bNextRequested = true;

					GameMode->LeaveGate();
					break;
				}

				// the top, or as far as this run was asked to go
				if (!Ladder->NextLadder)
				{
					++ReachedTop;

					TopWater += State.Water;
					TopDebt += State.Debt;
					TopName += State.Name;
				}

				return EndArc(GameMode);

			default:
				break;
			}

			return false;
		}

	private:

		/** What happened in one fight of the ladder, over every run that reached it */
		struct FFightStats
		{
			FString Name;
			int32 Fought = 0;
			int32 Won = 0;
			int32 Spared = 0;
			int32 Crippled = 0;
			int32 Unarmed = 0;
			int32 CrowdPaid = 0;
			int32 SponsorPaid = 0;
			TArray<float> Durations;
		};

		FFightStats* GetStats(int32 Rank, int32 Rung, const UGutLadderData* Ladder)
		{
			const int32 Key = (Rank * 100) + Rung;

			if (!Stats.Contains(Key))
			{
				FFightStats& New = Stats.Add(Key);

				New.Name = FString::Printf(TEXT("%s fight %d (%s)"), *Ladder->RankName.ToString(), Rung + 1,
					Ladder->Rungs.IsValidIndex(Rung) ? *Ladder->Rungs[Rung].OpponentName.ToString() : TEXT("?"));
			}

			return &Stats[Key];
		}

		/** Counts the run, and either starts the next or reports. Returns true when all the runs are done. */
		bool EndArc(AGutGameMode* GameMode)
		{
			++Arcs;

			if (Arcs >= NumArcs)
			{
				Finish();
				return true;
			}

			bNextRequested = true;

			GameMode->StartNewLadder();

			return false;
		}

		void Finish()
		{
			FString Text = FString::Printf(
				TEXT("BALANCE Ladder SUMMARY: %d runs, %d abandoned. Reached the Meat gate: %d, with %.0f L in hand and %.0f L owed on average."),
				Arcs, Abandoned, AtMeatGate, MeatGateWater / FMath::Max(1, AtMeatGate), MeatGateDebt / FMath::Max(1, AtMeatGate));

			if (bFullLadder)
			{
				Text += FString::Printf(TEXT(" Reached the top: %d, with %.0f L in hand, %.0f L still owed and a name of %.0f on average."),
					ReachedTop, TopWater / FMath::Max(1, ReachedTop), TopDebt / FMath::Max(1, ReachedTop), TopName / FMath::Max(1, ReachedTop));
			}

			UE_LOG(LogGut, Display, TEXT("%s"), *Text);

			Test->AddInfo(Text);

			Stats.KeySort(TLess<int32>());

			for (TPair<int32, FFightStats>& Pair : Stats)
			{
				FFightStats& Fight = Pair.Value;

				if (Fight.Fought == 0)
				{
					continue;
				}

				Fight.Durations.Sort();

				Text = FString::Printf(TEXT("BALANCE Ladder FIGHT %d: %s: fought %d, won %d (%.0f%%), spared %d, median %.0f s, %.2f limbs crippled coming in, unarmed %d times, crowd paid %d, sponsor paid %d"),
					Pair.Key, *Fight.Name, Fight.Fought, Fight.Won, 100.0f * Fight.Won / Fight.Fought, Fight.Spared, Fight.Durations[Fight.Durations.Num() / 2],
					static_cast<float>(Fight.Crippled) / Fight.Fought, Fight.Unarmed, Fight.CrowdPaid, Fight.SponsorPaid);

				UE_LOG(LogGut, Display, TEXT("%s"), *Text);

				Test->AddInfo(Text);
			}

			Test->TestEqual(TEXT("Every ladder fight ends"), Abandoned, 0);
		}

		FAutomationTestBase* Test;
		int32 NumArcs;
		bool bFullLadder;
		bool bTakeSpecial;

		/** The one fight to measure, if one was asked for, and what the stand-in comes to it holding */
		int32 AtRank = INDEX_NONE;
		int32 AtRung = 0;
		float AtWater = 60.0f;
		bool bPlacedAt = false;

		/** If true a stand-in that dies fights the same fight again, as a player would, and so every run reaches the end */
		bool bRetry;

		TWeakObjectPtr<AGutGameMode> CurrentGameMode;
		bool bLadderStarted = false;
		bool bRecorded = false;
		bool bNextRequested = false;

		/** The fight the stand-in is in */
		FFightStats* Current = nullptr;

		/** Every fight of the ladder any run has reached, by rank and rung */
		TMap<int32, FFightStats> Stats;

		int32 Arcs = 0;
		int32 Abandoned = 0;

		int32 AtMeatGate = 0;
		float MeatGateWater = 0.0f;
		float MeatGateDebt = 0.0f;

		int32 ReachedTop = 0;
		float TopWater = 0.0f;
		float TopDebt = 0.0f;
		float TopName = 0.0f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutBalanceLadderArcTest, "Ashmarket.Balance.Ladder.Arc", GutTests::Flags)

bool FGutBalanceLadderArcTest::RunTest(const FString& Parameters)
{
	using namespace GutBalance;

	if (!OpenCage(this))
	{
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FRunArcsCommand(this));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
