// Ashmarket prototype: the Gut.

#include "GutSpikeWall.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GutSpikeWallConstants
{
	/** Size of the engine's basic shapes */
	const float ShapeSize = 100.0f;

	/** Thickness of the plate, which stands just proud of the wall */
	const float PlateThickness = 2.0f;

	/** Gap between the floor and the lowest row of spikes, and between the top row and the top of the plate */
	const float EdgeMargin = 22.0f;

	/** Speed the spikes shake at during the warning */
	const float RattleRate = 38.0f;
}

AGutSpikeWall::AGutSpikeWall()
{
	DisplayName = FText::FromString(TEXT("SPIKES"));
	LiveText = FText::FromString(TEXT("THE SPIKES ARE OUT"));
	HitText = FText::FromString(TEXT("SPIKED"));

	ActivationRound = 2;
	Damage = 25.0f;
	StunTime = 0.7f;
	ThrowDistance = 150.0f;

	HurtSound = FName("spikes_hit");
	WarningSound = FName("spikes_rattle");
	SwitchSound = FName("spikes_out");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(RootComponent);
	Plate->SetStaticMesh(CubeMesh.Object);
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// the spikes hurt by rule, not by collision, so fighters can be pushed into them
	Spikes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spikes"));
	Spikes->SetupAttachment(RootComponent);
	Spikes->SetStaticMesh(ConeMesh.Object);
	Spikes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGutSpikeWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BuildSpikes();
}

float AGutSpikeWall::GetDangerDistance(const FVector& Point, FVector& OutAwayDirection) const
{
	// work in the wall's own frame: X out of the wall, Y along it
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Point);

	const float HalfWidth = Width * 0.5f;

	const float OutsideX = FMath::Max3<float>(-Local.X, Local.X - SpikeLength, 0.0f);
	const float OutsideY = FMath::Max3<float>(-HalfWidth - Local.Y, Local.Y - HalfWidth, 0.0f);

	// in among the spikes, or straight out from them: the way out is straight off the wall
	if (OutsideY <= 0.0f)
	{
		OutAwayDirection = GetActorForwardVector().GetSafeNormal2D();

		return OutsideX > 0.0f ? OutsideX : Local.X - SpikeLength;
	}

	// off to one end: the way out is away from the nearest corner
	const FVector Nearest(FMath::Clamp(Local.X, 0.0f, SpikeLength), FMath::Clamp(Local.Y, -HalfWidth, HalfWidth), Local.Z);

	OutAwayDirection = GetActorTransform().TransformVectorNoScale(Local - Nearest).GetSafeNormal2D();

	return FVector2D(OutsideX, OutsideY).Size();
}

void AGutSpikeWall::OnHazardStateChanged(EGutHazardState NewState)
{
	Spikes->SetMaterial(0, (NewState == EGutHazardState::Live && LiveSpikeMaterial) ? LiveSpikeMaterial.Get() : SpikeMaterial.Get());
}

void AGutSpikeWall::TickPresentation(float DeltaTime)
{
	switch (State)
	{
	case EGutHazardState::Warning:
		// rattle part of the way out and back, so the wall can be seen to be about to go
		SetExtent(WarningExtent * (0.5f + 0.5f * FMath::Sin(StateTime * GutSpikeWallConstants::RattleRate)));
		break;

	case EGutHazardState::Live:
		SetExtent(FMath::Min(1.0f, StateTime / ExtendTime));
		break;

	default:
		SetExtent(FMath::Max(0.0f, Extent - (DeltaTime / ExtendTime)));
		break;
	}
}

void AGutSpikeWall::BuildSpikes()
{
	const float Size = GutSpikeWallConstants::ShapeSize;

	Plate->SetRelativeLocation(FVector(GutSpikeWallConstants::PlateThickness * 0.5f, 0.0f, Height * 0.5f));
	Plate->SetRelativeScale3D(FVector(GutSpikeWallConstants::PlateThickness, Width, Height) / Size);
	Plate->SetMaterial(0, PlateMaterial);

	Spikes->ClearInstances();
	Spikes->SetMaterial(0, SpikeMaterial);

	for (UStaticMeshComponent* Panel : SpikePanels)
	{
		if (Panel)
		{
			Panel->DestroyComponent();
		}
	}

	SpikePanels.Reset();

	// real panels of spikes, if there are any, in place of the cones. They ride on the same part that slides.
	if (SpikePanelMesh)
	{
		const FBox Bounds = SpikePanelMesh->GetBoundingBox();
		const FVector PanelSize = Bounds.GetSize();

		const int32 Across = FMath::Max(1, FMath::RoundToInt32(Width / FMath::Max(PanelSize.Y, 1.0f)));
		const int32 Up = FMath::Max(1, FMath::RoundToInt32(Height / FMath::Max(PanelSize.Z, 1.0f)));

		const FVector PanelScale(SpikeLength / PanelSize.X, (Width / Across) / PanelSize.Y, (Height / Up) / PanelSize.Z);

		for (int32 i = 0; i < Across; ++i)
		{
			for (int32 j = 0; j < Up; ++j)
			{
				UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this);

				Panel->SetStaticMesh(SpikePanelMesh);
				Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Panel->SetReceivesDecals(false);
				Panel->SetupAttachment(Spikes);

				// its back against the wall, its points Spike Length out
				const FVector Middle(SpikeLength * 0.5f, (-Width * 0.5f) + (Width * (i + 0.5f) / Across), Height * (j + 0.5f) / Up);

				Panel->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Middle - (Bounds.GetCenter() * PanelScale), PanelScale));
				Panel->RegisterComponent();

				SpikePanels.Add(Panel);
			}
		}

		SetExtent(Extent);

		return;
	}

	// rows of spikes up the plate, every other row shifted half a space along so they sit in a diamond pattern
	const float Margin = GutSpikeWallConstants::EdgeMargin;
	const int32 Rows = FMath::Max(1, FMath::FloorToInt32((Height - (Margin * 2.0f)) / SpikeSpacing) + 1);
	const int32 Columns = FMath::Max(1, FMath::FloorToInt32(Width / SpikeSpacing));

	// each cone points out of the wall, its base on the wall and its tip a spike's length out
	const FRotator PointOut(-90.0f, 0.0f, 0.0f);
	const FVector Scale(SpikeThickness / Size, SpikeThickness / Size, SpikeLength / Size);

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float Z = Margin + (Row * SpikeSpacing);
		const float Shift = (Row % 2 == 1) ? SpikeSpacing * 0.5f : 0.0f;

		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const float Y = (-Width * 0.5f) + (SpikeSpacing * 0.5f) + (Column * SpikeSpacing) + Shift;

			if (Y > Width * 0.5f)
			{
				continue;
			}

			Spikes->AddInstance(FTransform(PointOut, FVector(SpikeLength * 0.5f, Y, Z), Scale));
		}
	}

	SetExtent(Extent);
}

void AGutSpikeWall::SetExtent(float NewExtent)
{
	Extent = FMath::Clamp(NewExtent, 0.0f, 1.0f);

	// all the way in, the spikes are hidden inside the wall
	Spikes->SetRelativeLocation(FVector((Extent - 1.0f) * SpikeLength, 0.0f, 0.0f));
}
