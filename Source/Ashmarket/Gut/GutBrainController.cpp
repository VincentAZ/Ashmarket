// Ashmarket prototype: the Gut.

#include "GutBrainController.h"
#include "GutFighter.h"

AGutBrainController::AGutBrainController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGutBrainController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AGutFighter* Fighter = Cast<AGutFighter>(GetPawn());
	AGutFighter* Target = Fighter ? Fighter->GetOpponent() : nullptr;

	// nothing to do outside a running fight
	const bool bFighting = bBrainEnabled && Fighter && Target && Fighter->IsAlive() && Target->IsAlive() && Fighter->IsCombatEnabled();

	if (!bFighting)
	{
		if (bWasFighting)
		{
			bWasFighting = false;
			OnFightStopped(Fighter);
		}

		return;
	}

	// the bell has just rung
	if (!bWasFighting)
	{
		bWasFighting = true;
		OnFightStarted(Fighter, Target);
	}

	TickBrain(Fighter, Target, DeltaTime);
}

void AGutBrainController::NotifyIncomingAttack(AGutFighter* Fighter, AGutFighter* Attacker)
{
	// stub
}

void AGutBrainController::OnFightStarted(AGutFighter* Fighter, AGutFighter* Target)
{
	// stub
}

void AGutBrainController::TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime)
{
	// stub
}

void AGutBrainController::OnFightStopped(AGutFighter* Fighter)
{
	// stub
}
