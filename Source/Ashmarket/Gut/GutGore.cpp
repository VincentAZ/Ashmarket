// Ashmarket prototype: the Gut.

#include "GutGore.h"
#include "Components/DecalComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GutGoreConstants
{
	/** Most splatters and pools kept on the floor at once */
	const int32 MaxDecals = 420;

	/** Most drops in the air at once, how long one may fly, and how hard it is pulled down */
	const int32 MaxFlying = 220;
	const float DropLife = 1.6f;
	const float Gravity = 980.0f;

	/** The pictures a drop in the air may be: whichever of them have been made (Tools/import_gut_blood.sh) */
	const TCHAR* DropPictures[] = { TEXT("/Game/Gut/Art/Blood/MI_Gut_blood_drop"), TEXT("/Game/Gut/Art/Blood/MI_Gut_blood_drop_2"), TEXT("/Game/Gut/Art/Blood/MI_Gut_blood_drop_3") };

	/** How far below a point the floor is looked for */
	const float FloorSearchDistance = 400.0f;

	/**
	 *  How far above and below the floor a mark reaches. No further than this: a mark that falls at the foot of a wall is
	 *  drawn up the wall as far as it reaches, in stripes.
	 */
	const float DecalDepth = 5.0f;

	/** Name of the color parameter on the gib material */
	const FName GibColorParameter("Color");
}

UDecalComponent* UGutGoreSubsystem::Splatter(UMaterialInterface* Material, const FVector& Location, float Radius)
{
	UWorld* World = GetWorld();

	if (!Material || !World || Radius <= 0.0f)
	{
		return nullptr;
	}

	// blood lands on the floor
	FHitResult Floor;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	if (!World->LineTraceSingleByObjectType(Floor, Location, Location - FVector(0.0f, 0.0f, GutGoreConstants::FloorSearchDistance), ObjectParams))
	{
		return nullptr;
	}

	// the oldest dries up to make room
	Decals.RemoveAll([](const UDecalComponent* Decal) { return !IsValid(Decal); });

	while (Decals.Num() >= GutGoreConstants::MaxDecals)
	{
		Decals[0]->DestroyComponent();
		Decals.RemoveAt(0);
	}

	// a decal looks along its own forward axis, so point it down, turned any way round
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Material, FVector(GutGoreConstants::DecalDepth, Radius, Radius),
		Floor.ImpactPoint, FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));

	if (Decal)
	{
		Decals.Add(Decal);
	}

	return Decal;
}

UDecalComponent* UGutGoreSubsystem::Pool(UMaterialInterface* Material, const FVector& Location, float StartRadius, float EndRadius, float SpreadTime)
{
	UDecalComponent* Decal = Splatter(Material, Location, StartRadius);

	if (Decal && SpreadTime > 0.0f && EndRadius > StartRadius)
	{
		FSpreadingPool Spreading;
		Spreading.Decal = Decal;
		Spreading.StartRadius = StartRadius;
		Spreading.EndRadius = EndRadius;
		Spreading.SpreadTime = SpreadTime;

		SpreadingPools.Add(Spreading);
	}

	return Decal;
}

UMaterialInterface* UGutGoreSubsystem::GetMark(EGutBloodMark Kind, UMaterialInterface* Otherwise)
{
	if (!bLookedForMarks)
	{
		bLookedForMarks = true;

		const auto Find = [](TArray<TObjectPtr<UMaterialInterface>>& Into, std::initializer_list<const TCHAR*> Names)
		{
			for (const TCHAR* Name : Names)
			{
				if (UMaterialInterface* Picture = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/Blood/MI_Gut_%s"), Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					Into.Add(Picture);
				}
			}
		};

		Find(ThrownMarks, { TEXT("blood_spatter_1"), TEXT("blood_spatter_2"), TEXT("blood_castoff"), TEXT("blood_splash"), TEXT("blood_mist") });
		Find(DrippedMarks, { TEXT("blood_drips"), TEXT("blood_drips_2"), TEXT("blood_drips_3"), TEXT("blood_mist") });
		Find(SmearedMarks, { TEXT("blood_smear") });
		Find(RanMarks, { TEXT("blood_run"), TEXT("blood_run_2") });
		Find(WoundMarks, { TEXT("blood_wound_1"), TEXT("blood_splash") });
		Find(GashMarks, { TEXT("blood_wound_2") });
		Find(SoakedMarks, { TEXT("blood_soak") });
	}

	const TArray<TObjectPtr<UMaterialInterface>>* Pictures = &ThrownMarks;

	switch (Kind)
	{
		case EGutBloodMark::Dripped: Pictures = &DrippedMarks; break;
		case EGutBloodMark::Smeared: Pictures = &SmearedMarks; break;
		case EGutBloodMark::Ran: Pictures = &RanMarks; break;
		case EGutBloodMark::Wound: Pictures = &WoundMarks; break;
		case EGutBloodMark::Gash: Pictures = &GashMarks; break;
		case EGutBloodMark::Soaked: Pictures = &SoakedMarks; break;
		default: break;
	}

	return Pictures->IsEmpty() ? Otherwise : (*Pictures)[FMath::RandHelper(Pictures->Num())].Get();
}

