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

	if (Town->IsMapUp())
	{
		DrawMap(Town, CenterX);
		return;
	}

	if (Town->IsBagOpen())
	{
		DrawBag(Town, CenterX);
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

	if (bShowControls && !Town->IsTalking() && !Town->IsShopping())
	{
		const TCHAR* Controls[][2] =
		{
			{ TEXT("Walk"), TEXT("WASD / left stick") },
			{ TEXT("Look"), TEXT("Mouse / right stick") },
			{ TEXT("Jog (hold)"), TEXT("Shift / LB") },
			{ TEXT("Talk, use"), TEXT("E / X") },
			{ TEXT("Back out"), TEXT("Backspace / B") },
			{ TEXT("Stop"), TEXT("Esc / Back") },
		};

		// on something dark, so that it can be read against a lit street
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 38.0f * UIScale, 86.0f * UIScale, 372.0f * UIScale, ((UE_ARRAY_COUNT(Controls) * 24.0f) + 14.0f) * UIScale);

		float Y = 92.0f;

		for (const auto& Line : Controls)
		{
			DrawLabel(Line[0], 50.0f, Y, 17, GutHUDColors::DimText);
			DrawLabel(Line[1], 190.0f, Y, 17, GutHUDColors::Text);

			Y += 24.0f;
		}
	}

	// the bag and the map, each with its key, in the corner where nothing else is: they are what a player looks for and cannot guess
	if (!Town->IsTalking() && !Town->IsShopping())
	{
		const float Right = LayoutWidth - 50.0f;
		const float Top = 938.0f;

		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), (Right - 452.0f) * UIScale, (Top - 10.0f) * UIScale, 488.0f * UIScale, 112.0f * UIScale);

		if (UTexture2D* Satchel = GetBagPicture(FName("bag_satchel")); Satchel && Satchel->GetResource())
		{
			DrawPiece(Satchel, Right - 446.0f, Top - 4.0f, 100.0f, 100.0f, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), FLinearColor::White, true);
		}

		DrawLabel(TEXT("YOUR BAG"), Right - 336.0f, Top + 6.0f, 22, GutHUDColors::Warning);
		DrawLabel(TEXT("I  /  D-pad right"), Right - 180.0f, Top + 9.0f, 19, GutHUDColors::Text);

		DrawLabel(TEXT("THE MAP"), Right - 336.0f, Top + 52.0f, 22, GutHUDColors::Warning);
		DrawLabel(TEXT("M  /  Y button"), Right - 180.0f, Top + 55.0f, 19, GutHUDColors::Text);
	}

	// what there is to do here, down the right-hand side, out of the way of the street
	if (const UTownPlaceData* Place = Town->GetPlace(); Place && !Place->Errand.IsEmpty() && !Town->IsTalking() && !Town->IsShopping())
	{
		// no wider than leaves the slate over the middle of the street alone, when something is being called out on it
		const float Column = 500.0f;

		float Y = 44.0f;

		for (int32 i = 0; i < Place->Errand.Num(); ++i)
		{
			// the first line is the heading
			const int32 Size = i == 0 ? 20 : 17;

			for (const FString& Line : WrapLabel(Place->Errand[i], Size, Column))
			{
				DrawLabel(Line, LayoutWidth - 50.0f - MeasureLabel(Line, Size), Y, Size, i == 0 ? GutHUDColors::Warning : GutHUDColors::Text);

				Y += i == 0 ? 32.0f : 25.0f;
			}
		}

		// and, wherever the player is, where what it owns has got to
		if (const FString Owned = Town->GetOwnedLine(); !Owned.IsEmpty())
		{
			Y += 4.0f;

			for (const FString& Line : WrapLabel(Owned, 17, Column))
			{
				DrawLabel(Line, LayoutWidth - 50.0f - MeasureLabel(Line, 17), Y, 17, GutHUDColors::Stamina);

				Y += 25.0f;
			}
		}
	}

	// what is being called out over the place: there is nobody to speak it, so it is written up, as what people say is
	if (const FTownCall* Call = Town->GetCall(); Call && !Town->IsTalking() && !Town->IsShopping())
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
	// heard and not read, if the player has chosen that and the line is spoken: then only what can be said back is written
	const TArray<FString> Said = Town->IsWordsUnwritten() ? TArray<FString>() : Wrap(Town->GetWords(), TalkLineLength);
	const TArray<FString> Answers = Town->GetAnswers();
	const TArray<bool> Asked = Town->GetAnswersAsked();

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

		// what has been asked already stands back, so that what has not is seen
		const bool bAsked = Asked.IsValidIndex(i) && Asked[i];

		DrawLabel((bChosen ? TEXT(">  ") : TEXT("    ")) + Answers[i], CenterX - 500.0f, Y, 22, bChosen ? GutHUDColors::Warning : (bAsked ? GutHUDColors::DimText : GutHUDColors::Text));

		Y += LineHeight;
	}

	DrawLabel(Answers.Num() > 1
		? TEXT("W / S or D-pad: choose          E, Enter or A button: say it          Backspace or B button: leave")
		: TEXT("E, Enter or A button: go on"),
		CenterX, Top + Height - 34.0f, 17, GutHUDColors::Text, true);
}

