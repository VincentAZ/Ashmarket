// Ashmarket: the town outside the cage.

#include "TownGameMode.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GutPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GutFighter.h"
#include "EngineUtils.h"
#include "GutArmorData.h"
#include "GutCrowd.h"
#include "GutGameInstance.h"
#include "GutGameMode.h"
#include "GutLadderData.h"
#include "GutWeaponData.h"
#include "GutSound.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TownHUD.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTown, Log, All);

namespace TownConstants
{
	/** How long the words of a shut door stay up */
	const float NoticeTime = 3.5f;

	// how hard somebody walking shoves what is loose, at a full walk: about 260 newtons
	const float ShoveForce = 26000.0f;

	/**
	 *  A kick: how near the walker's shins a loose thing is struck, how many times the walker's own pace the lightest thing
	 *  is sent off at and the heaviest, and the weight in kg that takes one off that
	 */
	const float KickReach = 16.0f;
	const float KickMost = 2.2f;
	const float KickLeast = 1.25f;
	const float KickWeight = 28.0f;
	const FName LooseTag(TEXT("TownLoose"));

	/** The owner's yard as a place of its own, beside the Gut */
	const TCHAR* YardLevel = TEXT("/Game/Town/Lvl_Yard");

	/** How long the camera and the line on the screen stay with somebody who has just been handed something out of the bag */
	const float GivenTime = 6.5f;

	/** What they do with it: light it, if it is smokes, and put anything else to the mouth. Motion capture (for-review/motion-sources.md). */
	// Somebody saying a line talks with their hands, and whoever they say it to stands as somebody listening: performed,
	// from the idles pack. He said of a Warden: "still fake - like a narrator".
	const TCHAR* TalkGestures[] =
	{
		TEXT("mc_am_stand_conv_talk_01"), TEXT("mc_am_stand_conv_talk_05_generic"), TEXT("mc_am_stand_conv_talk_06_generic"), TEXT("mc_am_stand_conv_talk_08_generic"),
		TEXT("mc_am_stand_conv_talk_02_happy"), TEXT("mc_am_stand_conv_talk_07_happy"), TEXT("mc_am_stand_conv_talk_03_angry"), TEXT("mc_am_stand_conv_talk_04_angry"),
	};
	const TCHAR* ListenGesture = TEXT("mc_am_stand_conv_listen_01_generic");

	const TCHAR* GivenSmokes = TEXT("/Game/Gut/Art/Motion/mc_am_stand_idle_smoke_lightup.mc_am_stand_idle_smoke_lightup");
	const TCHAR* GivenToMouth = TEXT("/Game/Gut/Art/Motion/mc_am_stand_idle_smoke_inhale_02.mc_am_stand_idle_smoke_inhale_02");

	/**
	 *  Each place's patch of the map's picture: the stretch of the town it covers (the map's own metres, x east and y
	 *  north) and where on the picture that stretch is drawn, as shares of the picture from its top left. The picture is
	 *  ArtSource/mixar/ui/ui_map.png.
	 */
	struct FMapPatch
	{
		const TCHAR* Place;
		FBox2D Town;
		FBox2D Picture;
	};

	const FMapPatch MapPatches[] =
	{
		// with the Gate Yard and the tunnel at its foot (25 November): the same scale on down to the dam
		{ TEXT("Main Street"), FBox2D(FVector2D(-12.0f, -19.0f), FVector2D(12.0f, 127.0f)), FBox2D(FVector2D(0.479f, 0.8875f), FVector2D(0.521f, 0.548f)) },
		{ TEXT("Block Square"), FBox2D(FVector2D(-7.5f, 130.0f), FVector2D(30.7f, 166.0f)), FBox2D(FVector2D(0.456f, 0.544f), FVector2D(0.541f, 0.433f)) },
		{ TEXT("Pool Street"), FBox2D(FVector2D(2.0f, 172.0f), FVector2D(108.0f, 200.0f)), FBox2D(FVector2D(0.508f, 0.460f), FVector2D(0.661f, 0.420f)) },
	};

	/** Where the level's own middle is on the town's map: every town level is laid out from the same one */
	const FVector2D MapOrigin(95.0f, 208.0f);

	/**
	 *  How the book's camera looks at somebody: from this far in front of them, level with a point this high over their
	 *  feet, this wide. That puts their head and chest in the top half of the screen, over the book, and nobody else's.
	 */
	/** The picture of them on the card: how far off the eye stands, how high it looks, how wide it sees, and the picture's size */
	const float BookEyeDistance = 330.0f;
	const float BookEyeHeight = 90.0f;
	const float BookEyeFieldOfView = 27.0f;
	const float BookEyeAside = 16.0f;
	const float BookLampCandelas = 9000.0f;
	const float BookStudioBelow = 6000.0f;
	const float BookSitterSettles = 0.7f;
	const int32 BookPictureWide = 540;
	const int32 BookPictureHigh = 720;

	const float BookCameraDistance = 250.0f;
	const float BookCameraHeight = 120.0f;
	const float BookCameraFieldOfView = 55.0f;

	// the street's own camera: over the right shoulder at about the height of the walker's eyes, a little down
	const FVector StreetCameraFrom(0.0f, 0.0f, 70.0f);
	const FVector StreetCameraOffset(0.0f, 52.0f, -32.0f);
	const float StreetCameraBoom = 400.0f;
	const float StreetCameraPitch = -6.0f;
	const float BookCameraBlend = 0.35f;

	/** What the builder marks a light with that is a fire (FIRE_TAG in Tools/build_town_level.py), and how fast a fire breathes and flutters */
	const FName FireTag = FName("TownFire");
	const float FireBreath = 1.3f;
	const float FireFlutter = 7.0f;

	/** How loud somebody talked to is, and whoever calls over the place; and how loud the caller is while somebody else is being talked to */
	const float TalkVoiceVolume = 1.0f;

	/** What a voice is turned up by to be heard from where its owner stands as it was heard laid into the ear: the falloff takes some of it at two metres */
	const float TalkVoiceAtArmsLength = 1.35f;
	const float CallVoiceVolume = 0.8f;
	const float CallVoiceUnderTalk = 0.25f;

	/** A fire's light at least this bright (candelas) is a fire that is heard */
	const float FireHeardFrom = 1500.0f;

	/**
	 *  What the market sells. Each is one plain thing with one plain use, bought at a stall and used from the bag.
	 *  Who sells which is the place's own business (FTownPerson::Sells).
	 */
	struct FGood
	{
		const TCHAR* Id;
		const TCHAR* Name;

		/** What it is for, in a few words, beside its name */
		const TCHAR* Detail;

		/** More about it, under the list */
		const TCHAR* About;

		float Price;

		/** What somebody who buys such things gives for it. Nought: nobody buys it. */
		float Worth = 0.0f;

		/** The water in it, which whoever holds it can pour into their own. Nought: none. */
		float Pours = 0.0f;
	};

	const FGood Goods[] =
	{
		{ TEXT("bread"), TEXT("Flat bread"), TEXT("wind, in your next fight"), TEXT("Hot, and heavy as a hand. Eaten before a fight, your wind comes back a quarter faster all through it. One fight's worth."), 2.0f },
		{ TEXT("tea"), TEXT("A cup of tea"), TEXT("steadies you, in your next fight"), TEXT("Leaves, and hot. Drunk before a fight, your wind starts coming back sooner after every effort in it. One fight's worth."), 0.1f },
		{ TEXT("splint"), TEXT("Splint and binding"), TEXT("mends a hurt limb"), TEXT("Two slats and a strip of inner tube. Bound on, it gives your worst-hurt limb back half of what it has lost. It does nothing for a limb that is broken through: that is the cutter's work."), 6.0f },

		// what is found lying about, not sold: a price of nought, and what it is good for
		{ TEXT("flask"), TEXT("A dented flask"), TEXT("half a litre in it"), TEXT("Army steel, older than anybody alive. Somebody put it down and did not come back for it. There is half a litre in it: poured into your own, it is yours."), 0.0f, 0.0f, 0.5f },
		{ TEXT("scrap"), TEXT("Scrap"), TEXT("the scrap seller pays for it"), TEXT("A fist of bolts, a coil of copper wire, a gear. Worth nothing to you, and a litre to anybody who mends things: the scrap seller on Main Street."), 0.0f, 1.0f, 0.0f },
		{ TEXT("smokes"), TEXT("Dry smokes"), TEXT("somebody will thank you for them"), TEXT("Four, rolled by hand, dry, in a tin. Nothing to smoke has been grown here in years. Given to somebody walking with you, it is remembered. The tea seller on Main Street will take them off you too."), 0.0f, 1.5f, 0.0f },
		{ TEXT("tag"), TEXT("A fighter's tag"), TEXT("the scrap seller pays for it"), TEXT("A stamped disc on a cord. Whoever wore it has no more use for it, and steel is steel: two litres from the scrap seller on Main Street."), 0.0f, 2.0f, 0.0f },
	};

	/** The folder a find's model is kept in, as far as the name of the good */
	const TCHAR* FindModel = TEXT("/Game/Gut/Art/Mixar/find_");

	/** How near a find has to be to be picked up, and what smokes take off what somebody thinks of whoever owns them */
	const float FindReach = 150.0f;
	const float SmokesEase = 7.0f;

	const FGood* FindGood(const FName& Id)
	{
		for (const FGood& Good : Goods)
		{
			if (Id == Good.Id)
			{
				return &Good;
			}
		}

		return nullptr;
	}

	/** How many of the Block's numbers have a bill of sale of their own, and where the armor pieces are kept and how they are named */
	const int32 LotPapers = 6;
	const TCHAR* ArmorFolder = TEXT("/Game/Gut/Armor");
	const TCHAR* ArmorPrefix = TEXT("DA_Gut_Armor_");

	/** What being given food, and being bound up, takes off what somebody thinks of whoever owns them */
	const float FoodEases = 4.0f;
	const float CareEases = 8.0f;

	/** A price in words: tenths of a litre are said as tenths */
	FString Litres(float Water)
	{
		return FMath::IsNearlyEqual(Water, FMath::RoundToFloat(Water), 0.01f) ? FString::Printf(TEXT("%.0f L"), Water) : FString::Printf(TEXT("%.1f L"), Water);
	}

	/** The colours of a built person's clothes, as their materials name them */
	const FName ClothParameters[] = { FName("diffuse_color_1"), FName("diffuse_color_2") };

	/** How much darker and lighter the same cloth is on one person than on another */
	const float DarkestShade = 0.7f;
	const float LightestShade = 1.2f;

	/** The parts of a built person that are not clothes */
	const FName BodyPart("Body");
	const FName FacePart("Face");
}

/** The cage's rules for an owner, lent to the town for as long as this lives: they work on the run given, with no cage open */
struct FTownLentRules
{
	AGutGameMode* Rules;

	FTownLentRules(const AGutGameMode* From, UGutGameInstance* Run) : Rules(const_cast<AGutGameMode*>(From))
	{
		if (Rules)
		{
			Rules->LendRulesTo(Run);
		}
	}

	~FTownLentRules()
	{
		if (Rules)
		{
			Rules->LendRulesTo(nullptr);
		}
	}
};

ATownGameMode::ATownGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// the player in the street is the player of the cage: the same fighter, with the same hands on the same keys
	static ConstructorHelpers::FClassFinder<APawn> Player(TEXT("/Game/Gut/Blueprints/BP_GutPlayer"));
	static ConstructorHelpers::FClassFinder<APlayerController> Hands(TEXT("/Game/Gut/Blueprints/BP_GutPlayerController"));

	if (Player.Succeeded())
	{
		DefaultPawnClass = Player.Class;
	}

	if (Hands.Succeeded())
	{
		PlayerControllerClass = Hands.Class;
	}

	HUDClass = ATownHUD::StaticClass();

	Place = TSoftObjectPtr<UTownPlaceData>(FSoftObjectPath(TEXT("/Game/Town/DA_Town_PoolStreet.DA_Town_PoolStreet")));
	PersonClass = TSoftClassPtr<AGutFighter>(FSoftObjectPath(TEXT("/Game/Gut/Blueprints/BP_GutDummy.BP_GutDummy_C")));
	FirstLadder = TSoftObjectPtr<UGutLadderData>(FSoftObjectPath(TEXT("/Game/Gut/Ladder/DA_Gut_MeatLadder.DA_Gut_MeatLadder")));
	CageRules = TSoftClassPtr<AGutGameMode>(FSoftObjectPath(TEXT("/Game/Gut/Blueprints/BP_GutGameMode.BP_GutGameMode_C")));
	Story = TSoftObjectPtr<UTownStoryData>(FSoftObjectPath(TEXT("/Game/Town/DA_Town_Story.DA_Town_Story")));
}

void ATownGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// each place has its list beside its level: Lvl_BlockSquare is described by DA_Town_BlockSquare
	FString Short = FPackageName::GetShortName(UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));

	if (Short.RemoveFromStart(TEXT("Lvl_")))
	{
		const FString Listed = FString::Printf(TEXT("/Game/Town/DA_Town_%s"), *Short);

		if (FPackageName::DoesPackageExist(Listed))
		{
			Place = TSoftObjectPtr<UTownPlaceData>(FSoftObjectPath(FString::Printf(TEXT("%s.DA_Town_%s"), *Listed, *Short)));
		}
	}

	PlaceData = Place.LoadSynchronous();
	LadderData = FirstLadder.LoadSynchronous();

	bBids = !FParse::Param(FCommandLine::Get(), TEXT("GutNoBidding"));

	// the town's story, where it has been written down (Tools/build_town_level.sh writes it with every place)
	if (FPackageName::DoesPackageExist(Story.GetLongPackageName()))
	{
		StoryData = Story.LoadSynchronous();
	}

	// -GutMarks=w1=100,wynn=1 sets the town's marks as the place opens (commas for spaces): for seeing how something came
	// out without doing it again
	if (FString Marks; FParse::Value(FCommandLine::Get(), TEXT("GutMarks="), Marks, false))
	{
		Apply(Marks.Replace(TEXT(","), TEXT(" ")));
	}

	if (!PlaceData)
	{
		UE_LOG(LogTown, Warning, TEXT("The street has no list of who and what is in it (%s): it is empty and has no edges"), *Place.ToString());
	}
}

UGutGameInstance* ATownGameMode::GetGutGameInstance() const
{
	return Cast<UGutGameInstance>(GetGameInstance());
}

AGutFighter* ATownGameMode::GetWalker() const
{
	return Cast<AGutFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ATownGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// -GutFrameRate on the command line writes the frame rate to the log every five seconds of real time, as it does in the cage
	static const bool bLogFrameRate = FParse::Param(FCommandLine::Get(), TEXT("GutFrameRate"));

	if (bLogFrameRate)
	{
		static double Since = FPlatformTime::Seconds();
		static int32 Frames = 0;
		static double Slowest = 0.0;
		static double LastFrame = FPlatformTime::Seconds();

		const double Now = FPlatformTime::Seconds();

		Slowest = FMath::Max(Slowest, Now - LastFrame);
		LastFrame = Now;
		++Frames;

		// -GutProfileAt=40 has the engine write where one frame's time on the graphics card went, that many seconds in
		static float ProfileAt = 0.0f;
		static bool bProfileRead = false;

		if (!bProfileRead)
		{
			bProfileRead = true;
			FParse::Value(FCommandLine::Get(), TEXT("GutProfileAt="), ProfileAt);
		}

		if (ProfileAt > 0.0f && GetWorld()->GetRealTimeSeconds() >= ProfileAt)
		{
			ProfileAt = 0.0f;
			GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
		}

		if (Now - Since >= 5.0)
		{
			UE_LOG(LogTown, Log, TEXT("Frame rate: %.0f a second, slowest frame %.0f ms"), Frames / (Now - Since), Slowest * 1000.0);

			Since = Now;
			Frames = 0;
			Slowest = 0.0;
		}
	}

	if (!bSettled)
	{
		Settle();
		return;
	}

	SettledFor += DeltaTime;

	UpdateCalls(DeltaTime);
	UpdateAuction(DeltaTime);
	UpdateSoundsDue();
	UpdateLed(DeltaTime);
	UpdateBeats(DeltaTime);
	UpdateTow(DeltaTime);
	UpdateFires();

	if (bAtBook)
	{
		ShowBookLine();
	}

	// one purse: whatever was gained or spent by any way since the last look is the player's, and all three ways hold it
	if (UGutGameInstance* Run = GetGutGameInstance())
	{
		Run->ShareWater();
	}

	// Something loose that has been set going scrapes on the ground; and it is kept on the place's ground and out of what
	// stands in it by the same list that keeps a walker there: nothing in the town is stopped by the shapes of things.
	for (int32 i = 0; i < Loose.Num(); ++i)
	{
		UStaticMeshComponent* Thing = Loose[i].Get();
		const bool bGoing = Thing && Thing->GetPhysicsLinearVelocity().SizeSquared() > FMath::Square(45.0f);

		if (Thing && Thing->IsAnyRigidBodyAwake() && LooseBegan.IsValidIndex(i))
		{
			const FVector Middle = Thing->Bounds.Origin;
			const FVector2D Flat(Middle.X, Middle.Y);
			const FVector2D Kept = GetKeptClear(Flat, FMath::Min(FMath::Max(Thing->Bounds.BoxExtent.X, Thing->Bounds.BoxExtent.Y) * 0.8f, 45.0f));

			if (Middle.Z < LooseBegan[i].GetLocation().Z - 150.0f)
			{
				// through the ground, where the ground is not solid: back where it began, and left there
				Thing->SetWorldTransform(LooseBegan[i], false, nullptr, ETeleportType::ResetPhysics);
				Thing->PutRigidBodyToSleep();
			}
			else if (!Kept.Equals(Flat, 0.5f))
			{
				const FVector Back(Kept.X - Flat.X, Kept.Y - Flat.Y, 0.0f);
				const FVector Out = Back.GetSafeNormal();

				FVector Going = Thing->GetPhysicsLinearVelocity();

				// brought back, and what it was doing into the wall turned round and mostly lost
				if (const float Into = -FVector::DotProduct(Going, Out); Into > 0.0f)
				{
					Going += Out * Into * 1.3f;
				}

				Thing->SetWorldLocation(Thing->GetComponentLocation() + Back, false, nullptr, ETeleportType::TeleportPhysics);
				Thing->SetPhysicsLinearVelocity(Going);
			}
		}

		// Walked into, it is kicked on: sent off faster than whoever walked into it, further the lighter it is, with some
		// spin and a little lift, and left to the ground to stop. What did this before was the walker's own slow push: a
		// thing was leaned on at walking pace and stopped a hand's width on, and a barrel did not go at all ("hardly
		// anything is moveable", 24 November).
		static const bool bNoKick = FParse::Param(FCommandLine::Get(), TEXT("GutNoLoose"));

		if (const AGutFighter* Kicker = bNoKick ? nullptr : GetWalker(); Kicker && Thing)
		{
			const FVector Stride(Kicker->GetVelocity().X, Kicker->GetVelocity().Y, 0.0f);
			const float Pace = Stride.Size();
			const FVector Feet = Kicker->GetActorLocation() - FVector(0.0f, 0.0f, Kicker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const float Girth = Kicker->GetCapsuleComponent()->GetScaledCapsuleRadius();

			if (Pace > 35.0f && FVector::DistSquared2D(Thing->Bounds.Origin, Feet) < FMath::Square(Thing->Bounds.SphereRadius + Girth + 30.0f))
			{
				// the nearest of it to the walker's shins
				FVector Nearest = FVector::ZeroVector;

				const FVector Shin = Feet + FVector(0.0f, 0.0f, 35.0f);

				if (const float Off = Thing->GetClosestPointOnCollision(Shin, Nearest); Off >= 0.0f && Nearest.Z < Feet.Z + 105.0f && Nearest.Z > Feet.Z - 20.0f
					&& FVector::DistSquared2D(Nearest, Feet) < FMath::Square(Girth + TownConstants::KickReach))
				{
					const FVector Along = Stride / Pace;
					const FVector To = FVector(Thing->Bounds.Origin.X - Feet.X, Thing->Bounds.Origin.Y - Feet.Y, 0.0f).GetSafeNormal();

					// only what is ahead of the feet, and only if it is not already going away faster
					if (FVector::DotProduct(Along, To) > 0.25f)
					{
						const float Weight = FMath::Max(Thing->GetMass(), 1.0f);
						const float Sent = Pace * FMath::Clamp(TownConstants::KickMost - (Weight / TownConstants::KickWeight), TownConstants::KickLeast, TownConstants::KickMost);
						const FVector Way = ((Along * 0.65f) + (To * 0.35f)).GetSafeNormal();

						FVector Going = Thing->GetPhysicsLinearVelocity();

						if (const float Has = FVector::DotProduct(Going, Way); Has < Pace * 0.95f)
						{
							Going += Way * (Sent - Has);
							Going.Z = FMath::Max(Going.Z, Sent * FMath::Clamp(0.30f - (Weight / 120.0f), 0.05f, 0.25f));

							Thing->WakeAllRigidBodies();
							Thing->SetPhysicsLinearVelocity(Going);

							// and turned over the way it was struck: about the level line across the kick, as far as its size lets a kick turn it
							const FVector Over = FVector::CrossProduct(FVector::UpVector, Way);
							const float Round = FMath::Max(Thing->Bounds.SphereRadius, 12.0f);

							Thing->SetPhysicsAngularVelocityInDegrees(Over * FMath::RadiansToDegrees(Sent / Round) * FMath::FRandRange(0.15f, 0.45f)
								+ FVector(0.0f, 0.0f, FMath::FRandRange(-120.0f, 120.0f)), true);
						}
					}
				}
			}
		}

		if (bGoing && !LooseMoving[i])
		{
			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(FName("scuff"), Thing->GetComponentLocation(), 0.7f, FMath::FRandRange(0.8f, 0.95f));
			}
		}

		LooseMoving[i] = bGoing;
	}

	if (AGutFighter* Walker = GetWalker())
	{
		KeepOnGround(Walker);
		FindReach(Walker);

		// whoever is near enough to be spoken to is looked at, as they look at whoever comes up to them
		Walker->SetLookTarget(People.IsValidIndex(PersonInReach) ? People[PersonInReach].Get() : nullptr);

		// and as often as not they give whoever has walked up a nod, once, and not again for a while
		static const bool bNoNod = FParse::Param(FCommandLine::Get(), TEXT("GutNoGreet"));

		if (PersonInReach != NoddedAt && People.IsValidIndex(PersonInReach) && People[PersonInReach] && !bNoNod && !IsTalking() && !bShopping)
		{
			const double Now = GetWorld()->GetTimeSeconds();
			double& Last = Nodded.FindOrAdd(PersonInReach, -1000.0);

			// -GutAlwaysGreet: every time, for looking at it
			static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("GutAlwaysGreet"));

			if (Now - Last > 90.0 && !People[PersonInReach]->IsFidgeting() && (bAlways || FMath::FRand() < 0.6f))
			{
				const TCHAR* Nod = (PersonInReach % 2) ? TEXT("mc_am_stand_idle_greet_02_nod") : TEXT("mc_am_stand_idle_greet_04_nod");

				People[PersonInReach]->Gesture(Cast<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/Motion/%s.%s"), Nod, Nod)).TryLoad()));

				Last = Now;
			}
		}

		NoddedAt = PersonInReach;

		// an open way to another place is gone through by walking up to it
		if (PlaceData && SettledFor > 1.0f && !IsTalking() && !bShopping && !bStoppedMenu)
		{
			for (const FTownDoor& Door : PlaceData->Doors)
			{
				if (Door.bWalkedThrough && Door.Deed == ETownDeed::Go && Holds(Door.When) && Door.IsEntered(Walker->GetActorLocation(), WayReach))
				{
					const FTownDoor Through = Door;

					Apply(Through.Does);
					Do(Through.Deed, &Through);
					return;
				}
			}
		}

		// while they talk the camera comes round to look past the player at whoever is speaking, wherever it was before,
		// and stays there while the player is at their counter
		const int32 Faced = IsTalking() ? TalkingTo : (bShopping ? ShopKeeper : INDEX_NONE);

		// and for a few seconds to whoever has just been handed something out of the bag, so that it is seen to be taken
		const AGutFighter* Looked = People.IsValidIndex(Faced) ? People[Faced].Get() : nullptr;

		if (!Looked && GivenTo.IsValid() && GetWorld()->GetTimeSeconds() < GivenUntil)
		{
			Looked = GivenTo.Get();
		}

		if (!Looked && bAtYardPage)
		{
			const int32 Which = YardFighters.IndexOfByKey(YardFocus);

			Looked = YardBodies.IsValidIndex(Which) ? YardBodies[Which].Get() : nullptr;
		}

		if (AController* Eyes = Walker->GetController(); Eyes && Looked)
		{
			const FVector Toward = Looked->GetActorLocation() - Walker->GetActorLocation();

			if (!Toward.IsNearlyZero())
			{
				// a camera that has been locked keeps its own height through a talk: it comes round, and no more
				const UGutGameInstance* Kept = GetGutGameInstance();

				const FRotator Wanted((Kept && Kept->bCameraLocked) ? Kept->CameraLockedPitch : TalkCameraPitch, Toward.Rotation().Yaw + TalkCameraYaw, 0.0f);

				Eyes->SetControlRotation(FMath::RInterpTo(Eyes->GetControlRotation(), Wanted, DeltaTime, TalkCameraTurnSpeed));
			}
		}
	}
}

