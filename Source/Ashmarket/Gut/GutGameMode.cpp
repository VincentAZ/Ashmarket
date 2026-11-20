// Ashmarket prototype: the Gut.

#include "GutGameMode.h"
#include "GutSound.h"
#include "EngineUtils.h"
#include "GutArmorData.h"
#include "GutBeast.h"
#include "GutCrowd.h"
#include "GutDogs.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "GutHazard.h"
#include "GutHUD.h"
#include "GutWeaponData.h"
#include "GutWeaponRack.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AudioDevice.h"

namespace GutGameModeOptions
{
	/** Level options naming the weapons to start with */
	const TCHAR* PlayerWeapon = TEXT("PlayerWeapon");
	const TCHAR* EnemyWeapon = TEXT("EnemyWeapon");

	/** Level option that switches the hazards off: Hazards=0 */
	const TCHAR* Hazards = TEXT("Hazards");

	/** Level option that opens the cage for single fights, with no ladder: Sandbox=1. Sandbox=0 goes back to the ladder. */
	const TCHAR* Sandbox = TEXT("Sandbox");

	/** Level option for single fights: Special=3 is one against three, Special=2 is two on two */
	const TCHAR* Special = TEXT("Special");

	/** Level option that starts outside, with the choice of coming in to fight, to own or to bet: Door=1 */
	const TCHAR* Door = TEXT("Door");

	/** Level option that comes to the cage to bet on its fights and not to fight them: Window=1 */
	const TCHAR* Window = TEXT("Window");

	/** Level option that comes to the cage as an owner of fighters: Stable=1 */
	const TCHAR* Stable = TEXT("Stable");

	/** Level option that forgets the run on disk and starts the ladder from the bottom: NewRun=1 */
	const TCHAR* NewRun = TEXT("NewRun");
}

AGutGameMode::AGutGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	HUDClass = AGutHUD::StaticClass();
}

void AGutGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// -GutFrameRate on the command line writes the frame rate to the log every five seconds of real time
	static const bool bLogFrameRate = FParse::Param(FCommandLine::Get(), TEXT("GutFrameRate"));

	if (bLogFrameRate)
	{
		static double Since = FPlatformTime::Seconds();
		static int32 Frames = 0;
		static double Slowest = 0.0;
		static double LastFrame = FPlatformTime::Seconds();

		const double Now = FPlatformTime::Seconds();

		Slowest = FMath::Max(Slowest, Now - LastFrame);
		LastFrame = Now;
		++Frames;

		if (Now - Since >= 5.0)
		{
			UE_LOG(LogGut, Log, TEXT("Frame rate: %.0f a second, slowest frame %.0f ms"), Frames / (Now - Since), Slowest * 1000.0);

			Since = Now;
			Frames = 0;
			Slowest = 0.0;
		}
	}

	// the player's fighter may not exist on the very first frame
	if (!bFightersReady)
	{
		bFightersReady = FindFighters();

		if (!bFightersReady)
		{
			return;
		}
	}

	PhaseTime += DeltaTime;

	if (Phase == EGutFightPhase::Fighting)
	{
		FightTime = PhaseTime;

		UpdateHazards();

		if (SpecialCard != EGutSpecialCard::None || bDogsLoose)
		{
			UpdateSpecialTargets();
		}
	}

	if (Phase == EGutFightPhase::Intro && PhaseTime >= IntroDuration)
	{
		StartFight();
	}

	// somebody who came to bet, or to watch what it owns, watches from the wall
	if (IsAtWindow() || IsAtStable() || IsAtDoor())
	{
		UpdateSpectatorCamera(DeltaTime);
	}

	// and an owner sees who it owns, standing in the yard
	if (IsAtStable() || !YardBodies.IsEmpty())
	{
		UpdateYardBodies();
	}
}

void AGutGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// the pit boss's dogs, where the game mode's own settings do not name any: -GutNoDogs leaves the cage without them
	if (!Dog && !FParse::Param(FCommandLine::Get(), TEXT("GutNoDogs")))
	{
		Dog = LoadObject<UGutBeastData>(nullptr, TEXT("/Game/Gut/Beast/DA_Gut_Dog.DA_Gut_Dog"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("GutNoDogs")))
	{
		Dog = nullptr;
	}

	// nobody is told a story in a run that nobody is watching, unless it asks for them
	bStories = !FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("GutStory"));

	// -GutDogsRound=2 lets them in on a fight sooner than the pit boss would, to look at it, and -GutRoundSeconds=12 makes every
	// round that long, so that everything the pit boss has is switched on inside a minute
	FParse::Value(FCommandLine::Get(), TEXT("GutDogsRound="), DogsRound);
	FParse::Value(FCommandLine::Get(), TEXT("GutRoundSeconds="), RoundLength);

	RequestedPlayerWeapon = UGameplayStatics::ParseOption(Options, GutGameModeOptions::PlayerWeapon);
	RequestedEnemyWeapon = UGameplayStatics::ParseOption(Options, GutGameModeOptions::EnemyWeapon);

	if (UGameplayStatics::HasOption(Options, GutGameModeOptions::Hazards))
	{
		bHazardsEnabled = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Hazards, 1) != 0;
	}

	if (UGameplayStatics::HasOption(Options, GutGameModeOptions::Sandbox))
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			GameInstance->bSandbox = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Sandbox, 1) != 0;
		}
	}

	// Window=1 comes to the cage to bet. Window=0 goes back to fighting.
	if (UGameplayStatics::HasOption(Options, GutGameModeOptions::Window))
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			GameInstance->bWindow = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Window, 1) != 0;

			if (GameInstance->bWindow)
			{
				GameInstance->bSandbox = false;
			}
		}
	}

	if (UGameplayStatics::HasOption(Options, GutGameModeOptions::Stable))
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			GameInstance->bStable = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Stable, 1) != 0;

			if (GameInstance->bStable)
			{
				GameInstance->bSandbox = false;
				GameInstance->bWindow = false;
			}
		}
	}

	RequestedSpecial = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Special, 0);

	if (UGameplayStatics::HasOption(Options, GutGameModeOptions::Door))
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			GameInstance->bAtDoor = UGameplayStatics::GetIntOption(Options, GutGameModeOptions::Door, 1) != 0;
			GameInstance->bCameByDoor = GameInstance->bAtDoor;
		}
	}

	if (UGameplayStatics::GetIntOption(Options, GutGameModeOptions::NewRun, 0) != 0 && Ladder)
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			GameInstance->ResetLadder(Ladder->StartingWater);
		}
	}
}

UGutGameInstance* AGutGameMode::GetGutGameInstance() const
{
	return Cast<UGutGameInstance>(GetGameInstance());
}

bool AGutGameMode::IsOnLadder() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return Ladder && Ladder->Rungs.Num() > 0 && GameInstance && !GameInstance->bSandbox && !GameInstance->bWindow && !GameInstance->bStable && !GameInstance->bAtDoor;
}

const UGutLadderData* AGutGameMode::GetLadder() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Current = Ladder;

	// each rank is reached through the one before it
	for (int32 i = 0; Current && GameInstance && i < GameInstance->Ladder.Rank && Current->NextLadder; ++i)
	{
		Current = Current->NextLadder;
	}

	return Current;
}

const FGutLadderState& AGutGameMode::GetLadderState() const
{
	static const FGutLadderState Empty;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance ? GameInstance->Ladder : Empty;
}

int32 AGutGameMode::GetLadderFightNumber() const
{
	return IsOnLadder() ? FMath::Min(GetLadderState().Rung + 1, GetLadder()->Rungs.Num()) : 0;
}

float AGutGameMode::GetWaterInHand() const
{
	return IsOnLadder() ? GetLadderState().Water : 0.0f;
}

const FGutLadderRung* AGutGameMode::GetRung() const
{
	if (!IsOnLadder())
	{
		return nullptr;
	}

	const int32 Rung = GetLadderState().Rung;

	// once a fight is settled the state has moved on, but this fight is still the one it was
	const int32 ThisRung = (Settlement.bSettled && Settlement.bPlayerWon) ? Rung - 1 : Rung;

	// the fight's own fighter, or whoever has the place of one who has died since
	return GetLadderFight(ThisRung);
}

