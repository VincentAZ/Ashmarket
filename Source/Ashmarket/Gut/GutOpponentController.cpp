// Ashmarket prototype: the Gut.

#include "GutOpponentController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GutFighter.h"
#include "GutHazard.h"
#include "GutWeaponData.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace GutOpponentConstants
{
	/** To knock a staggered target onto a hazard the opponent has to be this nearly straight behind it: the cosine of the angle */
	const float CageFinishLine = 0.8f;

	/** How many places along the way a blow would send the target are looked at, to see whether it would touch the hazard */
	const int32 CageFinishLooks = 8;

	/** How far ahead the opponent feels for a wall */
	const float WallProbeDistance = 120.0f;

	/** How much narrower than the fighter the wall probe is, so it does not start out touching a wall the fighter is standing against */
	const float WallProbeClearance = 3.0f;

	/** A swing counts as a threat this far beyond where it could actually reach */
	const float ThreatMargin = 60.0f;

	/** A swing counts as a threat when the opponent is within this angle of the attacker's facing */
	const float ThreatHalfAngle = 100.0f;

	/** Time the guard stays up after the last threat has passed */
	const float GuardReleaseDelay = 0.15f;

	/** How far beyond its circling distance the target can get before the opponent walks after it */
	const float ChaseMargin = 140.0f;

	/** Below this much stamina a guard is not worth holding */
	const float MinGuardStamina = 12.0f;

	/** Time between two answers to being crowded */
	const float CrowdedDecisionInterval = 0.5f;

	/** Time a cornered opponent stands and fights before it tries to back off again */
	const float CorneredTime = 2.0f;

	/** Shortest and longest time the opponent circles one way before it thinks about turning round */
	const float MinCircleTime = 1.5f;
	const float MaxCircleTime = 4.0f;

	/** How far ahead the opponent looks for a hazard before it walks somewhere */
	const float HazardLookAhead = 140.0f;

	/** How far clear of a hazard the opponent keeps the edge of its body */
	const float HazardMargin = 45.0f;

	/** How much further off a hazard the most cunning fighter keeps its own body */
	const float CunningMargin = 40.0f;

	/** How near its target has to be to a hazard for a cunning fighter to start working it round to it */
	const float CunningReach = 420.0f;

	/** How much likelier the most cunning fighter is to attack when a heavy blow would put its target onto a hazard */
	const float CunningAttack = 0.4f;

	/** Turns, in degrees away from the way it wants to go, that the opponent tries when a hazard is in the way */
	const float HazardDetourAngles[] = { 30.0f, 60.0f, 90.0f, 120.0f, 150.0f };

	/** Time the way must stay clear before the opponent forgets which side it was going round a hazard */
	const float DetourMemoryTime = 0.6f;

	/** How far to either side the opponent looks for a wall when it picks a side to go round a hazard */
	const float DetourRoomDistance = 2000.0f;

	/** How far off straight at a hazard the opponent must be walking for one side to count as the way that leads away from it */
	const float DetourDecisiveTurn = 0.15f;
}

void AGutOpponentController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// listen for how the fighter's attacks go, and for what lands on it
	if (AGutFighter* Fighter = Cast<AGutFighter>(InPawn))
	{
		Fighter->OnAttackLanded.AddDynamic(this, &AGutOpponentController::OnFighterAttackLanded);
		Fighter->OnAttackReceived.AddDynamic(this, &AGutOpponentController::OnFighterAttackReceived);
		Fighter->OnHazardHit.AddDynamic(this, &AGutOpponentController::OnFighterHazardHit);
	}
}

void AGutOpponentController::OnUnPossess()
{
	if (AGutFighter* Fighter = Cast<AGutFighter>(GetPawn()))
	{
		Fighter->OnAttackLanded.RemoveDynamic(this, &AGutOpponentController::OnFighterAttackLanded);
		Fighter->OnAttackReceived.RemoveDynamic(this, &AGutOpponentController::OnFighterAttackReceived);
		Fighter->OnHazardHit.RemoveDynamic(this, &AGutOpponentController::OnFighterHazardHit);
	}

	Super::OnUnPossess();
}

void AGutOpponentController::NotifyIncomingAttack(AGutFighter* Fighter, AGutFighter* Attacker)
{
	// if a block or dodge has come due, do it now rather than a moment too late
	if (IsBrainRunning() && Attacker && Attacker == Fighter->GetOpponent())
	{
		UpdateReaction(Fighter, Attacker);
	}
}

void AGutOpponentController::OnFightStarted(AGutFighter* Fighter, AGutFighter* Target)
{
	// face the target from now on, whichever way the fighter walks
	Fighter->SetSquareUp(true);

	PendingReaction = EReaction::None;
	JudgedAttackSerial = Target->GetAttackSerial();
	PunishJudgedSerial = Target->GetAttackSerial();
	ResolvedAttackSerial = -1;

	bSwingThrown = false;
	bCounterQueued = false;
	bGuardWhenFree = false;
	NoRetreatUntilTime = 0.0f;

	CircleDirection = FMath::RandBool() ? 1.0f : -1.0f;
	DetourSide = 0.0f;

	// the cage does not change during a fight, so look round it once
	Hazards.Reset();

	for (TActorIterator<AGutHazard> It(GetWorld()); It; ++It)
	{
		Hazards.Add(*It);
	}

	SetBrainState(EGutOpponentState::Approach);
}

