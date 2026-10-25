// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "CombatProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class AWeaponBase;

/** Generic projectile shared by bows and future ranged weapons. */
UCLASS(Blueprintable)
class CLIMBSYSTEM_API ACombatProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACombatProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Optional visual mesh. Assign an arrow mesh in a projectile Blueprint; collision remains on CollisionSphere. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileVisual;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile")
	FProjectileConfig ProjectileConfig;

	/** Called before FinishSpawningActor so config and direction are ready in BeginPlay. */
	void InitializeProjectile(const FProjectileConfig& InConfig, const FVector& Direction, AActor* InDamageInstigator, AWeaponBase* InSourceWeapon);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	float CalculateFalloffMultiplier() const;
	void ApplyHitDamage(AActor* TargetActor, const FHitResult& Hit);

	TWeakObjectPtr<AActor> DamageInstigator;
	TWeakObjectPtr<AWeaponBase> SourceWeapon;
	FVector InitialDirection = FVector::ForwardVector;
	FVector PreviousLocation = FVector::ZeroVector;
	float DistanceTravelled = 0.0f;
	bool bHasHit = false;
};