void AGutGameMode::HandleMenuInput(EGutMenuInput Input)
{
	if (!bFightersReady)
	{
		return;
	}

	// stopped, the menu that stopped it is the only thing listening
	if (bStoppedMenu)
	{
		return;
	}

	// a story being told has the screen to itself
	if (IsTellingStory())
	{
		if (Input == EGutMenuInput::Confirm || Input == EGutMenuInput::Right)
		{
			AdvanceStory();
		}

		return;
	}

	if (Input == EGutMenuInput::Confirm)
	{
		UE_LOG(LogGut, Log, TEXT("Menu: yes pressed at %s, with the choice on line %d of kind %d"), *UEnum::GetDisplayValueAsText(Phase).ToString(), CardSelection.Index, static_cast<int32>(CardSelection.Type));
	}

	switch (Phase)
	{
	case EGutFightPhase::Card:

		// up and down move along the card's lines
		if (Input == EGutMenuInput::Up)
		{
			MoveCardSelection(-1);
		}
		else if (Input == EGutMenuInput::Down)
		{
			MoveCardSelection(1);
		}
		else if (Input == EGutMenuInput::Confirm || Input == EGutMenuInput::Right)
		{
			ActivateCardSelection();
		}
		else if (Input == EGutMenuInput::Left && IsAtStable())
		{
			// back a page
			ActivateStableLine(GutStableLines::Back);
		}
		break;

	case EGutFightPhase::Choosing:

		if (Input == EGutMenuInput::Left)
		{
			CyclePlayerChoice(-1);
		}
		else if (Input == EGutMenuInput::Right)
		{
			CyclePlayerChoice(1);
		}
		else if (Input == EGutMenuInput::Up)
		{
			CycleEnemyChoice(1);
		}
		else if (Input == EGutMenuInput::Down && CameByDoor() && !IsOnLadder() && !IsAtWindow() && !IsAtStable())
		{
			// a single fight has no card to leave from: the rack is where it is walked away from
			GoToDoor();
		}
		else if (Input == EGutMenuInput::Confirm)
		{
			ConfirmChoice();
		}
		break;

	case EGutFightPhase::Betting:
	{
		// left and right move between no bet, a bet on the player and a bet against it. Up and down move the stake, a litre
		// at a time, whether or not it is on anybody yet: how much is the bettor's to say, down to the litre.
		const int32 Side = static_cast<int32>(BetSide);
		const float Stake = GetBetWanted();

		// somebody watching has each name on the side its fighter stands on, with no bet between them: left is the one on the left
		const bool bTurned = (IsAtWindow() || IsAtStable()) && !bFirstCornerOnRight;

		if (Input == EGutMenuInput::Left)
		{
			BetWanted = Stake;
			SetBet(static_cast<EGutBetSide>((Side + (bTurned ? 1 : 2)) % 3), Stake);
		}
		else if (Input == EGutMenuInput::Right)
		{
			BetWanted = Stake;
			SetBet(static_cast<EGutBetSide>((Side + (bTurned ? 2 : 1)) % 3), Stake);
		}
		else if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			BetWanted = FMath::Clamp(Stake + (Input == EGutMenuInput::Up ? StakeStep : -StakeStep), FMath::Min(StakeStep, GetMaxStake()), GetMaxStake());
			SetBet(BetSide, BetWanted);
		}
		else if (Input == EGutMenuInput::Confirm)
		{
			ConfirmBet();
		}
		break;
	}

	case EGutFightPhase::PlayerWon:
	case EGutFightPhase::PlayerLost:

		// whoever only watched has two ways on from here: the next fight, or the street
		if (CanLeaveForStreet() && (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down))
		{
			bLeaveChosen = !bLeaveChosen;
		}
		break;

	case EGutFightPhase::Gate:

		// a gambler's night is over: tomorrow night's bill, or the street
		if (CanLeaveForStreet() && (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down))
		{
			bLeaveChosen = !bLeaveChosen;
		}

		if (Input == EGutMenuInput::Confirm)
		{
			if (IsAtWindow())
			{
				LeaveWindowNight();
			}
			else
			{
				LeaveGate();
			}
		}
		break;

	default:
		break;
	}
}

bool AGutGameMode::IsAtDoor() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return Ladder && GameInstance && GameInstance->bAtDoor;
}

bool AGutGameMode::CameByDoor() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance && GameInstance->bCameByDoor && !GameInstance->bAtDoor;
}

void AGutGameMode::EnterBy(int32 Way)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!IsAtDoor() || Phase != EGutFightPhase::Card)
	{
		return;
	}

	const bool bAfresh = Way >= GutDoorLines::Afresh;

	if (bAfresh)
	{
		Way -= GutDoorLines::Afresh;
	}

	if (Way < GutDoorLines::Fight || Way > GutDoorLines::Spar)
	{
		return;
	}

	GameInstance->bAtDoor = false;
	GameInstance->bSandbox = Way == GutDoorLines::Spar;
	GameInstance->bStable = Way == GutDoorLines::Own;
	GameInstance->bWindow = Way == GutDoorLines::Bet;

	if (bAfresh)
	{
		switch (Way)
		{
		case GutDoorLines::Fight:
			GameInstance->ResetLadder(Ladder->StartingWater);
			break;

		case GutDoorLines::Own:
			GameInstance->Stable = FGutStableState();
			break;

		case GutDoorLines::Bet:
			GameInstance->Window = FGutWindowState();
			break;

		default:
			break;
		}

		GameInstance->SaveRun();
	}

	UE_LOG(LogGut, Log, TEXT("Door: in to %s%s"), Way == GutDoorLines::Fight ? TEXT("fight") : (Way == GutDoorLines::Own ? TEXT("own") : (Way == GutDoorLines::Bet ? TEXT("bet") : TEXT("spar"))),
		bAfresh ? TEXT(", from the start") : TEXT(""));

	// the cage is opened afresh as whatever it now is
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, bHazardsEnabled ? FString() : FString::Printf(TEXT("%s=0"), GutGameModeOptions::Hazards));
}

void AGutGameMode::GoToDoor()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	// only from a card: that is where a run is written down, and nothing is half done
	if (!GameInstance || GameInstance->bAtDoor || (Phase != EGutFightPhase::Card && !(GameInstance->bSandbox && Phase == EGutFightPhase::Choosing)))
	{
		return;
	}

	// the street is a place now, where it has been built
	if (GameInstance->IsStreetOpen())
	{
		UE_LOG(LogGut, Log, TEXT("Out to the street"));

		GameInstance->GoToStreet(this);
		return;
	}

	GameInstance->SaveRun();

	GameInstance->bAtDoor = true;
	GameInstance->bCameByDoor = true;
	GameInstance->bSandbox = false;
	GameInstance->bWindow = false;
	GameInstance->bStable = false;

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, bHazardsEnabled ? FString() : FString::Printf(TEXT("%s=0"), GutGameModeOptions::Hazards));
}

void AGutGameMode::EnterSandbox()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || GameInstance->bSandbox)
	{
		return;
	}

	const bool bWasAtWindow = GameInstance->bWindow || GameInstance->bStable || GameInstance->bAtDoor;

	GameInstance->bSandbox = true;
	GameInstance->bWindow = false;
	GameInstance->bStable = false;
	GameInstance->bAtDoor = false;
	GameInstance->bCameByDoor = false;

	// at the window the two in the cage have already been made into somebody: the cage is opened afresh for single fights
	if (bWasAtWindow)
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true);
		return;
	}

	// nothing has been done to the fighters yet at the card or the gate, so the cage can go straight on as a single fight
	if (bFightersReady && (Phase == EGutFightPhase::Card || Phase == EGutFightPhase::Gate))
	{
		if (RackWeapons.IsEmpty())
		{
			SetPhase(EGutFightPhase::Intro);
		}
		else
		{
			BeginChoosing();
		}
	}
}

void AGutGameMode::StartNewLadder()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !Ladder)
	{
		return;
	}

	GameInstance->bSandbox = false;
	GameInstance->ResetLadder(Ladder->StartingWater);

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, bHazardsEnabled ? FString() : FString::Printf(TEXT("%s=0"), GutGameModeOptions::Hazards));
}

void AGutGameMode::LeaveGate()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Rank = GetLadder();

	if (Phase != EGutFightPhase::Gate || !GameInstance || !Rank)
	{
		return;
	}

	// the top of the ladder: there is nowhere to go but back to the bottom
	if (!Rank->NextLadder)
	{
		StartNewLadder();
		return;
	}

	FGutLadderState& State = GameInstance->Ladder;

	// the pit boss takes what the player is holding against his bill, and writes down the rest
	if (Rank->bBillAtGate)
	{
		State.Debt += Rank->GetGateDebt(State.Water, State.bSpared);
		State.Water = 0.0f;
	}

	// on to the next rank, with its own count of fights and its own sums. Injuries, name, armor and debt come along.
	++State.Rank;

	State.Rung = 0;
	State.bFinished = false;
	State.bSpared = false;
	State.Purses = 0.0f;
	State.BettingNet = 0.0f;
	State.Extras = 0.0f;
	State.PatchSpend = 0.0f;
	State.ArmorSpend = 0.0f;
	State.DebtPaid = 0.0f;

	GameInstance->LadderAtRungStart = State;
	GameInstance->SaveRun();

	// out of Meat with a tag: the first thing anybody does with one is walk out of the door
	if (State.Rank == 1 && GameInstance->IsStreetOpen())
	{
		UE_LOG(LogGut, Log, TEXT("Ladder: out of the gate and into the street"));

		GameInstance->GoToStreet(this);
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, bHazardsEnabled ? FString() : FString::Printf(TEXT("%s=0"), GutGameModeOptions::Hazards));
}

void AGutGameMode::BeginCard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	// the first time the cage is opened, the player is at the foot of the ladder
	if (!GameInstance->bLadderStarted)
	{
		GameInstance->ResetLadder(Ladder->StartingWater);
	}

	// and is told how it got there, once, whichever way the run was begun
	if (GameInstance->PendingStory == EGutStory::Sold)
	{
		TellStory(GameInstance->Ladder.Rank == 0 && GameInstance->Ladder.Rung == 0 ? EGutStory::Sold : EGutStory::None);

		GameInstance->PendingStory = EGutStory::None;
	}

	// for anybody the cage has lost since, somebody new is in it: one of them has the place of a fight whose own fighter is dead
	RefillRoster();

	// a fight that is lost is fought again from here, and a game that is closed comes back here
	GameInstance->LadderAtRungStart = GameInstance->Ladder;
	GameInstance->SaveRun();

	UE_LOG(LogGut, Log, TEXT("Ladder: %s fight %d of %d, %.0f L in hand"), *GetLadder()->RankName.ToString(), GameInstance->Ladder.Rung + 1, GetLadder()->Rungs.Num(), GameInstance->Ladder.Water);

	SetPhase(EGutFightPhase::Card);
}