void ATownGameMode::Settle()
{
	AGutFighter* Walker = GetWalker();

	if (!Walker || !Walker->HasActorBegunPlay())
	{
		return;
	}

	if (!bWalkerSettled)
	{
		// nobody carries a weapon or wears a collar in the street, and nobody is on their guard
		Walker->EquipWeapon(nullptr);
		Walker->SetCollared(false);
		// the player stands plainly, whatever they would have been dealt: a hand on the hip all day is somebody else's habit
		Walker->SetAtEaseFamily(FName("stand"));
		Walker->SetAtEase(true);

		// What is loose in a place goes when it is walked into (a tyre, a crate, a can: the builder's LOOSE). It is shoved
		// about as hard as a shin shoves, whatever it is, so its own weight decides how far it goes: a tyre skids off and a
		// full barrel grudges an inch. Not struck like a ball at the first touch. -GutNoLoose: they stand like walls.
		if (UCharacterMovementComponent* Legs = Walker->GetCharacterMovement())
		{
			static const bool bNoLoose = FParse::Param(FCommandLine::Get(), TEXT("GutNoLoose"));

			Legs->bEnablePhysicsInteraction = !bNoLoose;
			Legs->bPushForceScaledToMass = false;
			Legs->bScalePushForceToVelocity = true;
			Legs->PushForceFactor = TownConstants::ShoveForce;
			Legs->InitialPushForceFactor = 0.0f;
			Legs->TouchForceFactor = 0.0f;
			Legs->RepulsionForce = 0.0f;
		}

		Loose.Reset();

		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TownConstants::LooseTag) && It->GetStaticMeshComponent())
			{
				Loose.Add(It->GetStaticMeshComponent());
			}
		}

		LooseMoving.Init(false, Loose.Num());
		LooseBegan.Reset();

		for (const TWeakObjectPtr<UStaticMeshComponent>& Thing : Loose)
		{
			LooseBegan.Add(Thing->GetComponentTransform());
		}

		UE_LOG(LogTown, Log, TEXT("Street: %d things in the place are loose, to be shoved by whoever walks into them"), Loose.Num());

		for (const TWeakObjectPtr<UStaticMeshComponent>& Thing : Loose)
		{
			const FVector2D Flat(Thing->Bounds.Origin.X, Thing->Bounds.Origin.Y);

			if (const FVector2D Kept = GetKeptClear(Flat, 12.0f); !Kept.Equals(Flat, 4.0f))
			{
				UE_LOG(LogTown, Log, TEXT("Street: loose %s (%s) at %.0f %.0f is set down in something that stands there: %.0f cm from clear ground"), *Thing->GetOwner()->GetName(),
					*GetNameSafe(Thing->GetStaticMesh()), Flat.X, Flat.Y, FVector2D::Distance(Kept, Flat));
			}
		}

		// somebody who has come out of the cage is stood where they went into it from, and somebody who has walked here from
		// another place where that way comes in. Anybody else stands where the level starts its player.
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			const FName Here(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));

			TOptional<FTransform> Stood;

			if (GameInstance->TownLevel == Here && GameInstance->TownStood.IsSet())
			{
				Stood = GameInstance->TownStood;
			}
			else if (const FTownArrival* Way = PlaceData ? PlaceData->Arrivals.FindByPredicate([GameInstance](const FTownArrival& Arrival) { return Arrival.Name == GameInstance->TownArrive; }) : nullptr)
			{
				Stood = FTransform(FRotator(0.0f, Way->Facing, 0.0f), Way->Place + FVector(0.0f, 0.0f, Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f));
			}

			if (Stood.IsSet())
			{
				Walker->SetActorLocationAndRotation(Stood->GetLocation(), FRotator(0.0f, Stood->Rotator().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

				if (AController* Eyes = Walker->GetController())
				{
					Eyes->SetControlRotation(FRotator(Eyes->GetControlRotation().Pitch, Stood->Rotator().Yaw, 0.0f));
				}
			}

			GameInstance->TownLevel = Here;
			GameInstance->TownStood.Reset();
			GameInstance->TownArrive = NAME_None;
		}

		// in the street the camera is at the walker's own height and nearly level. The cage's camera stands well above the
		// fighter and looks down, to see the other one over a shoulder: looked down on like that, anybody walking is
		// foreshortened, short in the leg and broad in the back.
		if (AGutPlayerCharacter* Seen = Cast<AGutPlayerCharacter>(Walker))
		{
			// the boom starts at the walker's head and not at their middle: where a wall behind them pushes the camera up
			// the boom, it comes up to the back of a head and not the back of a pair of shorts
			Seen->GetCameraBoom()->TargetOffset = TownConstants::StreetCameraFrom;
			Seen->GetCameraBoom()->SocketOffset = TownConstants::StreetCameraOffset;
			Seen->GetCameraBoom()->TargetArmLength = TownConstants::StreetCameraBoom;

			if (AController* Eyes = Walker->GetController())
			{
				const UGutGameInstance* Kept = GetGutGameInstance();

				Eyes->SetControlRotation(FRotator((Kept && Kept->bCameraLocked) ? Kept->CameraLockedPitch : TownConstants::StreetCameraPitch, Eyes->GetControlRotation().Yaw, 0.0f));
			}
		}

		bWalkerSettled = true;
	}

	UClass* Kind = PersonClass.LoadSynchronous();

	// the people are stood one to a frame, from a few frames after the level opens. (All of a place's people made in the
	// frame it opens in, ten of them in Block Square, stopped the renderer dead two frames later, every time.)
	if (PlaceData && Kind && People.Num() < PlaceData->People.Num())
	{
		if (++FramesSettling <= 4)
		{
			return;
		}

		// -TownPeople=3 stands only the first so many: for finding out what a place costs
		int32 Most = MAX_int32;
		FParse::Value(FCommandLine::Get(), TEXT("TownPeople="), Most);

		// whoever the town's marks do not have in the place is not stood in it: they keep their place in the list, empty
		const int32 Next = People.Num();

		People.Add((Next < Most && IsPresent(Next)) ? StandPerson(Next) : nullptr);

		// the next of them next frame
		return;
	}

	if (const UGutGameInstance* GameInstance = GetGutGameInstance())
	{
		GameInstance->ApplyLoudness(this);
	}

	FindFires();

	// whoever is brought into the yard has been brought home: they take their places with the rest
	if (IsYard())
	{
		HandInTow();
	}

	SettleOwned();
	SettleYard();

	// what the place sounds like, under the night
	if (UGutSoundSubsystem* Sounds = PlaceData ? UGutSoundSubsystem::Get(this) : nullptr)
	{
		for (const FName& Bed : PlaceData->Beds)
		{
			Sounds->StartBed(Bed, 1.0f, 3.0f);
		}

		// the night has things in it a long way off: gunfire, and now and then somebody. -GutNoFarSounds is a night without.
		static const bool bFarSounds = !FParse::Param(FCommandLine::Get(), TEXT("GutNoFarSounds"));

		Sounds->SetStrays(bFarSounds);
	}

	LayFinds();

	// whoever stands about a place where people are being sold is watching the sale: their heads are turned to the Block
	if (PlaceData)
	{
		FVector Sale = FVector::ZeroVector;
		int32 Lots = 0;

		for (const FTownPerson& Person : PlaceData->People)
		{
			if (Person.Lot > 0)
			{
				Sale += Person.Place;
				++Lots;
			}
		}

		if (Lots > 0)
		{
			// about the height of the faces of people standing on it
			const FVector Watched = (Sale / Lots) + FVector(0.0f, 0.0f, 160.0f);

			for (TActorIterator<AGutCrowd> It(GetWorld()); It; ++It)
			{
				It->SetWatched(Watched);
			}
		}
	}

	bSettled = true;

	UE_LOG(LogTown, Log, TEXT("Street: %s, %d people, %d doors, %d fires"), PlaceData ? *PlaceData->Name : TEXT("nowhere"), People.Num(), PlaceData ? PlaceData->Doors.Num() : 0, Fires.Num());
}

AGutFighter* ATownGameMode::StandPerson(int32 Index, const FVector* At)
{
	UClass* Kind = PersonClass.LoadSynchronous();
	AGutFighter* Walker = GetWalker();

	if (!PlaceData || !PlaceData->People.IsValidIndex(Index) || !Kind)
	{
		return nullptr;
	}

	const float HalfHeight = Kind->GetDefaultObject<AGutFighter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bCollars = !FParse::Param(FCommandLine::Get(), TEXT("TownNoCollars"));

	const FTownPerson& Person = PlaceData->People[Index];

	FVector Feet = At ? *At : Person.Place;

	// somebody up on something stands on it: whatever is under them there
	if (Person.bStandsOnWhatIsThere)
	{
		FCollisionQueryParams Looking(FName("TownStanding"), true, Walker);
		FHitResult Under;

		if (GetWorld()->LineTraceSingleByChannel(Under, Feet + FVector(0.0f, 0.0f, 230.0f), Feet - FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, Looking))
		{
			UE_LOG(LogTown, Log, TEXT("Street: under %s is %s, %.0f cm up"), *Person.Name, *GetNameSafe(Under.GetActor()), Under.Location.Z - Feet.Z);

			Feet.Z = Under.Location.Z;
		}
	}

	// where they stand is where their feet are
	const FTransform Where(FRotator(0.0f, Person.Facing, 0.0f), Feet + FVector(0.0f, 0.0f, HalfHeight));

	AGutFighter* Body = GetWorld()->SpawnActorDeferred<AGutFighter>(Kind, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Body)
	{
		return nullptr;
	}

	// the body they are seen as is named before they begin, so that they are never anybody else first
	if (!Person.Body.IsNull())
	{
		Body->SetBodyClass(Person.Body);
	}

	if (!Person.Stand.IsNone())
	{
		Body->SetAtEaseFamily(Person.Stand);
	}
	else if (Person.Idle)
	{
		Body->SetAtEaseIdle(Person.Idle);
	}

	// and their own way of walking, if they have one
	Body->SetAtEaseWalk(Person.Walk, Person.WalkSpeed);

	UGameplayStatics::FinishSpawningActor(Body, Where);

	Body->EquipWeapon(nullptr);
	Body->SetCollared(Person.bCollared && bCollars);
	Body->SetAtEase(true);

	// somebody stood up on something stays where they were stood: a truck's deck is not ground to whatever moves a
	// fighter about, which would walk them off the end of it
	if (Person.bStandsOnWhatIsThere)
	{
		Body->GetCharacterMovement()->DisableMovement();
	}

	// somebody with a round to walk is walked along it, from its first stop to its second
	if (!Person.Beat.IsEmpty())
	{
		Body->GetCharacterMovement()->DisableMovement();

		FTownBeat Round;
		Round.Person = Index;
		Round.Stop = Person.Beat.Num() > 1 ? 1 : 0;
		Round.Wait = Person.Beat[0].Wait;

		Beats.Add(Round);
	}

	// their clothes are their own, a shade off anybody else's who has the same
	Dye(Body, Person.Cloth);

	// a lot on the Block has its wrists tied behind it
	Body->SetBound(Person.bCollared && Person.Lot > 0);

	UE_LOG(LogTown, Log, TEXT("Street: %s stands at %s"), *Person.Name, *Feet.ToCompactString());

	return Body;
}

void ATownGameMode::Dye(const AGutFighter* Body, const FLinearColor& Cloth)
{
	const AActor* Seen = Body ? Body->GetBodyActor() : nullptr;

	if (!Seen)
	{
		return;
	}

	// people are dressed when they are made, each piece in its own colour, so the colour a place gives somebody is no
	// longer what their clothes are painted: it only says how dark or light theirs are beside somebody else's
	const float Shade = FMath::Lerp(TownConstants::DarkestShade, TownConstants::LightestShade, FMath::Frac((Cloth.R * 37.1f) + (Cloth.G * 91.7f) + (Cloth.B * 53.3f)));

	for (UActorComponent* Component : Seen->GetComponents())
	{
		USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);

		if (!Part || Part->GetFName() == TownConstants::BodyPart || Part->GetFName() == TownConstants::FacePart)
		{
			continue;
		}

		for (int32 i = 0; i < Part->GetNumMaterials(); ++i)
		{
			// only cloth that takes a dye at all: what was bought as a whole outfit keeps its maker's colours
			const UMaterialInterface* Worn = Part->GetMaterial(i);

			FLinearColor Now;

			if (!Worn || !Worn->GetVectorParameterValue(FHashedMaterialParameterInfo(TownConstants::ClothParameters[0]), Now))
			{
				continue;
			}

			if (UMaterialInstanceDynamic* Dyed = Part->CreateAndSetMaterialInstanceDynamic(i))
			{
				for (const FName& Parameter : TownConstants::ClothParameters)
				{
					// each piece keeps the colour it was made in, a little darker or lighter than on the next person who has the same
					if (Dyed->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Now))
					{
						Dyed->SetVectorParameterValue(Parameter, FLinearColor(Now.R * Shade, Now.G * Shade, Now.B * Shade, Now.A));
					}
				}
			}
		}
	}
}

AGutFighter* ATownGameMode::StandOwned(const FGutStableFighter& Fighter, const FTransform& Where)
{
	UClass* Kind = PersonClass.LoadSynchronous();

	AGutFighter* Body = Kind ? GetWorld()->SpawnActorDeferred<AGutFighter>(Kind, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;

	if (!Body)
	{
		return nullptr;
	}

	if (!Fighter.Body.IsNull())
	{
		Body->SetBodyClass(Fighter.Body);
	}

	UGameplayStatics::FinishSpawningActor(Body, Where);

	Body->EquipWeapon(nullptr);
	Body->SetCollared(Fighter.bOwned);
	Body->SetAtEase(true);
	Body->GetCharacterMovement()->DisableMovement();

	return Body;
}

void ATownGameMode::TakeInTow(AGutFighter* Body, int32 Fighter, int32 Offer, TArray<FVector> Way, float Wait)
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!Body || !GameInstance || !GameInstance->Stable.Fighters.IsValidIndex(Fighter))
	{
		return;
	}

	// somebody bought is led with their wrists tied. Somebody who signed walks with whoever pays them.
	Body->SetBound(GameInstance->Stable.Fighters[Fighter].bOwned);
	Body->GetCharacterMovement()->DisableMovement();

	// the player is never shut in by what they own
	Body->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	FTownTow& One = Tow.AddDefaulted_GetRef();

	One.Fighter = Fighter;
	One.Offer = Offer;
	One.Way = MoveTemp(Way);
	One.Wait = Wait;

	TowBodies.Add(Body);

	UE_LOG(LogTown, Log, TEXT("Street: %s is in tow, %d behind the player"), *GameInstance->Stable.Fighters[Fighter].FighterName.ToString(), Tow.Num());
}

bool ATownGameMode::IsLotGone(int32 Lot) const
{
	if (!PlaceData || Lot <= 0)
	{
		return false;
	}

	const int32 Who = PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; });

	// only once everybody has been stood: before that nobody is there yet
	return bSettled && People.IsValidIndex(Who) && !People[Who];
}

void ATownGameMode::SettleOwned()
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutFighter* Walker = GetWalker();

	if (!GameInstance || !Walker || !PlaceData || !GameInstance->Stable.bStarted)
	{
		return;
	}

	// whoever was bought off the Block today is not standing on it still
	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	for (int32 i = 0; i < Lots.Num(); ++i)
	{
		const int32 Lot = Lots[i];

		if (Lot <= 0 || (!GameInstance->Stable.BoughtToday.Contains(i) && !IsSoldAway(i)))
		{
			continue;
		}

		const int32 Who = PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; });

		if (People.IsValidIndex(Who) && People[Who])
		{
			People[Who]->Destroy();
			People[Who] = nullptr;

			UE_LOG(LogTown, Log, TEXT("Street: %s was bought today, and is not on the Block"), *PlaceData->People[Who].Name);
		}
	}

	// and whoever is in tow comes into the place behind the player
	const float HalfHeight = Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Back = -Walker->GetActorForwardVector();

	for (int32 i = 0; i < GameInstance->Stable.Fighters.Num(); ++i)
	{
		const FGutStableFighter& Fighter = GameInstance->Stable.Fighters[i];

		if (!Fighter.bInTow)
		{
			continue;
		}

		const FTransform Where(Walker->GetActorRotation(), Walker->GetActorLocation() + (Back * (TowGap * ((Tow.Num() / 2) + 1)))
			+ (Walker->GetActorRightVector() * (TowAside * ((Tow.Num() % 2) == 0 ? -1.0f : 1.0f))));

		if (AGutFighter* Body = StandOwned(Fighter, Where))
		{
			KeepOnGround(Body);
			TakeInTow(Body, i, INDEX_NONE);
		}
	}
}

void ATownGameMode::UpdateTow(float DeltaTime)
{
	AGutFighter* Walker = GetWalker();

	if (Tow.IsEmpty() || !Walker)
	{
		return;
	}

	const FVector WalkerFeet = Walker->GetActorLocation() - FVector(0.0f, 0.0f, Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// what is under somebody coming down off the Block is its deck, its ramp or the ground, and never anybody
	FCollisionQueryParams Looking(FName("TownTow"), true, Walker);

	for (const TArray<TObjectPtr<AGutFighter>>* Bodies : { &People, &TowBodies })
	{
		for (const AGutFighter* Body : *Bodies)
		{
			if (Body)
			{
				Looking.AddIgnoredActor(Body);
				Looking.AddIgnoredActor(Body->GetBodyActor());
			}
		}
	}

	for (int32 i = 0; i < Tow.Num(); ++i)
	{
		FTownTow& One = Tow[i];
		AGutFighter* Body = TowBodies.IsValidIndex(i) ? TowBodies[i].Get() : nullptr;

		if (!Body)
		{
			continue;
		}

		// The player's way is marked as it is walked, a little to their own side of it. Only where somebody can stand: a mark
		// to the side of a player who walks along a table is inside the table, and whoever followed walked at it for ever
		// (22 November: "my slave is stuck blocked by a table and keeps walking although she gets nowhere"). Where the side
		// is not free the mark is the player's own, which is.
		FVector Mark = WalkerFeet + (Walker->GetActorRightVector() * (TowAside * ((i % 2) == 0 ? -1.0f : 1.0f)));

		if (!GetKeptClear(FVector2D(Mark.X, Mark.Y)).Equals(FVector2D(Mark.X, Mark.Y), 1.0f))
		{
			Mark = WalkerFeet;
		}

		if (One.Trail.IsEmpty() || One.Own.IsEmpty() || FVector::Dist2D(One.Own.Last(), WalkerFeet) > TowCrumb)
		{
			One.Trail.Add(Mark);
			One.Own.Add(WalkerFeet);
		}

		if (One.Wait > 0.0f)
		{
			One.Wait -= DeltaTime;
			continue;
		}

		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FRotator Facing = Body->GetActorRotation();

		FVector Feet = Body->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
		FVector2D Flat(Feet.X, Feet.Y);

		const bool bComingDown = !One.Way.IsEmpty();

		// how far behind the player they are, by the way the player went
		float Behind = 0.0f;
		FVector2D From = Flat;

		for (const FVector& Crumb : One.Trail)
		{
			Behind += FVector2D::Distance(From, FVector2D(Crumb.X, Crumb.Y));
			From = FVector2D(Crumb.X, Crumb.Y);
		}

		Behind += FVector2D::Distance(From, FVector2D(WalkerFeet.X, WalkerFeet.Y));

		const float Gap = TowGap * ((i / 2) + 1);
		const bool bFarBehind = Behind > Gap + TowHurry;

		// they run when the player runs, and to catch up
		Body->SetJogging(!bComingDown && (Walker->IsJogging() || bFarBehind));
		Body->SetRunning(!bComingDown && Walker->IsRunning());

		const float OwnPace = Body->GetCharacterMovement()->MaxWalkSpeed;
		// Somebody who has dropped back steps out to make it up, as anybody led does: walking at the player's own pace and
		// no faster, whatever they lost at the start they never got back, and they trailed three metres behind, which is
		// where the camera is (the picture of 21 November was the back of their head).
		const float MakingUp = bComingDown ? 1.0f : FMath::GetMappedRangeValueClamped(FVector2f(Gap + 20.0f, Gap + 140.0f), FVector2f(1.0f, 1.3f), Behind);
		const float Pace = (OwnPace > 1.0f ? OwnPace : 150.0f) * FMath::Max(MakingUp, !bComingDown && bFarBehind ? 1.15f : 1.0f);

		TArray<FVector>& Path = bComingDown ? One.Way : One.Trail;

		// as far as a frame's going goes, and no nearer the player than their place behind
		float Left = bComingDown ? Pace * DeltaTime : FMath::Min(Pace * DeltaTime, FMath::Max(Behind - Gap, 0.0f));

		// Their place is measured along the way the player walked, and the player turns round: a player who walked back
		// was a full place away by the trail and in the same spot on the ground, one body through the other ("my slave
		// continually cuts me in all environments", 24 November). Within their place of the player in a straight line they
		// come no nearer, whatever the trail says, and the trail starts again from where the player is.
		const FVector2D WalkerFlat(WalkerFeet.X, WalkerFeet.Y);
		const float Apart = FVector2D::Distance(Flat, WalkerFlat);

		if (!bComingDown && Apart < Gap)
		{
			Left = 0.0f;

			One.Trail.Reset();
			One.Own.Reset();
			One.bOnOwnWay = false;
		}

		FVector2D Going = FVector2D::ZeroVector;
		bool bMoved = false;

		const FVector2D Before = Flat;
		const float Meant = Left;

		while (Left > UE_KINDA_SMALL_NUMBER && !Path.IsEmpty())
		{
			const FVector2D To(Path[0].X, Path[0].Y);
			const float Distance = FVector2D::Distance(Flat, To);

			if (Distance > UE_KINDA_SMALL_NUMBER)
			{
				Going = (To - Flat) / Distance;
			}

			const float Step = FMath::Min(Distance, Left);

			// the ground under the player's feet there is the ground under theirs
			if (!bComingDown && Distance > UE_KINDA_SMALL_NUMBER)
			{
				Feet.Z += (Path[0].Z - Feet.Z) * (Step / Distance);
			}

			Flat += Going * Step;
			Left -= Step;
			bMoved = bMoved || Step > UE_KINDA_SMALL_NUMBER;

			if (Distance <= Step + UE_KINDA_SMALL_NUMBER)
			{
				Path.RemoveAt(0);
			}
		}

		// And nearer than a body's room they give way: out of the player's path to their own side of it if the player is
		// walking, straight back if the player is standing. The same for whoever else is led.
		if (!bComingDown)
		{
			FVector2D Off = FVector2D::ZeroVector;

			if (Apart < TowApart)
			{
				FVector2D Away = Apart > 1.0f ? (Flat - WalkerFlat) / Apart : -FVector2D(Facing.Vector());

				const FVector2D Walking(Walker->GetVelocity().X, Walker->GetVelocity().Y);

				if (Walking.SizeSquared() > FMath::Square(20.0f))
				{
					const FVector2D Along = Walking.GetSafeNormal();

					FVector2D Aside(-Along.Y, Along.X);

					const float Side = FVector2D::DotProduct(Away, Aside);

					Aside *= FMath::Abs(Side) > 0.05f ? FMath::Sign(Side) : ((i % 2) == 0 ? -1.0f : 1.0f);

					Away = ((Aside * 0.85f) + (Away * 0.5f)).GetSafeNormal();
				}

				Off += Away * (TowApart - Apart);
			}

			for (int32 Other = 0; Other < i; ++Other)
			{
				if (const AGutFighter* Ahead = TowBodies.IsValidIndex(Other) ? TowBodies[Other].Get() : nullptr)
				{
					const FVector2D Theirs(Ahead->GetActorLocation().X, Ahead->GetActorLocation().Y);
					const float Between = FVector2D::Distance(Flat, Theirs);

					if (Between < TowApart * 0.85f)
					{
						Off += (Between > 1.0f ? (Flat - Theirs) / Between : FVector2D(Facing.Vector().Y, -Facing.Vector().X)) * ((TowApart * 0.85f) - Between);
					}
				}
			}

			if (const float Wanted = Off.Size(); Wanted > 0.5f)
			{
				const FVector2D Way = Off / Wanted;
				const float Step = FMath::Min(Wanted, FMath::Max(Pace, 170.0f) * 1.5f * DeltaTime);

				// only onto ground that can be stood on: against a wall they stay where they are, and the player goes round
				const FVector2D Stepped = GetKeptClear(Flat + (Way * Step));

				if (FVector2D::Distance(Stepped, Flat) > Step * 0.3f)
				{
					Going = (Stepped - Flat).GetSafeNormal();
					Flat = Stepped;
					bMoved = true;
				}
			}
		}

		if (bComingDown)
		{
			// on whatever is under them there, as a lot walked off the Block is: see UpdateLed
			float Highest = -TNumericLimits<float>::Max();

			for (float Along : { 0.0f, 14.0f, -14.0f })
			{
				const FVector2D At = Flat + (Going * Along);

				FHitResult Under;

				if (GetWorld()->LineTraceSingleByChannel(Under, FVector(At.X, At.Y, Feet.Z + 80.0f), FVector(At.X, At.Y, Feet.Z - 120.0f), ECC_Visibility, Looking))
				{
					Highest = FMath::Max(Highest, Under.Location.Z);
				}
			}

			if (Highest > -TNumericLimits<float>::Max())
			{
				Feet.Z = FMath::FInterpTo(Feet.Z, Highest, DeltaTime, 14.0f);
			}
		}

		// standing, they turn to whoever owns them; going, to where they are going
		FVector2D Toward = Going;

		if (!bMoved)
		{
			Toward = FVector2D(WalkerFeet.X, WalkerFeet.Y) - Flat;
		}

		FRotator Turned = Facing;

		if (!Toward.IsNearlyZero())
		{
			Turned = FMath::RInterpConstantTo(Facing, FRotator(0.0f, FVector(Toward.X, Toward.Y, 0.0f).Rotation().Yaw, 0.0f), DeltaTime, LotTurnSpeed);
		}

		Body->SetActorLocationAndRotation(FVector(Flat.X, Flat.Y, Feet.Z + HalfHeight), Turned, false, nullptr, ETeleportType::None);

		// and round whatever stands in the place, once they are down on its ground
		if (!bComingDown)
		{
			KeepOnGround(Body);

			// the marks that are behind them now are the player's own as well
			while (One.Own.Num() > One.Trail.Num())
			{
				One.Own.RemoveAt(0);
			}
		}

		// How far they really got. Their legs go at the pace their body does: somebody held up by a thing in the way stands
		// and does not walk on the spot.
		const float Got = FVector2D::Distance(Before, FVector2D(Body->GetActorLocation()));
		const float Speed = DeltaTime > UE_KINDA_SMALL_NUMBER ? FMath::Min(Got / DeltaTime, Pace) : 0.0f;

		Body->SetLedVelocity(bMoved && Speed > Pace * 0.2f ? FVector(Going.X, Going.Y, 0.0f) * Speed : FVector::ZeroVector);

		// Held up: for half a second they have got less than a third of the way they meant to go. They take the player's own
		// way from there, which the player got along. Held up even so for two seconds, they are put on it.
		const bool bHeld = !bComingDown && Meant > UE_KINDA_SMALL_NUMBER && Got < Meant * 0.33f;

		One.Held = bHeld ? One.Held + DeltaTime : 0.0f;

		if (One.Held > 0.5f && One.Own.Num() == One.Trail.Num() && !One.bOnOwnWay)
		{
			One.Trail = One.Own;
			One.bOnOwnWay = true;

			UE_LOG(LogTown, Log, TEXT("Street: %s is held up, and takes the player's own way"), *Body->GetDisplayName().ToString());
		}
		else if (One.Held > 2.0f && !One.Trail.IsEmpty())
		{
			const FVector Next = One.Trail[0];

			Body->SetActorLocation(FVector(Next.X, Next.Y, Next.Z + HalfHeight), false, nullptr, ETeleportType::None);

			One.Trail.RemoveAt(0);

			if (!One.Own.IsEmpty())
			{
				One.Own.RemoveAt(0);
			}

			One.Held = 0.0f;

			UE_LOG(LogTown, Log, TEXT("Street: %s is still held up, and is put on the player's way"), *Body->GetDisplayName().ToString());
		}
		else if (!bHeld && One.Trail.Num() <= 1)
		{
			One.bOnOwnWay = false;
		}
	}
}

void ATownGameMode::HandInTow()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return;
	}

	int32 Handed = 0;

	for (FGutStableFighter& Fighter : GameInstance->Stable.Fighters)
	{
		Handed += Fighter.bInTow ? 1 : 0;
		Fighter.bInTow = false;
	}

	if (Handed > 0)
	{
		GameInstance->SaveRun();

		UE_LOG(LogTown, Log, TEXT("Street: %d in tow handed in at the yard"), Handed);
	}
}

