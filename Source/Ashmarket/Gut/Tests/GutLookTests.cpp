// Ashmarket prototype: the Gut.

#include "GutTestHelpers.h"
#include "Animation/SkeletalMeshActor.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformNonRealtime.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GutOpponentController.h"
#include "GutBrainController.h"
#include "GutCrowd.h"
#include "GutArmorData.h"
#include "GutLadderData.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Serialization/JsonSerializer.h"

/**
 *  Not a test of anything: a walk round the cage with a camera, for judging the art.
 *
 *  Ashmarket.Look.Cage    stands the two fighters in the middle with a full deck above them and saves a picture from each
 *                         of a handful of places, with no HUD, to for-review/screenshots/look_*.png
 *
 *  It also serves to read the frame rate off screen: add -GutFrameRate -GutLookLinger=20 and run it in real time.
 *
 *  It is kept out of Ashmarket.Gut so that the test run does not include it:
 *  GUT_TESTS=Ashmarket.Look Tools/run_gut_tests.sh --render
 */
namespace GutLookTests
{
	using namespace GutTests;

	struct FView
	{
		const TCHAR* Name;
		FVector From;
		FVector At;
		float FieldOfView;

		/** If set, At is that bone of the player's body and From is measured from it: X in front of the player, Y to its right */
		FName Bone = NAME_None;
	};

	/** The places a picture is taken from. The pool is 14 m by 10 m and 3.2 m deep, its middle at the origin. */
	const FView Views[] =
	{
		{ TEXT("look_01_from_the_corner"), FVector(-620.0f, -420.0f, 175.0f), FVector(60.0f, 40.0f, 120.0f), 75.0f },
		{ TEXT("look_02_fighters_close"), FVector(-210.0f, -150.0f, 150.0f), FVector(40.0f, 0.0f, 130.0f), 55.0f },
		{ TEXT("look_03_long_wall"), FVector(250.0f, -380.0f, 160.0f), FVector(-300.0f, 500.0f, 150.0f), 70.0f },
		{ TEXT("look_04_up_at_the_deck"), FVector(-120.0f, -60.0f, 120.0f), FVector(500.0f, 500.0f, 480.0f), 80.0f },
		{ TEXT("look_05_floor"), FVector(380.0f, 260.0f, 230.0f), FVector(-120.0f, -80.0f, 0.0f), 65.0f },
		{ TEXT("look_06_from_the_deck"), FVector(-900.0f, -700.0f, 560.0f), FVector(0.0f, 0.0f, 60.0f), 60.0f },
		{ TEXT("look_16_over_the_rim"), FVector(-300.0f, -350.0f, 140.0f), FVector(300.0f, 900.0f, 520.0f), 80.0f },
		{ TEXT("look_17_over_the_end"), FVector(-450.0f, 100.0f, 150.0f), FVector(900.0f, -300.0f, 520.0f), 80.0f },
		{ TEXT("look_18_the_freeway"), FVector(200.0f, 380.0f, 150.0f), FVector(-200.0f, -900.0f, 560.0f), 75.0f },
		{ TEXT("look_19_from_above"), FVector(-1700.0f, 1500.0f, 1500.0f), FVector(0.0f, -200.0f, 250.0f), 65.0f },
		{ TEXT("look_07_face"), FVector(70.0f, -22.0f, 4.0f), FVector(0.0f, 0.0f, 6.0f), 30.0f, FName("head") },
		{ TEXT("look_08_hand"), FVector(55.0f, 40.0f, 18.0f), FVector(0.0f, 0.0f, -4.0f), 28.0f, FName("hand_r") },
		{ TEXT("look_09_hand_palm"), FVector(45.0f, -40.0f, -25.0f), FVector(0.0f, 0.0f, -6.0f), 28.0f, FName("hand_r") },
	};

