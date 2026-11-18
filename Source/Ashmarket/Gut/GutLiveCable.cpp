// Ashmarket prototype: the Gut.

#include "GutLiveCable.h"
#include "GutSound.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace GutLiveCableConstants
{
	/** Size of the engine's basic shapes */
	const float ShapeSize = 100.0f;

	/** Time the cable blinks on and off for during the warning */
	const float WarningBlinkTime = 0.25f;

	/** Shortest and longest the live cable stays lit, and stays dark, between flickers */
	const float MinLitTime = 0.08f;
	const float MaxLitTime = 0.45f;
	const float MinDarkTime = 0.03f;
	const float MaxDarkTime = 0.09f;

	/** How far past the point where the way round the free end opens a walker is sent, so it does not stop on the line */
	const float WayRoundMargin = 60.0f;
}

AGutLiveCable::AGutLiveCable()
{
	DisplayName = FText::FromString(TEXT("CABLE"));
	LiveText = FText::FromString(TEXT("THE CABLE IS LIVE"));
	HitText = FText::FromString(TEXT("SHOCKED"));

	bShocks = true;

	ActivationRound = 3;
	Damage = 20.0f;
	StunTime = 1.2f;
	ThrowDistance = 130.0f;
	bDodgeAvoids = true;

	HurtSound = FName("zap");
	WarningSound = FName("arc_warn");
	LiveSound = FName("arc_loop");
	SwitchSound = FName("arc_start");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// the generator is solid, but the camera can pass it
	Generator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Generator"));
	Generator->SetupAttachment(RootComponent);
	Generator->SetStaticMesh(CubeMesh.Object);
	GeneratorBox = CubeMesh.Object;
	Generator->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Generator->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// the cable shocks by rule, not by collision, and fighters walk over it
	Cable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cable"));
	Cable->SetupAttachment(RootComponent);
	Cable->SetStaticMesh(CylinderMesh.Object);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGutLiveCable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BuildCable();
}

float AGutLiveCable::GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const
{
	FVector Start;
	FVector End;
	GetCableEnds(Start, End);

	// everything happens on the floor
	const FVector Flat(Point.X, Point.Y, Start.Z);
	const FVector Nearest = FMath::ClosestPointOnSegment(Flat, Start, End);

	OutAwayDirection = (Flat - Nearest).GetSafeNormal2D();

	// right on top of the cable: off to whichever side is nearer is as good as any
	if (OutAwayDirection.IsNearlyZero())
	{
		OutAwayDirection = GetActorRightVector().GetSafeNormal2D();
	}

	return FVector::Dist2D(Flat, Nearest) - (CableThickness * 0.5f) - WetReach;
}

bool AGutLiveCable::GetWayRound(const FVector& From, const FVector& To, float Clearance, FVector& OutWaypoint) const
{
	// work in the cable's own frame: X along it from the wall, Y across it
	const FVector LocalFrom = GetActorTransform().InverseTransformPositionNoScale(From);
	const FVector LocalTo = GetActorTransform().InverseTransformPositionNoScale(To);

	// both on the same side: nothing to cross
	if ((LocalFrom.Y > 0.0f) == (LocalTo.Y > 0.0f) || FMath::IsNearlyEqual(LocalFrom.Y, LocalTo.Y))
	{
		return false;
	}

	// where the straight line between them crosses the line of the cable
	const float CrossX = FMath::Lerp(LocalFrom.X, LocalTo.X, LocalFrom.Y / (LocalFrom.Y - LocalTo.Y));

	// past the free end, with room to spare, the line is open. The generator and the wall close the other end.
	const float FreeEndX = GeneratorSize.X + Length + (CableThickness * 0.5f) + Clearance;

	if (CrossX > FreeEndX)
	{
		return false;
	}

	// walk to a point beyond the free end, in line with the cable. From there the line to the other side is open.
	OutWaypoint = GetActorTransform().TransformPositionNoScale(FVector(FreeEndX + GutLiveCableConstants::WayRoundMargin, 0.0f, 0.0f));
	OutWaypoint.Z = From.Z;

	return true;
}

void AGutLiveCable::GetCableEnds(FVector& OutStart, FVector& OutEnd) const
{
	OutStart = GetActorTransform().TransformPositionNoScale(FVector(GeneratorSize.X, 0.0f, 0.0f));
	OutEnd = GetActorTransform().TransformPositionNoScale(FVector(GeneratorSize.X + Length, 0.0f, 0.0f));
}

void AGutLiveCable::BeginPlay()
{
	Super::BeginPlay();

	// the generator runs all night, whether or not the cable is live
	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Loop(FName("generator"), Generator->GetComponentLocation(), 1.0f, Generator);
	}
}

FVector AGutLiveCable::GetSoundPlace() const
{
	FVector Start;
	FVector End;
	GetCableEnds(Start, End);

	return (Start + End) * 0.5f;
}

void AGutLiveCable::OnHazardStateChanged(EGutHazardState NewState)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	NextFlickerTime = Now;
	NextSparkTime = Now;

	ShowLit(NewState == EGutHazardState::Live);
}

