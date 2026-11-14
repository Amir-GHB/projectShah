// Copyright Epic Games / Modular Combat System. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "combat/CombatTypes.h"
#include "EnemyCharacter.generated.h"

class UCombatComponent;
class UDamageHandlerComponent;
class UDamageReactionComponent;
class UMotionWarpingComponent;
class ACombatProjectile;
class UAnimMontage;
class UBehaviorTree;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat|Motion Warping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	bool bEnablePatrol = true;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "10.0"))
	float PatrolAcceptanceRadius = 80.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTime = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	bool bUseRandomPatrolWhenNoPoints = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Movement", meta = (ClampMin = "0.0"))
	float PatrolWalkSpeed = 250.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Movement", meta = (ClampMin = "0.0"))
	float PursuitWalkSpeed = 600.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol|Animation")
	TArray<TObjectPtr<UAnimMontage>> PatrolMontages;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol|Animation", meta = (ClampMin = "0.1"))
	float PatrolMontageMinInterval = 4.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol|Animation", meta = (ClampMin = "0.1"))
	float PatrolMontageMaxInterval = 9.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|AI")
	bool bIsInDamageReaction = false;
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Perception", meta = (ClampMin = "0.1"))
	float InvestigateDuration = 5.0f;

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void PerformAICombatAction(AActor* Target);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeavyAttackChance = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat")
	TObjectPtr<UAnimMontage> HeavyAttackTelegraphMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "0.0"))
	float HeavyAttackTelegraphDuration = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "1"))
	int32 MaxAttackersPerTarget = 2;

	bool TryAcquireAttackSlot(AActor* Target);
	void ReleaseAICombatSlot();
	void QueueAICombatAction(AActor* Target, bool bHeavyAttack);
	void CancelPendingAICombatAction();
	bool HasPendingAICombatAction() const { return PendingAICombatTarget.IsValid(); }

protected:
	UFUNCTION()
	void HandleAICombatAttackEnded();

	UFUNCTION()
	void HandleAICombatDamageTaken(float ActualDamage, AActor* InstigatorActor,
		const FDamageInfo& DamageInfo, const FHitResult& HitResult);

private:
	FTimerHandle HeavyAttackTelegraphTimer;
	TWeakObjectPtr<AActor> AttackSlotTarget;
	TWeakObjectPtr<AActor> PendingAICombatTarget;
	bool bPendingAIHeavyAttack = false;
	void ReleaseQueuedAICombatAction();
};

UCLASS()
class CLIMBSYSTEM_API AMagicEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()
public:
	virtual void PerformAICombatAction(AActor* Target) override;
	UFUNCTION(BlueprintCallable, Category = "Enemy|Magic")
	void ReleaseMagicProjectile();
	void CancelPendingMagicAttack();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Animation")
	TObjectPtr<UAnimMontage> MagicAttackMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Magic|Projectile")
	TSubclassOf<ACombatProjectile> MagicProjectileClass;
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

UCLASS()
class CLIMBSYSTEM_API ABowEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()
public:
	virtual void PerformAICombatAction(AActor* Target) override;
	void CancelPendingBowAttack();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Bow", meta = (ClampMin = "0.05"))
	float BowDrawDuration = 0.8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Bow", meta = (ClampMin = "0.1"))
	float BowAttackCooldown = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Bow|Movement", meta = (ClampMin = "1.0"))
	float PreferredCombatDistanceMultiplier = 3.0f;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void ReleaseBowShot();
	FTimerHandle BowReleaseTimer;
	TWeakObjectPtr<AActor> PendingBowTarget;
	double NextBowAttackTime = 0.0;
};