void ATownGameMode::FindFires()
{
	Fires.Reset();

	// the builder marks every light that is a fire: embers in a drum, a lantern's wick
	for (TActorIterator<ALight> It(GetWorld()); It; ++It)
	{
		ULightComponent* Light = It->GetLightComponent();

		if (!Light || !It->ActorHasTag(TownConstants::FireTag))
		{
			continue;
		}

		FTownFire& Fire = Fires.AddDefaulted_GetRef();

		Fire.Light = Light;
		Fire.Brightness = Light->Intensity;

		// a fire big enough to stand round is heard where it burns: the drums, not the lanterns
		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && Light->Intensity >= TownConstants::FireHeardFrom)
		{
			Sounds->Loop(FName("fire_drum"), It->GetActorLocation());
		}

		// where it stands is where in the wavering it begins: the same every time the place is come to, and no two alike
		Fire.Phase = FMath::Fmod(FMath::Abs((It->GetActorLocation().X * 0.0137f) + (It->GetActorLocation().Y * 0.0291f)), 97.0f);
	}
}

void ATownGameMode::UpdateFires()
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (const FTownFire& Fire : Fires)
	{
		if (ULightComponent* Light = Fire.Light.Get())
		{
			// a slow breathing with a quick flutter on it, never brighter than it was made and never out
			const float Slow = FMath::PerlinNoise1D((Now * TownConstants::FireBreath) + Fire.Phase);
			const float Quick = FMath::PerlinNoise1D((Now * TownConstants::FireFlutter) + (Fire.Phase * 3.1f));

			Light->SetIntensity(Fire.Brightness * FMath::Clamp(0.8f + (Slow * 0.22f) + (Quick * 0.12f), 0.5f, 1.0f));
		}
	}
}

void ATownGameMode::KeepOnGround(AGutFighter* Walker) const
{
	if (!PlaceData || PlaceData->Ground.IsEmpty())
	{
		return;
	}

	const FVector Here = Walker->GetActorLocation();
	const FVector2D Flat(Here.X, Here.Y);
	const FVector2D Nearest = GetKeptClear(Flat);

	if (!Nearest.Equals(Flat, 0.01f))
	{
		Walker->SetActorLocation(FVector(Nearest.X, Nearest.Y, Here.Z), false, nullptr, ETeleportType::None);
	}
}

FVector2D ATownGameMode::GetKeptClear(const FVector2D& Flat, float Radius) const
{
	if (!PlaceData || PlaceData->Ground.IsEmpty())
	{
		return Flat;
	}

	FVector2D Kept = Flat;

	// round whatever stands there: clear of each thing by the nearest way, and again for a corner where two of them meet
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FTownBlock& Block : PlaceData->Blocks)
		{
			Kept = Block.KeepClear(Kept, Radius);
		}
	}

	// and on the ground: off every patch of it, back to the nearest edge
	FVector2D Nearest = Kept;
	float NearestDistance = TNumericLimits<float>::Max();

	for (const FBox2D& Patch : PlaceData->Ground)
	{
		if (Patch.IsInside(Kept))
		{
			Nearest = Kept;
			break;
		}

		const FVector2D Closest = Patch.GetClosestPointTo(Kept);
		const float Distance = FVector2D::DistSquared(Closest, Kept);

		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Closest;
		}
	}

	return Nearest;
}

void ATownGameMode::FindReach(const AGutFighter* Walker)
{
	PersonInReach = INDEX_NONE;
	DoorInReach = INDEX_NONE;

	if (!PlaceData)
	{
		return;
	}

	const FVector Here = Walker->GetActorLocation();

	float Nearest = Reach;

	for (int32 i = 0; i < PlaceData->People.Num(); ++i)
	{
		const float Distance = FVector::Dist2D(Here, GetPersonPlace(i));

		// somebody with nothing to say is not offered to be talked to
		if (Distance < Nearest && People.IsValidIndex(i) && People[i] && !PlaceData->People[i].Lines.IsEmpty() && !IsLed(i))
		{
			Nearest = Distance;
			PersonInReach = i;
		}
	}

	FindInReach = INDEX_NONE;
	YardInReach = INDEX_NONE;

	// somebody in reach comes before a door in reach: the door will still be there
	if (PersonInReach != INDEX_NONE)
	{
		return;
	}

	// and so does one of the player's own, in their yard
	for (int32 i = 0; i < YardBodies.Num(); ++i)
	{
		if (const AGutFighter* Body = YardBodies[i])
		{
			const float Distance = FVector::Dist2D(Here, Body->GetActorLocation());

			if (Distance < Nearest)
			{
				Nearest = Distance;
				YardInReach = i;
			}
		}
	}

	if (YardInReach != INDEX_NONE)
	{
		return;
	}

	// and something lying at the walker's feet comes before a door too
	float NearestFind = TownConstants::FindReach;

	for (int32 i = 0; i < PlaceData->Finds.Num(); ++i)
	{
		if (!FindActors.IsValidIndex(i) || !FindActors[i])
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Here, FindActors[i]->GetActorLocation());

		if (Distance < NearestFind)
		{
			NearestFind = Distance;
			FindInReach = i;
		}
	}

	if (FindInReach != INDEX_NONE)
	{
		return;
	}

	for (int32 i = 0; i < PlaceData->Doors.Num(); ++i)
	{
		const float Distance = FVector::Dist2D(Here, PlaceData->Doors[i].Place);

		// an open way is walked through, not used
		if (Distance < Nearest && !PlaceData->Doors[i].bWalkedThrough)
		{
			Nearest = Distance;
			DoorInReach = i;
		}
	}
}

void ATownGameMode::Interact()
{
	if (!bSettled || bStoppedMenu || !PlaceData)
	{
		return;
	}

	if (bShopping)
	{
		// at a counter the same key takes what the player is on
		HandleMenuInput(EGutMenuInput::Confirm);
	}
	else if (IsTalking())
	{
		AdvanceTalk();
	}
	else if (PlaceData->People.IsValidIndex(PersonInReach))
	{
		BeginTalk(PersonInReach);
	}
	else if (YardBodies.IsValidIndex(YardInReach))
	{
		OpenYardPage(YardInReach);
	}
	else if (PlaceData->Finds.IsValidIndex(FindInReach))
	{
		TakeFind(FindInReach);
	}
	else if (PlaceData->Doors.IsValidIndex(DoorInReach))
	{
		const FTownDoor& Door = PlaceData->Doors[DoorInReach];

		if (Door.Deed == ETownDeed::None || !Holds(Door.When))
		{
			Notice = Door.Shut;
			NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::NoticeTime;
		}
		else
		{
			const FTownDoor Through = Door;

			if (const FString Came = Apply(Through.Does); !Came.IsEmpty())
			{
				ShowNotice(Came);
			}

			Do(Through.Deed, &Through);
		}
	}
}

void ATownGameMode::BeginTalk(int32 Person)
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person) || PlaceData->People[Person].Lines.IsEmpty())
	{
		return;
	}

	TalkingTo = Person;
	TalkLine = GetOpeningLine(Person);
	TalkChoice = 0;
	TalkHub = (GetShownAnswers(PlaceData->People[Person].Lines[TalkLine]).IsEmpty() || IsSwitch(PlaceData->People[Person].Lines[TalkLine])) ? INDEX_NONE : TalkLine;
	TalkAsked.Reset();
	TalkTold.Reset();
	TalkToldLine = INDEX_NONE;
	DonePerson = INDEX_NONE;
	DoneLine = INDEX_NONE;
	Dealt.Reset();
	DealtId.Reset();

	// whoever is talking stands still to do it, and looks at who they are talking to
	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	if (AGutFighter* Walker = GetWalker())
	{
		const FVector Toward = GetPersonPlace(Person) - Walker->GetActorLocation();

		if (!Toward.IsNearlyZero())
		{
			Walker->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));
		}
	}

	UE_LOG(LogTown, Log, TEXT("Street: talking to %s"), *PlaceData->People[Person].Name);

	SaidPerson = INDEX_NONE;
	SaidLine = INDEX_NONE;

	DoTalkLine();
	SayTalkLine();
}

void ATownGameMode::SayTalkLine()
{
	if (!IsTalking() || (TalkingTo == SaidPerson && TalkLine == SaidLine))
	{
		return;
	}

	SaidPerson = TalkingTo;
	SaidLine = TalkLine;

	// whoever was speaking stops when the next thing is said
	if (TalkVoice)
	{
		TalkVoice->FadeOut(0.15f, 0.0f);
		TalkVoice = nullptr;
	}

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	// said with the hands as well, whether or not speech is heard: a different way for each line, and the same for that
	// line every time. Whoever it is said to listens.
	static const bool bStill = FParse::Param(FCommandLine::Get(), TEXT("GutNoTalkGestures"));

	if (!bStill && People.IsValidIndex(TalkingTo) && People[TalkingTo])
	{
		auto Performed = [](const TCHAR* Name)
		{
			return Cast<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/Motion/%s.%s"), Name, Name)).TryLoad());
		};

		const int32 Way = (GetTypeHash(People[TalkingTo]->GetName()) + (TalkLine * 3)) % 6;

		People[TalkingTo]->Gesture(Performed(TownConstants::TalkGestures[Way]), false);

		if (AGutFighter* Walker = GetWalker(); Walker && !Walker->IsFidgeting())
		{
			Walker->Gesture(Performed(TownConstants::ListenGesture), false);
		}
	}

	if (People.IsValidIndex(TalkingTo) && People[TalkingTo])
	{
		People[TalkingTo]->Speak(nullptr);
	}

	if (GameInstance && GameInstance->HearsSpeech() && Lines.IsValidIndex(TalkLine) && Lines[TalkLine].Voice)
	{
		// the face says it too, where a performance of the line has been made
		if (People.IsValidIndex(TalkingTo) && People[TalkingTo])
		{
			if (UAnimSequenceBase* Face = GetFaceFor(Lines[TalkLine].Voice))
			{
				People[TalkingTo]->Speak(Face);

				UE_LOG(LogTown, Log, TEXT("Street: %s says it with their face too (%s)"), *PlaceData->People[TalkingTo].Name, *Face->GetName());
			}
		}

		// The voice comes from where its owner stands, through the place's own air, as every other sound in the place does:
		// laid straight into the ear it was a recording, heard perfectly whatever stood round it. Somebody with people close
		// about them speaks up over them. -GutVoicesInTheEar is the old way.
		static const bool bInTheEar = FParse::Param(FCommandLine::Get(), TEXT("GutVoicesInTheEar"));

		UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this);
		const AGutFighter* Speaker = People.IsValidIndex(TalkingTo) ? People[TalkingTo].Get() : nullptr;

		if (bInTheEar || !Sounds || !Sounds->GetAttenuation() || !Speaker)
		{
			TalkVoice = UGameplayStatics::SpawnSound2D(this, Lines[TalkLine].Voice, TownConstants::TalkVoiceVolume);
		}
		else
		{
			const FVector Mouth = Speaker->GetActorLocation() + FVector(0.0f, 0.0f, Speaker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.75f);

			int32 About = 0;

			for (TActorIterator<AGutCrowd> It(GetWorld()); It; ++It)
			{
				About += It->CountNear(Mouth, 700.0f);
			}

			for (const AGutFighter* Other : People)
			{
				About += (Other && Other != Speaker && FVector::Dist2D(Other->GetActorLocation(), Mouth) < 700.0f) ? 1 : 0;
			}

			const float Raised = TownConstants::TalkVoiceVolume * TownConstants::TalkVoiceAtArmsLength * (1.0f + (0.07f * FMath::Min(About, 6)));

			TalkVoice = UGameplayStatics::SpawnSoundAtLocation(this, Lines[TalkLine].Voice, Mouth, FRotator::ZeroRotator, Raised, 1.0f, 0.0f, Sounds->GetAttenuation(), nullptr, false);

			UE_LOG(LogTown, Log, TEXT("Street: %s speaks from where they stand, with %d about them, at %.2f"), *PlaceData->People[TalkingTo].Name, About, Raised);
		}
	}

	// the caller is not shouted over
	if (CallVoice)
	{
		CallVoice->SetVolumeMultiplier(TownConstants::CallVoiceUnderTalk);
	}
}

bool ATownGameMode::IsWordsUnwritten() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!IsTalking() || !GameInstance || GameInstance->ReadsSpeech())
	{
		return false;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	return Lines.IsValidIndex(TalkLine) && Lines[TalkLine].Voice != nullptr;
}

void ATownGameMode::EndTalk()
{
	// nobody goes on talking with their hands after the talk
	if (People.IsValidIndex(TalkingTo) && People[TalkingTo])
	{
		People[TalkingTo]->StopGesture();
		People[TalkingTo]->Speak(nullptr);
	}

	if (AGutFighter* Walker = GetWalker())
	{
		Walker->StopGesture();
	}

	TalkingTo = INDEX_NONE;
	TalkLine = 0;
	TalkChoice = 0;
	TalkHub = INDEX_NONE;
	TalkAsked.Reset();

	// what the last thing said came to stays up in the street for a while, where the talk ended on it
	if (!TalkTold.IsEmpty())
	{
		ShowNotice(TalkTold);
		NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::GivenTime;
	}

	TalkTold.Reset();
	TalkToldLine = INDEX_NONE;

	// whoever was speaking is left in the middle of it, and the caller is heard again
	if (TalkVoice)
	{
		TalkVoice->FadeOut(0.25f, 0.0f);
		TalkVoice = nullptr;
	}

	if (CallVoice)
	{
		CallVoice->SetVolumeMultiplier(TownConstants::CallVoiceVolume);
	}

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->ResetIgnoreMoveInput();
	}
}

const FTownLine* ATownGameMode::GetAnsweredLine() const
{
	if (!IsTalking())
	{
		return nullptr;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	if (!Lines.IsValidIndex(TalkLine))
	{
		return nullptr;
	}

	const FTownLine& Line = Lines[TalkLine];

	if (!GetShownAnswers(Line).IsEmpty())
	{
		return &Line;
	}

	// said in answer to something, and nothing follows it: everything that could be said before can be said still. Not to
	// somebody who is walking off, or whom the town has in the place no longer, or where what was asked has had its day
	// (work that has since been taken is not offered again in the same breath): what they said was the last of it.
	if (!Lines.IsValidIndex(Line.Next) && Lines.IsValidIndex(TalkHub) && TalkHub != TalkLine && !IsLed(TalkingTo) && IsPresent(TalkingTo) && Holds(Lines[TalkHub].When) && !GetShownAnswers(Lines[TalkHub]).IsEmpty())
	{
		return &Lines[TalkHub];
	}

	return nullptr;
}

void ATownGameMode::AdvanceTalk()
{
	if (!IsTalking())
	{
		return;
	}

	const TArray<FTownLine>& Lines = PlaceData->People[TalkingTo].Lines;

	if (!Lines.IsValidIndex(TalkLine))
	{
		EndTalk();
		return;
	}

	const FTownLine& Line = Lines[TalkLine];
	const FTownLine* Asked = GetAnsweredLine();

	int32 Next = Line.Next;

	const TArray<int32> Shown = Asked ? GetShownAnswers(*Asked) : TArray<int32>();
	const int32 AskedLine = Asked ? static_cast<int32>(Asked - Lines.GetData()) : INDEX_NONE;

	if (Asked && Shown.IsValidIndex(TalkChoice))
	{
		// copied: what it does may be the end of whoever was asked
		const FTownAnswer Answer = Asked->Answers[Shown[TalkChoice]];

		// what it takes is said, and nothing is done, where the player has not got it
		if (FString Lacking; !Holds(Answer.Needs, &Lacking))
		{
			FString Mark, Figure;

			TalkTold = (Lacking.Split(TEXT(">="), &Mark, &Figure) && Mark == TEXT("water")) ? FString::Printf(TEXT("That takes %s L. You hold %d."), *Figure, GetMark(FName("water")))
				: FString(TEXT("You have not got what that takes."));
			TalkToldLine = TalkLine;

			ShopRefusedAt = GetWorld()->GetTimeSeconds();
			return;
		}

		TalkTold = Apply(Answer.Does);
		TalkToldLine = INDEX_NONE;

		WriteLog(Answer.Log);

		if (!Answer.Does.IsEmpty() || !Answer.Log.IsEmpty())
		{
			if (UGutGameInstance* GameInstance = GetGutGameInstance())
			{
				GameInstance->SaveRun();
			}
		}

		if (Answer.Deed == ETownDeed::Borrow || Answer.Deed == ETownDeed::Repay)
		{
			// done where they stand, and the talk goes on with what it came to
			Do(Answer.Deed);
		}
		else if (Answer.Deed == ETownDeed::Armor || Answer.Deed == ETownDeed::Buy || Answer.Deed == ETownDeed::Trade)
		{
			// to their counter, their book or their stall: whoever was being talked to keeps it
			const int32 Keeper = TalkingTo;
			const ETownDeed To = Answer.Deed;

			EndTalk();

			if (To == ETownDeed::Buy)
			{
				OpenBook(Keeper);
			}
			else if (To == ETownDeed::Trade)
			{
				OpenStall(Keeper);
			}
			else
			{
				OpenShop(Keeper);
			}

			return;
		}
		else if (Answer.Deed != ETownDeed::None)
		{
			EndTalk();
			Do(Answer.Deed);
			return;
		}

		Next = Answer.Next;

		// what the lender says back is the line for how it came out, where she has one
		if (Answer.Deed == ETownDeed::Borrow || Answer.Deed == ETownDeed::Repay)
		{
			if (const int32 Came = Lines.IndexOfByPredicate([this](const FTownLine& Line) { return !DealtId.IsEmpty() && Line.Id == DealtId; }); Came != INDEX_NONE)
			{
				Next = Came;
			}
		}

		// an answer that does nothing and is answered with nothing is the player leaving: the only way a talk with answers ends
		if (!Lines.IsValidIndex(Next))
		{
			EndTalk();
			return;
		}

		TalkAsked.Add((AskedLine * 100) + Shown[TalkChoice]);
	}

	if (!Lines.IsValidIndex(Next))
	{
		// somebody with nothing to be asked has said all they had to say
		if (!Asked)
		{
			EndTalk();
		}

		return;
	}

	// the same thing asked again is said again
	if (Next == TalkLine)
	{
		SaidLine = INDEX_NONE;
	}

	TalkLine = Next;
	TalkToldLine = Next;

	// a line with answers of its own starts the choosing again; one without keeps the arrow where it was, under what was just asked
	if (IsSwitch(Lines[Next]))
	{
		// a line that only sends the talk one way or another: nothing said before it can be gone back to
		TalkChoice = 0;
		TalkHub = INDEX_NONE;
	}
	else if (!GetShownAnswers(Lines[Next]).IsEmpty())
	{
		if (Next != TalkHub)
		{
			TalkChoice = 0;
		}

		TalkHub = Next;
	}

	DoTalkLine();

	// what could be said may be fewer than it was
	if (const FTownLine* Now = GetAnsweredLine())
	{
		TalkChoice = FMath::Clamp(TalkChoice, 0, FMath::Max(0, GetShownAnswers(*Now).Num() - 1));
	}

	SayTalkLine();
}

void ATownGameMode::HandleBack()
{
	if (bMapUp)
	{
		ToggleMap();
	}
	else if (bStoppedMenu)
	{
		SetStopped(false);
	}
	else if (bShopping && bAtBag && bBagAsking)
	{
		// back from who a thing is for, to the bag
		bBagAsking = false;
	}
	else if (bShopping && Auction.bOn)
	{
		// backing out of a lot that is up lets it go, where it can be let go
		LetGo();
	}
	else if (bShopping)
	{
		CloseShop();
	}
	else if (IsTalking())
	{
		EndTalk();
	}
}

void ATownGameMode::ToggleMap()
{
	if (!bSettled || bStoppedMenu || IsTalking() || bShopping)
	{
		return;
	}

	bMapUp = !bMapUp;

	UE_LOG(LogTown, Log, TEXT("Street: the map is %s"), bMapUp ? TEXT("up") : TEXT("put away"));

	// whoever reads a map stands still to do it
	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bMapUp)
		{
			Hands->SetIgnoreMoveInput(true);
		}
		else
		{
			Hands->ResetIgnoreMoveInput();
		}
	}
}

bool ATownGameMode::GetMapSpot(FVector2D& OutSpot) const
{
	const AGutFighter* Walker = GetWalker();

	if (!PlaceData || !Walker)
	{
		return false;
	}

	// the level's X is the map's north and its Y the map's east, in centimetres from the same middle
	const FVector Here = Walker->GetActorLocation();
	const FVector2D OnMap((Here.Y / 100.0f) + TownConstants::MapOrigin.X, (Here.X / 100.0f) + TownConstants::MapOrigin.Y);

	for (const TownConstants::FMapPatch& Patch : TownConstants::MapPatches)
	{
		if (PlaceData->Name == Patch.Place)
		{
			const FVector2D Share((OnMap - Patch.Town.Min) / (Patch.Town.Max - Patch.Town.Min));

			OutSpot = Patch.Picture.Min + (FVector2D(FMath::Clamp(Share.X, -0.1, 1.1), FMath::Clamp(Share.Y, -0.1, 1.1)) * (Patch.Picture.Max - Patch.Picture.Min));

			return true;
		}
	}

	return false;
}

FString ATownGameMode::GetOwnedLine() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !GameInstance->Stable.bStarted || GameInstance->Stable.Fighters.IsEmpty())
	{
		return FString();
	}

	// whoever is with the player now comes first, and where they are to be taken; then whoever is waiting in the yard, by name
	TArray<FString> With;
	TArray<FString> Yard;

	for (const FGutStableFighter& Fighter : GameInstance->Stable.Fighters)
	{
		(Fighter.bInTow ? With : Yard).Add(Fighter.FighterName.ToString());
	}

	FString Line;

	if (!With.IsEmpty())
	{
		Line = FString::Printf(TEXT("%s %s with you. Your yard: Pool Street, the yard hand at the gate beside the Gut."), *FString::Join(With, TEXT(" and ")), With.Num() == 1 ? TEXT("is") : TEXT("are"));
	}

	if (!Yard.IsEmpty())
	{
		Line += FString::Printf(TEXT("%s%s %s in your yard beside the Gut. The yard hand on Pool Street lets you in, to see to your fighters or to walk out with one."),
			Line.IsEmpty() ? TEXT("") : TEXT(" "), *FString::Join(Yard, TEXT(", ")), Yard.Num() == 1 ? TEXT("is") : TEXT("are"));
	}

	return Line;
}

void ATownGameMode::HandleMenuInput(EGutMenuInput Input)
{
	if (bStoppedMenu)
	{
		if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			StoppedChoice = (StoppedChoice + (Input == EGutMenuInput::Up ? TownStoppedLines::Count - 1 : 1)) % TownStoppedLines::Count;
			return;
		}

		if (Input != EGutMenuInput::Confirm)
		{
			return;
		}

		UGutGameInstance* GameInstance = GetGutGameInstance();

		switch (StoppedChoice)
		{
		case TownStoppedLines::GoOn:
			SetStopped(false);
			break;

		case TownStoppedLines::Bag:

			// the game goes on with the bag open, unless something else is up that has to be finished first
			SetStopped(false);
			ToggleBag();
			break;

		case TownStoppedLines::Map:
			SetStopped(false);
			ToggleMap();
			break;

		case TownStoppedLines::Sound:

			// each press a third quieter, and round again, as in the cage
			if (GameInstance)
			{
				GameInstance->Loudness = (GameInstance->Loudness + 3) % 4;
				GameInstance->KeepLoudness();
				GameInstance->ApplyLoudness(this);
			}
			break;

		case TownStoppedLines::Speech:

			// heard and written, written only, heard only: round and round
			if (GameInstance)
			{
				GameInstance->Speech = (GameInstance->Speech + 1) % 3;
				GameInstance->KeepLoudness();
			}
			break;

		default:

			// nothing is lost: a run is written down in the cage, and nothing is kept of a walk in the street
			SetStopped(false);

			UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
			break;
		}

		return;
	}

	if (bShopping && bAtBag)
	{
		HandleBagInput(Input);
		return;
	}

	if (bShopping)
	{
		// the pieces, and under them the line that leaves
		const int32 Pieces = GetShopLines().Num();

		if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			ShopChoice = (ShopChoice + (Input == EGutMenuInput::Up ? Pieces : 1)) % (Pieces + 1);

			// whoever the line is about is looked at as the player comes to it
			if (bAtBook)
			{
				ShowBookLine();
			}
		}
		else if (Input == EGutMenuInput::Confirm)
		{
			if (ShopChoice >= Pieces)
			{
				// with a lot up, the last line lets it go: the book stays open
				if (Auction.bOn)
				{
					LetGo();
				}
				else
				{
					CloseShop();
				}
			}
			else
			{
				BuyAtShop(ShopChoice);
			}
		}

		return;
	}

	if (!IsTalking())
	{
		return;
	}

	const FTownLine* Asked = GetAnsweredLine();
	const int32 Answers = (Asked && !IsSwitch(*Asked)) ? GetShownAnswers(*Asked).Num() : 0;

	if (Input == EGutMenuInput::Confirm)
	{
		AdvanceTalk();
	}
	else if (Answers > 1 && (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down))
	{
		TalkChoice = (TalkChoice + (Input == EGutMenuInput::Up ? Answers - 1 : 1)) % Answers;
	}
}

