// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "combat/WeaponBase.h"
#include "Weapon_Unarmed.generated.h"

UCLASS()
class CLIMBSYSTEM_API AWeapon_Unarmed : public AWeaponBase
{
	GENERATED_BODY()

public:
	AWeapon_Unarmed();

	virtual void BeginTrace() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unarmed|Sockets")
	TArray<FName> SkeletalTraceSockets = { FName("Hand_R_Fist"), FName("Hand_L_Fist"), FName("Foot_R_Socket") };

protected:
	virtual void SampleSocketLocations(TMap<FName, FVector>& OutLocations) const override;
};
