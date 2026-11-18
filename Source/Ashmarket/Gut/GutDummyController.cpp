// Ashmarket prototype: the Gut.

#include "GutDummyController.h"
#include "GutFighter.h"

void AGutDummyController::OnFightStarted(AGutFighter* Fighter, AGutFighter* Target)
{
	AttackCooldown = FirstAttackDelay;
}

void AGutDummyController::TickBrain(AGutFighter* Fighter, AGutFighter* Target, float DeltaTime)
{
	AttackCooldown -= DeltaTime;

	// only a free fighter can walk or start a swing
	if (Fighter->GetFighterState() != EGutFighterState::Idle)
	{
		return;
	}

	// which attack comes next, and how far does it reach?
	const bool bHeavyNext = HeavyAttackEvery > 0 && (AttackCount + 1) % HeavyAttackEvery == 0;
	const FGutAttackDef* NextLight = Fighter->GetNextLightAttack();
	const FGutAttackDef& NextAttack = (bHeavyNext || !NextLight) ? Fighter->GetHeavyAttack() : *NextLight;

	const float Gap = Fighter->GetGapTo(Target);

	if (Gap > NextAttack.Reach * StopAtReachFraction)
	{
		// walk straight at the opponent
		const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();

		Fighter->AddMovementInput(ToTarget, ApproachSpeedScale);
	}
	else
	{
		// close enough: stand and face them
		Fighter->TurnToward(Target->GetActorLocation(), DeltaTime, TurnRate);
	}

	if (AttackCooldown <= 0.0f && Gap <= NextAttack.Reach * AttackAtReachFraction)
	{
		const bool bStarted = bHeavyNext ? Fighter->TryHeavyAttack() : Fighter->TryLightAttack();

		if (bStarted)
		{
			++AttackCount;
			AttackCooldown = FMath::FRandRange(MinAttackInterval, MaxAttackInterval);
		}
	}
}