void AGutOpponentController::OnFightStopped(AGutFighter* Fighter)
{
	if (Fighter)
	{
		Fighter->SetBlocking(false);
		Fighter->SetSquareUp(false);
	}
}

void AGutOpponentController::TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime)
{
	Fought = Target;

	// see what is coming before deciding anything else
	UpdateReaction(Fighter, Target);

	const EGutFighterState FighterState = Fighter->GetFighterState();

	// a fighter that is reeling, mid dodge, beaten or busy killing cannot be steered
	if (FighterState == EGutFighterState::Stunned || FighterState == EGutFighterState::Dodging
		|| FighterState == EGutFighterState::Staggered || FighterState == EGutFighterState::Finishing)
	{
		return;
	}

	// the target is beaten: everything else can wait
	if (Target->IsStaggered())
	{
		if (State != EGutOpponentState::Finish)
		{
			Fighter->SetBlocking(false);

			bCounterQueued = false;
			bSwingThrown = false;
			PendingReaction = EReaction::None;

			FinishStartTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Tuning.MinFinishDelay, Tuning.MaxFinishDelay);
			PlannedFinisher = FMath::FRand() < Tuning.SlowFinisherChance ? EGutFinisherStyle::Slow : EGutFinisherStyle::Clean;

			// now and then, with the spikes or the cable close behind it, the cage is let finish it
			// (-GutCageFinish=0.3 on the command line is another chance for everybody, and 0 is never)
			static float AskedChance = -1.0f;
			static const bool bAsked = FParse::Value(FCommandLine::Get(), TEXT("GutCageFinish="), AskedChance);

			CageFinish = FMath::FRand() < (bAsked ? AskedChance : Tuning.CageFinishChance) ? FindCageFinish(Fighter, Target) : nullptr;
			bCageBlowThrown = false;
			CageRoundSide = 0.0f;

			if (CageFinish.IsValid())
			{
				UE_LOG(LogGut, Log, TEXT("%s means to put %s on %s"), *Fighter->GetName(), *Target->GetName(), *CageFinish->GetName());
			}

			SetBrainState(EGutOpponentState::Finish);
		}

		TickFinish(Fighter, Target);
		return;
	}

	// the target got back up, or the opponent has only just got back up itself
	if (State == EGutOpponentState::Finish)
	{
		EnterHold();
	}

	// running on empty: whatever the plan was, back off. Not with a swing on its way, and not mid swing.
	const bool bFree = FighterState == EGutFighterState::Idle || FighterState == EGutFighterState::Blocking;
	const bool bSpent = Fighter->GetStamina() < Tuning.RetreatStamina;

	// there is no walking away from a target that is right on top of it. Stand and trade until there is room,
	// unless there is not even enough left to hold a guard.
	const bool bRoomToRun = Fighter->GetGapTo(Target) >= GetHoldGapMin(Target) || Fighter->GetStamina() < GutOpponentConstants::MinGuardStamina;

	if (bFree && bSpent && bRoomToRun && State != EGutOpponentState::Retreat && GetWorld()->GetTimeSeconds() >= NoRetreatUntilTime && !IsThreatened(Fighter, Target))
	{
		EnterRetreat(Fighter);
	}

	switch (State)
	{
	case EGutOpponentState::Approach:
		TickApproach(Fighter, Target);
		break;

	case EGutOpponentState::Hold:
		TickHold(Fighter, Target);
		break;

	case EGutOpponentState::Attack:
		TickAttack(Fighter, Target);
		break;

	case EGutOpponentState::Guard:
		TickGuard(Fighter, Target);
		break;

	case EGutOpponentState::Retreat:
		TickRetreat(Fighter, Target);
		break;

	default:
		break;
	}
}

void AGutOpponentController::SetBrainState(EGutOpponentState NewState)
{
	// an answer that was never thrown is forgotten with the plan it was part of
	bCountering = false;

	// run with -LogCmds="LogGut Verbose" to follow the brain's thinking
	if (const AGutFighter* Fighter = Cast<AGutFighter>(GetPawn()))
	{
		UE_LOG(LogGut, Verbose, TEXT("%s brain: %s -> %s (gap %.0f, stamina %.0f)"),
			*Fighter->GetName(), *UEnum::GetDisplayValueAsText(State).ToString(), *UEnum::GetDisplayValueAsText(NewState).ToString(),
			Fighter->GetGapTo(Fighter->GetOpponent()), Fighter->GetStamina());
	}

	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
}

void AGutOpponentController::EnterHold(float ExtraDelay)
{
	NextDecisionTime = GetWorld()->GetTimeSeconds() + ExtraDelay + (FMath::FRandRange(Tuning.MinDecisionInterval, Tuning.MaxDecisionInterval) * GetTempoScale());

	SetBrainState(EGutOpponentState::Hold);
}

void AGutOpponentController::EnterGuard(AGutFighter* Fighter, float UntilTime)
{
	Fighter->SetBlocking(true);

	GuardUntilTime = (State == EGutOpponentState::Guard) ? FMath::Max(GuardUntilTime, UntilTime) : UntilTime;

	if (State != EGutOpponentState::Guard)
	{
		SetBrainState(EGutOpponentState::Guard);
	}
}

void AGutOpponentController::EnterRetreat(AGutFighter* Fighter)
{
	Fighter->SetBlocking(false);

	bCounterQueued = false;
	bSwingThrown = false;

	SetBrainState(EGutOpponentState::Retreat);
}

