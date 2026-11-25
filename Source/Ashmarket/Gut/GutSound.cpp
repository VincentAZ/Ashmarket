// Ashmarket prototype: the Gut.

#include "GutSound.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GutCage.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GutSound)

namespace GutSoundConstants
{
	/** Where Tools/import_gut_sound.sh leaves the list of sounds */
	const TCHAR* DataPath = TEXT("/Game/Gut/Sound/DA_Gut_Sound.DA_Gut_Sound");

	/** A bed stays on one recording at least this long, however short the recording, and takes this long to go over to the next */
	const float BedLeastTime = 24.0f;
	const float BedCrossFade = 4.0f;
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

		StartBed(Kind, 1.0f, 0.5f);
	}
}

int32 UGutSoundSubsystem::StartBed(FName Kind, float Volume, float FadeIn)
{
	if (!Has(Kind) || Volume <= 0.0f)
	{
		return INDEX_NONE;
	}

	FGutSoundBed& Bed = Beds.AddDefaulted_GetRef();

	Bed.Kind = Kind;
	Bed.Volume = Volume;

	ChangeBed(Bed, FadeIn);

	return Beds.Num() - 1;
}

void UGutSoundSubsystem::StopBed(int32 Bed, float FadeOut)
{
	if (!Beds.IsValidIndex(Bed) || Beds[Bed].bStopped)
	{
		return;
	}

	Beds[Bed].bStopped = true;

	if (UAudioComponent* Playing = Beds[Bed].Playing)
	{
		Playing->FadeOut(FadeOut, 0.0f);
	}

	Beds[Bed].Playing = nullptr;
}

void UGutSoundSubsystem::SetBedShare(int32 Bed, float Share)
{
	if (!Beds.IsValidIndex(Bed) || Beds[Bed].bStopped)
	{
		return;
	}

	Beds[Bed].Share = FMath::Max(Share, 0.0f);

	if (UAudioComponent* Playing = Beds[Bed].Playing)
	{
		Playing->SetVolumeMultiplier(Beds[Bed].Base * Beds[Bed].Share);
	}
}

USoundAttenuation* UGutSoundSubsystem::GetAttenuation()
{
	return GetData() ? Data->Attenuation.Get() : nullptr;
}

USoundBase* UGutSoundSubsystem::ChooseForBed(FName Kind, const FGutSoundSet*& OutSet)
{
	OutSet = GetData() ? Data->Sets.Find(Kind) : nullptr;

	if (!OutSet || OutSet->Sounds.IsEmpty())
	{
		return nullptr;
	}

	// Dealt like cards, and the deal kept for the whole sitting, through every change of place: each recording is heard
	// once before any is heard again. Chosen afresh at each place, one recording of the night (the one of crickets) opened
	// six of the nine times in a ten-minute play, because a place always begins its night at the same point in the dice.
	static TMap<FName, TArray<int32>> Deals;
	static TMap<FName, int32> LastDealt;
	static FRandomStream Dice(static_cast<int32>(FPlatformTime::Cycles64() & 0x7fffffff));

	TArray<int32>& Left = Deals.FindOrAdd(Kind);

	Left.RemoveAll([OutSet](int32 Index) { return !OutSet->Sounds.IsValidIndex(Index) || !OutSet->Sounds[Index]; });

	if (Left.IsEmpty())
	{
		for (int32 i = 0; i < OutSet->Sounds.Num(); ++i)
		{
			if (OutSet->Sounds[i])
			{
				Left.Add(i);
			}
		}

		for (int32 i = Left.Num() - 1; i > 0; --i)
		{
			Left.Swap(i, Dice.RandRange(0, i));
		}

		// the new deal does not begin with what the last one ended on
		if (const int32* Last = LastDealt.Find(Kind); Last && Left.Num() > 1 && Left.Last() == *Last)
		{
			Left.Swap(0, Left.Num() - 1);
		}
	}

	if (Left.IsEmpty())
	{
		return nullptr;
	}

	const int32 Index = Left.Pop();

	LastDealt.Add(Kind, Index);

	return OutSet->Sounds[Index];
}