void ATownHUD::DrawShop(const ATownGameMode* Town, float CenterX)
{
	const TArray<FTownShopLine> Lines = Town->GetShopLines();
	const TArray<FString> Said = Wrap(Town->GetShopSaid(), TalkLineLength);

	// what there is to know about whatever the player is on, under the list: room is kept for the longest of them, so the panel does not jump
	int32 AboutLines = 0;

	for (const FTownShopLine& Line : Lines)
	{
		AboutLines = FMath::Max(AboutLines, Wrap(Line.About, TalkLineLength).Num());
	}

	const float LineHeight = 40.0f;
	const float AboutHeight = AboutLines > 0 ? 16.0f + (AboutLines * 28.0f) : 0.0f;
	const float Height = 150.0f + (Said.Num() * 30.0f) + ((Lines.Num() + 1) * LineHeight) + AboutHeight + 58.0f;
	const float Top = 1040.0f - Height;
	const float Left = CenterX - 540.0f;

	// a counter is a slate with prices on it, like the window's
	DrawPanel(CenterX, Top, 1180.0f, Height, EGutPanel::Slate);

	DrawLabel(Town->GetShopTitle(), Left, Top + 22.0f, 30, GutHUDColors::Warning);

	// whose water it is: a fighter's own or an owner's, and nobody else's page
	if (const FString Held = Town->GetShopPurse(); !Held.IsEmpty())
	{
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
		const FString Price = Line.PriceText.IsEmpty() ? FString::Printf(TEXT("%.0f L"), Line.Price) : Line.PriceText;

		DrawLabel(Price, Left + 930.0f - MeasureLabel(Price, 22), Y, 22, Ink);

		if (!Line.Why.IsEmpty())
		{
			DrawLabel(Line.Why.ToUpper(), Left + 950.0f, Y + 5.0f, 15, Line.bWorn ? GutHUDColors::Stamina : GutHUDColors::DimText);
		}

		Y += LineHeight;
	}

	if (Lines.IsValidIndex(Town->GetShopChoice()))
	{
		float AboutY = Top + Height - 58.0f - AboutHeight + 16.0f;

		for (const FString& About : Wrap(Lines[Town->GetShopChoice()].About, TalkLineLength))
		{
			DrawLabel(About, Left + 20.0f, AboutY, 19, GutHUDColors::DimText);

			AboutY += 28.0f;
		}
	}

	DrawLabel(FString::Printf(TEXT("W / S or D-pad: choose          E, Enter or A button: %s          Backspace or B button: leave"), *Town->GetShopVerb()), CenterX, Top + Height - 34.0f, 17, GutHUDColors::Text, true);
}

UTexture2D* ATownHUD::GetBagPicture(const FName& Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<UTexture2D>* Fetched = BagPictures.Find(Name))
	{
		return *Fetched;
	}

	// looked for once, found or not
	UTexture2D* Picture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Gut/Art/UI/%s.%s"), *Name.ToString(), *Name.ToString()), nullptr, LOAD_NoWarn | LOAD_Quiet);

	BagPictures.Add(Name, Picture);

	return Picture;
}

