// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#include "combat/Weapons/Weapon_Unarmed.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AWeapon_Unarmed::AWeapon_Unarmed()
{
	WeaponConfig.WeaponType = EWeaponType::Unarmed;
	WeaponConfig.WeaponDisplayName = FText::FromString(TEXT("Unarmed Combat"));
	WeaponConfig.BaseDamage = 20.0f;
	WeaponConfig.PoiseDamage = 15.0f;
	WeaponConfig.KnockbackImpulse = 300.0f;
	WeaponConfig.DamageCategory = EDamageCategory::Bludgeon;
	WeaponConfig.TraceRadius = 18.0f;

	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(false);
	}
}

void AWeapon_Unarmed::SampleSocketLocations(TMap<FName, FVector>& OutLocations) const
{
	OutLocations.Empty(ValidatedTraceSockets.Num());

	if (!OwningCharacter.IsValid() || !OwningCharacter->GetMesh()) return;

	USkeletalMeshComponent* SkelMesh = OwningCharacter->GetMesh();
	for (const FName& SName : ValidatedTraceSockets)
	{
		const FVector Loc = SkelMesh->GetSocketLocation(SName);
		if (!Loc.ContainsNaN())
		{
			OutLocations.Add(SName, Loc);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Unarmed::SampleSocketLocations] Skeletal socket '%s' returned NaN!"), *SName.ToString());
		}
	}
}

void AWeapon_Unarmed::BeginTrace()
{
	if (!OwningCharacter.IsValid() || !OwningCharacter->GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Unarmed::BeginTrace] OwningCharacter or SkeletalMesh is null!"));
		bIsTracing = false;
		return;
	}

	USkeletalMeshComponent* SkelMesh = OwningCharacter->GetMesh();

	// Validate skeletal sockets exist on the character mesh before enabling trace
	ValidatedTraceSockets.Empty();
	for (const FName& SName : SkeletalTraceSockets)
	{
		if (SkelMesh->DoesSocketExist(SName))
		{
			ValidatedTraceSockets.Add(SName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AWeapon_Unarmed::BeginTrace] Skeletal socket '%s' not found on character mesh '%s'!"),
				*SName.ToString(), *GetNameSafe(SkelMesh->GetSkeletalMeshAsset()));
		}
	}

	if (ValidatedTraceSockets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[AWeapon_Unarmed::BeginTrace] No valid skeletal sockets found on character! Aborting unarmed trace."));
		bIsTracing = false;
		return;
	}

	bIsTracing = true;
	DamagedActorsThisSwing.Empty();
	PreviousSocketLocations.Empty();

	SampleSocketLocations(PreviousSocketLocations);
}
