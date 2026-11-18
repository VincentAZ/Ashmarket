// Ashmarket prototype: the Gut.

#include "GutCage.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GutCageConstants
{
	/** Edge length of the engine's basic cube */
	const float CubeSize = 100.0f;

	/** How far each bar of the lid reaches onto the top of the walls */
	const float BarOverlap = 30.0f;
}

AGutCage::AGutCage()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// create the root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// create the floor
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	Floor->SetStaticMesh(CubeMesh.Object);

	// the room is world static, like any level geometry. A static mesh component is world dynamic unless told otherwise,
	// and the opponent's brain feels for walls among the world static things only.
	Floor->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// create the four walls
	WallFront = CreateWall(TEXT("Wall Front"), CubeMesh.Object);
	WallBack = CreateWall(TEXT("Wall Back"), CubeMesh.Object);
	WallRight = CreateWall(TEXT("Wall Right"), CubeMesh.Object);
	WallLeft = CreateWall(TEXT("Wall Left"), CubeMesh.Object);

	// create the lid
	LidBars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Lid Bars"));
	LidBars->SetupAttachment(RootComponent);
	LidBars->SetStaticMesh(CubeMesh.Object);
	LidBars->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// the camera is allowed to look in through the lid
	LidBars->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// ...unless the lid is wire. Nothing but the camera's own probe meets this.
	LidCameraStop = CreateDefaultSubobject<UBoxComponent>(TEXT("Lid Camera Stop"));
	LidCameraStop->SetupAttachment(RootComponent);
	LidCameraStop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LidCameraStop->SetCollisionObjectType(ECC_WorldDynamic);
	LidCameraStop->SetCollisionResponseToAllChannels(ECR_Ignore);
	LidCameraStop->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	LidCameraStop->SetCanEverAffectNavigation(false);
}

void AGutCage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BuildCage();
}

