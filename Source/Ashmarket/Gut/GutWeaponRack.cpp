// Ashmarket prototype: the Gut.

#include "GutWeaponRack.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GutWeaponData.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GutWeaponRackConstants
{
	/** Edge length of the engine's basic cube */
	const float CubeSize = 100.0f;
}

AGutWeaponRack::AGutWeaponRack()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// create the root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// create the board. It hangs flat on the wall, so nothing needs to collide with it.
	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(RootComponent);
	Board->SetStaticMesh(CubeMesh.Object);
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGutWeaponRack::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float CubeSize = GutWeaponRackConstants::CubeSize;

	Board->SetRelativeLocation(FVector(BoardThickness * 0.5f, 0.0f, 0.0f));
	Board->SetRelativeScale3D(FVector(BoardThickness / CubeSize, BoardSize.X / CubeSize, BoardSize.Y / CubeSize));
	Board->SetMaterial(0, BoardMaterial);
	Board->SetVisibility(BackingMesh == nullptr);

	for (UStaticMeshComponent* Panel : BackingPanels)
	{
		if (Panel)
		{
			Panel->DestroyComponent();
		}
	}

	BackingPanels.Reset();

	if (BackingMesh)
	{
		const FBox Bounds = BackingMesh->GetBoundingBox();
		const FVector PanelSize = Bounds.GetSize();

		const int32 Count = FMath::Max(1, FMath::RoundToInt32(BoardSize.X / FMath::Max(PanelSize.X, 1.0f)));

		// turned so that its width runs along the wall
		const FRotator Turned(0.0f, 90.0f, 0.0f);
		const FVector Scale((BoardSize.X / Count) / PanelSize.X, 1.0f, BoardSize.Y / PanelSize.Z);

		for (int32 i = 0; i < Count; ++i)
		{
			UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this);

			Panel->SetStaticMesh(BackingMesh);
			Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Panel->SetupAttachment(RootComponent);

			const FVector Middle(BoardThickness * 0.5f, (-BoardSize.X * 0.5f) + (BoardSize.X * (i + 0.5f) / Count), 0.0f);

			Panel->SetRelativeTransform(FTransform(Turned, Middle - Turned.RotateVector(Bounds.GetCenter() * Scale), Scale));
			Panel->RegisterComponent();

			BackingPanels.Add(Panel);
		}
	}
}

void AGutWeaponRack::SetWeapons(const TArray<TObjectPtr<UGutWeaponData>>& NewWeapons)
{
	for (UStaticMeshComponent* Shape : WeaponShapes)
	{
		if (Shape)
		{
			Shape->DestroyComponent();
		}
	}

	Weapons.Reset();
	WeaponShapes.Reset();

	// spread the weapons evenly along the board, the first on the left as seen from in front
	const float Spacing = BoardSize.X / FMath::Max(1, NewWeapons.Num());
	const float FirstY = (BoardSize.X * 0.5f) - (Spacing * 0.5f);

	for (int32 i = 0; i < NewWeapons.Num(); ++i)
	{
		UGutWeaponData* Weapon = NewWeapons[i];
		UStaticMeshComponent* Shape = nullptr;

		if (Weapon && Weapon->Mesh)
		{
			Shape = NewObject<UStaticMeshComponent>(this);
			Shape->SetupAttachment(RootComponent);
			Shape->SetStaticMesh(Weapon->Mesh);
			Shape->SetMaterial(0, Weapon->Material);
			Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// the wall's stains stay on the wall
			Shape->SetReceivesDecals(false);

			// hung upright at the size it has in the hand, by its middle wherever the shape's own origin is
			const FVector Scale = Weapon->GripTransform.GetScale3D();
			const FVector Middle = Weapon->Mesh->GetBounds().Origin * Scale;

			Shape->SetRelativeLocation(FVector(BoardThickness + WeaponStandoff - Middle.X, FirstY - (Spacing * i) - Middle.Y, -Middle.Z));
			Shape->SetRelativeScale3D(Scale);

			// turned the way it is turned in the hand, so a shield shows its face to the room
			Shape->SetRelativeRotation(Weapon->GripTransform.GetRotation());

			Shape->RegisterComponent();
		}

		Weapons.Add(Weapon);
		WeaponShapes.Add(Shape);
	}
}

void AGutWeaponRack::SetDrawn(const TArray<const UGutWeaponData*>& DrawnWeapons)
{
	for (int32 i = 0; i < WeaponShapes.Num(); ++i)
	{
		if (WeaponShapes[i])
		{
			WeaponShapes[i]->SetVisibility(!DrawnWeapons.Contains(Weapons[i]));
		}
	}
}

bool AGutWeaponRack::IsOnBoard(const UGutWeaponData* Weapon) const
{
	const int32 Index = Weapons.IndexOfByKey(Weapon);

	return WeaponShapes.IsValidIndex(Index) && WeaponShapes[Index] && WeaponShapes[Index]->IsVisible();
}
