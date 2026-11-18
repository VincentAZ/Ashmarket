// Ashmarket prototype: the Gut.

// The pit boss's special cards (design document, chapter 7: "One on one by default. Special cards run two on two, one against
// three, or fighter against animal."). The two with people in them are here: the player alone against three, and the player
// and a second against two. These are members of the cage's game mode, kept in a file of their own.

#include "GutGameMode.h"
#include "Engine/World.h"
#include "GutBeast.h"
#include "GutCrowd.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutLadderData.h"
#include "GutOpponentController.h"

EGutSpecialCard AGutGameMode::GetSpecialOffer() const
{
	const FGutLadderRung* Rung = GetRung();
	const UGutLadderData* Rank = GetLadder();

	if (!IsOnLadder() || !Rung || !Rank || GetLadderState().Rank < SpecialsFromRank)
	{
		return EGutSpecialCard::None;
	}

	// not the fights the story hangs on: somebody who spares the player, and the last fight of a rank
	if (Rung->bSparesPlayer || GetLadderState().Rung >= Rank->Rungs.Num() - 1)
	{
		return EGutSpecialCard::None;
	}

	// one of each to a rank: the second fight is two on two, the third is against the dogs, the fourth is one against three
	switch (GetLadderState().Rung)
	{
	case 1:
		return EGutSpecialCard::TwoOnTwo;

	case 2:
		return Dog ? EGutSpecialCard::Animal : EGutSpecialCard::None;

	case 3:
		return EGutSpecialCard::OneAgainstThree;

	default:
		return EGutSpecialCard::None;
	}
}

void AGutGameMode::SetSpecialAccepted(bool bAccepted)
{
	if (Phase == EGutFightPhase::Card)
	{
		bSpecialAccepted = bAccepted && GetSpecialOffer() != EGutSpecialCard::None;
	}
}

float AGutGameMode::GetSpecialPurseScale(EGutSpecialCard Card) const
{
	switch (Card)
	{
	case EGutSpecialCard::OneAgainstThree:
		return OneAgainstThreePurseScale;

	case EGutSpecialCard::TwoOnTwo:
		return TwoOnTwoPurseScale;

	case EGutSpecialCard::Animal:
		return AnimalPurseScale;

	default:
		return 1.0f;
	}
}

int32 AGutGameMode::CountLiving(int32 Side) const
{
	int32 Count = 0;

	for (const AGutFighter* Fighter : { PlayerFighter.Get(), EnemyFighter.Get() })
	{
		Count += (Fighter && Fighter->IsAlive() && Fighter->GetSide() == Side) ? 1 : 0;
	}

	for (const AGutFighter* Extra : ExtraFighters)
	{
		Count += (Extra && Extra->IsAlive() && Extra->GetSide() == Side) ? 1 : 0;
	}

	return Count;
}

AGutFighter* AGutGameMode::SpawnExtra(int32 Side, const FVector& Where, const FGutLadderRung* As, const FText& Name, UGutBeastData* AsBeast)
{
	UClass* BrainClass = EnemyFighter->GetController() ? EnemyFighter->GetController()->GetClass() : nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// facing the middle of the cage, as the two who were already here do
	const FRotator Facing = Side == 0 ? PlayerFighter->GetActorRotation() : EnemyFighter->GetActorRotation();

	// an animal is one from the start: it is never dressed as a person first
	AGutFighter* Extra = GetWorld()->SpawnActorDeferred<AGutFighter>(EnemyFighter->GetClass(), FTransform(Facing, Where), nullptr, nullptr, Params.SpawnCollisionHandlingOverride);

	if (!Extra)
	{
		return nullptr;
	}

	Extra->SpawnAsBeast = AsBeast;
	Extra->FinishSpawning(FTransform(Facing, Where));

	// a fighter placed in the level is given its brain by the level; one brought in later is given it here
	if (!Extra->GetController() && BrainClass)
	{
		if (AController* Brain = GetWorld()->SpawnActor<AController>(BrainClass, Where, Facing, Params))
		{
			Brain->Possess(Extra);
		}
	}

	if (As)
	{
		MakeFighter(Extra, *As, false);
	}

	// an animal was made one before it had a brain to tell how such a thing fights
	if (AGutOpponentController* Brain = AsBeast ? Cast<AGutOpponentController>(Extra->GetController()) : nullptr)
	{
		Brain->SetTuning(AsBeast->Tuning);
	}

	if (!Name.IsEmpty())
	{
		Extra->SetDisplayName(Name);
	}

	Extra->SetSide(Side);
	Extra->SetSquareUp(true);
	Extra->SetCombatEnabled(false);

	Extra->OnDied.AddDynamic(this, &AGutGameMode::OnFighterDied);

	if (Crowd)
	{
		Extra->OnStaggered.AddDynamic(this, &AGutGameMode::OnAnyFighterStaggered);
		Extra->OnAttackReceived.AddDynamic(this, &AGutGameMode::OnAnyAttackReceived);
	}

	ExtraFighters.Add(Extra);

	return Extra;
}

