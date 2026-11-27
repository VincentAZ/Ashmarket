// Ashmarket: the town outside the cage.
//
// Bidding for a lot at the Block. The rest of ATownGameMode is in TownGameMode.cpp.

#include "TownGameMode.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "GutCrowd.h"
#include "GutFighter.h"
#include "GutGameInstance.h"
#include "GutGameMode.h"
#include "GutSound.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY_STATIC(LogTownAuction, Log, All);

namespace TownAuctionRules
{
	/** Who else buys at the Block: two or three of them want any one lot */
	const TCHAR* const Houses[] = { TEXT("Mr Rask of the Gut"), TEXT("the Velvet Tin"), TEXT("the Pump office"), TEXT("the Scrappers' guild"), TEXT("a man at the rope"), TEXT("a woman at the back") };

	/**
	 *  How far each of them will go, as a share of what the book has the lot at. The dearest of three such is, one lot
	 *  with another, about the book's own price: so a lot goes for a little under it or a little over, and now and then
	 *  for a good deal less, when nobody much wanted it.
	 */
	const float LeastLimit = 0.55f;
	const float MostLimit = 1.10f;

	/** Where the caller opens, as a share of the book's price */
	const float OpensAt = 0.6f;

	/** How long the caller waits: for somebody to answer the player, after somebody else has bid, and at each "going" */
	const float AnswerWait = 1.5f;
	const float BidWait = 3.4f;
	const float GoingWait = 2.6f;
	const float OpenWait = 2.8f;

	/** In a sale that goes on by itself: how long after the player comes into the square the first lot is put up, and how long after a hammer the next */
	const float FirstLotAfter = 15.0f;
	const float NextLotAfter = 12.0f;

	/** How often the caller comes down from where he opened before he gives a lot up, and to what share of it each time */
	const int32 MostDrops = 3;
	const float DropsTo = 0.75f;

	/** The figures that were recorded in the caller's voice: a bid outside them is written and not said */
	const int32 LeastSpoken = 4;
	const int32 MostSpoken = 160;

	/** How far the end of one recorded part runs under the start of the next (s) */
	const float RunsOn = 0.07f;

	/** The recorded part that names one of the houses, or nothing */
	static FString GetHousePart(const FString& Name)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(Houses); ++i)
		{
			if (Name == Houses[i])
			{
				return FString::Printf(TEXT("house_%d"), i);
			}
		}

		return FString();
	}
}

float ATownGameMode::GetBidStep(float Bid)
{
	return Bid < 40.0f ? 2.0f : (Bid < 80.0f ? 4.0f : 5.0f);
}

float ATownGameMode::GetOpeningBid(float BookPrice)
{
	return FMath::Max(6.0f, FMath::RoundToFloat(BookPrice * TownAuctionRules::OpensAt * 0.5f) * 2.0f);
}

float ATownGameMode::GetNextBid() const
{
	return Auction.Leader == INDEX_NONE ? Auction.Open : Auction.Bid + GetBidStep(Auction.Bid);
}

void ATownGameMode::GetBidders(int32 Line, const FGutStableFighter& Offer, TArray<FTownBidder>& OutBidders) const
{
	OutBidders.Reset();

	const FGutStableState State = GetOwnerState();

	// the same people want the same lot however often it is asked about, on the same day
	FRandomStream Draw(State.Seed + (State.Day * 7919) + (Line * 104729) + 17);

	TArray<int32> Left;

	for (int32 i = 0; i < UE_ARRAY_COUNT(TownAuctionRules::Houses); ++i)
	{
		Left.Add(i);
	}

	const int32 Wanting = Draw.RandRange(2, 3);

	for (int32 i = 0; i < Wanting; ++i)
	{
		const int32 Picked = Draw.RandRange(0, Left.Num() - 1);

		FTownBidder& One = OutBidders.AddDefaulted_GetRef();

		One.Name = TownAuctionRules::Houses[Left[Picked]];
		One.Limit = FMath::FloorToFloat(Offer.Price * Draw.FRandRange(TownAuctionRules::LeastLimit, TownAuctionRules::MostLimit));

		Left.RemoveAt(Picked);
	}
}