void UGutGoreSubsystem::Spray(UMaterialInterface* MarkMaterial, const FVector& From, const FVector& Direction, int32 Count, float Speed)
{
	UWorld* World = GetWorld();

	if (!World || Count <= 0)
	{
		return;
	}

	if (!bLookedForDropMaterials)
	{
		bLookedForDropMaterials = true;

		for (const TCHAR* Path : GutGoreConstants::DropPictures)
		{
			if (UMaterialInterface* Picture = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				DropMaterials.Add(Picture);
			}
		}
	}

	FVector Way = Direction.GetSafeNormal2D();

	if (Way.IsNearlyZero())
	{
		Way = FVector::ForwardVector;
	}

	for (int32 i = 0; i < Count && Flying.Num() < GutGoreConstants::MaxFlying; ++i)
	{
		FFlyingDrop Drop;

		// out the way the blow was going, in a fan, some of it up: most of it fast and small, a little of it slow and big
		const float Share = FMath::FRand();

		Drop.Place = From + FVector(FMath::FRandRange(-6.0f, 6.0f), FMath::FRandRange(-6.0f, 6.0f), FMath::FRandRange(-8.0f, 8.0f));
		Drop.Velocity = (Way.RotateAngleAxis(FMath::FRandRange(-38.0f, 38.0f), FVector::UpVector) * Speed * FMath::Lerp(0.25f, 1.25f, Share)) + FVector(0.0f, 0.0f, FMath::FRandRange(40.0f, 320.0f));
		Drop.Size = FMath::Lerp(5.0f, 1.6f, Share) * FMath::FRandRange(0.8f, 1.3f);
		Drop.Mark = MarkMaterial;

		if (!DropMaterials.IsEmpty())
		{
			if (!DropHolder)
			{
				DropHolder = World->SpawnActor<AActor>();

				if (DropHolder)
				{
					DropHolder->SetRootComponent(NewObject<USceneComponent>(DropHolder));
					DropHolder->GetRootComponent()->RegisterComponent();
				}
			}

			UMaterialBillboardComponent* Seen = SpareDrops.IsEmpty() ? nullptr : SpareDrops.Pop().Get();

			if (!Seen && DropHolder)
			{
				Seen = NewObject<UMaterialBillboardComponent>(DropHolder);
				Seen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Seen->SetCastShadow(false);
				Seen->SetupAttachment(DropHolder->GetRootComponent());
				Seen->RegisterComponent();
			}

			if (Seen)
			{
				Seen->SetElements({});
				// the drop takes up about half its picture
				Seen->AddElement(DropMaterials[FMath::RandHelper(DropMaterials.Num())], nullptr, false, Drop.Size * 2.0f, Drop.Size * 2.0f, nullptr);
				Seen->SetWorldLocation(Drop.Place);
				Seen->SetVisibility(true);

				Drop.Seen = Seen;
			}
		}

		Flying.Add(Drop);
	}
}

int32 UGutGoreSubsystem::GetNumBloodDecals() const
{
	int32 Count = 0;

	for (const UDecalComponent* Decal : Decals)
	{
		Count += IsValid(Decal) ? 1 : 0;
	}

	return Count;
}

void UGutGoreSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// the drops in the air: each falls until the floor is under it, and leaves its mark there
	for (int32 i = Flying.Num() - 1; i >= 0; --i)
	{
		FFlyingDrop& Drop = Flying[i];

		const FVector Before = Drop.Place;

		Drop.Age += DeltaTime;
		Drop.Velocity.Z -= GutGoreConstants::Gravity * DeltaTime;
		Drop.Place += Drop.Velocity * DeltaTime;

		FHitResult Floor;

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

		const bool bLanded = GetWorld()->LineTraceSingleByObjectType(Floor, Before, Drop.Place, ObjectParams);

		if (!bLanded && Drop.Age < GutGoreConstants::DropLife)
		{
			if (UMaterialBillboardComponent* Seen = Drop.Seen.Get())
			{
				Seen->SetWorldLocation(Drop.Place);
			}

			continue;
		}

		// one drop in three leaves a mark that can be seen, and the big ones always: the rest were mist
		if (bLanded && Drop.Mark.IsValid() && (Drop.Size > 3.6f || FMath::RandHelper(3) == 0))
		{
			Splatter(GetMark(EGutBloodMark::Dripped, Drop.Mark.Get()), Floor.ImpactPoint + FVector(0.0f, 0.0f, 4.0f), Drop.Size * FMath::FRandRange(1.2f, 2.6f));
		}

		if (UMaterialBillboardComponent* Seen = Drop.Seen.Get())
		{
			Seen->SetVisibility(false);

			SpareDrops.Add(Seen);
		}

		Flying.RemoveAtSwap(i);
	}

	for (int32 i = SpreadingPools.Num() - 1; i >= 0; --i)
	{
		FSpreadingPool& Spreading = SpreadingPools[i];
		UDecalComponent* Decal = Spreading.Decal.Get();

		if (!Decal)
		{
			SpreadingPools.RemoveAt(i);
			continue;
		}

		Spreading.Age += DeltaTime;

		// fast at first, then slower
		const float Alpha = FMath::Clamp(Spreading.Age / Spreading.SpreadTime, 0.0f, 1.0f);
		const float Radius = FMath::Lerp(Spreading.StartRadius, Spreading.EndRadius, FMath::Sqrt(Alpha));

		Decal->DecalSize = FVector(GutGoreConstants::DecalDepth, Radius, Radius);
		Decal->MarkRenderStateDirty();

		if (Alpha >= 1.0f)
		{
			SpreadingPools.RemoveAt(i);
		}
	}
}

TStatId UGutGoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGutGoreSubsystem, STATGROUP_Tickables);
}

AGutGib::AGutGib()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetCollisionProfileName(FName("PhysicsActor"));

	// it lies on the floor without tripping fighters or catching the camera
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AGutGib::ShowLimb(const TArray<USkeletalMeshComponent*>& Parts, const FVector& CutLocation)
{
	// near enough to nothing that what is left of the rest is a speck inside the stump
	const float Nothing = 0.001f;

	bool bShown = false;

	for (USkeletalMeshComponent* Part : Parts)
	{
		if (!Part || !Part->GetSkinnedAsset() || Part->GetBoneIndex(Bone) <= 0)
		{
			continue;
		}

		UPoseableMeshComponent* Limb = NewObject<UPoseableMeshComponent>(this);

		Limb->SetSkinnedAssetAndUpdate(Part->GetSkinnedAsset());
		Limb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Limb->SetReceivesDecals(false);
		// the shape it rides on is stretched to the limb's length: the limb keeps its own size
		Limb->SetUsingAbsoluteScale(true);
		Limb->SetWorldTransform(Part->GetComponentTransform());
		Limb->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform);
		Limb->RegisterComponent();

		for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
		{
			Limb->SetMaterial(Slot, Part->GetMaterial(Slot));
		}

		Limb->CopyPoseFromSkeletalComponent(Part);

		const int32 LimbIndex = Limb->GetBoneIndex(Bone);

		if (!Limb->BoneSpaceTransforms.IsValidIndex(LimbIndex))
		{
			Limb->DestroyComponent();

			continue;
		}

		// the whole skeleton shrinks to a point, and the limb alone is given its size back
		Limb->BoneSpaceTransforms[0].SetScale3D(FVector(Nothing));
		Limb->BoneSpaceTransforms[LimbIndex].SetScale3D(Limb->BoneSpaceTransforms[LimbIndex].GetScale3D() / Nothing);
		Limb->RefreshBoneTransforms();

		// that leaves the limb at the point everything else shrank to: carry it back to where it was cut
		Limb->AddWorldOffset(CutLocation - Limb->GetBoneLocationByName(Bone, EBoneSpaces::WorldSpace));
		Limb->SetBoundsScale(6.0f);

		bShown = true;
	}

	if (bShown)
	{
		Mesh->SetVisibility(false, false);
	}
}

void AGutGib::Throw(FName InBone, UStaticMesh* Shape, const FVector& Scale, UMaterialInterface* Material, const FLinearColor& Color, const FVector& Velocity)
{
	Bone = InBone;

	if (!Shape)
	{
		return;
	}

	Mesh->SetStaticMesh(Shape);
	Mesh->SetWorldScale3D(Scale);

	if (Material)
	{
		if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(0, Material))
		{
			Dynamic->SetVectorParameterValue(GutGoreConstants::GibColorParameter, Color);
		}
	}

	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-360.0f, 360.0f)));
}