void ATownHUD::DrawBag(const ATownGameMode* Town, float CenterX)
{
	const TArray<FTownBagLine> Things = Town->GetBagLines();
	const int32 Choice = Town->GetBagChoice();

	// the street stands back behind it
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	const float Wide = 1640.0f;
	const float Top = 50.0f;
	const float High = 980.0f;
	const float Left = CenterX - (Wide * 0.5f);

	DrawPanel(CenterX, Top, Wide, High, EGutPanel::Slate);

	// the bag itself, and what is in the fighter already for the fight ahead
	UTexture2D* Satchel = GetBagPicture(FName("bag_satchel"));

	if (Satchel && Satchel->GetResource())
	{
		DrawPiece(Satchel, Left + 30.0f, Top + 12.0f, 100.0f, 100.0f, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), FLinearColor::White, true);
	}

	DrawLabel(TEXT("YOUR BAG"), Left + 144.0f, Top + 30.0f, 34, GutHUDColors::Warning);

	if (const FString Ahead = Town->GetShopPurse(); !Ahead.IsEmpty())
	{
		DrawLabel(Ahead, Left + 146.0f, Top + 80.0f, 16, GutHUDColors::Stamina);
	}

	// everything in it, down the left, a part at a time
	const float ListLeft = Left + 44.0f;
	const float CountRight = Left + 560.0f;
	const float LineHeight = 33.0f;

	float Y = Top + 132.0f;
	FString Part;

	for (int32 i = 0; i <= Things.Num(); ++i)
	{
		const bool bChosen = i == Choice;
		const TCHAR* Mark = bChosen ? TEXT(">  ") : TEXT("    ");

		// under the things, the line that shuts it
		if (i == Things.Num())
		{
			DrawLabel(FString(Mark) + TEXT("Shut the bag"), ListLeft, Y + 16.0f, 21, bChosen ? GutHUDColors::Warning : GutHUDColors::Text);
			break;
		}

		const FTownBagLine& Thing = Things[i];

		if (Thing.Part != Part)
		{
			Part = Thing.Part;

			Y += i == 0 ? 0.0f : 12.0f;

			DrawLabel(Part, ListLeft, Y, 15, GutHUDColors::DimText);
			DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), (ListLeft + MeasureLabel(Part, 15) + 14.0f) * UIScale, (Y + 10.0f) * UIScale, (Left + 730.0f - ListLeft - MeasureLabel(Part, 15) - 14.0f) * UIScale, FMath::Max(1.0f, 2.0f * UIScale));

			Y += 26.0f;
		}

		const FLinearColor Ink = bChosen ? GutHUDColors::Warning : GutHUDColors::Text;

		DrawLabel(FString(Mark) + Thing.Name, ListLeft, Y, 21, Ink, false, 380.0f);

		if (!Thing.Count.IsEmpty())
		{
			DrawLabel(Thing.Count, CountRight - MeasureLabel(Thing.Count, 19), Y + 2.0f, 19, Ink);
		}

		// how it stands: with the player, worn, or why it cannot be used now
		if (!Thing.Why.IsEmpty())
		{
			DrawLabel(Thing.Why.ToUpper(), CountRight + 16.0f, Y + 6.0f, 14, Thing.Kind == ETownBagKind::Good ? GutHUDColors::DimText : GutHUDColors::Stamina);
		}

		Y += LineHeight;
	}

	// and the thing the player is on, on the right: its picture, and what there is to know about it
	const float PageLeft = Left + 800.0f;
	const float PageWide = 790.0f;

	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), (Left + 765.0f) * UIScale, (Top + 126.0f) * UIScale, FMath::Max(1.0f, 2.0f * UIScale), (High - 216.0f) * UIScale);

	const bool bOnThing = Things.IsValidIndex(Choice);

	UTexture2D* Picture = bOnThing ? GetBagPicture(Things[Choice].Picture) : Satchel;

	const float BoxTop = Top + 116.0f;
	const float BoxHigh = 430.0f;

	if (Picture && Picture->GetResource())
	{
		// whole, in its own shape, as large as its place allows
		const float Fit = FMath::Min(PageWide / FMath::Max(1, Picture->GetSizeX()), BoxHigh / FMath::Max(1, Picture->GetSizeY()));
		const float PictureWide = Picture->GetSizeX() * Fit;
		const float PictureHigh = Picture->GetSizeY() * Fit;

		DrawPiece(Picture, PageLeft + ((PageWide - PictureWide) * 0.5f), BoxTop + ((BoxHigh - PictureHigh) * 0.5f), PictureWide, PictureHigh, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), FLinearColor::White, true);
	}

	float PageY = BoxTop + BoxHigh + 22.0f;

	if (bOnThing)
	{
		const FTownBagLine& Thing = Things[Choice];

		DrawLabel(Thing.Name.ToUpper(), PageLeft, PageY, 28, GutHUDColors::Warning, false, PageWide - 200.0f);

		if (!Thing.Count.IsEmpty())
		{
			DrawLabel(Thing.Count, PageLeft + PageWide - MeasureLabel(Thing.Count, 24), PageY + 4.0f, 24, GutHUDColors::Text);
		}

		PageY += 48.0f;

		DrawLabel(Thing.Detail, PageLeft, PageY, 18, GutHUDColors::DimText, false, PageWide);

		PageY += 36.0f;

		// what there is to know about it: stood back while who it is for is being asked in its place
		for (const FString& About : Town->IsBagAsking() ? TArray<FString>() : Thing.About)
		{
			for (const FString& Line : WrapLabel(About, 17, PageWide))
			{
				DrawLabel(Line, PageLeft, PageY, 17, GutHUDColors::Text);

				PageY += 24.0f;
			}

			PageY += 7.0f;
		}
	}
	else
	{
		DrawLabel(TEXT("Everything you have on you is in here."), PageLeft, PageY, 20, GutHUDColors::DimText);

		PageY += 34.0f;
	}

	// what came of the last thing done: while who it is for is being asked, that is said where it is asked
	const TArray<FString> Said = WrapLabel(Town->GetBagSaid(), 19, Town->IsBagAsking() ? 580.0f : PageWide);

	for (const FString& Line : Town->IsBagAsking() ? TArray<FString>() : Said)
	{
		DrawLabel(Line, PageLeft, PageY + 8.0f, 19, GutHUDColors::Warning);

		PageY += 27.0f;
	}

	const FString Verb = Town->GetShopVerb();

	// who a thing is for, asked over the page
	if (Town->IsBagAsking())
	{
		const TArray<FTownBagTaker> Takers = Town->GetBagTakers(Town->GetBagAsked());

		const float AskWide = 660.0f;
		const float AskHigh = 126.0f + ((Takers.Num() + 1) * 38.0f) + (Said.IsEmpty() ? 0.0f : 12.0f + (Said.Num() * 27.0f));
		const float AskTop = PageY + 6.0f;
		const float AskCenter = PageLeft + (PageWide * 0.5f);
		const float AskLeft = AskCenter - (AskWide * 0.5f) + 36.0f;

		DrawPanel(AskCenter, AskTop, AskWide, AskHigh, EGutPanel::Steel);
		DrawLabel(TEXT("WHO IS IT FOR?"), AskLeft, AskTop + 24.0f, 26, GutHUDColors::Warning);

		float AskY = AskTop + 80.0f;

		for (int32 i = 0; i <= Takers.Num(); ++i)
		{
			const bool bChosen = i == Town->GetBagAskChoice();
			const TCHAR* Mark = bChosen ? TEXT(">  ") : TEXT("    ");

			if (i == Takers.Num())
			{
				DrawLabel(FString(Mark) + TEXT("Nobody: it keeps"), AskLeft, AskY, 21, bChosen ? GutHUDColors::Warning : GutHUDColors::Text);

				AskY += 50.0f;

				for (const FString& Line : Said)
				{
					DrawLabel(Line, AskLeft, AskY, 19, GutHUDColors::Warning);

					AskY += 27.0f;
				}

				break;
			}

			DrawLabel(FString(Mark) + Takers[i].Name, AskLeft, AskY, 21, bChosen ? GutHUDColors::Warning : (Takers[i].bCan ? GutHUDColors::Text : GutHUDColors::DimText), false, 330.0f);

			if (!Takers[i].Why.IsEmpty())
			{
				DrawLabel(Takers[i].Why.ToUpper(), AskLeft + 380.0f, AskY + 6.0f, 14, GutHUDColors::DimText);
			}

			AskY += 38.0f;
		}

		// what is drawn after this is on the bag's own slate again
		LastPanel = FBox2D(ForceInit);

		DrawLabel(TEXT("W / S or D-pad: choose          E, Enter or A button: give it          Backspace or B button: back to the bag"), CenterX, Top + High - 38.0f, 17, GutHUDColors::Text, true);
		return;
	}

	DrawLabel(Verb.IsEmpty()
		? FString(TEXT("W / S or D-pad: choose          I, Backspace or B button: shut the bag"))
		: FString::Printf(TEXT("W / S or D-pad: choose          E, Enter or A button: %s          I, Backspace or B button: shut the bag"), *Verb),
		CenterX, Top + High - 38.0f, 17, GutHUDColors::Text, true);
}

