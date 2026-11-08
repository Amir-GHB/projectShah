// Copyright Epic Games / Modular Combat System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TimerManager.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AEnemyCharacter;

/**
 * Small native state-driven AI: patrol, investigate heard sounds, pursue visible players,
 * and trigger the owner's existing weapon combat component at close range.
 */
UCLASS()
class CLIMBSYSTEM_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy AI")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

private:
	TWeakObjectPtr<AEnemyCharacter> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;
	FVector InvestigationLocation = FVector::ZeroVector;
	float InvestigationTimeRemaining = 0.0f;
	float AttackCooldownRemaining = 0.0f;
	int32 PatrolPointIndex = 0;
	bool bWaitingAtPatrolPoint = false;
	FVector RandomPatrolDestination = FVector::ZeroVector;
	bool bHasRandomPatrolDestination = false;
	FTimerHandle UpdateTimer;
	FTimerHandle PatrolWaitTimer;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleComboWindowOpened();

	void UpdateAI();
	void UpdatePatrol();
	void MoveToNextPatrolPoint();
	void FinishPatrolWait();
	void ClearTarget();
	bool IsValidCombatTarget(const AActor* Candidate) const;
};