bool AGutOpponentController::BeginAttack(AGutFighter* Fighter, AGutFighter* Target)
{
	// in a fight of more than two there is only room for so many round one body
	if (bWaitsItsTurn)
	{
		return false;
	}

	// keep something back to guard with afterwards
	const float Available = Fighter->GetStamina() - Tuning.AttackStaminaReserve;

	// a heavy is the answer to a raised guard
	float HeavyChance = Target->IsBlocking() ? Tuning.HeavyChanceAgainstGuard : Tuning.HeavyChance;

	// and it is the blow that sends a target standing with its back to the spikes or the cable onto them
	if (WouldKnockOntoHazard(Fighter, Target, Fighter->GetHeavyAttack().Knockback))
	{
		HeavyChance = FMath::Max(HeavyChance, Tuning.HeavyChanceIntoHazard);
	}

	bPlanHeavy = FMath::FRand() < HeavyChance && Available >= Fighter->GetHeavyAttack().StaminaCost;

	if (!bPlanHeavy)
	{
		const FGutAttackDef* Light = Fighter->GetNextLightAttack();

		if (!Light || Available < Light->StaminaCost)
		{
			return false;
		}

		PlannedLights = FMath::RandRange(1, FMath::Max(1, Tuning.MaxLightChain));
	}

	bSwingThrown = false;

	Fighter->SetBlocking(false);

	SetBrainState(EGutOpponentState::Attack);

	return true;
}

void AGutOpponentController::UpdateReaction(AGutFighter* Fighter, AGutFighter* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const EGutFighterState FighterState = Fighter->GetFighterState();
	const bool bFree = FighterState == EGutFighterState::Idle || FighterState == EGutFighterState::Blocking;

	// a new swing is coming: make up the opponent's mind about it once, the way a person would
	if (Target->GetAttackSerial() != JudgedAttackSerial && IsThreatened(Fighter, Target))
	{
		JudgedAttackSerial = Target->GetAttackSerial();
		PendingReaction = EReaction::None;

		const FGutAttackDef& Incoming = Target->GetCurrentAttack();
		const bool bHeavy = Target->GetCurrentAttackType() == EGutAttackType::Heavy;
		const bool bCanBlock = Fighter->GetStamina() >= Fighter->GetBlockCost(Incoming);
		const bool bCanDodge = Fighter->GetStamina() >= Fighter->GetDodgeStaminaCost();

		// already behind a guard that will hold: nothing more to do. A fighter that is committed to something else cannot react at all.
		const bool bGuardWillHold = Fighter->IsBlocking() && bCanBlock;

		if (bFree && !bGuardWillHold)
		{
			const float BlockChance = bCanBlock ? GetGuardChance(Fighter, bHeavy ? Tuning.BlockHeavyChance : Tuning.BlockLightChance) : 0.0f;
			const float DodgeChance = (bHeavy && bCanDodge) ? Tuning.DodgeHeavyChance : 0.0f;

			const float Roll = FMath::FRand();
			const float Delay = FMath::FRandRange(Tuning.MinReactionTime, Tuning.MaxReactionTime);

			// the delay runs from the moment the swing started, not from the moment this brain happened to look,
			// so that two opponents fighting each other react equally fast whichever of them takes its turn first
			const float SwingStartTime = Target->GetAttackStartTime();

			if (Roll < BlockChance)
			{
				PendingReaction = EReaction::Block;
				ReactionTime = SwingStartTime + Delay;
			}
			else if (Roll < BlockChance + DodgeChance)
			{
				// a dodge is only safe for a moment, so it waits until the blow is about to land
				PendingReaction = EReaction::Dodge;
				ReactionTime = SwingStartTime + FMath::Max(Delay, Incoming.Windup - Tuning.DodgeLeadTime);
			}
		}
	}

	if (PendingReaction == EReaction::None || Now < ReactionTime)
	{
		return;
	}

	const EReaction Reaction = PendingReaction;

	PendingReaction = EReaction::None;

	// too late, or no longer able
	if (!bFree || Target->GetAttackSerial() != JudgedAttackSerial || !IsThreatened(Fighter, Target))
	{
		return;
	}

	if (Reaction == EReaction::Block)
	{
		EnterGuard(Fighter, Now + GutOpponentConstants::GuardReleaseDelay);
	}
	else
	{
		const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();
		const FVector Sideways = FVector::CrossProduct(FVector::UpVector, ToTarget) * (FMath::RandBool() ? 1.0f : -1.0f);

		// straight back or to one side, whichever has room
		FVector Direction = FMath::RandBool() ? -ToTarget : Sideways;

		if (IsWallClose(Fighter, Direction))
		{
			Direction = IsWallClose(Fighter, -Sideways) ? Sideways : -Sideways;
		}

		Fighter->SetBlocking(false);

		if (Fighter->TryDodge(Direction))
		{
			EnterHold();
		}
	}
}

void AGutOpponentController::TickApproach(AGutFighter* Fighter, AGutFighter* Target)
{
	bool bRoundHazard = false;

	const FVector Way = GetWayTo(Fighter, Target, bRoundHazard);

	// close enough to circle, unless there is a hazard in between: then being close is no use until it has been walked round
	if (!bRoundHazard && Fighter->GetGapTo(Target) <= GetHoldGapMax(Target))
	{
		EnterHold();
		return;
	}

	Move(Fighter, Way, Tuning.ApproachSpeedScale);
}