bool ATownGameMode::IsSoldAway(int32 Line, FString* OutTo) const
{
	const UGutGameInstance* GameInstance = GetGutGameInstance();
	const FGutStableState State = GetOwnerState();

	if (!GameInstance || GameInstance->BlockSoldDay != State.Day || GameInstance->BlockSoldSeed != State.Seed)
	{
		return false;
	}

	const int32 Which = GameInstance->BlockSold.IndexOfByKey(Line);

	if (Which == INDEX_NONE)
	{
		return false;
	}

	if (OutTo && GameInstance->BlockSoldTo.IsValidIndex(Which))
	{
		*OutTo = GameInstance->BlockSoldTo[Which];
	}

	return true;
}

bool ATownGameMode::IsSaleLive() const
{
	return bBids && bLiveSale && PlaceData && PlaceData->People.ContainsByPredicate([](const FTownPerson& Person) { return Person.Lot > 0; });
}

int32 ATownGameMode::FindNextLot() const
{
	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	const FGutStableState State = GetOwnerState();

	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		if (Offers[i].bOwned && Lots.IsValidIndex(i) && Lots[i] > 0 && !IsSoldAway(i) && !State.BoughtToday.Contains(i) && !IsLotGone(Lots[i]))
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void ATownGameMode::GetBookShown(TArray<int32>& OutOffers) const
{
	OutOffers.Reset();

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	if (const int32 Next = FindNextLot(); Next != INDEX_NONE)
	{
		OutOffers.Add(Next);
	}

	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		if (!Offers[i].bOwned)
		{
			OutOffers.Add(i);
		}
	}
}

int32 ATownGameMode::GetBookOfferAt(int32 Line) const
{
	if (Auction.bOn)
	{
		return Auction.Line;
	}

	if (!IsSaleLive())
	{
		return Line;
	}

	TArray<int32> Shown;
	GetBookShown(Shown);

	return Shown.IsValidIndex(Line) ? Shown[Line] : INDEX_NONE;
}

void ATownGameMode::UpdateSale(float DeltaTime)
{
	AuctionCallLeft = FMath::Max(0.0f, AuctionCallLeft - DeltaTime);

	if (!bSettled || !IsSaleLive() || Auction.bOn || bSaleOver)
	{
		return;
	}

	SaleNextIn -= DeltaTime;

	// (he finishes what he is saying first: the last lot's hammer, or something said between lots)
	if (SaleNextIn > 0.0f || IsAuctionSpeaking() || bLiveCallOn)
	{
		return;
	}

	const int32 Next = FindNextLot();

	if (Next == INDEX_NONE)
	{
		bSaleOver = true;

		SayAuction({ TEXT("sale_over") });

		AuctionCall.Speaker = TEXT("Caller Strand");
		AuctionCall.Words = TEXT("That is the day's sale. The Block is swept. Come back tomorrow.");
		AuctionCallLeft = 6.0f;

		if (bShopping && bAtBook)
		{
			ShopSaid = TEXT("That is the day's lots, all of them sold. There are more tomorrow.");
			ShopChoice = 0;
		}

		UE_LOG(LogTownAuction, Log, TEXT("Sale: there is nobody left to put up today"));

		return;
	}

	PutUp(Next);
}

void ATownGameMode::SayLedger()
{
	const int32 Which = LedgerSaid++ % 3;

	static const TCHAR* Words[] = { TEXT("He ain't got the water."), TEXT("He ain't got the water. Ask me how I know."), TEXT("Not that much he hasn't. He ain't got the water.") };

	BarkCall = FTownCall();
	BarkCall.Speaker = TEXT("Mother Ledger");
	BarkCall.Words = Words[Which];
	BarkCall.Seconds = 3.0f;
	BarkUntil = GetWorld()->GetTimeSeconds() + BarkCall.Seconds;

	USoundBase* Said = LedgerAsides.IsValidIndex(Which) ? LedgerAsides[Which].Get() : nullptr;

	if (const UGutGameInstance* GameInstance = GetGutGameInstance(); GameInstance && GameInstance->HearsSpeech() && Said)
	{
		if (BarkVoice)
		{
			BarkVoice->Stop();
		}

		BarkVoice = UGameplayStatics::SpawnSound2D(this, Said, 1.0f);

		// and she is seen to say it, by anybody looking her way
		for (int32 Person = 0; PlaceData && Person < PlaceData->People.Num() && Person < People.Num(); ++Person)
		{
			if (People[Person] && PlaceData->People[Person].Name == TEXT("Mother Ledger"))
			{
				People[Person]->Speak(GetFaceFor(Said));
				break;
			}
		}
	}

	UE_LOG(LogTownAuction, Log, TEXT("Auction: Mother Ledger: %s (%s)"), *BarkCall.Words, Said ? TEXT("heard") : TEXT("written only"));
}