void ATownGameMode::Do(ETownDeed Deed, const FTownDoor* Door)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || Deed == ETownDeed::None)
	{
		return;
	}

	if (Deed == ETownDeed::Borrow || Deed == ETownDeed::Repay)
	{
		Lend(Deed == ETownDeed::Borrow);
		return;
	}

	if (Deed == ETownDeed::Armor)
	{
		OpenShop(INDEX_NONE);
		return;
	}

	// a stall is somebody's: with nobody keeping it there is nothing to open
	if (Deed == ETownDeed::Trade)
	{
		return;
	}

	// people are bought where they are sold, at the clerk's table: nobody is taken round to the cage for it
	if (Deed == ETownDeed::Buy)
	{
		OpenBook(INDEX_NONE);
		return;
	}

	// The day sat out in the yard: everybody eats, whoever has a tag is paid, and it is tomorrow
	if (Deed == ETownDeed::Rest)
	{
		if (const AGutGameMode* Rules = GetCageRules(); Rules && GameInstance->Stable.bStarted)
		{
			const TArray<FGutStableFighter> Before = GameInstance->Stable.Fighters;
			const float DebtBefore = GameInstance->Stable.Debt;

			{
				FTownLentRules Lent(Rules, GameInstance);

				Lent.Rules->RestDay();
			}

			const FString Paid = SettleAfterDay(Before, DebtBefore);

			GameInstance->SaveRun();

			ShowNotice(FString::Printf(TEXT("The day goes by. It is day %d. %s"), GameInstance->Stable.Day, *Paid));

			NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::GivenTime;

			UE_LOG(LogTown, Log, TEXT("Yard: %s"), *Notice);
		}
		else
		{
			ShowNotice(TEXT("You have nobody to feed and nothing to wait for. The Block sells people; the square is west along Pool Street."));
		}

		return;
	}

	// An owner goes round to their yard, which is a place of its own to walk about in, where the project has one. From
	// there the pit boss's hatch is the way into the cage.
	if (Deed == ETownDeed::Own && !IsYard() && FPackageName::DoesPackageExist(TownConstants::YardLevel))
	{
		static const bool bCardYard = FParse::Param(FCommandLine::Get(), TEXT("GutYardCard"));

		if (!bCardYard)
		{
			GameInstance->TownArrive = FName("from_pool_street");
			GameInstance->TownStood.Reset();

			UE_LOG(LogTown, Log, TEXT("Street: round to the yard"));

			UGameplayStatics::OpenLevel(this, FName(TownConstants::YardLevel), true);
			return;
		}
	}

	if (Deed == ETownDeed::Go)
	{
		if (!Door || Door->To.IsNone() || !FPackageName::DoesPackageExist(Door->To.ToString()))
		{
			UE_LOG(LogTown, Warning, TEXT("Street: a way that leads nowhere (%s)"), Door ? *Door->To.ToString() : TEXT("no door"));
			return;
		}

		// on foot to the next place, arriving by the way that leads from here
		GameInstance->TownArrive = Door->Arrive;
		GameInstance->TownStood.Reset();

		UE_LOG(LogTown, Log, TEXT("Street: on to %s by %s"), *Door->To.ToString(), *Door->Arrive.ToString());

		UGameplayStatics::OpenLevel(this, Door->To, true);
		return;
	}

	// whoever is in tow goes round to the yard, whichever of the Gut's ways the player goes in by: the yard hand sees to it
	HandInTow();

	// nobody with no fighters has business at the pit boss's hatch
	if (Deed == ETownDeed::Pit && GameInstance->Stable.Fighters.IsEmpty())
	{
		ShowNotice(TEXT("The hatch stays shut. The pit boss makes fights for people who have somebody to put in them."));
		return;
	}

	// the cage's way out leads back to where the player is standing now
	if (const AGutFighter* Walker = GetWalker())
	{
		GameInstance->TownStood = FTransform(Walker->GetActorRotation(), Walker->GetActorLocation());
	}

	// the cage is opened as whatever the player is going into it as, and remembers that they came from the street
	GameInstance->bAtDoor = false;
	GameInstance->bCameByDoor = true;
	GameInstance->bSandbox = Deed == ETownDeed::Spar;
	GameInstance->bStable = Deed == ETownDeed::Own || Deed == ETownDeed::Pit;
	GameInstance->bStableAtBlock = false;
	GameInstance->bStableAtBill = Deed == ETownDeed::Pit;
	GameInstance->bStableFromYard = Deed == ETownDeed::Pit;
	GameInstance->bWindow = Deed == ETownDeed::Bet;

	static const TCHAR* Ways[] = { TEXT("nowhere"), TEXT("fight"), TEXT("fight, from the start"), TEXT("spar"), TEXT("bet"), TEXT("own"), TEXT("another place") };

	UE_LOG(LogTown, Log, TEXT("Street: in to %s"), Ways[FMath::Min<int32>(static_cast<uint8>(Deed), UE_ARRAY_COUNT(Ways) - 1)]);

	UGameplayStatics::OpenLevel(this, CageLevel, true, Deed == ETownDeed::FightAfresh ? TEXT("NewRun=1") : TEXT(""));
}

void ATownGameMode::Lend(bool bBorrow)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return;
	}

	// her book is the window's: what a gambler holds is what she lends against, and what the window pays out is what she is paid from
	FGutWindowState& Page = GameInstance->Window;

	if (!Page.bStarted)
	{
		Dealt = TEXT("I have no page for you. Put water on a fight at Chalk's window first, and then I will know your face.");
		DealtId = TEXT("nopage");
	}
	else if (bBorrow)
	{
		if (Page.Debt + LoanOwed > LoanLimit)
		{
			Dealt = FString::Printf(TEXT("No. %.0f litres is as much as I let anybody owe me, and you know where debts are paid here."), LoanLimit);
			DealtId = TEXT("limit");
		}
		else
		{
			Page.Water += LoanGiven;
			Page.Debt += LoanOwed;

			Dealt = FString::Printf(TEXT("%.0f litres counted out. %.0f written down."), LoanGiven, LoanOwed);
			DealtId = TEXT("lent");

			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(FName("cash"), FVector::ZeroVector);
			}
		}
	}
	else
	{
		const float Paid = FMath::Min(Page.Water, Page.Debt);

		if (Page.Debt <= 0.0f)
		{
			Dealt = TEXT("Your page is clean. I take nothing I am not owed.");
			DealtId = TEXT("clean");
		}
		else if (Paid <= 0.0f)
		{
			Dealt = TEXT("With what? You are holding nothing.");
			DealtId = TEXT("empty");
		}
		else
		{
			Page.Water -= Paid;
			Page.Debt -= Paid;

			Dealt = FString::Printf(TEXT("%.0f litres, struck off."), Paid);
			DealtId = TEXT("paid");

			if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
			{
				Sounds->Play(FName("cash"), FVector::ZeroVector);
			}
		}
	}

	UE_LOG(LogTown, Log, TEXT("Street: the lender: %s Holding %.0f L, owing %.0f L"), *Dealt, Page.Water, Page.Debt);

	GameInstance->SaveRun();
}

FString ATownGameMode::Told(const FString& Words) const
{
	if (!Words.Contains(TEXT("{")))
	{
		return Words;
	}

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const FGutWindowState Page = GameInstance ? GameInstance->Window : FGutWindowState();

	FString Result = Words;

	Result.ReplaceInline(TEXT("{held}"), *FString::Printf(TEXT("%.0f"), Page.Water));
	Result.ReplaceInline(TEXT("{owed}"), *FString::Printf(TEXT("%.0f"), Page.Debt));
	Result.ReplaceInline(TEXT("{dealt}"), *Dealt);

	return Result.TrimStartAndEnd();
}

void ATownGameMode::UpdateCalls(float DeltaTime)
{
	// while a lot is up for the player the caller is calling that, and nothing else
	if (!PlaceData || PlaceData->Calls.IsEmpty() || Auction.bOn)
	{
		return;
	}

	CallTime += DeltaTime;

	// a call that is spoken stays up until it has been said, and a moment more
	if (!PlaceData->Calls.IsValidIndex(CallIndex) || CallTime >= FMath::Max(PlaceData->Calls[CallIndex].Seconds, CallSpoken))
	{
		// the next of them that is not about a lot the player has taken away: nobody is put up who is not there
		for (int32 Tried = 0; Tried < PlaceData->Calls.Num(); ++Tried)
		{
			CallIndex = (CallIndex + 1) % PlaceData->Calls.Num();

			if (!IsLotGone(PlaceData->Calls[CallIndex].About))
			{
				break;
			}
		}

		CallTime = 0.0f;

		BeginCall(PlaceData->Calls[CallIndex]);
	}
}

void ATownGameMode::SkipToCall(int32 Index)
{
	if (PlaceData && PlaceData->Calls.IsValidIndex(Index))
	{
		CallIndex = (Index + PlaceData->Calls.Num() - 1) % PlaceData->Calls.Num();
		CallTime = PlaceData->Calls[CallIndex].Seconds;
	}
}

UAnimSequenceBase* ATownGameMode::GetFaceFor(const USoundBase* Recording) const
{
	if (!Recording)
	{
		return nullptr;
	}

	// beside the recording, in Faces, as <recording>_face (Tools/make_town_faces.sh makes them)
	const FString Folder = FPackageName::GetLongPackagePath(Recording->GetPackage()->GetName());
	const FString Name = Recording->GetName() + TEXT("_face");

	if (!FPackageName::DoesPackageExist(FString::Printf(TEXT("%s/Faces/%s"), *Folder, *Name)))
	{
		return nullptr;
	}

	return Cast<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("%s/Faces/%s.%s"), *Folder, *Name, *Name)).TryLoad());
}

void ATownGameMode::BeginCall(const FTownCall& Call)
{
	// said out over the place, if it has been recorded and the player has not chosen to read only
	CallSpoken = 0.0f;

	if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && GameInstance->HearsSpeech() && Call.Voice)
	{
		if (CallVoice)
		{
			CallVoice->FadeOut(0.2f, 0.0f);
		}

		CallVoice = UGameplayStatics::SpawnSound2D(this, Call.Voice, IsTalking() ? TownConstants::CallVoiceUnderTalk : TownConstants::CallVoiceVolume);

		const USoundWave* Wave = Cast<USoundWave>(Call.Voice);

		CallSpoken = (Wave ? Wave->Duration : Call.Voice->GetDuration()) + 0.4f;

		// whoever calls it is seen to: their face says it, for anybody standing near enough to see
		for (int32 Person = 0; Person < PlaceData->People.Num() && Person < People.Num(); ++Person)
		{
			if (People[Person] && PlaceData->People[Person].Name == Call.Speaker && Person != TalkingTo)
			{
				People[Person]->Speak(GetFaceFor(Call.Voice));
				break;
			}
		}
	}

	// a lot knocked down gets something out of whoever is standing there
	if (Call.bStirsTheCrowd)
	{
		for (TActorIterator<AGutCrowd> It(GetWorld()); It; ++It)
		{
			It->Cheer(0.12f, 1.2f);
		}
	}

	// and is walked off, down the way it came up
	if (Call.Sells > 0)
	{
		SellLot(Call.Sells);
	}

	if (Call.bBringsUp)
	{
		BringLotsUp();
	}
}

FVector ATownGameMode::GetKeptPlace(int32 Back) const
{
	const TArray<FVector>& Way = PlaceData->LotWay;

	// the file runs back from where the way begins, away from where it goes
	return Way[0] + ((Way[0] - Way[1]).GetSafeNormal2D() * (LotFileGap * Back));
}

void ATownGameMode::SellLot(int32 Lot)
{
	if (!PlaceData || PlaceData->LotWay.Num() < 2)
	{
		return;
	}

	const int32 Who = PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; });

	if (!People.IsValidIndex(Who) || !People[Who] || KeptLots.Contains(Who))
	{
		return;
	}

	const FTownPerson& Person = PlaceData->People[Who];
	const TArray<FVector>& Way = PlaceData->LotWay;

	FTownLed Walk;
	Walk.Person = Who;
	Walk.Wait = LotSoldWait;

	// back off its mark, along behind the others to the head of the deck, down the way and to the end of the file
	Walk.Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));

	for (int32 i = Way.Num() - 1; i >= 0; --i)
	{
		Walk.Way.Add(Way[i]);
	}

	// the first to be sold goes to the far end of the file and the last stops at its head, so nobody walks through anybody
	const int32 Lots = PlaceData->People.FilterByPredicate([](const FTownPerson& Other) { return Other.Lot > 0; }).Num();
	const int32 Back = Lots - 1 - KeptLots.Num();

	if (Back > 0)
	{
		Walk.Way.Add(GetKeptPlace(Back));
	}

	// and waits looking the way it will come back
	Walk.Facing = (Way[1] - Way[0]).Rotation().Yaw;

	Led.RemoveAll([Who](const FTownLed& One) { return One.Person == Who; });
	Led.Add(MoveTemp(Walk));
	KeptLots.Add(Who);

	UE_LOG(LogTown, Log, TEXT("Sale: %s is knocked down and walked off, %d kept"), *Person.Name, KeptLots.Num());
}

void ATownGameMode::BringLotsUp()
{
	if (!PlaceData || PlaceData->LotWay.Num() < 2)
	{
		return;
	}

	const TArray<FVector>& Way = PlaceData->LotWay;

	// the head of the file first, which is whoever was walked off last
	for (int32 Order = 0; Order < KeptLots.Num(); ++Order)
	{
		const int32 Who = KeptLots[KeptLots.Num() - 1 - Order];

		if (!People.IsValidIndex(Who) || !People[Who])
		{
			continue;
		}

		const FTownPerson& Person = PlaceData->People[Who];
		const FVector Here = People[Who]->GetActorLocation();

		// from wherever on the way it has got to: somebody still going down it turns round where they are
		int32 Nearest = 0;

		for (int32 i = 1; i < Way.Num(); ++i)
		{
			if (FVector::DistSquared2D(Way[i], Here) < FVector::DistSquared2D(Way[Nearest], Here))
			{
				Nearest = i;
			}
		}

		FTownLed Walk;
		Walk.Person = Who;
		Walk.Wait = LotFileTime * Order;
		Walk.Facing = Person.Facing;

		for (int32 i = Nearest; i < Way.Num(); ++i)
		{
			Walk.Way.Add(Way[i]);
		}

		// along behind the marks to its own, and onto it
		Walk.Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));
		Walk.Way.Add(Person.Place);

		Led.RemoveAll([Who](const FTownLed& One) { return One.Person == Who; });
		Led.Add(MoveTemp(Walk));
	}

	UE_LOG(LogTown, Log, TEXT("Sale: %d lots are brought up"), KeptLots.Num());

	KeptLots.Reset();
}

void ATownGameMode::UpdateLed(float DeltaTime)
{
	if (Led.IsEmpty())
	{
		return;
	}

	// what is under each of them is the deck, the ramp or the ground, and never one of the others
	FCollisionQueryParams Looking(FName("TownLed"), true, GetWalker());

	for (const AGutFighter* Body : People)
	{
		if (Body)
		{
			Looking.AddIgnoredActor(Body);
			Looking.AddIgnoredActor(Body->GetBodyActor());
		}
	}

	for (int32 i = Led.Num() - 1; i >= 0; --i)
	{
		FTownLed& One = Led[i];
		AGutFighter* Body = People.IsValidIndex(One.Person) ? People[One.Person].Get() : nullptr;

		if (!Body)
		{
			Led.RemoveAt(i);
			continue;
		}

		if (One.Wait > 0.0f)
		{
			One.Wait -= DeltaTime;
			continue;
		}

		const FRotator Facing = Body->GetActorRotation();

		// there: it turns to face the way it is to stand, and is done
		if (One.Way.IsEmpty())
		{
			Body->SetLedVelocity(FVector::ZeroVector);
			Body->SetActorRotation(FMath::RInterpConstantTo(Facing, FRotator(0.0f, One.Facing, 0.0f), DeltaTime, LotTurnSpeed));

			if (One.bGoes)
			{
				// walked off out of the place: gone
				const int32 Gone = One.Person;

				Led.RemoveAt(i);
				Beats.RemoveAll([Gone](const FTownBeat& Round) { return Round.Person == Gone; });

				Body->Destroy();
				People[Gone] = nullptr;
			}
			else if (FMath::Abs(FRotator::NormalizeAxis(Body->GetActorRotation().Yaw - One.Facing)) < 1.0f)
			{
				Led.RemoveAt(i);
			}

			continue;
		}

		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float OwnPace = Body->GetCharacterMovement()->MaxWalkSpeed;
		const float Pace = (OwnPace > 1.0f ? OwnPace : 150.0f) * LotPace;

		FVector Feet = Body->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
		FVector2D Flat(Feet.X, Feet.Y);
		FVector2D Going = FVector2D::ZeroVector;

		// as far as a frame's walking goes, through as many of the way's places as that passes
		float Left = Pace * DeltaTime;

		while (Left > 0.0f && !One.Way.IsEmpty())
		{
			const FVector2D To(One.Way[0].X, One.Way[0].Y);
			const float Distance = FVector2D::Distance(Flat, To);

			if (Distance > UE_KINDA_SMALL_NUMBER)
			{
				Going = (To - Flat) / Distance;
			}

			if (Distance <= Left)
			{
				Left -= Distance;
				Flat = To;
				One.Way.RemoveAt(0);
			}
			else
			{
				Flat += Going * Left;
				Left = 0.0f;
			}
		}

		// on whatever is under them there: the deck, the ramp as it falls, the ground. A foot is longer than the gap between
		// two of a ramp's boards, so it is the highest of what is under the middle of them, a little ahead and a little behind.
		float Highest = -TNumericLimits<float>::Max();

		for (float Along : { 0.0f, 14.0f, -14.0f })
		{
			const FVector2D At = Flat + (Going * Along);

			FHitResult Under;

			if (GetWorld()->LineTraceSingleByChannel(Under, FVector(At.X, At.Y, Feet.Z + 80.0f), FVector(At.X, At.Y, Feet.Z - 120.0f), ECC_Visibility, Looking))
			{
				Highest = FMath::Max(Highest, Under.Location.Z);
			}
		}

		if (Highest > -TNumericLimits<float>::Max())
		{
			Feet.Z = FMath::FInterpTo(Feet.Z, Highest, DeltaTime, 14.0f);
		}

		FRotator Turned = Facing;

		if (!Going.IsNearlyZero())
		{
			Turned = FMath::RInterpConstantTo(Facing, FRotator(0.0f, FVector(Going.X, Going.Y, 0.0f).Rotation().Yaw, 0.0f), DeltaTime, LotTurnSpeed);
		}

		Body->SetActorLocationAndRotation(FVector(Flat.X, Flat.Y, Feet.Z + HalfHeight), Turned, false, nullptr, ETeleportType::None);
		Body->SetLedVelocity(FVector(Going.X, Going.Y, 0.0f) * Pace);
	}
}

FVector ATownGameMode::GetPersonPlace(int32 Person) const
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Person))
	{
		return FVector::ZeroVector;
	}

	// somebody who walks a round is wherever it has taken them
	if (!PlaceData->People[Person].Beat.IsEmpty() && People.IsValidIndex(Person) && People[Person])
	{
		return People[Person]->GetActorLocation();
	}

	// and so is somebody who has ways to be walked along: they may have been walked somewhere and be standing there
	if (!PlaceData->People[Person].Ways.IsEmpty() && People.IsValidIndex(Person) && People[Person])
	{
		const FVector Stood = People[Person]->GetActorLocation();

		return FVector(Stood.X, Stood.Y, PlaceData->People[Person].Place.Z);
	}

	return PlaceData->People[Person].Place;
}

void ATownGameMode::UpdateBeats(float DeltaTime)
{
	if (Beats.IsEmpty() || !PlaceData)
	{
		return;
	}

	const AGutFighter* Walker = GetWalker();

	for (FTownBeat& One : Beats)
	{
		AGutFighter* Body = People.IsValidIndex(One.Person) ? People[One.Person].Get() : nullptr;

		if (!Body || !PlaceData->People.IsValidIndex(One.Person) || !PlaceData->People[One.Person].Beat.IsValidIndex(One.Stop))
		{
			continue;
		}

		const FTownPerson& Person = PlaceData->People[One.Person];
		const int32 Stops = Person.Beat.Num();

		const FRotator Facing = Body->GetActorRotation();
		const FVector Here = Body->GetActorLocation();

		const auto TurnTo = [this, Body, &Facing, DeltaTime](float Yaw)
		{
			Body->SetActorRotation(FMath::RInterpConstantTo(Facing, FRotator(0.0f, Yaw, 0.0f), DeltaTime, LotTurnSpeed));
		};

		// spoken to, or kept at their counter: they stand and look at whoever it is
		if (Walker && (TalkingTo == One.Person || (bShopping && ShopKeeper == One.Person)))
		{
			Body->SetLedVelocity(FVector::ZeroVector);

			if (const FVector Toward = Walker->GetActorLocation() - Here; !Toward.IsNearlyZero())
			{
				TurnTo(Toward.Rotation().Yaw);
			}

			continue;
		}

		// standing at a stop, looking at what they stopped for
		if (One.Wait > 0.0f)
		{
			One.Wait -= DeltaTime;

			Body->SetLedVelocity(FVector::ZeroVector);

			if (const float Look = Person.Beat[(One.Stop + Stops - 1) % Stops].Facing; Look >= 0.0f)
			{
				TurnTo(Look);
			}

			continue;
		}

		const FTownBeatStop& Stop = Person.Beat[One.Stop];

		FVector2D Flat(Here.X, Here.Y);

		const FVector2D To(Stop.Place.X, Stop.Place.Y);
		const float Distance = FVector2D::Distance(Flat, To);
		const FVector2D Going = Distance > UE_KINDA_SMALL_NUMBER ? (To - Flat) / Distance : FVector2D::ZeroVector;

		// somebody in their way: they stand until the way is clear, as people do
		if (Walker)
		{
			const FVector2D Off(Walker->GetActorLocation().X - Here.X, Walker->GetActorLocation().Y - Here.Y);

			if (Off.Size() < BeatRoom && FVector2D::DotProduct(Off.GetSafeNormal(), Going) > 0.3f)
			{
				Body->SetLedVelocity(FVector::ZeroVector);
				continue;
			}
		}

		const float OwnPace = Body->GetCharacterMovement()->MaxWalkSpeed;
		const float Pace = (OwnPace > 1.0f ? OwnPace : 150.0f) * Person.Pace;
		const float Step = Pace * DeltaTime;

		if (Distance <= Step)
		{
			// there: they stand as long as the stop says, and then go on to the next
			Flat = To;

			One.Wait = Stop.Wait;
			One.Stop = (One.Stop + 1) % Stops;
		}
		else
		{
			Flat += Going * Step;
		}

		// on the ground, which the place knows the height of: what can be felt under anybody is only there where it is drawn
		const float HalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		FRotator Turned = Facing;

		if (!Going.IsNearlyZero())
		{
			Turned = FMath::RInterpConstantTo(Facing, FRotator(0.0f, FVector(Going.X, Going.Y, 0.0f).Rotation().Yaw, 0.0f), DeltaTime, LotTurnSpeed);
		}

		Body->SetActorLocationAndRotation(FVector(Flat.X, Flat.Y, PlaceData->GetGroundZ(Flat.X) + HalfHeight), Turned, false, nullptr, ETeleportType::None);
		Body->SetLedVelocity(FVector(Going.X, Going.Y, 0.0f) * Pace);
	}
}

const UGutLadderData* ATownGameMode::GetFighterRank() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Rank = LadderData;

	// each rank is reached through the one before it
	for (int32 i = 0; Rank && GameInstance && GameInstance->bLadderStarted && i < GameInstance->Ladder.Rank && Rank->NextLadder; ++i)
	{
		Rank = Rank->NextLadder;
	}

	return Rank;
}

const UGutLadderData* ATownGameMode::GetShelfRank() const
{
	// what is on the counter is what the fighter's rank is sold, or, if that is nothing, what the first rank above it is
	const UGutLadderData* Rank = GetFighterRank();

	while (Rank && Rank->ArmorShop.IsEmpty())
	{
		Rank = Rank->NextLadder;
	}

	return Rank;
}

bool ATownGameMode::HasFighter() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	return GameInstance && GameInstance->bLadderStarted;
}

float ATownGameMode::GetShopWater() const
{
	return HasFighter() ? GetGutGameInstance()->Ladder.Water : 0.0f;
}

FString ATownGameMode::GetShopKeeper() const
{
	return (PlaceData && PlaceData->People.IsValidIndex(ShopKeeper)) ? PlaceData->People[ShopKeeper].Name : FString();
}

TArray<FTownShopLine> ATownGameMode::GetShopLines() const
{
	if (bAtBook)
	{
		return GetBookLines();
	}

	// the bag has its own screen, and its own lines
	if (bAtBag)
	{
		return TArray<FTownShopLine>();
	}

	if (bAtStall)
	{
		return GetStallLines();
	}

	if (bAtYardPage)
	{
		TArray<FTownShopLine> Lines;
		TArray<int32> Codes;
		GetYardPage(Lines, Codes);

		return Lines;
	}

	TArray<FTownShopLine> Lines;

	const UGutLadderData* Shelf = GetShelfRank();
	const UGutLadderData* Own = GetFighterRank();

	if (!Shelf)
	{
		return Lines;
	}

	const bool bFighter = HasFighter();
	const float Held = GetShopWater();

	static const TCHAR* Parts[] = { TEXT("body"), TEXT("arms"), TEXT("legs") };

	for (const UGutArmorData* Piece : Shelf->ArmorShop)
	{
		if (!Piece)
		{
			continue;
		}

		FTownShopLine Line;

		Line.Name = Piece->DisplayName.ToString();
		Line.Price = Piece->Price;

		// what it takes off a blow that lands on the part it covers
		Line.Detail = FString::Printf(TEXT("%s:  %.0f%% off a club, %.0f%% off a blade"), Parts[FMath::Clamp<int32>(static_cast<int32>(Piece->Slot), 0, 2)],
			(1.0f - Piece->BluntScale) * 100.0f, (1.0f - Piece->BladedScale) * 100.0f);

		Line.bWorn = bFighter && GetGutGameInstance()->Ladder.Armor.Contains(Piece->GetFName());

		if (!bFighter)
		{
			Line.Why = TEXT("no fighter");
		}
		else if (Own != Shelf)
		{
			Line.Why = FString::Printf(TEXT("%s rank"), *Shelf->RankName.ToString());
		}
		else if (Line.bWorn)
		{
			Line.Why = TEXT("worn");
		}
		else if (Held < Piece->Price)
		{
			Line.Why = FString::Printf(TEXT("%.0f L short"), Piece->Price - Held);
		}
		else
		{
			Line.bCanBuy = true;
		}

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

void ATownGameMode::OpenShop(int32 Keeper)
{
	const UGutLadderData* Shelf = GetShelfRank();
	const UGutLadderData* Own = GetFighterRank();

	ShopKeeper = Keeper;
	ShopChoice = 0;
	bShopping = true;
	bAtBook = false;
	bAtStall = false;
	bAtBag = false;

	// why nothing on it can be bought, if nothing can: said once, at the top
	if (!Shelf)
	{
		ShopSaid = TEXT("There is nothing on the counter.");
	}
	else if (!HasFighter())
	{
		ShopSaid = TEXT("This is ring armor, cut for the Gut's fighters, and you are not one yet: go in at the fighters' door first.");
	}
	else if (Own != Shelf)
	{
		ShopSaid = FString::Printf(TEXT("Armor is worn in the Gut from %s rank up, and sold to nobody below it. You are fighting at %s rank."), *Shelf->RankName.ToString(), Own ? *Own->RankName.ToString() : TEXT("no"));
	}
	else
	{
		ShopSaid.Reset();
	}

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogTown, Log, TEXT("Street: at %s's counter. %s"), *GetShopKeeper(), *ShopSaid);
}

void ATownGameMode::CloseShop()
{
	if (!bShopping)
	{
		return;
	}

	if (bAtBook)
	{
		// a lot that is up is finished by the room: nothing is left half sold
		while (Auction.bOn)
		{
			Auction.Wait = 0.0f;
			UpdateAuction(0.0f);
		}

		EndBookShow();
	}

	bShopping = false;
	bAtBook = false;
	bAtStall = false;
	bAtBag = false;
	bAtYardPage = false;
	bBagAsking = false;
	ShopKeeper = INDEX_NONE;
	ShopSaid.Reset();

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->ResetIgnoreMoveInput();
	}
}