void AGutOpponentController::TickHold(AGutFighter* Fighter, AGutFighter* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Gap = Fighter->GetGapTo(Target);
	const float HoldGapMin = GetHoldGapMin(Target);
	const float HoldGapMax = GetHoldGapMax(Target);
	const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();

	// just been hit and decided to cover up
	if (bGuardWhenFree)
	{
		bGuardWhenFree = false;

		EnterGuard(Fighter, Now + FMath::FRandRange(Tuning.MinGuardTime, Tuning.MaxGuardTime));
		return;
	}

	bool bRoundHazard = false;

	GetWayTo(Fighter, Target, bRoundHazard);

	// the target has walked off, or has put a hazard between them
	if (bRoundHazard || Gap > HoldGapMax + GutOpponentConstants::ChaseMargin)
	{
		SetBrainState(EGutOpponentState::Approach);
		return;
	}

	// the target has swung and is stuck recovering: maybe step in and make it pay
	if (Target->GetFighterState() == EGutFighterState::Recovery && Target->GetAttackSerial() != PunishJudgedSerial)
	{
		PunishJudgedSerial = Target->GetAttackSerial();

		if (FMath::FRand() < Tuning.PunishChance && BeginAttack(Fighter, Target))
		{
			return;
		}
	}

	// the target has walked in close: do not just back away from it, swing or cover up
	if (Gap < HoldGapMin && Now >= NextCrowdedDecisionTime)
	{
		NextCrowdedDecisionTime = Now + GutOpponentConstants::CrowdedDecisionInterval;

		if (FMath::FRand() < Tuning.CrowdedAttackChance)
		{
			BeginAttack(Fighter, Target);
		}
		else if (!Tuning.bNeverCovers)
		{
			EnterGuard(Fighter, Now + FMath::FRandRange(Tuning.MinGuardTime, Tuning.MaxGuardTime));
		}

		// somebody paid to lose does not cover up when it is crowded either: this was where it still did, every time,
		// and behind a shield, which guards cheaply, it could not be killed and the fight never ended
	}
	else if (Now >= NextDecisionTime)
	{
		Decide(Fighter, Target);
	}

	if (State != EGutOpponentState::Hold)
	{
		return;
	}

	// keep the distance: out of the target's light attacks, inside a step of its own
	if (Gap < HoldGapMin)
	{
		if (!IsWallClose(Fighter, -ToTarget))
		{
			Move(Fighter, -ToTarget, Tuning.RetreatSpeedScale);
		}
		else
		{
			Circle(Fighter, ToTarget, Tuning.RetreatSpeedScale);
		}
	}
	else if (Gap > HoldGapMax)
	{
		Move(Fighter, ToTarget, Tuning.ApproachSpeedScale);
	}
	else
	{
		Circle(Fighter, ToTarget, Tuning.CircleSpeedScale);
	}
}

void AGutOpponentController::TickAttack(AGutFighter* Fighter, AGutFighter* Target)
{
	// mid swing: nothing to decide until it is over
	if (Fighter->IsAttacking())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// a swing has just finished. Carry on with the chain only if it is working.
	if (bSwingThrown)
	{
		bSwingThrown = false;

		bool bContinue = false;

		if (!bPlanHeavy && PlannedLights > 0)
		{
			if (LastSwingResult == EGutHitResult::Hit || LastSwingResult == EGutHitResult::GuardBroken)
			{
				bContinue = FMath::FRand() < Tuning.ChainAfterHitChance;
			}
			else if (LastSwingResult == EGutHitResult::Blocked)
			{
				bContinue = FMath::FRand() < Tuning.ChainAfterBlockChance;
			}
		}

		if (!bContinue)
		{
			EnterHold();
			return;
		}

		// the next swing gets its own time to get into range
		StateStartTime = Now;
	}

	const FGutAttackDef* NextLight = Fighter->GetNextLightAttack();
	const FGutAttackDef& Attack = (bPlanHeavy || !NextLight) ? Fighter->GetHeavyAttack() : *NextLight;

	// a heavy carries the fighter forward, so it can start from further out
	const float StrikeGap = (Attack.Reach * (bCountering ? 1.0f : Tuning.StrikeReachFraction)) + (bPlanHeavy ? Attack.LungeDistance * 0.5f : 0.0f);

	if (Fighter->GetGapTo(Target) > StrikeGap)
	{
		// the target is getting away: give up rather than chase it round the cage
		if (Now - StateStartTime > Tuning.MaxClosingTime)
		{
			EnterHold();
			return;
		}

		bool bRoundHazard = false;

		const FVector Way = GetWayTo(Fighter, Target, bRoundHazard);

		// there is a hazard in the way: the swing can wait until it has been walked round
		if (bRoundHazard)
		{
			SetBrainState(EGutOpponentState::Approach);
			return;
		}

		Move(Fighter, Way, Tuning.ApproachSpeedScale);
		return;
	}

	// do not swing the last of the stamina away
	if (Fighter->GetStamina() - Tuning.AttackStaminaReserve < Attack.StaminaCost)
	{
		EnterHold();
		return;
	}

	LastSwingResult = EGutHitResult::Ignored;

	const bool bStarted = bPlanHeavy ? Fighter->TryHeavyAttack() : Fighter->TryLightAttack();

	if (!bStarted)
	{
		EnterHold();
		return;
	}

	bSwingThrown = true;
	bCountering = false;

	if (!bPlanHeavy)
	{
		--PlannedLights;
	}
}

