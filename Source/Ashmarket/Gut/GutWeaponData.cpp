// Ashmarket prototype: the Gut.

#include "GutWeaponData.h"

namespace GutWeaponData
{
	/** Averages one number over a list of attacks */
	template <typename GetterType>
	float Average(const TArray<FGutAttackDef>& Attacks, GetterType Getter)
	{
		float Total = 0.0f;

		for (const FGutAttackDef& Attack : Attacks)
		{
			Total += Getter(Attack);
		}

		return Attacks.Num() > 0 ? Total / Attacks.Num() : 0.0f;
	}
}

float UGutWeaponData::GetLightReach() const
{
	return GutWeaponData::Average(LightAttacks, [](const FGutAttackDef& Attack) { return Attack.Reach; });
}

float UGutWeaponData::GetLightDamage() const
{
	return GutWeaponData::Average(LightAttacks, [](const FGutAttackDef& Attack) { return Attack.Damage; });
}

float UGutWeaponData::GetLightTime() const
{
	return GutWeaponData::Average(LightAttacks, [](const FGutAttackDef& Attack) { return Attack.GetTotalTime(); });
}