void AGutCage::BuildCage()
{
	const float CubeSize = GutCageConstants::CubeSize;

	const float OuterX = FloorSize.X + (WallThickness * 2.0f);
	const float OuterY = FloorSize.Y + (WallThickness * 2.0f);

	// the floor's top face sits at the actor's height
	Floor->SetRelativeLocation(FVector(0.0f, 0.0f, -FloorThickness * 0.5f));
	Floor->SetRelativeScale3D(FVector(OuterX, OuterY, FloorThickness) / CubeSize);
	Floor->SetMaterial(0, FloorMaterial);

	const float WallOffsetX = (FloorSize.X + WallThickness) * 0.5f;
	const float WallOffsetY = (FloorSize.Y + WallThickness) * 0.5f;
	const float WallZ = WallHeight * 0.5f;

	// the front and back walls run the full outer width and cover the corners
	WallFront->SetRelativeLocation(FVector(WallOffsetX, 0.0f, WallZ));
	WallBack->SetRelativeLocation(FVector(-WallOffsetX, 0.0f, WallZ));
	WallFront->SetRelativeScale3D(FVector(WallThickness, OuterY, WallHeight) / CubeSize);
	WallBack->SetRelativeScale3D(FVector(WallThickness, OuterY, WallHeight) / CubeSize);

	WallRight->SetRelativeLocation(FVector(0.0f, WallOffsetY, WallZ));
	WallLeft->SetRelativeLocation(FVector(0.0f, -WallOffsetY, WallZ));
	WallRight->SetRelativeScale3D(FVector(FloorSize.X, WallThickness, WallHeight) / CubeSize);
	WallLeft->SetRelativeScale3D(FVector(FloorSize.X, WallThickness, WallHeight) / CubeSize);

	for (UStaticMeshComponent* Wall : { WallFront, WallBack, WallRight, WallLeft })
	{
		Wall->SetMaterial(0, WallMaterial);
	}

	for (UStaticMeshComponent* Slab : DeckSlabs)
	{
		if (Slab)
		{
			Slab->DestroyComponent();
		}
	}

	DeckSlabs.Reset();

	// the ground round the pool, over the top of each wall and just short of the pool's edge, where the tile's rim shows
	if (DeckMaterial)
	{
		const float Rim = 22.0f;
		const float Skin = 2.0f;

		const FVector Places[4] = { FVector(WallOffsetX + (Rim * 0.5f), 0.0f, 0.0f), FVector(-WallOffsetX - (Rim * 0.5f), 0.0f, 0.0f), FVector(0.0f, WallOffsetY + (Rim * 0.5f), 0.0f), FVector(0.0f, -WallOffsetY - (Rim * 0.5f), 0.0f) };
		const FVector Sizes[4] = { FVector(WallThickness - Rim, OuterY, Skin), FVector(WallThickness - Rim, OuterY, Skin), FVector(FloorSize.X + (Rim * 2.0f), WallThickness - Rim, Skin), FVector(FloorSize.X + (Rim * 2.0f), WallThickness - Rim, Skin) };

		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* Slab = NewObject<UStaticMeshComponent>(this);

			Slab->SetStaticMesh(Floor->GetStaticMesh());
			Slab->SetMaterial(0, DeckMaterial);
			Slab->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Slab->SetupAttachment(RootComponent);
			Slab->SetRelativeLocation(Places[i] + FVector(0.0f, 0.0f, WallHeight + (Skin * 0.5f)));
			Slab->SetRelativeScale3D(Sizes[i] / CubeSize);
			Slab->RegisterComponent();

			DeckSlabs.Add(Slab);
		}
	}

	for (UStaticMeshComponent* Stone : CopingStones)
	{
		if (Stone)
		{
			Stone->DestroyComponent();
		}
	}

	CopingStones.Reset();

	// coping stones round the rim, each side's run turned so the rounded edge looks into the pool
	if (CopingMesh)
	{
		const FBox Bounds = CopingMesh->GetBoundingBox();
		const FVector StoneSize = Bounds.GetSize().ComponentMax(FVector(1.0f));

		// for each side: the middle of its edge, the way along it, and its length
		const FVector Middles[4] = { FVector(0.0f, FloorSize.Y * 0.5f, 0.0f), FVector(0.0f, -FloorSize.Y * 0.5f, 0.0f), FVector(FloorSize.X * 0.5f, 0.0f, 0.0f), FVector(-FloorSize.X * 0.5f, 0.0f, 0.0f) };
		const float Yaws[4] = { 0.0f, 180.0f, -90.0f, 90.0f };
		const float Runs[4] = { FloorSize.X + (StoneSize.Y * 2.0f), FloorSize.X + (StoneSize.Y * 2.0f), FloorSize.Y, FloorSize.Y };

		for (int32 Side = 0; Side < 4; ++Side)
		{
			const int32 Count = FMath::Max(1, FMath::RoundToInt32(Runs[Side] / StoneSize.X));
			const FVector Scale((Runs[Side] / Count) / StoneSize.X, 1.0f, 1.0f);
			const FRotator Turn(0.0f, Yaws[Side] + (bCopingTurned ? 180.0f : 0.0f), 0.0f);
			const FRotator Along(0.0f, Yaws[Side], 0.0f);

			for (int32 i = 0; i < Count; ++i)
			{
				UStaticMeshComponent* Stone = NewObject<UStaticMeshComponent>(this);

				Stone->SetStaticMesh(CopingMesh);
				Stone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Stone->SetupAttachment(RootComponent);

				// along the edge, half its depth back from it less a small overhang, sitting on the deck
				const FVector Place = Middles[Side]
					+ Along.RotateVector(FVector((-Runs[Side] * 0.5f) + (Runs[Side] * (i + 0.5f) / Count), (StoneSize.Y * 0.5f) - 4.0f, 0.0f))
					+ FVector(0.0f, 0.0f, WallHeight + (StoneSize.Z * 0.5f));

				Stone->SetRelativeTransform(FTransform(Turn, Place - Turn.RotateVector(Bounds.GetCenter() * Scale), Scale));
				Stone->RegisterComponent();

				CopingStones.Add(Stone);
			}
		}
	}

	LidBars->ClearInstances();

	// roof it with sections of real fencing, if there are any
	for (UStaticMeshComponent* Panel : LidPanels)
	{
		if (Panel)
		{
			Panel->DestroyComponent();
		}
	}

	LidPanels.Reset();

	// under the wire by enough that the camera does not sit in it
	LidCameraStop->SetBoxExtent(FVector(FloorSize.X * 0.5f, FloorSize.Y * 0.5f, 2.0f));
	LidCameraStop->SetRelativeLocation(FVector(0.0f, 0.0f, WallHeight - 14.0f));
	LidCameraStop->SetCollisionEnabled(LidPanelMesh ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (LidPanelMesh)
	{
		const FBox Panel = LidPanelMesh->GetBoundingBox();
		const FVector PanelSize = Panel.GetSize();

		const int32 CountX = FMath::Max(1, FMath::RoundToInt32(FloorSize.X / FMath::Max(PanelSize.X, 1.0f)));
		const int32 CountY = FMath::Max(1, FMath::RoundToInt32(FloorSize.Y / FMath::Max(PanelSize.Z, 1.0f)));

		// tipped over so that its height runs across the cage and its face looks up
		const FRotator Flat(0.0f, 0.0f, 90.0f);
		const FVector Scale((FloorSize.X / CountX) / PanelSize.X, 1.0f, (FloorSize.Y / CountY) / PanelSize.Z);

		for (int32 i = 0; i < CountX; ++i)
		{
			for (int32 j = 0; j < CountY; ++j)
			{
				const FVector Cell((-FloorSize.X * 0.5f) + (FloorSize.X * (i + 0.5f) / CountX), (-FloorSize.Y * 0.5f) + (FloorSize.Y * (j + 0.5f) / CountY), WallHeight);

				UStaticMeshComponent* Section = NewObject<UStaticMeshComponent>(this);

				Section->SetStaticMesh(LidPanelMesh);
				Section->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Section->SetupAttachment(RootComponent);
				Section->SetRelativeTransform(FTransform(Flat, Cell - Flat.RotateVector(Panel.GetCenter() * Scale), Scale));
				Section->RegisterComponent();

				LidPanels.Add(Section);
			}
		}

		return;
	}

	// otherwise weld a grid of bars across the top
	LidBars->SetMaterial(0, BarMaterial);

	const float BarLengthX = FloorSize.X + (GutCageConstants::BarOverlap * 2.0f);
	const float BarLengthY = FloorSize.Y + (GutCageConstants::BarOverlap * 2.0f);

	// bars across the short side, spread evenly along the long side
	const int32 CountAlongX = FMath::Max(1, FMath::RoundToInt32(FloorSize.X / BarSpacing));

	for (int32 i = 1; i < CountAlongX; ++i)
	{
		const float X = (-FloorSize.X * 0.5f) + (FloorSize.X * i / CountAlongX);

		LidBars->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, 0.0f, WallHeight), FVector(BarThickness, BarLengthY, BarThickness) / CubeSize));
	}

	// bars along the long side, resting on top of the others
	const int32 CountAlongY = FMath::Max(1, FMath::RoundToInt32(FloorSize.Y / BarSpacing));

	for (int32 i = 1; i < CountAlongY; ++i)
	{
		const float Y = (-FloorSize.Y * 0.5f) + (FloorSize.Y * i / CountAlongY);

		LidBars->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.0f, Y, WallHeight + BarThickness), FVector(BarLengthX, BarThickness, BarThickness) / CubeSize));
	}
}

UStaticMeshComponent* AGutCage::CreateWall(FName Name, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Wall->SetupAttachment(RootComponent);
	Wall->SetStaticMesh(Mesh);
	Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	return Wall;
}