	void LookFrom(const FFight& Fight, const FView& View)
	{
		UWorld* World = Fight.GameMode->GetWorld();
		APlayerController* Controller = Cast<APlayerController>(Fight.Player->GetController());

		if (!Controller)
		{
			return;
		}

		FVector From = View.From;
		FVector At = View.At;

		if (View.Bone != NAME_None)
		{
			const FVector Bone = Fight.Player->GetShownMesh()->GetBoneLocation(View.Bone);
			const FRotator Facing(0.0f, Fight.Player->GetActorRotation().Yaw, 0.0f);

			At = Bone + Facing.RotateVector(View.At);
			From = Bone + Facing.RotateVector(View.From);
		}

		ACameraActor* Camera = World->SpawnActor<ACameraActor>(From, (At - From).Rotation());

		Camera->GetCameraComponent()->SetFieldOfView(View.FieldOfView);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

		Controller->SetViewTarget(Camera);

		if (AHUD* HUD = Controller->GetHUD())
		{
			HUD->bShowHUD = false;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLookCageTest, "Ashmarket.Look.Cage", GutTests::Flags)

bool FGutLookCageTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		if (AGutCrowd* Crowd = Fight.GameMode->GetCrowd())
		{
			Crowd->Populate(100000);
		}

		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();
	} });

	// the deck takes a while to fill, a couple of people a frame
	Steps.Add({ 3.0f, [](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		// -GutLookWeapon=Chain on the command line puts that weapon in the player's hand, to see how the hand holds it
		FString Wanted;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutLookWeapon="), Wanted))
		{
			if (UGutWeaponData* Weapon = FindRackWeapon(Fight, Wanted))
			{
				Fight.Player->EquipWeapon(Weapon);
			}
		}
	} });

	for (const FView& View : Views)
	{
		Steps.Add({ 0.2f, [View](const FFight& Fight) { LookFrom(Fight, View); } });

		// long enough for the picture to settle after the cut
		Steps.Add({ 1.2f, [View](const FFight& Fight) { Screenshot(View.Name); } });
	}

	// then into a corner, where the lamps reach least, to see what skin does in the dark: once standing and once swinging
	Steps.Add({ 0.2f, [](const FFight& Fight)
	{
		PlaceAt(Fight.Player, -470.0f, -300.0f, 0.0f);
		PlaceAt(Fight.Enemy, -330.0f, -300.0f, 180.0f);

		static const FView Corner = { TEXT("look_10_corner"), FVector(-560.0f, -120.0f, 150.0f), FVector(-400.0f, -300.0f, 120.0f), 50.0f };

		LookFrom(Fight, Corner);
	} });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Screenshot(TEXT("look_10_corner")); } });
	Steps.Add({ 0.1f, [](const FFight& Fight) { Fight.Player->TryHeavyAttack(); } });
	Steps.Add({ 0.35f, [](const FFight& Fight) { Screenshot(TEXT("look_11_corner_swinging")); } });

	// and the same corner from as far off as a fight is usually seen, standing and then walking: skin is drawn differently at a distance
	Steps.Add({ 1.0f, [](const FFight& Fight)
	{
		static const FView Far = { TEXT("look_12_corner_far"), FVector(-420.0f, 180.0f, 165.0f), FVector(-400.0f, -300.0f, 110.0f), 62.0f };

		LookFrom(Fight, Far);
	} });

	Steps.Add({ 1.5f, [](const FFight& Fight) { Screenshot(TEXT("look_12_corner_far")); } });
	// the opponent is let off its lead from the far end of the pool, and seen walking in and fighting from where the camera stands
	Steps.Add({ 0.1f, [](const FFight& Fight)
	{
		PlaceAt(Fight.Enemy, 300.0f, -300.0f, 180.0f);

		if (AGutBrainController* Brain = Cast<AGutBrainController>(Fight.Enemy->GetController()))
		{
			Brain->SetBrainEnabled(true);
		}
	} });

	Steps.Add({ 1.2f, [](const FFight& Fight) { Screenshot(TEXT("look_13_walking_in")); } });
	Steps.Add({ 0.9f, [](const FFight& Fight) { Screenshot(TEXT("look_14_walking_in")); } });
	Steps.Add({ 1.1f, [](const FFight& Fight) { Screenshot(TEXT("look_15_fighting")); } });

	// -GutLookLinger=20 on the command line stays on the widest view that many seconds more, for reading the frame rate with -GutFrameRate
	float Linger = 0.0f;
	FParse::Value(FCommandLine::Get(), TEXT("GutLookLinger="), Linger);

	Steps.Add({ 0.5f, [](const FFight& Fight) { LookFrom(Fight, Views[0]); } });
	Steps.Add({ Linger, [](const FFight& Fight) {} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

namespace GutLookTests
{
	/**
	 *  Films a fight: a camera that stands off to one side of the two fighters and keeps both in the picture,
	 *  and a picture saved every other frame until a few seconds after one of them is dead.
	 */
	class FFilmFightCommand : public IAutomationLatentCommand
	{
	public:

		explicit FFilmFightCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return true;
			}

			UWorld* World = Fight.GameMode->GetWorld();
			const float Now = World->GetTimeSeconds();

			if (StartTime < 0.0f)
			{
				StartTime = Now;

				// what is heard is kept from the same moment as what is seen, so the two can be joined
				if (bSound && World->GetAudioDeviceRaw())
				{
					// the sound is worked out here, a frame's worth for each frame of the film. Left to itself the engine works
					// out twice as much as the game has lived through, and the film's sound runs at half the speed of its picture.
					if (IConsoleVariable* Itself = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick")))
					{
						Itself->Set(0);
					}

					UAudioMixerBlueprintLibrary::StartRecordingOutput(World, MaxSeconds + 10.0f);

					bRecording = true;
				}
			}

			if (bRecording)
			{
				Audio::FMixerDevice* Mixer = static_cast<Audio::FMixerDevice*>(World->GetAudioDeviceRaw());
				Audio::IAudioMixerPlatformInterface* Platform = Mixer ? Mixer->GetAudioMixerPlatform() : nullptr;

				if (Platform && Platform->IsNonRealtime())
				{
					static_cast<Audio::FMixerPlatformNonRealtime*>(Platform)->RenderAudio(World->GetDeltaSeconds());
				}
			}

			if (EndTime < 0.0f && IsFightOver(Fight))
			{
				EndTime = Now;
			}

			// the middle of the two of them, and a place to one side of the line between them
			// with more than two in the cage, the one the player's fighter is on is the one to keep in the picture
			// (but not a dog let in on a fight between two people: the picture stays on the two people)
			const AGutFighter* Facing = (Fight.GameMode->GetSpecialCard() != EGutSpecialCard::None && Fight.Player->GetOpponent() && Fight.Player->GetOpponent()->IsAlive() && Fight.Player->IsAlive())
				? Fight.Player->GetOpponent() : Fight.Enemy;

			const FVector Player = Fight.Player->GetActorLocation();
			const FVector Enemy = Facing->GetActorLocation();
			const FVector Middle = (Player + Enemy) * 0.5f;
			const float Apart = FVector::Dist2D(Player, Enemy);

			FVector Side = FVector::CrossProduct((Enemy - Player).GetSafeNormal2D(), FVector::UpVector);

			if (Side.IsNearlyZero())
			{
				Side = FVector::RightVector;
			}

			// once it is over the one on the floor is half the picture: further back and aimed lower, or the body is cut off at the bottom
			const bool bOver = EndTime >= 0.0f;

			const float Back = FMath::Clamp(250.0f + (Apart * 0.7f), 300.0f, 560.0f) + (bOver ? 130.0f : 0.0f);

			// inside the pool, at about head height
			const auto Inside = [](FVector Place)
			{
				Place.X = FMath::Clamp(Place.X, -650.0f, 650.0f);
				Place.Y = FMath::Clamp(Place.Y, -450.0f, 450.0f);
				Place.Z = 165.0f;

				return Place;
			};

			// whichever side the camera is already on, so it does not jump across when they turn about each other
			FVector Wanted = Inside(Middle + (Side * Back));
			FVector Other = Inside(Middle - (Side * Back));

			if (Camera && FVector::DistSquared2D(Other, Camera->GetActorLocation()) < FVector::DistSquared2D(Wanted, Camera->GetActorLocation()))
			{
				Swap(Wanted, Other);
			}

			// unless that side has no room left: with the two of them against a wall the camera was held in the pool a pace
			// from them and the picture was a chest. Then the film cuts to the other side, as a film would.
			bool bCut = false;

			if (FVector::Dist2D(Wanted, Middle) < Back * 0.6f && FVector::Dist2D(Other, Middle) > Back * 0.85f)
			{
				Wanted = Other;
				bCut = Camera != nullptr;
			}
			else if (FVector::Dist2D(Wanted, Middle) < Back * 0.6f)
			{
				// in a corner neither side has room: from out toward the middle of the pool
				const FVector Out = (FVector::ZeroVector - Middle).GetSafeNormal2D();

				Wanted = Inside(Middle + (Out * Back));
				bCut = Camera && FVector::Dist2D(Camera->GetActorLocation(), Middle) < Back * 0.5f;
			}

			// with more than two people in the cage, one of the others can be standing between the camera and the two it is
			// filming: the first seconds of a film of three against one were somebody's back. If that is so on this side and
			// not on the other, and the other has room, the film cuts across. Not more often than every few seconds.
			const auto InTheWay = [&](const FVector& From)
			{
				const FVector Along = (Middle - From).GetSafeNormal2D();
				const float Length = FVector::Dist2D(Middle, From);

				for (TActorIterator<AGutFighter> It(World); It; ++It)
				{
					const AGutFighter* Somebody = *It;

					// an animal is too low to hide anybody, and a body on the floor lower still
					if (Somebody == Fight.Player || Somebody == Facing || !Somebody->IsAlive() || Somebody->GetBeastBody())
					{
						continue;
					}

					const FVector To = Somebody->GetActorLocation() - From;
					const float Far = FVector::DotProduct(To, Along);

					if (Far > 30.0f && Far < Length * 0.85f && (To - (Along * Far)).Size2D() < 75.0f)
					{
						return true;
					}
				}

				return false;
			};

			if (!bCut && Now - LastCutTime > 3.0f && FVector::Dist2D(Other, Middle) > Back * 0.6f && InTheWay(Camera ? Camera->GetActorLocation() : Wanted) && !InTheWay(Other))
			{
				Wanted = Other;
				bCut = Camera != nullptr;
			}

			if (bCut)
			{
				LastCutTime = Now;
			}

			const FVector At = FVector(Middle.X, Middle.Y, bOver ? 78.0f : 110.0f);

			if (!Camera)
			{
				Camera = World->SpawnActor<ACameraActor>(Wanted, (At - Wanted).Rotation());
				Camera->GetCameraComponent()->SetFieldOfView(62.0f);
				Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

				LookAt = At;
			}
			else
			{
				// it follows, it does not snap: a sixtieth of a second a frame, as the film is shot
				const float Step = 1.0f / 60.0f;

				LookAt = FMath::VInterpTo(LookAt, At, Step, 4.0f);

				if (bCut)
				{
					LookAt = At;
				}

				const FVector Place = bCut ? Wanted : FMath::VInterpTo(Camera->GetActorLocation(), Wanted, Step, 2.0f);

				Camera->SetActorLocationAndRotation(Place, (LookAt - Place).Rotation());

				// the pool is only so wide: when the two of them are further apart than the picture is from where the camera can
				// stand, the picture is made wider until both are in it (they were being left out of it at both edges)
				const float Away = FMath::Max(FVector::Dist2D(Place, Middle), 100.0f);
				const float Needed = 2.0f * FMath::RadiansToDegrees(FMath::Atan(((Apart * 0.5f) + 95.0f) / Away));

				UCameraComponent* Lens = Camera->GetCameraComponent();

				Lens->SetFieldOfView(bCut ? FMath::Clamp(Needed, 62.0f, 104.0f) : FMath::FInterpTo(Lens->FieldOfView, FMath::Clamp(Needed, 62.0f, 104.0f), Step, 2.5f));
			}

			// a player controller with no fighter keeps taking the view back for itself: take it again
			if (APlayerController* Viewer = World->GetFirstPlayerController(); Viewer && Viewer->GetViewTarget() != Camera)
			{
				Viewer->bAutoManageActiveCameraTarget = false;
				Viewer->SetViewTarget(Camera);

				UE_LOG(LogTemp, Display, TEXT("[film] took the view back at frame %d"), Frame);
			}

			if (bTrace)
			{
				Trace(Fight, Now);
			}

			if (Frame++ % Every == 0)
			{
				const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm") / FString::Printf(TEXT("frame_%05d.png"), Saved++));

				// read straight from the game's picture. Asked for with the window's own interface on top, a screenshot makes the window
				// draw itself again, and taken every other frame that leaves skin and shadow speckled in every picture: the renderer
				// builds each frame on the ones before it and never gets the chance.
				FScreenshotRequest::RequestScreenshot(Path, false, false);
			}

			// long enough after the end to see who is on the floor, and never longer than the film is worth
			const bool bDone = (EndTime >= 0.0f && Now - EndTime > 8.0f) || Now - StartTime > MaxSeconds;

			if (bDone && bRecording)
			{
				bRecording = false;

				UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(World, EAudioRecordingExportType::WavFile, TEXT("sound"),
					FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutFilm")));
			}

			return bDone;
		}

		float MaxSeconds = 120.0f;

		/** A picture is saved every this many frames: 2 makes a film of 30 frames a second */
		int32 Every = 2;

		/** If true, what is heard is kept as well, as Saved/GutFilm/sound.wav. Needs the run to have sound: GUT_SOUND=1. */
		bool bSound = false;

		/** If true, where each fighter's bones are is written down every frame, to Saved/GutTrace.csv, for finding what jerks */
		bool bTrace = false;

	private:

		/** One line for each fighter: what it is doing, what plays on it, and where its body is, on the skeleton that fights and on the one seen */
		void Trace(const FFight& Fight, float Now)
		{
			static const FName Bones[] = { FName("pelvis"), FName("head"), FName("hand_l"), FName("hand_r"), FName("foot_l"), FName("foot_r") };

			if (TraceLines.IsEmpty())
			{
				TraceLines.Add(TEXT("frame,time,who,state,statetime,x,y,z,yaw,vx,vy,clip,cliptime,clipweight,move,neckgap,ownneckgap,"
					"pelvis_x,pelvis_y,pelvis_z,head_x,head_y,head_z,hand_l_x,hand_l_y,hand_l_z,hand_r_x,hand_r_y,hand_r_z,foot_l_x,foot_l_y,foot_l_z,foot_r_x,foot_r_y,foot_r_z,"
					"s_pelvis_x,s_pelvis_y,s_pelvis_z,s_head_x,s_head_y,s_head_z,s_hand_l_x,s_hand_l_y,s_hand_l_z,s_hand_r_x,s_hand_r_y,s_hand_r_z,s_foot_l_x,s_foot_l_y,s_foot_l_z,s_foot_r_x,s_foot_r_y,s_foot_r_z"));
			}

			int32 Who = 0;

			for (const AGutFighter* Fighter : { Fight.Player, Fight.Enemy })
			{
				const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
				const FAnimMontageInstance* Playing = Anim ? Anim->GetActiveMontageInstance() : nullptr;

				FString Clip = TEXT("-");

				if (Playing && Playing->Montage && !Playing->Montage->SlotAnimTracks.IsEmpty() && !Playing->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty())
				{
					const UAnimSequenceBase* Source = Playing->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();

					Clip = GetNameSafe(Source);
				}

				const FVector Place = Fighter->GetActorLocation();
				const FVector Going = Fighter->GetVelocity();

				FString Line = FString::Printf(TEXT("%d,%.4f,%d,%d,%.4f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%s,%.4f,%.3f,%.1f,%.1f,%.1f"),
					Frame, Now, Who++, static_cast<int32>(Fighter->GetFighterState()), Fighter->GetStateTime(), Place.X, Place.Y, Place.Z, Fighter->GetActorRotation().Yaw,
					Going.X, Going.Y, *Clip, Playing ? Playing->GetPosition() : 0.0f, Playing ? Playing->GetWeight() : 0.0f, Going.Size2D(), Fighter->GetWeaponNeckGap(), Fighter->GetWeaponNeckGap(true));

				for (const USkeletalMeshComponent* Mesh : { static_cast<const USkeletalMeshComponent*>(Fighter->GetMesh()), static_cast<const USkeletalMeshComponent*>(Fighter->GetShownMesh()) })
				{
					for (const FName& Bone : Bones)
					{
						const FVector At = Mesh ? Mesh->GetBoneLocation(Bone) : FVector::ZeroVector;

						Line += FString::Printf(TEXT(",%.2f,%.2f,%.2f"), At.X, At.Y, At.Z);
					}
				}

				TraceLines.Add(MoveTemp(Line));
			}

			// kept on disk as it goes, so a run that is cut short still leaves what it saw
			if (Frame % 120 == 0)
			{
				FFileHelper::SaveStringArrayToFile(TraceLines, *(FPaths::ProjectSavedDir() / TEXT("GutTrace.csv")));
			}
		}

		TArray<FString> TraceLines;
		bool bRecording = false;

		FAutomationTestBase* Test;
		ACameraActor* Camera = nullptr;
		FVector LookAt = FVector::ZeroVector;

		/** When the film last cut from one side of the fight to the other */
		float LastCutTime = -100.0f;
		float StartTime = -1.0f;
		float EndTime = -1.0f;
		int32 Frame = 0;
		int32 Saved = 0;
	};
}

/**
 *  Ashmarket.Fit.Collars   puts each body the ladders use on the player's fighter in turn, in a collar, and saves two
 *                          pictures of its neck to for-review/screenshots/collar_*.png: for judging how the ring sits
 *
 *  GUT_TESTS=Ashmarket.Fit Tools/run_gut_tests.sh --render
 */
/**
 *  Ashmarket.Look.Blood    cuts and beats the player's body where it stands, a few blows at a time, and saves pictures of
 *                          it from in front, the side and behind after each lot, and one of whoever did it:
 *                          for-review/screenshots/blood_*.png. For judging the blood on skin, which a fight hides behind arms.
 *
 *  GUT_TESTS=Ashmarket.Look.Blood Tools/run_gut_tests.sh --render      -GutLookWeapon=Club for something blunt
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLookBloodTest, "Ashmarket.Look.Blood", GutTests::Flags)

bool FGutLookBloodTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		FString Wanted = TEXT("Machete");
		FParse::Value(FCommandLine::Get(), TEXT("GutLookWeapon="), Wanted);

		if (UGutWeaponData* Weapon = FindRackWeapon(Fight, Wanted))
		{
			Fight.Enemy->EquipWeapon(Weapon);
		}

		Fight.Player->EquipWeapon(nullptr);

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
	} });

	// where the blows of each lot come from, as yaw round the player: mostly the front, as in a fight
	static const float Lots[][4] =
	{
		{ 0.0f, 20.0f, -30.0f, 0.0f },
		{ 10.0f, -15.0f, 90.0f, 180.0f },
		{ 0.0f, 35.0f, -90.0f, 160.0f },
	};

	static const FView Around[] =
	{
		{ TEXT("1_front"), FVector(165.0f, 35.0f, 12.0f), FVector(0.0f, 0.0f, 0.0f), 42.0f, FName("spine_03") },
		{ TEXT("2_side"), FVector(25.0f, -165.0f, 12.0f), FVector(0.0f, 0.0f, 0.0f), 42.0f, FName("spine_03") },
		{ TEXT("3_behind"), FVector(-165.0f, -35.0f, 12.0f), FVector(0.0f, 0.0f, 0.0f), 42.0f, FName("spine_03") },
	};

	for (int32 Lot = 0; Lot < static_cast<int32>(UE_ARRAY_COUNT(Lots)); ++Lot)
	{
		for (const float From : Lots[Lot])
		{
			Steps.Add({ 0.5f, [this, From](const FFight& Fight)
			{
				const FVector Stand = FRotator(0.0f, From, 0.0f).RotateVector(FVector(120.0f, 0.0f, 0.0f));

				PlaceAt(Fight.Enemy, Stand.X, Stand.Y, From + 180.0f);

				// it is to be looked at, not killed: what is left in it is topped up when it is nearly gone
				if (Fight.Player->GetHealth() < Fight.Player->GetMaxHealth() * 0.2f)
				{
					Fight.Player->SetHealth(Fight.Player->GetMaxHealth() * 0.28f);
				}

				const UGutWeaponData* Weapon = Fight.Enemy->GetWeapon();
				const bool bHeavy = FMath::RandBool() || !Weapon || Weapon->LightAttacks.IsEmpty();

				const EGutHitResult Result = Fight.Player->ReceiveAttack(Fight.Enemy, bHeavy ? Fight.Enemy->GetHeavyAttack() : Weapon->LightAttacks[0]);

				AddInfo(FString::Printf(TEXT("A blow from %.0f degrees: result %d, health now %.0f"), From, static_cast<int32>(Result), Fight.Player->GetHealth()));
			} });
		}

		// out of the way, and the body given time to stand up straight again
		Steps.Add({ 2.5f, [](const FFight& Fight) { PlaceAt(Fight.Enemy, -350.0f, 350.0f, 0.0f); } });

		for (const FView& View : Around)
		{
			Steps.Add({ 0.6f, [View](const FFight& Fight) { LookFrom(Fight, View); } });
			Steps.Add({ 0.8f, [View, Lot](const FFight& Fight) { Screenshot(FString::Printf(TEXT("blood_%d_%s"), Lot + 1, View.Name)); } });
			Steps.Add({ 0.2f, [](const FFight& Fight) {} });
		}
	}

	// and whoever did it, from in front
	Steps.Add({ 0.6f, [](const FFight& Fight)
	{
		PlaceAt(Fight.Enemy, 0.0f, 300.0f, 0.0f);

		static const FView Far = { TEXT("blood_4_who_did_it"), FVector(165.0f, 330.0f, 140.0f), FVector(0.0f, 300.0f, 125.0f), 42.0f };

		LookFrom(Fight, Far);
	} });

	Steps.Add({ 1.2f, [](const FFight& Fight) { Screenshot(TEXT("blood_4_who_did_it")); } });
	Steps.Add({ 0.2f, [](const FFight& Fight) {} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitCollarsTest, "Ashmarket.Fit.Collars", GutTests::Flags)

bool FGutFitCollarsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	// every body that fights on a ladder, once
	TArray<TSoftClassPtr<AActor>> Bodies;

	for (const TCHAR* Path : { TEXT("/Game/Gut/Ladder/DA_Gut_MeatLadder"), TEXT("/Game/Gut/Ladder/DA_Gut_DogLadder"),
		TEXT("/Game/Gut/Ladder/DA_Gut_ButcherLadder"), TEXT("/Game/Gut/Ladder/DA_Gut_ChampionLadder") })
	{
		if (const UGutLadderData* Ladder = LoadObject<UGutLadderData>(nullptr, Path))
		{
			for (const FGutLadderRung& Rung : Ladder->Rungs)
			{
				if (!Rung.Body.IsNull())
				{
					Bodies.AddUnique(Rung.Body);
				}
			}
		}
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);
	} });

	// the player's own body first, as it comes, and then each of the ladders'
	for (int32 i = -1; i < Bodies.Num(); ++i)
	{
		const TSoftClassPtr<AActor> Body = Bodies.IsValidIndex(i) ? Bodies[i] : TSoftClassPtr<AActor>();
		FString Name = Bodies.IsValidIndex(i) ? Body.GetAssetName() : FString(TEXT("the_player"));

		Name.RemoveFromEnd(TEXT("_C"));
		Name.RemoveFromStart(TEXT("BP_"));

		Steps.Add({ 0.3f, [Body](const FFight& Fight)
		{
			if (!Body.IsNull())
			{
				Fight.Player->SetBody(Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->EquipWeapon(nullptr);
			Fight.Player->SetCollared(true);
		} });

		// each picture is taken a frame after it is asked for, so the camera is never moved in the step that asks
		static const FView Necks[] =
		{
			{ TEXT("1_front"), FVector(85.0f, 40.0f, 14.0f), FVector(0.0f, 0.0f, 4.0f), 40.0f, FName("neck_01") },
			{ TEXT("2_side"), FVector(0.0f, -95.0f, 10.0f), FVector(0.0f, 0.0f, 4.0f), 40.0f, FName("neck_01") },
			{ TEXT("3_behind"), FVector(-80.0f, -40.0f, 16.0f), FVector(0.0f, 0.0f, 4.0f), 40.0f, FName("neck_01") },
		};

		for (const FView& View : Necks)
		{
			Steps.Add({ 1.2f, [View](const FFight& Fight) { LookFrom(Fight, View); } });
			Steps.Add({ 0.8f, [View, Name](const FFight& Fight) { Screenshot(FString::Printf(TEXT("collar_%s_%s"), *Name, View.Name)); } });
			Steps.Add({ 0.2f, [](const FFight& Fight) {} });
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Fit.Shield   Not a test: pictures. The Butcher of Lane Four, in his own body and armor with the stop-sign
 *                         strapped on, standing and with his guard up, from four sides. For seeing whether the shield
 *                         goes through him anywhere. Rendered runs only; pictures are shield_<pose>_<side>.png.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitShieldTest, "Ashmarket.Fit.Shield", GutTests::Flags)

bool FGutFitShieldTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);

		const UGutLadderData* Ladder = LoadObject<UGutLadderData>(nullptr, TEXT("/Game/Gut/Ladder/DA_Gut_ButcherLadder"));
		const FGutLadderRung* Butcher = Ladder ? Ladder->Rungs.FindByPredicate([](const FGutLadderRung& Rung) { return Rung.OpponentName.ToString().Contains(TEXT("Lane Four")); }) : nullptr;

		if (!TestNotNull(TEXT("The Butcher of Lane Four is on his ladder"), Butcher))
		{
			return;
		}

		// -GutFitBody=Ada wears it on somebody else, unarmored: a woman's arm is not the Butcher's
		FString Other;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutFitBody="), Other))
		{
			Fight.Player->SetBody(TSoftClassPtr<AActor>(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), *Other, *Other, *Other))), Fight.Player->GetBodyRetargeter());
			Fight.Player->ClearArmor();
		}
		else
		{
			if (!Butcher->Body.IsNull())
			{
				Fight.Player->SetBody(Butcher->Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->ClearArmor();

			for (UGutArmorData* Piece : Butcher->Armor)
			{
				Fight.Player->WearArmor(Piece);
			}
		}

		Fight.Player->EquipWeapon(Butcher->Weapon);

		// the other one out of the picture
		PlaceAt(Fight.Enemy, 550.0f, 350.0f, 180.0f);
	} });

	static const FView Sides[] =
	{
		{ TEXT("1_front"), FVector(190.0f, 0.0f, -10.0f), FVector(0.0f, 0.0f, -25.0f), 50.0f, FName("neck_01") },
		{ TEXT("2_shield_side"), FVector(20.0f, -190.0f, -10.0f), FVector(0.0f, 0.0f, -25.0f), 50.0f, FName("neck_01") },
		{ TEXT("3_other_side"), FVector(20.0f, 190.0f, -10.0f), FVector(0.0f, 0.0f, -25.0f), 50.0f, FName("neck_01") },
		{ TEXT("4_behind"), FVector(-190.0f, -30.0f, 10.0f), FVector(0.0f, 0.0f, -25.0f), 50.0f, FName("neck_01") },
	};

	for (const bool bGuard : { false, true })
	{
		Steps.Add({ 1.0f, [bGuard](const FFight& Fight) { Fight.Player->SetBlocking(bGuard); } });

		for (const FView& View : Sides)
		{
			Steps.Add({ 1.0f, [View](const FFight& Fight) { LookFrom(Fight, View); } });
			Steps.Add({ 0.6f, [View, bGuard](const FFight& Fight) { Screenshot(FString::Printf(TEXT("shield_%s_%s"), bGuard ? TEXT("guard") : TEXT("stance"), View.Name)); } });
			Steps.Add({ 0.2f, [](const FFight& Fight) {} });
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLookFightTest, "Ashmarket.Film.Fight", GutTests::Flags)

/**
 *  Ashmarket.Film.Fight    two fighters with the opponent's brain fight it out in front of a full deck, and every other frame
 *                          is saved to Saved/GutFilm. Tools/film_gut_fight.sh runs it and joins the frames into a film.
 *                          -GutLookWeapon=Chain and -GutLookEnemyWeapon=Shield choose what they hold.
 */
bool FGutLookFightTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		if (AGutCrowd* Crowd = Fight.GameMode->GetCrowd())
		{
			Crowd->Populate(100000);
		}

		UClass* BrainClass = Fight.Enemy->GetController() ? Fight.Enemy->GetController()->GetClass() : nullptr;

		if (!BrainClass || !GiveBrain(Fight.Player, BrainClass))
		{
			AddError(TEXT("Could not give the player's fighter the opponent's brain"));
			return;
		}

		// -GutFilmHealth=300 gives both of them that much, for a fight long enough to see everything the pit boss has
		float FilmHealth = 0.0f;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutFilmHealth="), FilmHealth) && FilmHealth > 0.0f)
		{
			Fight.Player->SetMaxHealth(FilmHealth);
			Fight.Enemy->SetMaxHealth(FilmHealth);
		}

		// -GutFilmPlayerHealth=40 gives the player's fighter alone that much: with -GutSpare, to film somebody beaten and let live
		if (FParse::Value(FCommandLine::Get(), TEXT("GutFilmPlayerHealth="), FilmHealth) && FilmHealth > 0.0f)
		{
			Fight.Player->SetMaxHealth(FilmHealth);
		}

		// -GutFilmSpecial=4 films the card against the dogs, 3 one against three, 2 two on two, 5 a fighter with a dog at heel
		int32 Special = 0;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutFilmSpecial="), Special) && Special >= 2 && Special <= 5)
		{
			// 5 is not a card: it is somebody who brings dogs of its own, as many as -GutRungDogs= says
			Fight.GameMode->BeginSpecial(Special == 3 ? EGutSpecialCard::OneAgainstThree : (Special == 4 ? EGutSpecialCard::Animal : (Special == 5 ? EGutSpecialCard::WithDogs : EGutSpecialCard::TwoOnTwo)));
		}

		Fight.GameMode->StartFight();

		// -GutFilmCage=1 films a fighter let finish by the cage: the fight is taken to where the spikes are out (2 is the
		// cable), the two are stood in front of them with the one who is to lose nearest, and the other is of a mind to
		// use them whenever it can. The one who is to lose has stopped fighting back, and the next blow beats it.
		int32 Cage = 0;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutFilmCage="), Cage) && (Cage == 1 || Cage == 2))
		{
			Fight.GameMode->AdvanceFightTime(Fight.GameMode->GetRoundStartTime(Cage == 1 ? 2 : 3) + 1.0f - Fight.GameMode->GetFightTime());

			if (Cage == 1)
			{
				// the spikes are in the long wall at Y -500
				PlaceAt(Fight.Player, 60.0f, -500.0f + 250.0f, 90.0f);
				PlaceAt(Fight.Enemy, 60.0f, -500.0f + 450.0f, -90.0f);
			}
			else
			{
				// the cable lies along Y 250, from X -630 to -30
				PlaceAt(Fight.Player, -330.0f, 250.0f - 235.0f, -90.0f);
				PlaceAt(Fight.Enemy, -330.0f, 250.0f - 430.0f, 90.0f);
			}

			if (AGutOpponentController* Brain = Cast<AGutOpponentController>(Fight.Enemy->GetController()))
			{
				FGutOpponentTuning Tuning = Brain->GetTuning();

				Tuning.CageFinishChance = 1.0f;

				Brain->SetTuning(Tuning);
			}

			// the one who is to lose is on its last legs and has stopped fighting back: the next blow beats it where it stands
			if (AGutBrainController* Spent = Cast<AGutBrainController>(Fight.Player->GetController()))
			{
				Spent->SetBrainEnabled(false);
			}

			Fight.Player->SetHealth(6.0f);

			// nothing is written over it: what is written in a fight is written to whoever is fighting it, and this is one
			// that is watched
			if (const APlayerController* Eyes = Fight.GameMode->GetWorld()->GetFirstPlayerController(); Eyes && Eyes->GetHUD())
			{
				Eyes->GetHUD()->bShowHUD = false;
			}
		}

		FString Wanted;

		// None for bare hands
		if (FParse::Value(FCommandLine::Get(), TEXT("GutLookWeapon="), Wanted))
		{
			if (Wanted == TEXT("None"))
			{
				Fight.Player->DropWeapon();
			}
			else if (UGutWeaponData* Weapon = FindRackWeapon(Fight, Wanted))
			{
				Fight.Player->EquipWeapon(Weapon);
			}
		}

		if (FParse::Value(FCommandLine::Get(), TEXT("GutLookEnemyWeapon="), Wanted))
		{
			if (Wanted == TEXT("None"))
			{
				Fight.Enemy->DropWeapon();
			}
			else if (UGutWeaponData* Weapon = FindRackWeapon(Fight, Wanted))
			{
				Fight.Enemy->EquipWeapon(Weapon);
			}
		}
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	FFilmFightCommand* Film = new FFilmFightCommand(this);
	FParse::Value(FCommandLine::Get(), TEXT("GutFilmSeconds="), Film->MaxSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("GutFilmEvery="), Film->Every);
	Film->Every = FMath::Max(1, Film->Every);
	Film->bTrace = FParse::Param(FCommandLine::Get(), TEXT("GutTrace"));
	Film->bSound = FParse::Param(FCommandLine::Get(), TEXT("GutFilmSound"));

	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(Film));

	return true;
}

