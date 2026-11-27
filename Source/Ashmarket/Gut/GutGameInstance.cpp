// Ashmarket prototype: the Gut.

#include "GutGameInstance.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
#include "AudioDevice.h"
#include "Engine/World.h"
#include "GutArmorData.h"
#include "GutLadderData.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UGutGameInstance::Init()
{
	Super::Init();

	bNeedsOn = !FParse::Param(FCommandLine::Get(), TEXT("GutNoNeeds"));

	// automated runs must never read or write the player's own run
	if (FParse::Param(FCommandLine::Get(), TEXT("GutNoSave")))
	{
		bSaveEnabled = false;
	}

	// which of the kept games was being played when the game was last shut: that one is taken up again
	if (bSaveEnabled)
	{
		if (const UGutSettingsSave* Kept = Cast<UGutSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlotName, 0)))
		{
			Game = FMath::Clamp(Kept->Game, 0, GameCount - 1);
			SaveSlotName = GetGameSlot(Game);
		}
	}

	LoadRun();

	// -GutRank=2 -GutWater=60 begins as a fighter who has already climbed that many ranks and holds that much: for trying
	// what only a Butcher can do without fighting up to it. Only with -GutNoSave, so that nobody's own run is written over.
	int32 Rank = 0;

	if (FParse::Value(FCommandLine::Get(), TEXT("GutRank="), Rank) && !bSaveEnabled)
	{
		float Held = 0.0f;
		FParse::Value(FCommandLine::Get(), TEXT("GutWater="), Held);

		ResetLadder(Held);

		Ladder.Rank = FMath::Max(Rank, 0);
		LadderAtRungStart = Ladder;
		PendingStory = EGutStory::None;
	}

	// how loud the player left it, which is theirs and not part of any run
	if (bSaveEnabled)
	{
		if (const UGutSettingsSave* Kept = Cast<UGutSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlotName, 0)))
		{
			Loudness = FMath::Clamp(Kept->Loudness, 0, 3);
			Speech = FMath::Clamp(Kept->Speech, 0, 2);

			UE_LOG(LogTemp, Log, TEXT("[gut] as loud as it was left: %d of 3"), Loudness);
		}
	}
}

void UGutGameInstance::KeepLoudness() const
{
	if (!bSaveEnabled)
	{
		return;
	}

	UGutSettingsSave* Kept = Cast<UGutSettingsSave>(UGameplayStatics::CreateSaveGameObject(UGutSettingsSave::StaticClass()));

	Kept->Loudness = Loudness;
	Kept->Speech = Speech;
	Kept->Game = Game;

	UGameplayStatics::SaveGameToSlot(Kept, SettingsSlotName, 0);
}

bool UGutGameInstance::BuyArmor(const UGutLadderData* Rank, int32 ShopIndex, float Credit)
{
	if (!bLadderStarted || !Rank || !Rank->ArmorShop.IsValidIndex(ShopIndex) || !Rank->ArmorShop[ShopIndex])
	{
		return false;
	}

	const UGutArmorData* Piece = Rank->ArmorShop[ShopIndex];

	// already on, or it cannot be paid for
	if (Ladder.Armor.Contains(Piece->GetFName()) || Ladder.Water - Piece->Price < -Credit)
	{
		return false;
	}

	// it goes on in place of whatever was worn on that part of the body
	for (const UGutArmorData* Other : Rank->ArmorShop)
	{
		if (Other && Other->Slot == Piece->Slot)
		{
			Ladder.Armor.Remove(Other->GetFName());
		}
	}

	Ladder.Armor.Add(Piece->GetFName());
	Ladder.Water -= Piece->Price;
	Ladder.ArmorSpend += Piece->Price;

	UE_LOG(LogGut, Log, TEXT("Ladder: bought %s for %.0f L, leaving %.0f L"), *Piece->GetName(), Piece->Price, Ladder.Water);

	return true;
}

void UGutGameInstance::ResetLadder(float StartingWater)
{
	Ladder = FGutLadderState();

	// with one purse a fighter starts with what the player already holds
	Ladder.Water = GetStartingWater(StartingWater);

	LadderAtRungStart = Ladder;

	bLadderStarted = true;

	PendingStory = EGutStory::Sold;

	if (bSaveEnabled)
	{
		if (Window.bStarted || Stable.bStarted || Story.Marks.Num() > 0 || bOnePurse)
		{
			// the new ladder is what is on disk now, beside whatever the gambler, the owner and the town have going
			SaveRun();
		}
		else
		{
			UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
		}
	}
}

int32 UGutGameInstance::GetBagCount(FName Good) const
{
	const FGutBagItem* Item = Bag.FindByPredicate([Good](const FGutBagItem& One) { return One.Good == Good; });

	return Item ? Item->Count : 0;
}

