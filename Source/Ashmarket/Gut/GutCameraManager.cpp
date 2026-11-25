// Ashmarket prototype: the Gut.

#include "GutCameraManager.h"
#include "EngineUtils.h"
#include "GutFighter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const float Framed = 16.0f / 9.0f;
}

float AGutCameraManager::PaniniAcross(float Angle, float D)
{
	return (D + 1.0f) * FMath::Sin(Angle) / (D + FMath::Cos(Angle));
}

void AGutCameraManager::GetWideView(float FramedView, float Aspect, float& OutView, float& OutPanini, float& OutLarger) const
{
	OutView = FramedView;
	OutPanini = 0.0f;
	OutLarger = 1.0f;

	const float Wider = Aspect / Framed;

	if (Wider <= 1.05f)
	{
		return;
	}

	// none on a 16:9 screen, all of it on one twice as wide
	const float D = Panini * FMath::Clamp(Wider - 1.0f, 0.0f, 1.0f);

	// what the engine opens the view to by itself, drawn flat: the tangent of half of it is how far the side of the picture is
	const float FlatSide = FMath::Tan(FMath::DegreesToRadians(FramedView * 0.5f)) * Wider;

	// the half view whose side, through the panini projection, is as far out as that: then the middle is as it was framed
	float Low = 0.0f;
	float High = FMath::DegreesToRadians(WidestView * 0.5f);

	if (PaniniAcross(High, D) <= FlatSide)
	{
		Low = High;
	}
	else
	{
		for (int32 i = 0; i < 24; ++i)
		{
			const float Mid = (Low + High) * 0.5f;

			(PaniniAcross(Mid, D) < FlatSide ? Low : High) = Mid;
		}
	}

	OutPanini = D;
	OutLarger = FlatSide / FMath::Max(PaniniAcross(Low, D), 0.01f);

	// in the engine's own terms: the view as it would be on a 16:9 screen
	OutView = FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(Low) / Wider));
}

void AGutCameraManager::Jar(float Strength)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoJar"));

	if (!bOff)
	{
		Jarred = FMath::Max(Jarred, FMath::Clamp(Strength, 0.0f, 1.0f));
	}
}

void AGutCameraManager::JarViewers(const UObject* WorldContext, float Strength)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;

	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* Viewer = It->Get())
		{
			if (AGutCameraManager* Camera = Cast<AGutCameraManager>(Viewer->PlayerCameraManager))
			{
				Camera->Jar(Strength);
			}
		}
	}
}

void AGutCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// a camera that something behind the walker has pushed right up against their back goes up over their shoulder and
	// looks down, as the cage's camera does: at eye height that close it shows nothing but the back of a pair of shorts
	if (const APawn* Walker = Cast<APawn>(OutVT.Target); Walker && Walker->FindComponentByClass<USpringArmComponent>())
	{
		const FVector Chest = Walker->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		const float Near = FVector::Dist(OutVT.POV.Location, Chest);
		const float Wanted = FMath::GetMappedRangeValueClamped(FVector2f(CloseBy, CloseBy * 0.45f), FVector2f(0.0f, 1.0f), Near);

		Crowded = FMath::FInterpTo(Crowded, Wanted, DeltaTime, 8.0f);

		if (Crowded > 0.01f)
		{
			// as far up as there is room over the walker's own head: a camera pressed against a wall cannot be swept from
			// where it is, so the room is looked for above the walker
			const FVector Head = Walker->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);

			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GutCloseView), false, Walker);

			const bool bRoofed = GetWorld()->LineTraceSingleByChannel(Hit, Head, Head + FVector(0.0f, 0.0f, RaisedWhenClose + 40.0f), ECC_Camera, Params);
			const float Room = bRoofed ? FMath::Max(Hit.Distance - 25.0f, 0.0f) : RaisedWhenClose;
			const float Raised = FMath::Min(RaisedWhenClose * Crowded, Room);

			OutVT.POV.Location.Z += Raised;
			OutVT.POV.Rotation.Pitch -= TiltedWhenClose * (RaisedWhenClose > 0.0f ? Raised / RaisedWhenClose : 0.0f);
		}
	}
	else
	{
		Crowded = 0.0f;
	}

	// Anybody the camera has come right up against is left out of the picture while it is there: somebody led by the wrists
	// walks a pace behind the walker, which is where the camera is when a wall has pushed it in, and the back of their head
	// was all there was to see. Only in the street: in the cage the two who fight are always seen.
	if (const AGutFighter* Seen = Cast<AGutFighter>(OutVT.Target); Seen && Seen->IsAtEase())
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoCameraHide"));

		bAnyLeftOut = false;

		for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
		{
			AGutFighter* Other = *It;

			if (Other == Seen)
			{
				continue;
			}

			// how near the camera is to the line from their knees to the top of their head
			const float Half = Other->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Low = Other->GetActorLocation() - FVector(0.0f, 0.0f, Half * 0.4f);
			const FVector High = Other->GetActorLocation() + FVector(0.0f, 0.0f, Half);
			const float Near = FMath::PointDistToSegment(OutVT.POV.Location, Low, High);

			// and only somebody on the camera's side of the one it follows: whoever the walker is face to face with stays,
			// however the camera has been pushed in
			const bool bBehind = FVector::DotProduct((Other->GetActorLocation() - Seen->GetActorLocation()).GetSafeNormal2D(), OutVT.POV.Rotation.Vector().GetSafeNormal2D()) < 0.2f;

			const bool bLeftOut = !bOff && bBehind && Near < (Other->IsLeftOutOfPicture() ? LeftOutWithin + 20.0f : LeftOutWithin);

			Other->SetLeftOutOfPicture(bLeftOut);

			bAnyLeftOut |= bLeftOut;
		}
	}
	else if (bAnyLeftOut)
	{
		// the picture is somebody else's now (the Block's book, a card, the cage): nobody is left out of that one for
		// having been too near this one
		for (TActorIterator<AGutFighter> It(GetWorld()); It; ++It)
		{
			It->SetLeftOutOfPicture(false);
		}

		bAnyLeftOut = false;
	}

	// a blow that landed: the picture is knocked a little and settles, fast
	if (Jarred > 0.005f)
	{
		JarTime += DeltaTime;

		const float Turn = MostJarred * Jarred;

		OutVT.POV.Rotation += FRotator(Turn * FMath::Sin(JarTime * 71.0f), Turn * 0.7f * FMath::Sin((JarTime * 53.0f) + 1.3f), Turn * 0.5f * FMath::Sin((JarTime * 61.0f) + 2.1f));

		Jarred *= FMath::Exp(-JarSettles * DeltaTime);
	}
	else
	{
		Jarred = 0.0f;
		JarTime = 0.0f;
	}

	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("GutNoWideView"));

	float Wanted = 0.0f;

	if (const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr; Viewport && !bOff && !OutVT.POV.bConstrainAspectRatio)
	{
		FVector2D Size;
		Viewport->GetViewportSize(Size);

		if (Size.X > 1.0 && Size.Y > 1.0)
		{
			float View = OutVT.POV.FOV;
			float Larger = 1.0f;

			GetWideView(OutVT.POV.FOV, static_cast<float>(Size.X / Size.Y), View, Wanted, Larger);

			OutVT.POV.FOV = View;

			// a camera on a boom is drawn back from what it follows, so that what it follows is the size it was framed at
			const APawn* Followed = Cast<APawn>(OutVT.Target);
			const USpringArmComponent* Boom = Followed ? Followed->FindComponentByClass<USpringArmComponent>() : nullptr;

			if (Boom && Larger > 1.02f)
			{
				const FVector Back = -OutVT.POV.Rotation.Vector();
				const float Far = Boom->TargetArmLength * FMath::Min(Larger - 1.0f, MostDrawnBack);

				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GutWideView), false, Followed);

				const bool bStopped = GetWorld()->SweepSingleByChannel(Hit, OutVT.POV.Location, OutVT.POV.Location + (Back * Far), FQuat::Identity, ECC_Camera,
					FCollisionShape::MakeSphere(12.0f), Params);

				OutVT.POV.Location += Back * (bStopped ? FMath::Max(Hit.Distance - 2.0f, 0.0f) : Far);
			}
		}
	}

	if (!FMath::IsNearlyEqual(Wanted, PaniniSet, 0.01f))
	{
		PaniniSet = Wanted;

		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LensDistortion.Panini.D")))
		{
			Variable->Set(Wanted, ECVF_SetByCode);
		}
	}
}