void AGutOpponentController::TickGuard(AGutFighter* Fighter, AGutFighter* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();

	const bool bThreatened = IsThreatened(Fighter, Target);

	if (bThreatened)
	{
		// never drop the guard with a swing on its way
		GuardUntilTime = FMath::Max(GuardUntilTime, Now + GutOpponentConstants::GuardReleaseDelay);
	}
	else if (Fighter->GetStamina() < GutOpponentConstants::MinGuardStamina)
	{
		// nothing left to guard with
		GuardUntilTime = Now;
	}

	if (Now >= GuardUntilTime)
	{
		Fighter->SetBlocking(false);

		// hit back after a block
		if (bCounterQueued)
		{
			bCounterQueued = false;

			if (BeginAttack(Fighter, Target))
			{
				// the answer to a blocked blow comes while the one who threw it is still recovering, or it is no answer
				bCountering = true;
				return;
			}
		}

		EnterHold();
		return;
	}

	// give ground slowly if the target crowds in, but stand firm to take a swing on the guard
	const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();

	if (!bThreatened && Fighter->GetGapTo(Target) < GetHoldGapMin(Target) && !IsWallClose(Fighter, -ToTarget))
	{
		Move(Fighter, -ToTarget, 1.0f);
	}
}

void AGutOpponentController::TickRetreat(AGutFighter* Fighter, AGutFighter* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// got its breath back, or has run for long enough
	if (Fighter->GetStamina() >= Tuning.RecoveredStamina || Now - StateStartTime > Tuning.MaxRetreatTime)
	{
		EnterHold();
		return;
	}

	if (Fighter->GetGapTo(Target) >= Tuning.RetreatGap)
	{
		return;
	}

	const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();

	if (!IsWallClose(Fighter, -ToTarget))
	{
		Move(Fighter, -ToTarget, Tuning.RetreatSpeedScale);
	}
	else if (!Circle(Fighter, ToTarget, Tuning.RetreatSpeedScale))
	{
		// cornered: stop running and fight
		NoRetreatUntilTime = Now + GutOpponentConstants::CorneredTime;

		EnterHold();
	}
}

void AGutOpponentController::TickFinish(AGutFighter* Fighter, AGutFighter* Target)
{
	// take a moment over it
	if (GetWorld()->GetTimeSeconds() < FinishStartTime)
	{
		return;
	}

	// the cage is to do it: round behind the target and knock it on, for as long as that can still be done
	if (CageFinish.IsValid())
	{
		if (TickCageFinish(Fighter, Target))
		{
			return;
		}

		UE_LOG(LogGut, Log, TEXT("%s gives the cage up and finishes %s by hand"), *Fighter->GetName(), *Target->GetName());

		CageFinish = nullptr;
	}

	// walk in until the target is in reach, then end it
	if (Fighter->GetGapTo(Target) > Fighter->GetFinisherRange() * Tuning.StrikeReachFraction)
	{
		bool bRoundHazard = false;

		Move(Fighter, GetWayTo(Fighter, Target, bRoundHazard), Tuning.ApproachSpeedScale);
		return;
	}

	Fighter->TryFinisher(PlannedFinisher);
}

float AGutOpponentController::GetLightThreatRange(const AGutFighter* Target) const
{
	const FGutAttackDef* Light = Target ? Target->GetNextLightAttack() : nullptr;

	return Light ? Light->Reach + Light->LungeDistance : 0.0f;
}

float AGutOpponentController::GetTempoScale() const
{
	const AGutFighter* Fighter = Cast<AGutFighter>(GetPawn());
	const UGutWeaponData* Armament = Fighter ? Fighter->GetArmament() : nullptr;

	if (!Armament || Tuning.TempoReferenceTime <= 0.0f || Armament->GetLightTime() <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(Armament->GetLightTime() / Tuning.TempoReferenceTime, Tuning.MinTempoScale, Tuning.MaxTempoScale);
}

float AGutOpponentController::GetGuardChance(const AGutFighter* Fighter, float BaseChance) const
{
	// somebody who has been told not to guard at all does not, whatever is in its hands. Without this a fighter paid to lose
	// went on guarding behind anything that guards cheaply: a shield made four chances in ten out of its none, and against a weapon
	// that cannot break a guard the fight never ended.
	if (BaseChance <= 0.0f)
	{
		return 0.0f;
	}

	const UGutWeaponData* Armament = Fighter->GetArmament();

	return Armament ? FMath::Clamp(1.0f - ((1.0f - BaseChance) * Armament->BlockCostScale), 0.0f, 1.0f) : BaseChance;
}

float AGutOpponentController::GetHoldGapMin(const AGutFighter* Target) const
{
	return GetLightThreatRange(Target) + Tuning.HoldMarginMin;
}

float AGutOpponentController::GetHoldGapMax(const AGutFighter* Target) const
{
	return GetLightThreatRange(Target) + Tuning.HoldMarginMax;
}

void AGutOpponentController::Decide(AGutFighter* Fighter, AGutFighter* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();

	NextDecisionTime = Now + (FMath::FRandRange(Tuning.MinDecisionInterval, Tuning.MaxDecisionInterval) * GetTempoScale());

	const float Roll = FMath::FRand();

	// somebody who uses the cage does not let a target stand with its back to the spikes for long
	const float AttackChance = Tuning.AttackChance + ((Tuning.Cunning > 0.0f && WouldKnockOntoHazard(Fighter, Target, Fighter->GetHeavyAttack().Knockback))
		? GutOpponentConstants::CunningAttack * Tuning.Cunning : 0.0f);

	if (Roll < AttackChance)
	{
		BeginAttack(Fighter, Target);
	}
	else if (Roll < AttackChance + GetGuardChance(Fighter, Tuning.GuardChance))
	{
		EnterGuard(Fighter, Now + FMath::FRandRange(Tuning.MinGuardTime, Tuning.MaxGuardTime));
	}

	// otherwise: keep circling and look again in a moment
}

bool AGutOpponentController::Circle(AGutFighter* Fighter, const FVector& ToTarget, float SpeedScale)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// change direction now and then, so the circling is not a constant orbit
	if (Now >= NextCircleFlipTime)
	{
		NextCircleFlipTime = Now + FMath::FRandRange(GutOpponentConstants::MinCircleTime, GutOpponentConstants::MaxCircleTime);

		if (FMath::RandBool())
		{
			CircleDirection = -CircleDirection;
		}

		// Somebody who uses the cage goes round the way that brings the target's back to the nearest spikes or cable:
		// toward the side of the target that is away from them, so that the target is between the two.
		if (Tuning.Cunning > 0.0f && FMath::FRand() < Tuning.Cunning)
		{
			const FVector Theirs = Fought.IsValid() ? Fought->GetActorLocation() : Fighter->GetActorLocation();

			FVector Away;

			if (Fought.IsValid() && GetHazardDistance(Theirs, Away) < GutOpponentConstants::CunningReach && !Away.IsNearlyZero())
			{
				const FVector Round = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal2D();

				if (const float Better = FVector::DotProduct(Round, Away.GetSafeNormal2D()); FMath::Abs(Better) > 0.1f)
				{
					CircleDirection = Better > 0.0f ? 1.0f : -1.0f;
				}
			}
		}
	}

	FVector Sideways = FVector::CrossProduct(FVector::UpVector, ToTarget) * CircleDirection;

	// turn back at a wall
	if (IsWallClose(Fighter, Sideways))
	{
		CircleDirection = -CircleDirection;
		Sideways = -Sideways;

		if (IsWallClose(Fighter, Sideways))
		{
			return false;
		}
	}

	Move(Fighter, Sideways, SpeedScale);

	return true;
}

