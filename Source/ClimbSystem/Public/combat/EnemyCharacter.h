// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "combat/CombatTypes.h"
#include "EnemyCharacter.generated.h"

class UCombatComponent;
class UDamageHandlerComponent;
class UDamageReactionComponent;
class AEnemyAIController;
class ACombatProjectile;
class UAnimMontage;

/**
 * Reusable AI-controlled combatant.
 * Configure its weapon classes and patrol route in a child Blueprint; core behavior is native.
 */
UCLASS()
class CLIMBSYSTEM_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UDamageHandlerComponent> DamageHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UDamageReactionComponent> DamageReactionComponent;

	/** Optional ordered patrol route. If empty, random reachable patrol is used when enabled. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "10.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	bool bUseRandomPatrolWhenNoPoints = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "100.0"))
	float RandomPatrolRadius = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "100.0"))
	float SightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "100.0"))
	float LoseSightRadius = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float SightPeripheralAngle = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "100.0"))
	float HearingRange = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "50.0"))
	float AttackRange = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "0.05"))
	float AttackRetryInterval = 0.35f;

	/** How long a heard noise location remains an investigation goal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "0.1"))
	float InvestigateDuration = 5.0f;

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	virtual void PerformAICombatAction(AActor* Target);

};
/** Basic ranged caster. Configure its projectile Blueprint and its ProjectileMovement component in the editor. */
UCLASS()
class CLIMBSYSTEM_API AMagicEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()
public:
	virtual void PerformAICombatAction(AActor* Target) override;

	/** Called by the authored montage notify at the exact projectile-release frame. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Magic")
	void ReleaseMagicProjectile();

	/** Clears the pending release when the casting montage is interrupted by a hit or parry. */
	void CancelPendingMagicAttack();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Animation")
	TObjectPtr<UAnimMontage> MagicAttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Projectile")
	TSubclassOf<ACombatProjectile> MagicProjectileClass;

	/** Temporary demo damage profile. Production weaponized ranged damage belongs in CombatComponent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Projectile")
	FProjectileConfig MagicProjectileConfig;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic")
	FName ProjectileSpawnSocket = FName("hand_r");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic")
	FVector ProjectileSpawnOffset = FVector(35.0f, 0.0f, 20.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic", meta = (ClampMin = "0.1"))
	float MagicAttackCooldown = 1.8f;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Movement", meta = (ClampMin = "1.0"))
	float PreferredCombatDistanceMultiplier = 2.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Magic|Movement")
	bool bNeedsRelocationAfterAttack = false;

private:
	TWeakObjectPtr<AActor> PendingMagicTarget;
	float NextMagicAttackTime = 0.0f;
};