void AGutGameMode::BeginSpecial(EGutSpecialCard Card, const FGutLadderRung* Others, const FGutLadderRung* Second)
{
	const bool bBeforeBell = Phase == EGutFightPhase::Card || Phase == EGutFightPhase::Choosing || Phase == EGutFightPhase::Betting || Phase == EGutFightPhase::Intro;

	if (Card == EGutSpecialCard::None || SpecialCard != EGutSpecialCard::None || !bFightersReady || !bBeforeBell || ((Card == EGutSpecialCard::Animal || Card == EGutSpecialCard::WithDogs) && !Dog))
	{
		return;
	}

	SpecialCard = Card;

	PlayerFighter->SetSide(0);
	EnemyFighter->SetSide(1);

	// the others stand beside whoever they came in with, a long stride off
	const FVector Beside = EnemyFighter->GetActorRightVector() * 230.0f;
	const FVector Back = EnemyFighter->GetActorForwardVector() * -60.0f;

	if (Card == EGutSpecialCard::WithDogs)
	{
		// the fighter on the card, and its dogs at its heel: -GutRungDogs=1 gives any fighter one, to measure it
		int32 Heel = GetRung() ? GetRung()->Dogs : 0;

		FParse::Value(FCommandLine::Get(), TEXT("GutRungDogs="), Heel);

		for (int32 i = 0; i < Heel; ++i)
		{
			const float Along = ((i / 2) + 1) * ((i % 2) ? -1.0f : 1.0f);

			SpawnExtra(1, EnemyFighter->GetActorLocation() + (Beside * Along * 0.7f) + (Back * -1.5f), nullptr, FText::GetEmpty(), Dog);
		}
	}
	else if (Card == EGutSpecialCard::Animal)
	{
		// the fighter on the card sits this one out: what stands in its place is a dog, and the others come in beside it
		EnemyFighter->BecomeBeast(Dog);

		for (int32 i = 1; i < DogsOnCard; ++i)
		{
			const float Along = ((i + 1) / 2) * ((i % 2) ? 1.0f : -1.0f);

			SpawnExtra(1, EnemyFighter->GetActorLocation() + (Beside * Along) + Back, nullptr, FText::GetEmpty(), Dog);
		}
	}
	else if (Card == EGutSpecialCard::OneAgainstThree)
	{
		// for trying the card without rebuilding: -GutThree=1,0.6 is how many may be at the player at once, and the extra two's share of their health
		FString Three;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutThree="), Three, false))
		{
			TArray<FString> Parts;
			Three.ParseIntoArray(Parts, TEXT(","));

			if (Parts.Num() == 2)
			{
				MostOnOneOfThree = FMath::Clamp(FCString::Atoi(*Parts[0]), 1, 3);
				ThreeExtraHealthScale = FMath::Clamp(FCString::Atof(*Parts[1]), 0.1f, 1.0f);
			}
		}

		for (const TCHAR* Name : { TEXT("New meat"), TEXT("More meat") })
		{
			const float Way = ExtraFighters.Num() == 0 ? 1.0f : -1.0f;

			if (AGutFighter* Extra = SpawnExtra(1, EnemyFighter->GetActorLocation() + (Beside * Way) + Back, Others, FText::FromString(Name)))
			{
				Extra->SetMaxHealth(Extra->GetMaxHealth() * ThreeExtraHealthScale);
				Extra->SetHealth(Extra->GetMaxHealth());

				// -GutThreeArmed gives them the weapon the bottom of the ladder fights with, as they had before this was measured
				if (bThreeExtrasUnarmed && !FParse::Param(FCommandLine::Get(), TEXT("GutThreeArmed")))
				{
					Extra->EquipWeapon(nullptr);
				}
			}
		}
	}
	else
	{
		SpawnExtra(1, EnemyFighter->GetActorLocation() + Beside + Back, Second ? Second : Others, FText::FromString(TEXT("Its second")));
		SpawnExtra(0, PlayerFighter->GetActorLocation() - (PlayerFighter->GetActorRightVector() * 230.0f), Second ? Second : Others, FText::FromString(TEXT("Your second")));
	}

	UpdateSpecialTargets();

	UE_LOG(LogGut, Log, TEXT("Special card: %s. %d against %d"),
		Card == EGutSpecialCard::OneAgainstThree ? TEXT("one against three") : (Card == EGutSpecialCard::Animal ? TEXT("against the dogs") : (Card == EGutSpecialCard::WithDogs ? TEXT("with its dogs") : TEXT("two on two"))), CountLiving(0), CountLiving(1));
}