void AGutOpponentController::Move(AGutFighter* Fighter, const FVector& Direction, float SpeedScale)
{
	FVector Wanted = Direction.GetSafeNormal2D();

	if (Wanted.IsNearlyZero())
	{
		return;
	}

	const float Clearance = Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() + GutOpponentConstants::HazardMargin + (GutOpponentConstants::CunningMargin * Tuning.Cunning);

	// already too close to one: get clear first, sliding along it rather than stepping further in
	FVector Away;

	if (GetHazardDistance(Fighter->GetActorLocation(), Away) < Clearance)
	{
		const float Into = FVector::DotProduct(Wanted, Away);

		if (Into < 0.0f)
		{
			Wanted -= Away * Into;
		}

		Fighter->AddMovementInput((Wanted + Away).GetSafeNormal2D(), SpeedScale);
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	if (IsHazardPathClear(Fighter, Wanted))
	{
		// once the way has been clear for a moment, the next hazard is a new choice of side
		if (Now - LastDetourTime > GutOpponentConstants::DetourMemoryTime)
		{
			DetourSide = 0.0f;
		}

		Fighter->AddMovementInput(Wanted, SpeedScale);
		return;
	}

	// something is in the way: find what, and which way leads away from it from where the fighter stands
	const FVector Step = Wanted * GutOpponentConstants::HazardLookAhead;
	const AGutHazard* Blocking = nullptr;

	if (GetHazardDistance(Fighter->GetActorLocation() + (Step * 0.5f), Away, &Blocking) >= Clearance)
	{
		GetHazardDistance(Fighter->GetActorLocation() + Step, Away, &Blocking);
	}

	if (Blocking)
	{
		Blocking->GetDangerDistance(Fighter->GetActorLocation(), Away);
	}

	// turn aside to whichever side leads away from it, which is the side the fighter is already on
	const float TurnAway = FVector::CrossProduct(Wanted, Away).Z;

	if (FMath::Abs(TurnAway) > GutOpponentConstants::DetourDecisiveTurn)
	{
		DetourSide = TurnAway > 0.0f ? 1.0f : -1.0f;
	}
	else if (DetourSide == 0.0f)
	{
		// walking straight at it, neither side leads away. Take the side with more floor: the other is likely a dead end against a wall.
		const FVector Left = Wanted.RotateAngleAxis(-90.0f, FVector::UpVector);

		const float RoomLeft = GetRoomToward(Fighter, Left, GutOpponentConstants::DetourRoomDistance);
		const float RoomRight = GetRoomToward(Fighter, -Left, GutOpponentConstants::DetourRoomDistance);

		DetourSide = RoomLeft > RoomRight ? -1.0f : 1.0f;
	}

	LastDetourTime = Now;

	// turn a little, then more, always to the chosen side, which keeps the hazard on the same hand all the way round
	for (const float Angle : GutOpponentConstants::HazardDetourAngles)
	{
		const FVector Detour = Wanted.RotateAngleAxis(Angle * DetourSide, FVector::UpVector);

		if (IsHazardPathClear(Fighter, Detour))
		{
			Fighter->AddMovementInput(Detour, SpeedScale);
			return;
		}
	}

	// hemmed in: stand still rather than walk into it
}

FVector AGutOpponentController::GetWayTo(const AGutFighter* Fighter, const AGutFighter* Target, bool& bOutRoundHazard) const
{
	const FVector From = Fighter->GetActorLocation();
	const float Clearance = Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() + GutOpponentConstants::HazardMargin;

	FVector Waypoint;

	bOutRoundHazard = AGutHazard::FindWayRound(this, From, Target->GetActorLocation(), Clearance, Waypoint);

	return ((bOutRoundHazard ? Waypoint : Target->GetActorLocation()) - From).GetSafeNormal2D();
}

float AGutOpponentController::GetRoomToward(const AGutFighter* Fighter, const FVector& Direction, float MaxDistance) const
{
	const FVector Start = Fighter->GetActorLocation();
	const FVector End = Start + (Direction.GetSafeNormal2D() * MaxDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Fighter);

	// feel with the same shape the wall probe uses
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() - GutOpponentConstants::WallProbeClearance);

	FHitResult Hit;

	return GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, CollisionShape, QueryParams) ? Hit.Distance : MaxDistance;
}