bool ATownGameMode::BuyAtShop(int32 Line)
{
	if (bAtBook)
	{
		return BuyAtBook(Line);
	}

	if (bAtBag)
	{
		return UseFromBag(Line);
	}

	if (bAtStall)
	{
		return BuyAtStall(Line);
	}

	if (bAtYardPage)
	{
		return DoYardLine(Line);
	}

	UGutGameInstance* GameInstance = GetGutGameInstance();
	const UGutLadderData* Shelf = GetShelfRank();

	const TArray<FTownShopLine> Lines = GetShopLines();

	if (!GameInstance || !Shelf || !Lines.IsValidIndex(Line) || !Shelf->ArmorShop.IsValidIndex(Line) || !Shelf->ArmorShop[Line])
	{
		return false;
	}

	const UGutArmorData* Piece = Shelf->ArmorShop[Line];

	if (!Lines[Line].bCanBuy)
	{
		// worn already, or short: said. Anything else was said when the counter was come to, and still stands.
		if (Lines[Line].bWorn && Lines[Line].Why == TEXT("worn"))
		{
			ShopSaid = FString::Printf(TEXT("You have the %s on already."), *Lines[Line].Name.ToLower());
		}
		else if (Lines[Line].Why.EndsWith(TEXT("short")))
		{
			ShopSaid = FString::Printf(TEXT("The %s is %.0f L and you hold %.0f. Nothing here goes on a slate."), *Lines[Line].Name.ToLower(), Piece->Price, GetShopWater());
		}
		else
		{
			// Asked for all the same: it is refused out loud, and for this piece. With nothing said and nothing heard the key
			// seemed dead (22 November: "cannot buy anything - enter button non-responsive").
			const UGutLadderData* Own = GetFighterRank();

			ShopSaid = !HasFighter()
				? FString::Printf(TEXT("%s leaves the %s where it hangs. It is cut for the Gut's fighters, and you are not one yet: go in at the fighters' door first."), *GetShopKeeper(), *Lines[Line].Name.ToLower())
				: FString::Printf(TEXT("%s leaves the %s where it hangs. Nobody below %s rank is sold armor, and you are fighting at %s rank. Win your way up and it is yours for %.0f L."),
					*GetShopKeeper(), *Lines[Line].Name.ToLower(), *Shelf->RankName.ToString(), Own ? *Own->RankName.ToString() : TEXT("no"), Piece->Price);
		}

		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			Sounds->Play(FName("refused"), FVector::ZeroVector, 0.6f);
		}

		return false;
	}

	// what it goes on in place of, to say so
	FString Off;

	for (const UGutArmorData* Other : Shelf->ArmorShop)
	{
		if (Other && Other != Piece && Other->Slot == Piece->Slot && GameInstance->Ladder.Armor.Contains(Other->GetFName()))
		{
			Off = Other->DisplayName.ToString().ToLower();
		}
	}

	// with water in hand: the armorers keep no slate
	if (!GameInstance->BuyArmor(Shelf, Line, 0.0f))
	{
		return false;
	}

	// the fight the fighter is waiting for is come to as it now is, wearing it and without the water
	GameInstance->LadderAtRungStart.Armor = GameInstance->Ladder.Armor;
	GameInstance->LadderAtRungStart.Water -= Piece->Price;
	GameInstance->LadderAtRungStart.ArmorSpend += Piece->Price;
	GameInstance->SaveRun();

	ShopSaid = Off.IsEmpty()
		? FString::Printf(TEXT("Bought: the %s, %.0f L. You will have it on in your next fight."), *Lines[Line].Name.ToLower(), Piece->Price)
		: FString::Printf(TEXT("Bought: the %s, %.0f L, in place of the %s."), *Lines[Line].Name.ToLower(), Piece->Price, *Off);

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

	return true;
}

const AGutGameMode* ATownGameMode::GetCageRules() const
{
	const UClass* Rules = CageRules.LoadSynchronous();

	return Rules ? Rules->GetDefaultObject<AGutGameMode>() : nullptr;
}

FGutStableState ATownGameMode::GetOwnerState() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (GameInstance && GameInstance->Stable.bStarted)
	{
		return GameInstance->Stable;
	}

	// nobody's owner yet: what they would come to the Block with on the day they first bought
	FGutStableState State;

	if (const AGutGameMode* Rules = GetCageRules())
	{
		Rules->BeginStableState(State, BookSeed);
	}

	return State;
}

void ATownGameMode::GetBookOffers(TArray<FGutStableFighter>& OutOffers, TArray<int32>& OutLots) const
{
	OutOffers.Reset();
	OutLots.Reset();

	const AGutGameMode* Rules = GetCageRules();

	if (!Rules || !PlaceData)
	{
		return;
	}

	const FGutStableState State = GetOwnerState();

	Rules->GetBlockOffersFor(State, OutOffers);

	// the lots standing in the place, by their numbers
	TArray<int32> Standing;

	for (int32 i = 0; i < PlaceData->People.Num(); ++i)
	{
		if (PlaceData->People[i].Lot > 0)
		{
			Standing.Add(i);
		}
	}

	Standing.Sort([this](int32 A, int32 B) { return PlaceData->People[A].Lot < PlaceData->People[B].Lot; });

	// which of them the book's people are: the same on the same day. Whoever is bought is the body that was seen on the Block.
	FRandomStream Draw(State.Seed + (State.Day * 7919));

	for (FGutStableFighter& Offer : OutOffers)
	{
		int32 Lot = 0;

		if (Offer.bOwned && !Standing.IsEmpty())
		{
			const int32 Picked = Draw.RandRange(0, Standing.Num() - 1);
			const FTownPerson& Person = PlaceData->People[Standing[Picked]];

			Lot = Person.Lot;

			if (!Person.Body.IsNull())
			{
				Offer.Body = Person.Body;
			}

			Standing.RemoveAt(Picked);
		}

		OutLots.Add(Lot);
	}
}

TArray<FTownShopLine> ATownGameMode::GetBookLines() const
{
	TArray<FTownShopLine> Lines;

	const AGutGameMode* Rules = GetCageRules();

	if (!Rules)
	{
		return Lines;
	}

	// while a lot is up, the lines are what can be bid
	if (Auction.bOn)
	{
		return GetAuctionLines();
	}

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	const FGutStableState State = GetOwnerState();

	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		const FGutStableFighter& Offer = Offers[i];

		FTownShopLine Line;

		Line.Name = Lots[i] > 0
			? FString::Printf(TEXT("Lot %d  \u00B7  %s"), Lots[i], *Offer.FighterName.ToString().ToUpper())
			: FString::Printf(TEXT("Signs  \u00B7  %s"), *Offer.FighterName.ToString().ToUpper());

		Line.Detail = FString::Printf(TEXT("strength %d, speed %d, nerve %d"), Offer.Strength, Offer.Speed, Offer.Nerve);

		// what they are led out holding, and how they came to be here
		Line.About = (Offer.Weapon ? FString::Printf(TEXT("Fights with the %s. "), *Offer.Weapon->DisplayName.ToString().ToLower()) : FString(TEXT("Fights with bare hands. "))) + Offer.Billing.ToString();
		Line.Price = Offer.Price;

		if (!Offer.bOwned)
		{
			// somebody with a tag: not bought, signed, and paid by the day
			Line.PriceText = FString::Printf(TEXT("%.0f L a day"), Offer.Wage);
			Line.About += TEXT(" Not for sale: signs with you for a wage and a share of each purse.");
		}

		// the cage's own rules say whether they can be had, tried on a copy
		FGutStableState Tried = State;

		if (bBids && Offer.bOwned)
		{
			// a lot is bid for: the price is where the caller opens, and it can be put up by whoever holds that much
			FGutStableFighter Opened = Offer;

			Opened.Price = GetOpeningBid(Offer.Price);

			Line.PriceText = FString::Printf(TEXT("opens at %.0f L"), Opened.Price);
			Line.About += FString::Printf(TEXT(" The book has %s at %.0f L: what %s goes for is up to the room."), UGutSoundSubsystem::WhoIs(this, Offer.Body).Him(), Offer.Price, UGutSoundSubsystem::WhoIs(this, Offer.Body).He());
			Line.bCanBuy = Rules->TakeBlockOffer(Tried, Opened, i, &Line.Why);

			if (FString To; IsSoldAway(i, &To))
			{
				Line.bCanBuy = false;
				Line.PriceText = TEXT("sold");
				Line.Why = FString::Printf(TEXT("went to %s"), *To);
			}
		}
		else
		{
			Line.bCanBuy = Rules->TakeBlockOffer(Tried, Offer, i, &Line.Why);
		}

		if (Line.Why == TEXT("taken"))
		{
			Line.Why = TEXT("yours");
			Line.bWorn = true;
		}

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

void ATownGameMode::OpenBook(int32 Keeper)
{
	ShopKeeper = Keeper;
	ShopChoice = 0;
	bShopping = true;
	bAtBook = true;
	bAtStall = false;
	bAtBag = false;

	// whoever has no stable yet has the same people read out to them however often they ask, until they buy
	if (BookSeed == 0)
	{
		BookSeed = FMath::RandRange(1, 1000000);
	}

	if (!GetCageRules())
	{
		ShopSaid = TEXT("The book is shut.");
	}
	else if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && GameInstance->Stable.bStarted)
	{
		ShopSaid = TEXT("These are not spoken for yet. Whoever you buy comes away with you: your yard is beside the Gut, on Pool Street.");
	}
	else
	{
		ShopSaid = TEXT("These are not spoken for yet. Buy one and you are an owner: whoever you buy comes away with you, to your yard beside the Gut, and fights from there.");
	}

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogTown, Log, TEXT("Street: at the Block's book, %d on it"), GetBookLines().Num());

	// whoever is in the book and is nobody's lot is stood beside the table while it is open: somebody bought or signed is
	// somebody seen first
	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	const int32 Signer = Lots.IndexOfByKey(0);
	UClass* Kind = PersonClass.LoadSynchronous();

	// unless they signed with the player today, and are behind them already
	if (Offers.IsValidIndex(Signer) && Kind && PlaceData->People.IsValidIndex(Keeper) && !GetOwnerState().BoughtToday.Contains(Signer))
	{
		const FTownPerson& Clerk = PlaceData->People[Keeper];

		// in front of the clerk and to one side, out of the way of whoever is at the table
		const FVector Ahead = FRotator(0.0f, Clerk.Facing, 0.0f).Vector();
		const FVector Feet = Clerk.Place + (Ahead * 170.0f) + (FVector::CrossProduct(FVector::UpVector, Ahead) * 130.0f);
		const float Facing = Clerk.Facing;

		const AGutFighter* Walker = GetWalker();
		const float HalfHeight = Walker ? Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
		const FTransform Where(FRotator(0.0f, Facing, 0.0f), Feet + FVector(0.0f, 0.0f, HalfHeight));

		if (AGutFighter* Body = GetWorld()->SpawnActorDeferred<AGutFighter>(Kind, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			if (!Offers[Signer].Body.IsNull())
			{
				Body->SetBodyClass(Offers[Signer].Body);
			}

			UGameplayStatics::FinishSpawningActor(Body, Where);

			Body->EquipWeapon(nullptr);
			Body->SetCollared(false);
			Body->SetAtEase(true);
			Body->GetCharacterMovement()->DisableMovement();

			BookSigner = Body;
		}
	}

	BookShown = INDEX_NONE;

	ShowBookLine();
}

void ATownGameMode::ShowBookLine()
{
	// while a lot is up it is the one looked at, whichever bid the player is on
	const int32 Turned = Auction.bOn ? Auction.Line : ShopChoice;

	APlayerController* Eyes = UGameplayStatics::GetPlayerController(this, 0);

	if (!bAtBook || !Eyes || !PlaceData)
	{
		return;
	}

	// turned to them when the player comes to their line, and kept on them after: a lot may be walked off the Block, or
	// brought back up it, while the book is open
	const bool bTurnedTo = Turned != BookShown;

	BookShown = Turned;

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	// who the line is about: the lot of that number where it stands, or the one stood by the table
	const AGutFighter* Shown = nullptr;

	if (Lots.IsValidIndex(Turned))
	{
		if (Lots[Turned] > 0)
		{
			const int32 Lot = Lots[Turned];
			const int32 Who = PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; });

			Shown = People.IsValidIndex(Who) ? People[Who].Get() : nullptr;
		}
		else
		{
			Shown = BookSigner;
		}

		// bought a moment ago: still looked at, wherever they have got to
		if (const int32 Towed = Tow.IndexOfByPredicate([Turned](const FTownTow& One) { return One.Offer == Turned; }); !Shown && TowBodies.IsValidIndex(Towed))
		{
			Shown = TowBodies[Towed];
		}
	}

	// Their picture is put on the card (ATownHUD::DrawBook), and the player goes on looking through their own eyes. The whole
	// view went to them before, behind the card and to either side of it. -GutBookCamera is that way.
	static const bool bWholeView = FParse::Param(FCommandLine::Get(), TEXT("GutBookCamera"));

	bBookPictured = false;

	if (!bWholeView)
	{
		// The picture is of them and of nobody and nothing else: a body of their own, stood out of sight under the place,
		// alone before the eye, from the front, whole, under the eye's own lamps, standing as they stand. It was taken of
		// them where they were in the square, from the player's side: the player stood in it (a hip filled the frame),
		// somebody just bought and standing in the dark by the table was a shape, and nobody could be seen who was not on
		// the Block just then (24 November: "just show a still of the slave... use a clip of her idle... a clear view").
		const bool bSomebody = Offers.IsValidIndex(Turned);
		const FString Sitting = bSomebody ? Offers[Turned].FighterName.ToString() + Offers[Turned].Body.GetAssetName() : FString();

		if (BookSitter && BookSitting != Sitting)
		{
			BookSitter->Destroy();
			BookSitter = nullptr;
		}

		if (!bSomebody)
		{
			if (BookEye)
			{
				BookEye->GetCaptureComponent2D()->bCaptureEveryFrame = false;
			}

			for (ULightComponent* Lamp : BookLamps)
			{
				if (Lamp)
				{
					Lamp->SetVisibility(false);
				}
			}

			return;
		}

		if (!BookPicture)
		{
			BookPicture = NewObject<UTextureRenderTarget2D>(this);
			BookPicture->RenderTargetFormat = RTF_RGBA8_SRGB;
			BookPicture->InitAutoFormat(TownConstants::BookPictureWide, TownConstants::BookPictureHigh);
			BookPicture->UpdateResourceImmediate(true);
		}

		if (!BookEye)
		{
			BookEye = GetWorld()->SpawnActor<ASceneCapture2D>();

			if (BookEye)
			{
				USceneCaptureComponent2D* Lens = BookEye->GetCaptureComponent2D();

				Lens->TextureTarget = BookPicture;
				Lens->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
				Lens->FOVAngle = TownConstants::BookEyeFieldOfView;
				Lens->bCaptureOnMovement = false;
				Lens->bAlwaysPersistRenderingState = true;
				Lens->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

				// nothing of the place's air between the eye and them
				Lens->ShowFlags.SetFog(false);
				Lens->ShowFlags.SetVolumetricFog(false);
				Lens->ShowFlags.SetAtmosphere(false);

				// The eye's own lamps: one high and to its left that shapes them, one low and to its right that fills the
				// shadow the first leaves, one behind them that picks their edge out of the dark. Where the eye is there is
				// nothing else for them to fall on, and nobody in the place can see them burn.
				struct FLamp { FVector At; float Candelas; FLinearColor Color; bool bShadows; };

				const FLamp Lamps[] =
				{
					{ FVector(-40.0f, -170.0f, 120.0f), TownConstants::BookLampCandelas, FLinearColor(1.0f, 0.86f, 0.68f), true },
					{ FVector(-20.0f, 190.0f, -30.0f), TownConstants::BookLampCandelas * 0.5f, FLinearColor(0.8f, 0.86f, 1.0f), false },
					{ FVector(TownConstants::BookEyeDistance + 190.0f, 120.0f, 150.0f), TownConstants::BookLampCandelas * 0.8f, FLinearColor(1.0f, 0.9f, 0.78f), false },
				};

				for (const FLamp& One : Lamps)
				{
					if (USpotLightComponent* Lamp = NewObject<USpotLightComponent>(BookEye))
					{
						Lamp->SetMobility(EComponentMobility::Movable);
						Lamp->SetIntensityUnits(ELightUnits::Candelas);
						Lamp->SetIntensity(One.Candelas);
						Lamp->SetLightColor(One.Color);
						Lamp->SetInnerConeAngle(24.0f);
						Lamp->SetOuterConeAngle(38.0f);
						Lamp->SetAttenuationRadius(1100.0f);
						Lamp->SetCastShadows(One.bShadows);
						Lamp->AttachToComponent(BookEye->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
						Lamp->RegisterComponent();

						// each is turned on their middle, which is straight ahead of the eye
						Lamp->SetRelativeLocation(One.At);
						Lamp->SetRelativeRotation((FVector(TownConstants::BookEyeDistance, 0.0f, 0.0f) - One.At).Rotation());

						BookLamps.Add(Lamp);
					}
				}
			}
		}

		if (!BookEye)
		{
			return;
		}

		if (!BookSitter)
		{
			const AGutFighter* Walker = GetWalker();
			const FVector Under = (Walker ? Walker->GetActorLocation() : FVector::ZeroVector) - FVector(0.0f, 0.0f, TownConstants::BookStudioBelow);

			BookSitter = StandOwned(Offers[Turned], FTransform(FRotator::ZeroRotator, Under));
			BookSitting = Sitting;
			BookSatAt = GetWorld()->GetTimeSeconds();

			if (BookSitter)
			{
				BookSitter->SetActorEnableCollision(false);
			}
		}

		if (!BookSitter)
		{
			return;
		}

		USceneCaptureComponent2D* Lens = BookEye->GetCaptureComponent2D();

		// them, what they are seen as, and whatever is fastened to either
		TArray<AActor*> Theirs;

		Theirs.Add(BookSitter);

		if (AActor* Seen = BookSitter->GetBodyActor())
		{
			Theirs.AddUnique(Seen);
		}

		for (int32 i = 0; i < Theirs.Num(); ++i)
		{
			TArray<AActor*> Fastened;

			Theirs[i]->GetAttachedActors(Fastened, true, true);

			for (AActor* One : Fastened)
			{
				Theirs.AddUnique(One);
			}
		}

		Lens->ShowOnlyActors.Reset();

		for (AActor* One : Theirs)
		{
			Lens->ShowOnlyActors.Add(One);
		}

		// from in front of them and a little to one side, level with their middle, far enough off to have all of them
		const FVector Soles = BookSitter->GetActorLocation() - FVector(0.0f, 0.0f, BookSitter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Toward = FRotator(0.0f, BookSitter->GetActorRotation().Yaw + TownConstants::BookEyeAside, 0.0f).Vector();

		const FVector Eye = Soles + (Toward * TownConstants::BookEyeDistance) + FVector(0.0f, 0.0f, TownConstants::BookEyeHeight + 10.0f);
		const FVector Middle = Soles + FVector(0.0f, 0.0f, TownConstants::BookEyeHeight);

		BookEye->SetActorLocationAndRotation(Eye, (Middle - Eye).Rotation());

		for (ULightComponent* Lamp : BookLamps)
		{
			if (Lamp)
			{
				Lamp->SetVisibility(true);
			}
		}

		// not until they have settled into how they stand: a body is a moment in coming to it
		const bool bSettledIn = GetWorld()->GetTimeSeconds() - BookSatAt > TownConstants::BookSitterSettles;

		Lens->bCaptureEveryFrame = bSettledIn;

		bBookPictured = bSettledIn;

		return;
	}

	// the line that leaves, or somebody who is not there to be seen: the player's own eyes
	if (!Shown)
	{
		if (AGutFighter* Walker = GetWalker(); Walker && bTurnedTo)
		{
			Eyes->SetViewTargetWithBlend(Walker, TownConstants::BookCameraBlend);
		}

		return;
	}

	if (!BookCamera)
	{
		BookCamera = GetWorld()->SpawnActor<ACameraActor>();

		if (BookCamera)
		{
			BookCamera->GetCameraComponent()->SetFieldOfView(TownConstants::BookCameraFieldOfView);
			BookCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
	}

	if (!BookCamera)
	{
		return;
	}

	// from in front of them and a little to one side, level with their chest
	const FVector Feet = Shown->GetActorLocation() - FVector(0.0f, 0.0f, Shown->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float Aside = Shown == BookSigner ? 0.0f : 18.0f;
	const FVector From = Feet + (FRotator(0.0f, Shown->GetActorRotation().Yaw + Aside, 0.0f).Vector() * TownConstants::BookCameraDistance) + FVector(0.0f, 0.0f, TownConstants::BookCameraHeight);
	const FVector At = Feet + FVector(0.0f, 0.0f, TownConstants::BookCameraHeight);

	BookCamera->SetActorLocationAndRotation(From, (At - From).Rotation());

	if (bTurnedTo)
	{
		Eyes->SetViewTargetWithBlend(BookCamera, TownConstants::BookCameraBlend);
	}
}

void ATownGameMode::EndBookShow()
{
	if (APlayerController* Eyes = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (AGutFighter* Walker = GetWalker())
		{
			Eyes->SetViewTargetWithBlend(Walker, TownConstants::BookCameraBlend);
		}
	}

	if (BookSigner)
	{
		BookSigner->Destroy();
		BookSigner = nullptr;
	}

	if (BookSitter)
	{
		BookSitter->Destroy();
		BookSitter = nullptr;
	}

	if (BookEye)
	{
		BookEye->GetCaptureComponent2D()->bCaptureEveryFrame = false;
	}

	for (ULightComponent* Lamp : BookLamps)
	{
		if (Lamp)
		{
			Lamp->SetVisibility(false);
		}
	}

	bBookPictured = false;
	BookShown = INDEX_NONE;
}

bool ATownGameMode::GetBookPerson(int32 Line, FGutStableFighter& OutPerson, int32& OutLot) const
{
	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	// while a lot is up it is the one looked at, whichever bid the player is on
	if (Auction.bOn)
	{
		Line = Auction.Line;
	}

	if (!Offers.IsValidIndex(Line) || !Lots.IsValidIndex(Line))
	{
		return false;
	}

	OutPerson = Offers[Line];
	OutLot = Lots[Line];

	return true;
}

int32 ATownGameMode::GetAgeOf(const FGutStableFighter& Person)
{
	// Nobody in the town has a paper that says when they were born. This is how old they look to whoever is selling them:
	// by the body they are seen as, where that body is plainly not young, and otherwise dealt by their name, the same
	// every time. The bodies named here were looked at; the rest are taken to be between nineteen and thirty-six.
	static const TMap<FString, int32> Older =
	{
		{ TEXT("Sookja"), 58 }, { TEXT("Vivian"), 52 }, { TEXT("Orlando"), 54 }, { TEXT("Omari"), 57 }, { TEXT("Bruce"), 44 }, { TEXT("Bo"), 46 },
	};

	const FString Body = Person.Body.GetAssetName();

	for (const TPair<FString, int32>& One : Older)
	{
		if (Body.Contains(One.Key))
		{
			return One.Value;
		}
	}

	return 19 + static_cast<int32>(GetTypeHash(Person.FighterName.ToString() + Body) % 18u);
}

bool ATownGameMode::BuyAtBook(int32 Line)
{
	// while a lot is up, what is chosen is a bid
	if (Auction.bOn)
	{
		return PlaceBid(Line);
	}

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	// a lot is put up and bid for; somebody with a tag signs for a wage, as before
	if (bBids && Offers.IsValidIndex(Line) && Offers[Line].bOwned)
	{
		return BeginAuction(Line);
	}

	return TakeAtBook(Line);
}

bool ATownGameMode::TakeAtBook(int32 Line, float Price)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutGameMode* Rules = GetCageRules();

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	if (!GameInstance || !Rules || !Offers.IsValidIndex(Line))
	{
		return false;
	}

	// for what was bid, where it was bid for
	FGutStableFighter Offer = Offers[Line];

	if (Price >= 0.0f)
	{
		Offer.Price = Price;
	}

	const FString Who = Offer.FighterName.ToString();

	// tried on a copy first, so that somebody who cannot buy is not made an owner by asking
	FGutStableState Stable = GetOwnerState();
	FString Why;

	if (!Rules->TakeBlockOffer(Stable, Offer, Line, &Why))
	{
		if (Why == TEXT("taken"))
		{
			ShopSaid = FString::Printf(TEXT("%s is yours already. I have your name against %s."), *Who, UGutSoundSubsystem::WhoIs(this, Offer.Body).Hers());
		}
		else if (Why == TEXT("yard full"))
		{
			ShopSaid = FString::Printf(TEXT("Your yard holds %d and it is full. Sell one, or bury one, and come back."), Rules->GetStableMostFighters());
		}
		else
		{
			// and how it is come by: he asked, 21 November
			ShopSaid = FString::Printf(TEXT("%s is %.0f L and you hold %.0f%s. The Block gives no credit. Water is made in the cage: %s"), *Who, Offer.Price, Stable.Water,
				GameInstance->IsOnePurse() ? TEXT("") : TEXT(" of an owner's water"),
				GameInstance->IsOnePurse() ? TEXT("fight in it yourself, bet at Chalk's window, or send one of yours in from your yard. All three are on Pool Street.")
					: (Stable.Fighters.IsEmpty() ? TEXT("sign somebody for a wage, the last on my book, and send your fighter in at your yard gate on Pool Street.")
						: TEXT("go in at your yard gate on Pool Street and take a fight from the pit boss for one of yours. The purse comes home to the yard.")));
		}

		return false;
	}

	// the first one bought is what makes an owner: their story is told when they first come to their yard
	if (!GameInstance->Stable.bStarted)
	{
		GameInstance->PendingStory = EGutStory::Owner;
	}

	// whoever is bought or signed goes with the buyer, and is in tow until the yard
	const int32 Fighter = Stable.Fighters.Num() - 1;

	if (Stable.Fighters.IsValidIndex(Fighter))
	{
		Stable.Fighters[Fighter].bInTow = true;

		// the number they stood on is on their bill of sale
		Stable.Fighters[Fighter].Lot = Lots.IsValidIndex(Line) ? Lots[Line] : 0;
	}

	GameInstance->Stable = Stable;
	GameInstance->SaveRun();

	const int32 Lot = Lots[Line];
	const int32 Stood = Lot > 0 ? PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; }) : INDEX_NONE;

	if (People.IsValidIndex(Stood) && People[Stood])
	{
		// a lot: no longer one of the sale's, and down off the Block by the way the lots go, from wherever on it they are
		AGutFighter* Body = People[Stood];

		People[Stood] = nullptr;
		KeptLots.Remove(Stood);
		Led.RemoveAll([Stood](const FTownLed& One) { return One.Person == Stood; });

		const FTownPerson& Person = PlaceData->People[Stood];
		const TArray<FVector>& Down = PlaceData->LotWay;
		const FVector Here = Body->GetActorLocation();

		TArray<FVector> Way;
		int32 Nearest = Down.Num() - 1;

		const bool bOnMark = FVector::Dist2D(Here, Person.Place) < 40.0f;

		if (bOnMark)
		{
			// still on its mark: back off it first, along behind the others
			Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));
		}
		else
		{
			for (int32 i = 0; i < Down.Num(); ++i)
			{
				if (FVector::DistSquared2D(Down[i], Here) < FVector::DistSquared2D(Down[Nearest], Here))
				{
					Nearest = i;
				}
			}
		}

		if (!PlaceData->LotDown.IsEmpty())
		{
			// the place has a way down to the buyers: up to the head of the deck first, if they are not on it
			for (int32 i = bOnMark ? Down.Num() : Nearest; i < Down.Num(); ++i)
			{
				Way.Add(Down[i]);
			}

			Way.Append(PlaceData->LotDown);
		}
		else
		{
			for (int32 i = Nearest; i >= 0; --i)
			{
				Way.Add(Down[i]);
			}
		}

		TakeInTow(Body, Fighter, Line, MoveTemp(Way), LotSoldWait);
	}
	else if (BookSigner && Lot <= 0)
	{
		// the one stood by the table, who walks with whoever pays them
		AGutFighter* Body = BookSigner;

		BookSigner = nullptr;

		TakeInTow(Body, Fighter, Line);
	}
	else if (const AGutFighter* Walker = GetWalker(); Walker && Stable.Fighters.IsValidIndex(Fighter))
	{
		// nobody to be seen was standing for them: they are brought to the buyer
		const FTransform Where(Walker->GetActorRotation(), Walker->GetActorLocation() - (Walker->GetActorForwardVector() * (TowGap * (Tow.Num() + 1))));

		if (AGutFighter* Body = StandOwned(Stable.Fighters[Fighter], Where))
		{
			KeepOnGround(Body);
			TakeInTow(Body, Fighter, Line);
		}
	}

	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Offer.Body);

	ShopSaid = Offer.bOwned
		? FString::Printf(TEXT("Sold: %s, %.0f L. Your name is against %s, and %s comes with you now. Your yard is beside the Gut, on Pool Street: the yard hand at the gate lets owners in."), *Who, Offer.Price, Sex.Hers(), Sex.He())
		: FString::Printf(TEXT("Signed: %s, for %.0f L a day and a share. %s walks with you now. Your yard is beside the Gut, on Pool Street."), *Who, Offer.Wage, Sex.HeCap());

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

	return true;
}

