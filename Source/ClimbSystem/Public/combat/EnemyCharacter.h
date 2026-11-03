// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class UCombatComponent;
class UDamageHandlerComponent;
class UDamageReactionComponent;
class AEnemyAIController;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UDamageHandlerComponent> DamageHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UDamageReactionComponent> DamageReactionComponent;

	/** Ordered patrol route. Leave empty for a stationary guard. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "10.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTime = 2.0f;

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

protected:
	virtual void BeginPlay() override;
};
