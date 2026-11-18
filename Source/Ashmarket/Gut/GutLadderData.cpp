// Ashmarket prototype: the Gut.

#include "GutLadderData.h"
#include "GutWeaponData.h"

namespace GutLadderConstants
{
	/** The book never calls a fight a certainty either way */
	const float MinBookChance = 0.08f;
	const float MaxBookChance = 0.92f;
}

float UGutLadderData::GetBookChance(int32 Rung, const UGutWeaponData* PlayerWeapon, const UGutWeaponData* EnemyWeapon, int32 PlayerCrippledLimbs) const
{
	float Chance = Rungs.IsValidIndex(Rung) ? Rungs[Rung].BookChance : 0.5f;

	// what the book knows about the two weapons
	for (const FGutFormEntry& Entry : FormBook)
	{
		if (Entry.Weapon == PlayerWeapon && Entry.Against == EnemyWeapon)
		{
			Chance += (Entry.WinShare - 0.5f) * FormWeight;
			break;
		}

		if (Entry.Weapon == EnemyWeapon && Entry.Against == PlayerWeapon)
		{
			Chance -= (Entry.WinShare - 0.5f) * FormWeight;
			break;
		}
	}

	// and what it can see of the player
	Chance -= PlayerCrippledLimbs * CrippledLimbChancePenalty;

	if (!PlayerWeapon)
	{
		Chance -= UnarmedChancePenalty;
	}

	return FMath::Clamp(Chance, GutLadderConstants::MinBookChance, GutLadderConstants::MaxBookChance);
}

float UGutLadderData::GetReturnPerLitre(float PlayerChance, EGutBetSide Side, float Stake) const
{
	if (Side == EGutBetSide::None)
	{
		return 0.0f;
	}

	// the crowd's water is already on the fight, split the way the book sees it
	const float SideChance = Side == EGutBetSide::Player ? PlayerChance : 1.0f - PlayerChance;

	const float Pot = CrowdPool + FMath::Max(0.0f, Stake);
	const float OnSide = (CrowdPool * SideChance) + FMath::Max(0.0f, Stake);

	// the house takes its cut and the rest is shared among the winners by what each put in
	return (Pot * (1.0f - HouseTake)) / FMath::Max(OnSide, 1.0f);
}

float UGutLadderData::GetGateBill(float Water, bool bSpared) const
{
	const float Bill = RoomAndBoard + (bSpared ? SparedPenalty : 0.0f);

	// room and board comes to whatever the player is holding, and then some
	return FMath::Max(Bill, Water + MinimumDebt);
}

float UGutLadderData::GetGateDebt(float Water, bool bSpared) const
{
	return GetGateBill(Water, bSpared) - Water;
}
