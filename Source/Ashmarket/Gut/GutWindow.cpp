// Ashmarket prototype: the Gut.

// The Gut entered as a gambler (design document, chapter 7): the fights on a night's bill are between the ladder's own
// fighters, the player watches from the wall with water on them, and can arrange for one of the two to lose.
// These are members of the cage's game mode, kept in a file of their own.

#include "GutGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GutCrowd.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"
#include "GutSound.h"
#include "GutWeaponData.h"
#include "Kismet/GameplayStatics.h"

namespace GutWindowConstants
{
	/** The book never calls a fight between two of the cage's own a certainty either way */
	const float MinBookChance = 0.12f;
	const float MaxBookChance = 0.88f;

	/** How far the watcher's camera stands off, and how high: inside the pool, under the wire */
	const float CameraHeight = 175.0f;
	const float CameraLookHeight = 105.0f;
}

bool AGutGameMode::IsAtWindow() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return Ladder && GameInstance && GameInstance->bWindow && !GameInstance->bSandbox && !GameInstance->bAtDoor && !GameInstance->Window.bCaught;
}

bool AGutGameMode::IsGamblersOwnFight() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return Ladder && GameInstance && GameInstance->bWindow && !GameInstance->bSandbox && !GameInstance->bAtDoor && GameInstance->Window.bCaught;
}

bool AGutGameMode::IsWindowNightOver() const
{
	return IsAtWindow() && GetWindowState().Fight >= WindowFightsPerNight;
}

const FGutWindowState& AGutGameMode::GetWindowState() const
{
	static const FGutWindowState Empty;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance ? GameInstance->Window : Empty;
}

void AGutGameMode::EnterWindow(int32 Seed)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !Ladder)
	{
		return;
	}

	GameInstance->bSandbox = false;
	GameInstance->bStable = false;
	GameInstance->bWindow = true;

	GameInstance->Window = FGutWindowState();
	GameInstance->Window.bStarted = true;
	GameInstance->PendingStory = EGutStory::Gambler;
	GameInstance->Window.Water = WindowStartingWater;
	GameInstance->Window.Seed = Seed != 0 ? Seed : FMath::RandRange(1, 1000000);

	UE_LOG(LogGut, Log, TEXT("Window: a gambler comes to the cage with %.0f L"), WindowStartingWater);

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true);
}

void AGutGameMode::GetRanks(TArray<const UGutLadderData*>& OutRanks) const
{
	OutRanks.Reset();

	for (const UGutLadderData* Rank = Ladder; Rank && OutRanks.Num() < 16; Rank = Rank->NextLadder)
	{
		OutRanks.Add(Rank);
	}
}

void AGutGameMode::BuildWindowBill(const FGutWindowState& State, TArray<FGutWindowFight>& OutBill) const
{
	OutBill.Reset();

	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	// everybody still alive, from the bottom of the ladders to the top: near neighbours are near matches
	TArray<TPair<int32, int32>> Living;

	for (int32 Rank = 0; Rank < Ranks.Num(); ++Rank)
	{
		for (int32 Rung = 0; Rung < Ranks[Rank]->Rungs.Num(); ++Rung)
		{
			// alive when the bill was posted: whoever has died on it since is still on it
			const int32 Place = State.Dead.IndexOfByKey((Rank * 100) + Rung);

			if (Place == INDEX_NONE || Place >= State.DeadBeforeTonight)
			{
				Living.Add({ Rank, Rung });
			}
		}
	}

	// the same seed and the same night are the same bill
	FRandomStream Draw(State.Seed + (State.Night * 7919));

	while (OutBill.Num() < WindowFightsPerNight && Living.Num() >= 2)
	{
		// one of them anywhere on the list, the other one or two places from it
		const int32 First = Draw.RandRange(0, Living.Num() - 1);

		int32 Second = First + (Draw.RandRange(0, 1) == 0 ? 1 : -1) * Draw.RandRange(1, 2);

		Second = FMath::Clamp(Second, 0, Living.Num() - 1);

		if (Second == First)
		{
			Second = First == 0 ? 1 : First - 1;
		}

		FGutWindowFight Fight;

		// which corner each stands in is a coin's toss, so the corner says nothing about the fighter
		const bool bSwap = Draw.RandRange(0, 1) == 1;

		Fight.Rank[bSwap ? 1 : 0] = Living[First].Key;
		Fight.Rung[bSwap ? 1 : 0] = Living[First].Value;
		Fight.Rank[bSwap ? 0 : 1] = Living[Second].Key;
		Fight.Rung[bSwap ? 0 : 1] = Living[Second].Value;

		OutBill.Add(Fight);

		// nobody fights twice in a night
		Living.RemoveAt(FMath::Max(First, Second));
		Living.RemoveAt(FMath::Min(First, Second));
	}
}

