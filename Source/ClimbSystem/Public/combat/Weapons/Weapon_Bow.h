// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "combat/WeaponBase.h"
#include "combat/CombatTypes.h"
#include "Weapon_Bow.generated.h"

class ACombatProjectile;
class USkeletalMeshComponent;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowDrawStateChangedSignature, bool, bIsDrawing, float, DrawAlpha);

/** Rigged bow weapon. Its skeletal mesh can use DrawAlpha to drive string and limb controls. */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API AWeapon_Bow : public AWeaponBase
{
	GENERATED_BODY()

public:
	AWeapon_Bow();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Visuals")
	TObjectPtr<USkeletalMeshComponent> SkeletalBowMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile")
	TSubclassOf<ACombatProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile")
	FProjectileConfig ProjectileConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile")
	FName ArrowSpawnSocketName = FName("Arrow_Spawn_Socket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	TObjectPtr<UAnimMontage> DrawMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	TObjectPtr<UAnimMontage> ReleaseMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.05"))
	float FullDrawTime = 1.0f;

	/** Allows a quick tap to fire at the minimum charge. If false, early release cancels the shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw")
	bool bAllowQuickShot = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MinimumDrawTime = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MinDrawDamageMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MaxDrawDamageMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MinDrawSpeedMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Draw", meta = (ClampMin = "0.0"))
	float MaxDrawSpeedMultiplier = 1.25f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Draw")
	bool bIsDrawing = false;

	/** 0..1 draw strength, intended for the rigged bow AnimBP/Control Rig. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Draw")
	float DrawAlpha = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowDrawStateChangedSignature OnBowDrawStateChanged;

	UFUNCTION(BlueprintCallable, Category = "Bow|Draw")
	bool BeginDraw();

	UFUNCTION(BlueprintCallable, Category = "Bow|Draw")
	bool ReleaseShot();

	UFUNCTION(BlueprintCallable, Category = "Bow|Draw")
	void CancelDraw();

	UFUNCTION(BlueprintPure, Category = "Bow|Draw")
	float GetDrawAlpha() const { return DrawAlpha; }

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	double DrawStartTime = 0.0;

	bool SpawnArrow(float ChargeAlpha);
	bool GetAimDirectionAndSpawnTransform(FVector& OutDirection, FTransform& OutSpawnTransform) const;
	void PlayCharacterMontage(UAnimMontage* Montage);
};
