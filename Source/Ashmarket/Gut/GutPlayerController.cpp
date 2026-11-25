// Ashmarket prototype: the Gut.

#include "GutPlayerController.h"
#include "GutCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GutFighter.h"
#include "GutGameMode.h"
#include "InputMappingContext.h"
#include "GutGameInstance.h"
#include "TownGameMode.h"

AGutPlayerController::AGutPlayerController()
{
	// screens wider than 16:9 see more to the sides and lose nothing from top to bottom
	PlayerCameraManagerClass = AGutCameraManager::StaticClass();
}

void AGutPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// the fight takes the mouse
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	// the rack inputs come and go with the rack
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>())
	{
		GameMode->OnPhaseChanged.AddDynamic(this, &AGutPlayerController::OnPhaseChanged);
	}
}

void AGutPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (int32 i = 0; i < DefaultMappingContexts.Num(); ++i)
			{
				Subsystem->AddMappingContext(DefaultMappingContexts[i], i);
			}
		}
	}

	// the menu that stops the game is worked by the keys themselves: every other input is asleep while the game is stopped.
	// None of these takes its key away from anything else.
	if (InputComponent)
	{
		auto Bind = [this](const FKey& Key, void (AGutPlayerController::*Handler)())
		{
			FInputKeyBinding& Binding = InputComponent->BindKey(Key, IE_Pressed, this, Handler);

			Binding.bExecuteWhenPaused = true;
			Binding.bConsumeInput = false;
		};

		Bind(EKeys::Escape, &AGutPlayerController::StopPressed);
		Bind(EKeys::Gamepad_Special_Left, &AGutPlayerController::StopPressed);

		for (const FKey& Key : { EKeys::W, EKeys::Up, EKeys::Gamepad_DPad_Up })
		{
			Bind(Key, &AGutPlayerController::StoppedUpPressed);
		}

		for (const FKey& Key : { EKeys::S, EKeys::Down, EKeys::Gamepad_DPad_Down })
		{
			Bind(Key, &AGutPlayerController::StoppedDownPressed);
		}

		for (const FKey& Key : { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom })
		{
			Bind(Key, &AGutPlayerController::StoppedConfirmPressed);
		}

		// and Backspace, or the gamepad's B, backs out of whatever screen is up, where there is anywhere to back out to
		for (const FKey& Key : { EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right })
		{
			Bind(Key, &AGutPlayerController::BackPressed);
		}

		// the town's map goes up and comes down on M, or the gamepad's Y
		for (const FKey& Key : { EKeys::M, EKeys::Gamepad_FaceButton_Top })
		{
			Bind(Key, &AGutPlayerController::MapPressed);
		}

		// the bag opens and shuts on I, or right on the gamepad's D-pad: only the street has one
		for (const FKey& Key : { EKeys::I, EKeys::Gamepad_DPad_Right })
		{
			Bind(Key, &AGutPlayerController::BagPressed);
		}

		// Enter says yes on every screen there is, and it is not a key anybody fights with, so it also goes on from a fight that is over
		Bind(EKeys::Enter, &AGutPlayerController::EnterPressed);

		// the street has two keys of its own, bound the same plain way: nothing in the cage listens for either
		for (const FKey& Key : { EKeys::E, EKeys::Gamepad_FaceButton_Left })
		{
			Bind(Key, &AGutPlayerController::InteractPressed);
		}

		Bind(EKeys::Gamepad_LeftThumbstick, &AGutPlayerController::RunPressed);

		// the street's camera: C locks it where it is, V goes through its set heights. On a pad, the D-pad's up and down
		// (in the street nothing else uses them: right is the bag).
		for (const FKey& Key : { EKeys::C, EKeys::Gamepad_DPad_Up })
		{
			Bind(Key, &AGutPlayerController::CameraLockPressed);
		}

		for (const FKey& Key : { EKeys::V, EKeys::Gamepad_DPad_Down })
		{
			Bind(Key, &AGutPlayerController::CameraStepPressed);
		}

		for (const FKey& Key : { EKeys::LeftShift, EKeys::Gamepad_LeftShoulder })
		{
			Bind(Key, &AGutPlayerController::JogPressed);

			FInputKeyBinding& Released = InputComponent->BindKey(Key, IE_Released, this, &AGutPlayerController::JogReleased);

			Released.bExecuteWhenPaused = true;
			Released.bConsumeInput = false;
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(RestartAction, ETriggerEvent::Started, this, &AGutPlayerController::RestartPressed);

		// The rack
		EnhancedInputComponent->BindAction(RackPreviousAction, ETriggerEvent::Started, this, &AGutPlayerController::RackPreviousPressed);
		EnhancedInputComponent->BindAction(RackNextAction, ETriggerEvent::Started, this, &AGutPlayerController::RackNextPressed);
		EnhancedInputComponent->BindAction(RackOpponentAction, ETriggerEvent::Started, this, &AGutPlayerController::RackOpponentPressed);
		EnhancedInputComponent->BindAction(RackDownAction, ETriggerEvent::Started, this, &AGutPlayerController::RackDownPressed);
		EnhancedInputComponent->BindAction(RackConfirmAction, ETriggerEvent::Started, this, &AGutPlayerController::RackConfirmPressed);
	}
}

void AGutPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// start looking the way the fighter faces, slightly from above
	SetControlRotation(FRotator(StartingCameraPitch, InPawn->GetActorRotation().Yaw, 0.0f));
}

bool AGutPlayerController::IsCameraHeld() const
{
	const UGutGameInstance* Run = Cast<UGutGameInstance>(GetGameInstance());

	if (!Run || !Run->bCameraLocked || !GetWorld() || !GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		return false;
	}

	const AGutFighter* Walker = Cast<AGutFighter>(GetPawn());

	return Walker && Walker->IsAtEase();
}

void AGutPlayerController::AddPitchInput(float Val)
{
	// He locked it and could still push it up and down, and it came back when he let go. Locked, it does not move at all.
	if (IsCameraHeld())
	{
		return;
	}

	Super::AddPitchInput(Val);
}

void AGutPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// The street's camera, locked: it is brought to its height and kept there, whatever the mouse or the stick asks of it
	// up and down. Turning is left alone.
	if (IsCameraHeld())
	{
		const UGutGameInstance* Run = Cast<UGutGameInstance>(GetGameInstance());

		FRotator Looking = GetControlRotation();

		Looking.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(Looking.Pitch), Run->CameraLockedPitch, DeltaTime, 8.0f);

		SetControlRotation(Looking);
	}

	// a run begun on the stick ends when the stick is let come back, as a run does on any pad
	if (bRunByStick)
	{
		const float Pushed = FVector2D(GetInputAnalogKeyState(EKeys::Gamepad_LeftX), GetInputAnalogKeyState(EKeys::Gamepad_LeftY)).Size();

		if (Pushed < 0.3f && !IsInputKeyDown(EKeys::Gamepad_LeftShoulder) && !IsInputKeyDown(EKeys::LeftShift))
		{
			bRunByStick = false;

			if (AGutFighter* Walker = Cast<AGutFighter>(GetPawn()))
			{
				Walker->SetJogging(false);
				Walker->SetRunning(false);
			}
		}
	}

	// at a betting screen a key that is held runs the stake up or down, a litre at a time, after a moment's wait: the first
	// litre is the press itself
	const AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>();
	const bool bBetting = GameMode && GameMode->GetPhase() == EGutFightPhase::Betting && !GameMode->IsStopped();

	const bool bMore = bBetting && (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up) || IsInputKeyDown(EKeys::Gamepad_DPad_Up));
	const bool bLess = bBetting && (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down) || IsInputKeyDown(EKeys::Gamepad_DPad_Down));

	if (bMore == bLess)
	{
		StakeHeldFor = 0.0f;
		StakeRunFor = 0.0f;
		return;
	}

	StakeHeldFor += DeltaTime;

	if (StakeHeldFor < StakeRunDelay)
	{
		return;
	}

	StakeRunFor += DeltaTime;

	while (StakeRunFor >= StakeRunEvery)
	{
		StakeRunFor -= StakeRunEvery;

		SendMenuInput(bMore ? EGutMenuInput::Up : EGutMenuInput::Down);
	}
}

void AGutPlayerController::RestartPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>())
	{
		GameMode->RequestRestart();
	}
}

void AGutPlayerController::RackPreviousPressed()
{
	SendMenuInput(EGutMenuInput::Left);
}

void AGutPlayerController::RackNextPressed()
{
	SendMenuInput(EGutMenuInput::Right);
}

void AGutPlayerController::RackOpponentPressed()
{
	SendMenuInput(EGutMenuInput::Up);
}

void AGutPlayerController::RackDownPressed()
{
	SendMenuInput(EGutMenuInput::Down);
}

void AGutPlayerController::RackConfirmPressed()
{
	SendMenuInput(EGutMenuInput::Confirm);
}

void AGutPlayerController::SendMenuInput(EGutMenuInput Input)
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>())
	{
		GameMode->HandleMenuInput(Input);
	}
}

void AGutPlayerController::SetAtRack(bool bNewAtRack)
{
	if (bAtRack == bNewAtRack)
	{
		return;
	}

	bAtRack = bNewAtRack;

	// the keys that walk the fighter choose weapons instead while it stands at the rack
	SetIgnoreMoveInput(bAtRack);

	if (!IsLocalPlayerController() || !RackMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (bAtRack)
		{
			// above every other context, so its keys are not also read as fight inputs
			Subsystem->AddMappingContext(RackMappingContext, DefaultMappingContexts.Num());
		}
		else
		{
			Subsystem->RemoveMappingContext(RackMappingContext);
		}
	}
}

void AGutPlayerController::OnPhaseChanged(EGutFightPhase NewPhase)
{
	SetAtRack(NewPhase == EGutFightPhase::Card || NewPhase == EGutFightPhase::Choosing || NewPhase == EGutFightPhase::Betting || NewPhase == EGutFightPhase::Gate);
}

void AGutPlayerController::StopPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>())
	{
		GameMode->SetStopped(!GameMode->IsStopped());
	}
	else if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->SetStopped(!Town->IsStopped());
	}
}