float* ATownGameMode::GetMarketWater(FString* Whose) const
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	const auto Is = [Whose](const TCHAR* Words)
	{
		if (Whose)
		{
			*Whose = Words;
		}
	};

	// one purse: it is all the same water, and a fighter's figure is the one that can stand below nothing
	if (GameInstance && GameInstance->IsOnePurse())
	{
		Is(TEXT("your water"));
		return &GameInstance->Ladder.Water;
	}

	if (GameInstance && GameInstance->bLadderStarted)
	{
		Is(TEXT("your fighter's"));
		return &GameInstance->Ladder.Water;
	}

	if (GameInstance && GameInstance->Stable.bStarted)
	{
		Is(TEXT("an owner's"));
		return &GameInstance->Stable.Water;
	}

	if (GameInstance && GameInstance->Window.bStarted)
	{
		Is(TEXT("what you bet from"));
		return &GameInstance->Window.Water;
	}

	return nullptr;
}

void ATownGameMode::OpenStall(int32 Keeper)
{
	if (!PlaceData || !PlaceData->People.IsValidIndex(Keeper) || (PlaceData->People[Keeper].Sells.IsEmpty() && PlaceData->People[Keeper].Buys.IsEmpty()))
	{
		return;
	}

	ShopKeeper = Keeper;
	ShopChoice = 0;
	bShopping = true;
	bAtBook = false;
	bAtStall = true;
	bAtBag = false;

	ShopSaid = GetMarketWater() ? FString(TEXT("Water in hand. What you buy goes in your bag: open it with I, or right on the D-pad."))
		: FString(TEXT("You are holding no water of any kind. Fight, bet or own at the Gut first: the market does not give."));

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogTown, Log, TEXT("Street: at %s's stall, %d things on it"), *GetShopKeeper(), GetStallLines().Num());
}

TArray<FTownShopLine> ATownGameMode::GetStallLines() const
{
	TArray<FTownShopLine> Lines;

	if (!PlaceData || !PlaceData->People.IsValidIndex(ShopKeeper))
	{
		return Lines;
	}

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const float* Held = GetMarketWater();

	for (const FName& Id : PlaceData->People[ShopKeeper].Sells)
	{
		const TownConstants::FGood* Good = TownConstants::FindGood(Id);

		if (!Good)
		{
			continue;
		}

		FTownShopLine Line;

		Line.Name = Good->Name;
		Line.Detail = Good->Detail;
		Line.About = Good->About;
		Line.Price = Good->Price;
		Line.PriceText = TownConstants::Litres(Good->Price);

		if (!Held)
		{
			Line.Why = TEXT("no water");
		}
		else if (*Held < Good->Price)
		{
			Line.Why = FString::Printf(TEXT("%s short"), *TownConstants::Litres(Good->Price - *Held));
		}
		else
		{
			Line.bCanBuy = true;

			// how many are carried already
			if (const int32 Carried = GameInstance ? GameInstance->GetBagCount(Id) : 0; Carried > 0)
			{
				Line.Why = FString::Printf(TEXT("%d in your bag"), Carried);
				Line.bWorn = true;
			}
		}

		Lines.Add(MoveTemp(Line));
	}

	// and after what they sell, what they will buy of what is in the bag
	for (const FName& Id : PlaceData->People[ShopKeeper].Buys)
	{
		const TownConstants::FGood* Good = TownConstants::FindGood(Id);
		const int32 Carried = (Good && GameInstance) ? GameInstance->GetBagCount(Id) : 0;

		if (Carried <= 0 || Good->Worth <= 0.0f)
		{
			continue;
		}

		FTownShopLine Line;

		Line.Name = FString::Printf(TEXT("Sell: %s"), *FString(Good->Name).ToLower());
		Line.Detail = FString::Printf(TEXT("%d in your bag"), Carried);
		Line.About = Good->About;
		Line.Price = Good->Worth;
		Line.PriceText = FString::Printf(TEXT("pays %s"), *TownConstants::Litres(Good->Worth));
		Line.bCanBuy = true;

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

bool ATownGameMode::BuyAtStall(int32 Line)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !PlaceData || !PlaceData->People.IsValidIndex(ShopKeeper))
	{
		return false;
	}

	// the lines are the keeper's goods that the town knows, in the keeper's order
	TArray<const TownConstants::FGood*> Sold;

	for (const FName& Id : PlaceData->People[ShopKeeper].Sells)
	{
		if (const TownConstants::FGood* Good = TownConstants::FindGood(Id))
		{
			Sold.Add(Good);
		}
	}

	if (!Sold.IsValidIndex(Line))
	{
		// past what they sell: what they buy, of what is in the bag, in the same order as the lines
		TArray<const TownConstants::FGood*> Bought;

		for (const FName& Id : PlaceData->People[ShopKeeper].Buys)
		{
			if (const TownConstants::FGood* Good = TownConstants::FindGood(Id); Good && Good->Worth > 0.0f && GameInstance->GetBagCount(Id) > 0)
			{
				Bought.Add(Good);
			}
		}

		if (!Bought.IsValidIndex(Line - Sold.Num()))
		{
			return false;
		}

		const TownConstants::FGood& Given = *Bought[Line - Sold.Num()];

		// into whatever water the player holds; with none begun, a fighter's
		float* Into = GetMarketWater();

		if (!Into)
		{
			Into = &GameInstance->Ladder.Water;
		}

		*Into += Given.Worth;

		if (Into == &GameInstance->Ladder.Water)
		{
			GameInstance->LadderAtRungStart.Water += Given.Worth;
		}

		GameInstance->AddToBag(FName(Given.Id), -1);
		GameInstance->SaveRun();

		ShopSaid = FString::Printf(TEXT("Sold: %s, for %s."), *FString(Given.Name).ToLower(), *TownConstants::Litres(Given.Worth));

		if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
		{
			Sounds->Play(FName("cash"), FVector::ZeroVector);
		}

		UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

		// the lines have changed under the player's choice
		ShopChoice = FMath::Clamp(ShopChoice, 0, FMath::Max(GetStallLines().Num() - 1, 0));

		return true;
	}

	const TownConstants::FGood& Good = *Sold[Line];

	float* Held = GetMarketWater();

	if (!Held)
	{
		ShopSaid = TEXT("With what? You are holding no water of any kind.");
		return false;
	}

	if (*Held < Good.Price)
	{
		ShopSaid = FString::Printf(TEXT("%s is %s and you hold %s. Nothing here goes on a slate."), Good.Name, *TownConstants::Litres(Good.Price), *TownConstants::Litres(*Held));
		return false;
	}

	*Held -= Good.Price;

	// a fighter's water is also kept as it stood before its next fight, which is what is written down
	if (Held == &GameInstance->Ladder.Water)
	{
		GameInstance->LadderAtRungStart.Water -= Good.Price;
	}

	GameInstance->AddToBag(FName(Good.Id), 1);
	GameInstance->SaveRun();

	ShopSaid = FString::Printf(TEXT("Bought: %s, %s. It is in your bag, with %d of them now."), *FString(Good.Name).ToLower(), *TownConstants::Litres(Good.Price), GameInstance->GetBagCount(FName(Good.Id)));

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

	return true;
}

void ATownGameMode::ToggleBag()
{
	if (bShopping && bAtBag)
	{
		CloseShop();
		return;
	}

	if (!bSettled || bStoppedMenu || IsTalking() || bShopping || bMapUp)
	{
		// written down, so that a key that seemed to do nothing can be told from a key that was never pressed
		UE_LOG(LogTown, Log, TEXT("Street: the bag was asked for and stays shut (%s)"),
			!bSettled ? TEXT("the place is not ready") : (bStoppedMenu ? TEXT("the game is stopped") : (IsTalking() ? TEXT("somebody is talking") : (bShopping ? TEXT("a counter is up") : TEXT("the map is up")))));

		return;
	}

	ShopKeeper = INDEX_NONE;
	ShopChoice = 0;
	bShopping = true;
	bAtBook = false;
	bAtStall = false;
	bAtBag = true;
	bBagAsking = false;

	ShopSaid.Reset();

	// whoever looks in a bag stands still to do it
	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogTown, Log, TEXT("Street: the bag is open, %d things in it"), GetBagLines().Num());
}

