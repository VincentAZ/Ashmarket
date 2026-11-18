// Ashmarket: the town outside the cage.

#include "TownHUD.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "TownGameMode.h"

void ATownHUD::DrawHUD()
{
	// none of the cage's screens: only what every HUD does, and then the street's own
	AHUD::DrawHUD();

	const ATownGameMode* Town = GetWorld()->GetAuthGameMode<ATownGameMode>();

	if (!Canvas || !Town || !Town->IsSettled())
	{
		return;
	}

	// everything is laid out for 1080p and scaled to fit, as in the cage
	UIScale = Canvas->SizeY / 1080.0f;
	LastPanel = FBox2D(ForceInit);

	const float LayoutWidth = Canvas->SizeX / UIScale;
	const float CenterX = LayoutWidth * 0.5f;

	if (Town->IsStopped())
	{
		DrawTownStopped(Town, CenterX);
		return;
	}

	// where this is, and what the player has on them
	if (const UTownPlaceData* Place = Town->GetPlace())
	{
		DrawLabel(Place->Name.ToUpper(), 50.0f, 44.0f, 28, GutHUDColors::Text);
	}

	// what the player holds, a line for each of the ways it has come to the cage, the last on the bottom
	if (const TArray<FString> Purses = Town->GetPurses(); !Purses.IsEmpty() && !Town->IsTalking() && !Town->IsShopping())
	{
		float Y = 1010.0f - ((Purses.Num() - 1) * 28.0f);

		for (const FString& Purse : Purses)
		{
			DrawLabel(Purse, 50.0f, Y, 20, GutHUDColors::Text);

			Y += 28.0f;
		}
	}

	if (bShowControls && !Town->IsTalking())
	{
		const TCHAR* Controls[][2] =
		{
			{ TEXT("Walk"), TEXT("WASD / left stick") },
			{ TEXT("Look"), TEXT("Mouse / right stick") },
			{ TEXT("Jog (hold)"), TEXT("Shift / LB") },
			{ TEXT("Talk, use"), TEXT("E / X") },
			{ TEXT("Stop"), TEXT("Esc / Back") },
		};

		float Y = 92.0f;

		for (const auto& Line : Controls)
		{
			DrawLabel(Line[0], 50.0f, Y, 16, GutHUDColors::DimText);
			DrawLabel(Line[1], 190.0f, Y, 16, GutHUDColors::DimText);

			Y += 20.0f;
		}
	}

	// what there is to do here, down the right-hand side, out of the way of the street
	if (const UTownPlaceData* Place = Town->GetPlace(); Place && !Place->Errand.IsEmpty() && !Town->IsTalking())
	{
		float Y = 44.0f;

		for (int32 i = 0; i < Place->Errand.Num(); ++i)
		{
			// the first line is the heading
			DrawLabel(Place->Errand[i], LayoutWidth - 50.0f - MeasureLabel(Place->Errand[i], i == 0 ? 20 : 17), Y, i == 0 ? 20 : 17, i == 0 ? GutHUDColors::Warning : GutHUDColors::Text);

			Y += i == 0 ? 32.0f : 25.0f;
		}
	}

	// what is being called out over the place: there is nobody to speak it, so it is written up, as what people say is
	if (const FTownCall* Call = Town->GetCall(); Call && !Town->IsTalking())
	{
		const TArray<FString> Said = Wrap(Call->Words, 52);
		const float Height = 62.0f + (Said.Num() * 30.0f);

		DrawPanel(CenterX, 36.0f, 780.0f, Height, EGutPanel::Slate);
		DrawLabel(Call->Speaker.ToUpper(), CenterX, 46.0f, 16, GutHUDColors::Warning, true);

		float Y = 74.0f;

		for (const FString& Line : Said)
		{
			DrawLabel(Line, CenterX, Y, 21, GutHUDColors::Text, true);

			Y += 30.0f;
		}

		// what is drawn after this is not on that slate, and is not to be fitted to it
		LastPanel = FBox2D(ForceInit);
	}

	if (Town->IsTalking())
	{
		DrawTalk(Town, CenterX);
		return;
	}

	if (Town->IsShopping())
	{
		DrawShop(Town, CenterX);
		return;
	}

	if (const FString Notice = Town->GetNotice(); !Notice.IsEmpty())
	{
		DrawPanel(CenterX, 880.0f, 900.0f, 84.0f, EGutPanel::Steel);
		DrawLabel(Notice, CenterX, 908.0f, 22, GutHUDColors::Text, true);
	}
	else if (const FString Prompt = Town->GetPrompt(); !Prompt.IsEmpty())
	{
		DrawPanel(CenterX, 900.0f, 620.0f, 64.0f, EGutPanel::Steel);
		DrawLabel(FString::Printf(TEXT("E  /  X      %s"), *Prompt), CenterX, 920.0f, 22, GutHUDColors::Warning, true);
	}
}

TArray<FString> ATownHUD::Wrap(const FString& Words, int32 Length)
{
	TArray<FString> Lines;
	TArray<FString> Each;

	Words.ParseIntoArray(Each, TEXT(" "), true);

	FString Line;

	for (const FString& Word : Each)
	{
		if (!Line.IsEmpty() && Line.Len() + 1 + Word.Len() > Length)
		{
			Lines.Add(Line);
			Line.Reset();
		}

		Line += (Line.IsEmpty() ? TEXT("") : TEXT(" ")) + Word;
	}

	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}

	return Lines;
}