float AGutOpponentController::GetHazardDistance(const FVector& Point, FVector& OutAwayDirection, const AGutHazard** OutHazard) const
{
	float Nearest = UE_BIG_NUMBER;

	OutAwayDirection = FVector::ZeroVector;

	if (OutHazard)
	{
		*OutHazard = nullptr;
	}

	for (const TWeakObjectPtr<AGutHazard>& Hazard : Hazards)
	{
		// a hazard that is about to go live is given the same room as one that is live
		if (!Hazard.IsValid() || Hazard->GetHazardState() == EGutHazardState::Off)
		{
			continue;
		}

		FVector Away;
		const float Distance = Hazard->GetDangerDistance(Point, Away);

		if (Distance < Nearest)
		{
			Nearest = Distance;
			OutAwayDirection = Away;

			if (OutHazard)
			{
				*OutHazard = Hazard.Get();
			}
		}
	}

	return Nearest;
}

AGutHazard* AGutOpponentController::FindCageFinish(const AGutFighter* Fighter, const AGutFighter* Target) const
{
	// the player is finished by hand, as it always was: this is between two of the cage's own
	if (!Cast<AGutBrainController>(Target->GetController()) || Fighter->IsBeast() || Target->IsBeast() || !Fighter->GetArmament())
	{
		return nullptr;
	}

	// what a blow's drive carries it, less a little: the blow is thrown from straight behind it, or not at all
	const float Carried = (Target->GetDriveDistance() * 0.9f) + Target->GetCapsuleComponent()->GetScaledCapsuleRadius();

	AGutHazard* Found = nullptr;
	float Nearest = Carried;

	for (const TWeakObjectPtr<AGutHazard>& Hazard : Hazards)
	{
		// one that is only about to go live would not hurt it when it got there
		if (!Hazard.IsValid() || !Hazard->IsLive())
		{
			continue;
		}

		FVector Away;
		const float Distance = Hazard->GetDangerDistance(Target->GetActorLocation(), Away);

		if (Distance < Nearest && !Away.IsNearlyZero())
		{
			Nearest = Distance;
			Found = Hazard.Get();
		}
	}

	return Found;
}

bool AGutOpponentController::TickCageFinish(AGutFighter* Fighter, AGutFighter* Target)
{
	const EGutFighterState FighterState = Fighter->GetFighterState();

	// the blow is on its way, or has been thrown: one is all it gets, and if the target is still standing there after
	// it, the cage is given up
	if (FighterState == EGutFighterState::Windup || FighterState == EGutFighterState::Active || FighterState == EGutFighterState::Recovery)
	{
		return true;
	}

	if (bCageBlowThrown || !CageFinish->IsLive())
	{
		return false;
	}

	// not with so little of the stagger left that there would be no time to finish it by hand afterwards
	if (Target->GetStaggerTimeRemaining() < Tuning.CageFinishGiveUpTime)
	{
		return false;
	}

	FVector Away;
	const float Distance = CageFinish->GetDangerDistance(Target->GetActorLocation(), Away);

	if (Away.IsNearlyZero() || Distance > Target->GetDriveDistance() + Target->GetCapsuleComponent()->GetScaledCapsuleRadius())
	{
		return false;
	}

	const FGutAttackDef& Blow = Fighter->GetHeavyAttack();

	const FVector Here = Fighter->GetActorLocation();
	const FVector There = Target->GetActorLocation();
	const FVector ToTarget = (There - Here).GetSafeNormal2D();

	// behind it is the side away from the hazard: from there a blow sends it straight on
	const float Behind = FVector::DotProduct(ToTarget, -Away);
	const float StrikeGap = Blow.Reach * Tuning.StrikeReachFraction;

	// would a blow from here send it onto the hazard? Its body touches when it comes within its own width of it, anywhere
	// along the way it is sent.
	bool bCarriesOn = false;

	for (int32 i = 1; i <= GutOpponentConstants::CageFinishLooks && !bCarriesOn; ++i)
	{
		FVector Out;

		bCarriesOn = CageFinish->GetDangerDistance(There + (ToTarget * (Target->GetDriveDistance() * 0.9f * i / GutOpponentConstants::CageFinishLooks)), Out) <= Target->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}

	if (bCarriesOn && Behind >= GutOpponentConstants::CageFinishLine && Fighter->GetGapTo(Target) <= StrikeGap)
	{
		Fighter->SetBlocking(false);

		bCageBlowThrown = Fighter->TryDrive();

		return bCageBlowThrown;
	}

	// to where the blow is thrown from. Straight there if that does not mean walking through the target; otherwise
	// round it, always the same way.
	const FVector Stand = There + (Away * (StrikeGap + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() - 10.0f));

	FVector Way = (Stand - Here).GetSafeNormal2D();

	if (Behind < GutOpponentConstants::CageFinishLine && FVector::Dist2D(Here, There) < FVector::Dist2D(Here, Stand) + 40.0f)
	{
		if (CageRoundSide == 0.0f)
		{
			CageRoundSide = FVector::CrossProduct(ToTarget, Away).Z >= 0.0f ? 1.0f : -1.0f;
		}

		// across the line to the target, toward the side the hazard is not on, and a little out from it
		Way = ((FVector::CrossProduct(FVector::UpVector, ToTarget) * CageRoundSide) - (ToTarget * 0.25f)).GetSafeNormal2D();
	}

	Move(Fighter, Way, Tuning.ApproachSpeedScale);

	return true;
}

