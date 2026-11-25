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

#endif
