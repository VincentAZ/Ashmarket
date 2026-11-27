// Ashmarket: the town outside the cage.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Tests/TownTestHelpers.h"

/**
 *  Ashmarket.Town.Bidding   a lot at the Block is bid for (25 November: "the auction is a real auction, but I pay a fixed
 *                           price - let the protagonist bid, up to what he can pay"). Somebody with 70 L has a lot put up
 *                           and bids until it is theirs: they pay what was bid, no more than a step over what the
 *                           keenest of the others would go to, and the lot comes away with them. A bid that is more
 *                           than is held is refused; a bid that stands cannot be walked away from; and a lot let go is
 *                           somebody else's, and is not on the book or the Block again that day.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownBiddingTest, "Ashmarket.Town.Bidding", GutTests::Flags)

bool FTownBiddingTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	struct FNotes
	{
		FGutStableState StableWas;
		TArray<int32> SoldWas;
		TArray<FString> SoldToWas;
		int32 SoldDayWas = -1;

		float Open = 0.0f;
		float BookPrice = 0.0f;
		float Keenest = 0.0f;

		int32 Won = INDEX_NONE;
		int32 Other = INDEX_NONE;
		bool bAnybodyWanted = false;
		int32 OtherLot = 0;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	TArray<FScript::FStep> Steps;

	Steps.Add({ 0.5f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		Notes->StableWas = Run->Stable;
		Notes->SoldWas = Run->BlockSold;
		Notes->SoldToWas = Run->BlockSoldTo;
		Notes->SoldDayWas = Run->BlockSoldDay;

		// somebody who owns nobody, at the clerk's table, with lots bid for
		Run->bStreetForced = true;
		Run->Stable = FGutStableState();
		Run->BlockSold.Reset();
		Run->BlockSoldTo.Reset();
		Run->BlockSoldDay = -1;

		Town->bBids = true;

		// (the way it was before the sale went on by itself: a lot put up for the asking. The same bidding, hammer and
		// taking are under both, and Ashmarket.Town.LiveSale is the sale as it is played.)
		Town->bLiveSale = false;

		const int32 Clerk = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Name == TEXT("Margin"); });

		Town->OpenBook(Clerk);

		const TArray<FTownShopLine> Lines = Town->GetShopLines();

		TestTrue(TEXT("The book is open with lots on it"), Town->IsAtBook() && Lines.Num() >= 2);

		if (Lines.Num() < 2)
		{
			return;
		}

		TestTrue(*FString::Printf(TEXT("A lot is not priced: it says where the caller opens (%s)"), *Lines[0].PriceText), Lines[0].Name.StartsWith(TEXT("Lot ")) && Lines[0].PriceText.StartsWith(TEXT("opens at ")) && Lines[0].bCanBuy);
		TestTrue(TEXT("And what the book has them at is said, with it being up to the room"), Lines[0].About.Contains(TEXT("up to the room")));
		TestTrue(TEXT("Somebody with a tag still signs for a wage"), Lines.Last().Name.StartsWith(TEXT("Signs")) && Lines.Last().PriceText.Contains(TEXT("a day")));

		// the first lot is put up
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("Asking for a lot puts it up"), Town->IsBidding());
		TestFalse(TEXT("Nobody is made an owner by having a lot put up"), Run->Stable.bStarted);

		const FTownAuction Up = Town->GetAuction();

		TestTrue(*FString::Printf(TEXT("Two or three others want it (%d)"), Up.Rivals.Num()), Up.Rivals.Num() >= 2 && Up.Rivals.Num() <= 3);
		TestTrue(*FString::Printf(TEXT("The caller opens under what the book has them at (%.0f of %.0f)"), Up.Open, Up.Offer.Price), Up.Open < Up.Offer.Price && Up.Open >= 6.0f);
		TestTrue(*FString::Printf(TEXT("And is heard calling it over the square (%s)"), Town->GetCall() ? *Town->GetCall()->Words : TEXT("nothing")), Town->GetCall() && Town->GetCall()->Words.Contains(TEXT("Who opens")));

		float Keenest = 0.0f;

		for (const FTownBidder& Rival : Up.Rivals)
		{
			TestTrue(*FString::Printf(TEXT("%s will go to between a little over half and a little over all of the book's price (%.0f of %.0f)"), *Rival.Name, Rival.Limit, Up.Offer.Price),
				Rival.Limit >= FMath::FloorToFloat(Up.Offer.Price * 0.55f) && Rival.Limit <= Up.Offer.Price * 1.1f);

			Keenest = FMath::Max(Keenest, Rival.Limit);
		}

		const TArray<FTownShopLine> Bids = Town->GetShopLines();

		TestTrue(TEXT("What can be bid is the opening, a jump over it, and all that is held"), Bids.Num() == 3 && Bids[0].Price == Up.Open && Bids[2].Price == 70.0f && Bids[0].bCanBuy);

		Notes->Open = Up.Open;
		Notes->BookPrice = Up.Offer.Price;
		Notes->Keenest = Keenest;

		GutTests::Screenshot(TEXT("56_a_lot_put_up"));
	}, []() { return IsStreetReady(); } });

	Steps.Add({ 0.4f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (!Town->IsBidding())
		{
			AddError(TEXT("The lot is not up any more"));
			return;
		}

		const float Keenest = Notes->Keenest;

		struct { float Open; } Up = { Notes->Open };

		// bid every time the bid is somebody else's, until the hammer
		int32 Mine = 0;

		for (int32 i = 0; i < 400 && Town->IsBidding(); ++i)
		{
			if (Town->GetAuction().Leader != 0)
			{
				while (Town->GetShopChoice() != 0)
				{
					Town->HandleMenuInput(EGutMenuInput::Up);
				}

				Town->HandleMenuInput(EGutMenuInput::Confirm);

				TestEqual(TEXT("A bid makes the bid the player's"), Town->GetAuction().Leader, 0);

				++Mine;

				// and it cannot be walked away from
				Town->HandleBack();

				TestTrue(TEXT("A bid that stands cannot be backed out of"), Town->IsBidding() && Town->GetShopSaid().Contains(TEXT("Your bid stands")));
			}

			Town->UpdateAuction(10.0f);
		}

		TestFalse(TEXT("The hammer comes down"), Town->IsBidding());
		TestTrue(TEXT("The lot is the player's, who is an owner now"), Run->Stable.bStarted && Run->Stable.Fighters.Num() == 1);

		if (Run->Stable.Fighters.Num() != 1)
		{
			return;
		}

		const float Paid = Run->Stable.Fighters[0].Price;

		TestEqual(TEXT("What was bid is what was paid"), Run->Stable.Water, 70.0f - Paid);
		TestTrue(*FString::Printf(TEXT("And it is no more than a step over what the keenest of the others would go to (%.0f paid, %.0f their most, %d bids)"), Paid, Keenest, Mine),
			Paid >= Up.Open && Paid <= FMath::Max(Up.Open, Keenest + ATownGameMode::GetBidStep(Keenest)) && Mine >= 1);
		TestTrue(*FString::Printf(TEXT("The clerk says what it went for and what the book had (%s)"), *Town->GetShopSaid()), Town->GetShopSaid().Contains(TEXT("Sold")) && Town->GetShopSaid().Contains(TEXT("The book had")));
		TestEqual(TEXT("Whoever was bought comes away with the buyer"), Town->GetTowCount(), 1);
		TestTrue(TEXT("And is the player's on the book"), Town->GetShopLines().IsValidIndex(0) && Town->GetShopLines()[0].Why == TEXT("yours"));

		GutTests::Screenshot(TEXT("56_knocked_down_to_the_player"));

		// another lot, with only what opens it in hand
		const TArray<FTownShopLine> After = Town->GetShopLines();

		for (int32 i = 1; i < After.Num(); ++i)
		{
			if (After[i].Name.StartsWith(TEXT("Lot ")))
			{
				Notes->Other = i;
				break;
			}
		}

		if (Notes->Other == INDEX_NONE)
		{
			AddError(TEXT("There is no second lot on the book to let go"));
			return;
		}

		while (Town->GetShopChoice() != Notes->Other)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		FGutStableFighter Person;

		Town->GetBookPerson(Notes->Other, Person, Notes->OtherLot);

		Run->Stable.Water = ATownGameMode::GetOpeningBid(Person.Price) + 1.0f;

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(TEXT("The second lot is put up"), Town->IsBidding());

		// more than is held is there to be read, and is refused
		Town->HandleMenuInput(EGutMenuInput::Down);
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(*FString::Printf(TEXT("A bid of more than is held is refused, and says so (%s)"), *Town->GetShopSaid()), Town->IsBidding() && Town->GetAuction().Leader != 0 && Town->GetShopSaid().Contains(TEXT("more than you hold")));

		for (const FTownBidder& Rival : Town->GetAuction().Rivals)
		{
			Notes->bAnybodyWanted |= Rival.Limit >= Town->GetAuction().Open;
		}

		// let it go: the room finishes it
		Town->HandleBack();

		TestFalse(TEXT("Letting it go ends it"), Town->IsBidding());
		TestTrue(TEXT("And the book is still open"), Town->IsAtBook());
		TestEqual(TEXT("Nothing was paid for it"), Run->Stable.Fighters.Num(), 1);

		const TArray<FTownShopLine> Last = Town->GetShopLines();

		if (Notes->bAnybodyWanted)
		{
			TestTrue(*FString::Printf(TEXT("It went to somebody else, and the clerk says who (%s)"), *Town->GetShopSaid()), Town->GetShopSaid().StartsWith(TEXT("Sold:")) && Town->IsSoldAway(Notes->Other));
			TestTrue(TEXT("It is on the book as sold, and cannot be put up again"), Last.IsValidIndex(Notes->Other) && Last[Notes->Other].PriceText == TEXT("sold") && !Last[Notes->Other].bCanBuy && Last[Notes->Other].Why.StartsWith(TEXT("went to")));

			Town->HandleMenuInput(EGutMenuInput::Confirm);

			TestFalse(TEXT("Asking for it again does not put it up"), Town->IsBidding());
		}
		else
		{
			TestTrue(TEXT("Nobody wanted it: it stays on the book"), Town->GetShopSaid().Contains(TEXT("No bid")) && !Town->IsSoldAway(Notes->Other));
		}

		Town->CloseShop();
	} });

	// a lot that went to somebody else is walked off the Block and is not one of the sale's again
	Steps.Add({ 0.5f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->bAnybodyWanted && Notes->OtherLot > 0)
		{
			const int32 Stood = Town->GetPlace()->People.IndexOfByPredicate([Notes](const FTownPerson& Person) { return Person.Lot == Notes->OtherLot; });

			TestTrue(TEXT("The lot that went to somebody else is gone from the Block"), Town->GetPeople().IsValidIndex(Stood) && !Town->GetPeople()[Stood]);
		}

		Run->Stable = Notes->StableWas;
		Run->BlockSold = Notes->SoldWas;
		Run->BlockSoldTo = Notes->SoldToWas;
		Run->BlockSoldDay = Notes->SoldDayWas;
	}, [Notes]()
	{
		const ATownGameMode* Town = GetTown();

		if (!IsStreetReady() || !Notes->bAnybodyWanted || Notes->OtherLot <= 0)
		{
			return IsStreetReady();
		}

		const int32 Stood = Town->GetPlace()->People.IndexOfByPredicate([Notes](const FTownPerson& Person) { return Person.Lot == Notes->OtherLot; });

		return Town->GetPeople().IsValidIndex(Stood) && !Town->GetPeople()[Stood];
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

/**
 *  Ashmarket.Town.LiveSale  the Block's sale goes on by itself (26 November: "the card shows you only who is currently
 *                           being bidded on - you can join the bidding - so, the auction goes like a regular auction and
 *                           you can bid anytime. if you bid more than you have, Mother Ledger pipes in 'He ain't got the
 *                           water' - bidding of new lots starts 15 seconds after you enter the block - and starts and
 *                           finishes with or without you"). The first lot goes up with nobody at the table and is sold
 *                           to somebody else; the second is bid for and won; a bid of more than is held on the third
 *                           brings Mother Ledger in, and the table is left while the lot is up; and the sale runs out,
 *                           with nobody on the page who is not on the Block.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTownLiveSaleTest, "Ashmarket.Town.LiveSale", GutTests::Flags)

bool FTownLiveSaleTest::RunTest(const FString& Parameters)
{
	using namespace TownTests;

	if (!AutomationOpenMap(TEXT("/Game/Town/Lvl_BlockSquare"), true))
	{
		AddError(TEXT("Could not open Block Square's level"));
		return false;
	}

	struct FNotes
	{
		FGutStableState StableWas;
		TArray<int32> SoldWas;
		TArray<FString> SoldToWas;
		int32 SoldDayWas = -1;

		int32 Clerk = INDEX_NONE;
		int32 First = INDEX_NONE;
		int32 Third = INDEX_NONE;
		float Waited = 0.0f;
		bool bFailed = false;
	};

	TSharedRef<FNotes> Notes = MakeShared<FNotes>();

	// how many lots stand on their marks on the Block, and how many of them the page at the table has
	const auto Standing = []()
	{
		const ATownGameMode* Town = GetTown();
		int32 Count = 0;

		for (int32 i = 0; i < Town->GetPlace()->People.Num(); ++i)
		{
			const FTownPerson& Person = Town->GetPlace()->People[i];

			Count += (Person.Lot > 0 && Town->GetPeople().IsValidIndex(i) && Town->GetPeople()[i] && FVector::Dist2D(Town->GetPeople()[i]->GetActorLocation(), Person.Place) < 40.0f) ? 1 : 0;
		}

		return Count;
	};

	TArray<FScript::FStep> Steps;

	Steps.Add({ 3.5f, [this, Notes, Standing]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();
		const UTownPlaceData* Place = Town->GetPlace();

		Notes->StableWas = Run->Stable;
		Notes->SoldWas = Run->BlockSold;
		Notes->SoldToWas = Run->BlockSoldTo;
		Notes->SoldDayWas = Run->BlockSoldDay;

		Run->bStreetForced = true;
		Run->Stable = FGutStableState();
		Run->BlockSold.Reset();
		Run->BlockSoldTo.Reset();
		Run->BlockSoldDay = -1;

		// (the tests are run with the bidding off, so that the others buy at the book's price: here it is on, as it is played)
		Town->bBids = true;
		Town->bLiveSale = true;

		TestTrue(TEXT("The Block's sale goes on by itself"), Town->IsSaleLive() && !Town->IsSaleOver());
		TestFalse(TEXT("Nobody is up when the player has only just come into the square"), Town->GetAuction().bOn);
		TestTrue(*FString::Printf(TEXT("The first lot goes up a quarter of a minute after (%.1f s to go)"), Town->GetSaleNextIn()), Town->GetSaleNextIn() > 5.0f && Town->GetSaleNextIn() <= 15.0f);

		Notes->Clerk = Place->People.IndexOfByPredicate([](const FTownPerson& Person) { return Person.Name == TEXT("Margin"); });
		Notes->First = Town->FindNextLot();

		TestTrue(*FString::Printf(TEXT("There are lots standing on the Block (%d)"), Standing()), Standing() >= 3 && Notes->First != INDEX_NONE);

		// the page at the table, before anybody is up: whoever is next, to look at, and whoever signs
		Town->OpenBook(Notes->Clerk);

		const TArray<FTownShopLine> Lines = Town->GetShopLines();
		const int32 OnThePage = Lines.FilterByPredicate([](const FTownShopLine& Line) { return Line.Name.Contains(TEXT("Lot ")); }).Num();

		TestEqual(TEXT("The page has one lot on it, however many stand on the Block: the one that is next"), OnThePage, 1);

		if (Lines.Num() >= 1)
		{
			TestTrue(*FString::Printf(TEXT("It is marked as next, and says when it comes up (%s: %s)"), *Lines[0].Name, *Lines[0].Why), Lines[0].Name.StartsWith(TEXT("Next")) && !Lines[0].bCanBuy && Lines[0].Why.Contains(TEXT("comes up in")));
			TestTrue(TEXT("Somebody with a tag still signs for a wage"), Lines.Last().Name.StartsWith(TEXT("Signs")) || Lines.Num() == 1);
		}

		// asked for, it is not put up: the sale has its own order and its own time
		Town->HandleMenuInput(EGutMenuInput::Confirm);

		TestTrue(*FString::Printf(TEXT("Nobody is put up for the asking (%s)"), *Town->GetShopSaid()), !Town->GetAuction().bOn && Town->GetShopSaid().Contains(TEXT("not up yet")));

		GutTests::Screenshot(TEXT("57_next_on_the_page"));

		Town->CloseShop();

		TestFalse(TEXT("The table is left"), Town->IsAtBook());

		// (the quarter of a minute is not waited out)
		Town->SetSaleNextIn(0.2f);
	}, []() { return IsStreetReady(); } });

	// the first lot goes up with nobody at the table, and is sold to whoever wants it
	Steps.Add({ 0.3f, [this, Notes, Standing]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (!TestTrue(TEXT("The first lot is put up with nobody at the table"), Town->GetAuction().bOn && Town->GetAuction().Line == Notes->First && !Town->IsBidding()))
		{
			Notes->bFailed = true;
			return;
		}

		TestTrue(*FString::Printf(TEXT("And the caller is heard opening it over the square (%s)"), Town->GetCall() ? *Town->GetCall()->Words : TEXT("nothing")), Town->GetCall() && Town->GetCall()->Words.Contains(TEXT("Who opens")));

		const int32 Before = Standing();

		for (int32 i = 0; i < 400 && Town->GetAuction().bOn; ++i)
		{
			Town->UpdateAuction(10.0f);
		}

		FString To;

		TestTrue(TEXT("The hammer comes down without the player"), !Town->GetAuction().bOn);
		TestTrue(*FString::Printf(TEXT("It went to one of the others (%s)"), *To), Town->IsSoldAway(Notes->First, &To) && !To.IsEmpty());
		TestFalse(TEXT("Nobody is made an owner by a sale they took no part in"), Run->Stable.bStarted);
		TestTrue(*FString::Printf(TEXT("The caller is heard knocking it down (%s)"), Town->GetCall() ? *Town->GetCall()->Words : TEXT("nothing")), Town->GetCall() && Town->GetCall()->Words.StartsWith(TEXT("Sold")));
		TestTrue(*FString::Printf(TEXT("The next lot is some seconds off (%.1f s)"), Town->GetSaleNextIn()), Town->GetSaleNextIn() > 5.0f);
		TestTrue(TEXT("And is another lot"), Town->FindNextLot() != INDEX_NONE && Town->FindNextLot() != Notes->First);

		// at the table for the next
		Town->OpenBook(Notes->Clerk);

		const TArray<FTownShopLine> Lines = Town->GetShopLines();

		TestTrue(*FString::Printf(TEXT("The page has gone on to whoever is next (%s)"), Lines.IsValidIndex(0) ? *Lines[0].Name : TEXT("nobody")), Lines.IsValidIndex(0) && Lines[0].Name.StartsWith(TEXT("Next")) && !Lines[0].Name.Contains(TEXT("Lot 1,")));
		TestTrue(*FString::Printf(TEXT("Whoever was sold is not standing on their number (%d stood, %d stand)"), Before, Standing()), Standing() == Before || Standing() == Before - 1);

		Town->SetSaleNextIn(0.2f);
	}, [Notes]() { return GetTown()->GetAuction().bOn || (Notes->Waited += GetTown()->GetWorld()->GetDeltaSeconds()) > 20.0f; } });

	// the second is bid for, from the table, and won
	Steps.Add({ 0.3f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->bFailed || !TestTrue(TEXT("The second lot comes up while the player is at the table"), Town->IsBidding()))
		{
			Notes->bFailed = true;
			return;
		}

		TestTrue(*FString::Printf(TEXT("The page is that lot, up now (%s)"), *Town->GetShopTitle()), Town->GetShopTitle().Contains(TEXT("UP NOW")));

		const TArray<FTownShopLine> Bids = Town->GetShopLines();

		TestTrue(TEXT("And its lines are what can be bid: the opening, a jump over it, and all that is held"), Bids.Num() == 3 && Bids[0].Price == Town->GetAuction().Open && Bids[0].bCanBuy);

		GutTests::Screenshot(TEXT("57_up_now"));

		int32 Mine = 0;

		for (int32 i = 0; i < 400 && Town->GetAuction().bOn; ++i)
		{
			if (Town->GetAuction().Leader != 0)
			{
				while (Town->GetShopChoice() != 0)
				{
					Town->HandleMenuInput(EGutMenuInput::Up);
				}

				Town->HandleMenuInput(EGutMenuInput::Confirm);

				TestEqual(TEXT("A bid makes the bid the player's"), Town->GetAuction().Leader, 0);

				++Mine;

				Town->HandleBack();

				TestTrue(TEXT("A bid that stands cannot be walked away from"), Town->IsBidding() && Town->GetShopSaid().Contains(TEXT("Your bid stands")));
			}

			Town->UpdateAuction(10.0f);
		}

		TestTrue(*FString::Printf(TEXT("The lot is the player's, who is an owner now (%d bids)"), Mine), Run->Stable.bStarted && Run->Stable.Fighters.Num() == 1 && Mine >= 1);
		TestEqual(TEXT("Whoever was bought comes away with the buyer"), Town->GetTowCount(), 1);
		TestTrue(TEXT("And the table is still open, on whoever is next"), Town->IsAtBook() && Town->GetShopLines().IsValidIndex(0) && Town->GetShopLines()[0].Name.StartsWith(TEXT("Next")));

		Notes->Third = Town->FindNextLot();
		Notes->Waited = 0.0f;

		Town->SetSaleNextIn(0.2f);
	}, [Notes]() { return Notes->bFailed || GetTown()->IsBidding() || (Notes->Waited += GetTown()->GetWorld()->GetDeltaSeconds()) > 40.0f; } });

	// the third: a bid of more than is held, and the table left while it is up
	Steps.Add({ 0.3f, [this, Notes]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (Notes->bFailed || !TestTrue(TEXT("The third lot comes up"), Town->IsBidding() && Town->GetAuction().Line == Notes->Third))
		{
			Notes->bFailed = true;
			return;
		}

		// with only what opens it in hand, the jump over the opening is more than is held
		Run->Stable.Water = Town->GetAuction().Open + 1.0f;

		while (Town->GetShopChoice() != 1)
		{
			Town->HandleMenuInput(EGutMenuInput::Down);
		}

		Town->HandleMenuInput(EGutMenuInput::Confirm);

		const FTownCall* Aside = Town->GetBark();

		TestTrue(*FString::Printf(TEXT("A bid of more than is held is refused (%s)"), *Town->GetShopSaid()), Town->GetAuction().Leader != 0 && Town->GetShopSaid().Contains(TEXT("more than you hold")));
		TestTrue(*FString::Printf(TEXT("And Mother Ledger pipes in (%s: %s)"), Aside ? *Aside->Speaker : TEXT("nobody"), Aside ? *Aside->Words : TEXT("")),
			Aside && Aside->Speaker == TEXT("Mother Ledger") && Aside->Words.Contains(TEXT("ain't got the water")) && Town->GetCall() == Aside);

		GutTests::Screenshot(TEXT("57_he_aint_got_the_water"));

		// the table is left with the lot still up: the room goes on with it
		Town->HandleBack();

		TestTrue(TEXT("The table can be left while a lot is up, and the lot stays up"), !Town->IsAtBook() && Town->GetAuction().bOn);

		for (int32 i = 0; i < 400 && Town->GetAuction().bOn; ++i)
		{
			Town->UpdateAuction(10.0f);
		}

		TestTrue(TEXT("It is knocked down to somebody else"), !Town->GetAuction().bOn && Town->IsSoldAway(Notes->Third) && Run->Stable.Fighters.Num() == 1);

		Notes->Waited = 0.0f;
	}, [Notes]() { return Notes->bFailed || GetTown()->IsBidding() || (Notes->Waited += GetTown()->GetWorld()->GetDeltaSeconds()) > 40.0f; } });

	// and the rest of the day's lots go, one after another, until there are none
	Steps.Add({ 0.5f, [this, Notes, Standing]()
	{
		ATownGameMode* Town = GetTown();
		UGutGameInstance* Run = GetRun();

		if (!Notes->bFailed)
		{
			TestTrue(*FString::Printf(TEXT("The sale runs out (%.0f s)"), Notes->Waited), Town->IsSaleOver() && Town->FindNextLot() == INDEX_NONE);
			TestTrue(*FString::Printf(TEXT("And the caller says so (%s)"), Town->GetCall() ? *Town->GetCall()->Words : TEXT("nothing")), Town->GetCall() && Town->GetCall()->Words.Contains(TEXT("day's sale")));

			Town->OpenBook(Notes->Clerk);

			const TArray<FTownShopLine> Lines = Town->GetShopLines();
			const int32 OnThePage = Lines.FilterByPredicate([](const FTownShopLine& Line) { return Line.Name.Contains(TEXT("Lot ")); }).Num();

			TestEqual(TEXT("Nobody is on the page who is not on the Block: no lots on either"), OnThePage, 0);
			TestTrue(*FString::Printf(TEXT("The clerk says the day's lots are sold (%s)"), *Town->GetShopSaid()), Town->GetShopSaid().Contains(TEXT("all of them sold")));

			GutTests::Screenshot(TEXT("57_the_days_sale_done"));

			Town->CloseShop();
		}

		Run->Stable = Notes->StableWas;
		Run->BlockSold = Notes->SoldWas;
		Run->BlockSoldTo = Notes->SoldToWas;
		Run->BlockSoldDay = Notes->SoldDayWas;
	}, [Notes]()
	{
		ATownGameMode* Town = GetTown();

		if (Notes->bFailed || Town->IsSaleOver())
		{
			return true;
		}

		// (nothing is waited out: each lot is put up as soon as the caller has finished with the last, and the room is hurried through it)
		if (Town->GetAuction().bOn)
		{
			Town->UpdateAuction(10.0f);
		}
		else
		{
			Town->SetSaleNextIn(0.0f);
		}

		return (Notes->Waited += Town->GetWorld()->GetDeltaSeconds()) > 90.0f;
	} });

	ADD_LATENT_AUTOMATION_COMMAND(FScript(this, MoveTemp(Steps)));

	return true;
}

#endif