void AGutGameMode::ConfirmCard()
{
	const FGutLadderRung* Rung = GetRung();

	if (Phase != EGutFightPhase::Card || !bFightersReady || !Rung)
	{
		return;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("card"), FVector::ZeroVector);
	}

	// the one fighter standing in the cage becomes this rung's opponent
	EnemyFighter->SetDisplayName(Rung->OpponentName);
	EnemyFighter->SetMaxHealth(Rung->Health);
	EnemyFighter->SetBodyTint(Rung->BodyTint);

	if (!Rung->Body.IsNull())
	{
		EnemyFighter->SetBody(Rung->Body, EnemyFighter->GetBodyRetargeter());
	}

	if (AGutOpponentController* Brain = Cast<AGutOpponentController>(EnemyFighter->GetController()))
	{
		if (Rung->bOverrideTuning)
		{
			Brain->SetTuning(Rung->Tuning);
		}
	}

	for (UGutArmorData* Piece : Rung->Armor)
	{
		EnemyFighter->WearArmor(Piece);
	}

	// the collar comes off with the tag, which is what the first rank is fought for. Above it, everyone in the cage chose to be there.
	const bool bCollars = GetLadderState().Rank == 0;

	PlayerFighter->SetCollared(bCollars);
	EnemyFighter->SetCollared(bCollars);

	const FGutLadderState& State = GetLadderState();
	const FGutSponsor* Sponsor = bSponsorAccepted ? GetSponsorOffer() : nullptr;

	// a fighter in a sponsor's colors wears them and nothing over them
	if (Sponsor && Sponsor->Demand == EGutSponsorDemand::WearColors)
	{
		PlayerFighter->SetBodyTint(Sponsor->Colors);
	}
	else
	{
		for (UGutArmorData* Piece : GetLadder()->ArmorShop)
		{
			if (IsArmorWorn(Piece))
			{
				PlayerFighter->WearArmor(Piece);
			}
		}
	}

	// what the player has taken, what it needs and has not had, and what its heart has already paid
	if (AreDrugsSold() || State.HeartDamage > 0.0f)
	{
		float Health = PlayerFighter->GetMaxHealth() - State.HeartDamage;
		float Speed = 1.0f;
		float Hand = 1.0f;

		if (IsDrugTaken(EGutDrug::Dust))
		{
			Speed = DustSpeedScale;
			Hand = DustDamageScale;
		}
		else if (IsInWithdrawal(EGutDrug::Dust))
		{
			Speed = DustWithdrawalScale;
			Hand = DustWithdrawalScale;
		}

		if (IsInWithdrawal(EGutDrug::Hush))
		{
			Health *= HushWithdrawalHealthScale;
		}

		PlayerFighter->SetMaxHealth(FMath::Max(Health, 20.0f));
		PlayerFighter->SetMoveSpeedScale(Speed);
		PlayerFighter->SetDamageScale(Hand);
		PlayerFighter->SetStunScale(IsDrugTaken(EGutDrug::Hush) ? HushStunScale : 1.0f);
	}

	// bread and tea from the market, taken since the last fight: wind that comes back faster, and sooner
	PlayerFighter->SetStaminaRegenScale(State.bFed ? FedStaminaRegenScale : 1.0f);
	PlayerFighter->SetStaminaRegenDelayScale(State.bSteadied ? SteadiedRegenDelayScale : 1.0f);

	// the player comes to the fight in the state the last one left it. Empty its hands first:
	// an arm that arrives crippled has nothing to drop.

	if (State.bInjuriesCarried)
	{
		PlayerFighter->EquipWeapon(nullptr);

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			PlayerFighter->SetLimbHealth(static_cast<EGutLimb>(i), State.LimbHealth[i]);
		}
	}

	// the pit boss's special card, if the player took it: the others are brought in now, before the rack and the window
	if (bSpecialAccepted && GetSpecialOffer() != EGutSpecialCard::None)
	{
		TArray<const UGutLadderData*> Ranks;
		GetRanks(Ranks);

		// the others are new meat, as the bottom of the ladder is; the second on each side of two on two fights as the first on this rank does
		const FGutLadderRung* Meat = (Ranks.Num() > 0 && Ranks[0]->Rungs.Num() > 0) ? &Ranks[0]->Rungs[0] : nullptr;

		BeginSpecial(GetSpecialOffer(), Meat, GetLadder()->Rungs.Num() > 0 ? &GetLadder()->Rungs[0] : nullptr);
	}
	else
	{
		// somebody who has dogs of its own brings them in whatever the player took or left
		int32 Heel = Rung->Dogs;

		FParse::Value(FCommandLine::Get(), TEXT("GutRungDogs="), Heel);

		if (Heel > 0)
		{
			BeginSpecial(EGutSpecialCard::WithDogs);
		}
	}

	if (RackWeapons.IsEmpty())
	{
		SetBet(EGutBetSide::None, 0.0f);
		SetPhase(EGutFightPhase::Betting);
	}
	else
	{
		BeginChoosing();
	}
}

TArray<FGutCardItem> AGutGameMode::GetCardItems() const
{
	TArray<FGutCardItem> Items;

	const UGutLadderData* Rank = GetLadder();

	// outside: the ways in, and under each that has been used before, the way to start it over
	if (IsAtDoor())
	{
		const UGutGameInstance* GameInstance = GetGutGameInstance();

		const bool bBegun[] = { GameInstance->bLadderStarted, GameInstance->Stable.bStarted, GameInstance->Window.bStarted };

		for (const int32 Way : { GutDoorLines::Fight, GutDoorLines::Own, GutDoorLines::Bet })
		{
			Items.Add({ EGutCardItemType::Door, Way });

			if (bBegun[Way])
			{
				Items.Add({ EGutCardItemType::Door, GutDoorLines::Afresh + Way });
			}
		}

		Items.Add({ EGutCardItemType::Door, GutDoorLines::Spar });

		return Items;
	}

	// in the owner's yard: the lines of whichever page is up
	if (IsAtStable())
	{
		GetStableCardItems(Items);

		// and from the yard itself, the street
		if (CameByDoor() && StablePage == EGutStablePage::Yard)
		{
			Items.Add({ EGutCardItemType::Door, GutDoorLines::Leave });
		}

		return Items;
	}

	// at the window: the way on to the betting first, since that is what anybody came for, then each way of fixing the
	// fight, for each corner, and last the way out
	if (IsAtWindow())
	{
		Items.Add({ EGutCardItemType::Rack, 0 });

		for (const EGutFix Fix : { EGutFix::Bribe, EGutFix::DrugBucket, EGutFix::TamperRack })
		{
			for (int32 Corner = 0; Corner < 2; ++Corner)
			{
				Items.Add({ EGutCardItemType::Fix, (static_cast<int32>(Fix) * 2) + Corner });
			}
		}

		if (CameByDoor())
		{
			Items.Add({ EGutCardItemType::Door, GutDoorLines::Leave });
		}

		return Items;
	}

	if (!IsOnLadder() || !Rank)
	{
		return Items;
	}

	const FGutLadderState& State = GetLadderState();

	// the cutter, while there is a crippled limb to patch
	bool bCrippled = false;

	for (int32 i = 0; i < GutNumLimbs; ++i)
	{
		bCrippled |= State.bInjuriesCarried && State.LimbHealth[i] <= 0.0f;
	}

	if (bCrippled)
	{
		Items.Add({ EGutCardItemType::Patch, 0 });
	}

	if (GetSponsorOffer())
	{
		Items.Add({ EGutCardItemType::Sponsor, 0 });
	}

	if (GetSpecialOffer() != EGutSpecialCard::None)
	{
		Items.Add({ EGutCardItemType::Special, 0 });
	}

	for (int32 i = 0; i < Rank->ArmorShop.Num(); ++i)
	{
		if (Rank->ArmorShop[i])
		{
			Items.Add({ EGutCardItemType::Armor, i });
		}
	}

	// Dust and Hush are legal in the ring, for anybody with a tag and the water
	if (AreDrugsSold())
	{
		Items.Add({ EGutCardItemType::Drug, static_cast<int32>(EGutDrug::Dust) });
		Items.Add({ EGutCardItemType::Drug, static_cast<int32>(EGutDrug::Hush) });
	}

	if (CameByDoor())
	{
		Items.Add({ EGutCardItemType::Door, GutDoorLines::Leave });
	}

	Items.Add({ EGutCardItemType::Rack, 0 });

	return Items;
}

void AGutGameMode::MoveCardSelection(int32 Step)
{
	if (Phase != EGutFightPhase::Card)
	{
		return;
	}

	const TArray<FGutCardItem> Items = GetCardItems();

	if (Items.IsEmpty())
	{
		return;
	}

	// a line that has gone, like the cutter's once nothing is crippled, leaves the choice on the way on, wherever on the card that is
	int32 Index = Items.IndexOfByKey(CardSelection);

	if (Index == INDEX_NONE)
	{
		Index = Items.IndexOfByPredicate([](const FGutCardItem& Item) { return Item.Type == EGutCardItemType::Rack; });
	}

	if (Index == INDEX_NONE)
	{
		Index = Items.Num() - 1;
	}

	CardSelection = Items[FMath::Clamp(Index + Step, 0, Items.Num() - 1)];
}