namespace GutLookTests
{
	/**
	 *  Plays motion clips one after another on the player's fighter and saves a picture every other frame,
	 *  from a camera that stands off its weapon side and a little in front.
	 */
	class FFilmMotionCommand : public IAutomationLatentCommand
	{
	public:

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			if (!Fight.IsValid())
			{
				return true;
			}

			UWorld* World = Fight.GameMode->GetWorld();
			const float Now = World->GetTimeSeconds();

			if (StartTime < 0.0f)
			{
				StartTime = Now;
			}

			// the fighters are given a moment to be placed and to settle before the first clip
			if (Now - StartTime < Settle)
			{
				return false;
			}

			if (!Camera)
			{
				const FRotator Facing(0.0f, Fight.Player->GetActorRotation().Yaw, 0.0f);
				const FVector Feet = Fight.Player->GetActorLocation() - FVector(0.0f, 0.0f, Fight.Player->GetSimpleCollisionHalfHeight());

				const FVector From = Feet + Facing.RotateVector(CameraFrom);
				const FVector At = Feet + Facing.RotateVector(CameraAt);

				Camera = World->SpawnActor<ACameraActor>(From, (At - From).Rotation());
				Camera->GetCameraComponent()->SetFieldOfView(FieldOfView);
				Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

				if (APlayerController* Viewer = World->GetFirstPlayerController())
				{
					Viewer->SetViewTarget(Camera);

					if (AHUD* HUD = Viewer->GetHUD())
					{
						HUD->bShowHUD = false;
					}
				}

				// the dog's clips: its body stands where the player's fighter stood, and the fighter is not seen
				if (bBeast)
				{
					USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Gut/Art/Beast/Dog/pit_dog/SkeletalMeshes/pit_dog.pit_dog"));

					Beast = World->SpawnActor<ASkeletalMeshActor>(Feet, Facing + FRotator(0.0f, BeastYaw, 0.0f));

					if (Beast && Body)
					{
						Beast->GetSkeletalMeshComponent()->SetSkeletalMesh(Body);
						Beast->GetSkeletalMeshComponent()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						Beast->GetSkeletalMeshComponent()->SetReceivesDecals(false);
						Beast->SetActorScale3D(FVector(BeastScale));
					}

					TArray<AActor*> Worn;
					Fight.Player->GetAttachedActors(Worn, true, true);
					Worn.Add(Fight.Player);

					for (AActor* Part : Worn)
					{
						Part->SetActorHiddenInGame(true);
					}
				}
			}