FGutWindowFight AGutGameMode::GetWindowFight() const
{
	TArray<FGutWindowFight> Bill;

	BuildWindowBill(GetWindowState(), Bill);

	// once a fight is settled the bill has moved on, but this fight is still the one it was
	const int32 Index = GetWindowState().Fight - (WindowSettlement.bSettled && !WindowSettlement.bOwnFight ? 1 : 0);

	return Bill.IsValidIndex(Index) ? Bill[Index] : FGutWindowFight();
}

const UGutLadderData* AGutGameMode::GetWindowRank(int32 Corner) const
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	const FGutWindowFight Fight = GetWindowFight();

	return (Fight.IsValid() && Ranks.IsValidIndex(Fight.Rank[Corner & 1])) ? Ranks[Fight.Rank[Corner & 1]] : nullptr;
}

const FGutLadderRung* AGutGameMode::GetWindowRung(int32 Corner) const
{
	const UGutLadderData* Rank = GetWindowRank(Corner);
	const int32 Rung = GetWindowFight().Rung[Corner & 1];

	return (Rank && Rank->Rungs.IsValidIndex(Rung)) ? &Rank->Rungs[Rung] : nullptr;
}

float AGutGameMode::GetWindowBookChance() const
{
	const FGutLadderRung* First = GetWindowRung(0);
	const FGutLadderRung* Second = GetWindowRung(1);

	if (!First || !Second)
	{
		return 0.5f;
	}

	// each rung says what the book gives a newcomer against the fighter on it: the less it gives, the better the fighter
	float Chance = 0.5f + (Second->BookChance - First->BookChance);

	// and what the book knows about the two weapons
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	for (const UGutLadderData* Rank : Ranks)
	{
		bool bKnown = false;

		for (const FGutFormEntry& Entry : Rank->FormBook)
		{
			if (Entry.Weapon == First->Weapon && Entry.Against == Second->Weapon)
			{
				Chance += (Entry.WinShare - 0.5f) * Rank->FormWeight;
				bKnown = true;
				break;
			}

			if (Entry.Weapon == Second->Weapon && Entry.Against == First->Weapon)
			{
				Chance -= (Entry.WinShare - 0.5f) * Rank->FormWeight;
				bKnown = true;
				break;
			}
		}

		if (bKnown)
		{
			break;
		}
	}

	return FMath::Clamp(Chance, GutWindowConstants::MinBookChance, GutWindowConstants::MaxBookChance);
}

float AGutGameMode::GetFixCost(EGutFix Fix, int32 Corner) const
{
	switch (Fix)
	{
	case EGutFix::Bribe:
	{
		// a fighter is being asked to die, or to trust that it will be spared: it wants more than the purse it is fighting for
		const FGutLadderRung* Rung = GetWindowRung(Corner);

		return Rung ? FMath::RoundToFloat(Rung->Purse * BribePurseScale) : 0.0f;
	}

	case EGutFix::DrugBucket:
		return DrugBucketCost;

	case EGutFix::TamperRack:
		return TamperRackCost;

	default:
		return 0.0f;
	}
}

float AGutGameMode::GetFixDetectChance(EGutFix Fix) const
{
	float Chance = 0.0f;

	switch (Fix)
	{
	case EGutFix::Bribe:
		Chance = BribeDetectChance;
		break;

	case EGutFix::DrugBucket:
		Chance = DrugBucketDetectChance;
		break;

	case EGutFix::TamperRack:
		Chance = TamperRackDetectChance;
		break;

	default:
		return 0.0f;
	}

	// a gambler who has been lucky before is looked at harder
	return FMath::Clamp(Chance + GetWindowState().Heat, 0.0f, 0.95f);
}

void AGutGameMode::SetWindowFix(EGutFix Fix, int32 Corner)
{
	if (Phase != EGutFightPhase::Card || !IsAtWindow())
	{
		return;
	}

	// only what the gambler can pay for
	if (Fix != EGutFix::None && GetFixCost(Fix, Corner) > GetWindowState().Water)
	{
		return;
	}

	WindowFix = Fix;
	WindowFixCorner = Corner & 1;
}