bool AGutOpponentController::WouldKnockOntoHazard(const AGutFighter* Fighter, const AGutFighter* Target, float Knockback) const
{
	const FVector Push = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D() * Knockback;

	// the target touches a hazard when its body does, wherever along the knockback that happens
	const float Radius = Target->GetCapsuleComponent()->GetScaledCapsuleRadius();

	FVector Away;

	return GetHazardDistance(Target->GetActorLocation() + (Push * 0.5f), Away) <= Radius || GetHazardDistance(Target->GetActorLocation() + Push, Away) <= Radius;
}

bool AGutOpponentController::IsHazardPathClear(const AGutFighter* Fighter, const FVector& Direction) const
{
	const float Clearance = Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() + GutOpponentConstants::HazardMargin;
	const FVector Start = Fighter->GetActorLocation();
	const FVector Step = Direction.GetSafeNormal2D() * GutOpponentConstants::HazardLookAhead;

	// look half way and all the way, so a thin cable cannot slip between the two
	FVector Away;

	return GetHazardDistance(Start + (Step * 0.5f), Away) >= Clearance && GetHazardDistance(Start + Step, Away) >= Clearance;
}

bool AGutOpponentController::IsWallClose(const AGutFighter* Fighter, const FVector& Direction) const
{
	// a live hazard is as good as a wall
	if (!IsHazardPathClear(Fighter, Direction))
	{
		return true;
	}

	const FVector Start = Fighter->GetActorLocation();
	const FVector End = Start + (Direction.GetSafeNormal2D() * GutOpponentConstants::WallProbeDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Fighter);

	// feel with something nearly as wide as the fighter, so a wall it is already brushing along counts too.
	// A thin line would miss it, and the fighter would slide along the wall instead of turning back.
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() - GutOpponentConstants::WallProbeClearance);

	return GetWorld()->SweepTestByObjectType(Start, End, FQuat::Identity, ObjectParams, CollisionShape, QueryParams);
}

bool AGutOpponentController::IsThreatened(const AGutFighter* Fighter, const AGutFighter* Target) const
{
	const EGutFighterState TargetState = Target->GetFighterState();

	if (TargetState != EGutFighterState::Windup && TargetState != EGutFighterState::Active)
	{
		return false;
	}

	// a swing that has already landed, been blocked or been dodged is no longer a threat
	if (Target->GetAttackSerial() == ResolvedAttackSerial)
	{
		return false;
	}

	const FGutAttackDef& Incoming = Target->GetCurrentAttack();

	return Target->IsInArc(Fighter, Incoming.Reach + Incoming.LungeDistance + GutOpponentConstants::ThreatMargin, GutOpponentConstants::ThreatHalfAngle);
}

void AGutOpponentController::OnFighterHazardHit(AGutFighter* Fighter, AGutHazard* Hazard)
{
	if (!IsBrainRunning())
	{
		return;
	}

	// like taking a clean hit: whatever the plan was, it is gone
	PendingReaction = EReaction::None;
	bCounterQueued = false;
	bSwingThrown = false;
	bGuardWhenFree = FMath::FRand() < GetGuardChance(Fighter, Tuning.GuardAfterHitChance);

	Fighter->SetBlocking(false);

	if (State != EGutOpponentState::Retreat)
	{
		EnterHold();
	}
}

void AGutOpponentController::OnFighterAttackLanded(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result)
{
	LastSwingResult = Result;
}

void AGutOpponentController::OnFighterAttackReceived(AGutFighter* Fighter, AGutFighter* Other, EGutHitResult Result)
{
	// a brain that is switched off leaves its fighter alone
	if (!IsBrainRunning())
	{
		return;
	}

	// this swing is dealt with, one way or the other
	ResolvedAttackSerial = Other ? Other->GetAttackSerial() : -1;

	switch (Result)
	{
	case EGutHitResult::Blocked:

		// maybe hit back while the attacker is still recovering
		if (FMath::FRand() < Tuning.CounterChance)
		{
			bCounterQueued = true;
			GuardUntilTime = GetWorld()->GetTimeSeconds();
		}
		break;

	case EGutHitResult::Hit:
	case EGutHitResult::GuardBroken:

		// whatever the plan was, it is gone. Decide whether to cover up once the stun wears off.
		PendingReaction = EReaction::None;
		bCounterQueued = false;
		bSwingThrown = false;
		bGuardWhenFree = FMath::FRand() < GetGuardChance(Fighter, Tuning.GuardAfterHitChance);

		Fighter->SetBlocking(false);

		if (State != EGutOpponentState::Retreat)
		{
			EnterHold();
		}
		break;

	default:
		break;
	}
}