			if (Index < 0 || Now > ClipEnd + Gap)
			{
				if (++Index >= Clips.Num())
				{
					return true;
				}

				Saved = 0;
				Frame = 0;
				ClipStart = Now;
				ClipEnd = Now;
				bMeasured = false;

				UAnimSequenceBase* Clip = LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), bBeast ? TEXT("/Game/Gut/Art/Beast/Dog/Clips") : TEXT("/Game/Gut/Art/Motion"), *Clips[Index], *Clips[Index]));
				UAnimInstance* Driver = Fight.Player->GetMesh()->GetAnimInstance();

				if (Clip && Beast)
				{
					// an animal's clip on the animal's own body, round and round
					Beast->GetSkeletalMeshComponent()->PlayAnimation(Clip, true);

					ClipEnd = Now + (Clip->GetPlayLength() * Loops);
				}
				else if (Clip && Driver)
				{
					Driver->PlaySlotAnimationAsDynamicMontage(Clip, FName("DefaultSlot"), 0.15f, 0.2f, 1.0f, Loops);

					ClipEnd = Now + (Clip->GetPlayLength() * Loops);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[motion] no clip called %s"), *Clips[Index]);
				}
			}

			// half-way through, where the feet are: on the skeleton that plays the clip and on the body that is seen
			if (!bMeasured && Now > (ClipStart + ClipEnd) * 0.5f)
			{
				bMeasured = true;

				const float Floor = Fight.Player->GetActorLocation().Z - Fight.Player->GetSimpleCollisionHalfHeight();

				for (const USkeletalMeshComponent* Mesh : { Fight.Player->GetMesh(), Fight.Player->GetShownMesh() })
				{
					FString Line;

					for (const FName Bone : { FName("pelvis"), FName("foot_l"), FName("ball_l"), FName("foot_r"), FName("ball_r"), FName("hand_r"), FName("head") })
					{
						Line += FString::Printf(TEXT(" %s %.1f"), *Bone.ToString(), Mesh->GetBoneLocation(Bone).Z - Floor);
					}

					UE_LOG(LogTemp, Display, TEXT("[motion] %s, %s, height above the floor:%s"), *Clips[Index], *Mesh->GetName(), *Line);
				}
			}

			if (Frame++ % Every == 0)
			{
				const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutMotion") / Clips[Index] / FString::Printf(TEXT("frame_%04d.png"), Saved++));

				FScreenshotRequest::RequestScreenshot(Path, false, false);
			}

			return false;
		}

		TArray<FString> Clips;
		int32 Loops = 1;
		int32 Every = 2;
		float Settle = 4.0f;
		float Gap = 0.5f;
		FVector CameraFrom = FVector(300.0f, 420.0f, 125.0f);
		FVector CameraAt = FVector(30.0f, 0.0f, 98.0f);
		float FieldOfView = 46.0f;

		/** If true the clips are the dog's, played on the dog's own body. How that body is turned to face as a fighter faces, and its size. */
		bool bBeast = false;
		float BeastYaw = -90.0f;
		float BeastScale = 1.15f;

	private:

		ASkeletalMeshActor* Beast = nullptr;
		ACameraActor* Camera = nullptr;
		float StartTime = -1.0f;
		float ClipStart = 0.0f;
		float ClipEnd = 0.0f;
		bool bMeasured = false;
		int32 Index = -1;
		int32 Frame = 0;
		int32 Saved = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLookMotionTest, "Ashmarket.Film.Motion", GutTests::Flags)