void UGutGameInstance::AddToBag(FName Good, int32 Count)
{
	FGutBagItem* Item = Bag.FindByPredicate([Good](const FGutBagItem& One) { return One.Good == Good; });

	if (!Item)
	{
		Item = &Bag.AddDefaulted_GetRef();
		Item->Good = Good;
	}

	Item->Count += Count;

	// nothing is carried in noughts
	Bag.RemoveAll([](const FGutBagItem& One) { return One.Count <= 0; });
}

int32 UGutGameInstance::GetRackCount(FName Good) const
{
	const FGutBagItem* Item = Rack.FindByPredicate([Good](const FGutBagItem& One) { return One.Good == Good; });

	return Item ? Item->Count : 0;
}

void UGutGameInstance::AddToRack(FName Good, int32 Count)
{
	FGutBagItem* Item = Rack.FindByPredicate([Good](const FGutBagItem& One) { return One.Good == Good; });

	if (!Item)
	{
		Item = &Rack.AddDefaulted_GetRef();
		Item->Good = Good;
	}

	Item->Count = FMath::Max(0, Item->Count + Count);

	Rack.RemoveAll([](const FGutBagItem& One) { return One.Count <= 0; });
}

void UGutGameInstance::FinishPreparing(const TCHAR* Where)
{
#if WITH_EDITOR
	const int32 Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
	const int32 Shaders = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;

	if (Assets <= 0 && Shaders <= 0)
	{
		return;
	}

	const double Began = FPlatformTime::Seconds();

	FAssetCompilingManager::Get().FinishAllCompilation();

	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	UE_LOG(LogTemp, Log, TEXT("[preparing] %s: waited %.1f s for %d things and %d shaders to be made ready"), Where, FPlatformTime::Seconds() - Began, Assets, Shaders);
#endif
}

void UGutGameInstance::WriteDebt(const FString& What, float Counted, float Written, float After)
{
	FGutDebtLine Line;

	Line.Day = Stable.Day;
	Line.What = What;
	Line.Counted = Counted;
	Line.Written = Written;
	Line.After = FMath::Max(0.0f, After);

	// a debt there was before the book was kept (a run from before 25 November) is carried in at its head
	if (DebtBook.IsEmpty() && After - Written > 0.5f)
	{
		FGutDebtLine Carried;

		Carried.Day = Stable.Day;
		Carried.What = TEXT("owed from before this page");
		Carried.Written = After - Written;
		Carried.After = After - Written;

		DebtBook.Add(Carried);
	}

	DebtBook.Add(Line);

	// she keeps a long book, and not an endless one
	if (DebtBook.Num() > 240)
	{
		DebtBook.RemoveAt(0, DebtBook.Num() - 240);
	}
}

void UGutGameInstance::ShareWater()
{
	static const bool bApart = FParse::Param(FCommandLine::Get(), TEXT("GutThreePurses"));

	// a sitting that is running the automation tests: the engine's own flag for that is only up while a test is being
	// started, not while its steps run
	static const bool bUnderTest = FString(FCommandLine::Get()).Contains(TEXT("Automation RunTests"));

	if (bApart || (bUnderTest && !bSharesWaterInTests))
	{
		return;
	}

	float* Held[3] = { &Ladder.Water, &Window.Water, &Stable.Water };
	const bool bBegun[3] = { bLadderStarted, Window.bStarted, Stable.bStarted };

	if (!bOnePurse)
	{
		// the first time: everything the player holds, whichever way they came by it, is what they hold
		bool bAny = false;

		Purse = 0.0f;

		for (int32 i = 0; i < 3; ++i)
		{
			if (bBegun[i])
			{
				Purse += *Held[i];
				bAny = true;
			}
		}

		if (!bAny)
		{
			Purse = FirstWater;
		}

		bOnePurse = true;

		UE_LOG(LogGut, Log, TEXT("Purse: one purse from now on, holding %.0f L"), Purse);
	}
	else if (bPurseSeen)
	{
		// whatever any of the three has gained or spent since
		for (int32 i = 0; i < 3; ++i)
		{
			Purse += *Held[i] - PurseSeen[i];
		}
	}

	// and each of them holds what the player holds. Only a fighter can be in the pit boss's debt on the slate: nobody
	// bets or buys with water they have not got.
	for (int32 i = 0; i < 3; ++i)
	{
		*Held[i] = i == 0 ? Purse : FMath::Max(Purse, 0.0f);
		PurseSeen[i] = *Held[i];
	}

	// the fight a fighter comes back to after dying is come back to with what they hold now
	LadderAtRungStart.Water = Ladder.Water;

	bPurseSeen = true;
}