void UGutSoundSubsystem::ChangeBed(FGutSoundBed& Bed, float Fade)
{
	const FGutSoundSet* Set = nullptr;

	USoundBase* Sound = ChooseForBed(Bed.Kind, Set);

	if (!Sound)
	{
		Bed.bStopped = true;
		return;
	}

	// a recording made to go round is as long as its wave, whatever it says about going on for ever
	const USoundWave* Wave = Cast<USoundWave>(Sound);
	const float Length = Wave ? FMath::Max(Wave->Duration, 1.0f) : FMath::Max(Sound->GetDuration(), 1.0f);

	// whole times round, so that a short one is heard more than once before it gives way, and begun somewhere in its
	// first half, so that the same recording does not always open the same way
	const float Hold = Length * FMath::CeilToFloat(GutSoundConstants::BedLeastTime / Length);
	const float From = Bed.Changes > 0 ? FMath::FRandRange(0.0f, Length * 0.5f) : 0.0f;

	Bed.Base = Set->Volume * Bed.Volume;

	UAudioComponent* Next = UGameplayStatics::CreateSound2D(this, Sound, Bed.Base * Bed.Share, 1.0f, 0.0f, nullptr, false, false);

	if (!Next)
	{
		Bed.bStopped = true;
		return;
	}

	if (UAudioComponent* Last = Bed.Playing)
	{
		Last->FadeOut(Fade, 0.0f);
	}

	Next->FadeIn(Fade, 1.0f, From);

	Bed.Playing = Next;
	Bed.ChangeAt = GetWorld()->GetTimeSeconds() + Hold;

	UE_LOG(LogTemp, Log, TEXT("[sound] the %s bed goes to %s for %.0f s, from %.0f s in, at %.2f of full"), *Bed.Kind.ToString(), *Sound->GetName(), Hold, From, Set->Volume * Bed.Volume);

	++Bed.Changes;
	++TimesPlayed.FindOrAdd(Bed.Kind);
}

void UGutSoundSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	for (FGutSoundBed& Bed : Beds)
	{
		if (Bed.bStopped || Now < Bed.ChangeAt)
		{
			continue;
		}

		// a kind with one recording has nothing to change to: it goes on round
		const FGutSoundSet* Set = Data ? Data->Sets.Find(Bed.Kind) : nullptr;

		if (!Set || Set->Sounds.Num() < 2)
		{
			Bed.ChangeAt = Now + 3600.0;
			continue;
		}

		ChangeBed(Bed, GutSoundConstants::BedCrossFade);
	}

	UpdateStrays(DeltaTime);
}

namespace GutStrays
{
	// How often, decided on 23 November and meant to be argued with. A town with no law, at night, heard from inside it:
	// gunfire is far off and ordinary, about once in five minutes, and when it comes it is as likely as not more than one
	// shot (somebody answers, or it was a burst). A scream is rarer, about once in thirteen minutes, and one exchange of
	// shots in eight is followed by one. In any one minute that is about one chance in five of hearing a shot and one in
	// fourteen of hearing somebody: a sitting of a quarter of an hour has two or three exchanges in it and a scream.
	// Never sooner than the least gap, so two never come as a pair of events that were dealt separately.
	const double ShotEvery = 300.0;
	const double ShotLeast = 50.0;
	const double ScreamEvery = 780.0;
	const double ScreamLeast = 150.0;
	const double FirstLeast = 40.0;

	const float SecondShot = 0.42f;
	const float ThirdShot = 0.35f;
	const float ScreamAfterShots = 0.125f;

	// how long the player has been in places that have them, in all, and when the next of each is: kept through every
	// change of place, so that walking through a lane does not start the night again
	double Sitting = 0.0;
	double NextShot = -1.0;
	double NextScream = -1.0;

	double After(double Mean, double Least)
	{
		// the wait for something that happens at random, so often on average
		return FMath::Max(Least, -Mean * FMath::Loge(FMath::Max(FMath::FRand(), 0.0001f)));
	}
}

void UGutSoundSubsystem::HurryStrays()
{
	GutStrays::NextShot = GutStrays::Sitting;
}

