// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Hammer.h"

AWeapon_Hammer::AWeapon_Hammer()
{
	WeaponConfig.WeaponType = EWeaponType::BullHeadHammer;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Bull Head Hammer"));
	WeaponConfig.BaseDamage = 65.0f;
	WeaponConfig.PoiseDamage = 80.0f;
	WeaponConfig.KnockbackImpulse = 900.0f;
	WeaponConfig.DamageCategory = EDamageCategory::Blunt;
	WeaponConfig.HandSocketName = FName("Hammer_Hand_Socket");
	WeaponConfig.HolsterSocketName = FName("Hammer_Holster_Socket");
	WeaponConfig.TraceSocketNames = { FName("Hammer_Head_Base"), FName("Hammer_Head_Mid"), FName("Hammer_Head_Tip") };
	WeaponConfig.TraceRadius = 24.0f;
}
