// Ashmarket prototype: the Gut.

#include "GutSound.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GutCage.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutSound)

namespace GutSoundConstants
{
	/** Where Tools/import_gut_sound.sh leaves the list of sounds */
	const TCHAR* DataPath = TEXT("/Game/Gut/Sound/DA_Gut_Sound.DA_Gut_Sound");
}

UGutSoundSubsystem* UGutSoundSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	return World ? World->GetSubsystem<UGutSoundSubsystem>() : nullptr;
}

bool UGutSoundSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);

	return World && World->IsGameWorld();
}

void UGutSoundSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!GetData())
	{
		return;
	}

	// the pool's echo, and the sound of the room it is, belong to the pool: in the street there is only the night
	const bool bInTheCage = static_cast<bool>(TActorIterator<AGutCage>(&InWorld));

	// and so does the sound of a foot on its tile. Outside it the ground is concrete and grit, if that has been recorded.
	Footfall = (!bInTheCage && Has(FName("step_street"))) ? FName("step_street") : FName("step");

	if (Data->Reverb && bInTheCage)
	{
		UGameplayStatics::ActivateReverbEffect(&InWorld, Data->Reverb, FName("Gut"), 1.0f, Data->ReverbVolume, 0.5f);
	}

	for (const FName& Kind : Data->Always)
	{
		if (!bInTheCage && Kind != FName("night"))
		{
			continue;
		}

		if (UAudioComponent* Playing = Loop(Kind, FVector::ZeroVector))
		{
			AlwaysPlaying.Add(Playing);
		}
	}
}

UGutSoundData* UGutSoundSubsystem::GetData()
{
	// looked for once: with no sounds brought in yet, everything here is silence
	if (!bLookedForData)
	{
		bLookedForData = true;

		Data = LoadObject<UGutSoundData>(nullptr, GutSoundConstants::DataPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	return Data;
}

bool UGutSoundSubsystem::Has(FName Kind)
{
	const FGutSoundSet* Set = GetData() ? Data->Sets.Find(Kind) : nullptr;

	return Set && !Set->Sounds.IsEmpty();
}

USoundBase* UGutSoundSubsystem::Choose(FName Kind, const FGutSoundSet*& OutSet, bool bCountGap)
{
	OutSet = GetData() ? Data->Sets.Find(Kind) : nullptr;

	if (!OutSet || OutSet->Sounds.IsEmpty())
	{
		return nullptr;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	if (bCountGap && OutSet->LeastGap > 0.0f)
	{
		if (const double* Last = LastPlayed.Find(Kind); Last && Now - *Last < OutSet->LeastGap && Now >= *Last)
		{
			return nullptr;
		}
	}

	LastPlayed.Add(Kind, Now);

	// any but the one played last
	const int32 Count = OutSet->Sounds.Num();
	int32 Index = FMath::RandHelper(Count);

	if (const int32* Last = LastChosen.Find(Kind); Last && Count > 1 && Index == *Last)
	{
		Index = (Index + 1 + FMath::RandHelper(Count - 1)) % Count;
	}

	LastChosen.Add(Kind, Index);

	return OutSet->Sounds[Index];
}

void UGutSoundSubsystem::Play(FName Kind, const FVector& Where, float Volume, float Pitch)
{
	const FGutSoundSet* Set = nullptr;

	USoundBase* Sound = Choose(Kind, Set, true);

	if (!Sound || Volume <= 0.0f)
	{
		return;
	}

	++TimesPlayed.FindOrAdd(Kind);

	const float Loudness = Set->Volume * Volume;
	const float Tone = Pitch * FMath::FRandRange(Set->LowPitch, Set->HighPitch);

	if (Set->bEverywhere)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Loudness, Tone);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Loudness, Tone, 0.0f, Data->Attenuation);
	}
}

UAudioComponent* UGutSoundSubsystem::Loop(FName Kind, const FVector& Where, float Volume, USceneComponent* AttachTo)
{
	const FGutSoundSet* Set = nullptr;

	USoundBase* Sound = Choose(Kind, Set, false);

	if (!Sound)
	{
		return nullptr;
	}

	const float Loudness = Set->Volume * Volume;

	if (Set->bEverywhere)
	{
		return UGameplayStatics::SpawnSound2D(this, Sound, Loudness, 1.0f, 0.0f, nullptr, false, false);
	}

	if (AttachTo)
	{
		return UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, Loudness, 1.0f, 0.0f, Data->Attenuation, nullptr, false);
	}

	return UGameplayStatics::SpawnSoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Loudness, 1.0f, 0.0f, Data->Attenuation, nullptr, false);
}

FName UGutSoundSubsystem::VoiceOf(const FString& BodyClassName)
{
	static const FName Woman(TEXT("woman"));
	static const FName Man(TEXT("man"));

	// a Blueprint's class is named after the Blueprint, with _C on the end
	FString Name = BodyClassName;
	Name.RemoveFromEnd(TEXT("_C"));

	return (GetData() && Data->WomenBodies.Contains(FName(*Name))) ? Woman : Man;
}
