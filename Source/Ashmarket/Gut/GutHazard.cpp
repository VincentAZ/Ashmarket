// Ashmarket prototype: the Gut.

#include "GutHazard.h"
#include "Components/AudioComponent.h"
#include "GutSound.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GutFighter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AGutHazard::AGutHazard()
{
	PrimaryActorTick.bCanEverTick = true;

	// a hazard looks at the fighters after they have moved and counted their own time for the frame.
	// Looking before, it saw a dodging fighter where the dodge had carried it but not yet safe, at 60 frames a second and below.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	DisplayName = FText::FromString(TEXT("HAZARD"));
	LiveText = FText::FromString(TEXT("THE CAGE IS LIVE"));
	HitText = FText::FromString(TEXT("HURT"));
}

void AGutHazard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	StateTime += DeltaTime;

	TickPresentation(DeltaTime);

	if (State != EGutHazardState::Live)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
	{
		TryHurt(*It, Now);
	}
}

void AGutHazard::SetHazardState(EGutHazardState NewState)
{
	if (NewState == State)
	{
		return;
	}

	State = NewState;
	StateTime = 0.0f;

	UE_LOG(LogGut, Log, TEXT("%s: %s"), *GetName(), *UEnum::GetDisplayValueAsText(NewState).ToString());

	// what it sounds like now
	if (StateSound)
	{
		StateSound->FadeOut(0.25f, 0.0f);
		StateSound = nullptr;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		const FName Lasting = NewState == EGutHazardState::Live ? LiveSound : (NewState == EGutHazardState::Warning ? WarningSound : NAME_None);

		if (!Lasting.IsNone())
		{
			StateSound = Sounds->Loop(Lasting, GetSoundPlace());
		}

		if (NewState == EGutHazardState::Live && !SwitchSound.IsNone())
		{
			Sounds->Play(SwitchSound, GetSoundPlace());
		}
	}

	OnHazardStateChanged(NewState);

	BP_OnHazardStateChanged(NewState);
}

float AGutHazard::GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const
{
	// stub: a hazard that does not say where it is cannot be touched
	OutAwayDirection = FVector::ZeroVector;

	return UE_BIG_NUMBER;
}

bool AGutHazard::GetWayRound(const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint) const
{
	// stub: nothing to go round
	return false;
}

bool AGutHazard::FindWayRound(const UObject* WorldContextObject, const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	if (!World)
	{
		return false;
	}

	for (TActorIterator<AGutHazard> It(World); It; ++It)
	{
		if (It->GetHazardState() != EGutHazardState::Off && It->GetWayRound(From, To, Clearance, OutWaypoint))
		{
			return true;
		}
	}

	return false;
}

bool AGutHazard::IsTouching(const AGutFighter* Fighter) const
{
	if (!Fighter)
	{
		return false;
	}

	FVector Away;

	return GetDangerDistance(Fighter->GetActorLocation(), Away) <= Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

void AGutHazard::TryHurt(AGutFighter* Fighter, float Now)
{
	if (!Fighter || !Fighter->IsAlive() || !Fighter->IsCombatEnabled())
	{
		return;
	}

	// one touch at a time: give the fighter the chance to get clear
	if (const float* LastHitTime = LastHitTimes.Find(Fighter))
	{
		if (Now - *LastHitTime < HitCooldown)
		{
			return;
		}
	}

	FVector Away;

	if (GetDangerDistance(Fighter->GetActorLocation(), Away) > Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius())
	{
		return;
	}

	if (!Fighter->ReceiveHazard(this, Away))
	{
		return;
	}

	LastHitTimes.Add(Fighter, Now);

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && !HurtSound.IsNone())
	{
		Sounds->Play(HurtSound, Fighter->GetActorLocation());
	}

	if (HitEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, Fighter->GetActorLocation() - (Away * Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius()));
	}

	BP_OnFighterHurt(Fighter);
}

void AGutHazard::OnHazardStateChanged(EGutHazardState NewState)
{
	// stub
}

void AGutHazard::TickPresentation(float DeltaTime)
{
	// stub
}