TArray<FTownShopLine> ATownGameMode::GetAuctionLines() const
{
	TArray<FTownShopLine> Lines;

	if (!Auction.bOn)
	{
		return Lines;
	}

	const float Held = FMath::FloorToFloat(GetOwnerState().Water);

	// whether the player could take anybody at all, at any price: a yard that is full bids for nobody
	FString Barred;

	if (const AGutGameMode* Rules = GetCageRules())
	{
		FGutStableState Tried = GetOwnerState();
		FGutStableFighter Free = Auction.Offer;
		FString Why;

		Free.Price = 0.0f;

		if (!Rules->TakeBlockOffer(Tried, Free, Auction.Line, &Why))
		{
			Barred = Why == TEXT("taken") ? FString(TEXT("yours already")) : FString::Printf(TEXT("your yard holds %d and is full"), Rules->GetStableMostFighters());
		}
	}

	// with the bid the player's own there is nothing to do but hear it out
	if (Auction.Leader == 0)
	{
		FTownShopLine Line;

		Line.Name = FString::Printf(TEXT("The bid is yours: %.0f L"), Auction.Bid);
		Line.PriceText = TEXT(" ");
		Line.bCanBuy = false;
		Line.Why = TEXT("wait for the hammer");

		Lines.Add(MoveTemp(Line));

		return Lines;
	}

	const float Next = GetNextBid();
	const float Jump = Next + (GetBidStep(Next) * 2.0f);

	// the next bid, a jump over it, and everything in hand: each is there to be read, and refused if it is more than is held
	TArray<float> Bids = { Next, Jump };

	if (Held > Jump)
	{
		Bids.Add(Held);
	}

	for (const float Bid : Bids)
	{
		FTownShopLine Line;

		Line.Name = Bid == Held && Bid > Jump ? FString::Printf(TEXT("Bid all you hold: %.0f L"), Bid) : FString::Printf(TEXT("Bid %.0f L"), Bid);
		Line.Price = Bid;
		Line.PriceText = TEXT(" ");
		Line.bCanBuy = Bid <= Held && Barred.IsEmpty();
		Line.Why = !Barred.IsEmpty() ? Barred : (Line.bCanBuy ? FString() : FString::Printf(TEXT("you hold %.0f L"), Held));

		Lines.Add(MoveTemp(Line));
	}

	return Lines;
}

FString ATownGameMode::GetFigurePart(float Figure)
{
	const int32 Whole = FMath::RoundToInt32(Figure);

	return (Whole >= TownAuctionRules::LeastSpoken && Whole <= TownAuctionRules::MostSpoken) ? FString::Printf(TEXT("num_%d"), Whole) : FString();
}

USoundBase* ATownGameMode::GetAuctionSound(const FString& Part)
{
	if (Part.IsEmpty())
	{
		return nullptr;
	}

	const FName Key(*Part);

	if (const TObjectPtr<USoundBase>* Known = AuctionSounds.Find(Key))
	{
		return Known->Get();
	}

	// (asked for once: a part that is not there is not looked for again)
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Town/Voices/block_square/bid_%s.bid_%s"), *Part, *Part), nullptr, LOAD_NoWarn | LOAD_Quiet);

	AuctionSounds.Add(Key, Sound);

	// (and made ready, if it was fetched late: a sound not yet ready is not heard)
	if (Sound && bSettled)
	{
		UGutGameInstance::FinishPreparing(*Part);
	}

	return Sound;
}