void AGutGameMode::ActivateCardSelection()
{
	if (Phase != EGutFightPhase::Card)
	{
		return;
	}

	const TArray<FGutCardItem> Items = GetCardItems();

	if (!Items.Contains(CardSelection))
	{
		// the owner's card and the street have no one way out for the choice to rest on: it rests on the first line
		CardSelection = ((IsAtStable() || IsAtDoor()) && !Items.IsEmpty()) ? Items[0] : FGutCardItem();
	}

	switch (CardSelection.Type)
	{
	case EGutCardItemType::Patch:
		PatchLimb();
		break;

	case EGutCardItemType::Sponsor:
		SetSponsorAccepted(!bSponsorAccepted);
		break;

	case EGutCardItemType::Special:
		SetSpecialAccepted(!bSpecialAccepted);
		break;

	case EGutCardItemType::Armor:
		BuyArmor(CardSelection.Index);
		break;

	case EGutCardItemType::Stable:
		ActivateStableLine(CardSelection.Index);
		return;

	case EGutCardItemType::Door:

		if (CardSelection.Index == GutDoorLines::Leave)
		{
			GoToDoor();
		}
		else
		{
			EnterBy(CardSelection.Index);
		}

		return;

	case EGutCardItemType::Drug:
		TakeDrug(static_cast<EGutDrug>(CardSelection.Index));
		break;

	case EGutCardItemType::Fix:
	{
		// taking the line that is already taken hands it back: one fix to a fight
		const EGutFix Fix = static_cast<EGutFix>(CardSelection.Index / 2);
		const int32 Corner = CardSelection.Index % 2;

		SetWindowFix((WindowFix == Fix && WindowFixCorner == Corner) ? EGutFix::None : Fix, Corner);

		// with something arranged there is one thing left to do, and the choice goes to it: the next press is the bet
		if (WindowFix != EGutFix::None)
		{
			CardSelection = FGutCardItem();
		}
		break;
	}

	default:

		if (IsAtWindow())
		{
			ConfirmWindowCard();
		}
		else
		{
			ConfirmCard();
		}
		break;
	}

	// if that used the line up, the choice goes back to the way out
	if (Phase == EGutFightPhase::Card && !GetCardItems().Contains(CardSelection))
	{
		CardSelection = FGutCardItem();
	}
}

bool AGutGameMode::AreDrugsSold() const
{
	return IsOnLadder() && GetLadderState().Rank >= DrugsFromRank;
}

bool AGutGameMode::IsDrugTaken(EGutDrug Drug) const
{
	return IsOnLadder() && GetLadderState().IsTaken(static_cast<int32>(Drug));
}

float AGutGameMode::GetDrugHabit(EGutDrug Drug) const
{
	return IsOnLadder() ? GetLadderState().Habit[static_cast<int32>(Drug)] : 0.0f;
}

bool AGutGameMode::IsInWithdrawal(EGutDrug Drug) const
{
	return IsOnLadder() && !IsDrugTaken(Drug) && GetDrugHabit(Drug) >= WithdrawalHabit;
}

bool AGutGameMode::TakeDrug(EGutDrug Drug)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (Phase != EGutFightPhase::Card || !AreDrugsSold() || IsDrugTaken(Drug))
	{
		return false;
	}

	FGutLadderState& State = GameInstance->Ladder;

	const int32 Which = static_cast<int32>(Drug);
	const float Price = GetDrugPrice(Drug);

	// on the slate, like everything else at the card
	if (State.Water - Price < -GetLadder()->SlateLimit)
	{
		return false;
	}

	State.Water -= Price;
	State.PatchSpend += Price;

	// Dust on top of a full habit is more than a heart is built for, and the heart does not mend
	if (Drug == EGutDrug::Dust && State.Habit[Which] >= 1.0f)
	{
		State.HeartDamage += DustHeartDamage;

		UE_LOG(LogGut, Log, TEXT("Ladder: Dust on a full habit. The heart is %.0f the worse for good"), State.HeartDamage);
	}

	State.SetTaken(Which, true);
	State.Habit[Which] = FMath::Min(1.0f, State.Habit[Which] + (Drug == EGutDrug::Dust ? DustHabitPerDose : HushHabitPerDose));

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogGut, Log, TEXT("Ladder: a dose of %s for %.0f L, leaving %.0f L"), Drug == EGutDrug::Dust ? TEXT("Dust") : TEXT("Hush"), Price, State.Water);

	return true;
}

const FGutSponsor* AGutGameMode::GetSponsorOffer() const
{
	const UGutLadderData* Rank = GetLadder();

	if (!IsOnLadder() || !Rank || Rank->Sponsors.IsEmpty())
	{
		return nullptr;
	}

	// one offer to a fight, the sponsors taking turns
	return &Rank->Sponsors[GetLadderState().Rung % Rank->Sponsors.Num()];
}

void AGutGameMode::SetSponsorAccepted(bool bAccepted)
{
	if (Phase == EGutFightPhase::Card)
	{
		bSponsorAccepted = bAccepted && GetSponsorOffer() != nullptr;
	}
}

bool AGutGameMode::IsArmorWorn(const UGutArmorData* Piece) const
{
	return Piece && IsOnLadder() && GetLadderState().Armor.Contains(Piece->GetFName());
}

bool AGutGameMode::BuyArmor(int32 ShopIndex)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Rank = GetLadder();

	// at the card, on the pit boss's slate as far as it stretches
	return Phase == EGutFightPhase::Card && IsOnLadder() && Rank && GameInstance->BuyArmor(Rank, ShopIndex, Rank->SlateLimit);
}

bool AGutGameMode::PatchLimb()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (Phase != EGutFightPhase::Card || !IsOnLadder() || !GameInstance->Ladder.bInjuriesCarried)
	{
		return false;
	}

	FGutLadderState& State = GameInstance->Ladder;

	// the first crippled limb in the order the HUD lists them
	int32 Limb = INDEX_NONE;

	for (int32 i = 0; i < GutNumLimbs; ++i)
	{
		if (State.LimbHealth[i] <= 0.0f)
		{
			Limb = i;
			break;
		}
	}

	// nothing to patch, or the slate will not stretch to it
	if (Limb == INDEX_NONE || State.Water - GetLadder()->PatchPrice < -GetLadder()->SlateLimit)
	{
		return false;
	}

	State.Water -= GetLadder()->PatchPrice;
	State.PatchSpend += GetLadder()->PatchPrice;
	State.LimbHealth[Limb] = PlayerFighter->GetMaxLimbHealth() * GetLadder()->PatchedLimbShare;

	UE_LOG(LogGut, Log, TEXT("Ladder: the cutter patches a limb for %.0f L, leaving %.0f L"), GetLadder()->PatchPrice, State.Water);

	return true;
}

float AGutGameMode::GetPatchPrice() const
{
	return IsOnLadder() ? GetLadder()->PatchPrice : 0.0f;
}

float AGutGameMode::GetBetWanted() const
{
	// what is down already, or failing that what the screen was last set to, which begins at the window's usual bet
	const float Most = GetMaxStake();
	const float Usual = GetLadder() ? GetLadder()->BetStep : 0.0f;

	return BetSide != EGutBetSide::None ? BetStake : FMath::Clamp(BetWanted > 0.0f ? BetWanted : Usual, FMath::Min(StakeStep, Most), Most);
}

bool AGutGameMode::CanLeaveForStreet() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !GameInstance->IsStreetOpen())
	{
		return false;
	}

	// from the night's reckoning, once its sums are on the slate
	if (Phase == EGutFightPhase::Gate)
	{
		return IsAtWindow();
	}

	// not somebody who was caught, who goes into the cage. At the end of a night the street is still a way out: the night
	// is reckoned on the way down the stairs.
	return CanRestart() && WindowSettlement.bSettled && !WindowSettlement.bOwnFight && !WindowSettlement.bCaught;
}

bool AGutGameMode::CanGoBack() const
{
	if (IsStopped() || IsTellingStory())
	{
		return false;
	}

	if (Phase == EGutFightPhase::Betting)
	{
		return IsAtWindow() || IsAtStable();
	}

	return CanLeaveForStreet();
}

void AGutGameMode::HandleBack()
{
	if (!CanGoBack())
	{
		return;
	}

	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (Phase == EGutFightPhase::Betting)
	{
		SetBet(EGutBetSide::None, 0.0f);

		if (IsAtStable())
		{
			// the fight that was made is unmade, and the owner is in the yard again
			UE_LOG(LogGut, Log, TEXT("Stable: backed out of the betting, to the yard"));

			BeginStableCard();
		}
		else if (GameInstance->IsStreetOpen())
		{
			// no bet and nothing watched. Anything arranged at the card was paid for there, and stays paid.
			UE_LOG(LogGut, Log, TEXT("Window: backed out of the betting, down to the street"));

			GameInstance->GoToStreet(this);
		}
		else
		{
			UE_LOG(LogGut, Log, TEXT("Window: backed out of the betting, to the card"));

			BeginWindowCard();
		}

		return;
	}

	// a fight that was watched, or a night's reckoning: back is the street
	bLeaveChosen = true;

	if (Phase == EGutFightPhase::Gate)
	{
		LeaveWindowNight();
	}
	else
	{
		RequestRestart();
	}
}

