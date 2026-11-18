// Ashmarket prototype: the Gut.

#include "GutFlood.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GutFighter.h"
#include "GutLiveCable.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGutFlood::AGutFlood()
{
	DisplayName = FText::FromString(TEXT("WATER"));
	LiveText = FText::FromString(TEXT("THE VALVE IS OPEN"));
	HitText = FText::GetEmpty();

	// the last thing the pit boss does to a fight that will not end
	ActivationRound = 4;
	Damage = 0.0f;

	WarningSound = FName("flood_pour");
	LiveSound = FName("flood_lap");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Root);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Water->SetReceivesDecals(false);
	Water->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SheetFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	Sheet = SheetFinder.Object;
}

void AGutFlood::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Water->SetStaticMesh(Sheet);

	if (WaterMaterial)
	{
		Water->SetMaterial(0, WaterMaterial);
	}

	// the engine's sheet is a metre square
	Water->SetRelativeScale3D(FVector(FloorSize.X / 100.0f, FloorSize.Y / 100.0f, 1.0f));

	PlaceWater();
}

void AGutFlood::PlaceWater()
{
	// a dry floor has no sheet lying in it
	Water->SetRelativeLocation(FVector(0.0f, 0.0f, Level));
	Water->SetHiddenInGame(Level <= 0.2f);
}

void AGutFlood::OnHazardStateChanged(EGutHazardState NewState)
{
	Super::OnHazardStateChanged(NewState);

	if (NewState == EGutHazardState::Off)
	{
		// a new fight, or the hazards switched off: the pool is dry again
		Level = 0.0f;

		PlaceWater();
		SetEverybodyWading(false);
	}
}

void AGutFlood::TickPresentation(float DeltaTime)
{
	Super::TickPresentation(DeltaTime);

	if (State == EGutHazardState::Off)
	{
		return;
	}

	// the valve is open from the warning on, and the water comes up at a steady rate until it is as deep as it gets
	if (Level < Depth)
	{
		Level = FMath::Min(Depth, Level + (Depth / FMath::Max(RiseTime, 0.1f) * DeltaTime));

		PlaceWater();
	}

	// over the ankles: from here it is waded through, and it carries current
	const bool bDeepEnough = Level >= Depth * 0.5f;

	if (bDeepEnough != bWaded)
	{
		SetEverybodyWading(bDeepEnough);
	}
	else if (bWaded)
	{
		// anybody brought into the cage since
		for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
		{
			if (!It->IsWading())
			{
				It->SetWading(true, WadeSpeedScale, WadeDodgeScale);
			}
		}
	}
}

void AGutFlood::SetEverybodyWading(bool bInWater)
{
	bWaded = bInWater;

	for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
	{
		It->SetWading(bInWater, WadeSpeedScale, WadeDodgeScale);
	}

	for (TActorIterator<AGutLiveCable> It(GetWorld()); It; ++It)
	{
		It->SetWetReach(bInWater ? CurrentReach : 0.0f);
	}
}