void ATownGameMode::FetchAuctionSounds()
{
	if (!PlaceData || PlaceData->Calls.IsEmpty())
	{
		return;
	}

	for (int32 Figure = TownAuctionRules::LeastSpoken; Figure <= TownAuctionRules::MostSpoken; ++Figure)
	{
		GetAuctionSound(FString::Printf(TEXT("num_%d"), Figure));
	}

	for (int32 Lot = 1; Lot <= 12; ++Lot)
	{
		GetAuctionSound(FString::Printf(TEXT("lot_%d"), Lot));
	}

	for (int32 House = 0; House < UE_ARRAY_COUNT(TownAuctionRules::Houses); ++House)
	{
		GetAuctionSound(FString::Printf(TEXT("house_%d"), House));
	}

	for (const TCHAR* Part : { TEXT("who_opens"), TEXT("at_the_table"), TEXT("any_more"), TEXT("against_you"), TEXT("nobody_at"), TEXT("once"), TEXT("twice"), TEXT("going_once"),
		TEXT("going_twice"), TEXT("sold"), TEXT("to_the_table"), TEXT("no_bid"), TEXT("no_takers"), TEXT("sale_over") })
	{
		GetAuctionSound(Part);
	}

	// and what Mother Ledger throws in when somebody bids water he has not got
	LedgerAsides.Reset();

	for (int32 i = 1; i <= 3; ++i)
	{
		LedgerAsides.Add(LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Town/Voices/block_square/mother_ledger_aside_no_water_%d.mother_ledger_aside_no_water_%d"), i, i), nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	int32 There = 0;

	for (const TPair<FName, TObjectPtr<USoundBase>>& One : AuctionSounds)
	{
		There += One.Value ? 1 : 0;
	}

	UE_LOG(LogTownAuction, Log, TEXT("Auction: %d of %d parts of the caller's bidding are fetched"), There, AuctionSounds.Num());
}

void ATownGameMode::SayAuction(const TArray<FString>& Parts)
{
	// whatever he was saying, he stops
	if (CallVoice)
	{
		CallVoice->FadeOut(0.08f, 0.0f);
		CallVoice = nullptr;
	}

	AuctionParts.Reset();
	AuctionPart = 0;
	AuctionPartLeft = 0.0f;

	for (const FString& Part : Parts)
	{
		if (!GetAuctionSound(Part))
		{
			UE_LOG(LogTownAuction, Log, TEXT("Auction: not said, for want of the part \"%s\""), *Part);
			return;
		}
	}

	AuctionParts = Parts;
}

void ATownGameMode::UpdateAuctionVoice(float DeltaTime)
{
	AuctionPartLeft = FMath::Max(0.0f, AuctionPartLeft - DeltaTime);

	if (AuctionPartLeft > 0.0f || !AuctionParts.IsValidIndex(AuctionPart))
	{
		return;
	}

	USoundBase* Sound = GetAuctionSound(AuctionParts[AuctionPart]);

	++AuctionPart;

	if (Sound)
	{
		CallVoice = UGameplayStatics::SpawnSound2D(this, Sound, IsTalking() ? 0.25f : 0.9f);

		// each runs into the next as a caller's words do: the last of one is under the first of the other
		AuctionPartLeft = FMath::Max(0.1f, Sound->GetDuration() - TownAuctionRules::RunsOn);

		UE_LOG(LogTownAuction, Log, TEXT("Auction: says \"%s\" (%.2f s long, at %.2f of full, %s)"), *AuctionParts[AuctionPart - 1], Sound->GetDuration(), IsTalking() ? 0.25f : 0.9f,
			CallVoice ? (CallVoice->IsPlaying() ? TEXT("playing") : TEXT("made, not playing")) : TEXT("NOT MADE"));

#if WITH_EDITOR
		if (const USoundWave* Wave = Cast<USoundWave>(Sound); Wave && Wave->IsCompiling())
		{
			UE_LOG(LogTownAuction, Warning, TEXT("Auction: \"%s\" is not yet ready to be played, and will not be heard"), *AuctionParts[AuctionPart - 1]);
		}
#endif
	}
}

void ATownGameMode::CallAuction(const FString& Words, float Wait, const TArray<FString>& Parts)
{
	Auction.Called = Words;

	SayAuction(Parts);

	// he is not hurried through what he has to say: the wait is at least as long as the saying of it, and a breath
	float Saying = 0.0f;

	for (const FString& Part : AuctionParts)
	{
		if (const USoundBase* Sound = GetAuctionSound(Part))
		{
			Saying += FMath::Max(0.1f, Sound->GetDuration() - TownAuctionRules::RunsOn);
		}
	}

	Auction.Wait = FMath::Max(Wait, Saying + 0.35f);

	AuctionCall.Speaker = TEXT("Caller Strand");
	AuctionCall.Words = Words;

	// and how it stands for whoever is at the table, under it
	if (bShopping && bAtBook)
	{
		TellAuction();
	}

	UE_LOG(LogTownAuction, Log, TEXT("Auction: \"%s\" (lot %d, %s, bid %.0f L, %s)"), *Words, Auction.Lot, *Auction.Offer.FighterName.ToString(), Auction.Bid,
		Auction.Leader == INDEX_NONE ? TEXT("nobody's") : (Auction.Leader == 0 ? TEXT("the player's") : *Auction.Rivals[Auction.Leader - 1].Name));
}

void ATownGameMode::TellAuction()
{
	if (!Auction.bOn)
	{
		return;
	}

	const float Held = FMath::FloorToFloat(GetOwnerState().Water);

	// (somebody who has not bid has nothing "against" them: the bid is with whoever it is with)
	const FString Stands = Auction.Leader == INDEX_NONE ? FString::Printf(TEXT("Nobody has bid. It opens at %.0f L."), Auction.Open)
		: (Auction.Leader == 0 ? FString::Printf(TEXT("The bid is yours, at %.0f L."), Auction.Bid)
			: FString::Printf(TEXT("The bid is %.0f L, with %s."), Auction.Bid, *Auction.Rivals[Auction.Leader - 1].Name));

	ShopSaid = FString::Printf(TEXT("\"%s\"   %s You hold %.0f L. The book has %s at %.0f L."), *Auction.Called, *Stands, Held, UGutSoundSubsystem::WhoIs(this, Auction.Offer.Body).Him(), Auction.Offer.Price);
}

bool ATownGameMode::BeginAuction(int32 Line)
{
	const AGutGameMode* Rules = GetCageRules();

	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	if (!Rules || !Offers.IsValidIndex(Line) || Auction.bOn)
	{
		return false;
	}

	const FGutStableFighter& Offer = Offers[Line];
	const FString Who = Offer.FighterName.ToString();
	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Offer.Body);

	if (FString To; IsSoldAway(Line, &To))
	{
		ShopSaid = FString::Printf(TEXT("%s went to %s today. I have their name against %s."), *Who, *To, Sex.Hers());
		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	// whether the player could take anybody at all: tried on a copy, for nothing
	FGutStableState Tried = GetOwnerState();
	FGutStableFighter Free = Offer;
	FString Why;

	Free.Price = 0.0f;

	if (!Rules->TakeBlockOffer(Tried, Free, Line, &Why))
	{
		ShopSaid = Why == TEXT("taken") ? FString::Printf(TEXT("%s is yours already. I have your name against %s."), *Who, Sex.Hers())
			: FString::Printf(TEXT("Your yard holds %d and it is full. Sell one, or bury one, and come back."), Rules->GetStableMostFighters());
		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	return PutUp(Line);
}

bool ATownGameMode::PutUp(int32 Line)
{
	TArray<FGutStableFighter> Offers;
	TArray<int32> Lots;
	GetBookOffers(Offers, Lots);

	if (!Offers.IsValidIndex(Line) || Auction.bOn)
	{
		return false;
	}

	const FGutStableFighter& Offer = Offers[Line];
	const FString Who = Offer.FighterName.ToString();

	// whatever he was saying between lots is done with
	bLiveCallOn = false;

	Auction = FTownAuction();
	Auction.bOn = true;
	Auction.Line = Line;
	Auction.Lot = Lots.IsValidIndex(Line) ? Lots[Line] : 0;
	Auction.Offer = Offer;
	Auction.Open = GetOpeningBid(Offer.Price);

	GetBidders(Line, Offer, Auction.Rivals);

	// (somebody at the table has the lot's own bids before them now; somebody at their bag or a stall is left alone)
	if (bShopping && bAtBook)
	{
		ShopChoice = 0;
	}

	// the caller stops whatever he was calling, and calls this
	if (CallVoice)
	{
		CallVoice->FadeOut(0.3f, 0.0f);
		CallVoice = nullptr;
	}

	// (said without the name: there are more names than can be recorded)
	TArray<FString> Parts;

	if (Auction.Lot >= 1 && Auction.Lot <= 12)
	{
		Parts.Add(FString::Printf(TEXT("lot_%d"), Auction.Lot));
	}

	Parts.Add(TEXT("who_opens"));
	Parts.Add(GetFigurePart(Auction.Open));

	CallAuction(Auction.Lot > 0 ? FString::Printf(TEXT("Lot %d. %s. Who opens at %.0f litres?"), Auction.Lot, *Who, Auction.Open) : FString::Printf(TEXT("%s. Who opens at %.0f litres?"), *Who, Auction.Open),
		TownAuctionRules::OpenWait, Parts);

	// and the table's picture turns to whoever it is
	if (bShopping && bAtBook)
	{
		BookShown = INDEX_NONE;

		ShowBookLine();
	}

	return true;
}

bool ATownGameMode::PlaceBid(int32 Option)
{
	const TArray<FTownShopLine> Lines = GetAuctionLines();

	if (!Auction.bOn || !Lines.IsValidIndex(Option))
	{
		return false;
	}

	if (Auction.Leader == 0)
	{
		ShopRefusedAt = GetWorld()->GetTimeSeconds();
		return false;
	}

	if (!Lines[Option].bCanBuy)
	{
		if (Lines[Option].Price > FMath::FloorToFloat(GetOwnerState().Water))
		{
			ShopSaid = FString::Printf(TEXT("\"%s\"   %.0f L is more than you hold: %s. The Block gives no credit."), *Auction.Called, Lines[Option].Price, *Lines[Option].Why);

			// and the room hears of it, from the woman who knows to the litre what he holds
			SayLedger();
		}
		else
		{
			ShopSaid = FString::Printf(TEXT("\"%s\"   Not for you: %s."), *Auction.Called, *Lines[Option].Why);
		}

		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	Auction.Bid = Lines[Option].Price;
	Auction.Leader = 0;
	Auction.Count = 0;

	ShopChoice = 0;

	CallAuction(FString::Printf(TEXT("%.0f, at the table."), Auction.Bid), TownAuctionRules::AnswerWait, { GetFigurePart(Auction.Bid), TEXT("at_the_table") });

	return true;
}

bool ATownGameMode::LetGo()
{
	if (!Auction.bOn)
	{
		return false;
	}

	// a bid that stands is a bid: nobody walks away from the table with the caller's hammer up over their own water
	if (Auction.Leader == 0)
	{
		ShopSaid = FString::Printf(TEXT("\"%s\"   Your bid stands, at %.0f L. The caller is not done with it."), *Auction.Called, Auction.Bid);
		ShopRefusedAt = GetWorld()->GetTimeSeconds();

		return false;
	}

	// where the sale goes on by itself there is nothing to let go: the player leaves the table, and the room goes on bidding
	if (IsSaleLive())
	{
		CloseShop();

		return true;
	}

	// let go: the room finishes it without the player, at once
	while (Auction.bOn)
	{
		Auction.Wait = 0.0f;
		UpdateAuction(0.0f);
	}

	return true;
}

void ATownGameMode::UpdateAuction(float DeltaTime)
{
	if (!Auction.bOn)
	{
		return;
	}

	Auction.Wait -= DeltaTime;

	if (Auction.Wait > 0.0f)
	{
		return;
	}

	const float Next = GetNextBid();

	// whoever else will still go that far, and does not have the bid already
	TArray<int32> Able;

	for (int32 i = 0; i < Auction.Rivals.Num(); ++i)
	{
		if (i + 1 != Auction.Leader && Auction.Rivals[i].Limit >= Next)
		{
			Able.Add(i);
		}
	}

	if (!Able.IsEmpty())
	{
		const bool bOverThePlayer = Auction.Leader == 0;
		const int32 Bids = Able[FMath::RandRange(0, Able.Num() - 1)];

		Auction.Bid = Next;
		Auction.Leader = Bids + 1;
		Auction.Count = 0;

		CallAuction(FString::Printf(TEXT("%.0f, %s.%s"), Auction.Bid, *Auction.Rivals[Bids].Name, bOverThePlayer ? TEXT(" Against you at the table.") : TEXT(" Any more?")), TownAuctionRules::BidWait,
			{ GetFigurePart(Auction.Bid), TownAuctionRules::GetHousePart(Auction.Rivals[Bids].Name), bOverThePlayer ? TEXT("against_you") : TEXT("any_more") });

		return;
	}

	++Auction.Count;

	if (Auction.Count == 1)
	{
		CallAuction(Auction.Leader == INDEX_NONE ? FString::Printf(TEXT("Nobody at %.0f? Once."), Auction.Open) : FString::Printf(TEXT("Going once, at %.0f."), Auction.Bid), TownAuctionRules::GoingWait,
			Auction.Leader == INDEX_NONE ? TArray<FString>({ TEXT("nobody_at"), GetFigurePart(Auction.Open), TEXT("once") }) : TArray<FString>({ TEXT("going_once"), GetFigurePart(Auction.Bid) }));
	}
	else if (Auction.Count == 2)
	{
		CallAuction(Auction.Leader == INDEX_NONE ? FString(TEXT("Twice.")) : FString::Printf(TEXT("Going twice, at %.0f."), Auction.Bid), TownAuctionRules::GoingWait,
			Auction.Leader == INDEX_NONE ? TArray<FString>({ TEXT("twice") }) : TArray<FString>({ TEXT("going_twice"), GetFigurePart(Auction.Bid) }));
	}
	else if (Auction.Leader == INDEX_NONE && IsSaleLive() && Auction.Drops < TownAuctionRules::MostDrops && Auction.Open > 4.0f)
	{
		// Nobody will open there: he comes down, and asks again. (A sale that goes on by itself does not leave a lot
		// standing unsold on the Block to be asked for again.)
		++Auction.Drops;

		Auction.Open = FMath::Max(4.0f, FMath::RoundToFloat(Auction.Open * TownAuctionRules::DropsTo * 0.5f) * 2.0f);
		Auction.Count = 0;

		CallAuction(FString::Printf(TEXT("No? Then lower. Who opens at %.0f litres?"), Auction.Open), TownAuctionRules::OpenWait, { TEXT("no_takers"), TEXT("who_opens"), GetFigurePart(Auction.Open) });
	}
	else
	{
		EndAuction();
	}
}

void ATownGameMode::EndAuction()
{
	const FTownAuction Done = Auction;

	Auction = FTownAuction();

	const FString Who = Done.Offer.FighterName.ToString();
	const FGutWho Sex = UGutSoundSubsystem::WhoIs(this, Done.Offer.Body);

	const bool bLive = IsSaleLive();
	const bool bAtTable = bShopping && bAtBook;

	// the list again: on whoever it was, or, where the sale goes on by itself, on whoever is next
	if (bAtTable)
	{
		ShopChoice = bLive ? 0 : Done.Line;
	}

	// and the caller goes back to his own sale, with the next thing he has to call
	CallTime = 1000.0f;

	SaleNextIn = TownAuctionRules::NextLotAfter;

	// what he called last stays up while he says it
	AuctionCall.Speaker = TEXT("Caller Strand");
	AuctionCallLeft = 4.5f;

	const auto WriteSold = [this, &Done](const FString& To)
	{
		if (UGutGameInstance* GameInstance = GetGutGameInstance())
		{
			const FGutStableState State = GetOwnerState();

			if (GameInstance->BlockSoldDay != State.Day || GameInstance->BlockSoldSeed != State.Seed)
			{
				GameInstance->BlockSold.Reset();
				GameInstance->BlockSoldTo.Reset();
				GameInstance->BlockSoldDay = State.Day;
				GameInstance->BlockSoldSeed = State.Seed;
			}

			GameInstance->BlockSold.Add(Done.Line);
			GameInstance->BlockSoldTo.Add(To);
			GameInstance->SaveRun();
		}
	};

	if (Done.Leader == INDEX_NONE)
	{
		SayAuction({ TEXT("no_bid") });

		AuctionCall.Words = TEXT("No bid.");

		if (bLive)
		{
			// nobody would have them at any figure: back to the Holding Yard, and not put up again today
			WriteSold(TEXT("nobody: back to the Holding Yard"));
			WalkLotAway(Done.Lot);

			if (bAtTable)
			{
				ShopSaid = FString::Printf(TEXT("No bid. %s goes back to the Holding Yard unsold."), *Who);
			}
		}
		else
		{
			ShopSaid = FString::Printf(TEXT("No bid. %s stays on the book, and can be put up again."), *Who);
		}

		if (bAtTable && bLive)
		{
			BookShown = INDEX_NONE;

			ShowBookLine();
		}

		return;
	}

	for (TActorIterator<AGutCrowd> It(GetWorld()); It; ++It)
	{
		It->Cheer(0.12f, 1.2f);
	}

	// the hammer, and who to
	SayAuction({ TEXT("sold"), GetFigurePart(Done.Bid), Done.Leader == 0 ? FString(TEXT("to_the_table")) : TownAuctionRules::GetHousePart(Done.Rivals[Done.Leader - 1].Name) });

	AuctionCall.Words = FString::Printf(TEXT("Sold. %.0f litres, to %s."), Done.Bid, Done.Leader == 0 ? TEXT("the table") : *Done.Rivals[Done.Leader - 1].Name);

	if (Done.Leader == 0)
	{
		// the player's: taken as anybody bought at the book is, for what was bid
		if (TakeAtBook(Done.Line, Done.Bid))
		{
			ShopSaid += FString::Printf(TEXT(" The book had %s at %.0f L."), Sex.Him(), Done.Offer.Price);
		}

		if (bAtTable && bLive)
		{
			ShopChoice = 0;
			BookShown = INDEX_NONE;

			ShowBookLine();
		}

		return;
	}

	// somebody else's: written against their name, and walked off the Block for good
	const FString To = Done.Rivals[Done.Leader - 1].Name;

	WriteSold(To);

	WalkLotAway(Done.Lot);

	const FString Told = FString::Printf(TEXT("Sold: %s, to %s, for %.0f L. %s is walked off to the Holding Yard, and is theirs."), *Who, *To, Done.Bid, Sex.HeCap());

	if (bAtTable || !bLive)
	{
		ShopSaid = Told;
	}

	if (bAtTable && bLive)
	{
		BookShown = INDEX_NONE;

		ShowBookLine();
	}

	UE_LOG(LogTownAuction, Log, TEXT("Auction: %s"), *Told);
}

void ATownGameMode::WalkLotAway(int32 Lot)
{
	const int32 Stood = (PlaceData && Lot > 0) ? PlaceData->People.IndexOfByPredicate([Lot](const FTownPerson& Person) { return Person.Lot == Lot; }) : INDEX_NONE;

	if (!People.IsValidIndex(Stood) || !People[Stood])
	{
		return;
	}

	// no longer one of the sale's: not kept, not brought back up
	KeptLots.Remove(Stood);

	const FTownPerson& Person = PlaceData->People[Stood];
	const TArray<FVector>& Down = PlaceData->LotWay;
	const FVector Here = People[Stood]->GetActorLocation();

	FTownLed Walk;

	Walk.Person = Stood;
	Walk.Wait = LotSoldWait;
	Walk.bGoes = true;

	int32 Nearest = Down.Num() - 1;

	if (FVector::Dist2D(Here, Person.Place) < 40.0f)
	{
		// still on its mark: back off it first, along behind the others
		Walk.Way.Add(Person.Place - (FRotator(0.0f, Person.Facing, 0.0f).Vector() * PlaceData->LotAisle));
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

	// down the way the lots came up, to where they are kept, and out of the sale
	for (int32 i = Nearest; i >= 0; --i)
	{
		Walk.Way.Add(Down[i]);
	}

	if (Walk.Way.IsEmpty())
	{
		People[Stood]->Destroy();
		People[Stood] = nullptr;

		return;
	}

	Led.RemoveAll([Stood](const FTownLed& One) { return One.Person == Stood; });
	Led.Add(MoveTemp(Walk));
}
