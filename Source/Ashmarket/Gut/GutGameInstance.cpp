// Ashmarket prototype: the Gut.

#include "GutGameInstance.h"
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

	// automated runs must never read or write the player's own run
	if (FParse::Param(FCommandLine::Get(), TEXT("GutNoSave")))
	{
		bSaveEnabled = false;
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
		if (Window.bStarted || Stable.bStarted)
		{
			// the new ladder is what is on disk now, beside whatever the gambler and the owner have going
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
	if (!bSaveEnabled || !(bLadderStarted || Window.bStarted || Stable.bStarted))
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
	Save->Roster = Roster;
	Save->Purse = Purse;
	Save->bOnePurse = bOnePurse;
	Save->Found = Found;

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
	Roster = Save->Roster;

	// the purse as it was written down: the three are set to it the next time water is shared, with nothing counted as
	// gained or spent in between
	Purse = Save->Purse;
	bOnePurse = Save->bOnePurse;
	Found = Save->Found;
	bPurseSeen = false;

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