/**
 *  Ashmarket.Film.Motion    plays the clips named with -GutMotion=a+b+c (assets in /Game/Gut/Art/Motion) on the player's fighter,
 *                           one after another, and saves every other frame to Saved/GutMotion/<clip>. Tools/film_gut_motion.sh
 *                           runs it and joins the frames. -GutLookWeapon=Club chooses what is in the hand, -GutMotionLoops=3 plays
 *                           each clip that many times, -GutMotionAlone sends the opponent out of the picture,
 *                           -GutMotionFront looks from in front instead of from the side.
 */
bool FGutLookMotionTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();
	} });

	Steps.Add({ 1.0f, [](const FFight& Fight)
	{
		PlaceFaceToFace(Fight);

		if (FParse::Param(FCommandLine::Get(), TEXT("GutMotionAlone")))
		{
			// straight ahead and a long way off: still in front of the fighter, so it does not turn, and behind the camera
			PlaceAt(Fight.Enemy, 640.0f, 0.0f, 180.0f);
		}

		FString Wanted;

		if (FParse::Value(FCommandLine::Get(), TEXT("GutLookWeapon="), Wanted))
		{
			// -GutLookWeapon=None for bare hands
			if (Wanted == TEXT("None"))
			{
				Fight.Player->DropWeapon();
			}
			else if (UGutWeaponData* Weapon = FindRackWeapon(Fight, Wanted))
			{
				Fight.Player->EquipWeapon(Weapon);
			}
		}
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	FFilmMotionCommand* Film = new FFilmMotionCommand();

	FString Names;
	FParse::Value(FCommandLine::Get(), TEXT("GutMotion="), Names);
	Names.ParseIntoArray(Film->Clips, TEXT("+"));

	FParse::Value(FCommandLine::Get(), TEXT("GutMotionLoops="), Film->Loops);
	Film->Loops = FMath::Max(1, Film->Loops);

	if (FParse::Param(FCommandLine::Get(), TEXT("GutMotionFront")))
	{
		Film->CameraFrom = FVector(510.0f, 70.0f, 125.0f);
		Film->CameraAt = FVector(0.0f, 0.0f, 98.0f);
	}

	// -GutMotionBeast: the clips are the dog's (/Game/Gut/Art/Beast/Dog/Clips), seen from nearer and lower
	if (FParse::Param(FCommandLine::Get(), TEXT("GutMotionBeast")))
	{
		Film->bBeast = true;
		Film->CameraFrom = FVector(170.0f, 300.0f, 75.0f);
		Film->CameraAt = FVector(15.0f, 0.0f, 45.0f);

		FParse::Value(FCommandLine::Get(), TEXT("GutBeastYaw="), Film->BeastYaw);
		FParse::Value(FCommandLine::Get(), TEXT("GutBeastScale="), Film->BeastScale);
	}

	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(Film));

	return true;
}

namespace GutLookTests
{
	/**
	 *  Stages finishers one after another, each in a cage opened afresh: the opponent is beaten to a stagger, left to sway for a
	 *  moment, and finished with the weapon and in the style asked for, filmed from the side from the stagger to the body on the floor.
	 */
	class FFilmFinishersCommand : public IAutomationLatentCommand
	{
	public:

		explicit FFilmFinishersCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			const FFight Fight = GetFight();

			// between cages there is nobody to film
			if (!Fight.IsValid())
			{
				return ++Waited > 3000;
			}

			Waited = 0;

			AGutGameMode* GameMode = Fight.GameMode;
			UWorld* World = GameMode->GetWorld();
			const float Now = World->GetTimeSeconds();

			// a cage just opened: the next finisher on the list
			if (LastGameMode != GameMode)
			{
				LastGameMode = GameMode;

				if (++Index >= Wanted.Num())
				{
					return true;
				}

				Stage = 0;
				StageTime = Now;
				Tries = 0;
				Frame = 0;
				Saved = 0;
				Camera = nullptr;

				Wanted[Index].Split(TEXT(":"), &WeaponName, &StyleName);
			}

			const float Since = Now - StageTime;
			const EGutFinisherStyle Style = StyleName.Equals(TEXT("Slow"), ESearchCase::IgnoreCase) ? EGutFinisherStyle::Slow : EGutFinisherStyle::Clean;

			auto Next = [this, Now]() { ++Stage; StageTime = Now; };

			switch (Stage)
			{
			case 0:

				// through the rack to the bell, with the people given time to load
				if (Since > 0.5f)
				{
					GameMode->EnterSandbox();
					GameMode->SkipRack();
					GameMode->StartFight();

					if (AGutBrainController* Brain = Cast<AGutBrainController>(Fight.Enemy->GetController()))
					{
						Brain->SetBrainEnabled(false);
					}

					Next();
				}
				break;

			case 1:

				if (Since > (Index == 0 ? 4.0f : 2.0f))
				{
					PlaceFaceToFace(Fight);

					if (WeaponName == TEXT("None"))
					{
						Fight.Player->DropWeapon();
					}
					else if (UGutWeaponData* Weapon = FindRackWeapon(Fight, WeaponName))
					{
						Fight.Player->EquipWeapon(Weapon);
					}

					// side on to the pair, a little toward the one who will be finished
					const FVector Feet = FVector(10.0f, 0.0f, Fight.Player->GetActorLocation().Z - Fight.Player->GetSimpleCollisionHalfHeight());
					const FVector From = Feet + FVector(70.0f, 440.0f, 125.0f);
					const FVector At = Feet + FVector(40.0f, 0.0f, 92.0f);

					Camera = World->SpawnActor<ACameraActor>(From, (At - From).Rotation());
					Camera->GetCameraComponent()->SetFieldOfView(46.0f);
					Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

					if (APlayerController* Viewer = World->GetFirstPlayerController())
					{
						Viewer->SetViewTarget(Camera);

						if (AHUD* HUD = Viewer->GetHUD())
						{
							HUD->bShowHUD = false;
						}
					}

					Next();
				}
				break;

			case 2:

				// one blow on somebody with nothing left puts it out on its feet
				if (Since > 0.6f)
				{
					if (Fight.Enemy->IsStaggered())
					{
						Next();
					}
					else if (Tries++ > 4)
					{
						UE_LOG(LogTemp, Error, TEXT("[finisher] %s: the opponent would not stagger"), *Wanted[Index]);

						Stage = 5;
						StageTime = Now;
					}
					else
					{
						Fight.Enemy->SetHealth(1.0f);
						Fight.Player->SetStamina(Fight.Player->GetMaxStamina());
						Fight.Player->TryLightAttack();

						StageTime = Now;
					}
				}
				break;

			case 3:

				// left to sway for a moment, so the film shows what it was doing before
				if (Since > 1.6f)
				{
					Fight.Player->SetStamina(Fight.Player->GetMaxStamina());

					if (Fight.Player->TryFinisher(Style))
					{
						Length = Fight.Player->GetCurrentFinisher().Windup + 4.5f;

						UE_LOG(LogTemp, Display, TEXT("[finisher] %s: %s, %.1f s to the blow"), *Wanted[Index], *Fight.Player->GetCurrentFinisher().Label.ToString(), Fight.Player->GetCurrentFinisher().Windup);

						Next();
					}
					else if (Tries++ > 8)
					{
						UE_LOG(LogTemp, Error, TEXT("[finisher] %s: the finisher would not start"), *Wanted[Index]);

						Stage = 5;
						StageTime = Now;
					}
				}
				break;

			case 4:

				if (Since > Length)
				{
					Next();
				}
				break;

			default:

				// on to the next, in a cage opened afresh. The last one ends the film.
				if (Index + 1 >= Wanted.Num())
				{
					return true;
				}

				if (!Fight.Enemy->IsAlive() && GameMode->CanRestart())
				{
					GameMode->RequestRestart();
				}
				else if (Fight.Enemy->IsAlive() && Since > 1.0f)
				{
					// it never came to a death: end it so the cage can be opened again
					Fight.Enemy->Kill(Fight.Player, FVector(1.0f, 0.0f, 0.0f));
				}
				break;
			}

			// from the stagger to the body on the floor
			if (Camera && Stage >= 3 && Stage <= 4 && Frame++ % Every == 0)
			{
				const FString Name = FString::Printf(TEXT("%s_%s"), *WeaponName.ToLower(), *StyleName.ToLower());
				const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GutMotion") / Name / FString::Printf(TEXT("frame_%04d.png"), Saved++));

				FScreenshotRequest::RequestScreenshot(Path, false, false);
			}

			return false;
		}

		TArray<FString> Wanted;
		int32 Every = 2;

	private:

		FAutomationTestBase* Test;
		TWeakObjectPtr<AGutGameMode> LastGameMode;
		ACameraActor* Camera = nullptr;
		FString WeaponName;
		FString StyleName;
		int32 Index = -1;
		int32 Stage = 0;
		int32 Tries = 0;
		int32 Frame = 0;
		int32 Saved = 0;
		int32 Waited = 0;
		float StageTime = 0.0f;
		float Length = 6.0f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutLookFinishersTest, "Ashmarket.Film.Finishers", GutTests::Flags)

/**
 *  Ashmarket.Film.Finishers    stages the finishers named with -GutFinishers=Chain:Slow+Machete:Clean+None:Slow (a rack weapon or
 *                              None, and Clean or Slow), each in its own cage, and saves every other frame from the stagger to the
 *                              body on the floor to Saved/GutMotion/<weapon>_<style>. Tools/film_gut_finishers.sh runs it and joins the frames.
 */
bool FGutLookFinishersTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this, true))
	{
		return false;
	}

	FFilmFinishersCommand* Film = new FFilmFinishersCommand(this);

	FString Names = TEXT("None:Clean+None:Slow+Club:Clean+Club:Slow+Machete:Clean+Machete:Slow+Chain:Clean+Chain:Slow+Shield:Clean+Shield:Slow");
	FParse::Value(FCommandLine::Get(), TEXT("GutFinishers="), Names);
	Names.ParseIntoArray(Film->Wanted, TEXT("+"));

	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(Film));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