void AGutPlayerController::BackPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>())
	{
		GameMode->HandleBack();
	}
	else if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->HandleBack();
	}
}

void AGutPlayerController::BagPressed()
{
	if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->ToggleBag();
	}
}

void AGutPlayerController::MapPressed()
{
	if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->ToggleMap();
	}
}

void AGutPlayerController::StoppedUpPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>(); GameMode && GameMode->IsStopped())
	{
		GameMode->HandleStoppedInput(EGutMenuInput::Up);
	}
	else if (GameMode && !bAtRack && GameMode->CanLeaveForStreet())
	{
		// a fight that was watched is over: the same keys move between going on and going back down to the street
		GameMode->HandleMenuInput(EGutMenuInput::Up);
	}
	else if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		// the same keys choose among answers when somebody has been spoken to
		Town->HandleMenuInput(EGutMenuInput::Up);
	}
}

void AGutPlayerController::StoppedDownPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>(); GameMode && GameMode->IsStopped())
	{
		GameMode->HandleStoppedInput(EGutMenuInput::Down);
	}
	else if (GameMode && !bAtRack && GameMode->CanLeaveForStreet())
	{
		GameMode->HandleMenuInput(EGutMenuInput::Down);
	}
	else if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->HandleMenuInput(EGutMenuInput::Down);
	}
}

void AGutPlayerController::StoppedConfirmPressed()
{
	if (AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>(); GameMode && GameMode->IsStopped())
	{
		GameMode->HandleStoppedInput(EGutMenuInput::Confirm);
	}
	else if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->HandleMenuInput(EGutMenuInput::Confirm);
	}
}

void AGutPlayerController::EnterPressed()
{
	AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>();

	if (GameMode && !GameMode->IsStopped())
	{
		GameMode->RequestRestart();
		return;
	}

	StoppedConfirmPressed();
}

void AGutPlayerController::InteractPressed()
{
	if (ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>())
	{
		Town->Interact();
	}
}

void AGutPlayerController::JogPressed()
{
	if (AGutFighter* Walker = Cast<AGutFighter>(GetPawn()); Walker && Walker->IsAtEase())
	{
		// Held, a jog. Let go and pressed again at once and held, a flat-out run: three paces on one key
		// (22 November: "I prefer walk, fast walk, and run").
		const double Now = FPlatformTime::Seconds();

		Walker->SetJogging(true);
		Walker->SetRunning(Now - JogLetGo < 0.4);
	}
}

void AGutPlayerController::JogReleased()
{
	if (AGutFighter* Walker = Cast<AGutFighter>(GetPawn()); Walker && Walker->IsAtEase())
	{
		Walker->SetJogging(false);
		Walker->SetRunning(false);
	}

	JogLetGo = FPlatformTime::Seconds();
}

void AGutPlayerController::CameraLockPressed()
{
	ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>();
	UGutGameInstance* Run = Cast<UGutGameInstance>(GetGameInstance());
	const AGutFighter* Walker = Cast<AGutFighter>(GetPawn());

	// only out in the street, and not at a counter or in a talk, where the same keys choose
	if (!Town || !Run || !Walker || !Walker->IsAtEase() || Town->IsTalking() || Town->IsShopping())
	{
		return;
	}

	Run->bCameraLocked = !Run->bCameraLocked;
	Run->CameraLockedPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);

	Town->ShowNotice(Run->bCameraLocked
		? TEXT("Camera locked at this height. It turns with you, and stays as high as it is. C lets it go.")
		: TEXT("Camera let go."));
}

void AGutPlayerController::CameraStepPressed()
{
	ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>();
	UGutGameInstance* Run = Cast<UGutGameInstance>(GetGameInstance());
	const AGutFighter* Walker = Cast<AGutFighter>(GetPawn());

	if (!Town || !Run || !Walker || !Walker->IsAtEase() || Town->IsTalking() || Town->IsShopping())
	{
		return;
	}

	// at the shoulder, three-quarters overhead, and nearly straight down: the next one steeper than it is now, and round again
	static const float Heights[] = { -6.0f, -42.0f, -68.0f };
	static const TCHAR* Named[] = { TEXT("at your shoulder"), TEXT("three-quarters overhead"), TEXT("overhead") };

	const float Now = FRotator::NormalizeAxis(GetControlRotation().Pitch);

	int32 Next = 0;

	for (int32 i = 0; i < UE_ARRAY_COUNT(Heights); ++i)
	{
		if (Heights[i] < Now - 4.0f)
		{
			Next = i;
			break;
		}
	}

	Run->bCameraLocked = true;
	Run->CameraLockedPitch = Heights[Next];

	Town->ShowNotice(FString::Printf(TEXT("Camera locked %s. C lets it go."), Named[Next]));
}

void AGutPlayerController::RunPressed()
{
	// the left stick pressed in: a flat-out run for as long as the stick is kept pushed
	if (AGutFighter* Walker = Cast<AGutFighter>(GetPawn()); Walker && Walker->IsAtEase())
	{
		Walker->SetJogging(true);
		Walker->SetRunning(true);

		bRunByStick = true;
	}
}