void AGutGameMode::MakeFighter(AGutFighter* Fighter, const FGutLadderRung& Rung, bool bCollared)
{
	Fighter->SetDisplayName(Rung.OpponentName);
	Fighter->SetMaxHealth(Rung.Health);
	Fighter->SetBodyTint(Rung.BodyTint);

	if (!Rung.Body.IsNull())
	{
		Fighter->SetBody(Rung.Body, Fighter->GetBodyRetargeter());
	}

	if (AGutOpponentController* Brain = Cast<AGutOpponentController>(Fighter->GetController()))
	{
		if (Rung.bOverrideTuning)
		{
			Brain->SetTuning(Rung.Tuning);
		}
	}

	Fighter->ClearArmor();

	for (UGutArmorData* Piece : Rung.Armor)
	{
		Fighter->WearArmor(Piece);
	}

	Fighter->SetCollared(bCollared);
	Fighter->EquipWeapon(Rung.Weapon);
}

void AGutGameMode::BeginWindowCard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance->Window.bStarted)
	{
		GameInstance->Window = FGutWindowState();
		GameInstance->Window.bStarted = true;
		GameInstance->PendingStory = EGutStory::Gambler;
		GameInstance->Window.Water = WindowStartingWater;
		GameInstance->Window.Seed = FMath::RandRange(1, 1000000);
	}

	WindowFix = EGutFix::None;
	WindowFixCorner = 0;
	WindowSettlement = FGutWindowSettlement();

	// the fights are fought by the cage's own, both of them: the player's fighter is handed over and the player goes to the wall
	StartSpectating();

	if (GameInstance->Window.Fight >= WindowFightsPerNight || !GetWindowFight().IsValid())
	{
		// the bill is fought out: all that is left of the night is the reckoning
		GameInstance->Window.Fight = WindowFightsPerNight;

		SetPhase(EGutFightPhase::Gate);
		return;
	}

	UE_LOG(LogGut, Log, TEXT("Window: night %d, fight %d of %d: %s against %s, %.0f L in hand"), GameInstance->Window.Night, GameInstance->Window.Fight + 1, WindowFightsPerNight,
		*GetWindowRung(0)->OpponentName.ToString(), *GetWindowRung(1)->OpponentName.ToString(), GameInstance->Window.Water);

	// the two of them are in the cage to be looked over while the gambler makes up its mind
	MakeFighter(PlayerFighter, *GetWindowRung(0), GetWindowFight().Rank[0] == 0);
	MakeFighter(EnemyFighter, *GetWindowRung(1), GetWindowFight().Rank[1] == 0);

	CardSelection = FGutCardItem();

	// a game that is closed comes back at this card
	GameInstance->SaveRun();

	SetPhase(EGutFightPhase::Card);

	// a gambler's first night begins with where it has come to
	if (GameInstance->PendingStory == EGutStory::Gambler)
	{
		TellStory(EGutStory::Gambler);

		GameInstance->PendingStory = EGutStory::None;
	}
}

void AGutGameMode::ConfirmWindowCard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (Phase != EGutFightPhase::Card || !bFightersReady || !IsAtWindow())
	{
		return;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("card"), FVector::ZeroVector);
	}

	// what was arranged is paid for now, and done
	if (WindowFix != EGutFix::None)
	{
		const float Cost = GetFixCost(WindowFix, WindowFixCorner);

		GameInstance->Window.Water -= Cost;
		GameInstance->Window.FixSpend += Cost;

		AGutFighter* Fixed = WindowFixCorner == 0 ? PlayerFighter : EnemyFighter;
		AGutOpponentController* Brain = Cast<AGutOpponentController>(Fixed->GetController());

		switch (WindowFix)
		{
		case EGutFix::Bribe:

			// it has been paid to lose: it barely swings, and it does not guard or get out of the way
			if (Brain)
			{
				FGutOpponentTuning Thrown = Brain->GetTuning();

				Thrown.AttackChance = 0.04f;
				Thrown.CrowdedAttackChance = 0.04f;
				Thrown.GuardChance = 0.0f;
				Thrown.BlockLightChance = 0.0f;
				Thrown.BlockHeavyChance = 0.0f;
				Thrown.DodgeHeavyChance = 0.0f;
				Thrown.CounterChance = 0.0f;
				Thrown.PunishChance = 0.0f;
				Thrown.GuardAfterHitChance = 0.0f;
				Thrown.bNeverCovers = true;

				Brain->SetTuning(Thrown);
			}
			break;

		case EGutFix::DrugBucket:

			Fixed->SetStaminaRegenScale(DruggedStaminaScale);

			if (Brain)
			{
				FGutOpponentTuning Slow = Brain->GetTuning();

				Slow.MinReactionTime *= DruggedReactionScale;
				Slow.MaxReactionTime *= DruggedReactionScale;
				Slow.MinDecisionInterval *= DruggedReactionScale;
				Slow.MaxDecisionInterval *= DruggedReactionScale;

				Brain->SetTuning(Slow);
			}
			break;

		case EGutFix::TamperRack:
			Fixed->SetWeaponSabotaged(true);
			break;

		default:
			break;
		}

		UE_LOG(LogGut, Log, TEXT("Window: %s arranged for %s, for %.0f L"), *UEnum::GetDisplayValueAsText(WindowFix).ToString(), *Fixed->GetDisplayName().ToString(), Cost);
	}

	SetBet(EGutBetSide::None, 0.0f);
	SetPhase(EGutFightPhase::Betting);
}

