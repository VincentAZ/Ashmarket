// Ashmarket prototype: the Gut.

#include "GutDroppedWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "GutWeaponData.h"

AGutDroppedWeapon::AGutDroppedWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the weapon shape
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetCollisionProfileName(FName("PhysicsActor"));

	// it lies on the floor without tripping fighters or catching the camera
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// the pool's lane lines and stains are on the pool, not on what lies in it
	Mesh->SetReceivesDecals(false);
}

void AGutDroppedWeapon::Drop(UGutWeaponData* InWeapon, const FVector& Scale, const FVector& Velocity)
{
	Weapon = InWeapon;

	if (!Weapon || !Weapon->Mesh)
	{
		return;
	}

	Mesh->SetStaticMesh(Weapon->Mesh);
	Mesh->SetMaterial(0, Weapon->Material);
	Mesh->SetWorldScale3D(Scale);

	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(Velocity);
}