/**
 *  Ashmarket.Fit.Bound   Not a test: pictures. Somebody with their wrists tied behind their back, as a lot on the Block
 *  or somebody bought and led away is: standing, led at a walk and led at a jog, from behind, from the side and from
 *  three quarters behind. Two bodies, one broad and one slight, since no two are as wide across the back.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitBoundTest, "Ashmarket.Fit.Bound", GutTests::Flags)

bool FGutFitBoundTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->StartFight();

		PlaceFaceToFace(Fight);

		// the other one out of the picture
		PlaceAt(Fight.Enemy, 550.0f, 350.0f, 180.0f);
	} });

	static const FView Sides[] =
	{
		{ TEXT("1_behind"), FVector(-170.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") },
		{ TEXT("2_side"), FVector(0.0f, -170.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") },
		{ TEXT("3_three_quarters"), FVector(-130.0f, 110.0f, 25.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") },
		{ TEXT("4_whole"), FVector(-330.0f, 120.0f, 40.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") },
	};

	static const TCHAR* Bodies[] = { TEXT("Asha"), TEXT("Mix05") };

	for (const TCHAR* Who : Bodies)
	{
		Steps.Add({ 0.3f, [this, Who](const FFight& Fight)
		{
			const TSoftClassPtr<AActor> Body(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), Who, Who, Who)));

			if (TestNotNull(*FString::Printf(TEXT("There is a body called %s"), Who), Body.LoadSynchronous()))
			{
				Fight.Player->SetBody(Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->ClearArmor();
			Fight.Player->EquipWeapon(nullptr);
			Fight.Player->SetAtEase(true);
			Fight.Player->SetCollared(true);
			Fight.Player->SetBound(true);
		} });

		for (const float Pace : { 0.0f, 150.0f, 340.0f })
		{
			Steps.Add({ 1.5f, [Pace](const FFight& Fight) { Fight.Player->SetLedVelocity(Fight.Player->GetActorForwardVector() * Pace); } });

			for (const FView& View : Sides)
			{
				Steps.Add({ 0.5f, [View](const FFight& Fight) { LookFrom(Fight, View); } });
				Steps.Add({ 0.5f, [View, Who, Pace](const FFight& Fight) { Screenshot(FString::Printf(TEXT("bound_%s_%s_%s"), Who, Pace <= 0.0f ? TEXT("standing") : (Pace < 200.0f ? TEXT("walked") : TEXT("jogged")), View.Name)); } });
				Steps.Add({ 0.2f, [](const FFight& Fight) {} });
			}
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Fit.Outfit    people dressed out of the wardrobe, seen standing, walking and at a jog from in front, from
 *                          the side, from behind and from three quarters, so that anything that cuts through them is seen.
 *                          -GutFitBodies=Try01+Try02 names who (the folder each was built in);
 *                          -GutFitIdle=style_idle_folded stands them that way.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitOutfitTest, "Ashmarket.Fit.Outfit", GutTests::Flags)

bool FGutFitOutfitTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	FString Named;

	if (!FApp::CanEverRender() || !FParse::Value(FCommandLine::Get(), TEXT("GutFitBodies="), Named))
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FString> Bodies;
	Named.ParseIntoArray(Bodies, TEXT("+"), true);

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		// a long look: nothing the pit boss does to a long fight is to come into it (the dogs are kept out with -GutNoDogs)
		Fight.GameMode->SetHazardsEnabled(false);
		Fight.GameMode->StartFight();

		// in the middle of the pool, with room to stand back from them on every side
		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);

		// the other one out of the picture
		PlaceAt(Fight.Enemy, 600.0f, 420.0f, 180.0f);
	} });

	// each from the hips: X in front of them, Y to their right
	static const FView Sides[] =
	{
		{ TEXT("1_front"), FVector(540.0f, 0.0f, -8.0f), FVector(0.0f, 0.0f, -8.0f), 40.0f, FName("pelvis") },
		{ TEXT("2_side"), FVector(0.0f, -540.0f, -8.0f), FVector(0.0f, 0.0f, -8.0f), 40.0f, FName("pelvis") },
		{ TEXT("3_behind"), FVector(-540.0f, 0.0f, -8.0f), FVector(0.0f, 0.0f, -8.0f), 40.0f, FName("pelvis") },
		{ TEXT("4_three_quarters"), FVector(420.0f, 340.0f, 10.0f), FVector(0.0f, 0.0f, -8.0f), 40.0f, FName("pelvis") },
		{ TEXT("5_close"), FVector(120.0f, 45.0f, 5.0f), FVector(0.0f, 0.0f, -12.0f), 40.0f, FName("head") },
	};

	for (const FString& Who : Bodies)
	{
		Steps.Add({ 0.3f, [this, Who](const FFight& Fight)
		{
			const TSoftClassPtr<AActor> Body(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who)));

			if (TestNotNull(*FString::Printf(TEXT("There is a body called %s"), *Who), Body.LoadSynchronous()))
			{
				Fight.Player->SetBody(Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->ClearArmor();
			Fight.Player->EquipWeapon(nullptr);
			Fight.Player->SetAtEase(true);
			Fight.Player->SetCollared(false);
			Fight.Player->SetBound(FParse::Param(FCommandLine::Get(), TEXT("GutFitBound")));

			// -GutFitWeapon=Club has them stand as a fighter stands with that in the hand, not at ease: for seeing whether
			// the hand and what is in it are in the wearer's own hip or thigh
			FString Armed;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitWeapon="), Armed))
			{
				if (UGutWeaponData* Held = LoadObject<UGutWeaponData>(nullptr, *FString::Printf(TEXT("/Game/Gut/Weapons/DA_Gut_%s.DA_Gut_%s"), *Armed, *Armed)))
				{
					Fight.Player->SetAtEase(false);
					Fight.Player->EquipWeapon(Held);
				}
			}

			// -GutFitArmor=RubberVest+RubberSleeves puts those pieces on over the clothes; -GutFitBound ties the wrists
			FString Worn;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitArmor="), Worn))
			{
				TArray<FString> Pieces;
				Worn.ParseIntoArray(Pieces, TEXT("+"), true);

				for (const FString& Piece : Pieces)
				{
					if (UGutArmorData* Armor = LoadObject<UGutArmorData>(nullptr, *FString::Printf(TEXT("/Game/Gut/Armor/DA_Gut_Armor_%s.DA_Gut_Armor_%s"), *Piece, *Piece)))
					{
						Fight.Player->WearArmor(Armor);
					}
				}
			}

			// -GutFitIdle=style_idle_folded stands them a given way, to see whether arms that are folded or on the hips
			// go through this body as they do not through the one the clip was looked at on
			FString Stood;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitIdle="), Stood))
			{
				Fight.Player->SetAtEaseIdle(LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/Motion/%s.%s"), *Stood, *Stood)));
			}

			// -GutFitFamily=clasp stands them in that family of standing, as a place's list does
			FString Family;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitFamily="), Family))
			{
				Fight.Player->SetAtEaseFamily(FName(*Family));
			}

			// and -GutFitWalk=style_walk_behind -GutFitWalkSpeed=95 walks them that way, at the speed it was walked at
			FString Walked;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitWalk="), Walked))
			{
				float Speed = 90.0f;
				FParse::Value(FCommandLine::Get(), TEXT("GutFitWalkSpeed="), Speed);

				Fight.Player->SetAtEaseWalk(LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/Motion/%s.%s"), *Walked, *Walked)), Speed);
			}
		} });

		// at the town's own walk (or the asked one's own speed) and its jog
		float OwnWalk = 112.0f;
		FParse::Value(FCommandLine::Get(), TEXT("GutFitWalkSpeed="), OwnWalk);

		for (const float Pace : { 0.0f, OwnWalk, 340.0f })
		{
			Steps.Add({ 2.0f, [Pace](const FFight& Fight) { Fight.Player->SetLedVelocity(Fight.Player->GetActorForwardVector() * Pace); } });

			for (int32 i = 0; i < UE_ARRAY_COUNT(Sides); ++i)
			{
				// standing is looked at from everywhere; moving, from the side and from three quarters
				if (Pace > 0.0f && i != 1 && i != 3)
				{
					continue;
				}

				const FView View = Sides[i];

				Steps.Add({ 0.5f, [View](const FFight& Fight) { LookFrom(Fight, View); } });
				Steps.Add({ 0.5f, [View, Who, Pace](const FFight& Fight) { Screenshot(FString::Printf(TEXT("outfit_%s_%s_%s"), *Who, Pace <= 0.0f ? TEXT("standing") : (Pace < 200.0f ? TEXT("walked") : TEXT("jogged")), View.Name)); } });
				Steps.Add({ 0.2f, [](const FFight& Fight) {} });
			}
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

namespace GutLookTests
{
	/** One part of a dressed body as it is posed at this moment: its points in the world, and for skin which way each faces */
	struct FPosedPart
	{
		FString Name;
		TArray<FVector> Points;
		TArray<FVector> Normals;
		TBitArray<> Drawn;
	};

	/** Poses one part on the processor, at its finest level of detail whatever is being drawn. False if the part has nothing to pose. */
	bool PosePart(USkeletalMeshComponent* Part, bool bWithNormals, FPosedPart& Out)
	{
		USkeletalMesh* Mesh = Part ? Part->GetSkeletalMeshAsset() : nullptr;
		FSkeletalMeshRenderData* Data = Mesh ? Mesh->GetResourceForRendering() : nullptr;

		if (!Data || Data->LODRenderData.IsEmpty())
		{
			return false;
		}

		const FSkeletalMeshLODRenderData& Level = Data->LODRenderData[0];
		const int32 Count = Level.GetNumVertices();

		if (Count <= 0 || !Level.StaticVertexBuffers.PositionVertexBuffer.GetVertexData() || Level.SkinWeightVertexBuffer.GetNumVertices() != static_cast<uint32>(Count))
		{
			return false;
		}

		TArray<FMatrix44f> RefToLocal;
		TArray<FVector3f> Skinned;

		Part->CacheRefToLocalMatrices(RefToLocal);

		if (RefToLocal.IsEmpty())
		{
			return false;
		}

		USkinnedMeshComponent::ComputeSkinnedPositions(Part, Skinned, RefToLocal, Level, Level.SkinWeightVertexBuffer);

		if (Skinned.Num() != Count)
		{
			return false;
		}

		const FTransform ToWorld = Part->GetComponentTransform();

		Out.Name = Part->GetName();
		Out.Points.SetNumUninitialized(Count);

		for (int32 i = 0; i < Count; ++i)
		{
			Out.Points[i] = ToWorld.TransformPosition(FVector(Skinned[i]));
		}

		// what of it is drawn: a part's hidden pieces are given a material that draws nothing
		Out.Drawn.Init(false, Count);

		for (const FSkelMeshRenderSection& Section : Level.RenderSections)
		{
			const UMaterialInterface* Material = Part->GetMaterial(Section.MaterialIndex);

			if (Section.bDisabled || !Part->IsMaterialSectionShown(Section.MaterialIndex, 0) || (Material && Material->GetName().Contains(TEXT("NotDrawn"))))
			{
				continue;
			}

			Out.Drawn.SetRange(Section.BaseVertexIndex, FMath::Min<int32>(Section.NumVertices, Count - Section.BaseVertexIndex), true);
		}

		if (bWithNormals)
		{
			TArray<uint32> Corners;
			Level.MultiSizeIndexContainer.GetIndexBuffer(Corners);

			Out.Normals.Init(FVector::ZeroVector, Count);

			for (int32 i = 0; i + 2 < Corners.Num(); i += 3)
			{
				const int32 A = Corners[i], B = Corners[i + 1], C = Corners[i + 2];

				if (A >= Count || B >= Count || C >= Count)
				{
					continue;
				}

				// the engine winds its faces so that this points out of the body
				const FVector Face = FVector::CrossProduct(Out.Points[C] - Out.Points[A], Out.Points[B] - Out.Points[A]);

				Out.Normals[A] += Face;
				Out.Normals[B] += Face;
				Out.Normals[C] += Face;
			}

			for (FVector& Normal : Out.Normals)
			{
				Normal = Normal.GetSafeNormal();
			}
		}

		return true;
	}

	/** Where a garment goes under the skin of the body wearing it */
	struct FCut
	{
		FString Garment;
		FString Region;
		int32 Points = 0;
		float Deepest = 0.0f;
		FVector Where = FVector::ZeroVector;
		FVector Outward = FVector::ZeroVector;
	};

	/** Which part of the body a place is nearest: the limb between two joints, by the name of the upper one */
	FString RegionOf(const USkeletalMeshComponent* Body, const FVector& Where)
	{
		static const TCHAR* Limbs[][2] =
		{
			{ TEXT("upperarm_l"), TEXT("lowerarm_l") }, { TEXT("lowerarm_l"), TEXT("hand_l") }, { TEXT("hand_l"), TEXT("middle_01_l") },
			{ TEXT("upperarm_r"), TEXT("lowerarm_r") }, { TEXT("lowerarm_r"), TEXT("hand_r") }, { TEXT("hand_r"), TEXT("middle_01_r") },
			{ TEXT("thigh_l"), TEXT("calf_l") }, { TEXT("calf_l"), TEXT("foot_l") }, { TEXT("foot_l"), TEXT("ball_l") },
			{ TEXT("thigh_r"), TEXT("calf_r") }, { TEXT("calf_r"), TEXT("foot_r") }, { TEXT("foot_r"), TEXT("ball_r") },
			{ TEXT("pelvis"), TEXT("spine_03") }, { TEXT("spine_03"), TEXT("spine_05") }, { TEXT("spine_05"), TEXT("neck_01") }, { TEXT("neck_01"), TEXT("head") },
			{ TEXT("clavicle_l"), TEXT("upperarm_l") }, { TEXT("clavicle_r"), TEXT("upperarm_r") },
		};

		FString Nearest = TEXT("?");
		double Least = TNumericLimits<double>::Max();

		for (const auto& Limb : Limbs)
		{
			if (Body->GetBoneIndex(Limb[0]) == INDEX_NONE || Body->GetBoneIndex(Limb[1]) == INDEX_NONE)
			{
				continue;
			}

			const double Off = FMath::PointDistToSegmentSquared(Where, Body->GetBoneLocation(Limb[0]), Body->GetBoneLocation(Limb[1]));

			if (Off < Least)
			{
				Least = Off;
				Nearest = Limb[0];
			}
		}

		return Nearest;
	}

	/**
	 *  Measures, on the body as it is posed now, where what it wears is under its skin: that is where skin shows through
	 *  cloth, or an arm lies inside a vest. Each garment's points are set against the nearest point of the skin that is
	 *  drawn; one that is behind that skin by more than the tolerance, with the skin over it, is a cut.
	 */
	void MeasureCuts(const AGutFighter* Fighter, float Tolerance, TArray<FCut>& Out, TArray<FString>* OutParts = nullptr)
	{
		const AActor* Dressed = Fighter->GetBodyActor();

		if (!Dressed)
		{
			return;
		}

		TArray<USkeletalMeshComponent*> Parts;
		Dressed->GetComponents(Parts);

		const USkeletalMeshComponent* Body = nullptr;

		TArray<FPosedPart> Skin;
		TArray<FPosedPart> Worn;

		for (USkeletalMeshComponent* Part : Parts)
		{
			const USkeletalMesh* Mesh = Part->GetSkeletalMeshAsset();

			if (!Mesh || !Part->IsVisible())
			{
				continue;
			}

			const FString Name = Part->GetName();
			const bool bSkin = Name == TEXT("Body") || Name == TEXT("Face");

			if (Name == TEXT("Body"))
			{
				Body = Part;
			}

			FPosedPart Posed;

			if (!PosePart(Part, bSkin, Posed))
			{
				continue;
			}

			Posed.Name = FString::Printf(TEXT("%s (%s)"), *Name, *Mesh->GetName());

			if (OutParts)
			{
				OutParts->Add(FString::Printf(TEXT("%s, %d points, %s"), *Posed.Name, Posed.Points.Num(), bSkin ? TEXT("skin") : TEXT("worn")));
			}

			(bSkin ? Skin : Worn).Add(MoveTemp(Posed));
		}

		if (!Body || Skin.IsEmpty())
		{
			return;
		}

		// the skin's points in boxes four centimetres across, to find the nearest of them to anything quickly
		const double Box = 4.0;

		auto BoxOf = [Box](const FVector& Point) { return FIntVector(FMath::FloorToInt32(Point.X / Box), FMath::FloorToInt32(Point.Y / Box), FMath::FloorToInt32(Point.Z / Box)); };

		struct FSkinPoint
		{
			FVector Point;
			FVector Normal;
		};

		TMap<FIntVector, TArray<FSkinPoint, TInlineAllocator<8>>> Boxes;

		for (const FPosedPart& Part : Skin)
		{
			for (int32 i = 0; i < Part.Points.Num(); ++i)
			{
				if (Part.Drawn[i] && !Part.Normals[i].IsNearlyZero())
				{
					Boxes.FindOrAdd(BoxOf(Part.Points[i])).Add({ Part.Points[i], Part.Normals[i] });
				}
			}
		}

		for (const FPosedPart& Part : Worn)
		{
			TMap<FString, FCut> ByRegion;

			for (int32 i = 0; i < Part.Points.Num(); ++i)
			{
				if (!Part.Drawn[i])
				{
					continue;
				}

				const FVector& Point = Part.Points[i];
				const FIntVector Middle = BoxOf(Point);

				const FSkinPoint* Nearest = nullptr;
				double Least = FMath::Square(Box);

				for (int32 X = -1; X <= 1; ++X)
				{
					for (int32 Y = -1; Y <= 1; ++Y)
					{
						for (int32 Z = -1; Z <= 1; ++Z)
						{
							if (const auto* Held = Boxes.Find(Middle + FIntVector(X, Y, Z)))
							{
								for (const FSkinPoint& One : *Held)
								{
									const double Off = FVector::DistSquared(One.Point, Point);

									if (Off < Least)
									{
										Least = Off;
										Nearest = &One;
									}
								}
							}
						}
					}
				}

				if (!Nearest)
				{
					continue;
				}

				const FVector Off = Point - Nearest->Point;
				const double Depth = -FVector::DotProduct(Off, Nearest->Normal);
				const double Aside = (Off + (Nearest->Normal * Depth)).Size();

				// under the skin, with that skin over it and not merely somewhere near its edge
				if (Depth < Tolerance || Aside > 1.6)
				{
					continue;
				}

				FCut& Cut = ByRegion.FindOrAdd(RegionOf(Body, Point));

				++Cut.Points;

				if (Depth > Cut.Deepest)
				{
					Cut.Deepest = Depth;
					Cut.Where = Point;
					Cut.Outward = Nearest->Normal;
				}
			}

			for (TPair<FString, FCut>& Region : ByRegion)
			{
				Region.Value.Garment = Part.Name;
				Region.Value.Region = Region.Key;

				Out.Add(Region.Value);
			}
		}
	}
}

