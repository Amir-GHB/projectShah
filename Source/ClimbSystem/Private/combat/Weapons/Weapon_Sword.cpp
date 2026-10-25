// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Sword.h"

AWeapon_Sword::AWeapon_Sword()
{
	WeaponConfig.WeaponType = EWeaponType::Sword;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Knight Longsword"));
	WeaponConfig.BaseDamage = 38.0f;
	WeaponConfig.PoiseDamage = 25.0f;
	WeaponConfig.KnockbackImpulse = 450.0f;
	WeaponConfig.DamageCategory = EDamageCategory::Slashing;
	WeaponConfig.HandSocketName = FName("Sword_Hand_Socket");
	WeaponConfig.HolsterSocketName = FName("Sword_Holster_Socket");
	WeaponConfig.TraceSocketNames = { FName("Blade_Hilt"), FName("Blade_Mid"), FName("Blade_Tip") };
	WeaponConfig.TraceRadius = 14.0f;
}