TArray<FTownBagLine> ATownGameMode::GetBagLines() const
{
	TArray<FTownBagLine> Lines;

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutFighter* Walker = GetWalker();

	if (!GameInstance)
	{
		return Lines;
	}

	// water first: it is what everything else was bought with
	const float* Spent = GetMarketWater();

	// how many kinds of water are in the bag: with more than one, each says that it is kept apart from the others
	const int32 Kinds = (GameInstance->bLadderStarted ? 1 : 0) + (GameInstance->Window.bStarted ? 1 : 0) + (GameInstance->Stable.bStarted ? 1 : 0);

	const auto Water = [&Lines, Spent, Kinds](const float& Held, float Debt, const TCHAR* Whose, const TCHAR* Kept, const TCHAR* What, const TCHAR* Owed, const TCHAR* More)
	{
		FTownBagLine Line;

		Line.Kind = ETownBagKind::Water;
		Line.Part = TEXT("WATER");
		// which water, where there is more than one kind in the bag
		Line.Name = FString::Printf(TEXT("Water, %s"), Whose);
		Line.Count = TownConstants::Litres(FMath::Max(Held, 0.0f));
		Line.Detail = Kept;
		Line.Picture = FName("bag_water");

		Line.About.Add(TEXT("What everything in Ashmarket is paid in, and the only thing that is."));
		Line.About.Add(What);

		if (Spent == &Held)
		{
			Line.About.Add(TEXT("The stalls on Main Street are paid out of this."));
		}

		if (Debt > 0.5f)
		{
			Line.About.Add(FString::Printf(TEXT("Against it, %.0f L is owed%s."), Debt, Owed));
		}

		// the question anybody short of it asks
		Line.About.Add(FString(TEXT("TO GET MORE: ")) + More);

		if (Kinds > 1)
		{
			Line.About.Add(TEXT("Each way into the cage keeps its own water. What is won one way does not pay for another."));
		}

		Lines.Add(MoveTemp(Line));
	};

	if (GameInstance->IsOnePurse())
	{
		// one purse, and every way of filling it
		FTownBagLine Line;

		Line.Kind = ETownBagKind::Water;
		Line.Part = TEXT("WATER");
		Line.Name = TEXT("Water");
		Line.Count = TownConstants::Litres(FMath::Max(GameInstance->GetPurse(), 0.0f));
		Line.Detail = TEXT("all you hold");
		Line.Picture = FName("bag_water");

		Line.About.Add(TEXT("What everything in Ashmarket is paid in, and the only thing that is. One purse: it buys at the stalls, the armorers and the Block's book, and it is what you bet."));

		if (GameInstance->GetPurse() < -0.5f)
		{
			Line.About.Add(FString::Printf(TEXT("You hold none, and are %.0f L down on the pit boss's slate."), -GameInstance->GetPurse()));
		}

		Line.About.Add(TEXT("TO GET MORE, three ways, all at the Gut on Pool Street:"));
		Line.About.Add(TEXT("Fight. In at the fighters' door: the purse is yours if you win."));
		Line.About.Add(TEXT("Bet. Chalk's window, up Cold Alley: back the corner that wins."));
		Line.About.Add(TEXT("Own. Sign or buy somebody at the Block, then in at your yard gate and \"To the pit boss, for a fight\": the purse comes home to you."));

		const auto Owed = [&Line](float Debt, const TCHAR* Whom)
		{
			if (Debt > 0.5f)
			{
				Line.About.Add(FString::Printf(TEXT("Against it, %.0f L is owed %s."), Debt, Whom));
			}
		};

		Owed(GameInstance->bLadderStarted ? GameInstance->Ladder.Debt : 0.0f, TEXT("to the pit boss"));
		Owed(GameInstance->Window.bStarted ? GameInstance->Window.Debt : 0.0f, TEXT("to Mother Ledger"));
		Owed(GameInstance->Stable.bStarted ? GameInstance->Stable.Debt : 0.0f, TEXT("on your yard"));

		Lines.Add(MoveTemp(Line));
	}
	else
	{
	if (GameInstance->bLadderStarted)
	{
		Water(GameInstance->Ladder.Water, GameInstance->Ladder.Debt, TEXT("your fighter's"), TEXT("won in the cage"), TEXT("What you have won in the cage and kept. It buys at the armorers' counter, and stands behind what you stake on yourself."), TEXT(" to the pit boss, on his slate"),
			TEXT("win in the cage, in at the Gut's door on Pool Street. The purse is yours, and so is what you staked on yourself at the window before the bell."));
	}

	if (GameInstance->Window.bStarted)
	{
		Water(GameInstance->Window.Water, GameInstance->Window.Debt, TEXT("to bet with"), TEXT("staked at Chalk's window"), TEXT("What you put on other people's fights at Chalk's window on Pool Street, and what the window has paid you back."), TEXT(" to Mother Ledger"),
			TEXT("back the corner that wins at Chalk's window on Pool Street. Cleaned out, Mother Ledger will stake you again, at her price."));
	}

	if (GameInstance->Stable.bStarted)
	{
		Water(GameInstance->Stable.Water, GameInstance->Stable.Debt, TEXT("an owner's"), TEXT("what your yard runs on"), TEXT("What your yard runs on: it buys at the Block's book, feeds and trains whoever you keep, and is where the purses come home to."), TEXT(""),
			TEXT("in at your yard gate on Pool Street, then \"To the pit boss, for a fight\": one of yours goes in, and the purse comes home with a win. A yard that never fights only drinks."));
	}
	}

	// what was bought at the stalls
	for (const FGutBagItem& Item : GameInstance->Bag)
	{
		const TownConstants::FGood* Good = TownConstants::FindGood(Item.Good);

		if (!Good || Item.Count <= 0)
		{
			continue;
		}

		FTownBagLine Line;

		Line.Kind = ETownBagKind::Good;
		Line.Part = TEXT("FOOD AND PHYSIC");
		Line.Name = Good->Name;
		Line.Count = FString::Printf(TEXT("x %d"), Item.Count);
		Line.Detail = Good->Detail;
		Line.Picture = FName(*(FString(TEXT("bag_")) + Good->Id));
		Line.Good = Item.Good;

		Line.About.Add(Good->About);

		// who could have it now: if nobody, why not
		const TArray<FTownBagTaker> Takers = GetBagTakers(Item.Good);

		for (const FTownBagTaker& Taker : Takers)
		{
			Line.bCanUse |= Taker.bCan;
		}

		Line.Verb = Takers.Num() > 1 ? TEXT("use it, or give it") : TEXT("use it");

		if (Takers.Num() > 1)
		{
			Line.About.Add(TEXT("It will do the same for whoever is walking with you as it does for you."));
		}

		if (!Line.bCanUse)
		{
			Line.Why = Takers.IsEmpty() ? FString(TEXT("no fight ahead")) : (Takers.Num() == 1 ? Takers[0].Why : FString(TEXT("nobody needs it")));
		}

		Lines.Add(MoveTemp(Line));
	}

	// whoever the player owns, or pays: the paper that says so
	const float Whole = Walker ? Walker->GetMaxLimbHealth() : 0.0f;

	for (int32 i = 0; i < GameInstance->Stable.Fighters.Num(); ++i)
	{
		const FGutStableFighter& Fighter = GameInstance->Stable.Fighters[i];

		FTownBagLine Line;

		Line.Kind = ETownBagKind::Person;
		Line.Part = TEXT("PEOPLE");
		Line.Name = Fighter.FighterName.ToString();
		const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Fighter.Body);

		Line.Detail = Fighter.bOwned ? FString::Printf(TEXT("yours: %s bill of sale"), Sex.His()) : FString::Printf(TEXT("signed on: %s mark, and yours"), Sex.His());
		Line.Why = Fighter.bInTow ? TEXT("with you") : TEXT("in your yard");
		Line.Fighter = i;

		// the paper has on it the number they stood on at the Block, where they stood on one
		Line.Picture = !Fighter.bOwned ? FName("bag_paper_wage")
			: ((Fighter.Lot >= 1 && Fighter.Lot <= TownConstants::LotPapers) ? FName(*FString::Printf(TEXT("bag_paper_lot%d"), Fighter.Lot)) : FName("bag_paper_sale"));

		// who it is, as the caller would cry it: a man or a woman, how old, and what can be asked of them
		Line.About.Add(FString::Printf(TEXT("A %s, about %d.  %s"), Sex.Man(), GetAgeOf(Fighter), *Fighter.Billing.ToString()).TrimEnd());

		Line.About.Add(Fighter.bOwned
			? (Fighter.Lot > 0 ? FString::Printf(TEXT("Bought at the Block for %.0f L, off lot %d. The paper is all the Block gives: whoever holds it owns %s."), Fighter.Price, Fighter.Lot, Sex.Him())
				: FString::Printf(TEXT("Bought at the Block for %.0f L. The paper is all the Block gives: whoever holds it owns %s."), Fighter.Price, Sex.Him()))
			: FString::Printf(TEXT("Signed for %.0f L a day and a share of every purse. No collar: %s can walk away, and the paper says so."), Fighter.Wage, Sex.He()));

		Line.About.Add(FString::Printf(TEXT("%s skills, each out of %d: strength %d, speed %d, nerve %d.  Fights with %s.  Fights won: %d."), Sex.HisCap(), GetCageRules() ? GetCageRules()->GetStableMostTaught() : 5,
			Fighter.Strength, Fighter.Speed, Fighter.Nerve, Fighter.Weapon ? *(TEXT("the ") + Fighter.Weapon->DisplayName.ToString().ToLower()) : TEXT("bare hands"), Fighter.Wins));

		// how they are
		bool bHurt = false;

		for (const float Limb : Fighter.LimbHealth)
		{
			bHurt |= Limb > 0.0f && Limb < Whole - 0.5f;
		}

		FString State = Fighter.IsCrippled() ? TEXT("A limb is broken through: that is the cutter's work.") : (bHurt ? TEXT("Carrying a hurt that a splint would help.") : TEXT("Whole."));

		if (Fighter.bFed || Fighter.bSteadied)
		{
			State += FString::Printf(TEXT("  %s for %s next fight."), Fighter.bFed && Fighter.bSteadied ? TEXT("Fed, and steadied,") : (Fighter.bFed ? TEXT("Fed") : TEXT("Steadied")), Sex.His());
		}

		Line.About.Add(State);

		Line.About.Add(Fighter.bInTow ? FString::Printf(TEXT("Walking with you, until you bring %s in at the Gut. What is in your bag can be given to %s on the way."), Sex.Him(), Sex.Him())
			: FString::Printf(TEXT("In your yard, beside the Gut. The yard hand at the gate on Pool Street takes you to %s."), Sex.Him()));

		Lines.Add(MoveTemp(Line));
	}

	// what the player's fighter has on
	static const TCHAR* Parts[] = { TEXT("body"), TEXT("arms"), TEXT("legs") };

	for (const FName& Worn : GameInstance->bLadderStarted ? GameInstance->Ladder.Armor : TArray<FName>())
	{
		const UGutArmorData* Piece = LoadObject<UGutArmorData>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), TownConstants::ArmorFolder, *Worn.ToString(), *Worn.ToString()), nullptr, LOAD_NoWarn | LOAD_Quiet);

		if (!Piece)
		{
			continue;
		}

		FTownBagLine Line;

		Line.Kind = ETownBagKind::Worn;
		Line.Part = TEXT("WORN");
		Line.Name = Piece->DisplayName.ToString();
		Line.Detail = FString::Printf(TEXT("%s: %.0f%% off a club, %.0f%% off a blade"), Parts[FMath::Clamp<int32>(static_cast<int32>(Piece->Slot), 0, 2)], (1.0f - Piece->BluntScale) * 100.0f, (1.0f - Piece->BladedScale) * 100.0f);
		Line.Why = TEXT("worn");

		// its picture is named for its asset: DA_Gut_Armor_RubberVest is bag_rubbervest
		Line.Picture = FName(*(FString(TEXT("bag_")) + Worn.ToString().Replace(TownConstants::ArmorPrefix, TEXT("")).ToLower()));

		Line.About.Add(TEXT("Cut and fitted at Hide & Plate on Main Street. It is on you now, and goes into the cage with you."));
		Line.About.Add(FString::Printf(TEXT("Paid %.0f L for it."), Piece->Price));

		Lines.Add(MoveTemp(Line));
	}

	// papers: there is always the map
	{
		FTownBagLine Line;

		Line.Kind = ETownBagKind::Map;
		Line.Part = TEXT("PAPERS");
		Line.Name = TEXT("Map of the town");
		Line.Detail = TEXT("where everything is");
		Line.Picture = FName("ui_map");
		Line.Verb = TEXT("open it");
		Line.bCanUse = true;

		Line.About.Add(TEXT("Ashmarket, as somebody who walks it drew it: Pool Street and the Gut, Block Square, Main Street as far as the Gate Yard."));
		Line.About.Add(TEXT("M, or the Y button, opens it from anywhere in the street."));

		Lines.Add(MoveTemp(Line));
	}

	if (GameInstance->Window.bStarted && GameInstance->Window.Debt > 0.5f)
	{
		FTownBagLine Line;

		Line.Kind = ETownBagKind::Note;
		Line.Part = TEXT("PAPERS");
		Line.Name = TEXT("Mother Ledger's note");
		// what is owed is said outright, and the paper's own figures are said to be her terms: he read "25 FOR 35" on the
		// paper beside "67 L owed" and asked which he owed (25 November)
		Line.Count = FString::Printf(TEXT("you owe %.0f L"), GameInstance->Window.Debt);
		Line.Detail = FString::Printf(TEXT("her terms: %.0f lent is written as %.0f"), LoanGiven, LoanOwed);

		// the paper has no figure on it (bag_note_owed): the one it had, her terms, was read as the debt
		Line.Picture = FName("bag_note_owed");

		// what the paper means, in plain words: he could not make head or tail of it (24 November)
		Line.About.Add(FString::Printf(TEXT("You owe Mother Ledger %.0f L. That is the whole of it. Her terms: each time she lends she counts out %.0f L and writes down %.0f. The 10 is her price. It does not grow after that."),
			GameInstance->Window.Debt, LoanGiven, LoanOwed));
		Line.About.Add(FString::Printf(TEXT("\"Every debt is paid\" is the town's second law. Until this is clear, half of anything the window or the pit pays you is kept against it, and she will not let you owe more than %.0f L."), LoanLimit));
		Line.About.Add(TEXT("Pay it down whenever you like, any amount you hold, at her table outside the Counting House on Block Square."));

		Lines.Add(MoveTemp(Line));
	}

	// work taken and how it stands, work done and how it came out
	for (const FTownMission& Mission : StoryData ? StoryData->Missions : TArray<FTownMission>())
	{
		const int32 Stands = GetMark(Mission.Key);

		if (Stands <= 0)
		{
			continue;
		}

		FTownBagLine Line;

		Line.Kind = ETownBagKind::Note;
		Line.Part = TEXT("WORK");
		Line.Name = Mission.Name;
		Line.Count = Stands >= TownStory::Ended ? TEXT("done") : TEXT("in hand");
		Line.Detail = Mission.Giver;
		Line.Picture = FName("bag_paper_wage");
		Line.About.Add(GetMissionStep(Mission));

		Lines.Add(MoveTemp(Line));
	}

	// how the player stands with each of the town's powers it has had to do with
	for (const FTownFaction& Faction : StoryData ? StoryData->Factions : TArray<FTownFaction>())
	{
		const FName Mark(*(FString(TEXT("st.")) + Faction.Key.ToString()));

		if (!GameInstance->Story.Marks.Contains(Mark))
		{
			continue;
		}

		const int32 Standing = GetMark(Mark);

		FTownBagLine Line;

		Line.Kind = ETownBagKind::Note;
		Line.Part = TEXT("STANDING");
		Line.Name = Faction.Name;
		Line.Count = FString::Printf(TEXT("%s%d"), Standing > 0 ? TEXT("+") : TEXT(""), Standing);
		Line.Detail = GetStandingWord(Standing);
		Line.About.Add(Faction.About);
		Line.About.Add(TEXT("From 100 down to 100 under. Work is offered from 19 under; at 20 the harder work, at 40 what they offer their own. At 20 under they go cold, and at 50 under they come for you."));

		Lines.Add(MoveTemp(Line));
	}

	// and what the player has done, as it would tell it: the last of it
	if (!GameInstance->Story.Journal.IsEmpty())
	{
		FTownBagLine Line;

		Line.Kind = ETownBagKind::Note;
		Line.Part = TEXT("STANDING");
		Line.Name = TEXT("What you have done");
		Line.Count = FString::Printf(TEXT("trust %d  \u00B7  fear %d"), GetMark(FName("trust")), GetMark(FName("fear")));
		Line.Detail = TEXT("as the town tells it");

		for (int32 i = FMath::Max(0, GameInstance->Story.Journal.Num() - 6); i < GameInstance->Story.Journal.Num(); ++i)
		{
			Line.About.Add(GameInstance->Story.Journal[i]);
		}

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

TArray<FTownBagTaker> ATownGameMode::GetBagTakers(const FName& Good) const
{
	TArray<FTownBagTaker> Takers;

	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutFighter* Walker = GetWalker();

	if (!GameInstance || !TownConstants::FindGood(Good))
	{
		return Takers;
	}

	// what is found, not bought: water is poured into one's own, smokes are given to whoever is walking with the player,
	// and what is only good for selling is nobody's to use
	if (const TownConstants::FGood* Found = TownConstants::FindGood(Good); Found->Pours > 0.0f)
	{
		FTownBagTaker Taker;

		Taker.Name = TEXT("Yourself");
		Taker.bCan = true;

		Takers.Add(MoveTemp(Taker));

		return Takers;
	}
	else if (Found->Price <= 0.0f)
	{
		for (int32 i = 0; Good == FName("smokes") && i < GameInstance->Stable.Fighters.Num(); ++i)
		{
			if (GameInstance->Stable.Fighters[i].bInTow)
			{
				FTownBagTaker Taker;

				Taker.Name = GameInstance->Stable.Fighters[i].FighterName.ToString();
				Taker.Fighter = i;
				Taker.bCan = true;

				Takers.Add(MoveTemp(Taker));
			}
		}

		return Takers;
	}

	const float Whole = Walker ? Walker->GetMaxLimbHealth() : 0.0f;

	// whether somebody has had one already, or has nothing a splint would help
	const auto Could = [&Good, Whole](FTownBagTaker& Taker, bool bFed, bool bSteadied, const float* Limbs, bool bLimbsKept)
	{
		if (Good == FName("bread"))
		{
			Taker.bCan = !bFed;
			Taker.Why = bFed ? TEXT("eaten already") : TEXT("");
		}
		else if (Good == FName("tea"))
		{
			Taker.bCan = !bSteadied;
			Taker.Why = bSteadied ? TEXT("drunk already") : TEXT("");
		}
		else
		{
			// a limb that is hurt and not broken through
			for (int32 i = 0; bLimbsKept && i < GutNumLimbs; ++i)
			{
				Taker.bCan |= Limbs[i] > 0.0f && Limbs[i] < Whole - 0.5f;
			}

			Taker.Why = Taker.bCan ? TEXT("") : TEXT("nothing to bind");
		}
	};

	// the player's own fighter, if it has one: there is no fight ahead of anybody else
	if (GameInstance->bLadderStarted)
	{
		FTownBagTaker Taker;

		Taker.Name = TEXT("Yourself");

		Could(Taker, GameInstance->Ladder.bFed, GameInstance->Ladder.bSteadied, GameInstance->Ladder.LimbHealth, GameInstance->Ladder.bInjuriesCarried);

		Takers.Add(MoveTemp(Taker));
	}

	// and whoever is walking with the player: nothing can be handed to somebody who is in the yard
	for (int32 i = 0; i < GameInstance->Stable.Fighters.Num(); ++i)
	{
		const FGutStableFighter& Fighter = GameInstance->Stable.Fighters[i];

		if (!Fighter.bInTow)
		{
			continue;
		}

		FTownBagTaker Taker;

		Taker.Name = Fighter.FighterName.ToString();
		Taker.Fighter = i;

		Could(Taker, Fighter.bFed, Fighter.bSteadied, Fighter.LimbHealth, true);

		Takers.Add(MoveTemp(Taker));
	}

	return Takers;
}

bool ATownGameMode::UseFromBag(int32 Line)
{
	const TArray<FTownBagLine> Lines = GetBagLines();

	if (!IsBagOpen() || !Lines.IsValidIndex(Line))
	{
		return false;
	}

	const FTownBagLine& Thing = Lines[Line];

	if (Thing.Kind == ETownBagKind::Map)
	{
		// the bag is shut, and the map is what is up
		CloseShop();
		ToggleMap();

		return true;
	}

	if (Thing.Kind != ETownBagKind::Good)
	{
		return false;
	}

	const TArray<FTownBagTaker> Takers = GetBagTakers(Thing.Good);

	if (Takers.IsEmpty())
	{
		const TownConstants::FGood* Good = TownConstants::FindGood(Thing.Good);

		ShopSaid = (Good && Good->Price <= 0.0f)
			? (Thing.Good == FName("smokes") ? FString(TEXT("Nobody is walking with you to give them to. The tea seller on Main Street will take them off you.")) : FString(TEXT("It is no use to you. The scrap seller on Main Street pays for it.")))
			: FString(TEXT("It would be wasted: you have no fight ahead of you, and nobody walking with you to give it to. It keeps."));

		return false;
	}

	// only one who could have it: nothing to ask
	if (Takers.Num() == 1)
	{
		return GiveFromBag(Thing.Good, Takers[0].Fighter);
	}

	// asked, with the player on the first who could do with it
	bBagAsking = true;
	BagAsked = Thing.Good;
	BagAskChoice = FMath::Max(0, Takers.IndexOfByPredicate([](const FTownBagTaker& Taker) { return Taker.bCan; }));

	ShopSaid.Reset();

	return false;
}

bool ATownGameMode::GiveFromBag(const FName& Good, int32 Fighter)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutFighter* Walker = GetWalker();

	if (!GameInstance || !IsBagOpen() || GameInstance->GetBagCount(Good) <= 0)
	{
		return false;
	}

	const FTownBagTaker* Taker = nullptr;
	const TArray<FTownBagTaker> Takers = GetBagTakers(Good);

	for (const FTownBagTaker& One : Takers)
	{
		if (One.Fighter == Fighter)
		{
			Taker = &One;
		}
	}

	if (!Taker)
	{
		return false;
	}

	const bool bSelf = Fighter == INDEX_NONE;
	const float Whole = Walker ? Walker->GetMaxLimbHealth() : 0.0f;

	// what was found: water poured into one's own, or smokes handed to somebody who is owned
	if (const TownConstants::FGood* Found = TownConstants::FindGood(Good); Found && Found->Price <= 0.0f && Taker->bCan)
	{
		if (Found->Pours > 0.0f)
		{
			float* Into = GetMarketWater();

			if (!Into)
			{
				Into = &GameInstance->Ladder.Water;
			}

			*Into += Found->Pours;

			if (Into == &GameInstance->Ladder.Water)
			{
				GameInstance->LadderAtRungStart.Water += Found->Pours;
			}

			ShopSaid = FString::Printf(TEXT("Poured into your own: %s more."), *TownConstants::Litres(Found->Pours));
		}
		else if (GameInstance->Stable.Fighters.IsValidIndex(Fighter))
		{
			FGutStableFighter& Theirs = GameInstance->Stable.Fighters[Fighter];

			Theirs.Resentment = FMath::Max(0.0f, Theirs.Resentment - TownConstants::SmokesEase);

			ShopSaid = FString::Printf(TEXT("%s takes the tin without a word, and looks at you a moment longer than usual. It is remembered."), *Taker->Name);
		}
		else
		{
			return false;
		}

		GameInstance->AddToBag(Good, -1);
		GameInstance->SaveRun();

		bBagAsking = false;

		UE_LOG(LogTown, Log, TEXT("Street: %s"), *ShopSaid);

		if (Found->Pours <= 0.0f)
		{
			ShowGiven(Fighter, Good, ShopSaid);
		}

		return true;
	}

	if (!Taker->bCan)
	{
		ShopSaid = Good == FName("splint")
			? (bSelf ? FString(TEXT("There is nothing to bind: no limb of yours is hurt that a splint would help."))
				: FString::Printf(TEXT("There is nothing to bind: %s has no hurt that a splint would help."), *Taker->Name))
			: (bSelf ? FString(TEXT("You have had one since your last fight. A second does nothing: it keeps."))
				: FString::Printf(TEXT("%s has had one since %s last fight. A second does nothing: it keeps."), *Taker->Name,
					(GameInstance->Stable.Fighters.IsValidIndex(Fighter) ? UGutSoundSubsystem::WhoIs(this, GameInstance->Stable.Fighters[Fighter].Body) : FGutWho()).His()));

		return false;
	}

	// the worst of the limbs that are hurt and not broken through gets back half of what it has lost
	static const TCHAR* LimbNames[] = { TEXT("left arm"), TEXT("right arm"), TEXT("left leg"), TEXT("right leg") };

	const auto Bind = [Whole](float* Limbs)
	{
		int32 Worst = INDEX_NONE;

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			if (Limbs[i] > 0.0f && Limbs[i] < Whole - 0.5f && (Worst == INDEX_NONE || Limbs[i] < Limbs[Worst]))
			{
				Worst = i;
			}
		}

		if (Worst != INDEX_NONE)
		{
			Limbs[Worst] += (Whole - Limbs[Worst]) * 0.5f;
		}

		return Worst;
	};

	if (bSelf)
	{
		FGutLadderState& Mine = GameInstance->Ladder;

		if (Good == FName("bread"))
		{
			Mine.bFed = true;
			ShopSaid = TEXT("Eaten. Your wind will come back faster in your next fight.");
		}
		else if (Good == FName("tea"))
		{
			Mine.bSteadied = true;
			ShopSaid = TEXT("Drunk. Your wind will start coming back sooner in your next fight.");
		}
		else
		{
			const int32 Bound = Bind(Mine.LimbHealth);

			if (Bound == INDEX_NONE)
			{
				return false;
			}

			ShopSaid = FString::Printf(TEXT("Bound on. Your %s is better than it was."), LimbNames[FMath::Clamp(Bound, 0, 3)]);
		}

		// what is written down is the fighter as it stood before its next fight: this is part of that
		GameInstance->LadderAtRungStart.bFed = Mine.bFed;
		GameInstance->LadderAtRungStart.bSteadied = Mine.bSteadied;

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			GameInstance->LadderAtRungStart.LimbHealth[i] = Mine.LimbHealth[i];
		}
	}
	else
	{
		FGutStableFighter& Theirs = GameInstance->Stable.Fighters[Fighter];
		const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Theirs.Body);

		if (Good == FName("bread"))
		{
			Theirs.bFed = true;
			Theirs.Resentment = FMath::Max(0.0f, Theirs.Resentment - TownConstants::FoodEases);

			ShopSaid = FString::Printf(TEXT("Given to %s, and gone before you have shut the bag. %s wind will come back faster in %s next fight, and it is remembered."), *Taker->Name, Sex.HisCap(), Sex.His());
		}
		else if (Good == FName("tea"))
		{
			Theirs.bSteadied = true;
			Theirs.Resentment = FMath::Max(0.0f, Theirs.Resentment - TownConstants::FoodEases);

			ShopSaid = FString::Printf(TEXT("Given to %s, who drinks it slowly. %s wind will start coming back sooner in %s next fight, and it is remembered."), *Taker->Name, Sex.HisCap(), Sex.His());
		}
		else
		{
			const int32 Bound = Bind(Theirs.LimbHealth);

			if (Bound == INDEX_NONE)
			{
				return false;
			}

			Theirs.Resentment = FMath::Max(0.0f, Theirs.Resentment - TownConstants::CareEases);

			ShopSaid = FString::Printf(TEXT("Bound on. %s's %s is better than it was, and it is remembered."), *Taker->Name, LimbNames[FMath::Clamp(Bound, 0, 3)]);
		}
	}

	GameInstance->AddToBag(Good, -1);
	GameInstance->SaveRun();

	bBagAsking = false;

	// the thing the player was on may be gone
	ShopChoice = FMath::Min(ShopChoice, GetBagLines().Num());

	UE_LOG(LogTown, Log, TEXT("Street: from the bag: %s"), *ShopSaid);

	// handed to somebody who is walking with the player: it is seen to be taken
	if (!bSelf)
	{
		ShowGiven(Fighter, Good, ShopSaid);
	}

	return true;
}

void ATownGameMode::SettleYard()
{
	UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutFighter* Walker = GetWalker();

	if (!IsYard() || !GameInstance || !Walker)
	{
		return;
	}

	const float HalfHeight = Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (int32 i = 0; i < GameInstance->Stable.Fighters.Num(); ++i)
	{
		const FGutStableFighter& Fighter = GameInstance->Stable.Fighters[i];

		if (Fighter.bInTow)
		{
			continue;
		}

		const FTownArrival& Stall = PlaceData->Stalls[YardBodies.Num() % PlaceData->Stalls.Num()];

		// one to a place, and a second row a pace in front if there are ever more of them than places
		const FVector Feet = Stall.Place + (FRotator(0.0f, Stall.Facing, 0.0f).Vector() * (110.0f * (YardBodies.Num() / PlaceData->Stalls.Num())));

		if (AGutFighter* Body = StandOwned(Fighter, FTransform(FRotator(0.0f, Stall.Facing, 0.0f), Feet + FVector(0.0f, 0.0f, HalfHeight))))
		{
			KeepOnGround(Body);

			// with a fight to come or one just had: they stretch, they are sore, they get their breath
			Body->SetAtEaseFamily(FName("yard"));

			YardBodies.Add(Body);
			YardFighters.Add(i);
		}
	}

	UE_LOG(LogTown, Log, TEXT("Yard: %d of the player's people stand in it, day %d, %.0f L held"), YardBodies.Num(), GameInstance->Stable.Day, GameInstance->Stable.Water);
}

void ATownGameMode::OpenYardPage(int32 Which)
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !YardFighters.IsValidIndex(Which) || !GameInstance->Stable.Fighters.IsValidIndex(YardFighters[Which]))
	{
		return;
	}

	YardFocus = YardFighters[Which];

	ShopKeeper = INDEX_NONE;
	ShopChoice = 0;
	bShopping = true;
	bAtBook = false;
	bAtStall = false;
	bAtBag = false;
	bAtYardPage = true;

	const FGutStableFighter& Who = GameInstance->Stable.Fighters[YardFocus];

	// who they are, and how they are with the player
	FString Mood;

	if (!Who.bOwned)
	{
		Mood = FString::Printf(TEXT(" Paid %.0f L a day."), Who.Wage);
	}
	else if (Who.Resentment >= 75.0f)
	{
		Mood = TEXT(" Will not look at you.");
	}
	else if (Who.Resentment >= 55.0f)
	{
		Mood = TEXT(" Sullen.");
	}

	// a man or a woman, how old, and what he or she can do: the sheet had none of it (24 November)
	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Who.Body);

	ShopSaid = FString::Printf(TEXT("A %s, about %d. Strength %d, speed %d, nerve %d, each out of %d; fights with %s. "), Sex.Man(), GetAgeOf(Who), Who.Strength, Who.Speed, Who.Nerve,
		GetCageRules() ? GetCageRules()->GetStableMostTaught() : 5, Who.Weapon ? *(TEXT("the ") + Who.Weapon->DisplayName.ToString().ToLower()) : TEXT("bare hands"));

	ShopSaid += Who.Billing.ToString() + (Who.Wins > 0 ? FString::Printf(TEXT(" %d %s for you."), Who.Wins, Who.Wins == 1 ? TEXT("fight won") : TEXT("fights won")) : FString()) + Mood;

	// what a day costs, said where a day can be spent: somebody on a wage who is not paid walks
	ShopSaid += FString::Printf(TEXT(" A day gone by costs %.0f L in feed and wages; you hold %.0f."), GetDayDue(), GameInstance->Stable.Water);

	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0))
	{
		Hands->SetIgnoreMoveInput(true);
	}

	if (YardBodies.IsValidIndex(Which) && YardBodies[Which])
	{
		YardBodies[Which]->SetLookTarget(GetWalker());
	}

	UE_LOG(LogTown, Log, TEXT("Yard: seeing to %s"), *Who.FighterName.ToString());
}

void ATownGameMode::GetYardPage(TArray<FTownShopLine>& OutLines, TArray<int32>& OutCodes) const
{
	using namespace GutStableLines;

	OutLines.Reset();
	OutCodes.Reset();

	UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutGameMode* Rules = GetCageRules();

	if (!GameInstance || !Rules || !GameInstance->Stable.Fighters.IsValidIndex(YardFocus))
	{
		return;
	}

	FTownLentRules Lent(Rules, GameInstance);

	const FGutStableState& State = GameInstance->Stable;
	const FGutStableFighter& Who = State.Fighters[YardFocus];
	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Who.Body);

	const auto Add = [&OutLines, &OutCodes, &State](int32 Code, const FString& Name, const FString& Detail, float Price, const FString& PriceText, bool bCan, const FString& Why)
	{
		FTownShopLine Line;

		Line.Name = Name;
		Line.Detail = Detail;
		Line.Price = Price;
		Line.PriceText = PriceText;
		Line.bCanBuy = bCan && Price <= State.Water;
		Line.Why = !bCan ? Why : (Price > State.Water ? FString::Printf(TEXT("%.0f L short"), Price - State.Water) : FString());

		OutLines.Add(MoveTemp(Line));
		OutCodes.Add(Code);
	};

	const int32 Most = Rules->GetStableMostTaught();

	static const TCHAR* Taught[] = { TEXT("strength"), TEXT("speed"), TEXT("nerve") };

	const int32 Has[] = { Who.Strength, Who.Speed, Who.Nerve };

	// training takes the day, and the day has to be paid for out of what the training leaves
	const bool bShortAfter = State.Water - Rules->GetTrainingCost() < GetDayDue();

	for (int32 i = 0; i < 3; ++i)
	{
		Add(TrainStrength + i, FString::Printf(TEXT("Train %s"), Taught[i]), FString::Printf(TEXT("%d of %d now  \u00B7  takes the day"), Has[i], Most),
			Rules->GetTrainingCost(), FString(), Has[i] < Most, TEXT("taught"));

		// The warning is under the list, where there is room for it: on the line it ran on under the price (24 November)
		OutLines.Last().About = bShortAfter
			? FString::Printf(TEXT("Training is %.0f L and takes the day. That would leave you %.0f L, and the day's feed and wages are %.0f L: the day would go unpaid, and whoever is on a wage walks."),
				Rules->GetTrainingCost(), FMath::Max(State.Water - Rules->GetTrainingCost(), 0.0f), GetDayDue())
			: FString::Printf(TEXT("Training is %.0f L and takes the day. Feed and wages for the day, %.0f L, come out of what is left."), Rules->GetTrainingCost(), GetDayDue());
	}

	if (Who.IsCrippled())
	{
		Add(Patch, TEXT("Mend a limb"), TEXT("Doc Stitch comes: the first that is gone"), Rules->GetStablePatchCost(), FString(), true, FString());
	}

	Add(Dope, FString::Printf(TEXT("Give %s Dust"), Sex.Him()), Who.Habit >= 0.5f ? FString::Printf(TEXT("%s needs it now"), Sex.He()) : FString(TEXT("hits harder; comes to need it")), Rules->GetDopeCost(), FString(), !Who.bDoped, TEXT("dusted"));

	if (Who.bOwned)
	{
		if (Lent.Rules->IsTagDue(YardFocus))
		{
			Add(HandOverTag, FString::Printf(TEXT("Hand over %s tag"), Sex.His()), FString::Printf(TEXT("bought at the Pump office: %s has won it"), Sex.He()), Rules->GetTagPrice(), FString(), true, FString());
			Add(DenyTag, TEXT("Refuse the tag"), TEXT("you gave your word: it is remembered"), 0.0f, TEXT(" "), true, FString());
		}
		else if (!Who.bPromisedTag && !Who.bBetrayed)
		{
			Add(Promise, TEXT("Promise a tag"), FString::Printf(TEXT("for %d wins: %s fights harder for it"), Rules->GetWinsForTag(), Sex.He()), 0.0f, TEXT("a promise"), true, FString());
		}
	}

	Add(WalkOut, TEXT("Come with me"), FString::Printf(TEXT("%s walks behind you, out to the street"), Sex.He()), 0.0f, TEXT(" "), true, FString());

	Add(Sell, Who.bOwned ? FString(TEXT("Sell at the Block")) : FString::Printf(TEXT("Let %s go"), Sex.Him()), Who.bOwned ? FString(TEXT("gone for good")) : FString::Printf(TEXT("%s owes you nothing"), Sex.He()), 0.0f,
		Who.bOwned ? FString::Printf(TEXT("about %.0f L back"), FMath::RoundToFloat(Who.Price * 0.8f * (Who.IsCrippled() ? 0.5f : 1.0f))) : FString(TEXT(" ")), true, FString());
}

bool ATownGameMode::DoYardLine(int32 Line)
{
	using namespace GutStableLines;

	TArray<FTownShopLine> Lines;
	TArray<int32> Codes;
	GetYardPage(Lines, Codes);

	UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutGameMode* Rules = GetCageRules();

	if (!GameInstance || !Rules || !Lines.IsValidIndex(Line) || !GameInstance->Stable.Fighters.IsValidIndex(YardFocus))
	{
		return false;
	}

	const FString Name = GameInstance->Stable.Fighters[YardFocus].FighterName.ToString();
	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, GameInstance->Stable.Fighters[YardFocus].Body);
	const int32 Which = YardFighters.IndexOfByKey(YardFocus);

	if (!Lines[Line].bCanBuy)
	{
		ShopSaid = Lines[Line].Why.EndsWith(TEXT("short"))
			? FString::Printf(TEXT("That is %.0f L, and you hold %.0f."), Lines[Line].Price, GameInstance->Stable.Water)
			: (Lines[Line].Why == TEXT("taught") ? FString::Printf(TEXT("%s has been taught all of that there is to teach."), *Name) : FString::Printf(TEXT("%s has had that already."), *Name));

		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	const int32 Code = Codes[Line];
	const int32 DayBefore = GameInstance->Stable.Day;
	const float WaterBefore = GameInstance->Stable.Water;
	const float DebtBefore = GameInstance->Stable.Debt;
	const TArray<FGutStableFighter> Before = GameInstance->Stable.Fighters;

	bool bDone = false;

	{
		FTownLentRules Lent(Rules, GameInstance);

		switch (Code)
		{
		case TrainStrength:
		case TrainSpeed:
		case TrainNerve:
			bDone = Lent.Rules->TrainFighter(YardFocus, static_cast<EGutTraining>(Code - TrainStrength));
			break;

		case Patch:
			bDone = Lent.Rules->PatchFighter(YardFocus);
			break;

		case Dope:
			bDone = Lent.Rules->DopeFighter(YardFocus);
			break;

		case Promise:
			bDone = Lent.Rules->PromiseTag(YardFocus);
			break;

		case HandOverTag:
			bDone = Lent.Rules->GiveTag(YardFocus);
			break;

		case DenyTag:
			bDone = Lent.Rules->BreakPromise(YardFocus);
			break;

		case Sell:
			bDone = Lent.Rules->SellFighter(YardFocus);
			break;

		case WalkOut:
			bDone = true;
			break;

		default:
			break;
		}
	}

	if (!bDone)
	{
		ShopSaid = TEXT("It cannot be done today.");
		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this); Sounds && GameInstance->Stable.Water != WaterBefore)
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector);
	}

	// a day gone by may have lost somebody, the one being seen to among them
	const FString Day = GameInstance->Stable.Day != DayBefore
		? FString::Printf(TEXT(" The day is gone with it: it is day %d. %s"), GameInstance->Stable.Day, *SettleAfterDay(Before, DebtBefore)) : FString();

	if (GameInstance->Stable.Day != DayBefore && !GameInstance->Stable.Fighters.IsValidIndex(YardFocus))
	{
		static const TCHAR* Worked[] = { TEXT("strength"), TEXT("speed"), TEXT("nerve") };

		CloseShop();

		ShowNotice(FString::Printf(TEXT("%s is worked at %s until dark.%s"), *Name, Worked[FMath::Clamp(Code - TrainStrength, 0, 2)], *Day));

		NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::GivenTime;

		GameInstance->SaveRun();

		UE_LOG(LogTown, Log, TEXT("Yard: %s"), *Notice);

		return true;
	}

	if (Code == Sell || Code == WalkOut)
	{
		AGutFighter* Body = YardBodies.IsValidIndex(Which) ? YardBodies[Which].Get() : nullptr;

		// out of the yard's own places either way: sold, they are gone, and every place after theirs in the stable moves up one
		if (YardBodies.IsValidIndex(Which))
		{
			YardBodies.RemoveAt(Which);
			YardFighters.RemoveAt(Which);
		}

		CloseShop();

		if (Code == Sell)
		{
			if (Body)
			{
				Body->Destroy();
			}

			for (int32& Other : YardFighters)
			{
				Other -= Other > YardFocus ? 1 : 0;
			}

			// and whoever is behind the player is the same person still
			for (FTownTow& One : Tow)
			{
				One.Fighter -= One.Fighter > YardFocus ? 1 : 0;
			}

			ShowNotice(FString::Printf(TEXT("%s is gone. You hold %.0f L."), *Name, GameInstance->Stable.Water));
		}
		else if (Body)
		{
			GameInstance->Stable.Fighters[YardFocus].bInTow = true;

			Body->SetLookTarget(nullptr);

			TakeInTow(Body, YardFocus, INDEX_NONE);

			ShowNotice(FString::Printf(TEXT("%s falls in behind you."), *Name));
		}

		GameInstance->SaveRun();

		return true;
	}

	static const TCHAR* Taught[] = { TEXT("strength"), TEXT("speed"), TEXT("nerve") };

	switch (Code)
	{
	case TrainStrength:
	case TrainSpeed:
	case TrainNerve:
		ShopSaid = FString::Printf(TEXT("%s is worked at %s until dark.%s"), *Name, Taught[Code - TrainStrength], *Day);
		break;

	case Patch:
		ShopSaid = FString::Printf(TEXT("Doc Stitch has been and gone. %s has the limb back, and it is remembered."), *Name);
		break;

	case Dope:
		ShopSaid = FString::Printf(TEXT("%s has the Dust for %s next fight."), *Name, Sex.His());
		break;

	case Promise:
		ShopSaid = FString::Printf(TEXT("%s has your word: a tag for the wins."), *Name);
		break;

	case HandOverTag:
		ShopSaid = FString::Printf(TEXT("%s has a tag, and is nobody's. %s may sign with you yet."), *Name, Sex.HeCap());
		break;

	default:
		ShopSaid = FString::Printf(TEXT("%s hears it, and says nothing."), *Name);
		break;
	}

	// what it cost is said with what was done: 5 L of Dust went unnoticed, and the next page's figure looked wrong (24 November)
	if (const float Paid = WaterBefore - GameInstance->Stable.Water; Paid > 0.5f)
	{
		ShopSaid += FString::Printf(TEXT(" Paid %.0f L: you hold %.0f."), Paid, GameInstance->Stable.Water);
	}

	GameInstance->SaveRun();

	ShopChoice = FMath::Min(ShopChoice, GetShopLines().Num());

	UE_LOG(LogTown, Log, TEXT("Yard: %s"), *ShopSaid);

	return true;
}

