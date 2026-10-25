// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Spear.h"

AWeapon_Spear::AWeapon_Spear()
{
	WeaponConfig.WeaponType = EWeaponType::Spear;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Spear"));
	WeaponConfig.BaseDamage = 42.0f;
	WeaponConfig.PoiseDamage = 30.0f;
	WeaponConfig.KnockbackImpulse = 500.0f;
	WeaponConfig.DamageCategory = EDamageCategory::Piercing;
	WeaponConfig.HandSocketName = FName("Spear_Hand_Socket");
	WeaponConfig.HolsterSocketName = FName("Spear_Holster_Socket");
	WeaponConfig.TraceSocketNames = { FName("Spear_Trace_Base"), FName("Spear_Trace_Mid"), FName("Spear_Trace_Tip") };
	WeaponConfig.TraceRadius = 12.0f;
}