float AGutGameMode::GetMaxStake() const
{
	// a gambler, or an owner, stakes what it holds: the window gives neither a slate
	if ((IsAtWindow() || IsAtStable()) && Ladder)
	{
		const float WindowStep = FMath::Max(StakeStep, 1.0f);
		const float Held = IsAtStable() ? GetStableState().Water : GetWindowState().Water;

		return FMath::Max(0.0f, FMath::FloorToFloat(FMath::Min(Ladder->MaxBet, Held) / WindowStep) * WindowStep);
	}

	if (!IsOnLadder())
	{
		return 0.0f;
	}

	// what the player holds and what the slate allows, in whole steps, up to the window's limit
	const float Available = FMath::Min(GetLadder()->MaxBet, GetLadderState().Water + GetLadder()->SlateLimit);
	const float Step = FMath::Max(StakeStep, 1.0f);

	return FMath::Max(0.0f, FMath::FloorToFloat(Available / Step) * Step);
}

float AGutGameMode::GetBookChance() const
{
	if (!IsOnLadder() || !bFightersReady)
	{
		return 0.5f;
	}

	const float Chance = GetLadder()->GetBookChance(GetLadderState().Rung, PlayerFighter->GetWeapon(), EnemyFighter->GetWeapon(), PlayerFighter->GetCrippledLimbCount());

	// less for every fight the other one has won in this cage since the player came to it: the book has seen those
	return FMath::Clamp(Chance + GetRecordEdge(0, GetCageWins(GetLadderOpponent(GetLadderState().Rung))), 0.08f, 0.92f);
}

float AGutGameMode::GetReturnPerLitre(EGutBetSide Side, float Stake) const
{
	// at the window the two sides are the two corners, and the book's chance is the first corner's
	if (IsAtWindow() && Ladder)
	{
		return Ladder->GetReturnPerLitre(GetWindowBookChance(), Side, Stake);
	}

	// in a stable's fight the first side is the stable's own fighter
	if (IsAtStable() && Ladder)
	{
		return Ladder->GetReturnPerLitre(GetStableBookChance(), Side, Stake);
	}

	return IsOnLadder() ? GetLadder()->GetReturnPerLitre(GetBookChance(), Side, Stake) : 0.0f;
}

void AGutGameMode::SetBet(EGutBetSide Side, float Stake)
{
	if (Phase != EGutFightPhase::Betting && Phase != EGutFightPhase::Card && Phase != EGutFightPhase::Choosing)
	{
		return;
	}

	const float MaxStake = GetMaxStake();

	BetSide = MaxStake > 0.0f ? Side : EGutBetSide::None;
	BetStake = FMath::Clamp(Stake, 0.0f, MaxStake);

	// picking a side with nothing staked stakes the smallest bet, and a bet with nothing staked is no bet
	if (BetSide != EGutBetSide::None && BetStake <= 0.0f)
	{
		BetStake = FMath::Min(GetLadder() ? GetLadder()->BetStep : 0.0f, MaxStake);
	}

	if (BetSide == EGutBetSide::None)
	{
		BetStake = 0.0f;
	}
}

void AGutGameMode::ConfirmBet()
{
	if (Phase != EGutFightPhase::Betting || !bFightersReady)
	{
		return;
	}

	if (UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && BetSide != EGutBetSide::None && BetStake > 0.0f)
	{
		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			Sounds->Play(FName("cash"), FVector::ZeroVector);
		}

		// the odds are those posted with the player's own water in the pot
		BetReturnPerLitre = GetReturnPerLitre(BetSide, BetStake);

		if (IsAtStable())
		{
			GameInstance->Stable.Water -= BetStake;
		}
		else if (IsAtWindow())
		{
			GameInstance->Window.Water -= BetStake;
			GameInstance->Window.Staked += BetStake;
		}
		else
		{
			GameInstance->Ladder.Water -= BetStake;
			GameInstance->Ladder.BettingNet -= BetStake;
		}

		UE_LOG(LogGut, Log, TEXT("Ladder: %.0f L on %s at %.2f back for each litre"), BetStake,
			BetSide == EGutBetSide::Player ? TEXT("the player") : TEXT("the opponent"), BetReturnPerLitre);
	}
	else
	{
		BetSide = EGutBetSide::None;
		BetStake = 0.0f;
	}

	SetPhase(EGutFightPhase::Intro);
}

bool AGutGameMode::IsThrowingFight() const
{
	const FGutSponsor* Sponsor = bSponsorAccepted ? GetSponsorOffer() : nullptr;

	return Sponsor && Sponsor->Demand == EGutSponsorDemand::ThrowFight;
}

void AGutGameMode::SettleLadderFight(bool bPlayerWon, bool bSpared, bool bThrown)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const FGutLadderRung* Rung = GetRung();

	if (!GameInstance || !Rung)
	{
		return;
	}

	FGutLadderState& State = GameInstance->Ladder;

	// whoever was in the other corner is dead for the window and the pit boss's bills too, or has one more to show the book.
	// Read now: the fight is still this one.
	if (const int32 Against = GetLadderOpponent(State.Rung); bPlayerWon)
	{
		RecordCageFight(-1, Against, true);
	}
	else
	{
		RecordCageFight(Against, -1, false);
	}

	Settlement = FGutSettlement();
	Settlement.bPlayerWon = bPlayerWon;
	Settlement.bSpared = bSpared;
	Settlement.bThrown = bThrown;
	Settlement.Purse = bPlayerWon ? FMath::RoundToFloat(Rung->Purse * GetSpecialPurseScale(SpecialCard)) : 0.0f;
	Settlement.Stake = BetStake;
	Settlement.BetSide = BetSide;

	// the window pays whoever backed the fighter still standing
	const bool bBetWon = (BetSide == EGutBetSide::Player && bPlayerWon) || (BetSide == EGutBetSide::Opponent && !bPlayerWon);

	if (bBetWon)
	{
		Settlement.BetReturn = BetStake * BetReturnPerLitre;
	}

	const UGutLadderData* Rank = GetLadder();

	if (bPlayerWon)
	{
		// the crowd pays for the finish it called for, and remembers who gave it
		Settlement.bCrowdCallMatched = bCrowdCalling && EnemyFighter->WasFinished() && EnemyFighter->GetKillingFinisherStyle() == CrowdCall;
		Settlement.CrowdBonus = Settlement.bCrowdCallMatched ? Rank->CrowdCallBonus : 0.0f;
		Settlement.NameGained = Rank->NameForWin + (Settlement.bCrowdCallMatched ? Rank->NameForCrowdCall : 0);

		// the cut of the gate goes by the name the player came in with
		Settlement.GateCut = Rank->GetGateCut(State.Name);

		// a sponsor pays for a win on its terms, and for nothing else
		if (const FGutSponsor* Sponsor = bSponsorAccepted ? GetSponsorOffer() : nullptr)
		{
			if (Sponsor->Demand == EGutSponsorDemand::ThrowFight)
			{
				// the water was for going down. The other corner was paid out of it, and that is now the player's to make good.
				Settlement.SponsorOwed = Sponsor->Pay;
			}
			else
			{
				const bool bMet = Sponsor->Demand == EGutSponsorDemand::WearColors
					|| (EnemyFighter->WasFinished() && EnemyFighter->GetKillingFinisherStyle() == EGutFinisherStyle::Slow);

				Settlement.SponsorPay = bMet ? Sponsor->Pay : 0.0f;
			}
		}

		// and the pit boss keeps his share of the purse until he is paid
		Settlement.DebtPaid = FMath::Min(State.Debt, Settlement.Purse * Rank->DebtShareOfPurse);
	}

	if (bThrown)
	{
		// paid to lose, and it lost. The crowd knows what it saw.
		if (const FGutSponsor* Sponsor = GetSponsorOffer())
		{
			Settlement.SponsorPay = Sponsor->Pay;
		}

		Settlement.NameLost = FMath::Min(State.Name, Rank->NameForWin * 2);
	}

	Settlement.bSponsored = bSponsorAccepted;

	State.Debt += Settlement.SponsorOwed;
	State.Name -= Settlement.NameLost;

	State.Water += Settlement.Purse - Settlement.DebtPaid + Settlement.CrowdBonus + Settlement.GateCut + Settlement.SponsorPay + Settlement.BetReturn;
	State.Purses += Settlement.Purse;
	State.BettingNet += Settlement.BetReturn;
	State.Extras += Settlement.CrowdBonus + Settlement.GateCut + Settlement.SponsorPay;
	State.Debt -= Settlement.DebtPaid;
	State.DebtPaid += Settlement.DebtPaid;
	State.Name += Settlement.NameGained;

	// a fight without a dose takes a little of the habit off; a fight with one has already added to it
	if (bPlayerWon || bSpared || bThrown)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			if (!State.IsTaken(i))
			{
				State.Habit[i] = FMath::Max(0.0f, State.Habit[i] - HabitLostPerFight);
			}

			State.SetTaken(i, false);
		}
	}

	// what was eaten and drunk for this fight has been used in it
	State.bFed = false;
	State.bSteadied = false;

	// a player that walks out, or is left alive, carries its limbs to whatever comes next
	if (bPlayerWon || bSpared || bThrown)
	{
		State.bInjuriesCarried = true;

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			State.LimbHealth[i] = PlayerFighter->GetLimbHealth(static_cast<EGutLimb>(i));
		}
	}

	if (bPlayerWon)
	{
		++State.Rung;

		State.bFinished = State.Rung >= GetLadder()->Rungs.Num();
	}
	else if (bSpared)
	{
		State.bFinished = true;
		State.bSpared = true;
	}

	// set last, so that until here this fight's rung is still the one being read
	Settlement.bSettled = true;

	// a fight that is over and paid for stays that way if the game is closed now. A thrown fight is over and paid for,
	// and the same opponent is still there to be fought.
	if (bPlayerWon || bSpared || bThrown)
	{
		GameInstance->LadderAtRungStart = State;
		GameInstance->SaveRun();
	}

	UE_LOG(LogGut, Log, TEXT("Ladder: %s. Purse %.0f L, the window pays %.0f L, %.0f L in hand"),
		bPlayerWon ? TEXT("won") : (bSpared ? TEXT("spared") : (bThrown ? TEXT("thrown") : TEXT("dead"))), Settlement.Purse, Settlement.BetReturn, State.Water);
}

