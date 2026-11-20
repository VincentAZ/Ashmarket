// Ashmarket prototype: the Gut.

#include "GutHUD.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GutArmorData.h"
#include "GutFighter.h"
#include "GutGameMode.h"
#include "GutGameInstance.h"
#include "GutHazard.h"
#include "GutLadderData.h"
#include "GutPlayerCharacter.h"
#include "GutWeaponData.h"
#include "Engine/Texture2D.h"
#include "Application/SlateApplicationBase.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

AGutHUD::AGutHUD()
{
	ControlsHelp = {
		TEXT("Move|WASD / left stick"),
		TEXT("Look|Mouse / right stick"),
		TEXT("Light attack|Left mouse / X"),
		TEXT("Heavy attack|F or middle mouse / Y"),
		TEXT("Block (hold)|Right mouse / LB"),
		TEXT("Dodge|Space / B"),
		TEXT("Lock on / off|Tab / right stick click"),
		TEXT("Fight again|Enter / Start")
	};
}

void AGutHUD::DrawHUD()
{
	Super::DrawHUD();

	const AGutGameMode* GameMode = GetWorld()->GetAuthGameMode<AGutGameMode>();

	if (!Canvas || !GameMode)
	{
		return;
	}

	// everything is laid out for 1080p and scaled to fit
	UIScale = Canvas->SizeY / 1080.0f;
	LastPanel = FBox2D(ForceInit);

	const float LayoutWidth = Canvas->SizeX / UIScale;

	// a story being told has the screen to itself
	if (GameMode->IsTellingStory())
	{
		DrawStory(GameMode, LayoutWidth);

		if (GameMode->IsStopped())
		{
			DrawStopped(GameMode, LayoutWidth * 0.5f);
		}

		return;
	}

	// drawn last of all, over everything, when this frame's screen is done
	ON_SCOPE_EXIT
	{
		if (GameMode->IsStopped())
		{
			DrawStopped(GameMode, LayoutWidth * 0.5f);
		}
	};

	const AGutFighter* Player = GameMode->GetPlayerFighter();
	const AGutFighter* Enemy = GameMode->GetEnemyFighter();

	// labels over the fighters' heads
	DrawOverheadText(Player);
	DrawOverheadText(Enemy);

	for (const AGutFighter* Extra : GameMode->GetExtraFighters())
	{
		DrawOverheadText(Extra);
	}

	// with more than two in the cage, the one at the top of the screen is whoever the player is fighting now
	if (GameMode->GetSpecialCard() != EGutSpecialCard::None && Player && Player->GetOpponent() && GameMode->GetPhase() == EGutFightPhase::Fighting)
	{
		Enemy = Player->GetOpponent();
	}

	// somebody watching from the wall has no corner of its own, and nothing to press
	const bool bWatching = (GameMode->IsAtWindow() || GameMode->IsAtStable()) && !GameMode->IsGamblersOwnFight();

	// what the locked camera is looking at
	if (const AGutPlayerCharacter* PlayerCharacter = Cast<AGutPlayerCharacter>(Player); PlayerCharacter && !bWatching)
	{
		// not once it is decided: there is nothing left to aim at, and it sat on the chest of whoever was standing over the loser
		if (PlayerCharacter->IsLockedOn() && GameMode->GetPhase() != EGutFightPhase::PlayerWon && GameMode->GetPhase() != EGutFightPhase::PlayerLost)
		{
			DrawLockOnMarker(Enemy);
		}
	}

	const EGutFightPhase Phase = GameMode->GetPhase();

	// the card and the gate are about the ladder, not about the two fighters standing in the cage
	const bool bBetweenFights = Phase == EGutFightPhase::Card || Phase == EGutFightPhase::Gate;

	// one of the screens that takes inputs of its own, and needs the room
	const bool bScreenUp = bBetweenFights || Phase == EGutFightPhase::Choosing || Phase == EGutFightPhase::Betting;

	// whoever is fighting has its own bars at the bottom, by its hands, and the other's over the fight. Somebody watching has
	// no corner: the two are side by side along the top, each on the side its fighter stands on, with the clock between them
	if (bWatching && !bBetweenFights)
	{
		const bool bTurned = GameMode->IsFirstCornerOnRight();

		DrawFighterPanel(bTurned ? Enemy : Player, (LayoutWidth * 0.5f) - 460.0f, 50.0f, 520.0f, true);
		DrawFighterPanel(bTurned ? Player : Enemy, (LayoutWidth * 0.5f) + 460.0f, 50.0f, 520.0f, true, true);
	}
	else if (!bBetweenFights)
	{
		DrawFighterPanel(Enemy, LayoutWidth * 0.5f, 50.0f, 640.0f, true);
		DrawFighterPanel(Player, 60.0f, 960.0f, 460.0f, false);
	}

	// on a ladder: which fight this is, and the water in hand
	if (GameMode->IsOnLadder() && !bBetweenFights)
	{
		const FGutLadderState& State = GameMode->GetLadderState();

		// a fight that is won has already moved the ladder on
		const int32 FightNumber = FMath::Min(State.Rung + ((GameMode->GetSettlement().bSettled && GameMode->GetSettlement().bPlayerWon) ? 0 : 1), GameMode->GetLadder()->Rungs.Num());

		DrawLabel(FString::Printf(TEXT("%s  \u00B7  FIGHT %d OF %d  \u00B7  %.0f L"), *GameMode->GetLadder()->RankName.ToString().ToUpper(), FightNumber, GameMode->GetLadder()->Rungs.Num(), State.Water),
			60.0f, 926.0f, 16, GutHUDColors::DimText);
	}

	// what the crowd wants, along the bottom where nothing else is
	if (Phase == EGutFightPhase::Fighting && GameMode->IsCrowdCalling())
	{
		DrawLabel(GameMode->GetCrowdCall() == EGutFinisherStyle::Slow ? TEXT("THE CROWD WANTS IT SLOW") : TEXT("THE CROWD WANTS IT CLEAN"),
			LayoutWidth * 0.5f, 1010.0f, 20, GutHUDColors::Warning, true);
	}

	// the fight clock, and the round it is in
	if (!bScreenUp && Phase != EGutFightPhase::Intro)
	{
		const int32 Seconds = FMath::FloorToInt32(GameMode->GetFightTime());

		DrawLabel(FString::Printf(TEXT("ROUND %d    %d:%02d"), GameMode->GetRound(), Seconds / 60, Seconds % 60), LayoutWidth * 0.5f, bWatching ? 88.0f : 136.0f, 16, GutHUDColors::DimText, true);
	}

	if (GameMode->GetPhase() == EGutFightPhase::Fighting)
	{
		DrawHazardNews(GameMode, LayoutWidth * 0.5f);
	}

	// somebody watching has nothing to press, and is told so, with what they have riding on it: at the bottom, under the fight
	if (bWatching && !bScreenUp)
	{
		const EGutBetSide Side = GameMode->GetBetSide();
		const AGutFighter* Backed = Side == EGutBetSide::Player ? GameMode->GetPlayerFighter() : GameMode->GetEnemyFighter();

		DrawLabel(TEXT("YOU ARE WATCHING"), 60.0f, 944.0f, 22, GutHUDColors::Warning);

		DrawLabel((Side == EGutBetSide::None || !Backed || GameMode->GetBetStake() <= 0.0f)
			? FString(TEXT("Nothing of yours is on this one."))
			: FString::Printf(TEXT("Your %.0f L is on %s."), GameMode->GetBetStake(), *Backed->GetDisplayName().ToString()),
			60.0f, 978.0f, 18, GutHUDColors::Text);

		if (Phase == EGutFightPhase::Intro || Phase == EGutFightPhase::Fighting)
		{
			DrawLabel(TEXT("Nothing to press until one of them is down."), 60.0f, 1004.0f, 16, GutHUDColors::DimText);
		}
	}

	if (bShowControls && !bScreenUp && !bWatching)
	{
		float LineY = 60.0f;

		for (const FString& Line : ControlsHelp)
		{
			// the action goes in one column and its inputs in the next
			FString Action = Line;
			FString Inputs;

			Line.Split(TEXT("|"), &Action, &Inputs);

			DrawLabel(Action, 60.0f, LineY, 13, GutHUDColors::DimText);
			DrawLabel(Inputs, 200.0f, LineY, 13, GutHUDColors::DimText);

			LineY += 24.0f;
		}
	}

	// the bell and the verdict
	const float CenterX = LayoutWidth * 0.5f;

	const bool bOnLadder = GameMode->IsOnLadder();
	const bool bAtWindow = GameMode->IsAtWindow();
	const bool bAtStable = GameMode->IsAtStable();
	const FGutStableSettlement& StableSettlement = GameMode->GetStableSettlement();
	const FGutSettlement& Settlement = GameMode->GetSettlement();
	const FGutWindowSettlement& WindowSettlement = GameMode->GetWindowSettlement();

	switch (Phase)
	{
	case EGutFightPhase::Card:

		if (GameMode->IsAtDoor())
		{
			DrawDoor(GameMode, CenterX);
		}
		else if (bAtStable)
		{
			DrawStableCard(GameMode, CenterX);
		}
		else if (bAtWindow)
		{
			DrawWindowCard(GameMode, CenterX);
		}
		else
		{
			DrawCard(GameMode, CenterX);
		}
		break;

	case EGutFightPhase::Choosing:
		DrawRack(GameMode, CenterX);
		break;

	case EGutFightPhase::Betting:

		if (bAtWindow || bAtStable)
		{
			DrawWindowBetting(GameMode, CenterX);
		}
		else
		{
			DrawBettingWindow(GameMode, CenterX);
		}
		break;

	case EGutFightPhase::Gate:

		if (bAtWindow)
		{
			DrawWindowNight(GameMode, CenterX);
		}
		else
		{
			DrawGate(GameMode, CenterX);
		}
		break;

	case EGutFightPhase::Intro:
		DrawLabel(TEXT("THE GUT"), CenterX, 170.0f, 54, GutHUDColors::Text, true);
		DrawLabel((bAtWindow || bAtStable) ? TEXT("One of them walks out.") : (GameMode->IsGamblersOwnFight() ? TEXT("This is what the cage does with a fixer.") : TEXT("One of you walks out.")), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
		DrawLabel(FString::Printf(TEXT("%d"), FMath::CeilToInt32(GameMode->GetIntroTimeRemaining())), CenterX, 300.0f, 40, GutHUDColors::Warning, true);
		break;

	case EGutFightPhase::Fighting:
		// the stagger prompt uses the same spot as the bell
		if (GameMode->GetPhaseTime() < 1.0f && Player && Enemy && !Player->IsStaggered() && !Enemy->IsStaggered())
		{
			DrawLabel(TEXT("FIGHT"), CenterX, 170.0f, 54, GutHUDColors::Warning, true);
		}

		DrawStaggerPrompt(Player, Enemy, CenterX, bWatching);
		break;

	case EGutFightPhase::PlayerWon:

		// a fight that was only watched: who is still standing
		if ((WindowSettlement.bSettled && !WindowSettlement.bOwnFight) || StableSettlement.bSettled)
		{
			const AGutFighter* Standing = (StableSettlement.bSettled ? StableSettlement.bWon : WindowSettlement.Winner == 0) ? Player : Enemy;

			DrawLabel(FString::Printf(TEXT("%s WALKS OUT"), Standing ? *Standing->GetDisplayName().ToString().ToUpper() : TEXT("ONE")), CenterX, 170.0f, 54, GutHUDColors::Text, true);

			// if it was the cage that ended the other one, whoever watched is told which part of it
			const AGutFighter* Down = Standing == Player ? Enemy : Player;

			if (const AGutHazard* Took = Down ? Down->GetKillingHazard() : nullptr)
			{
				DrawLabel(FString::Printf(TEXT("The %s took %s."), *Took->GetDisplayName().ToString().ToLower(), *Down->GetDisplayName().ToString()), CenterX, 255.0f, 22, GutHUDColors::Warning, true);
			}
			else
			{
				DrawLabel(TEXT("The cage's verdict is final."), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
			}

			break;
		}

		DrawLabel(TEXT("YOU WALK OUT"), CenterX, 170.0f, 54, GutHUDColors::Text, true);
		DrawLabel((Enemy && Enemy->GetKillingHazard()) ? TEXT("The cage finished them for you.") : TEXT("The cage's verdict is final."), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
		break;

	case EGutFightPhase::PlayerLost:

		// beaten as arranged, and left alive as arranged
		if (Settlement.bSettled && Settlement.bThrown)
		{
			DrawLabel(TEXT("YOU WENT DOWN"), CenterX, 170.0f, 54, GutHUDColors::Warning, true);
			DrawLabel(TEXT("As arranged. The crowd knows what it saw."), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
			break;
		}

		// beaten, but the opponent would not finish it
		if (Settlement.bSettled && Settlement.bSpared)
		{
			DrawLabel(TEXT("SPARED"), CenterX, 170.0f, 54, GutHUDColors::Warning, true);
			DrawLabel(FString::Printf(TEXT("%s will not finish you."), Enemy ? *Enemy->GetDisplayName().ToString() : TEXT("The opponent")), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
			break;
		}

		DrawLabel(TEXT("YOU ARE MEAT"), CenterX, 170.0f, 54, GutHUDColors::Health, true);
		DrawLabel((Player && Player->GetKillingHazard()) ? TEXT("The cage finished you.") : TEXT("The cage's verdict is final."), CenterX, 255.0f, 22, GutHUDColors::DimText, true);
		break;

	default:
		break;
	}

	const bool bOver = Phase == EGutFightPhase::PlayerWon || Phase == EGutFightPhase::PlayerLost;

	// what the fight paid
	float SettlementBottom = 296.0f;

	if (bOver && bOnLadder && Settlement.bSettled)
	{
		SettlementBottom = DrawSettlement(GameMode, CenterX, 296.0f);
	}

	if (bOver && WindowSettlement.bSettled)
	{
		SettlementBottom = DrawWindowResult(GameMode, CenterX);
	}

	if (bOver && StableSettlement.bSettled)
	{
		SettlementBottom = DrawStableResult(GameMode, CenterX);
	}

	if (GameMode->CanRestart())
	{
		FString Prompt = TEXT("Enter or Start: fight again");
		float PromptY = 300.0f;

		if (StableSettlement.bSettled)
		{
			PromptY = SettlementBottom + 20.0f;
			Prompt = TEXT("Enter or Start: back to the yard");
		}
		else if (WindowSettlement.bSettled)
		{
			PromptY = SettlementBottom + 20.0f;

			if (WindowSettlement.bOwnFight)
			{
				Prompt = WindowSettlement.bSurvived ? TEXT("Enter or Start: back to the window") : TEXT("Enter or Start: somebody else comes to the window");
			}
			else if (WindowSettlement.bCaught)
			{
				Prompt = TEXT("Enter or Start: into the cage");
			}
			else
			{
				Prompt = GameMode->IsWindowNightOver() ? TEXT("Enter or Start: the night's reckoning") : TEXT("Enter or Start: the next fight on the bill");
			}
		}

		if (bOnLadder && Settlement.bSettled)
		{
			PromptY = SettlementBottom + 20.0f;

			if (GameMode->GetLadderState().bFinished)
			{
				Prompt = TEXT("Enter or Start: to the gate");
			}
			else if (Settlement.bPlayerWon)
			{
				Prompt = TEXT("Enter or Start: on to the next fight");
			}
			else
			{
				Prompt = TEXT("Enter or Start: fight it again, as you came to it");
			}
		}

		// whoever only watched chooses: the next fight, or the street
		if (GameMode->CanLeaveForStreet())
		{
			const bool bLeave = GameMode->IsLeaveChosen();

			DrawLabel(FString(bLeave ? TEXT("") : TEXT(">  ")) + (GameMode->IsWindowNightOver() ? TEXT("The night's reckoning") : TEXT("The next fight on the bill")), CenterX, PromptY, 26, bLeave ? GutHUDColors::Text : GutHUDColors::Warning, true);
			DrawLabel(FString(bLeave ? TEXT(">  ") : TEXT("")) + TEXT("Back down the stairs to the street"), CenterX, PromptY + 42.0f, 26, bLeave ? GutHUDColors::Warning : GutHUDColors::Text, true);
			DrawLabel(TEXT("W / S or D-pad: move the arrow          Enter or Start: do it          Backspace or B button: the street"), CenterX, PromptY + 92.0f, 17, GutHUDColors::Text, true);
			return;
		}

		DrawLabel(Prompt, CenterX, PromptY, 26, GutHUDColors::Warning, true);
	}
}

void AGutHUD::LoadLook()
{
	if (bLookLoaded)
	{
		return;
	}

	bLookLoaded = true;

	PanelSteel = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_steel.ui_steel"));
	PanelSlate = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_slate.ui_slate"));
	PanelHoarding = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_hoarding.ui_hoarding"));
	PanelPaint = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_paint.ui_paint"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	FigureStencil = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_figure.ui_figure"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	auto Typeface = [](const TCHAR* File) -> TSharedPtr<const FCompositeFont>
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Gut/Fonts") / File);

		if (!FPaths::FileExists(Path))
		{
			return nullptr;
		}

		return MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	};

	BodyFont = Typeface(TEXT("SpecialElite-Regular.ttf"));
	HeadFont = Typeface(TEXT("StardosStencil-Bold.ttf"));
}

void AGutHUD::DrawPanel(float CenterX, float Y, float Width, float Height, EGutPanel Kind)
{
	LoadLook();

	const float Left = (CenterX - (Width * 0.5f)) * UIScale;
	const float Top = Y * UIScale;
	const float Wide = Width * UIScale;
	const float High = Height * UIScale;

	LastPanel = FBox2D(FVector2D(Left, Top), FVector2D(Left + Wide, Top + High));

	UTexture2D* Surface = Kind == EGutPanel::Slate ? PanelSlate.Get() : (Kind == EGutPanel::Hoarding ? PanelHoarding.Get() : PanelSteel.Get());

	if (!Surface || !Surface->GetResource() || !Canvas || High <= 0.0f)
	{
		DrawRect(GutHUDColors::Card, Left, Top, Wide, High);
		return;
	}

	// the part of the photograph that has the panel's own shape, from its middle, so that nothing is stretched
	const float PictureShape = static_cast<float>(Surface->GetSizeX()) / FMath::Max(1, Surface->GetSizeY());
	const float PanelShape = Wide / High;

	const float U = PanelShape < PictureShape ? PanelShape / PictureShape : 1.0f;
	const float V = PanelShape > PictureShape ? PictureShape / PanelShape : 1.0f;

	// darkened, so that pale type reads on the paler patches
	const float Shade = Kind == EGutPanel::Steel ? 0.6f : (Kind == EGutPanel::Slate ? 0.62f : 0.5f);

	FCanvasTileItem Tile(FVector2D(Left, Top), Surface->GetResource(), FVector2D(Wide, High),
		FVector2D((1.0f - U) * 0.5f, (1.0f - V) * 0.5f), FVector2D((1.0f + U) * 0.5f, (1.0f + V) * 0.5f), FLinearColor(Shade, Shade, Shade, 1.0f));

	Tile.BlendMode = SE_BLEND_Opaque;

	Canvas->DrawItem(Tile);
}

void AGutHUD::DrawStopped(const AGutGameMode* GameMode, float CenterX)
{
	const TArray<FString> Lines = GameMode->GetStoppedLines();

	const float LineHeight = 44.0f;
	const float Top = 330.0f;

	// the rest of the screen stands back
	if (Canvas)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
	}

	DrawPanel(CenterX, Top, 620.0f, 150.0f + (Lines.Num() * LineHeight));

	DrawLabel(TEXT("STOPPED"), CenterX, Top + 22.0f, 44, GutHUDColors::Text, true);

	float Y = Top + 104.0f;

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const bool bChosen = i == GameMode->GetStoppedChoice();

		DrawLabel((bChosen ? TEXT(">  ") : TEXT("")) + Lines[i], CenterX, Y, 24, bChosen ? GutHUDColors::Warning : GutHUDColors::Text, true);

		Y += LineHeight;
	}

	DrawLabel(TEXT("W / S: choose        Enter: do it        Esc: go on"), CenterX, Y + 8.0f, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawStory(const AGutGameMode* GameMode, float LayoutWidth)
{
	const FGutStoryPanel* Panel = GameMode->GetStoryPanel();

	if (!Panel || !Canvas)
	{
		return;
	}

	LoadLook();

	TObjectPtr<UTexture2D>& Picture = StoryPictures.FindOrAdd(Panel->Picture);

	if (!Picture)
	{
		Picture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/UI/%s.%s"), *Panel->Picture.ToString(), *Panel->Picture.ToString()), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	const float Wide = Canvas->SizeX;
	const float High = Canvas->SizeY;

	DrawRect(FLinearColor::Black, 0.0f, 0.0f, Wide, High);

	if (Picture && Picture->GetResource())
	{
		// the whole screen covered, from the middle of the photograph, nothing stretched
		const float PictureShape = static_cast<float>(Picture->GetSizeX()) / FMath::Max(1, Picture->GetSizeY());
		const float ScreenShape = Wide / FMath::Max(High, 1.0f);

		const float U = ScreenShape < PictureShape ? ScreenShape / PictureShape : 1.0f;
		const float V = ScreenShape > PictureShape ? PictureShape / ScreenShape : 1.0f;

		FCanvasTileItem Tile(FVector2D(0.0f, 0.0f), Picture->GetResource(), FVector2D(Wide, High),
			FVector2D((1.0f - U) * 0.5f, (1.0f - V) * 0.5f), FVector2D((1.0f + U) * 0.5f, (1.0f + V) * 0.5f), FLinearColor::White);

		Tile.BlendMode = SE_BLEND_Opaque;

		Canvas->DrawItem(Tile);
	}

	// the words on a dark band along the bottom, so that they read whatever is behind them. A line too long for the
	// screen it is on is carried over to the next.
	TArray<FString> Words;

	for (const FText& Line : Panel->Words)
	{
		Words.Append(WrapLabel(Line.ToString(), 26, LayoutWidth - 240.0f));
	}

	const float LineHeight = 40.0f;
	const float BandHeight = 76.0f + (Words.Num() * LineHeight);
	const float BandTop = 1080.0f - BandHeight;

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f, BandTop * UIScale, Wide, BandHeight * UIScale);

	float Y = BandTop + 22.0f;

	for (const FString& Line : Words)
	{
		DrawLabel(Line, LayoutWidth * 0.5f, Y, 26, GutHUDColors::Text, true);

		Y += LineHeight;
	}

	DrawLabel(FString::Printf(TEXT("%d of %d          Left mouse, Enter or A button: go on"), GameMode->GetStoryIndex() + 1, GameMode->GetStoryLength()),
		LayoutWidth * 0.5f, Y + 6.0f, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawCard(const AGutGameMode* GameMode, float CenterX)
{
	const FGutLadderRung* Rung = GameMode->GetRung();

	if (!Rung)
	{
		return;
	}

	const UGutLadderData* Ladder = GameMode->GetLadder();
	const FGutLadderState& State = GameMode->GetLadderState();

	const TArray<FGutCardItem> Items = GameMode->GetCardItems();

	const float LineHeight = 32.0f;
	const float ItemHeight = 28.0f;
	const float Top = 110.0f;

	DrawPanel(CenterX, Top, 980.0f, 268.0f + (Rung->Card.Num() * LineHeight) + (Items.Num() * ItemHeight));

	float Y = Top + 24.0f;

	DrawLabel(FString::Printf(TEXT("%s  \u00B7  FIGHT %d OF %d"), *Ladder->RankName.ToString().ToUpper(), State.Rung + 1, Ladder->Rungs.Num()), CenterX, Y, 16, GutHUDColors::DimText, true);

	Y += 34.0f;

	// the story so far
	for (const FText& Line : Rung->Card)
	{
		DrawLabel(Line.ToString(), CenterX, Y, 20, GutHUDColors::Text, true);

		Y += LineHeight;
	}

	Y += 14.0f;

	// who is waiting in the cage
	DrawLabel(Rung->OpponentName.ToString().ToUpper(), CenterX, Y, 44, GutHUDColors::Health, true);

	Y += 66.0f;

	// who it is, and what it has won in this cage since the player came to it
	const int32 Won = GameMode->GetCageWins(GameMode->GetLadderOpponent(State.Rung));

	DrawLabel(Rung->Billing.ToString() + (Won <= 0 ? FString() : FString::Printf(TEXT("  Has won %d here."), Won)), CenterX, Y, 18, GutHUDColors::DimText, true);

	Y += 46.0f;

	// the state the player comes to it in
	FString Injuries;
	FString WorstLimb;

	static const TCHAR* LimbNames[GutNumLimbs] = { TEXT("left arm"), TEXT("right arm"), TEXT("left leg"), TEXT("right leg") };

	if (State.bInjuriesCarried && GameMode->GetPlayerFighter())
	{
		const float MaxLimbHealth = GameMode->GetPlayerFighter()->GetMaxLimbHealth();

		for (int32 i = 0; i < GutNumLimbs; ++i)
		{
			if (State.LimbHealth[i] <= 0.0f)
			{
				Injuries += FString::Printf(TEXT("%s%s crippled"), Injuries.IsEmpty() ? TEXT("") : TEXT(", "), LimbNames[i]);

				if (WorstLimb.IsEmpty())
				{
					WorstLimb = LimbNames[i];
				}
			}
			else if (State.LimbHealth[i] < MaxLimbHealth)
			{
				Injuries += FString::Printf(TEXT("%s%s hurt"), Injuries.IsEmpty() ? TEXT("") : TEXT(", "), LimbNames[i]);
			}
		}
	}

	FString Holding = FString::Printf(TEXT("You hold %.0f L."), State.Water);

	if (State.Debt > 0.0f)
	{
		Holding += FString::Printf(TEXT("  You owe %.0f L."), State.Debt);
	}

	DrawLabel(FString::Printf(TEXT("%s  %s"), *Holding, Injuries.IsEmpty() ? TEXT("You are whole.") : *(TEXT("You carry: ") + Injuries + TEXT("."))),
		CenterX, Y, 18, Injuries.IsEmpty() ? GutHUDColors::Text : GutHUDColors::Warning, true);

	Y += 40.0f;

	// what can be done before the fight, a line each, the last being the way out
	const FString Cutter = Ladder->CutterName.IsEmpty() ? FString(TEXT("The cutter")) : Ladder->CutterName.ToString();

	for (const FGutCardItem& Item : Items)
	{
		FString Line;
		bool bAvailable = true;

		switch (Item.Type)
		{
		case EGutCardItemType::Patch:
			bAvailable = State.Water - Ladder->PatchPrice >= -Ladder->SlateLimit;

			Line = bAvailable
				? FString::Printf(TEXT("%s patches your %s: %.0f L"), *Cutter, *WorstLimb, Ladder->PatchPrice)
				: FString::Printf(TEXT("%s wants %.0f L for your %s, and the slate will not stretch to it"), *Cutter, Ladder->PatchPrice, *WorstLimb);
			break;

		case EGutCardItemType::Sponsor:

			if (const FGutSponsor* Sponsor = GameMode->GetSponsorOffer())
			{
				switch (Sponsor->Demand)
				{
				case EGutSponsorDemand::WearColors:
					Line = FString::Printf(TEXT("%s: %.0f L for a win in their colors, with no armor over them"), *Sponsor->SponsorName.ToString(), Sponsor->Pay);
					break;

				case EGutSponsorDemand::ThrowFight:
					Line = FString::Printf(TEXT("%s: %.0f L to go down. They have paid the other corner to leave you breathing"), *Sponsor->SponsorName.ToString(), Sponsor->Pay);
					break;

				default:
					Line = FString::Printf(TEXT("%s: %.0f L for a win finished the slow way"), *Sponsor->SponsorName.ToString(), Sponsor->Pay);
					break;
				}

				if (GameMode->IsSponsorAccepted())
				{
					Line += TEXT("   [TAKEN]");
				}
			}
			break;

		case EGutCardItemType::Drug:
		{
			const EGutDrug Drug = static_cast<EGutDrug>(Item.Index);
			const float Habit = GameMode->GetDrugHabit(Drug);

			Line = Drug == EGutDrug::Dust
				? FString::Printf(TEXT("Dust  \u00B7  faster, and a heavier hand, for this fight  \u00B7  %.0f L"), GameMode->GetDrugPrice(Drug))
				: FString::Printf(TEXT("Hush  \u00B7  a blow stops you half as long, for this fight  \u00B7  %.0f L"), GameMode->GetDrugPrice(Drug));

			if (GameMode->IsDrugTaken(Drug))
			{
				Line += TEXT("   [TAKEN]");
			}
			else if (GameMode->IsInWithdrawal(Drug))
			{
				Line += Drug == EGutDrug::Dust ? TEXT("   you are slow without it") : TEXT("   you are half yourself without it");
			}
			else if (Habit > 0.0f)
			{
				Line += Habit >= 0.67f ? TEXT("   it has hold of you") : TEXT("   you have had it before");
			}

			bAvailable = GameMode->IsDrugTaken(Drug) || State.Water - GameMode->GetDrugPrice(Drug) >= -Ladder->SlateLimit;
			break;
		}

		case EGutCardItemType::Armor:

			if (const UGutArmorData* Piece = Ladder->ArmorShop[Item.Index])
			{
				// what it takes off a blow that lands on it
				FString Effect;

				if (Piece->BluntScale < 1.0f)
				{
					Effect += FString::Printf(TEXT("blunt -%.0f%%"), (1.0f - Piece->BluntScale) * 100.0f);
				}

				if (Piece->BladedScale < 1.0f)
				{
					Effect += FString::Printf(TEXT("%sbladed -%.0f%%"), Effect.IsEmpty() ? TEXT("") : TEXT(", "), (1.0f - Piece->BladedScale) * 100.0f);
				}

				const bool bWorn = GameMode->IsArmorWorn(Piece);

				bAvailable = bWorn || State.Water - Piece->Price >= -Ladder->SlateLimit;

				Line = FString::Printf(TEXT("%s  \u00B7  %s"), *Piece->DisplayName.ToString(), *Effect);
				Line += bWorn ? FString(TEXT("   [WORN]")) : FString::Printf(TEXT("  \u00B7  %.0f L"), Piece->Price);
			}
			break;

		case EGutCardItemType::Door:
			Line = TEXT("Back out to the street");
			break;

		case EGutCardItemType::Special:
		{
			const EGutSpecialCard Offer = GameMode->GetSpecialOffer();

			Line = Offer == EGutSpecialCard::Animal
				? FString::Printf(TEXT("The pit boss's card: the dogs, %d of them, in place of %s  \u00B7  %.0f L"),
					GameMode->GetDogsOnCard(), *Rung->OpponentName.ToString(), FMath::RoundToFloat(Rung->Purse * GameMode->GetSpecialPurseScale(Offer)))
				: Offer == EGutSpecialCard::OneAgainstThree
				? FString::Printf(TEXT("The pit boss's card: you alone against %s and two of the new meat  \u00B7  %.0f L"),
					*Rung->OpponentName.ToString(), FMath::RoundToFloat(Rung->Purse * GameMode->GetSpecialPurseScale(Offer)))
				: FString::Printf(TEXT("The pit boss's card: two on two, you and a second against %s and its own  \u00B7  %.0f L"),
					*Rung->OpponentName.ToString(), FMath::RoundToFloat(Rung->Purse * GameMode->GetSpecialPurseScale(Offer)));

			if (GameMode->IsSpecialAccepted())
			{
				Line += TEXT("   [TAKEN]");
			}
			break;
		}

		default:
			Line = TEXT("To the rack, and the fight");
			break;
		}

		const bool bSelected = Item == GameMode->GetCardSelection() || (Item.Type == EGutCardItemType::Rack && !Items.Contains(GameMode->GetCardSelection()));

		DrawLabel((bSelected ? TEXT(">  ") : TEXT("")) + Line, CenterX, Y, 18,
			bSelected ? GutHUDColors::Warning : (bAvailable ? GutHUDColors::Text : GutHUDColors::DimText), true);

		Y += ItemHeight;
	}

	Y += 8.0f;

	DrawLabel(TEXT("W / S or D-pad: choose          Left mouse, Enter or A button: do it"), CenterX, Y, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawBettingWindow(const AGutGameMode* GameMode, float CenterX)
{
	const UGutLadderData* Ladder = GameMode->GetLadder();
	const AGutFighter* Enemy = GameMode->GetEnemyFighter();

	if (!Ladder || !Enemy)
	{
		return;
	}

	const FGutLadderState& State = GameMode->GetLadderState();
	const FString EnemyName = Enemy->GetDisplayName().ToString().ToUpper();

	const EGutBetSide Side = GameMode->GetBetSide();
	const float Stake = GameMode->GetBetStake();

	// the stake is its own line and its own keys, before a side is chosen and after
	const float Wanted = GameMode->GetBetWanted();

	DrawPanel(CenterX, 140.0f, 980.0f, 410.0f, EGutPanel::Slate);

	DrawLabel(TEXT("THE WINDOW"), CenterX, 152.0f, 34, GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("The book gives you %.0f chances in 100."), GameMode->GetBookChance() * 100.0f), CenterX, 208.0f, 18, GutHUDColors::DimText, true);

	// what a litre brings back on each side, with the stake the screen is set to in the pot
	const float OnPlayer = GameMode->GetReturnPerLitre(EGutBetSide::Player, Wanted);
	const float OnEnemy = GameMode->GetReturnPerLitre(EGutBetSide::Opponent, Wanted);

	DrawLabel(FString::Printf(TEXT("ON YOU: %.2f back for a litre"), OnPlayer), CenterX - 230.0f, 248.0f, 18,
		Side == EGutBetSide::Player ? GutHUDColors::Warning : GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("ON %s: %.2f back for a litre"), *EnemyName, OnEnemy), CenterX + 230.0f, 248.0f, 18,
		Side == EGutBetSide::Opponent ? GutHUDColors::Warning : GutHUDColors::Text, true);

	DrawLabel(TEXT("<  A          WHO ON          D  >"), CenterX, 280.0f, 17, GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("STAKE  %.0f L"), Wanted), CenterX, 312.0f, 30, GutHUDColors::Warning, true);

	DrawLabel(FString::Printf(TEXT("W: a litre more          S: a litre less          hold either to run          from 1 L to %.0f L"), GameMode->GetMaxStake()), CenterX, 368.0f, 17, GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("You hold %.0f L. The window takes no more than %.0f L on one fight, the slate covering what you have not got."), State.Water, Ladder->MaxBet),
		CenterX, 398.0f, 16, GutHUDColors::DimText, true);

	// the bet the player is about to place
	FString Bet = TEXT("NO BET YET: A OR D PUTS IT ON ONE OF YOU");

	if (Side != EGutBetSide::None)
	{
		const float Return = Stake * (Side == EGutBetSide::Player ? OnPlayer : OnEnemy);

		Bet = FString::Printf(TEXT("%.0f L ON %s  \u00B7  BRINGS BACK %.0f L"), Stake, Side == EGutBetSide::Player ? TEXT("YOURSELF") : *EnemyName, Return);
	}

	DrawLabel(Bet, CenterX, 436.0f, Side == EGutBetSide::None ? 20 : 28, Side == EGutBetSide::None ? GutHUDColors::DimText : GutHUDColors::Warning, true);

	// a bet against yourself only pays if you are there to collect
	const FGutLadderRung* Rung = GameMode->GetRung();

	if (Side == EGutBetSide::Opponent && Rung && !Rung->bSparesPlayer && !GameMode->IsThrowingFight())
	{
		DrawLabel(TEXT("If it wins, you will not be there to collect."), CenterX, 478.0f, 16, GutHUDColors::Health, true);
	}

	DrawLabel(TEXT("Left mouse, Enter or A button: to the cage"), CenterX, 508.0f, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawStableCard(const AGutGameMode* GameMode, float CenterX)
{
	using namespace GutStableLines;

	const FGutStableState& State = GameMode->GetStableState();
	const TArray<FGutCardItem> Items = GameMode->GetCardItems();
	const EGutStablePage Page = GameMode->GetStablePage();

	TArray<FGutStableFighter> AtBlock;
	GameMode->GetBlockOffers(AtBlock);

	TArray<int32> OnBill;
	GameMode->GetStableOffers(OnBill);

	const float ItemHeight = 30.0f;
	const float Top = 96.0f;

	DrawPanel(CenterX, Top, 1180.0f, 250.0f + (Items.Num() * ItemHeight));

	float Y = Top + 22.0f;

	static const TCHAR* PageNames[] = { TEXT("THE YARD"), TEXT("THE YARD"), TEXT("THE BLOCK"), TEXT("THE BILL"), TEXT("THE BILL"), TEXT("THE COLD ROOM") };

	DrawLabel(FString::Printf(TEXT("%s  \u00B7  DAY %d"), PageNames[static_cast<int32>(Page)], State.Day), CenterX, Y, 16, GutHUDColors::DimText, true);

	Y += 36.0f;

	FString Holding = FString::Printf(TEXT("You hold %.0f L."), State.Water);

	if (State.Debt > 0.0f)
	{
		Holding += FString::Printf(TEXT("  You owe %.0f L."), State.Debt);
	}

	Holding += FString::Printf(TEXT("  %d of %d places in the yard are taken. Each mouth is %.0f L a day."), State.Fighters.Num(), GameMode->GetStableMostFighters(), GameMode->GetFeedPerDay());

	DrawLabel(Holding, CenterX, Y, 18, GutHUDColors::Text, true);

	Y += 40.0f;

	/** How a fighter reads on one line: what it has been taught, what state it is in, and what it thinks */
	auto Describe = [GameMode](const FGutStableFighter& Fighter, bool bMood)
	{
		FString Text = FString::Printf(TEXT("%s  \u00B7  strength %d, speed %d, nerve %d  \u00B7  %s"), *Fighter.FighterName.ToString().ToUpper(),
			Fighter.Strength, Fighter.Speed, Fighter.Nerve, Fighter.Weapon ? *Fighter.Weapon->DisplayName.ToString() : TEXT("bare hands"));

		if (Fighter.Wins > 0)
		{
			Text += FString::Printf(TEXT("  \u00B7  %d %s"), Fighter.Wins, Fighter.Wins == 1 ? TEXT("win") : TEXT("wins"));
		}

		if (Fighter.IsCrippled())
		{
			Text += TEXT("  \u00B7  crippled");
		}

		if (Fighter.bDoped)
		{
			Text += TEXT("  \u00B7  on Dust");
		}
		else if (Fighter.Habit >= 0.5f)
		{
			Text += TEXT("  \u00B7  needs Dust");
		}

		if (bMood)
		{
			if (!Fighter.bOwned)
			{
				Text += FString::Printf(TEXT("  \u00B7  signed, %.0f L a day"), Fighter.Wage);
			}
			else if (Fighter.Resentment >= 75.0f)
			{
				Text += TEXT("  \u00B7  will not look at you");
			}
			else if (Fighter.Resentment >= 55.0f)
			{
				Text += TEXT("  \u00B7  sullen");
			}
			else if (Fighter.bPromisedTag)
			{
				Text += TEXT("  \u00B7  promised its tag");
			}
		}

		return Text;
	};

	// what the page is about, above its lines
	if (Page == EGutStablePage::Fighter && State.Fighters.IsValidIndex(GameMode->GetStableFocus()))
	{
		const FGutStableFighter& Fighter = State.Fighters[GameMode->GetStableFocus()];

		DrawLabel(Describe(Fighter, true), CenterX, Y, 20, GutHUDColors::Health, true);
		DrawLabel(Fighter.Billing.ToString(), CenterX, Y + 30.0f, 15, GutHUDColors::DimText, true);

		if (GameMode->IsTagDue(GameMode->GetStableFocus()))
		{
			DrawLabel(TEXT("It has won what you asked of it. It is waiting for its tag."), CenterX, Y + 54.0f, 16, GutHUDColors::Warning, true);
		}
	}
	else if (Page == EGutStablePage::Block)
	{
		DrawLabel(TEXT("The Chainmen lead them out one at a time. The last has a tag, and will sign."), CenterX, Y + 14.0f, 16, GutHUDColors::DimText, true);
	}
	else if (Page == EGutStablePage::Bill || Page == EGutStablePage::Pick)
	{
		DrawLabel(Page == EGutStablePage::Pick ? TEXT("Who goes in?") : TEXT("The pit boss has these fights to make. One of yours against one of his."), CenterX, Y + 14.0f, 16, GutHUDColors::DimText, true);
	}
	else if (Page == EGutStablePage::Body)
	{
		DrawLabel(FString::Printf(TEXT("%s is on the pit boss's table, and is yours to see to."), *State.BodyOf.ToString()), CenterX, Y, 20, GutHUDColors::Health, true);
		DrawLabel(TEXT("The yard will know by morning where it went."), CenterX, Y + 30.0f, 15, GutHUDColors::DimText, true);
	}
	else if (State.Fighters.IsEmpty())
	{
		DrawLabel(TEXT("The yard is empty. Fighters are bought at the Block, or signed there for a wage."), CenterX, Y + 14.0f, 16, GutHUDColors::DimText, true);
	}

	Y += 86.0f;

	for (const FGutCardItem& Item : Items)
	{
		FString Line;
		bool bAvailable = true;

		const int32 Code = Item.Index;

		if (Item.Type == EGutCardItemType::Door)
		{
			Line = TEXT("Out of the yard gate to the street");
		}
		else if (Code >= SendFighter)
		{
			Line = State.Fighters.IsValidIndex(Code - SendFighter) ? Describe(State.Fighters[Code - SendFighter], true) : FString();
		}
		else if (Code >= TakeFight)
		{
			const int32 Against = OnBill.IsValidIndex(Code - TakeFight) ? OnBill[Code - TakeFight] : -1;

			if (const FGutLadderRung* Rung = GameMode->FindRung(Against))
			{
				Line = FString::Printf(TEXT("%s  \u00B7  %s  \u00B7  %s  \u00B7  pays %.0f L"), *Rung->OpponentName.ToString().ToUpper(), *GameMode->FindRank(Against)->RankName.ToString(),
					Rung->Weapon ? *Rung->Weapon->DisplayName.ToString() : TEXT("bare hands"), GameMode->GetStablePurse(Against));
			}
		}
		else if (Code >= Buy)
		{
			if (AtBlock.IsValidIndex(Code - Buy))
			{
				const FGutStableFighter& Offer = AtBlock[Code - Buy];

				Line = Describe(Offer, false) + (Offer.bOwned ? FString::Printf(TEXT("  \u00B7  %.0f L"), Offer.Price) : FString::Printf(TEXT("  \u00B7  signs for %.0f L a day and a share"), Offer.Wage));

				bAvailable = Offer.Price <= State.Water && State.Fighters.Num() < GameMode->GetStableMostFighters();
			}
		}
		else if (Code >= OpenFighter)
		{
			Line = State.Fighters.IsValidIndex(Code - OpenFighter) ? Describe(State.Fighters[Code - OpenFighter], true) : FString();
		}
		else
		{
			const FGutStableFighter* Fighter = State.Fighters.IsValidIndex(GameMode->GetStableFocus()) ? &State.Fighters[GameMode->GetStableFocus()] : nullptr;

			switch (Code)
			{
			case ToBlock:
				Line = TEXT("To the Block");
				break;

			case ToBill:
				Line = TEXT("To the pit boss, for a fight");
				break;

			case Wait:
				Line = TEXT("Let the day go by");
				break;

			case TrainStrength:
			case TrainSpeed:
			case TrainNerve:
			{
				static const TCHAR* What[] = { TEXT("strength"), TEXT("speed"), TEXT("nerve") };

				const int32 Taught = !Fighter ? 0 : (Code == TrainStrength ? Fighter->Strength : (Code == TrainSpeed ? Fighter->Speed : Fighter->Nerve));

				Line = FString::Printf(TEXT("Train %s  \u00B7  %.0f L and the day"), What[Code - TrainStrength], GameMode->GetTrainingCost());

				bAvailable = Taught < GameMode->GetStableMostTaught() && GameMode->GetTrainingCost() <= State.Water;
				break;
			}

			case Patch:
				Line = FString::Printf(TEXT("Doc Stitch sees to a limb  \u00B7  %.0f L"), GameMode->GetStablePatchCost());
				bAvailable = GameMode->GetStablePatchCost() <= State.Water;
				break;

			case Dope:
				Line = (Fighter && Fighter->bDoped) ? FString(TEXT("Dusted for its next fight")) : FString::Printf(TEXT("Give it Dust for its next fight  \u00B7  %.0f L"), GameMode->GetDopeCost());
				bAvailable = Fighter && !Fighter->bDoped && GameMode->GetDopeCost() <= State.Water;
				break;

			case Promise:
				Line = FString::Printf(TEXT("Promise it its tag for %d wins"), GameMode->GetWinsForTag());
				break;

			case HandOverTag:
				Line = FString::Printf(TEXT("Buy its tag at the Pump office and hand it over  \u00B7  %.0f L"), GameMode->GetTagPrice());
				bAvailable = GameMode->GetTagPrice() <= State.Water;
				break;

			case DenyTag:
				Line = TEXT("Tell it there will be no tag");
				break;

			case BodyToGround:
				Line = FString::Printf(TEXT("The ground behind the pumps, with the yard standing by  \u00B7  costs %.0f L"), -GameMode->GetBodyPrice(EGutBodyGoesTo::Ground));
				bAvailable = -GameMode->GetBodyPrice(EGutBodyGoesTo::Ground) <= State.Water;
				break;

			case BodyToParts:
				Line = FString::Printf(TEXT("Doc Stitch, for parts  \u00B7  pays %.0f L"), GameMode->GetBodyPrice(EGutBodyGoesTo::Parts));
				break;

			case BodyToGrill:
				Line = FString::Printf(TEXT("The grill carts  \u00B7  pays %.0f L"), GameMode->GetBodyPrice(EGutBodyGoesTo::Grill));
				break;

			case WalkOut:
				Line = TEXT("Out of the yard gate to the street, with it walking behind you");
				break;

			case Sell:
				Line = (Fighter && !Fighter->bOwned) ? FString(TEXT("Let it go")) : FString::Printf(TEXT("Sell it at the Block  \u00B7  about %.0f L back"), Fighter ? FMath::RoundToFloat(Fighter->Price * 0.8f * (Fighter->IsCrippled() ? 0.5f : 1.0f)) : 0.0f);
				break;

			default:
				Line = TEXT("Back");
				break;
			}
		}

		const bool bSelected = Item == GameMode->GetCardSelection();

		DrawLabel((bSelected ? TEXT(">  ") : TEXT("")) + Line, CenterX, Y, 18,
			bSelected ? GutHUDColors::Warning : (bAvailable ? GutHUDColors::Text : GutHUDColors::DimText), true);

		Y += ItemHeight;
	}

	Y += 8.0f;

	DrawLabel(TEXT("W / S or D-pad: choose          Left mouse, Enter or A button: do it          A or D-pad left: back a page"), CenterX, Y, 17, GutHUDColors::Text, true);

	// whoever stands in the yard has its name at its feet, and the one the choice is on is picked out
	for (int32 i = 0; i < State.Fighters.Num(); ++i)
	{
		const AGutFighter* Body = GameMode->GetYardBody(i);

		if (!Body)
		{
			continue;
		}

		const FVector At = Project(Body->GetActorLocation() - FVector(0.0f, 0.0f, Body->GetSimpleCollisionHalfHeight() + 4.0f), false);

		if (At.Z <= 0.0f)
		{
			continue;
		}

		const FGutCardItem Chosen = GameMode->GetCardSelection();

		const bool bPicked = (Page == EGutStablePage::Fighter && GameMode->GetStableFocus() == i)
			|| (Chosen.Type == EGutCardItemType::Stable && (Chosen.Index == OpenFighter + i || Chosen.Index == SendFighter + i));

		DrawLabelAtScreen(State.Fighters[i].FighterName.ToString().ToUpper(), At.X, At.Y, 22, bPicked ? GutHUDColors::Warning : GutHUDColors::Text, true);
		DrawLabelAtScreen(State.Fighters[i].bOwned ? TEXT("collared") : TEXT("signed"), At.X, At.Y + (30.0f * UIScale), 15, GutHUDColors::DimText, true);
	}
}

float AGutHUD::DrawStableResult(const AGutGameMode* GameMode, float CenterX)
{
	const FGutStableSettlement& Result = GameMode->GetStableSettlement();
	const FGutStableState& State = GameMode->GetStableState();

	TArray<FString> Lines;

	if (Result.bWon)
	{
		Lines.Add(Result.FightersShare > 0.0f
			? FString::Printf(TEXT("The purse is %.0f L. %s keeps %.0f of it."), Result.Purse, *Result.FighterName.ToString(), Result.FightersShare)
			: FString::Printf(TEXT("The purse is %.0f L, and it is yours."), Result.Purse));

		if (Result.bTagDue)
		{
			Lines.Add(FString::Printf(TEXT("%s has won what you asked of it. It is waiting for its tag."), *Result.FighterName.ToString()));
		}
	}
	else
	{
		Lines.Add(Result.bLayDown
			? FString::Printf(TEXT("%s did not fight. That was what it thought of you."), *Result.FighterName.ToString())
			: FString::Printf(TEXT("%s is dead, and what you paid for it with it."), *Result.FighterName.ToString()));
	}

	if (Result.Backed >= 0)
	{
		Lines.Add(Result.Return > 0.0f
			? FString::Printf(TEXT("The window pays %.0f L on your %.0f."), Result.Return, Result.Stake)
			: FString::Printf(TEXT("The window keeps your %.0f L."), Result.Stake));
	}

	if (Result.DebtPaid > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("%.0f L of what came in is kept against what you owe."), Result.DebtPaid));
	}

	Lines.Add(FString::Printf(TEXT("You hold %.0f L."), State.Water));

	float Y = 296.0f;

	for (const FString& Line : Lines)
	{
		DrawLabel(Line, CenterX, Y, 20, GutHUDColors::Text, true);

		Y += 30.0f;
	}

	return Y;
}

void AGutHUD::DrawDoor(const AGutGameMode* GameMode, float CenterX)
{
	const UGutGameInstance* GameInstance = GameMode->GetGameInstance<UGutGameInstance>();

	if (!GameInstance)
	{
		return;
	}

	const TArray<FGutCardItem> Items = GameMode->GetCardItems();

	const float ItemHeight = 34.0f;
	const float Top = 150.0f;

	DrawPanel(CenterX, Top, 1320.0f, 268.0f + (Items.Num() * ItemHeight), EGutPanel::Hoarding);

	float Y = Top + 22.0f;

	DrawLabel(TEXT("ASHMARKET"), CenterX, Y, 16, GutHUDColors::DimText, true);

	Y += 32.0f;

	DrawLabel(TEXT("THE GUT"), CenterX, Y, 44, GutHUDColors::Text, true);

	Y += 78.0f;

	DrawLabel(TEXT("A drained pool with a cage in it. Every fight ends with one of the two dead. There are three ways in."), CenterX, Y, 18, GutHUDColors::Text, true);

	Y += 52.0f;

	for (const FGutCardItem& Item : Items)
	{
		FString Line;

		const bool bAfresh = Item.Index >= GutDoorLines::Afresh;

		switch (Item.Index % GutDoorLines::Afresh)
		{
		case GutDoorLines::Fight:

			if (bAfresh)
			{
				Line = TEXT("...or be sold to it again, from the first fight");
			}
			else if (GameInstance->bLadderStarted)
			{
				const UGutLadderData* Rank = GameMode->GetLadder();

				Line = FString::Printf(TEXT("Through the pens, to fight  \u00B7  %s, fight %d of %d, %.0f L"), Rank ? *Rank->RankName.ToString() : TEXT(""),
					FMath::Min(GameInstance->Ladder.Rung + 1, Rank ? Rank->Rungs.Num() : 1), Rank ? Rank->Rungs.Num() : 1, GameInstance->Ladder.Water);
			}
			else
			{
				Line = TEXT("Through the pens, in a collar, to fight");
			}
			break;

		case GutDoorLines::Own:

			if (bAfresh)
			{
				Line = TEXT("...or start another stable, from an empty yard");
			}
			else if (GameInstance->Stable.bStarted)
			{
				Line = FString::Printf(TEXT("Through the yard gate, to your fighters  \u00B7  day %d, %d in the yard, %.0f L"), GameInstance->Stable.Day, GameInstance->Stable.Fighters.Num(), GameInstance->Stable.Water);
			}
			else
			{
				Line = TEXT("Through the yard gate, with water, to own the ones who fight");
			}
			break;

		case GutDoorLines::Bet:

			if (bAfresh)
			{
				Line = TEXT("...or come back as somebody the window has not seen");
			}
			else if (GameInstance->Window.bStarted)
			{
				Line = FString::Printf(TEXT("Up the stairs to the window, to bet  \u00B7  night %d, %.0f L"), GameInstance->Window.Night, GameInstance->Window.Water);
			}
			else
			{
				Line = TEXT("Up the stairs to the window, to bet on them");
			}
			break;

		default:
			Line = TEXT("A single fight, with nothing on it");
			break;
		}

		const bool bSelected = Item == GameMode->GetCardSelection();

		DrawLabel((bSelected ? TEXT(">  ") : TEXT("")) + Line, CenterX, Y, bAfresh ? 16 : 20,
			bSelected ? GutHUDColors::Warning : (bAfresh ? GutHUDColors::DimText : GutHUDColors::Text), true);

		Y += ItemHeight;
	}

	Y += 12.0f;

	DrawLabel(TEXT("W / S or D-pad: choose          Left mouse, Enter or A button: go in"), CenterX, Y, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawWindowCard(const AGutGameMode* GameMode, float CenterX)
{
	const FGutLadderRung* Corners[2] = { GameMode->GetWindowRung(0), GameMode->GetWindowRung(1) };
	const UGutLadderData* Ranks[2] = { GameMode->GetWindowRank(0), GameMode->GetWindowRank(1) };

	if (!Corners[0] || !Corners[1] || !Ranks[0] || !Ranks[1])
	{
		return;
	}

	const FGutWindowState& State = GameMode->GetWindowState();
	const TArray<FGutCardItem> Items = GameMode->GetCardItems();

	const float ItemHeight = 28.0f;
	const float Top = 96.0f;

	DrawPanel(CenterX, Top, 1100.0f, 402.0f + (Items.Num() * ItemHeight), EGutPanel::Slate);

	float Y = Top + 22.0f;

	DrawLabel(FString::Printf(TEXT("THE WINDOW  \u00B7  NIGHT %d  \u00B7  FIGHT %d OF %d"), State.Night, State.Fight + 1, GameMode->GetWindowFightsPerNight()), CenterX, Y, 16, GutHUDColors::DimText, true);

	Y += 40.0f;

	// the two of them, a column each, on the side of the slate its fighter stands on behind it
	for (int32 Corner = 0; Corner < 2; ++Corner)
	{
		const float X = CenterX + ((Corner == 0) == GameMode->IsFirstCornerOnRight() ? 270.0f : -270.0f);
		const FGutLadderRung* Rung = Corners[Corner];

		// each in its own half of the slate, clear of the word between them
		DrawLabel(Rung->OpponentName.ToString().ToUpper(), X, Y, 34, GutHUDColors::Health, true, 440.0f);
		DrawLabel(Rung->Billing.ToString(), X, Y + 52.0f, 15, GutHUDColors::DimText, true, 500.0f);
		DrawLabel(FString::Printf(TEXT("%s  \u00B7  %s"), *Ranks[Corner]->RankName.ToString(), Rung->Weapon ? *Rung->Weapon->DisplayName.ToString() : TEXT("Bare hands")), X, Y + 82.0f, 18, GutHUDColors::Text, true);
		// and what it has won here since the gambler first came: the book has counted those
		const int32 Won = GameMode->GetCageWins(GameMode->GetWindowCode(Corner));

		DrawLabel(FString::Printf(TEXT("Fighting for %.0f L%s%s"), Rung->Purse, Rung->Armor.IsEmpty() ? TEXT("") : TEXT("  \u00B7  in armor"),
			Won <= 0 ? TEXT("") : *FString::Printf(TEXT("  \u00B7  has won %d here"), Won)), X, Y + 110.0f, 16, Won > 0 ? GutHUDColors::Text : GutHUDColors::DimText, true);
	}

	DrawLabel(TEXT("AGAINST"), CenterX, Y + 12.0f, 14, GutHUDColors::DimText, true);

	Y += 150.0f;

	const float Chance = GameMode->GetWindowBookChance();

	DrawLabel(FString::Printf(TEXT("The book makes it %s, %.0f chances in 100."), Chance >= 0.5f ? *Corners[0]->OpponentName.ToString() : *Corners[1]->OpponentName.ToString(),
		FMath::Max(Chance, 1.0f - Chance) * 100.0f), CenterX, Y, 18, GutHUDColors::Text, true);

	Y += 32.0f;

	FString Holding = FString::Printf(TEXT("You hold %.0f L."), State.Water);

	if (State.Debt > 0.0f)
	{
		Holding += FString::Printf(TEXT("  You owe the lender %.0f L."), State.Debt);
	}

	if (State.Heat > 0.01f)
	{
		Holding += State.Heat >= 0.3f ? TEXT("  The pit boss is watching you.") : TEXT("  The pit boss has noticed you.");
	}

	DrawLabel(Holding, CenterX, Y, 18, State.Heat >= 0.3f ? GutHUDColors::Warning : GutHUDColors::Text, true);

	Y += 46.0f;

	for (const FGutCardItem& Item : Items)
	{
		const bool bSelected = Item == GameMode->GetCardSelection() || (Item.Type == EGutCardItemType::Rack && !Items.Contains(GameMode->GetCardSelection()));

		// the way on is what anybody came for: it is first, and bigger than what is under it
		if (Item.Type == EGutCardItemType::Rack)
		{
			DrawLabel(FString(bSelected ? TEXT(">  ") : TEXT("")) + TEXT("BET ON THIS FIGHT AND WATCH IT"), CenterX, Y, 24, bSelected ? GutHUDColors::Warning : GutHUDColors::Text, true);

			Y += 50.0f;

			DrawLabel(TEXT("Or first, help it along. One thing, one fighter. If it is noticed the bet is void and you go into the cage."), CenterX, Y, 15, GutHUDColors::DimText, true);

			Y += 30.0f;

			continue;
		}

		FString Line = TEXT("Back down the stairs to the street, with no bet");
		bool bAvailable = true;

		if (Item.Type == EGutCardItemType::Fix)
		{
			const EGutFix Fix = static_cast<EGutFix>(Item.Index / 2);
			const int32 Corner = Item.Index % 2;
			const FString Name = Corners[Corner]->OpponentName.ToString();
			const float Cost = GameMode->GetFixCost(Fix, Corner);
			const float Noticed = GameMode->GetFixDetectChance(Fix) * 100.0f;

			switch (Fix)
			{
			case EGutFix::Bribe:
				Line = FString::Printf(TEXT("Pay %s to go down  \u00B7  %.0f L  \u00B7  noticed %.0f times in 100"), *Name, Cost, Noticed);
				break;

			case EGutFix::DrugBucket:
				Line = FString::Printf(TEXT("Something in %s's water bucket  \u00B7  %.0f L  \u00B7  noticed %.0f times in 100"), *Name, Cost, Noticed);
				break;

			default:
				Line = FString::Printf(TEXT("See to %s's weapon on the rack  \u00B7  %.0f L  \u00B7  noticed %.0f times in 100"), *Name, Cost, Noticed);
				break;
			}

			bAvailable = Cost <= State.Water;

			if (GameMode->GetWindowFix() == Fix && GameMode->GetWindowFixCorner() == Corner)
			{
				Line += TEXT("   [ARRANGED]");
			}
		}

		DrawLabel((bSelected ? TEXT(">  ") : TEXT("")) + Line, CenterX, Y, 18,
			bSelected ? GutHUDColors::Warning : (bAvailable ? GutHUDColors::Text : GutHUDColors::DimText), true);

		Y += ItemHeight;
	}

	Y += 8.0f;

	DrawLabel(TEXT("W / S or D-pad: move the arrow          Left mouse, Enter or A button: do the line the arrow is on"), CenterX, Y, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawWindowBetting(const AGutGameMode* GameMode, float CenterX)
{
	const AGutFighter* Corners[2] = { GameMode->GetPlayerFighter(), GameMode->GetEnemyFighter() };

	if (!Corners[0] || !Corners[1])
	{
		return;
	}

	// the two in the cage are who the betting is on, and whoever is betting holds what it holds
	const float Held = GameMode->IsAtStable() ? GameMode->GetStableState().Water : GameMode->GetWindowState().Water;
	const FString Names[2] = { Corners[0]->GetDisplayName().ToString().ToUpper(), Corners[1]->GetDisplayName().ToString().ToUpper() };
	const EGutBetSide Side = GameMode->GetBetSide();
	const float Stake = GameMode->GetBetStake();

	// the stake is its own line and its own keys, before anybody is chosen and after
	const float Wanted = GameMode->GetBetWanted();

	DrawPanel(CenterX, 140.0f, 1040.0f, 386.0f, EGutPanel::Slate);

	DrawLabel(TEXT("THE WINDOW"), CenterX, 152.0f, 34, GutHUDColors::Text, true);

	const float Chance = GameMode->IsAtStable() ? GameMode->GetStableBookChance() : GameMode->GetWindowBookChance();

	DrawLabel(FString::Printf(TEXT("The book makes it %s, %.0f chances in 100."), Chance >= 0.5f ? *Corners[0]->GetDisplayName().ToString() : *Corners[1]->GetDisplayName().ToString(),
		FMath::Max(Chance, 1.0f - Chance) * 100.0f), CenterX, 210.0f, 16, GutHUDColors::DimText, true);

	// what a litre brings back on each of them, with the stake the screen is set to in the pot
	const float OnFirst = GameMode->GetReturnPerLitre(EGutBetSide::Player, Wanted);
	const float OnSecond = GameMode->GetReturnPerLitre(EGutBetSide::Opponent, Wanted);

	// each on the side its fighter stands on, which is the way left and right choose between them
	const float FirstX = GameMode->IsFirstCornerOnRight() ? 250.0f : -250.0f;

	DrawLabel(FString::Printf(TEXT("ON %s: %.2f back for a litre"), *Names[0], OnFirst), CenterX + FirstX, 244.0f, 18,
		Side == EGutBetSide::Player ? GutHUDColors::Warning : GutHUDColors::Text, true);
	DrawLabel(FString::Printf(TEXT("ON %s: %.2f back for a litre"), *Names[1], OnSecond), CenterX - FirstX, 244.0f, 18,
		Side == EGutBetSide::Opponent ? GutHUDColors::Warning : GutHUDColors::Text, true);

	DrawLabel(TEXT("<  A          WHO ON          D  >"), CenterX, 276.0f, 17, GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("STAKE  %.0f L"), Wanted), CenterX, 312.0f, 30, GutHUDColors::Warning, true);

	DrawLabel(FString::Printf(TEXT("W: a litre more          S: a litre less          hold either to run          from 1 L to %.0f L"), GameMode->GetMaxStake()), CenterX, 368.0f, 17, GutHUDColors::Text, true);

	DrawLabel(FString::Printf(TEXT("You hold %.0f L. The window takes no more than %.0f L on one fight, and the odds shorten as you put it on."), Held, GameMode->GetLadder() ? GameMode->GetLadder()->MaxBet : 0.0f),
		CenterX, 398.0f, 16, GutHUDColors::DimText, true);

	FString Bet = TEXT("NO BET YET: A OR D PUTS IT ON ONE OF THEM");

	if (Side != EGutBetSide::None)
	{
		Bet = FString::Printf(TEXT("%.0f L ON %s  \u00B7  BRINGS BACK %.0f L"), Stake, *Names[Side == EGutBetSide::Player ? 0 : 1], Stake * (Side == EGutBetSide::Player ? OnFirst : OnSecond));
	}

	DrawLabel(Bet, CenterX, 436.0f, Side == EGutBetSide::None ? 20 : 28, Side == EGutBetSide::None ? GutHUDColors::DimText : GutHUDColors::Warning, true);

	DrawLabel(Side == EGutBetSide::None
		? TEXT("Left mouse, Enter or A button: watch it with no bet")
		: TEXT("Left mouse, Enter or A button: place it and watch"), CenterX, 478.0f, 17, GutHUDColors::Text, true);

	// and the way out of it, for whoever has thought better of betting or of watching
	if (GameMode->CanGoBack())
	{
		DrawLabel(GameMode->IsAtStable()
			? TEXT("Backspace or B button: no fight, back to the yard")
			: TEXT("Backspace or B button: no bet, back down the stairs to the street"), CenterX, 504.0f, 17, GutHUDColors::Text, true);
	}
}

float AGutHUD::DrawWindowResult(const AGutGameMode* GameMode, float CenterX)
{
	const FGutWindowSettlement& Result = GameMode->GetWindowSettlement();
	const FGutWindowState& State = GameMode->GetWindowState();

	TArray<FString> Lines;

	if (Result.bOwnFight)
	{
		Lines.Add(Result.bSurvived ? TEXT("You have paid for it. The window is open to you again.") : TEXT("That is what the cage does with those who owe it."));

		if (Result.bSurvived && Result.DebtCleared > 0.0f)
		{
			Lines.Add(FString::Printf(TEXT("The cage's verdict is final: the %.0f L you owed the lender is struck out."), Result.DebtCleared));
		}
	}
	else
	{
		if (Result.bCaught)
		{
			Lines.Add(TEXT("It was noticed. The bet is void and the window keeps your stake."));

			if (Result.Fine > 0.0f)
			{
				Lines.Add(FString::Printf(TEXT("The pit boss takes %.0f L on the spot."), Result.Fine));
			}

			Lines.Add(TEXT("The next fight in the cage is yours."));
		}
		else
		{
			if (Result.Fix != EGutFix::None)
			{
				Lines.Add(TEXT("Nobody noticed what you arranged."));
			}

			if (Result.Backed < 0)
			{
				Lines.Add(TEXT("You had nothing on it."));
			}
			else if (Result.Return > 0.0f)
			{
				Lines.Add(FString::Printf(TEXT("The window pays %.0f L on your %.0f."), Result.Return, Result.Stake));
			}
			else
			{
				Lines.Add(FString::Printf(TEXT("The window keeps your %.0f L."), Result.Stake));
			}
		}

		if (Result.DebtPaid > 0.0f)
		{
			Lines.Add(FString::Printf(TEXT("The lender is standing at the window, and takes %.0f L of it."), Result.DebtPaid));
		}

		if (Result.bStakedAgain)
		{
			Lines.Add(TEXT("You are cleaned out. The lender stakes you again, and writes it down."));
		}

		if (Result.bSentToLender)
		{
			Lines.Add(TEXT("You are cleaned out. Mother Ledger lends, at her card table on Block Square."));
		}

		if (Result.bCalledIn)
		{
			Lines.Add(TEXT("You are cleaned out, and the lender has lent enough. Every debt gets paid: the next fight in the cage is yours."));
		}
	}

	if (!Result.bOwnFight || Result.bSurvived)
	{
		Lines.Add(FString::Printf(TEXT("You hold %.0f L."), State.Water));
	}

	float Y = 296.0f;

	for (const FString& Line : Lines)
	{
		DrawLabel(Line, CenterX, Y, 20, GutHUDColors::Text, true);

		Y += 30.0f;
	}

	return Y;
}

void AGutHUD::DrawWindowNight(const AGutGameMode* GameMode, float CenterX)
{
	const FGutWindowState& State = GameMode->GetWindowState();

	DrawPanel(CenterX, 150.0f, 760.0f, GameMode->CanLeaveForStreet() ? 440.0f : 380.0f, EGutPanel::Slate);

	DrawLabel(FString::Printf(TEXT("NIGHT %d IS OVER"), State.Night), CenterX, 168.0f, 40, GutHUDColors::Text, true);

	float Y = 250.0f;

	auto Sum = [this, CenterX, &Y](const TCHAR* What, float Amount, const FLinearColor& Color)
	{
		DrawLabel(What, CenterX - 300.0f, Y, 20, Color);
		DrawLabel(FString::Printf(TEXT("%+.0f L"), Amount), CenterX + 210.0f, Y, 20, Color);

		Y += 34.0f;
	};

	Sum(TEXT("Put on at the window"), -State.Staked, GutHUDColors::Text);
	Sum(TEXT("Paid back by the window"), State.Returned, GutHUDColors::Text);
	Sum(TEXT("Spent on arrangements"), -State.FixSpend, GutHUDColors::Text);

	if (State.Fines > 0.0f)
	{
		Sum(TEXT("Taken by the pit boss"), -State.Fines, GutHUDColors::Health);
	}

	Y += 10.0f;

	DrawLabel(FString::Printf(TEXT("You hold %.0f L%s"), State.Water, State.Debt > 0.0f ? *FString::Printf(TEXT(", and owe the lender %.0f L."), State.Debt) : TEXT(".")), CenterX, Y, 22, GutHUDColors::Warning, true);

	Y += 50.0f;

	// on to tomorrow night, or back down to the street
	if (GameMode->CanLeaveForStreet())
	{
		const bool bLeave = GameMode->IsLeaveChosen();

		DrawLabel(FString(bLeave ? TEXT("") : TEXT(">  ")) + TEXT("Tomorrow night's bill"), CenterX, Y - 14.0f, 22, bLeave ? GutHUDColors::Text : GutHUDColors::Warning, true);
		DrawLabel(FString(bLeave ? TEXT(">  ") : TEXT("")) + TEXT("Back down the stairs to the street"), CenterX, Y + 18.0f, 22, bLeave ? GutHUDColors::Warning : GutHUDColors::Text, true);
		DrawLabel(TEXT("W / S or D-pad: move the arrow          Left mouse, Enter or A button: do it"), CenterX, Y + 58.0f, 17, GutHUDColors::Text, true);
		return;
	}

	DrawLabel(TEXT("Left mouse, Enter or A button: tomorrow night's bill"), CenterX, Y, 17, GutHUDColors::Text, true);
}

float AGutHUD::DrawSettlement(const AGutGameMode* GameMode, float CenterX, float Y)
{
	const FGutSettlement& Settlement = GameMode->GetSettlement();

	// one line for each thing that changed hands
	TArray<FString> Lines;

	if (Settlement.Purse > 0.0f)
	{
		Lines.Add(Settlement.DebtPaid > 0.0f
			? FString::Printf(TEXT("Purse %.0f L, of which %.0f L is kept against what you owe."), Settlement.Purse, Settlement.DebtPaid)
			: FString::Printf(TEXT("Purse %.0f L."), Settlement.Purse));
	}

	if (Settlement.GateCut > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("Your cut of the gate: %.0f L."), Settlement.GateCut));
	}

	if (Settlement.bSponsored && !Settlement.bThrown && Settlement.SponsorOwed <= 0.0f)
	{
		Lines.Add(Settlement.SponsorPay > 0.0f
			? FString::Printf(TEXT("Your sponsor pays %.0f L."), Settlement.SponsorPay)
			: FString(TEXT("Your sponsor did not get what it paid for, and pays nothing.")));
	}

	if (Settlement.bCrowdCallMatched)
	{
		Lines.Add(FString::Printf(TEXT("The crowd got the finish it called for, and throws in %.0f L."), Settlement.CrowdBonus));
	}

	if (Settlement.BetSide != EGutBetSide::None)
	{
		Lines.Add(Settlement.BetReturn > 0.0f
			? FString::Printf(TEXT("The window pays %.0f L on your %.0f."), Settlement.BetReturn, Settlement.Stake)
			: FString::Printf(TEXT("The window keeps your %.0f L."), Settlement.Stake));
	}

	if (Settlement.bThrown)
	{
		Lines.Add(FString::Printf(TEXT("%.0f L for going down. It cost you %d Name, and the fight is still to be won."), Settlement.SponsorPay, Settlement.NameLost));
	}

	if (Settlement.SponsorOwed > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("You were paid to lose this one. %.0f L goes on what you owe."), Settlement.SponsorOwed));
	}

	// nothing more is said to a fighter that is dead
	if (Settlement.bPlayerWon || Settlement.bSpared || Settlement.bThrown)
	{
		Lines.Add(FString::Printf(TEXT("You hold %.0f L."), GameMode->GetLadderState().Water));
	}

	for (const FString& Line : Lines)
	{
		DrawLabel(Line, CenterX, Y, 20, GutHUDColors::Text, true);

		Y += 30.0f;
	}

	return Y;
}

void AGutHUD::DrawGate(const AGutGameMode* GameMode, float CenterX)
{
	const UGutLadderData* Ladder = GameMode->GetLadder();

	if (!Ladder)
	{
		return;
	}

	const FGutLadderState& State = GameMode->GetLadderState();
	const TArray<FText>& Ending = State.bSpared ? Ladder->SparedEnding : Ladder->WonEnding;

	const float LineHeight = 32.0f;
	const float Top = 100.0f;

	// as tall as the sums that will be listed
	const int32 NumSums = 4 + (State.Extras > 0.0f ? 1 : 0) + (State.ArmorSpend > 0.0f ? 1 : 0) + (State.DebtPaid > 0.0f ? 1 : 0)
		+ (Ladder->bBillAtGate ? 2 : (State.Debt > 0.0f ? 1 : 0));

	DrawPanel(CenterX, Top, 980.0f, 190.0f + (Ending.Num() * LineHeight) + (NumSums * 30.0f));

	float Y = Top + 20.0f;

	DrawLabel(TEXT("THE GATE"), CenterX, Y, 48, GutHUDColors::Text, true);

	Y += 84.0f;

	for (const FText& Line : Ending)
	{
		DrawLabel(Line.ToString(), CenterX, Y, 20, GutHUDColors::Text, true);

		Y += LineHeight;
	}

	Y += 20.0f;

	// the sums, in two columns
	auto DrawSum = [this, CenterX, &Y](const TCHAR* Name, float Amount, bool bSigned, const FLinearColor& Color)
	{
		DrawLabel(Name, CenterX - 240.0f, Y, 20, Color);
		// a sum that comes to nothing has no sign
		const bool bShowSign = bSigned && FMath::Abs(Amount) >= 0.5f;

		DrawLabel(bShowSign ? FString::Printf(TEXT("%+.0f L"), Amount) : FString::Printf(TEXT("%.0f L"), FMath::Abs(Amount)), CenterX + 150.0f, Y, 20, Color);

		Y += 30.0f;
	};

	const FString Cutter = Ladder->CutterName.IsEmpty() ? FString(TEXT("The cutter")) : Ladder->CutterName.ToString();

	DrawSum(TEXT("Purses"), State.Purses, true, GutHUDColors::DimText);
	DrawSum(TEXT("The betting window"), State.BettingNet, true, GutHUDColors::DimText);

	if (State.Extras > 0.0f)
	{
		DrawSum(TEXT("The crowd and the gate"), State.Extras, true, GutHUDColors::DimText);
	}

	DrawSum(*Cutter, -State.PatchSpend, true, GutHUDColors::DimText);

	if (State.ArmorSpend > 0.0f)
	{
		DrawSum(TEXT("Armor"), -State.ArmorSpend, true, GutHUDColors::DimText);
	}

	if (State.DebtPaid > 0.0f)
	{
		DrawSum(TEXT("Kept against your debt"), -State.DebtPaid, true, GutHUDColors::DimText);
	}

	DrawSum(TEXT("In hand at the gate"), State.Water, false, GutHUDColors::Text);

	if (Ladder->bBillAtGate)
	{
		// the pit boss's bill: the player's water goes against it and the rest is owed
		DrawSum(TEXT("Room and board"), -Ladder->GetGateBill(State.Water, State.bSpared), true, GutHUDColors::Text);

		Y += 6.0f;

		DrawSum(TEXT("YOU LEAVE OWING"), State.Debt + Ladder->GetGateDebt(State.Water, State.bSpared), false, GutHUDColors::Warning);
	}
	else if (State.Debt > 0.0f)
	{
		DrawSum(TEXT("YOU STILL OWE"), State.Debt, false, GutHUDColors::Warning);
	}

	Y += 14.0f;

	DrawLabel(Ladder->NextLadder
		? FString::Printf(TEXT("Left mouse, Enter or A button: back into the cage as %s"), *Ladder->NextLadder->RankName.ToString().ToUpper())
		: FString(TEXT("Left mouse, Enter or A button: start again from the first fight")),
		CenterX, Y, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawRack(const AGutGameMode* GameMode, float CenterX)
{
	const TArray<TObjectPtr<UGutWeaponData>>& Weapons = GameMode->GetRackWeapons();

	if (Weapons.IsEmpty())
	{
		return;
	}

	DrawLabel(TEXT("THE RACK"), CenterX, 146.0f, 34, GutHUDColors::Text, true);

	// one card for each weapon, side by side
	const float CardWidth = 300.0f;
	const float CardHeight = 214.0f;
	const float CardGap = 16.0f;
	const float CardsTop = 206.0f;

	const float TotalWidth = (CardWidth * Weapons.Num()) + (CardGap * (Weapons.Num() - 1));

	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		const float CardX = CenterX - (TotalWidth * 0.5f) + ((CardWidth + CardGap) * i);

		DrawWeaponCard(GameMode, Weapons[i], CardX, CardsTop, CardWidth, CardHeight, i == GameMode->GetPlayerChoice(), i == GameMode->GetEnemyChoice());
	}

	// how the drawn weapon fights, in a line
	if (GameMode->GetPlayerFighter() && !GameMode->GetPlayerFighter()->HasWeapon())
	{
		DrawLabel(TEXT("You have no arm left to hold any of them. You fight with what is left of your hands."), CenterX, CardsTop + CardHeight + 18.0f, 18, GutHUDColors::Health, true);
	}
	else if (Weapons.IsValidIndex(GameMode->GetPlayerChoice()))
	{
		DrawLabel(Weapons[GameMode->GetPlayerChoice()]->Description.ToString(), CenterX, CardsTop + CardHeight + 18.0f, 18, GutHUDColors::Text, true);
	}

	// changing the opponent's draw is for trying matchups in single fights. On a ladder its weapon is its own business.
	DrawLabel(GameMode->IsOnLadder()
		? TEXT("A / D or D-pad: choose          Left mouse, Enter or A button: take it to the window")
		: (GameMode->CameByDoor()
			? TEXT("A / D or D-pad: choose          Left mouse, Enter or A button: take it          W or D-pad up: change the opponent's          S or D-pad down: back out to the street")
			: TEXT("A / D or D-pad: choose          Left mouse, Enter or A button: take it          W or D-pad up: change the opponent's")),
		CenterX, CardsTop + CardHeight + 52.0f, 17, GutHUDColors::Text, true);
}

void AGutHUD::DrawWeaponCard(const AGutGameMode* GameMode, const UGutWeaponData* Weapon, float X, float Y, float Width, float Height, bool bPlayers, bool bEnemys)
{
	if (!Weapon)
	{
		return;
	}

	// the best of each number on the rack, to measure the bars against
	float BestReach = 0.0f;
	float BestDamage = 0.0f;
	float BestHeavy = 0.0f;
	float BestTime = UE_BIG_NUMBER;
	float BestGuard = UE_BIG_NUMBER;

	for (const UGutWeaponData* Other : GameMode->GetRackWeapons())
	{
		BestReach = FMath::Max(BestReach, Other->GetLightReach());
		BestDamage = FMath::Max(BestDamage, Other->GetLightDamage());
		BestHeavy = FMath::Max(BestHeavy, Other->HeavyAttack.Damage);
		BestTime = FMath::Min(BestTime, Other->GetLightTime());
		BestGuard = FMath::Min(BestGuard, Other->BlockCostScale);
	}

	DrawRect(GutHUDColors::Card, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);

	// a bar across the top of the card the player has drawn, and one across the bottom of the opponent's
	if (bPlayers && GameMode->CanPlayerHold(Weapon))
	{
		DrawRect(GutHUDColors::Warning, X * UIScale, Y * UIScale, Width * UIScale, 5.0f * UIScale);
	}

	if (bEnemys)
	{
		DrawRect(GutHUDColors::Health, X * UIScale, (Y + Height - 5.0f) * UIScale, Width * UIScale, 5.0f * UIScale);
	}

	// a weapon the player has no working arm for cannot be drawn
	const bool bCanHold = GameMode->CanPlayerHold(Weapon);

	DrawLabel(Weapon->DisplayName.ToString().ToUpper(), X + 16.0f, Y + 16.0f, 18, !bCanHold ? GutHUDColors::DimText : (bPlayers ? GutHUDColors::Warning : GutHUDColors::Text));

	// who has drawn it, along the bottom of the card
	if (!bCanHold)
	{
		DrawLabel(TEXT("NO ARM"), X + 16.0f, Y + Height - 32.0f, 13, GutHUDColors::Health);
	}
	else if (bPlayers)
	{
		DrawLabel(TEXT("YOURS"), X + 16.0f, Y + Height - 32.0f, 13, GutHUDColors::Warning);
	}

	if (bEnemys)
	{
		DrawLabel(TEXT("THE OPPONENT'S"), X + Width - 156.0f, Y + Height - 32.0f, 13, GutHUDColors::Health);
	}

	// each number as a bar against the best on the rack, with the number itself beside it
	const float BarX = X + 88.0f;
	const float BarWidth = 92.0f;
	float RowY = Y + 52.0f;

	auto DrawRow = [&](const TCHAR* Name, float Fraction, const FString& Value)
	{
		DrawLabel(Name, X + 16.0f, RowY, 13, GutHUDColors::DimText);

		DrawRect(GutHUDColors::CardBarBacking, BarX * UIScale, (RowY + 4.0f) * UIScale, BarWidth * UIScale, 10.0f * UIScale);
		DrawRect(GutHUDColors::CardBar, BarX * UIScale, (RowY + 4.0f) * UIScale, BarWidth * FMath::Clamp(Fraction, 0.0f, 1.0f) * UIScale, 10.0f * UIScale);

		DrawLabel(Value, BarX + BarWidth + 10.0f, RowY, 13, GutHUDColors::DimText);

		RowY += 24.0f;
	};

	DrawRow(TEXT("Reach"), Weapon->GetLightReach() / BestReach, FString::Printf(TEXT("%.0f cm"), Weapon->GetLightReach()));
	DrawRow(TEXT("Speed"), BestTime / Weapon->GetLightTime(), FString::Printf(TEXT("%.2f s"), Weapon->GetLightTime()));
	DrawRow(TEXT("Damage"), Weapon->GetLightDamage() / BestDamage, FString::Printf(TEXT("%.1f"), Weapon->GetLightDamage()));
	DrawRow(TEXT("Heavy"), Weapon->HeavyAttack.Damage / BestHeavy, FString::Printf(TEXT("%.0f"), Weapon->HeavyAttack.Damage));
	DrawRow(TEXT("Guard"), BestGuard / Weapon->BlockCostScale, FString::Printf(TEXT("x%.2f cost"), Weapon->BlockCostScale));
}

void AGutHUD::DrawFighterPanel(const AGutFighter* Fighter, float X, float Y, float Width, bool bCentered, bool bFigureBefore)
{
	if (!Fighter)
	{
		return;
	}

	const float Left = bCentered ? X - (Width * 0.5f) : X;

	// the fighter's name and what it is holding
	FString Title = Fighter->GetDisplayName().ToString().ToUpper();

	if (const UGutWeaponData* Armament = Fighter->GetArmament())
	{
		Title += TEXT("  \u00B7  ") + Armament->DisplayName.ToString().ToUpper();
	}

	DrawLabel(Title, bCentered ? X : Left, Y, 18, GutHUDColors::Text, bCentered);

	// an animal has no limbs the rules know of
	if (!Fighter->IsBeast())
	{
		DrawLimbFigure(Fighter, bFigureBefore ? Left - 66.0f : Left + Width + 22.0f, Y + 14.0f);
	}

	DrawBar(Left, Y + 36.0f, Width, 22.0f, Fighter->GetHealth() / Fighter->GetMaxHealth(), GutHUDColors::Health);

	// the stamina bar flashes when an action was refused for lack of it
	const bool bRefused = Fighter->GetTimeSinceStaminaFail() < 0.35f;

	DrawBar(Left, Y + 66.0f, Width, 10.0f, Fighter->GetStamina() / Fighter->GetMaxStamina(), bRefused ? GutHUDColors::StaminaRefused : GutHUDColors::Stamina);
}

void AGutHUD::DrawLimbFigure(const AGutFighter* Fighter, float X, float Y)
{
	auto GetLimbColor = [Fighter](EGutLimb Limb)
	{
		if (Fighter->IsLimbCrippled(Limb))
		{
			return GutHUDColors::LimbCrippled;
		}

		const float Fraction = Fighter->GetLimbHealth(Limb) / Fighter->GetMaxLimbHealth();

		return Fraction > 0.67f ? GutHUDColors::LimbHealthy : (Fraction > 0.34f ? GutHUDColors::LimbHurt : GutHUDColors::LimbBad);
	};

	auto DrawPart = [this](const FLinearColor& Color, float PartX, float PartY, float Width, float Height)
	{
		DrawRect(Color, PartX * UIScale, PartY * UIScale, Width * UIScale, Height * UIScale);
	};

	LoadLook();

	// the figure is seen from behind, like the player's fighter: its left is on the left
	if (FigureStencil && FigureStencil->GetResource())
	{
		// a figure sprayed through a stencil, photographed: the part of the photograph that holds it, and where in that part
		// each arm and leg is, as shares of the whole picture. The whole is drawn in the body's colour and each limb over it in its own.
		const FVector2D From(0.14f, 0.03f);
		const FVector2D To(0.86f, 0.97f);

		const float High = 82.0f;
		const float Wide = High * ((To.X - From.X) * FigureStencil->GetSizeX()) / FMath::Max((To.Y - From.Y) * FigureStencil->GetSizeY(), 1.0f);
		const float Left = X - 6.0f + ((56.0f - Wide) * 0.5f);
		const float Top = Y - 6.0f;

		DrawRect(FLinearColor::Black, (X - 6.0f) * UIScale, Top * UIScale, 56.0f * UIScale, High * UIScale);

		auto Piece = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color)
		{
			DrawPiece(FigureStencil, Left + ((A.X - From.X) / (To.X - From.X) * Wide), Top + ((A.Y - From.Y) / (To.Y - From.Y) * High),
				(B.X - A.X) / (To.X - From.X) * Wide, (B.Y - A.Y) / (To.Y - From.Y) * High, A, B, Color);
		};

		Piece(From, To, GutHUDColors::Body);

		Piece(FVector2D(0.215f, 0.215f), FVector2D(0.349f, 0.59f), GetLimbColor(EGutLimb::LeftArm));
		Piece(FVector2D(0.645f, 0.215f), FVector2D(0.785f, 0.59f), GetLimbColor(EGutLimb::RightArm));
		Piece(FVector2D(0.345f, 0.56f), FVector2D(0.4975f, 0.97f), GetLimbColor(EGutLimb::LeftLeg));
		Piece(FVector2D(0.4975f, 0.56f), FVector2D(0.65f, 0.97f), GetLimbColor(EGutLimb::RightLeg));

		return;
	}

	DrawRect(GutHUDColors::Backing, (X - 6.0f) * UIScale, (Y - 6.0f) * UIScale, 56.0f * UIScale, 82.0f * UIScale);

	DrawPart(GutHUDColors::Body, X + 16.0f, Y, 12.0f, 12.0f);
	DrawPart(GutHUDColors::Body, X + 13.0f, Y + 14.0f, 18.0f, 26.0f);

	DrawPart(GetLimbColor(EGutLimb::LeftArm), X + 3.0f, Y + 14.0f, 8.0f, 28.0f);
	DrawPart(GetLimbColor(EGutLimb::RightArm), X + 33.0f, Y + 14.0f, 8.0f, 28.0f);
	DrawPart(GetLimbColor(EGutLimb::LeftLeg), X + 13.0f, Y + 42.0f, 8.0f, 28.0f);
	DrawPart(GetLimbColor(EGutLimb::RightLeg), X + 23.0f, Y + 42.0f, 8.0f, 28.0f);
}

void AGutHUD::DrawStaggerPrompt(const AGutFighter* Player, const AGutFighter* Enemy, float CenterX, bool bWatching)
{
	if (!Player || !Enemy)
	{
		return;
	}

	// neither of them is whoever is watching, who has nothing to finish and nothing to wait out: only who is down, by name
	if (bWatching)
	{
		const AGutFighter* Down = Enemy->IsStaggered() ? Enemy : (Player->IsStaggered() ? Player : nullptr);

		if (Down)
		{
			DrawLabel(FString::Printf(TEXT("%s IS STAGGERED"), *Down->GetDisplayName().ToString().ToUpper()), CenterX, 190.0f, 40, GutHUDColors::Warning, true);

			if (!Down->IsBeingFinished())
			{
				DrawLabel(FString::Printf(TEXT("Second wind in %.1f"), Down->GetStaggerTimeRemaining()), CenterX, 264.0f, 22, GutHUDColors::Text, true);
			}
		}

		return;
	}

	if (Enemy->IsStaggered())
	{
		DrawLabel(TEXT("STAGGERED"), CenterX, 190.0f, 40, GutHUDColors::Warning, true);

		if (Enemy->IsBeingFinished())
		{
			return;
		}

		// how long is left to do it
		DrawLabel(FString::Printf(TEXT("%.1f"), Enemy->GetStaggerTimeRemaining()), CenterX, 250.0f, 22, GutHUDColors::Text, true);

		// in reach is enough to show the inputs: a press made while the player is still busy is buffered
		// with the spikes or the cable behind it, the heavy blow sends it onto them
		const AGutHazard* Behind = Player->FindDriveHazard(Enemy);

		const FString Prompt = Behind
			? FString::Printf(TEXT("Light attack: clean kill     Heavy attack: onto the %s"), *Behind->GetDisplayName().ToString().ToLower())
			: (Player->GetGapTo(Enemy) <= Player->GetFinisherRange()
				? FString(TEXT("Light attack: clean kill     Heavy attack: slow kill"))
				: FString(TEXT("Get close to finish it")));

		DrawLabel(Prompt, CenterX, 290.0f, 20, GutHUDColors::Text, true);
	}
	else if (Player->IsStaggered())
	{
		DrawLabel(TEXT("YOU ARE STAGGERED"), CenterX, 190.0f, 40, GutHUDColors::Health, true);

		if (!Player->IsBeingFinished())
		{
			DrawLabel(FString::Printf(TEXT("Second wind in %.1f"), Player->GetStaggerTimeRemaining()), CenterX, 250.0f, 22, GutHUDColors::Text, true);
		}
	}
}

void AGutHUD::DrawHazardNews(const AGutGameMode* GameMode, float CenterX)
{
	// one line under the clock: a countdown to the next hazard, or the news that one has just gone live
	float LineY = 162.0f;

	for (const AGutHazard* Hazard : GameMode->GetHazards())
	{
		if (!Hazard)
		{
			continue;
		}

		if (Hazard->GetHazardState() == EGutHazardState::Warning)
		{
			const float TimeLeft = GameMode->GetRoundStartTime(Hazard->GetActivationRound()) - GameMode->GetFightTime();

			DrawLabel(FString::Printf(TEXT("%s IN %d"), *Hazard->GetDisplayName().ToString(), FMath::Max(1, FMath::CeilToInt32(TimeLeft))), CenterX, LineY, 26, GutHUDColors::Warning, true);

			LineY += 34.0f;
		}
		else if (Hazard->IsLive() && Hazard->GetStateTime() < HazardNewsDuration)
		{
			DrawLabel(Hazard->GetLiveText().ToString(), CenterX, LineY, 26, GutHUDColors::Health, true);

			LineY += 34.0f;
		}
	}
}

void AGutHUD::DrawLockOnMarker(const AGutFighter* Target)
{
	if (!Target || !Target->IsAlive())
	{
		return;
	}

	// on the chest of a person, on the back of an animal
	const FVector ScreenPosition = Project(Target->GetActorLocation() + FVector(0.0f, 0.0f, Target->IsBeast() ? -35.0f : 30.0f), false);

	if (ScreenPosition.Z <= 0.0f)
	{
		return;
	}

	// a small hollow square on the chest
	const float Half = 9.0f * UIScale;
	const float Thickness = FMath::Max(1.0f, 2.0f * UIScale);
	const float Left = ScreenPosition.X - Half;
	const float Top = ScreenPosition.Y - Half;

	DrawRect(GutHUDColors::Warning, Left, Top, Half * 2.0f, Thickness);
	DrawRect(GutHUDColors::Warning, Left, Top + (Half * 2.0f) - Thickness, Half * 2.0f, Thickness);
	DrawRect(GutHUDColors::Warning, Left, Top, Thickness, Half * 2.0f);
	DrawRect(GutHUDColors::Warning, Left + (Half * 2.0f) - Thickness, Top, Thickness, Half * 2.0f);
}

void AGutHUD::DrawPiece(UTexture2D* Picture, float X, float Y, float Width, float Height, const FVector2D& From, const FVector2D& To, const FLinearColor& Color, bool bCutOut)
{
	if (!Picture || !Picture->GetResource() || !Canvas || Width <= 0.0f || Height <= 0.0f)
	{
		return;
	}

	FCanvasTileItem Tile(FVector2D(X * UIScale, Y * UIScale), Picture->GetResource(), FVector2D(Width * UIScale, Height * UIScale), From, To, Color);

	Tile.BlendMode = bCutOut ? SE_BLEND_Translucent : SE_BLEND_Opaque;

	Canvas->DrawItem(Tile);
}

void AGutHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor)
{
	LoadLook();

	const float Border = 3.0f;
	const float Full = FMath::Clamp(Fraction, 0.0f, 1.0f);

	if (!PanelPaint || !PanelPaint->GetResource() || !PanelSteel || !PanelSteel->GetResource())
	{
		DrawRect(GutHUDColors::Backing, (X - Border) * UIScale, (Y - Border) * UIScale, (Width + (Border * 2.0f)) * UIScale, (Height + (Border * 2.0f)) * UIScale);
		DrawRect(FillColor, X * UIScale, Y * UIScale, Width * Full * UIScale, Height * UIScale);
		return;
	}

	// a strip of each photograph the shape of the bar, from its middle, so that nothing is stretched; and the paint is not
	// squeezed as the bar empties: what is gone is cut off its end
	auto Strip = [](const UTexture2D* Picture, float Wide, float High)
	{
		const float PictureShape = static_cast<float>(Picture->GetSizeX()) / FMath::Max(1, Picture->GetSizeY());

		return FMath::Clamp(PictureShape / FMath::Max(Wide / FMath::Max(High, 1.0f), 0.01f), 0.02f, 1.0f);
	};

	const float SteelV = Strip(PanelSteel, Width + (Border * 2.0f), Height + (Border * 2.0f));
	const float PaintV = Strip(PanelPaint, Width, Height);

	DrawPiece(PanelSteel, X - Border, Y - Border, Width + (Border * 2.0f), Height + (Border * 2.0f),
		FVector2D(0.0f, 0.5f - (SteelV * 0.5f)), FVector2D(1.0f, 0.5f + (SteelV * 0.5f)), FLinearColor(0.32f, 0.32f, 0.32f, 1.0f));

	DrawPiece(PanelPaint, X, Y, Width * Full, Height, FVector2D(0.0f, 0.5f - (PaintV * 0.5f)), FVector2D(Full, 0.5f + (PaintV * 0.5f)), FillColor);
}

void AGutHUD::DrawOverheadText(const AGutFighter* Fighter)
{
	if (!Fighter)
	{
		return;
	}

	// find the point over the fighter's head on screen
	const FVector ScreenPosition = Project(Fighter->GetActorLocation() + FVector(0.0f, 0.0f, 115.0f), false);

	if (ScreenPosition.Z <= 0.0f)
	{
		return;
	}

	// what the fighter is doing, since the placeholder animation cannot be trusted to show it
	static const bool bLabelsAsked = FParse::Param(FCommandLine::Get(), TEXT("GutStateLabels"));

	if ((bShowStateLabels || bLabelsAsked) && Fighter->IsAlive())
	{
		FString Label;
		FLinearColor LabelColor = GutHUDColors::DimText;

		switch (Fighter->GetFighterState())
		{
		case EGutFighterState::Windup:
			Label = Fighter->GetCurrentAttack().Label.ToString().ToUpper();
			LabelColor = Fighter->GetCurrentAttackType() == EGutAttackType::Heavy ? GutHUDColors::Health : GutHUDColors::Warning;
			break;

		case EGutFighterState::Blocking:
			Label = TEXT("GUARD");
			break;

		case EGutFighterState::Stunned:
			Label = TEXT("STUNNED");
			break;

		case EGutFighterState::Staggered:

			// somebody on its knees for good says so itself
			if (!Fighter->HasYielded())
			{
				Label = TEXT("STAGGERED");
				LabelColor = GutHUDColors::Warning;
			}
			break;

		case EGutFighterState::Finishing:
			Label = Fighter->GetCurrentFinisher().Label.ToString().ToUpper();
			LabelColor = GutHUDColors::Health;
			break;

		default:
			break;
		}

		if (!Label.IsEmpty())
		{
			DrawLabelAtScreen(Label, ScreenPosition.X, ScreenPosition.Y, 14, LabelColor, true);
		}
	}

	// damage numbers and the like drift up and fade
	FString Text;
	FLinearColor TextColor;
	float Age = 0.0f;

	if (Fighter->GetCombatText(Text, TextColor, Age) && Age < CombatTextDuration)
	{
		const float Alpha = Age / CombatTextDuration;

		TextColor.A = 1.0f - (Alpha * Alpha);

		DrawLabelAtScreen(Text, ScreenPosition.X, ScreenPosition.Y - ((34.0f + (Alpha * 40.0f)) * UIScale), 20, TextColor, true);
	}
}

void AGutHUD::DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCentered, float MaxWidth)
{
	if (!Canvas)
	{
		return;
	}

	// nothing written runs off what it is written on: the panel under it if there is one, and the screen if not.
	// A line too long for its place is set smaller until it fits.
	const FVector2D At(X * UIScale, Y * UIScale);

	float Left = 24.0f * UIScale;
	float Right = Canvas->SizeX - (24.0f * UIScale);

	if (LastPanel.bIsValid && LastPanel.IsInside(At))
	{
		Left = LastPanel.Min.X + (16.0f * UIScale);
		Right = LastPanel.Max.X - (16.0f * UIScale);
	}

	float Room = (bCentered ? 2.0f * FMath::Min(At.X - Left, Right - At.X) : Right - At.X) / UIScale;

	// or the column it is set in
	if (MaxWidth > 0.0f)
	{
		Room = FMath::Min(Room, MaxWidth);
	}

	// a little more than it measures: big type comes out a few parts in a hundred wider than the measure says, which
	// was enough to put the ends of a long headline off the screen
	const float Width = MeasureLabel(Text, Size) * 1.06f;

	float Shrink = 1.0f;

	if (Room > 0.0f && Width > Room)
	{
		Shrink = Room / Width;

		static const bool bReport = FParse::Param(FCommandLine::Get(), TEXT("GutTextLog"));

		if (bReport && !ReportedTooLong.Contains(Text))
		{
			ReportedTooLong.Add(Text);

			UE_LOG(LogTemp, Display, TEXT("[text] %.2f of its size to fit: %s"), Shrink, *Text);
		}
	}

	DrawLabelAtScreen(Text, At.X, At.Y, Size, Color, bCentered, Shrink);
}

FSlateFontInfo AGutHUD::LabelFont(int32 Size, float Shrink)
{
	LoadLook();

	const int32 FontSize = FMath::Max(6, FMath::FloorToInt32(FMath::RoundToInt32(Size * UIScale) * Shrink));

	// big words in the stencil, everything else in the typewriter's face
	const TSharedPtr<const FCompositeFont>& Face = (Size >= 28 && HeadFont.IsValid()) ? HeadFont : BodyFont;

	return Face.IsValid() ? FSlateFontInfo(Face, FontSize) : FCoreStyle::GetDefaultFontStyle("Bold", FontSize);
}

float AGutHUD::MeasureLabel(const FString& Text, int32 Size)
{
	const FSlateFontInfo Font = LabelFont(Size, 1.0f);

	// measured by whatever draws it. With no screen at all there is nothing to measure with, and a guess does.
	if (FSlateApplicationBase::IsInitialized())
	{
		if (FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer())
		{
			return Renderer->GetFontMeasureService()->Measure(Text, Font).X / FMath::Max(UIScale, 0.01f);
		}
	}

	return Text.Len() * Size * 0.6f;
}

TArray<FString> AGutHUD::WrapLabel(const FString& Text, int32 Size, float MaxWidth)
{
	TArray<FString> Lines;
	TArray<FString> Words;

	Text.ParseIntoArray(Words, TEXT(" "), true);

	FString Line;

	for (const FString& Word : Words)
	{
		const FString Longer = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;

		if (!Line.IsEmpty() && MeasureLabel(Longer, Size) > MaxWidth)
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Longer;
		}
	}

	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}

	return Lines;
}

void AGutHUD::DrawLabelAtScreen(const FString& Text, float ScreenX, float ScreenY, int32 Size, const FLinearColor& Color, bool bCentered, float Shrink)
{
	// the canvas only draws text that has an engine font, so give it one and let the Slate font set the size and weight
	FCanvasTextItem TextItem(FVector2D(ScreenX, ScreenY), FText::FromString(Text), GEngine->GetMediumFont(), Color);

	TextItem.SlateFontInfo = LabelFont(Size, Shrink);

	TextItem.bCentreX = bCentered;
	TextItem.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.85f));

	Canvas->DrawItem(TextItem);
}