void ATownGameMode::ShowGiven(int32 Fighter, const FName& Good, const FString& Said)
{
	const int32 Which = Tow.IndexOfByPredicate([Fighter](const FTownTow& One) { return One.Fighter == Fighter; });

	AGutFighter* Body = TowBodies.IsValidIndex(Which) ? TowBodies[Which].Get() : nullptr;
	AGutFighter* Walker = GetWalker();

	if (!Body || !Walker)
	{
		return;
	}

	// The bag is put away and the two of them are looked at. A line in the bag saying it was drunk was all there was, with
	// whoever drank it behind the bag and behind the player (22 November: "I need confirmation somehow").
	CloseShop();

	Notice = Said;
	NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::GivenTime;

	GivenTo = Body;
	GivenUntil = GetWorld()->GetTimeSeconds() + TownConstants::GivenTime;

	// they turn to whoever is handing it to them
	const FVector Toward = Walker->GetActorLocation() - Body->GetActorLocation();

	if (!Toward.IsNearlyZero())
	{
		Body->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));
	}

	Body->SetLookTarget(Walker);

	// Smokes are lit. Anything else goes to the mouth. Somebody whose wrists are tied has them let go for as long as it takes.
	UAnimSequenceBase* Taking = Cast<UAnimSequenceBase>(FSoftObjectPath(Good == FName("smokes") ? TownConstants::GivenSmokes : TownConstants::GivenToMouth).TryLoad());

	if (Taking)
	{
		Body->TakeInHand(Taking);
	}

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("coins"), Body->GetActorLocation());
	}
}

void ATownGameMode::HandleBagInput(EGutMenuInput Input)
{
	if (bBagAsking)
	{
		// whoever could have it, and under them nobody
		const TArray<FTownBagTaker> Takers = GetBagTakers(BagAsked);

		if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
		{
			BagAskChoice = (BagAskChoice + (Input == EGutMenuInput::Up ? Takers.Num() : 1)) % (Takers.Num() + 1);
		}
		else if (Input == EGutMenuInput::Confirm)
		{
			if (!Takers.IsValidIndex(BagAskChoice))
			{
				bBagAsking = false;
			}
			else
			{
				GiveFromBag(BagAsked, Takers[BagAskChoice].Fighter);
			}
		}

		return;
	}

	// the things, and under them the line that shuts the bag
	const int32 Things = GetBagLines().Num();

	if (Input == EGutMenuInput::Up || Input == EGutMenuInput::Down)
	{
		ShopChoice = (ShopChoice + (Input == EGutMenuInput::Up ? Things : 1)) % (Things + 1);

		// what was said was about the thing the player was on
		ShopSaid.Reset();
	}
	else if (Input == EGutMenuInput::Confirm)
	{
		if (ShopChoice >= Things)
		{
			CloseShop();
		}
		else
		{
			UseFromBag(ShopChoice);
		}
	}
}

FString ATownGameMode::GetShopVerb() const
{
	if (bAtBag)
	{
		// whatever the thing the player is on says can be done with it
		const TArray<FTownBagLine> Things = GetBagLines();

		return bBagAsking ? FString(TEXT("give it")) : (Things.IsValidIndex(ShopChoice) ? Things[ShopChoice].Verb : FString(TEXT("shut the bag")));
	}

	if (bAtYardPage)
	{
		return TEXT("do it");
	}

	// at a counter where the thing the player is on cannot be had, the key asks for it and is told why not
	if (!bAtBook && !bAtStall)
	{
		const TArray<FTownShopLine> Lines = GetShopLines();

		if (Lines.IsValidIndex(ShopChoice) && !Lines[ShopChoice].bCanBuy)
		{
			return TEXT("ask for it");
		}
	}

	if (bAtBook && Auction.bOn)
	{
		return TEXT("bid");
	}

	if (bAtBook && bBids)
	{
		TArray<FGutStableFighter> Offers;
		TArray<int32> Lots;
		GetBookOffers(Offers, Lots);

		if (Offers.IsValidIndex(ShopChoice))
		{
			return Offers[ShopChoice].bOwned ? TEXT("have the lot put up") : TEXT("sign");
		}
	}

	return TEXT("buy it");
}

FString ATownGameMode::GetShopTitle() const
{
	if (bAtBook && Auction.bOn)
	{
		return Auction.Lot > 0 ? FString::Printf(TEXT("LOT %d  \u00B7  %s  \u00B7  UP NOW"), Auction.Lot, *Auction.Offer.FighterName.ToString().ToUpper())
			: FString::Printf(TEXT("%s  \u00B7  UP NOW"), *Auction.Offer.FighterName.ToString().ToUpper());
	}

	if (bAtBook)
	{
		return TEXT("THE BLOCK'S BOOK");
	}

	if (bAtBag)
	{
		return TEXT("YOUR BAG");
	}

	if (bAtStall)
	{
		return FString::Printf(TEXT("%s'S STALL"), *GetShopKeeper().ToUpper());
	}

	if (const UGutGameInstance* GameInstance = GetGutGameInstance(); bAtYardPage && GameInstance && GameInstance->Stable.Fighters.IsValidIndex(YardFocus))
	{
		const FGutStableFighter& Who = GameInstance->Stable.Fighters[YardFocus];

		return FString::Printf(TEXT("%s  \u00B7  %s"), *Who.FighterName.ToString().ToUpper(), Who.bOwned ? TEXT("YOURS") : TEXT("SIGNED WITH YOU"));
	}

	const FString Keeper = GetShopKeeper();

	return Keeper.IsEmpty() ? FString(TEXT("THE COUNTER")) : FString::Printf(TEXT("%s'S COUNTER"), *Keeper.ToUpper());
}

FString ATownGameMode::GetShopPurse() const
{
	if (bAtBook)
	{
		const FGutStableState State = GetOwnerState();

		return FString::Printf(TEXT("As an owner you hold %.0f L  \u00B7  yard: %d of %d"), State.Water, State.Fighters.Num(), GetCageRules() ? GetCageRules()->GetStableMostFighters() : 0);
	}

	if (bAtBag)
	{
		const UGutGameInstance* GameInstance = GetGutGameInstance();

		// what the fighter has in it already, for the fight ahead
		return (GameInstance && GameInstance->bLadderStarted && (GameInstance->Ladder.bFed || GameInstance->Ladder.bSteadied))
			? FString::Printf(TEXT("For your next fight: %s"), GameInstance->Ladder.bFed && GameInstance->Ladder.bSteadied ? TEXT("fed, and steadied") : (GameInstance->Ladder.bFed ? TEXT("fed") : TEXT("steadied")))
			: FString();
	}

	if (bAtStall)
	{
		FString Whose;

		const float* Held = GetMarketWater(&Whose);

		const UGutGameInstance* Run = GetGutGameInstance();

		if (Held && Run && Run->IsOnePurse())
		{
			return FString::Printf(TEXT("You hold %s"), *TownConstants::Litres(FMath::Max(*Held, 0.0f)));
		}

		return Held ? FString::Printf(TEXT("You hold %s (%s)"), *TownConstants::Litres(*Held), *Whose) : FString();
	}

	// in the yard it is an owner's water that is spent
	if (const UGutGameInstance* GameInstance = GetGutGameInstance(); bAtYardPage && GameInstance)
	{
		return GameInstance->Stable.Debt > 0.5f
			? FString::Printf(TEXT("You hold %.0f L  \u00B7  the yard owes %.0f L  \u00B7  day %d"), GameInstance->Stable.Water, GameInstance->Stable.Debt, GameInstance->Stable.Day)
			: FString::Printf(TEXT("You hold %.0f L  \u00B7  day %d"), GameInstance->Stable.Water, GameInstance->Stable.Day);
	}

	return HasFighter() ? FString::Printf(TEXT("You hold %.0f L"), GetShopWater()) : FString();
}

const FTownCall* ATownGameMode::GetCall() const
{
	if (Auction.bOn)
	{
		return &AuctionCall;
	}

	return (bSettled && PlaceData && PlaceData->Calls.IsValidIndex(CallIndex)) ? &PlaceData->Calls[CallIndex] : nullptr;
}

FString ATownGameMode::GetPrompt() const
{
	if (!bSettled || !PlaceData || IsTalking() || bShopping || bStoppedMenu)
	{
		return FString();
	}

	if (PlaceData->People.IsValidIndex(PersonInReach))
	{
		const FTownPerson& Person = PlaceData->People[PersonInReach];

		return Person.Prompt.IsEmpty() ? FString::Printf(TEXT("Talk to %s"), *Person.Name) : Person.Prompt;
	}

	if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && YardFighters.IsValidIndex(YardInReach) && GameInstance->Stable.Fighters.IsValidIndex(YardFighters[YardInReach]))
	{
		return FString::Printf(TEXT("See to %s: train, mend, or take %s with you"), *GameInstance->Stable.Fighters[YardFighters[YardInReach]].FighterName.ToString(),
			UGutSoundSubsystem::WhoIs(this, GameInstance->Stable.Fighters[YardFighters[YardInReach]].Body).Him());
	}

	if (PlaceData->Finds.IsValidIndex(FindInReach))
	{
		const TownConstants::FGood* Good = TownConstants::FindGood(PlaceData->Finds[FindInReach].Good);

		return Good ? FString::Printf(TEXT("Pick up: %s"), *FString(Good->Name).ToLower()) : FString();
	}

	if (PlaceData->Doors.IsValidIndex(DoorInReach))
	{
		const FTownDoor& Door = PlaceData->Doors[DoorInReach];

		// what the day would cost is said before it is sat out, and whether it can be paid
		if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && Door.Deed == ETownDeed::Rest && !GameInstance->Stable.Fighters.IsEmpty())
		{
			const float Due = GetDayDue();

			return FString::Printf(TEXT("Sit out the day on the crate: feed and wages are %.0f L, and you hold %.0f%s"), Due, GameInstance->Stable.Water,
				Due > GameInstance->Stable.Water ? TEXT(". It cannot be paid: whoever is on a wage walks") : TEXT(""));
		}

		// somebody dead on the pit boss's table is said at his hatch, before it is knocked at
		if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && Door.Deed == ETownDeed::Pit && GameInstance->Stable.bBodyWaiting)
		{
			return FString::Printf(TEXT("Knock at the hatch: %s's body first, then the fights"), *GameInstance->Stable.BodyOf.ToString());
		}

		return Door.Prompt;
	}

	return FString();
}

float ATownGameMode::GetDayDue() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const AGutGameMode* Rules = GetCageRules();

	float Due = 0.0f;

	if (GameInstance && Rules)
	{
		for (const FGutStableFighter& Fighter : GameInstance->Stable.Fighters)
		{
			Due += Rules->GetFeedPerDay() + (Fighter.bOwned ? 0.0f : Fighter.Wage);
		}
	}

	return Due;
}

FString ATownGameMode::SettleAfterDay(const TArray<FGutStableFighter>& Before, float DebtBefore)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return FString();
	}

	const TArray<FGutStableFighter>& Now = GameInstance->Stable.Fighters;

	// Whoever is lost is taken out of the list and the rest keep their order, so the two lists are walked together
	TArray<int32> Goes;
	FString Lost;

	int32 Next = 0;

	for (const FGutStableFighter& Was : Before)
	{
		if (Now.IsValidIndex(Next) && Now[Next].FighterName.EqualTo(Was.FighterName))
		{
			Goes.Add(Next++);
			continue;
		}

		Goes.Add(INDEX_NONE);

		Lost += TEXT(" ") + Was.FighterName.ToString() + (Was.bOwned ? TEXT(" is gone in the night.") : TEXT(" was not paid, and has walked."));

		UE_LOG(LogTown, Log, TEXT("Yard: %s is lost with the day, and leaves the place"), *Was.FighterName.ToString());
	}

	const auto GoesTo = [&Goes](int32 Was) { return Goes.IsValidIndex(Was) ? Goes[Was] : INDEX_NONE; };

	for (int32 i = YardFighters.Num() - 1; i >= 0; --i)
	{
		YardFighters[i] = GoesTo(YardFighters[i]);

		if (YardFighters[i] == INDEX_NONE)
		{
			if (YardBodies.IsValidIndex(i) && YardBodies[i])
			{
				YardBodies[i]->Destroy();
			}

			YardBodies.RemoveAt(i);
			YardFighters.RemoveAt(i);
		}
	}

	for (int32 i = Tow.Num() - 1; i >= 0; --i)
	{
		Tow[i].Fighter = GoesTo(Tow[i].Fighter);

		if (Tow[i].Fighter == INDEX_NONE)
		{
			if (TowBodies.IsValidIndex(i) && TowBodies[i])
			{
				TowBodies[i]->Destroy();
			}

			Tow.RemoveAt(i);

			if (TowBodies.IsValidIndex(i))
			{
				TowBodies.RemoveAt(i);
			}
		}
	}

	YardFocus = GoesTo(YardFocus);
	YardInReach = INDEX_NONE;

	// the day goes by in the dark, which is also where somebody leaves
	if (APlayerController* Hands = UGameplayStatics::GetPlayerController(this, 0); Hands && Hands->PlayerCameraManager)
	{
		Hands->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.6f, FLinearColor::Black, false, false);
	}

	const FString Paid = GameInstance->Stable.Debt > DebtBefore + 0.01f
		? FString::Printf(TEXT("There was not enough for feed and wages: %.0f L is owed now, and you hold nothing."), GameInstance->Stable.Debt)
		: FString::Printf(TEXT("Everybody is fed and paid, and you hold %.0f L."), GameInstance->Stable.Water);

	return Paid + Lost;
}

void ATownGameMode::LayFinds()
{
	FindActors.Reset();
	FindInReach = INDEX_NONE;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!PlaceData || !GameInstance)
	{
		return;
	}

	int32 Lying = 0;

	for (int32 i = 0; i < PlaceData->Finds.Num(); ++i)
	{
		const FTownFind& Find = PlaceData->Finds[i];

		// one place kept for each, so that the finds and what stands for them stay in step
		FindActors.Add(nullptr);

		if (GameInstance->Found.Contains(GetFindName(i)) || !TownConstants::FindGood(Find.Good))
		{
			continue;
		}

		// its model: whichever mesh was brought in under the good's own folder
		const FString Model = FString(TownConstants::FindModel) + Find.Good.ToString();

		UStaticMesh* Mesh = nullptr;

		TArray<FAssetData> Assets;

		if (const IAssetRegistry* Registry = IAssetRegistry::Get())
		{
			Registry->GetAssetsByPath(FName(*Model), Assets, true);
		}

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
			{
				Mesh = Cast<UStaticMesh>(Asset.GetAsset());
				break;
			}
		}

		if (!Mesh)
		{
			// nothing to see is nothing to find
			UE_LOG(LogTown, Warning, TEXT("Street: no model for a find, %s"), *Model);
			continue;
		}

		// on whatever is under it there
		FVector Where = Find.Place;
		FHitResult Hit;

		if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.0f, 0.0f, 150.0f), Where - FVector(0.0f, 0.0f, 300.0f), ECC_WorldStatic))
		{
			Where = Hit.ImpactPoint;
		}

		// its model stands on its own lowest point
		Where.Z -= Mesh->GetBoundingBox().Min.Z;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* Thing = GetWorld()->SpawnActor<AStaticMeshActor>(Where, FRotator(0.0f, Find.Facing, 0.0f), Params);

		if (!Thing)
		{
			continue;
		}

		Thing->SetMobility(EComponentMobility::Movable);
		Thing->GetStaticMeshComponent()->SetStaticMesh(Mesh);

		// nobody trips on a tin
		Thing->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FindActors[i] = Thing;
		++Lying;
	}

	UE_LOG(LogTown, Log, TEXT("Street: %d of %d things lie about to be found"), Lying, PlaceData->Finds.Num());
}

TArray<FVector> ATownGameMode::GetFindsLying() const
{
	TArray<FVector> Places;

	for (const TObjectPtr<AActor>& Thing : FindActors)
	{
		if (Thing)
		{
			FVector Middle;
			FVector Size;

			Thing->GetActorBounds(false, Middle, Size);

			Places.Add(Middle + FVector(0.0f, 0.0f, Size.Z * 0.6f));
		}
	}

	return Places;
}

FName ATownGameMode::GetFindName(int32 Find) const
{
	return FName(*FString::Printf(TEXT("%s:%d"), PlaceData ? *PlaceData->Name : TEXT("nowhere"), Find));
}

bool ATownGameMode::TakeFind(int32 Find)
{
	UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance || !PlaceData || !PlaceData->Finds.IsValidIndex(Find) || !FindActors.IsValidIndex(Find) || !FindActors[Find])
	{
		return false;
	}

	const TownConstants::FGood* Good = TownConstants::FindGood(PlaceData->Finds[Find].Good);

	if (!Good)
	{
		return false;
	}

	FindActors[Find]->Destroy();
	FindActors[Find] = nullptr;
	FindInReach = INDEX_NONE;

	GameInstance->Found.AddUnique(GetFindName(Find));
	GameInstance->AddToBag(FName(Good->Id), 1);
	GameInstance->SaveRun();

	Notice = FString::Printf(TEXT("%s. It is in your bag: %s."), Good->Name, Good->Detail);
	NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::NoticeTime;

	if (UGutSoundSubsystem* Sounds = UGutSoundSubsystem::Get(this))
	{
		Sounds->Play(FName("cash"), FVector::ZeroVector, 0.5f);
	}

	UE_LOG(LogTown, Log, TEXT("Street: picked up %s"), Good->Name);

	return true;
}

FString ATownGameMode::GetSpeaker() const
{
	return IsTalking() ? PlaceData->People[TalkingTo].Name : FString();
}

FString ATownGameMode::GetWords() const
{
	if (!IsTalking() || !PlaceData->People[TalkingTo].Lines.IsValidIndex(TalkLine))
	{
		return FString();
	}

	return Told(PlaceData->People[TalkingTo].Lines[TalkLine].Words);
}

FString ATownGameMode::GetTalkNote() const
{
	if (!IsTalking() || !PlaceData->People[TalkingTo].Lines.IsValidIndex(TalkLine))
	{
		return FString();
	}

	const FString Noted = Told(PlaceData->People[TalkingTo].Lines[TalkLine].Note);

	// and what the last thing done came to: who thinks better or worse of the player for it, and what was paid
	if (!TalkTold.IsEmpty() && TalkToldLine == TalkLine)
	{
		return Noted.IsEmpty() ? TalkTold : Noted + TEXT("   \u00B7   ") + TalkTold;
	}

	return Noted;
}

FString ATownGameMode::GetGoodName(const FName& Good)
{
	const TownConstants::FGood* Known = TownConstants::FindGood(Good);

	return Known ? FString(Known->Name) : Good.ToString();
}

TArray<FString> ATownGameMode::GetAnswers() const
{
	TArray<FString> Result;

	if (const FTownLine* Asked = GetAnsweredLine())
	{
		// nothing is offered at a line that only sends the talk on
		for (const int32 Which : IsSwitch(*Asked) ? TArray<int32>() : GetShownAnswers(*Asked))
		{
			Result.Add(Told(Asked->Answers[Which].Words));
		}
	}

	return Result;
}

TArray<bool> ATownGameMode::GetAnswersAsked() const
{
	TArray<bool> Result;

	if (const FTownLine* Asked = GetAnsweredLine())
	{
		const int32 Line = static_cast<int32>(Asked - PlaceData->People[TalkingTo].Lines.GetData());

		for (const int32 Which : IsSwitch(*Asked) ? TArray<int32>() : GetShownAnswers(*Asked))
		{
			Result.Add(TalkAsked.Contains((Line * 100) + Which));
		}
	}

	return Result;
}

void ATownGameMode::ShowNotice(const FString& Words)
{
	Notice = Words;
	NoticeUntil = GetWorld()->GetTimeSeconds() + TownConstants::NoticeTime;
}

FString ATownGameMode::GetNotice() const
{
	return (!Notice.IsEmpty() && GetWorld()->GetTimeSeconds() < NoticeUntil) ? Notice : FString();
}

TArray<FString> ATownGameMode::GetPurses() const
{
	TArray<FString> Lines;

	const UGutGameInstance* GameInstance = GetGutGameInstance();

	if (!GameInstance)
	{
		return Lines;
	}

	// one purse: what is held, and then whoever is owed
	if (GameInstance->IsOnePurse())
	{
		Lines.Add(GameInstance->GetPurse() >= 0.0f ? FString::Printf(TEXT("You hold %s"), *TownConstants::Litres(GameInstance->GetPurse()))
			: FString::Printf(TEXT("You hold nothing, and are %.0f L down on the pit boss's slate"), -GameInstance->GetPurse()));

		const auto Owed = [&Lines](float Debt, const TCHAR* Whom)
		{
			if (Debt > 0.5f)
			{
				Lines.Add(FString::Printf(TEXT("Owing %s:  %.0f L"), Whom, Debt));
			}
		};

		Owed(GameInstance->bLadderStarted ? GameInstance->Ladder.Debt : 0.0f, TEXT("the pit boss"));
		Owed(GameInstance->Window.bStarted ? GameInstance->Window.Debt : 0.0f, TEXT("Mother Ledger"));
		Owed(GameInstance->Stable.bStarted ? GameInstance->Stable.Debt : 0.0f, TEXT("on your yard"));

		return Lines;
	}

	// one way in, and the line is what it always was. More than one, and each says whose water it is.
	const int32 Ways = (GameInstance->bLadderStarted ? 1 : 0) + (GameInstance->Window.bStarted ? 1 : 0) + (GameInstance->Stable.bStarted ? 1 : 0);

	auto Line = [&Lines, Ways](const TCHAR* As, float Water, float Debt, const TCHAR* Whom)
	{
		FString Text = Ways > 1 ? FString::Printf(TEXT("%s:  %.0f L in hand"), As, Water) : FString::Printf(TEXT("%.0f L in hand"), Water);

		if (Debt > 0.5f)
		{
			Text += FString::Printf(TEXT("   \u00B7   owing %s%.0f L"), Whom, Debt);
		}

		Lines.Add(Text);
	};

	if (GameInstance->bLadderStarted)
	{
		Line(TEXT("Fighting"), GameInstance->Ladder.Water, GameInstance->Ladder.Debt, Ways > 1 ? TEXT("the pit boss ") : TEXT(""));
	}

	if (GameInstance->Window.bStarted)
	{
		Line(TEXT("Betting"), GameInstance->Window.Water, GameInstance->Window.Debt, TEXT("Mother Ledger "));
	}

	if (GameInstance->Stable.bStarted)
	{
		Line(TEXT("Owning"), GameInstance->Stable.Water, GameInstance->Stable.Debt, TEXT(""));
	}

	return Lines;
}

void ATownGameMode::SetStopped(bool bStopped)
{
	if (bStopped == bStoppedMenu)
	{
		return;
	}

	bStoppedMenu = bStopped;
	StoppedChoice = 0;

	UGameplayStatics::SetGamePaused(this, bStoppedMenu);
}

TArray<FString> ATownGameMode::GetStoppedLines() const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();

	static const TCHAR* Loud[] = { TEXT("off"), TEXT("a third"), TEXT("two thirds"), TEXT("full") };

	static const TCHAR* Said[] = { TEXT("heard and written"), TEXT("written only"), TEXT("heard, with only your answers written") };

	return { TEXT("Go on"), TEXT("Your bag"), TEXT("The map"), FString::Printf(TEXT("Sound: %s"), Loud[FMath::Clamp(GameInstance ? GameInstance->Loudness : 3, 0, 3)]),
		FString::Printf(TEXT("What people say: %s"), Said[FMath::Clamp(GameInstance ? GameInstance->Speech : 0, 0, 2)]), TEXT("Leave the game") };
}