void UGutSoundSubsystem::UpdateStrays(float DeltaTime)
{
	using namespace GutStrays;

	const UWorld* World = GetWorld();

	if (!bStrays || !World || DeltaTime <= 0.0f)
	{
		return;
	}

	if (NextShot < 0.0)
	{
		NextShot = FMath::Max(FirstLeast, After(ShotEvery, ShotLeast) * 0.6);
		NextScream = FMath::Max(FirstLeast * 2.0, After(ScreamEvery, ScreamLeast));
	}

	Sitting += DeltaTime;

	const APlayerCameraManager* Eye = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector Ears = Eye ? Eye->GetCameraLocation() : FVector::ZeroVector;
	const double Now = World->GetTimeSeconds();

	// from some way round the compass, and from a long way off or a very long way off: more often the second
	const auto From = []() { return FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector(); };
	const auto HowLoud = []() { return FMath::Lerp(0.3f, 1.0f, FMath::Square(FMath::FRand())); };

	const auto Hear = [this, &Ears](FName Kind, float Volume, const FVector& Way)
	{
		// near enough that the place's own falling-off leaves it as it was cut, and to one side or the other
		Play(Kind, Ears + (Way * 600.0f) + FVector(0.0f, 0.0f, 150.0f), Volume);

		UE_LOG(LogTemp, Log, TEXT("[sound] far off, %.0f degrees round: %s, at %.2f of full"), FMath::Fmod(Way.Rotation().Yaw + 360.0f, 360.0f), *Kind.ToString(), Volume);
	};

	if (Sitting >= NextShot)
	{
		const FVector Way = From();
		const float Volume = HowLoud();

		Hear(FName("far_shot"), Volume, Way);

		double Last = Now;

		if (FMath::FRand() < SecondShot)
		{
			Last += FMath::FRandRange(1.2f, 6.0f);
			StraysDue.Add({ Last, FName("far_shot"), Volume * FMath::FRandRange(0.8f, 1.1f), (Way + (From() * 0.25f)).GetSafeNormal() });

			if (FMath::FRand() < ThirdShot)
			{
				Last += FMath::FRandRange(0.8f, 4.0f);
				StraysDue.Add({ Last, FName("far_shot"), Volume * FMath::FRandRange(0.8f, 1.1f), (Way + (From() * 0.25f)).GetSafeNormal() });
			}
		}

		if (FMath::FRand() < ScreamAfterShots)
		{
			StraysDue.Add({ Last + FMath::FRandRange(2.5f, 8.0f), FName("far_scream"), Volume * 0.9f, Way });

			// and that was the scream for a while
			NextScream = FMath::Max(NextScream, Sitting + ScreamLeast * 2.0);
		}

		NextShot = Sitting + After(ShotEvery, ShotLeast);

		// somebody screaming does not come on top of gunfire that had nothing to do with it
		NextScream = FMath::Max(NextScream, Sitting + 30.0);

		UE_LOG(LogTemp, Log, TEXT("[sound] the next far gunfire is in %.0f s, the next far scream in %.0f s"), NextShot - Sitting, NextScream - Sitting);
	}
	else if (Sitting >= NextScream)
	{
		Hear(FName("far_scream"), HowLoud(), From());

		NextScream = Sitting + After(ScreamEvery, ScreamLeast);
		NextShot = FMath::Max(NextShot, Sitting + 25.0);
	}

	for (int32 i = StraysDue.Num() - 1; i >= 0; --i)
	{
		if (Now >= StraysDue[i].At)
		{
			Hear(StraysDue[i].Kind, StraysDue[i].Volume, StraysDue[i].Way);

			StraysDue.RemoveAt(i);
		}
	}
}

void UGutSoundSubsystem::HurryBeds()
{
	for (FGutSoundBed& Bed : Beds)
	{
		Bed.ChangeAt = 0.0;
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

USoundBase* UGutSoundSubsystem::Choose(FName Kind, const FGutSoundSet*& OutSet, bool bCountGap, uint32 Whose)
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

	// somebody's own voice: the half or so of the recordings that fell to them, the same every time
	if (Whose != 0 && Count >= 4)
	{
		TArray<int32, TInlineAllocator<32>> Own;

		for (int32 i = 0; i < Count; ++i)
		{
			if ((HashCombineFast(Whose, GetTypeHash(i)) % 100) < 55)
			{
				Own.Add(i);
			}
		}

		if (Own.Num() >= 2)
		{
			// kept apart from everybody else's last choice
			const FName Mine(*FString::Printf(TEXT("%s#%u"), *Kind.ToString(), Whose));

			int32 Place = FMath::RandHelper(Own.Num());

			if (const int32* Last = LastChosen.Find(Mine); Last && Own[Place] == *Last)
			{
				Place = (Place + 1 + FMath::RandHelper(Own.Num() - 1)) % Own.Num();
			}

			LastChosen.Add(Mine, Own[Place]);

			return OutSet->Sounds[Own[Place]];
		}
	}

	int32 Index = FMath::RandHelper(Count);

	if (const int32* Last = LastChosen.Find(Kind); Last && Count > 1 && Index == *Last)
	{
		Index = (Index + 1 + FMath::RandHelper(Count - 1)) % Count;
	}

	LastChosen.Add(Kind, Index);

	return OutSet->Sounds[Index];
}

void UGutSoundSubsystem::Play(FName Kind, const FVector& Where, float Volume, float Pitch, uint32 Whose)
{
	const FGutSoundSet* Set = nullptr;

	USoundBase* Sound = Choose(Kind, Set, true, Whose);

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

FGutWho UGutSoundSubsystem::WhoIs(const UObject* WorldContextObject, const TSoftClassPtr<AActor>& Body)
{
	UGutSoundSubsystem* Sounds = Get(WorldContextObject);

	return FGutWho(Sounds && !Body.IsNull() && Sounds->VoiceOf(Body.GetAssetName()) == FName(TEXT("woman")));
}