void AGutLiveCable::TickPresentation(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();

	switch (State)
	{
	case EGutHazardState::Warning:
		// a slow, even blink: the generator is coughing into life
		ShowLit(FMath::FloorToInt32(StateTime / GutLiveCableConstants::WarningBlinkTime) % 2 == 1);
		break;

	case EGutHazardState::Live:

		// mostly lit, with a ragged flicker
		if (Now >= NextFlickerTime)
		{
			ShowLit(!bShowingLit);

			NextFlickerTime = Now + (bShowingLit
				? FMath::FRandRange(GutLiveCableConstants::MinLitTime, GutLiveCableConstants::MaxLitTime)
				: FMath::FRandRange(GutLiveCableConstants::MinDarkTime, GutLiveCableConstants::MaxDarkTime));
		}

		// sparks jump from somewhere along it
		if (SparkEffect && Now >= NextSparkTime)
		{
			NextSparkTime = Now + SparkInterval;

			FVector Start;
			FVector End;
			GetCableEnds(Start, End);

			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SparkEffect, FMath::Lerp(Start, End, FMath::FRand()) + FVector(0.0f, 0.0f, CableThickness));
		}
		break;

	default:
		break;
	}
}

void AGutLiveCable::BuildCable()
{
	const float Size = GutLiveCableConstants::ShapeSize;

	// the generator stands against the wall
	if (GeneratorMesh)
	{
		// a real one: its long side along the wall, its back to it, its own feet on the floor
		const FBox Bounds = GeneratorMesh->GetBoundingBox();

		Generator->SetStaticMesh(GeneratorMesh);
		Generator->EmptyOverrideMaterials();
		Generator->SetRelativeLocationAndRotation(FVector(GeneratorSize.X - (Bounds.GetSize().Y * 0.5f), 0.0f, -Bounds.Min.Z), FRotator(0.0f, 90.0f, 0.0f));
		Generator->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		Generator->SetStaticMesh(GeneratorBox);
		Generator->SetRelativeLocationAndRotation(FVector(GeneratorSize.X * 0.5f, 0.0f, GeneratorSize.Z * 0.5f), FRotator::ZeroRotator);
		Generator->SetRelativeScale3D(GeneratorSize / Size);
		Generator->SetMaterial(0, GeneratorMaterial);
	}

	// the cable lies flat from its foot along the floor. A cylinder stands up its own Z axis, so lay it over onto X.
	Cable->SetRelativeLocationAndRotation(FVector(GeneratorSize.X + (Length * 0.5f), 0.0f, CableThickness * 0.5f), FRotator(-90.0f, 0.0f, 0.0f));
	Cable->SetRelativeScale3D(FVector(CableThickness, CableThickness, Length) / Size);

	for (UStaticMeshComponent* Section : CableSections)
	{
		if (Section)
		{
			Section->DestroyComponent();
		}
	}

	for (UPointLightComponent* Arc : Arcs)
	{
		if (Arc)
		{
			Arc->DestroyComponent();
		}
	}

	CableSections.Reset();
	Arcs.Reset();

	Cable->SetVisibility(CableMesh == nullptr);

	// real cable, if there is any, laid end to end where the cylinder lay
	if (CableMesh)
	{
		const FBox Bounds = CableMesh->GetBoundingBox();
		const FVector SectionSize = Bounds.GetSize().ComponentMax(FVector(1.0f));

		const int32 Count = FMath::Max(1, FMath::RoundToInt32(Length / SectionSize.X));
		// as thick as the hazard's own cable, with a little over for the tape and the scuffs
		const float Thick = CableThickness * 1.5f;
		const FVector Scale((Length / Count) / SectionSize.X, Thick / SectionSize.Y, Thick / SectionSize.Z);

		for (int32 i = 0; i < Count; ++i)
		{
			UStaticMeshComponent* Section = NewObject<UStaticMeshComponent>(this);

			Section->SetStaticMesh(CableMesh);
			Section->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Section->SetupAttachment(RootComponent);

			// every other length is turned end for end, so the same scuff does not come round like wallpaper
			const FRotator Turned(0.0f, (i % 2 == 1) ? 180.0f : 0.0f, 0.0f);
			const FVector Middle(GeneratorSize.X + (Length * (i + 0.5f) / Count), 0.0f, Thick * 0.5f);

			Section->SetRelativeTransform(FTransform(Turned, Middle - Turned.RotateVector(Bounds.GetCenter() * Scale), Scale));
			Section->RegisterComponent();

// what is painted on the floor stays on the floor
			Section->SetReceivesDecals(false);

						CableSections.Add(Section);
		}

		// where it arcs when it is live
		for (const float Share : { 0.2f, 0.55f, 0.85f })
		{
			UPointLightComponent* Arc = NewObject<UPointLightComponent>(this);

			Arc->SetupAttachment(RootComponent);
			Arc->SetRelativeLocation(FVector(GeneratorSize.X + (Length * Share), 0.0f, 12.0f));
			Arc->SetIntensityUnits(ELightUnits::Candelas);
			Arc->SetIntensity(900.0f);
			Arc->SetLightColor(FLinearColor(0.55f, 0.75f, 1.0f));
			Arc->SetAttenuationRadius(450.0f);
			Arc->SetCastShadows(false);
			Arc->SetVisibility(false);
			Arc->RegisterComponent();

			Arcs.Add(Arc);
		}
	}

	ShowLit(bShowingLit);
}

void AGutLiveCable::ShowLit(bool bLit)
{
	bShowingLit = bLit;

	UMaterialInterface* Material = (bLit && LiveCableMaterial) ? LiveCableMaterial.Get() : CableMaterial.Get();

	Cable->SetMaterial(0, Material);

	for (UPointLightComponent* Arc : Arcs)
	{
		if (Arc)
		{
			Arc->SetVisibility(bLit);
		}
	}
}