void AGutGameMode::BeginGamblersOwnFight()
{
	TArray<const UGutLadderData*> Ranks;
	GetRanks(Ranks);

	const FGutWindowState& State = GetWindowState();

	WindowSettlement = FGutWindowSettlement();

	// one of the cage's own, from the rank a fixer is thought to deserve
	const UGutLadderData* Rank = Ranks.IsValidIndex(CaughtFixerRank) ? Ranks[CaughtFixerRank] : Ladder.Get();

	if (Rank && !Rank->Rungs.IsEmpty())
	{
		// whichever of that rank is still alive, a different one each time if there is a choice
		int32 Chosen = State.TimesCaught % Rank->Rungs.Num();

		for (int32 i = 0; i < Rank->Rungs.Num(); ++i)
		{
			const int32 Rung = (State.TimesCaught + i) % Rank->Rungs.Num();

			if (!State.Dead.Contains((Ranks.IndexOfByKey(Rank) * 100) + Rung))
			{
				Chosen = Rung;
				break;
			}
		}

		MakeFighter(EnemyFighter, Rank->Rungs[Chosen], false);
	}

	// the fixer goes in as it stands: no rack, and a collar for the occasion
	PlayerFighter->EquipWeapon(nullptr);
	PlayerFighter->SetCollared(true);

	UE_LOG(LogGut, Log, TEXT("Window: the fixer goes into the cage against %s"), *EnemyFighter->GetDisplayName().ToString());

	SetPhase(EGutFightPhase::Intro);
}