bool UGutGameInstance::SaveRun()
{
	// (Something done in the town is a game begun, as a fight, a bet or a yard is: with several games kept, one in which
	// the player had only walked and talked was written nowhere, and was gone when another was taken up.)
	if (!bSaveEnabled || !(bLadderStarted || Window.bStarted || Stable.bStarted || Story.Marks.Num() > 0 || bOnePurse))
	{
		return false;
	}

	// what is written down is one purse
	if (bOnePurse)
	{
		ShareWater();
	}

	UGutSaveGame* Save = Cast<UGutSaveGame>(UGameplayStatics::CreateSaveGameObject(UGutSaveGame::StaticClass()));

	// a game closed in the middle of a fight comes back at that fight's card, as the player came to it
	Save->Ladder = LadderAtRungStart;
	Save->bLadderStarted = bLadderStarted;

	// the gambler's nights and the owner's yard are kept beside it, as they stand
	Save->Window = Window;
	Save->Stable = Stable;
	Save->Bag = Bag;
	Save->Rack = Rack;
	Save->Roster = Roster;
	Save->Purse = Purse;
	Save->bOnePurse = bOnePurse;
	Save->Found = Found;
	Save->Story = Story;
	Save->Needs = Needs;
	Save->BlockSold = BlockSold;
	Save->BlockSoldTo = BlockSoldTo;
	Save->BlockSoldDay = BlockSoldDay;
	Save->DebtBook = DebtBook;
	Save->BlockSoldSeed = BlockSoldSeed;

	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool UGutGameInstance::LoadRun()
{
	if (!bSaveEnabled || !HasSavedRun())
	{
		return false;
	}

	const UGutSaveGame* Save = Cast<UGutSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

	if (!Save)
	{
		return false;
	}

	Ladder = Save->Ladder;
	LadderAtRungStart = Ladder;

	bLadderStarted = Save->bLadderStarted;

	Window = Save->Window;
	Stable = Save->Stable;
	Bag = Save->Bag;
	Rack = Save->Rack;
	Roster = Save->Roster;

	// the purse as it was written down: the three are set to it the next time water is shared, with nothing counted as
	// gained or spent in between
	Purse = Save->Purse;
	bOnePurse = Save->bOnePurse;
	Found = Save->Found;
	Story = Save->Story;
	Needs = Save->Needs;
	BlockSold = Save->BlockSold;
	BlockSoldTo = Save->BlockSoldTo;
	BlockSoldDay = Save->BlockSoldDay;
	DebtBook = Save->DebtBook;
	BlockSoldSeed = Save->BlockSoldSeed;
	bPurseSeen = false;

	if (FParse::Param(FCommandLine::Get(), TEXT("GutStoryAfresh")))
	{
		Story = FTownStoryState();
	}

	// a run written down before the dead were counted once for all three: whoever the window and the yard each knew to be dead
	if (Roster.Dead.IsEmpty())
	{
		for (const int32 Code : Window.Dead)
		{
			Roster.Dead.AddUnique(Code);
		}

		for (const int32 Code : Stable.Dead)
		{
			Roster.Dead.AddUnique(Code);
		}
	}

	return true;
}

void UGutGameInstance::ClearRun()
{
	// as the game instance is before anything has been read into it or done
	const UGutGameInstance* Fresh = GetDefault<UGutGameInstance>(GetClass());

	Ladder = Fresh->Ladder;
	LadderAtRungStart = Fresh->LadderAtRungStart;
	bLadderStarted = Fresh->bLadderStarted;
	Window = Fresh->Window;
	Stable = Fresh->Stable;
	Bag = Fresh->Bag;
	Rack = Fresh->Rack;
	Roster = Fresh->Roster;
	Purse = Fresh->Purse;
	bOnePurse = Fresh->bOnePurse;
	bPurseSeen = Fresh->bPurseSeen;
	Found = Fresh->Found;
	Story = Fresh->Story;
	Needs = FGutNeeds();
	BlockSold = Fresh->BlockSold;
	BlockSoldTo = Fresh->BlockSoldTo;
	BlockSoldDay = Fresh->BlockSoldDay;
	BlockSoldSeed = Fresh->BlockSoldSeed;
	DebtBook = Fresh->DebtBook;
	PendingStory = Fresh->PendingStory;
	Taking = FGutTaking();
	DaySeen = 0;

	// and nowhere in the town yet
	TownStood = Fresh->TownStood;
	TownArrive = Fresh->TownArrive;
	TownLevel = Fresh->TownLevel;
	bAtDoor = Fresh->bAtDoor;
	bCameByDoor = Fresh->bCameByDoor;
	bSandbox = Fresh->bSandbox;
	bWindow = Fresh->bWindow;
	bStable = Fresh->bStable;
}

FString UGutGameInstance::DescribeGame(int32 Which) const
{
	// the one being played, as it stands now; another, as it was written down
	const UGutSaveGame* Kept = Which == Game ? nullptr : Cast<UGutSaveGame>(UGameplayStatics::DoesSaveGameExist(GetGameSlot(Which), 0) ? UGameplayStatics::LoadGameFromSlot(GetGameSlot(Which), 0) : nullptr);

	if (Which != Game && !Kept)
	{
		return FString();
	}

	const FGutStableState& Yard = Kept ? Kept->Stable : Stable;
	const FGutWindowState& Bets = Kept ? Kept->Window : Window;
	const FGutLadderState& Climb = Kept ? Kept->Ladder : Ladder;
	const bool bClimbing = Kept ? Kept->bLadderStarted : bLadderStarted;
	const bool bOne = Kept ? Kept->bOnePurse : bOnePurse;

	const int32 Done = Kept ? Kept->Story.Marks.Num() : Story.Marks.Num();

	if (!bClimbing && !Bets.bStarted && !Yard.bStarted && Done == 0 && !bOne)
	{
		return Which == Game ? FString(TEXT("just begun")) : FString();
	}

	const float Held = bOne ? (Kept ? Kept->Purse : GetPurse()) : (Climb.Water + Bets.Water + Yard.Water);
	const float Owed = (bClimbing ? Climb.Debt : 0.0f) + (Bets.bStarted ? Bets.Debt : 0.0f) + (Yard.bStarted ? Yard.Debt : 0.0f);

	FString Line = FString::Printf(TEXT("%.0f L"), FMath::Max(Held, 0.0f));

	if (Yard.bStarted)
	{
		Line = FString::Printf(TEXT("day %d  \u00B7  %s  \u00B7  %d in the yard"), Yard.Day, *Line, Yard.Fighters.Num());
	}

	if (Owed > 0.5f)
	{
		Line += FString::Printf(TEXT("  \u00B7  owing %.0f L"), Owed);
	}

	return Line;
}

void UGutGameInstance::PlayGame(int32 Which)
{
	Which = FMath::Clamp(Which, 0, GameCount - 1);

	// what is being played is written down as it stands, and put away
	SaveRun();

	Game = Which;
	SaveSlotName = GetGameSlot(Which);

	ClearRun();
	LoadRun();

	// and it is this one that comes back the next time the game is started
	KeepLoudness();

	UE_LOG(LogTemp, Log, TEXT("[gut] game %d taken up: %s"), Which + 1, HasSavedRun() ? *DescribeGame(Which) : TEXT("new"));
}

void UGutGameInstance::ForgetGame(int32 Which)
{
	Which = FMath::Clamp(Which, 0, GameCount - 1);

	if (bSaveEnabled && UGameplayStatics::DoesSaveGameExist(GetGameSlot(Which), 0))
	{
		UGameplayStatics::DeleteGameInSlot(GetGameSlot(Which), 0);
	}

	if (Which == Game)
	{
		ClearRun();
	}

	UE_LOG(LogTemp, Log, TEXT("[gut] game %d forgotten"), Which + 1);
}

bool UGutGameInstance::HasSavedRun() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0);
}