void AGutGameMode::OnPlayerStaggered(AGutFighter* Fighter)
{
	const FGutLadderRung* Rung = GetRung();

	// a fight the player was paid to lose: the other corner was paid too, to leave it breathing
	const bool bThrown = Phase == EGutFightPhase::Fighting && Rung && IsThrowingFight();

	// -GutSpare has the other corner spare the player in a single fight too, which has nobody on its card: to see it or film it
	static const bool bSpareAnyway = FParse::Param(FCommandLine::Get(), TEXT("GutSpare"));

	if (bSpareAnyway && !Rung && Phase == EGutFightPhase::Fighting)
	{
		UE_LOG(LogGut, Log, TEXT("Fight decided after %.1f seconds: the player is spared"), FightTime);

		PlayerFighter->SetCombatEnabled(false);
		EnemyFighter->SetCombatEnabled(false);

		PlayerFighter->Yield(0.9f);
		EnemyFighter->Spare(1.2f);

		SetPhase(EGutFightPhase::PlayerLost);
		return;
	}

	// an opponent that will not finish the player has won the moment the player is beaten
	if (Phase != EGutFightPhase::Fighting || !Rung || !(Rung->bSparesPlayer || bThrown))
	{
		return;
	}

	UE_LOG(LogGut, Log, TEXT("Fight decided after %.1f seconds: the player %s"), FightTime, bThrown ? TEXT("went down as arranged") : TEXT("is spared"));

	PlayerFighter->SetCombatEnabled(false);
	EnemyFighter->SetCombatEnabled(false);

	// the beaten one sways a moment longer and goes down onto its knees, and stays there. The one who would not kill it
	// lets its hands drop and stands over it; the one who was paid not to shows the crowd it won all the same.
	PlayerFighter->Yield(0.9f);

	if (bThrown)
	{
		EnemyFighter->Celebrate(1.6f);
	}
	else
	{
		EnemyFighter->Spare(1.2f);
	}

	// the crowd came for a death
	if (bThrown && Crowd)
	{
		Crowd->Cheer(0.15f);
	}

	SettleLadderFight(false, !bThrown, bThrown);

	SetPhase(EGutFightPhase::PlayerLost);
}

void AGutGameMode::OnAnyFighterStaggered(AGutFighter* Fighter)
{
	if (Crowd)
	{
		Crowd->Cheer(0.5f);
	}
}

void AGutGameMode::OnAnyAttackReceived(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result)
{
	// a clean hit gets a few of them going, a broken guard more
	if (Crowd && Result == EGutHitResult::Hit)
	{
		Crowd->Cheer(0.08f);
	}
	else if (Crowd && Result == EGutHitResult::GuardBroken)
	{
		Crowd->Cheer(0.3f);
	}
}

bool AGutGameMode::CanPlayerHold(const UGutWeaponData* Weapon) const
{
	return PlayerFighter && Weapon && !PlayerFighter->IsLimbCrippled(PlayerFighter->GetArmFor(Weapon));
}

void AGutGameMode::CyclePlayerChoice(int32 Step)
{
	if (Phase != EGutFightPhase::Choosing || !bFightersReady || RackWeapons.IsEmpty())
	{
		return;
	}

	// move along the rack to the next weapon the player has the arm for. With no such weapon the draw stays where it is.
	for (int32 i = 0; i < RackWeapons.Num(); ++i)
	{
		PlayerChoice = ((PlayerChoice + Step) % RackWeapons.Num() + RackWeapons.Num()) % RackWeapons.Num();

		if (CanPlayerHold(RackWeapons[PlayerChoice]))
		{
			break;
		}
	}

	ApplyChoices();
}

void AGutGameMode::CycleEnemyChoice(int32 Step)
{
	// on a ladder the opponent's weapon is its own business
	if (Phase != EGutFightPhase::Choosing || !bFightersReady || RackWeapons.IsEmpty() || IsOnLadder())
	{
		return;
	}

	EnemyChoice = ((EnemyChoice + Step) % RackWeapons.Num() + RackWeapons.Num()) % RackWeapons.Num();

	ApplyChoices();
}

void AGutGameMode::ConfirmChoice()
{
	if (Phase != EGutFightPhase::Choosing || !bFightersReady)
	{
		return;
	}

	bWeaponsDrawn = true;

	UE_LOG(LogGut, Log, TEXT("Weapons drawn: %s against %s"),
		*GetNameSafe(PlayerFighter->GetArmament()), *GetNameSafe(EnemyFighter->GetArmament()));

	// on a ladder the betting window comes between the rack and the bell
	if (IsOnLadder())
	{
		SetBet(EGutBetSide::None, 0.0f);
		SetPhase(EGutFightPhase::Betting);
	}
	else
	{
		SetPhase(EGutFightPhase::Intro);
	}
}

void AGutGameMode::SkipRack()
{
	if (Phase != EGutFightPhase::Choosing || !bFightersReady)
	{
		return;
	}

	PlayerFighter->EquipWeapon(PlayerFighter->GetStartingWeapon());
	EnemyFighter->EquipWeapon(EnemyFighter->GetStartingWeapon());

	if (Rack)
	{
		Rack->SetDrawn({});
	}

	SetPhase(EGutFightPhase::Intro);
}

void AGutGameMode::StartFight()
{
	const bool bBeforeBell = Phase == EGutFightPhase::Choosing || Phase == EGutFightPhase::Betting || Phase == EGutFightPhase::Intro;

	if (!bBeforeBell || !bFightersReady)
	{
		return;
	}

	PlayerFighter->SetCombatEnabled(true);
	EnemyFighter->SetCombatEnabled(true);

	for (AGutFighter* Extra : ExtraFighters)
	{
		Extra->SetCombatEnabled(true);
	}

	// the crowd makes up its mind how it wants this one ended
	bCrowdCalling = IsOnLadder() && GetLadder()->bCrowdCalls;
	CrowdCall = FMath::RandBool() ? EGutFinisherStyle::Slow : EGutFinisherStyle::Clean;

	// the bell
	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("bell"), FVector(0.0f, 0.0f, 400.0f));
	}

	SetPhase(EGutFightPhase::Fighting);
}

void AGutGameMode::SetHazardsEnabled(bool bEnabled)
{
	bHazardsEnabled = bEnabled;

	UpdateHazards();
}

void AGutGameMode::AdvanceFightTime(float Seconds)
{
	if (Phase != EGutFightPhase::Fighting || Seconds <= 0.0f)
	{
		return;
	}

	PhaseTime += Seconds;
	FightTime = PhaseTime;

	UpdateHazards();
}

void AGutGameMode::RequestRestart()
{
	if (!CanRestart())
	{
		return;
	}

	// whoever watched and has had enough goes back down to the street: what the fight came to is already written down
	if (IsLeaveChosen())
	{
		// after the last fight on the bill the night is turned over on the way out, as its reckoning would have done
		if (IsAtWindow() && IsWindowNightOver())
		{
			TurnWindowNight();
		}

		UE_LOG(LogGut, Log, TEXT("Window: back down to the street after the fight"));

		GetGutGameInstance()->GoToStreet(this);
		return;
	}

	FString Options;

	if (IsOnLadder())
	{
		// a fight the player died in is fought again, as the player came to it. Anything else has already moved the ladder on.
		if (Settlement.bSettled && !Settlement.bPlayerWon && !Settlement.bSpared && !Settlement.bThrown)
		{
			UGutGameInstance* GameInstance = GetGutGameInstance();

			GameInstance->Ladder = GameInstance->LadderAtRungStart;
		}

		// the player comes back to the rack with the weapon it last drew, if it can still hold it
		if (bWeaponsDrawn && RackWeapons.IsValidIndex(PlayerChoice))
		{
			Options = FString::Printf(TEXT("%s=%s"), GutGameModeOptions::PlayerWeapon, *RackWeapons[PlayerChoice]->GetName());
		}
	}
	else if (bWeaponsDrawn && RackWeapons.IsValidIndex(PlayerChoice) && RackWeapons.IsValidIndex(EnemyChoice))
	{
		// come back to the rack with the same two weapons drawn
		Options = FString::Printf(TEXT("%s=%s?%s=%s"),
			GutGameModeOptions::PlayerWeapon, *RackWeapons[PlayerChoice]->GetName(),
			GutGameModeOptions::EnemyWeapon, *RackWeapons[EnemyChoice]->GetName());
	}

	// and to a cage that is as dangerous as this one was
	if (!bHazardsEnabled)
	{
		Options += FString::Printf(TEXT("%s%s=0"), Options.IsEmpty() ? TEXT("") : TEXT("?"), GutGameModeOptions::Hazards);
	}

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, Options);
}