void AGutGameMode::SettleWindowFight(const AGutFighter* DeadFighter)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	FGutWindowState& State = GameInstance->Window;

	// read before anything is settled: once it is, the bill has moved on
	const FGutWindowFight Fight = GetWindowFight();

	WindowSettlement = FGutWindowSettlement();
	WindowSettlement.bSettled = true;

	if (State.bCaught)
	{
		// the gambler's own fight, for fixing
		WindowSettlement.bOwnFight = true;
		WindowSettlement.bSurvived = DeadFighter != PlayerFighter;

		if (WindowSettlement.bSurvived)
		{
			// it has paid: the window is open to it again, and tonight's bill goes on.
			// The cage's verdict is final: whoever walks out alive owes nothing.
			WindowSettlement.DebtCleared = State.Debt;

			State.bCaught = false;
			State.Debt = 0.0f;
		}
		else
		{
			// there is nobody left to bet
			State = FGutWindowState();
		}

		UE_LOG(LogGut, Log, TEXT("Window: the fixer %s"), WindowSettlement.bSurvived ? TEXT("walks out") : TEXT("is dead"));

		return;
	}

	WindowSettlement.Winner = DeadFighter == PlayerFighter ? 1 : 0;
	WindowSettlement.Backed = BetSide == EGutBetSide::None ? -1 : (BetSide == EGutBetSide::Player ? 0 : 1);
	WindowSettlement.Stake = BetStake;
	WindowSettlement.Fix = WindowFix;
	WindowSettlement.FixedCorner = WindowFixCorner;

	// was it noticed?
	if (WindowFix != EGutFix::None)
	{
		const float Roll = NextWindowRoll >= 0.0f ? NextWindowRoll : FMath::FRand();

		NextWindowRoll = -1.0f;

		WindowSettlement.bCaught = Roll < GetFixDetectChance(WindowFix);

		State.Heat = FMath::Clamp(State.Heat + (WindowSettlement.bCaught ? HeatWhenCaught : HeatPerFix), 0.0f, 1.0f);
	}

	if (WindowSettlement.bCaught)
	{
		// the bet is void and the stake is kept, there is a fine, and the next fight in the cage is the fixer's own
		WindowSettlement.Fine = FMath::Min(FixFine, FMath::Max(State.Water, 0.0f));

		State.Water -= WindowSettlement.Fine;
		State.Fines += WindowSettlement.Fine;
		State.bCaught = true;
		++State.TimesCaught;
	}
	else if (WindowSettlement.Backed == WindowSettlement.Winner)
	{
		WindowSettlement.Return = BetStake * BetReturnPerLitre;

		// the lender stands at the window when somebody who owes is paid
		WindowSettlement.DebtPaid = FMath::Min(State.Debt, FMath::RoundToFloat(WindowSettlement.Return * DebtShareOfWinnings));

		State.Debt -= WindowSettlement.DebtPaid;
		State.Water += WindowSettlement.Return - WindowSettlement.DebtPaid;
		State.Returned += WindowSettlement.Return;
	}

	// the one on the floor does not fight again
	const int32 Loser = 1 - WindowSettlement.Winner;

	State.Dead.AddUnique((Fight.Rank[Loser] * 100) + Fight.Rung[Loser]);

	++State.Fight;

	// a gambler with nothing left to stake is staked by the lender, who writes it down
	const float Step = Ladder ? Ladder->BetStep : 5.0f;

	if (State.Water < Step && !State.bCaught && CameByDoor() && GameInstance->IsLenderInTown())
	{
		// where the lender has a table of her own she lends at it, to whoever comes and asks
		WindowSettlement.bSentToLender = true;
	}
	else if (State.Water < Step && !State.bCaught)
	{
		if (State.Debt + LenderOwed > LenderLimit)
		{
			// the lender has lent enough. What is owed is paid in the cage.
			State.bCaught = true;

			WindowSettlement.bCalledIn = true;
		}
		else
		{
			State.Water += LenderStake;
			State.Debt += LenderOwed;

			WindowSettlement.bStakedAgain = true;
		}
	}

	GameInstance->SaveRun();

	UE_LOG(LogGut, Log, TEXT("Window: %s is dead. The window pays %.0f L on %.0f%s, %.0f L in hand"), *DeadFighter->GetDisplayName().ToString(),
		WindowSettlement.Return, WindowSettlement.Stake, WindowSettlement.bCaught ? TEXT(", and the fix was noticed") : TEXT(""), State.Water);
}

void AGutGameMode::LeaveWindowNight()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (Phase != EGutFightPhase::Gate || !IsAtWindow())
	{
		return;
	}

	TurnWindowNight();

	// the new night waits for whoever has had enough of this one and goes back down to the street
	if (bLeaveChosen && GameInstance->IsStreetOpen())
	{
		UE_LOG(LogGut, Log, TEXT("Window: back down to the street after the night's reckoning"));

		GameInstance->GoToStreet(this);
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true);
}

void AGutGameMode::TurnWindowNight()
{
	FGutWindowState& State = GetGutGameInstance()->Window;

	// a new night: a new bill, the sums start again, and the pit boss has had a night to forget
	++State.Night;

	State.Fight = 0;
	State.Staked = 0.0f;
	State.Returned = 0.0f;
	State.FixSpend = 0.0f;
	State.Fines = 0.0f;
	State.Heat = FMath::Max(0.0f, State.Heat - HeatLostPerNight);
	State.DeadBeforeTonight = State.Dead.Num();

	// when the cage has killed nearly everybody there is new meat: the roster starts again
	TArray<FGutWindowFight> Bill;

	BuildWindowBill(State, Bill);

	if (Bill.Num() < FMath::Min(WindowFightsPerNight, 2))
	{
		State.Dead.Reset();
		State.DeadBeforeTonight = 0;
	}
}

