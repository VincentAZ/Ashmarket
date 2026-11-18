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
	Ladder.Water = StartingWater;

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

bool UGutGameInstance::SaveRun()
{
	if (!bSaveEnabled || !(bLadderStarted || Window.bStarted || Stable.bStarted))
	{
		return false;
	}

	UGutSaveGame* Save = Cast<UGutSaveGame>(UGameplayStatics::CreateSaveGameObject(UGutSaveGame::StaticClass()));

	// a game closed in the middle of a fight comes back at that fight's card, as the player came to it
	Save->Ladder = LadderAtRungStart;
	Save->bLadderStarted = bLadderStarted;

	// the gambler's nights and the owner's yard are kept beside it, as they stand
	Save->Window = Window;
	Save->Stable = Stable;

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