void AGutGameMode::BeginChoosing()
{
	// the player starts on the weapon it asked for, or the first on the rack, or failing those the first it has the arm for
	PlayerChoice = FMath::Max(0, FindRackWeapon(RequestedPlayerWeapon));

	if (!CanPlayerHold(RackWeapons[PlayerChoice]))
	{
		const int32 Holdable = RackWeapons.IndexOfByPredicate([this](const UGutWeaponData* Weapon) { return CanPlayerHold(Weapon); });

		PlayerChoice = Holdable != INDEX_NONE ? Holdable : PlayerChoice;
	}

	// the opponent draws what it was asked to, or what its rung of the ladder gives it, or what it is set to, or at random
	EnemyChoice = FindRackWeapon(RequestedEnemyWeapon);

	if (const FGutLadderRung* Rung = GetRung())
	{
		EnemyChoice = RackWeapons.IndexOfByKey(Rung->Weapon);
	}

	if (EnemyChoice == INDEX_NONE)
	{
		EnemyChoice = RackWeapons.IndexOfByKey(OpponentWeapon);
	}

	if (EnemyChoice == INDEX_NONE)
	{
		EnemyChoice = FMath::RandHelper(RackWeapons.Num());
	}

	// the rack on the wall, if the level has one, shows what is left
	const TActorIterator<AGutWeaponRack> RackIt(GetWorld());

	Rack = RackIt ? *RackIt : nullptr;

	if (Rack)
	{
		Rack->SetWeapons(RackWeapons);
	}

	ApplyChoices();

	UE_LOG(LogGut, Log, TEXT("At the rack: %s against %s"), *GetNameSafe(RackWeapons[PlayerChoice]), *GetNameSafe(RackWeapons[EnemyChoice]));

	SetPhase(EGutFightPhase::Choosing);
}

void AGutGameMode::ApplyChoices()
{
	UGutWeaponData* PlayerWeapon = RackWeapons[PlayerChoice];
	UGutWeaponData* EnemyWeapon = RackWeapons[EnemyChoice];

	// a player with no arm for what it has drawn comes away from the rack with nothing
	if (!PlayerFighter->EquipWeapon(PlayerWeapon))
	{
		PlayerFighter->EquipWeapon(nullptr);

		PlayerWeapon = nullptr;
	}

	EnemyFighter->EquipWeapon(EnemyWeapon);

	if (Rack)
	{
		Rack->SetDrawn({ PlayerWeapon, EnemyWeapon });
	}
}

int32 AGutGameMode::FindRackWeapon(const FString& NamePart) const
{
	if (NamePart.IsEmpty())
	{
		return INDEX_NONE;
	}

	return RackWeapons.IndexOfByPredicate([&NamePart](const UGutWeaponData* Weapon) { return Weapon && Weapon->GetName().Contains(NamePart); });
}

int32 AGutGameMode::GetRound() const
{
	return 1 + FMath::FloorToInt32(FightTime / FMath::Max(RoundLength, 1.0f));
}

float AGutGameMode::GetIntroTimeRemaining() const
{
	return Phase == EGutFightPhase::Intro ? FMath::Max(0.0f, IntroDuration - PhaseTime) : 0.0f;
}

bool AGutGameMode::CanRestart() const
{
	const bool bOver = Phase == EGutFightPhase::PlayerWon || Phase == EGutFightPhase::PlayerLost;

	return bOver && PhaseTime >= RestartDelay;
}

bool AGutGameMode::FindFighters()
{
	PlayerFighter = Cast<AGutFighter>(UGameplayStatics::GetPlayerPawn(this, 0));

	if (!PlayerFighter)
	{
		return false;
	}

	// the opponent is the other fighter in the level
	EnemyFighter = nullptr;

	for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
	{
		if (*It != PlayerFighter && It->IsAlive())
		{
			EnemyFighter = *It;
			break;
		}
	}

	if (!EnemyFighter)
	{
		return false;
	}

	PlayerFighter->SetOpponent(EnemyFighter);
	EnemyFighter->SetOpponent(PlayerFighter);

	PlayerFighter->OnDied.AddDynamic(this, &AGutGameMode::OnFighterDied);
	EnemyFighter->OnDied.AddDynamic(this, &AGutGameMode::OnFighterDied);

	// the dogs are the pit boss's and not the pool's: there is no place in the level that they are, so the thing that
	// lets them in on a long fight is made here
	if (Dog && DogsRound > 0 && !TActorIterator<AGutDogs>(GetWorld()))
	{
		if (AGutDogs* Kennel = GetWorld()->SpawnActor<AGutDogs>())
		{
			Kennel->SetActivationRound(DogsRound);
			Kennel->Count = DogsLoosed;
		}
	}

	// as loud as it was left the last time the cage was open
	ApplyLoudness();

	// every hazard starts the fight switched off
	Hazards.Reset();

	for (TActorIterator<AGutHazard> It(GetWorld()); It; ++It)
	{
		It->SetHazardState(EGutHazardState::Off);

		Hazards.Add(*It);
	}

	// nobody swings before the bell
	PlayerFighter->SetCombatEnabled(false);
	EnemyFighter->SetCombatEnabled(false);

	PlayerFighter->OnStaggered.AddDynamic(this, &AGutGameMode::OnPlayerStaggered);

	// the crowd on the deck: bigger for a fighter with a name, and it reacts to what happens below
	const TActorIterator<AGutCrowd> CrowdIt(GetWorld());

	Crowd = CrowdIt ? *CrowdIt : nullptr;

	if (Crowd)
	{
		// a night at the window is a full house: the bill is the cage's own fighters, and they have names
		Crowd->Populate(IsOnLadder() ? GetLadderState().Name : (IsAtWindow() || IsGamblersOwnFight() ? 100000 : (IsAtStable() ? GetStableState().Name : 0)));

		for (AGutFighter* Fighter : { PlayerFighter.Get(), EnemyFighter.Get() })
		{
			Fighter->OnStaggered.AddDynamic(this, &AGutGameMode::OnAnyFighterStaggered);
			Fighter->OnAttackReceived.AddDynamic(this, &AGutGameMode::OnAnyAttackReceived);
		}
	}

	// with nothing on the rack there is nothing to choose
	RackWeapons.Remove(nullptr);

	if (IsAtDoor())
	{
		// nobody is driven and nothing is at stake: the two in the cage stand there, seen from the wall
		StartSpectating();

		CardSelection = GetCardItems()[0];

		SetPhase(EGutFightPhase::Card);
	}
	else if (IsAtStable())
	{
		BeginStableCard();
	}
	else if (IsGamblersOwnFight())
	{
		BeginGamblersOwnFight();
	}
	else if (IsAtWindow())
	{
		BeginWindowCard();
	}
	else if (IsOnLadder())
	{
		// the last fight is behind the player: all that is left is the gate
		if (GetLadderState().bFinished)
		{
			SetPhase(EGutFightPhase::Gate);

			// out of Meat with a tag, or the Champion dead: a picture or two before the sums. Somebody spared has not won either.
			if (!GetLadderState().bSpared)
			{
				TArray<const UGutLadderData*> Ranks;
				GetRanks(Ranks);

				TellStory(GetLadderState().Rank == 0 ? EGutStory::Tagged : (GetLadderState().Rank == Ranks.Num() - 1 ? EGutStory::Champion : EGutStory::None));
			}
		}
		else
		{
			BeginCard();
		}
	}
	else if (RackWeapons.IsEmpty())
	{
		SetPhase(EGutFightPhase::Intro);
	}
	else
	{
		BeginChoosing();
	}

	// a single fight asked for as a special card: 3 is one against three, 2 is two on two, 4 is against the dogs
	if (!IsOnLadder() && !IsAtDoor() && !IsAtStable() && !IsAtWindow() && !IsGamblersOwnFight() && RequestedSpecial >= 2 && RequestedSpecial <= 4)
	{
		BeginSpecial(RequestedSpecial == 3 ? EGutSpecialCard::OneAgainstThree : (RequestedSpecial == 4 ? EGutSpecialCard::Animal : EGutSpecialCard::TwoOnTwo));
	}

	return true;
}

void AGutGameMode::UpdateHazards()
{
	// once the fight is decided the cage stays as it was
	if (Phase == EGutFightPhase::PlayerWon || Phase == EGutFightPhase::PlayerLost || Phase == EGutFightPhase::Gate)
	{
		return;
	}

	const bool bRunning = Phase == EGutFightPhase::Fighting;

	for (AGutHazard* Hazard : Hazards)
	{
		if (!Hazard)
		{
			continue;
		}

		EGutHazardState NewState = EGutHazardState::Off;

		if (bHazardsEnabled && bRunning)
		{
			const float LiveTime = GetRoundStartTime(Hazard->GetActivationRound());

			if (FightTime >= LiveTime)
			{
				NewState = EGutHazardState::Live;
			}
			else if (FightTime >= LiveTime - HazardWarningTime)
			{
				NewState = EGutHazardState::Warning;
			}
		}

		Hazard->SetHazardState(NewState);
	}
}

void AGutGameMode::SetPhase(EGutFightPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;

	UE_LOG(LogGut, Log, TEXT("Fight phase: %s"), *UEnum::GetDisplayValueAsText(NewPhase).ToString());

	BP_OnPhaseChanged(NewPhase);

	OnPhaseChanged.Broadcast(NewPhase);
}