void ATownHUD::DrawTalk(const ATownGameMode* Town, float CenterX)
{
	const TArray<FString> Said = Wrap(Town->GetWords(), TalkLineLength);
	const TArray<FString> Answers = Town->GetAnswers();

	const float LineHeight = 34.0f;
	const float Height = 96.0f + (Said.Num() * LineHeight) + (Answers.IsEmpty() ? 40.0f : 26.0f + (Answers.Num() * LineHeight)) + 44.0f;
	const float Top = 1040.0f - Height;

	// what is said is written on the slate, as the odds are
	DrawPanel(CenterX, Top, 1180.0f, Height, EGutPanel::Slate);

	DrawLabel(Town->GetSpeaker().ToUpper(), CenterX - 540.0f, Top + 22.0f, 30, GutHUDColors::Warning);

	float Y = Top + 78.0f;

	for (const FString& Line : Said)
	{
		DrawLabel(Line, CenterX - 540.0f, Y, 22, GutHUDColors::Text);

		Y += LineHeight;
	}

	Y += 26.0f;

	for (int32 i = 0; i < Answers.Num(); ++i)
	{
		const bool bChosen = i == Town->GetTalkChoice();

		DrawLabel((bChosen ? TEXT(">  ") : TEXT("    ")) + Answers[i], CenterX - 500.0f, Y, 22, bChosen ? GutHUDColors::Warning : GutHUDColors::Text);

		Y += LineHeight;
	}

	DrawLabel(Answers.Num() > 1
		? TEXT("W / S or D-pad: choose          E, Enter or A button: say it")
		: TEXT("E, Enter or A button: go on"),
		CenterX, Top + Height - 34.0f, 17, GutHUDColors::Text, true);
}

void ATownHUD::DrawShop(const ATownGameMode* Town, float CenterX)
{
	const TArray<FTownShopLine> Lines = Town->GetShopLines();
	const TArray<FString> Said = Wrap(Town->GetShopSaid(), TalkLineLength);

	const float LineHeight = 40.0f;
	const float Height = 150.0f + (Said.Num() * 30.0f) + ((Lines.Num() + 1) * LineHeight) + 58.0f;
	const float Top = 1040.0f - Height;
	const float Left = CenterX - 540.0f;

	// a counter is a slate with prices on it, like the window's
	DrawPanel(CenterX, Top, 1180.0f, Height, EGutPanel::Slate);

	const FString Keeper = Town->GetShopKeeper();

	DrawLabel(Keeper.IsEmpty() ? FString(TEXT("THE COUNTER")) : FString::Printf(TEXT("%s'S COUNTER"), *Keeper.ToUpper()), Left, Top + 22.0f, 30, GutHUDColors::Warning);

	// whose water it is: a fighter's own, and nobody else's page
	if (Town->HasFighter())
	{
		const FString Held = FString::Printf(TEXT("Your fighter holds %.0f L"), Town->GetShopWater());

		DrawLabel(Held, CenterX + 540.0f - MeasureLabel(Held, 22), Top + 28.0f, 22, GutHUDColors::Text);
	}

	float Y = Top + 78.0f;

	for (const FString& Line : Said)
	{
		DrawLabel(Line, Left, Y, 21, GutHUDColors::Text);

		Y += 30.0f;
	}

	Y += 16.0f;

	for (int32 i = 0; i <= Lines.Num(); ++i)
	{
		const bool bChosen = i == Town->GetShopChoice();
		const TCHAR* Mark = bChosen ? TEXT(">  ") : TEXT("    ");

		// under the pieces, the line that leaves
		if (i == Lines.Num())
		{
			DrawLabel(FString(Mark) + TEXT("Nothing more"), Left + 20.0f, Y + 8.0f, 22, bChosen ? GutHUDColors::Warning : GutHUDColors::Text);
			break;
		}

		const FTownShopLine& Line = Lines[i];
		const FLinearColor Ink = bChosen ? GutHUDColors::Warning : (Line.bCanBuy ? GutHUDColors::Text : GutHUDColors::DimText);

		DrawLabel(FString(Mark) + Line.Name, Left + 20.0f, Y, 22, Ink);
		DrawLabel(Line.Detail, Left + 408.0f, Y + 5.0f, 16, bChosen ? GutHUDColors::Text : GutHUDColors::DimText);

		// the price, and beside it why it cannot be had, if it cannot
		const FString Price = FString::Printf(TEXT("%.0f L"), Line.Price);

		DrawLabel(Price, Left + 930.0f - MeasureLabel(Price, 22), Y, 22, Ink);

		if (!Line.Why.IsEmpty())
		{
			DrawLabel(Line.Why.ToUpper(), Left + 950.0f, Y + 5.0f, 15, Line.bWorn ? GutHUDColors::Stamina : GutHUDColors::DimText);
		}

		Y += LineHeight;
	}

	DrawLabel(TEXT("W / S or D-pad: choose          E, Enter or A button: buy it, or leave"), CenterX, Top + Height - 34.0f, 17, GutHUDColors::Text, true);
}

void ATownHUD::DrawTownStopped(const ATownGameMode* Town, float CenterX)
{
	const TArray<FString> Lines = Town->GetStoppedLines();

	const float LineHeight = 44.0f;
	const float Top = 330.0f;

	// the rest of the screen stands back
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	DrawPanel(CenterX, Top, 620.0f, 150.0f + (Lines.Num() * LineHeight));

	DrawLabel(TEXT("STOPPED"), CenterX, Top + 22.0f, 44, GutHUDColors::Text, true);

	float Y = Top + 104.0f;

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const bool bChosen = i == Town->GetStoppedChoice();

		DrawLabel((bChosen ? TEXT(">  ") : TEXT("")) + Lines[i], CenterX, Y, 24, bChosen ? GutHUDColors::Warning : GutHUDColors::Text, true);

		Y += LineHeight;
	}

	DrawLabel(TEXT("W / S: choose        Enter: do it        Esc: go on"), CenterX, Y + 8.0f, 17, GutHUDColors::Text, true);
}