void AGutGameMode::StartSpectating()
{
	APlayerController* Viewer = GetWorld()->GetFirstPlayerController();

	// already handed over
	if (!PlayerFighter || !EnemyFighter || (Viewer && Viewer->GetPawn() != PlayerFighter && SpectatorCamera))
	{
		return;
	}

	UClass* BrainClass = EnemyFighter->GetController() ? EnemyFighter->GetController()->GetClass() : nullptr;

	if (Viewer && Viewer->GetPawn() == PlayerFighter)
	{
		// a controller with nobody to drive would otherwise keep taking the view back for itself
		Viewer->bAutoManageActiveCameraTarget = false;
		Viewer->UnPossess();
	}

	if (BrainClass && !Cast<AGutOpponentController>(PlayerFighter->GetController()))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AController* Brain = GetWorld()->SpawnActor<AController>(BrainClass, PlayerFighter->GetActorLocation(), PlayerFighter->GetActorRotation(), Params))
		{
			Brain->Possess(PlayerFighter);
		}
	}

	// both keep their eyes on each other, as the cage's own always do
	PlayerFighter->SetSquareUp(true);

	if (!SpectatorCamera)
	{
		SpectatorCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(0.0f, -420.0f, GutWindowConstants::CameraHeight), FRotator(0.0f, 90.0f, 0.0f));

		if (SpectatorCamera)
		{
			SpectatorCamera->GetCameraComponent()->SetFieldOfView(62.0f);
			SpectatorCamera->GetCameraComponent()->SetConstraintAspectRatio(false);

			SpectatorLookAt = FVector(0.0f, 0.0f, GutWindowConstants::CameraLookHeight);
		}
	}

	if (Viewer && SpectatorCamera)
	{
		Viewer->SetViewTarget(SpectatorCamera);
	}
}

void AGutGameMode::UpdateSpectatorCamera(float DeltaTime)
{
	if (!SpectatorCamera || !PlayerFighter || !EnemyFighter)
	{
		return;
	}

	// from one side of the line between the two of them, far enough off to keep both in the picture
	const FVector First = PlayerFighter->GetActorLocation();
	const FVector Second = EnemyFighter->GetActorLocation();
	const FVector Middle = (First + Second) * 0.5f;
	const float Apart = FVector::Dist2D(First, Second);

	FVector Side = FVector::CrossProduct((Second - First).GetSafeNormal2D(), FVector::UpVector);

	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}

	const float Back = FMath::Clamp(250.0f + (Apart * 0.7f), 300.0f, 560.0f);

	// whichever side the camera is already on, so it does not jump across when they turn about each other
	FVector Wanted = Middle + (Side * Back);

	if (FVector::DistSquared2D(Middle - (Side * Back), SpectatorCamera->GetActorLocation()) < FVector::DistSquared2D(Wanted, SpectatorCamera->GetActorLocation()))
	{
		Wanted = Middle - (Side * Back);
	}

	// inside the pool, at about head height
	Wanted.X = FMath::Clamp(Wanted.X, -650.0f, 650.0f);
	Wanted.Y = FMath::Clamp(Wanted.Y, -450.0f, 450.0f);
	Wanted.Z = GutWindowConstants::CameraHeight;

	const FVector At(Middle.X, Middle.Y, GutWindowConstants::CameraLookHeight);

	// it follows, it does not snap, except to where it starts from
	if (!bSpectatorPlaced)
	{
		bSpectatorPlaced = true;

		SpectatorLookAt = At;
		SpectatorCamera->SetActorLocation(Wanted);
	}

	SpectatorLookAt = FMath::VInterpTo(SpectatorLookAt, At, DeltaTime, 4.0f);

	const FVector Place = FMath::VInterpTo(SpectatorCamera->GetActorLocation(), Wanted, DeltaTime, 2.0f);

	SpectatorCamera->SetActorLocationAndRotation(Place, (SpectatorLookAt - Place).Rotation());

	// which of them is on which side of the picture, for the names on the screens. Not once the bell has gone: the camera
	// keeps to its side of them, and where it cannot, a name that changed sides in the middle of a fight would be worse
	if (Phase != EGutFightPhase::Fighting && Phase != EGutFightPhase::PlayerWon && Phase != EGutFightPhase::PlayerLost)
	{
		const float Across = FVector::DotProduct(First - Second, SpectatorCamera->GetActorRightVector());

		// seen end on, neither of them is on a side
		if (FMath::Abs(Across) > 30.0f)
		{
			bFirstCornerOnRight = Across > 0.0f;
		}
	}

	if (APlayerController* Viewer = GetWorld()->GetFirstPlayerController(); Viewer && Viewer->GetViewTarget() != SpectatorCamera)
	{
		Viewer->SetViewTarget(SpectatorCamera);
	}
}