void AGutGameMode::OnFighterDied(AGutFighter* Fighter, AGutFighter* Killer)
{
	// the first death decides the fight
	if (Phase != EGutFightPhase::Fighting)
	{
		return;
	}

	// a dog let in on the fight is nobody's opponent: killing it decides nothing
	if (bDogsLoose && Fighter->IsBeast() && Fighter->GetSide() == GutDogsSide)
	{
		UE_LOG(LogGut, Log, TEXT("A dog is dead after %.1f seconds"), FightTime);

		if (Crowd)
		{
			Crowd->Cheer(0.6f, 0.4f);
		}

		UpdateSpecialTargets();
		return;
	}

	// unless there are more than two in the cage: then it goes on until the player is dead or nobody is left against it
	if (SpecialCard != EGutSpecialCard::None && Fighter != PlayerFighter && (Fighter->GetSide() == 0 || CountLiving(1) > 0))
	{
		UE_LOG(LogGut, Log, TEXT("%s is dead after %.1f seconds. %d left against %d"), *Fighter->GetDisplayName().ToString(), FightTime, CountLiving(0), CountLiving(1));

		if (Crowd)
		{
			Crowd->Cheer(0.7f, 0.5f);
		}

		UpdateSpecialTargets();
		return;
	}

	UE_LOG(LogGut, Log, TEXT("Fight decided after %.1f seconds%s"), FightTime, Fighter->GetKillingHazard() ? TEXT(", by the cage") : TEXT(""));

	// the survivor can walk around, but the fighting is over
	PlayerFighter->SetCombatEnabled(false);
	EnemyFighter->SetCombatEnabled(false);

	for (AGutFighter* Extra : ExtraFighters)
	{
		Extra->SetCombatEnabled(false);
	}

	// a death is what they came for
	if (Crowd)
	{
		Crowd->Cheer(1.0f, 0.8f);
	}

	// and whoever is left standing against the dead one shows them
	TArray<AGutFighter*> Standing = { PlayerFighter.Get(), EnemyFighter.Get() };

	for (AGutFighter* Extra : ExtraFighters)
	{
		Standing.Add(Extra);
	}

	for (AGutFighter* Other : Standing)
	{
		if (Other && Other != Fighter && Other->IsAlive() && !Other->IsBeast() && (SpecialCard == EGutSpecialCard::None || Other->GetSide() != Fighter->GetSide()))
		{
			Other->Celebrate(1.0f + (0.4f * Standing.IndexOfByKey(Other)));
		}
	}

	if (IsAtStable())
	{
		SettleStableFight(Fighter);

		SetPhase(EGutFightPhase::PlayerWon);
		return;
	}

	if (IsAtWindow() || IsGamblersOwnFight())
	{
		const bool bOwnFight = IsGamblersOwnFight();

		SettleWindowFight(Fighter);

		// a fight that was only watched is over when either of them is dead: what it came to is on the settlement
		SetPhase((bOwnFight && Fighter == PlayerFighter) ? EGutFightPhase::PlayerLost : EGutFightPhase::PlayerWon);
		return;
	}

	if (IsOnLadder())
	{
		SettleLadderFight(Fighter != PlayerFighter, false);
	}

	SetPhase(Fighter == PlayerFighter ? EGutFightPhase::PlayerLost : EGutFightPhase::PlayerWon);
}

void AGutGameMode::TellStory(EGutStory Which)
{
	Story.Reset();
	StoryIndex = INDEX_NONE;

	if (!bStories || Which == EGutStory::None)
	{
		return;
	}

	auto Panel = [this](const TCHAR* Picture, std::initializer_list<const TCHAR*> Lines)
	{
		FGutStoryPanel& Added = Story.AddDefaulted_GetRef();

		Added.Picture = FName(Picture);

		for (const TCHAR* Line : Lines)
		{
			Added.Words.Add(FText::FromString(Line));
		}
	};

	switch (Which)
	{
	case EGutStory::Sold:
		Panel(TEXT("story_kiln"), { TEXT("The Kiln, two days out from anywhere with a name."), TEXT("You had a jug, a direction, and no tag. They had trucks.") });
		Panel(TEXT("story_collar"), { TEXT("No tag means you're property. That is the third law, and it is painted on the dam."), TEXT("The collar took four rivets. The brand took less time than that.") });
		Panel(TEXT("story_block"), { TEXT("Ashmarket. The Block, under the lamps."), TEXT("Mother Ledger's auctioneer called you sound, and Ham Rask bought you for forty litres.") });
		Panel(TEXT("story_gut"), { TEXT("The Gut. Every fight in it ends with one fighter dead."), TEXT("Three fights earn a tag. Ham Rask keeps the count.") });
		break;

	case EGutStory::Tagged:
		Panel(TEXT("story_gate"), { TEXT("A tag with your number on it, and the gate."), TEXT("No water, and Ham Rask's bill for room and board. The cage's verdict is final. His arithmetic is not.") });
		break;

	case EGutStory::Champion:
		Panel(TEXT("story_champion"), { TEXT("Morning. The lamps are off and the deck is empty."), TEXT("You were sold into this pool for forty litres. A piece of it is yours now.") });
		break;

	case EGutStory::Owner:
		Panel(TEXT("story_block"), { TEXT("The Block sells every third evening, and the Gut buys."), TEXT("You have water, and the pit boss will give anybody's fighter a card.") });
		Panel(TEXT("story_yard"), { TEXT("A yard behind the pool. Four cots, a barrel, a post."), TEXT("Whoever sleeps here is yours to feed, to train, to promise things to, and to send down the ladder.") });
		break;

	case EGutStory::Gambler:
		Panel(TEXT("story_window"), { TEXT("The window. Four fights a night, and the odds are chalked an hour before each."), TEXT("They move with the money. So do other things, for anybody who will pay, and the pit boss is watching for that.") });
		break;

	default:
		break;
	}

	StoryIndex = Story.IsEmpty() ? INDEX_NONE : 0;

	if (IsTellingStory())
	{
		UE_LOG(LogGut, Log, TEXT("Story: %s, %d pictures"), *UEnum::GetDisplayValueAsText(Which).ToString(), Story.Num());
	}
}

void AGutGameMode::AdvanceStory()
{
	if (!IsTellingStory())
	{
		return;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("card"), FVector::ZeroVector);
	}

	if (++StoryIndex >= Story.Num())
	{
		Story.Reset();
		StoryIndex = INDEX_NONE;
	}
}

namespace GutStoppedLines
{
	enum { GoOn, Sound, Street, Leave };
}

/** What each line of the menu that stops the game does, in order: the way out to the street is only there where there is one */
static TArray<int32> GetStoppedCodes(const AGutGameMode* GameMode, const UGutGameInstance* GameInstance)
{
	TArray<int32> Codes = { GutStoppedLines::GoOn, GutStoppedLines::Sound };

	// out to the street only from a card, where a run is written down and nothing is half done, and not from the street itself
	const bool bFromCard = GameMode->GetPhase() == EGutFightPhase::Card || (GameInstance && GameInstance->bSandbox && GameMode->GetPhase() == EGutFightPhase::Choosing);

	if (GameInstance && !GameInstance->bAtDoor && bFromCard && !GameMode->IsTellingStory())
	{
		Codes.Add(GutStoppedLines::Street);
	}

	Codes.Add(GutStoppedLines::Leave);

	return Codes;
}

void AGutGameMode::ApplyLoudness()
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (FAudioDeviceHandle Device = GetWorld()->GetAudioDevice(); Device && GameInstance)
	{
		Device->SetTransientPrimaryVolume(FMath::Clamp(GameInstance->Loudness, 0, 3) / 3.0f);
	}
}

void AGutGameMode::SetStopped(bool bStopped)
{
	if (bStopped == bStoppedMenu)
	{
		return;
	}

	bStoppedMenu = bStopped;
	StoppedChoice = 0;

	UGameplayStatics::SetGamePaused(this, bStoppedMenu);
}

TArray<FString> AGutGameMode::GetStoppedLines() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	static const TCHAR* Loud[] = { TEXT("off"), TEXT("a third"), TEXT("two thirds"), TEXT("full") };

	TArray<FString> Lines;

	for (int32 Code : GetStoppedCodes(this, GameInstance))
	{
		switch (Code)
		{
		case GutStoppedLines::GoOn:
			Lines.Add(TEXT("Go on"));
			break;

		case GutStoppedLines::Sound:
			Lines.Add(FString::Printf(TEXT("Sound: %s"), Loud[FMath::Clamp(GameInstance ? GameInstance->Loudness : 3, 0, 3)]));
			break;

		case GutStoppedLines::Street:
			Lines.Add(TEXT("Out to the street"));
			break;

		default:
			Lines.Add(TEXT("Leave the game"));
			break;
		}
	}

	return Lines;
}

void AGutGameMode::HandleStoppedInput(EGutMenuInput Input)
{
	if (!bStoppedMenu)
	{
		return;
	}

	UGutGameInstance* GameInstance = GetGutGameInstance();

	const TArray<int32> Codes = GetStoppedCodes(this, GameInstance);

	if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
	{
		StoppedChoice = (StoppedChoice + (Input == EGutMenuInput::Up ? Codes.Num() - 1 : 1)) % Codes.Num();
		return;
	}

	if (Input != EGutMenuInput::Confirm || !Codes.IsValidIndex(StoppedChoice))
	{
		return;
	}

	switch (Codes[StoppedChoice])
	{
	case GutStoppedLines::GoOn:
		SetStopped(false);
		break;

	case GutStoppedLines::Sound:

		// each press a third quieter, and round again
		if (GameInstance)
		{
			GameInstance->Loudness = (GameInstance->Loudness + 3) % 4;
			GameInstance->KeepLoudness();

			ApplyLoudness();
		}
		break;

	case GutStoppedLines::Street:
		SetStopped(false);
		GoToDoor();
		break;

	default:

		// a run is written down at every card, so what is lost is at most the fight in hand
		SetStopped(false);

		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
		break;
	}
}