bool UGutGameInstance::IsStreetOpen() const
{
	if (StreetLevel.IsNone() || FParse::Param(FCommandLine::Get(), TEXT("GutNoStreet")))
	{
		return false;
	}

	if (bStreetForced.IsSet())
	{
		return bStreetForced.GetValue();
	}

	return !GIsAutomationTesting && FPackageName::DoesPackageExist(StreetLevel.ToString());
}

bool UGutGameInstance::IsLenderInTown() const
{
	return IsStreetOpen() && !LenderLevel.IsNone() && FPackageName::DoesPackageExist(LenderLevel.ToString());
}

void UGutGameInstance::GoToStreet(const UObject* WorldContext)
{
	SaveRun();

	// the cage's screens are not drawn again: the level is on its way out, and the last picture of it stays up while the
	// street loads. With the ways in already forgotten it would be the wrong card (a fighter's, over an owner's yard).
	bGoingToStreet = true;

	// nobody is in the cage in any of its ways while they are in the street
	bAtDoor = false;
	bCameByDoor = true;
	bSandbox = false;
	bWindow = false;
	bStable = false;

	// back to wherever in the town the player came in from
	UGameplayStatics::OpenLevel(WorldContext, TownLevel.IsNone() ? StreetLevel : TownLevel, true);
}

void UGutGameInstance::ApplyLoudness(const UObject* WorldContext) const
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;

	if (FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle())
	{
		Device->SetTransientPrimaryVolume(FMath::Clamp(Loudness, 0, 3) / 3.0f);
	}
}