/**
 *  Ashmarket.Fit.Cuts     Not pictures first: a measure. Every body named with -GutFitBodies=A+B+C is stood every way
 *                         the game stands people (each family of standing its sex is dealt, wrists tied, walking, at a
 *                         jog, and in a fighter's guard), and in each it is measured where what it wears is under its
 *                         skin. Each finding is a line "[cuts] who | how stood | garment | where | points | cm", the
 *                         whole is written to Saved/GutCuts/<who>.json, and whatever is over -GutCutsShown=1.0 cm deep
 *                         with twenty points or more is photographed from where it shows (for-review/screenshots).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitCutsTest, "Ashmarket.Fit.Cuts", GutTests::Flags)

bool FGutFitCutsTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	FString Named;

	if (!FApp::CanEverRender() || !FParse::Value(FCommandLine::Get(), TEXT("GutFitBodies="), Named))
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FString> Bodies;
	Named.ParseIntoArray(Bodies, TEXT("+"), true);

	float Shown = 1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("GutCutsShown="), Shown);

	float Tolerance = 0.35f;
	FParse::Value(FCommandLine::Get(), TEXT("GutCutsTolerance="), Tolerance);

	struct FFound
	{
		FString Who;
		FString Stood;
		TArray<TSharedPtr<FJsonValue>> Lines;
		bool bPicture = false;
	};

	TSharedRef<FFound> Found = MakeShared<FFound>();
	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->SetHazardsEnabled(false);
		Fight.GameMode->StartFight();

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 600.0f, 420.0f, 180.0f);
	} });

	// every way of standing there is: which of them a body is stood in is decided when it is known whose body it is
	static const TCHAR* Ways[] =
	{
		TEXT("stand"), TEXT("stand_f"), TEXT("shift_l"), TEXT("shift_r"), TEXT("crossed"), TEXT("crossed_b"), TEXT("hip_l"), TEXT("hip_r"),
		TEXT("hips"), TEXT("clasp"), TEXT("watch"), TEXT("tied"), TEXT("walk"), TEXT("jog"), TEXT("guard"), TEXT("club"), TEXT("machete"),
	};

	// -GutCutsWhole: the whole figure photographed in every stance, from in front and to one side and from behind and to
	// the other (whole_<who>_<stance>_a and _b), for looking at every body by eye and not only by the measure
	const bool bWhole = FParse::Param(FCommandLine::Get(), TEXT("GutCutsWhole"));

	TArray<FString> OnlyWays;

	if (FString Asked; FParse::Value(FCommandLine::Get(), TEXT("GutCutsWays="), Asked))
	{
		Asked.ParseIntoArray(OnlyWays, TEXT("+"), true);
	}

	for (const FString& Who : Bodies)
	{
		Steps.Add({ 0.6f, [this, Who, Found](const FFight& Fight)
		{
			const TSoftClassPtr<AActor> Body(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who)));

			if (TestNotNull(*FString::Printf(TEXT("There is a body called %s"), *Who), Body.LoadSynchronous()))
			{
				Fight.Player->SetBody(Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->ClearArmor();
			Fight.Player->EquipWeapon(nullptr);
			Fight.Player->SetCollared(false);
			Fight.Player->SetBound(false);
			Fight.Player->SetAtEase(true);

			Found->Who = Who;
			Found->Lines.Reset();

			// a camera near enough that the body is posed at its finest
			LookFrom(Fight, { TEXT("near"), FVector(260.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") });
		} });

		for (const TCHAR* Way : Ways)
		{
			const FString Stood(Way);

			// -GutCutsWays=tied+clasp: only those ways of standing
			if (!OnlyWays.IsEmpty() && !OnlyWays.Contains(Stood))
			{
				continue;
			}

			Steps.Add({ 0.15f, [Stood, Found](const FFight& Fight)
			{
				AGutFighter* Body = Fight.Player;

				Found->Stood.Reset();
				Found->bPicture = false;

				Body->SetLedVelocity(FVector::ZeroVector);

				const bool bFighting = Stood == TEXT("guard") || Stood == TEXT("club") || Stood == TEXT("machete");

				// a fighter's guard with nothing in the hand, with the club and with the machete
				Body->EquipWeapon(Stood == TEXT("club") ? LoadObject<UGutWeaponData>(nullptr, TEXT("/Game/Gut/Weapons/DA_Gut_Club.DA_Gut_Club"))
					: (Stood == TEXT("machete") ? LoadObject<UGutWeaponData>(nullptr, TEXT("/Game/Gut/Weapons/DA_Gut_Machete.DA_Gut_Machete")) : nullptr));

				if (Stood == TEXT("tied") || Stood == TEXT("walk") || Stood == TEXT("jog") || bFighting)
				{
					Body->SetAtEase(!bFighting);
					Body->SetBound(Stood == TEXT("tied"));

					if (!bFighting)
					{
						Body->SetAtEaseFamily(Body->IsWoman() ? FName("stand_f") : FName("stand"));
					}

					Body->SetLedVelocity(Body->GetActorForwardVector() * (Stood == TEXT("walk") ? 153.0f : (Stood == TEXT("jog") ? 326.0f : 0.0f)));
				}
				else
				{
					// only the ways this body's sex is dealt
					const FGutStandFamily* Family = Body->GetAtEaseFamilies().FindByPredicate([&Stood](const FGutStandFamily& One) { return One.Name == FName(*Stood); });

					// the ways this body's sex is dealt, and the ways that are only ever given by name (a Warden's clasped hands)
					if (!Family || (Family->DealtTo != 0 && !(Family->DealtTo & (Body->IsWoman() ? 2 : 1))))
					{
						return;
					}

					Body->SetBound(false);
					Body->SetAtEase(true);
					Body->SetAtEaseFamily(Family->Name);
				}

				Found->Stood = Stood;

				// held where it is: somebody walking is looked at in the same place
				Body->SetActorLocation(FVector(0.0f, 0.0f, Body->GetActorLocation().Z));
			} });

			// Not until it has stood that way for a second and a half. Until the evening of 23 November this was a third of a
			// second, which is less than a body takes to ease from one way of standing to the next: what was measured and
			// photographed under each name was mostly the way before it.
			Steps.Add({ 1.5f, [Found, Shown, Tolerance](const FFight& Fight)
			{
				if (Found->Stood.IsEmpty())
				{
					return;
				}

				TArray<FCut> Cuts;
				TArray<FString> Parts;

				MeasureCuts(Fight.Player, Tolerance, Cuts, Found->Lines.IsEmpty() ? &Parts : nullptr);

				for (const FString& Part : Parts)
				{
					UE_LOG(LogGut, Log, TEXT("[cuts] %s wears: %s"), *Found->Who, *Part);
				}

				const FCut* Worst = nullptr;

				for (const FCut& Cut : Cuts)
				{
					if (Cut.Points < 6 || Cut.Deepest < Tolerance * 1.5f)
					{
						continue;
					}

					UE_LOG(LogGut, Log, TEXT("[cuts] %s | %s | %s | %s | %d | %.1f"), *Found->Who, *Found->Stood, *Cut.Garment, *Cut.Region, Cut.Points, Cut.Deepest);

					TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();

					Line->SetStringField(TEXT("who"), Found->Who);
					Line->SetStringField(TEXT("stood"), Found->Stood);
					Line->SetStringField(TEXT("garment"), Cut.Garment);
					Line->SetStringField(TEXT("where"), Cut.Region);
					Line->SetNumberField(TEXT("points"), Cut.Points);
					Line->SetNumberField(TEXT("deep"), Cut.Deepest);

					Found->Lines.Add(MakeShared<FJsonValueObject>(Line));

					if (Cut.Points >= 20 && Cut.Deepest >= Shown && (!Worst || Cut.Deepest * Cut.Points > Worst->Deepest * Worst->Points))
					{
						Worst = &Cut;
					}
				}

				if (Found->Lines.IsEmpty())
				{
					// so that a body with nothing wrong is told from one that was never measured
					TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();

					Line->SetStringField(TEXT("who"), Found->Who);
					Line->SetStringField(TEXT("stood"), Found->Stood);
					Line->SetNumberField(TEXT("points"), 0);

					Found->Lines.Add(MakeShared<FJsonValueObject>(Line));
				}

				FString Text;
				FJsonSerializer::Serialize(Found->Lines, TJsonWriterFactory<>::Create(&Text));
				FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("GutCuts") / (Found->Who + TEXT(".json"))));

				if (Worst)
				{
					// from where it shows: out from the skin there, level, a metre off
					FVector Out = Worst->Outward.GetSafeNormal2D();

					if (Out.IsNearlyZero())
					{
						Out = Fight.Player->GetActorForwardVector();
					}

					LookFrom(Fight, { TEXT("cut"), Worst->Where + (Out * 105.0f) + FVector(0.0f, 0.0f, 12.0f), Worst->Where, 34.0f, NAME_None });

					Found->bPicture = true;
				}
			} });

			Steps.Add({ 0.3f, [Found](const FFight& Fight)
			{
				if (Found->bPicture)
				{
					Screenshot(FString::Printf(TEXT("cuts_%s_%s"), *Found->Who, *Found->Stood));
				}
			} });

			if (bWhole)
			{
				// a picture is of the view that was set the step before it: each view has a step to itself
				for (int32 Side = 0; Side < 2; ++Side)
				{
					Steps.Add({ 0.05f, [Found, Side](const FFight& Fight)
					{
						if (!Found->Stood.IsEmpty())
						{
							const float Way = Side == 0 ? 1.0f : -1.0f;

							// the field of view is the width of a wide picture: on an upright one this takes in the whole figure
							LookFrom(Fight, { TEXT("whole"), FVector(330.0f * Way, 190.0f * Way, 25.0f), FVector(0.0f, 0.0f, -3.0f), 58.0f, FName("pelvis") });
						}
					} });

					Steps.Add({ 0.3f, [Found, Side](const FFight& Fight)
					{
						if (!Found->Stood.IsEmpty())
						{
							Screenshot(FString::Printf(TEXT("whole_%s_%s_%s"), *Found->Who, *Found->Stood, Side == 0 ? TEXT("a") : TEXT("b")));

							Found->bPicture = true;
						}
					} });
				}
			}

			Steps.Add({ 0.05f, [Found](const FFight& Fight)
			{
				if (Found->bPicture)
				{
					LookFrom(Fight, { TEXT("near"), FVector(260.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), 40.0f, FName("pelvis") });
				}
			} });
		}

		Steps.Add({ 0.2f, [](const FFight& Fight)
		{
			// the last body's own is let go before the next is brought in: seventy of them do not fit in memory at once
			GEngine->ForceGarbageCollection(true);
		} });
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Fit.Blink    Pictures of a face at ease, close, ten a second for seven seconds, to see whether it blinks and
 *                         its eyes move (for-review/screenshots, blink_<who>_NN). -GutFitBodies=A+B names who.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGutFitBlinkTest, "Ashmarket.Fit.Blink", GutTests::Flags)

bool FGutFitBlinkTest::RunTest(const FString& Parameters)
{
	using namespace GutTests;
	using namespace GutLookTests;
	using FStep = FScriptCommand::FStep;

	FString Named;

	if (!FApp::CanEverRender() || !FParse::Value(FCommandLine::Get(), TEXT("GutFitBodies="), Named))
	{
		return true;
	}

	if (!OpenCage(this))
	{
		return false;
	}

	TArray<FString> Bodies;
	Named.ParseIntoArray(Bodies, TEXT("+"), true);

	TArray<FStep> Steps;

	Steps.Add({ 0.3f, [this](const FFight& Fight)
	{
		DisableEnemyBrain(this, Fight);

		Fight.GameMode->SetHazardsEnabled(false);
		Fight.GameMode->StartFight();

		PlaceAt(Fight.Player, 0.0f, 0.0f, 0.0f);
		PlaceAt(Fight.Enemy, 600.0f, 420.0f, 180.0f);
	} });

	for (const FString& Who : Bodies)
	{
		Steps.Add({ 2.5f, [this, Who](const FFight& Fight)
		{
			const TSoftClassPtr<AActor> Body(FSoftObjectPath(FString::Printf(TEXT("/Game/Gut/Art/MetaHumans/%s/BP_%s.BP_%s_C"), *Who, *Who, *Who)));

			if (TestNotNull(*FString::Printf(TEXT("There is a body called %s"), *Who), Body.LoadSynchronous()))
			{
				Fight.Player->SetBody(Body, Fight.Player->GetBodyRetargeter());
			}

			Fight.Player->ClearArmor();
			Fight.Player->EquipWeapon(nullptr);
			Fight.Player->SetCollared(false);
			Fight.Player->SetBound(false);
			Fight.Player->SetAtEase(true);

			LookFrom(Fight, { TEXT("face"), FVector(62.0f, 10.0f, 2.0f), FVector(0.0f, 0.0f, 3.0f), 24.0f, FName("head") });
		} });

		// -GutFitSpeak=/Game/Town/Voices/main_street/Faces/warden_loop_face has the face say that line as the pictures begin
		Steps.Add({ 0.05f, [](const FFight& Fight)
		{
			FString Line;

			if (FParse::Value(FCommandLine::Get(), TEXT("GutFitSpeak="), Line))
			{
				const FString Name = FPaths::GetBaseFilename(Line);

				Fight.Player->Speak(LoadObject<UAnimSequenceBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Line, *Name)));

				// from in front of where the face is turned, wide enough to hold the whole of it
				const USkeletalMeshComponent* Seen = Fight.Player->GetShownMesh();
				const FTransform Head = Seen->GetBoneTransform(Seen->GetBoneIndex(FName("head")));
				const FVector Middle = Head.GetLocation() + (Fight.Player->GetActorForwardVector() * 6.0f) + FVector(0.0f, 0.0f, 16.0f);

				LookFrom(Fight, { TEXT("mouth"), Middle + (Fight.Player->GetActorForwardVector() * 70.0f), Middle, 22.0f, NAME_None });
			}
		} });

		for (int32 i = 0; i < 70; ++i)
		{
			Steps.Add({ 0.1f, [Who, i](const FFight& Fight) { Screenshot(FString::Printf(TEXT("blink_%s_%02d"), *Who, i)); } });
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FScriptCommand(this, MoveTemp(Steps)));

	return true;
}