void ATownHUD::DrawMap(const ATownGameMode* Town, float CenterX)
{
	if (!MapPicture)
	{
		MapPicture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Gut/Art/UI/ui_map.ui_map"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// the street stands back behind it
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	if (!MapPicture || !MapPicture->GetResource())
	{
		DrawLabel(TEXT("There is no map of the town here."), CenterX, 500.0f, 26, GutHUDColors::Text, true);
		return;
	}

	// the whole picture, as tall as the screen allows, in its own shape
	const float High = 900.0f;
	const float Wide = High * MapPicture->GetSizeX() / FMath::Max(1, MapPicture->GetSizeY());
	const float Left = CenterX - (Wide * 0.5f);
	const float Top = 70.0f;

	DrawPiece(MapPicture, Left, Top, Wide, High, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), FLinearColor::White);

	LoadLook();

	// where the player is: the figure the cage's screens are drawn with, breathing a little so that the eye finds it
	FVector2D Spot;

	if (Town->GetMapSpot(Spot))
	{
		const float X = Left + (Spot.X * Wide);
		const float Y = Top + (Spot.Y * High);
		const float Beat = 1.0f + (0.12f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.0f));

		if (FigureStencil && FigureStencil->GetResource())
		{
			const FVector2D From(0.14f, 0.03f);
			const FVector2D To(0.86f, 0.97f);

			const float FigureHigh = 46.0f * Beat;
			const float FigureWide = FigureHigh * ((To.X - From.X) * FigureStencil->GetSizeX()) / FMath::Max((To.Y - From.Y) * FigureStencil->GetSizeY(), 1.0f);

			// its feet on the spot
			DrawPiece(FigureStencil, X - (FigureWide * 0.5f), Y - FigureHigh, FigureWide, FigureHigh, From, To, GutHUDColors::Warning);
		}

		DrawLabel(TEXT("YOU"), X, Y + 4.0f, 18, GutHUDColors::Warning, true);
	}

	if (const UTownPlaceData* Place = Town->GetPlace())
	{
		DrawLabel(FString::Printf(TEXT("YOU ARE IN %s"), *Place->Name.ToUpper()), CenterX, 22.0f, 26, GutHUDColors::Text, true);
	}

	if (const FString Owned = Town->GetOwnedLine(); !Owned.IsEmpty())
	{
		DrawLabel(Owned, CenterX, 984.0f, 19, GutHUDColors::Stamina, true);
	}

	DrawLabel(TEXT("M, Backspace, or the Y or B button: put the map away"), CenterX, 1026.0f, 17, GutHUDColors::Text, true);
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