void AGutGameMode::LooseDogs(int32 Count)
{
	if (!Dog || bDogsLoose || Phase != EGutFightPhase::Fighting || !PlayerFighter || !EnemyFighter)
	{
		return;
	}

	bDogsLoose = true;

	// the two who were fighting are still against each other, and the dogs are against both
	if (SpecialCard == EGutSpecialCard::None)
	{
		PlayerFighter->SetSide(0);
		EnemyFighter->SetSide(1);
	}

	// they come in from the two ends of the pool that are furthest from the fighting
	const FVector Middle = (PlayerFighter->GetActorLocation() + EnemyFighter->GetActorLocation()) * 0.5f;

	TArray<FVector> Doors = { FVector(560.0f, 360.0f, 0.0f), FVector(-560.0f, -360.0f, 0.0f), FVector(560.0f, -360.0f, 0.0f), FVector(-560.0f, 360.0f, 0.0f) };

	Doors.Sort([Middle](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, Middle) > FVector::DistSquared2D(B, Middle); });

	for (int32 i = 0; i < Count && i < Doors.Num(); ++i)
	{
		const FVector Where(Doors[i].X, Doors[i].Y, PlayerFighter->GetActorLocation().Z);

		if (AGutFighter* One = SpawnExtra(GutDogsSide, Where, nullptr, FText::GetEmpty(), Dog))
		{
			One->SetActorRotation((Middle - Where).GetSafeNormal2D().Rotation());
			One->SetCombatEnabled(true);
		}
	}

	UpdateSpecialTargets();

	UE_LOG(LogGut, Log, TEXT("The dogs are loose after %.1f seconds: %d of them"), FightTime, Count);
}

void AGutGameMode::CallOffDogs()
{
	if (!bDogsLoose)
	{
		return;
	}

	bDogsLoose = false;

	for (int32 i = ExtraFighters.Num() - 1; i >= 0; --i)
	{
		AGutFighter* Extra = ExtraFighters[i];

		if (Extra && Extra->IsBeast() && Extra->GetSide() == GutDogsSide)
		{
			ExtraFighters.RemoveAt(i);

			if (AController* Brain = Extra->GetController())
			{
				Brain->Destroy();
			}

			Extra->Destroy();
		}
	}

	if (PlayerFighter && EnemyFighter && SpecialCard == EGutSpecialCard::None)
	{
		PlayerFighter->SetOpponent(EnemyFighter);
		EnemyFighter->SetOpponent(PlayerFighter);
	}
}

void AGutGameMode::UpdateSpecialTargets()
{
	TArray<AGutFighter*> Everybody = { PlayerFighter.Get(), EnemyFighter.Get() };

	for (AGutFighter* Extra : ExtraFighters)
	{
		Everybody.Add(Extra);
	}

	for (AGutFighter* Fighter : Everybody)
	{
		if (!Fighter || !Fighter->IsAlive())
		{
			continue;
		}

		// whoever is nearest on the other side
		AGutFighter* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();

		for (AGutFighter* Other : Everybody)
		{
			if (!Other || !Other->IsAlive() || Other->GetSide() == Fighter->GetSide())
			{
				continue;
			}

			const float Distance = FVector::Dist2D(Fighter->GetActorLocation(), Other->GetActorLocation());

			if (Distance < NearestDistance)
			{
				Nearest = Other;
				NearestDistance = Distance;
			}
		}

		AGutFighter* Current = Fighter->GetOpponent();

		const bool bCurrentGood = Current && Current->IsAlive() && Current->GetSide() != Fighter->GetSide();

		// a fighter in the middle of something with somebody does not look away from it, and nobody turns for a small difference
		if (bCurrentGood && (!Nearest || Fighter->GetFighterState() == EGutFighterState::Finishing
			|| FVector::Dist2D(Fighter->GetActorLocation(), Current->GetActorLocation()) <= NearestDistance + RetargetMargin))
		{
			continue;
		}

		if (Nearest && Nearest != Current)
		{
			Fighter->SetOpponent(Nearest);
		}
	}

	// there is only room for so many round one body: the nearest of those after the same fighter go at it, and the rest hold off
	for (AGutFighter* Target : Everybody)
	{
		if (!Target || !Target->IsAlive())
		{
			continue;
		}

		TArray<AGutFighter*> After;

		for (AGutFighter* Fighter : Everybody)
		{
			if (Fighter && Fighter->IsAlive() && Fighter->GetOpponent() == Target && Cast<AGutOpponentController>(Fighter->GetController()))
			{
				After.Add(Fighter);
			}
		}

		After.Sort([Target](const AGutFighter& A, const AGutFighter& B)
		{
			// whoever is already swinging keeps its place, then the nearest
			const bool bSwingingA = A.IsAttacking();
			const bool bSwingingB = B.IsAttacking();

			if (bSwingingA != bSwingingB)
			{
				return bSwingingA;
			}

			return FVector::DistSquared2D(A.GetActorLocation(), Target->GetActorLocation()) < FVector::DistSquared2D(B.GetActorLocation(), Target->GetActorLocation());
		});

		for (int32 i = 0; i < After.Num(); ++i)
		{
			Cast<AGutOpponentController>(After[i]->GetController())->SetWaitsItsTurn(i >= (SpecialCard == EGutSpecialCard::OneAgainstThree ? MostOnOneOfThree : MostOnOne));
		}
	}
}
