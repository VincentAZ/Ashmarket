// Ashmarket prototype: the Gut.

#include "GutDogs.h"
#include "Engine/World.h"
#include "GutGameMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutDogs)

AGutDogs::AGutDogs()
{
	DisplayName = FText::FromString(TEXT("DOGS"));
	LiveText = FText::FromString(TEXT("THE DOGS ARE LOOSE"));
	HitText = FText::GetEmpty();

	ActivationRound = 5;
	Damage = 0.0f;

	// heard behind the gate before it is opened
	WarningSound = FName("dog_kennel");
	SwitchSound = FName("dog_gate");
}

void AGutDogs::OnHazardStateChanged(EGutHazardState NewState)
{
	Super::OnHazardStateChanged(NewState);

	AGutGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AGutGameMode>() : nullptr;

	if (!GameMode)
	{
		return;
	}

	if (NewState == EGutHazardState::Live)
	{
		GameMode->LooseDogs(Count);
	}
	else if (NewState == EGutHazardState::Off)
	{
		// a new fight, or the hazards switched off: they are taken out again
		GameMode->CallOffDogs();
	}
}
